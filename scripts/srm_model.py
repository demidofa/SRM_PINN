# -*- coding: utf-8 -*-
"""
Учебная аналитическая модель трехфазного вентильно-индукторного двигателя 6/4.

Модель поверхности намагничивания (угол theta_e — электрический, 0 = рассогласованное
положение, pi = согласованное):

    psi(theta_e, i) = Lu*i + psi_m*g(theta_e)*(1 - exp(-k*i)),   k = (La - Lu)/psi_m

При малых токах L(theta_e) = Lu + (La - Lu)*g(theta_e); при больших токах
магнитопровод насыщается. Функция g — сглаженный трапециевидный профиль
(плоские участки вблизи согласованного и рассогласованного положений).

Момент фазы вычисляется через коэнергию W'(theta, i) = int_0^i psi di:
    T = dW'/dtheta_mech = Nr * psi_m * g'(theta_e) * (i - (1 - exp(-k*i))/k)

Параметры иллюстративные и не соответствуют конкретному серийному двигателю.
"""
import numpy as np

class SRM:
    def __init__(self, Ns=6, Nr=4, m=3, Lu=0.012, La=0.120, psi_m=1.1,
                 R=0.6, Udc=540.0, J=0.02, p_shape=2.0):
        self.Ns, self.Nr, self.m = Ns, Nr, m
        self.Lu, self.La, self.psi_m = Lu, La, psi_m
        self.k = (La - Lu) / psi_m
        self.R, self.Udc, self.J = R, Udc, J
        self.p = p_shape

    # --- профиль индуктивности ---
    def _s(self, th):
        return 0.5 * (1.0 - np.cos(th))

    def g(self, th):
        s = self._s(th); p = self.p
        a, b = s**p, (1.0 - s)**p
        return a / (a + b)

    def dg(self, th):
        s = np.clip(self._s(th), 1e-12, 1 - 1e-12); p = self.p
        a, b = s**p, (1.0 - s)**p
        dg_ds = p * s**(p - 1) * (1 - s)**(p - 1) / (a + b)**2
        return dg_ds * 0.5 * np.sin(th)          # d/dtheta_e

    def L_unsat(self, th):
        return self.Lu + (self.La - self.Lu) * self.g(th)

    # --- поверхность намагничивания и момент ---
    def psi(self, th, i):
        return self.Lu * i + self.psi_m * self.g(th) * (1.0 - np.exp(-self.k * i))

    def dpsi_di(self, th, i):
        return self.Lu + self.psi_m * self.g(th) * self.k * np.exp(-self.k * i)

    def torque(self, th, i):
        i = np.maximum(i, 0.0)
        return self.Nr * self.psi_m * self.dg(th) * (i - (1.0 - np.exp(-self.k * i)) / self.k)

    def coenergy(self, th, i):
        return (self.Lu * i**2 / 2
                + self.psi_m * self.g(th) * (i - (1.0 - np.exp(-self.k * i)) / self.k))

    def current(self, th, psi, i0=None, iters=6):
        """Обращение psi(theta, i) методом Ньютона (функция монотонна по i)."""
        psi = np.maximum(psi, 0.0)
        i = psi / self.L_unsat(th) if i0 is None else np.maximum(i0, 0.0)
        for _ in range(iters):
            i = i - (self.psi(th, i) - psi) / self.dpsi_di(th, i)
            i = np.maximum(i, 0.0)
        return i

    def phase_angle(self, theta_mech, ph):
        """Электрический угол фазы ph (0, 1, 2) при механическом угле ротора."""
        stroke = 2 * np.pi / (self.m * self.Nr)           # шаг, мех. рад
        return np.mod(self.Nr * (theta_mech - ph * stroke), 2 * np.pi)


def in_window(th, th_on, th_off):
    """Фаза в интервале проводимости [th_on, th_off) с учетом периодичности угла."""
    return np.mod(th - th_on, 2 * np.pi) < np.mod(th_off - th_on, 2 * np.pi)


def simulate_time(mot, rpm, th_on, th_off, i_ref, band=0.5, Ts=None, dt=5e-6,
                  t_end=None, controller="hyst", soft=True):
    """
    Моделирование трех фаз при постоянной скорости с несимметричными мостами.
    controller: "hyst"   — релейный регулятор (непрерывный при Ts=None,
                            дискретный с периодом Ts),
                "mpc"    — управление с прогнозированием на один шаг,
                           набор напряжений {+U, 0, -U}, период Ts.
    Углы th_on, th_off — электрические, рад.
    """
    w = rpm * 2 * np.pi / 60
    if t_end is None:
        t_end = 2 * np.pi / (mot.Nr * w)                  # один электрический период
    n = int(round(t_end / dt))
    t = np.arange(n) * dt
    th_m = w * t
    psi = np.zeros(mot.m); i = np.zeros(mot.m); v = np.zeros(mot.m)
    I = np.zeros((n, mot.m)); V = np.zeros((n, mot.m)); T = np.zeros((n, mot.m))
    PS = np.zeros((n, mot.m))
    step_ctrl = 1 if Ts is None else max(1, int(round(Ts / dt)))
    U = mot.Udc
    for k in range(n):
        th = np.array([mot.phase_angle(th_m[k], p) for p in range(mot.m)])
        if k % step_ctrl == 0:
            for p in range(mot.m):
                active = in_window(th[p], th_on, th_off)
                if not active:
                    v[p] = -U if i[p] > 1e-3 else 0.0
                    continue
                if controller == "hyst":
                    if i[p] > i_ref + (band if Ts is None else 0.0):
                        v[p] = 0.0 if soft else -U
                    elif i[p] < i_ref - (band if Ts is None else 0.0):
                        v[p] = U
                else:  # одношаговое прогнозирование
                    h = step_ctrl * dt
                    th_next = np.mod(th[p] + mot.Nr * w * h, 2 * np.pi)
                    best, vbest = 1e18, 0.0
                    for cand in (U, 0.0, -U):
                        psi_p = psi[p] + (cand - mot.R * i[p]) * h
                        i_p = mot.current(th_next, psi_p, i[p])
                        cost = (i_ref - i_p)**2 + 1e-6 * abs(cand - v[p]) / U
                        if cost < best:
                            best, vbest = cost, cand
                    v[p] = vbest
        psi = np.maximum(psi + (v - mot.R * i) * dt, 0.0)
        i = mot.current(th, psi, i)
        I[k], V[k], PS[k] = i, v, psi
        T[k] = mot.torque(th, i)
    return dict(t=t, i=I, v=V, psi=PS, T=T, Te=T.sum(axis=1), w=w)


def cycle_angle_domain(mot, rpm, th_on, th_off, i_max, n_steps=None, periods=2,
                       dt_max=1e-5):
    """
    Векторизованный расчет установившегося цикла одной фазы в угловой области
    для массивов углов th_on, th_off (одинаковой формы). Регулятор тока идеальный
    (ограничение на уровне i_max). Возвращает средний момент двигателя (все фазы)
    и действующее значение тока фазы, признак непрерывного тока и пиковый ток.
    """
    w = rpm * 2 * np.pi / 60
    if n_steps is None:
        n_steps = max(720, int(np.ceil(2 * np.pi / (mot.Nr * w * dt_max))))
    dth = 2 * np.pi / n_steps
    dt = dth / (mot.Nr * w)
    th_on = np.asarray(th_on, float); th_off = np.asarray(th_off, float)
    psi = np.zeros_like(th_on); i = np.zeros_like(th_on)
    Tsum = np.zeros_like(th_on); I2 = np.zeros_like(th_on)
    Ipk = np.zeros_like(th_on)
    U = mot.Udc
    for per in range(periods):
        for k in range(n_steps):
            th = (k + 0.5) * dth
            active = in_window(th, th_on, th_off)
            v_on = np.where(i < i_max, U, np.where(i > 1.02 * i_max, -U, 0.0))
            v = np.where(active, v_on, np.where(i > 1e-4, -U, 0.0))
            psi = np.maximum(psi + (v - mot.R * i) * dt, 0.0)
            i = mot.current(th, psi, i, iters=4)
            if per == periods - 1:
                Tsum += mot.torque(th, i); I2 += i**2
                Ipk = np.maximum(Ipk, i)
    T_avg = mot.m * Tsum / n_steps
    I_rms = np.sqrt(I2 / n_steps)
    # признак непрерывного тока: к концу периода ток не спал до нуля
    continuous = i > 1e-2
    return T_avg, I_rms, continuous, Ipk
