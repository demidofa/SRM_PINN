# -*- coding: utf-8 -*-
"""
Управление фазой ВИД с прогнозированием табличным методом и онлайн-идентификация
поверхности намагничивания (по учебнику, гл. 4, п. «Онлайн идентификация»).

Прогнозирующее звено (непрерывный набор управляющих воздействий):
    psi_ref = Psi(theta_next, i_ref)
    u       = (psi_ref - psi_hat)/Ts + R*i,   ограничение |u| <= Udc
где psi_hat — потокосцепление фазы, оцененное интегрированием (u - R*i).
Если таблица занижена, заданное потокосцепление достигается при токе ниже
задания, и возникает установившаяся ошибка тока, которую устраняет коррекция.
Коррекция таблицы: ближайший к рабочей точке (theta, i_ref) узел, если нормированный
квадрат расстояния до него меньше d2_max, изменяется на k*(i_ref - i).
Таблица системы управления в начале построена для двигателя с индуктивностью
в согласованном положении на 20 % ниже фактической.
"""
import numpy as np
from srm_model import SRM, in_window

class FluxTable:
    def __init__(self, mot, n_th=36, n_i=21, i_max=30.0):
        self.th = np.linspace(0, 2 * np.pi, n_th + 1)[:-1]
        self.i = np.linspace(0, i_max, n_i)
        self.dth = self.th[1]; self.di = self.i[1]
        TH, I = np.meshgrid(self.th, self.i, indexing="ij")
        self.P = mot.psi(TH, I)
        self.n_th, self.n_i = n_th, n_i

    def _idx(self, th, i):
        x = np.mod(th, 2 * np.pi) / self.dth
        y = np.clip(i, 0, self.i[-1] - 1e-9) / self.di
        return x, y

    def __call__(self, th, i):
        x, y = self._idx(th, i)
        x0 = int(np.floor(x)); y0 = int(np.floor(y))
        fx, fy = x - x0, y - y0
        x1 = (x0 + 1) % self.n_th; y1 = min(y0 + 1, self.n_i - 1)
        P = self.P
        return ((1 - fx) * (1 - fy) * P[x0, y0] + fx * (1 - fy) * P[x1, y0]
                + (1 - fx) * fy * P[x0, y1] + fx * fy * P[x1, y1])

    def correct(self, th, i_ref, err, k, d2_max=0.25):
        x, y = self._idx(th, i_ref)
        for xn in (np.floor(x), np.ceil(x)):
            for yn in (np.floor(y), np.ceil(y)):
                d2 = (xn - x) ** 2 + (yn - y) ** 2
                if d2 < d2_max and 0 <= yn < self.n_i:
                    self.P[int(xn) % self.n_th, int(yn)] += k * err


def run(rpm=300, i_ref=12.0, th_on=np.deg2rad(20), th_off=np.deg2rad(150),
        Ts=1e-4, sub=10, cycles=40, k=2e-4, adapt=True, err_la=-0.20, seed=0, mot_kw=None):
    mot_kw = mot_kw or {}
    mot = SRM(**mot_kw)
    ctrl_mot = SRM(La=mot.Lu + (1 + err_la) * (mot.La - mot.Lu), **mot_kw)
    tab = FluxTable(ctrl_mot)
    w = rpm * 2 * np.pi / 60
    T_el = 2 * np.pi / (mot.Nr * w)
    n = int(round(cycles * T_el / Ts))
    dt = Ts / sub
    psi = 0.0; i = 0.0; th_m = 0.0; psi_hat = 0.0
    rms = np.zeros(cycles); cnt = np.zeros(cycles)
    trace_first, trace_last = [], []
    for kk in range(n):
        th = np.mod(mot.Nr * th_m, 2 * np.pi)
        cyc = min(int(kk * Ts / T_el), cycles - 1)
        active = in_window(th, th_on, th_off)
        if active:
            th_next = th + mot.Nr * w * Ts
            u = (tab(th_next, i_ref) - psi_hat) / Ts + mot.R * i
            u = float(np.clip(u, -mot.Udc, mot.Udc))
            if adapt and i > 0.5 * i_ref:
                tab.correct(th, i_ref, i_ref - i, k)
            if i > 0.8 * i_ref:
                rms[cyc] += (i_ref - i) ** 2; cnt[cyc] += 1
        else:
            u = -mot.Udc if i > 1e-3 else 0.0
        if cyc == 0: trace_first.append((kk * Ts, i))
        if cyc == cycles - 1: trace_last.append((kk * Ts, i))
        psi_hat = max(psi_hat + (u - mot.R * i) * Ts, 0.0)   # наблюдатель потокосцепления
        if not active and i <= 1e-3:
            psi_hat = 0.0
        for _ in range(sub):
            psi = max(psi + (u - mot.R * i) * dt, 0.0)
            th_m += w * dt
            i = float(mot.current(np.mod(mot.Nr * th_m, 2 * np.pi), psi, i))
    rms = np.sqrt(rms / np.maximum(cnt, 1))
    return dict(rms=rms, first=np.array(trace_first), last=np.array(trace_last),
                tab=tab, true=FluxTable(mot), init=FluxTable(ctrl_mot), T_el=T_el)

def figure(path):
    import mstyle
    from mstyle import plt, C, finish, lab
    ks = (5e-5, 2e-4, 1e-3, 5e-3)
    runs = {k: run(k=k, cycles=30) for k in ks}
    r = runs[1e-3]
    fig, ax = plt.subplots(1, 2, figsize=(7.0, 3.0))
    f, l = r["first"], r["last"]
    ax[0].plot(f[:, 0] * 1e3, f[:, 1], color=C[1], label="1-й период")
    ax[0].plot((l[:, 0] - l[0, 0]) * 1e3, l[:, 1], color=C[0], label="30-й период")
    ax[0].axhline(12, color="k", ls="--", lw=0.7)
    ax[0].set_xlabel(lab("t", "мс")); ax[0].set_ylabel(lab("i", "А"))
    ax[0].set_xlim(0, 25); ax[0].set_ylim(0, 16); ax[0].legend(loc="lower center", fontsize=9)
    for k, c, mk in zip(ks, C, "osd^"):
        ax[1].plot(np.arange(1, 31), runs[k]["rms"], "-" + mk, ms=3, color=c,
                   label="$k$ = " + f"{k:.0e}".replace("e-0", "·10$^{-") + "}$")
    ax[1].set_xlabel("номер периода"); ax[1].set_ylabel(lab(r"\sigma_i", "А"))
    ax[1].set_xlim(0, 31); ax[1].set_ylim(0, 1.5); ax[1].legend(loc="upper right", fontsize=9)
    finish(list(ax), labels=True)
    fig.tight_layout(); fig.savefig(path, dpi=300, bbox_inches="tight")
    return {k: (v["rms"][0], v["rms"][-1]) for k, v in runs.items()}


if __name__ == "__main__":
    import os
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "figs", "fig_adapt.png")
    print(figure(out))
