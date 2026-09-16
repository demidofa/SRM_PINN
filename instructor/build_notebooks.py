# -*- coding: utf-8 -*-
import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
SETUP = '''# Подготовка среды: папки scripts, autograder и detective должны лежать рядом с notebooks
# (в Colab: загрузите архив материалов дисциплины и распакуйте его в /content)
import sys, os, numpy as np, matplotlib.pyplot as plt
for p in ("../scripts", "scripts", "/content/scripts", "../detective", "/content/detective"):
    if os.path.isdir(p): sys.path.insert(0, os.path.abspath(p))
from srm_model import SRM, simulate_time, cycle_angle_domain
mot = SRM(); deg = np.deg2rad
print("Модель загружена: ВИД", f"{mot.Ns}/{mot.Nr}", "Udc =", mot.Udc, "В")'''

def nb(cells):
    out = []
    for kind, src in cells:
        c = {"cell_type": kind, "metadata": {}, "source": src.strip("\n").splitlines(keepends=True)}
        if kind == "code":
            c["outputs"] = []; c["execution_count"] = None
        out.append(c)
    return {"cells": out, "metadata": {"kernelspec": {"name": "python3", "display_name": "Python 3"},
            "language_info": {"name": "python"}}, "nbformat": 4, "nbformat_minor": 5}

NB = {}
NB["ЛР1_двигатель_из_данных.ipynb"] = [
("markdown", """# ЛР1. Цифровой двойник, этап 1: двигатель из данных
Цель: разработка модели поверхности намагничивания ВИД по данным заторможенного ротора и исследование влияния физических ограничений на корректность нейросетевой модели.

Этап А выполняется на синтетических данных, где известен точный ответ. Этап Б выполняется на осциллограммах стенда."""),
("code", SETUP),
("markdown", "## Этап А. Нейросеть без ограничений и ФИНС"),
("code", """import lab1_pinn_demo as demo
w_nn = demo.train(0.0, seed=0)   # без физических ограничений
w_pi = demo.train(1.0, seed=0)   # ФИНС
for name, w in (("ИНС", w_nn), ("ФИНС", w_pi)):
    print(name, {k: round(float(v), 4) for k, v in demo.evaluate(w).items()})"""),
("code", """i = np.linspace(0, 20, 200); th = deg(22.5)
plt.plot(i, mot.psi(th, i), 'k', label='модель')
plt.plot(i, demo.net(w_nn, np.full_like(i, th), i), '--', label='ИНС')
plt.plot(i, demo.net(w_pi, np.full_like(i, th), i), label='ФИНС')
plt.axvspan(0, 3, alpha=0.15, color='r'); plt.xlabel('Ток, А'); plt.ylabel('ψ, Вб'); plt.legend(); plt.grid(alpha=.3)"""),
("markdown", """**Задание.** Проверьте требования 1–3 ЛР1 и объясните, почему нейросеть без ограничений им не удовлетворяет. Измените коэффициент физической части функции потерь (второй аргумент `train`) и опишите его влияние."""),
("markdown", "## Подготовка файла для автопроверки"),
("code", """import pandas as pd, pathlib
grid_path = next(p for p in ("../autograder/lab1_grid.csv", "autograder/lab1_grid.csv", "/content/autograder/lab1_grid.csv") if os.path.exists(p))
g = pd.read_csv(grid_path)
out = pathlib.Path("results/lab1"); out.mkdir(parents=True, exist_ok=True)
g["psi_Wb"] = demo.net(w_pi, g.theta_rad.values, g.i_A.values)
g.to_csv(out / "lab1_predictions.csv", index=False)
print("Сохранено:", out / "lab1_predictions.csv")"""),
("markdown", """## Этап Б. Данные стенда
Загрузите осциллограммы стенда (файлы выдает преподаватель). Для каждого углового положения рассчитайте потокосцепление
$\\psi(t)=\\int_0^t (u - R\\,i)\\,d\\tau$ и постройте кривые $\\psi(i)$. Оцените влияние погрешности сопротивления и смещения нуля датчика тока."""),
("code", """# ЗАДАНИЕ: замените синтетический пример на чтение файла стенда
t = np.linspace(0, 0.02, 2001); dt = t[1] - t[0]
u = np.where(t < 0.01, 300.0, -300.0)          # пример: импульс напряжения
psi_true, i_meas = np.zeros_like(t), np.zeros_like(t)
for k in range(1, len(t)):
    psi_true[k] = max(psi_true[k-1] + (u[k-1] - mot.R*i_meas[k-1])*dt, 0)
    i_meas[k] = mot.current(np.pi, psi_true[k], i_meas[k-1])
for R_err in (1.0, 1.2):
    psi_calc = np.maximum(np.cumsum((u - R_err*mot.R*i_meas))*dt, 0)
    plt.plot(i_meas, psi_calc, label=f'R × {R_err}')
plt.xlabel('Ток, А'); plt.ylabel('ψ, Вб'); plt.legend(); plt.grid(alpha=.3)"""),
]
NB["ЛР2_углы_коммутации.ipynb"] = [
("markdown", """# ЛР2. Цифровой двойник, этап 2: углы коммутации и предельная характеристика
Предварительно найдите режим в интерактивном тренажере (ссылка в РПД, п. 3.2)."""),
("code", SETUP),
("markdown", "## Перебор углов при 1500 об/мин"),
("code", """on, off = np.meshgrid(deg(np.arange(-20, 41, 2.5)), deg(np.arange(110, 171, 2.5)))
T, Irms, cont, Ipk = cycle_angle_domain(mot, 1500, on, off, i_max=20)
ok = (~cont) & (Ipk <= 21) & (Irms <= 12.5)
Tm = np.where(ok, T, np.nan)
plt.contourf(np.rad2deg(on), np.rad2deg(off), Tm, 20); plt.colorbar(label='момент, Н·м')
plt.xlabel('угол включения, эл. град'); plt.ylabel('угол отключения, эл. град')
j = np.unravel_index(np.nanargmax(Tm), T.shape)
print('лучшее:', np.rad2deg(on[j]), np.rad2deg(off[j]), 'момент', round(float(T[j]), 2), 'Н·м')"""),
("markdown", "## Проверка во временной области и по энергии цикла"),
("code", """theta_on, theta_off, i_ref = 20.0, 145.0, 20.0     # ЗАДАНИЕ: подставьте свой режим
Tel = 60/1500/mot.Nr
r = simulate_time(mot, 1500, deg(theta_on), deg(theta_off), i_ref, band=0.8, t_end=2*Tel, dt=5e-6)
n = len(r['t'])//2
W = np.trapezoid(r['i'][n:,0]*r['v'][n:,0] - mot.R*r['i'][n:,0]**2, r['t'][n:])
T_W = W*mot.m*mot.Nr/(2*np.pi)
print(f"момент по модели {r['Te'][n:].mean():.2f} Н·м, по энергии цикла {T_W:.2f} Н·м")
print(f"пиковый ток {r['i'][n:,0].max():.2f} А, действующий {np.sqrt(np.mean(r['i'][n:,0]**2)):.2f} А")
plt.plot(r['i'][n:,0], r['psi'][n:,0]); plt.xlabel('Ток, А'); plt.ylabel('ψ, Вб'); plt.grid(alpha=.3)"""),
("code", """import json, pathlib
out = pathlib.Path("results/lab2"); out.mkdir(parents=True, exist_ok=True)
json.dump({"theta_on_deg": theta_on, "theta_off_deg": theta_off, "i_ref": i_ref,
           "band": 0.8, "soft": True, "T_from_W": float(T_W)}, open(out/"lab2_params.json", "w"), indent=1)
print("Сохранено для автопроверки")"""),
("markdown", "**Задание.** Постройте предельную характеристику в двигательном и генераторном режимах (см. `make_figures.fig_limit_curve`) и объясните изменение оптимальных углов со скоростью."),
]
NB["ЛР3_система_управления.ipynb"] = [
("markdown", "# ЛР3. Цифровой двойник, этап 3: система управления"),
("code", SETUP),
("code", """rpm, Ts = 600, 1e-4
kw = dict(dt=2e-6, t_end=60/rpm/mot.Nr, Ts=Ts)
res = {c: simulate_time(mot, rpm, deg(10), deg(150), 15, controller=c, **kw) for c in ("hyst", "mpc")}
for c, r in res.items():
    plt.plot(r['t']*1e3, r['i'][:,0], label=c)
plt.axhline(15, ls='--', c='k'); plt.legend(); plt.xlabel('мс'); plt.ylabel('А'); plt.grid(alpha=.3)"""),
("code", """import pathlib
r = res["mpc"]                       # ЗАДАНИЕ: сохраните трассу своего регулятора
act = (np.mod(r['t']*r['w']*mot.Nr - deg(10), 2*np.pi) < deg(140))
out = pathlib.Path("results/lab3"); out.mkdir(parents=True, exist_ok=True)
np.savetxt(out/"lab3_trace.csv", np.column_stack([r['t'], r['i'][:,0], np.full(len(act), 15.0), act]),
           delimiter=",", header="t_s,i_A,i_ref_A,active", comments="")"""),
("markdown", "## Онлайн-идентификация поверхности намагничивания"),
("code", """import srm_adapt, json
k = 1e-3                              # ЗАДАНИЕ: исследуйте несколько значений
a = srm_adapt.run(k=k, cycles=15)
plt.plot(np.arange(1, 16), a['rms'], 'o-'); plt.xlabel('период'); plt.ylabel('СКО тока, А'); plt.grid(alpha=.3)
n1 = int(np.argmax(a['rms'] <= 1.0)) + 1
json.dump({"k": k, "periods_to_1A": n1}, open(out/"lab3_adapt.json", "w"))
print("периодов до СКО <= 1 А:", n1)"""),
("markdown", "## Как звучат пульсации момента\nЗапустите `sound_of_srm.py` и прослушайте файлы в папке `sound`. Опишите различие спектров и объясните, почему это не расчет акустического шума."),
("code", """try:
    from IPython.display import Audio, display
    for f in ("hyst_band0p8A.wav", "mpc_Ts100us.wav"):
        for d in ("../sound", "sound", "/content/sound"):
            if os.path.exists(os.path.join(d, f)): display(Audio(os.path.join(d, f)))
except ImportError:
    print("Прослушайте файлы в папке sound")"""),
]
NB["ЛР4_привод_без_датчика.ipynb"] = [
("markdown", "# ЛР4. Цифровой двойник, этап 4: торможение и привод без датчика"),
("code", SETUP),
("code", """# ЗАДАНИЕ: улучшите оценку (исследовательский уровень). Текст функции сохраняется для автопроверки.
ESTIMATOR = \'\'\'
import numpy as np
def estimate(La, Lb, Lc):
    l_al = (2*La - Lb - Lc)/3
    l_be = (Lb - Lc)/np.sqrt(3)
    return np.mod(np.arctan2(-l_be, -l_al), 2*np.pi)
\'\'\'
exec(ESTIMATOR)
thm = np.linspace(0, 2*np.pi/mot.Nr, 720, endpoint=False)
L = [mot.L_unsat(mot.phase_angle(thm, p)) for p in range(3)]
err = np.rad2deg(np.angle(np.exp(1j*(estimate(*L) - mot.Nr*thm))))
plt.plot(np.rad2deg(mot.Nr*thm), err); plt.xlabel('эл. град'); plt.ylabel('ошибка, эл. град'); plt.grid(alpha=.3)
print('амплитуда ошибки', round(float(np.abs(err).max()), 3), 'эл. град')"""),
("code", """import pathlib
out = pathlib.Path("results/lab4"); out.mkdir(parents=True, exist_ok=True)
(out/"lab4_estimator.py").write_text(ESTIMATOR, encoding="utf-8")"""),
("markdown", """## Неисправный привод
Преподаватель выдает файл `case_XX.csv`. Определите вид неисправности и фазу. Подсказка: рассчитайте потокосцепление каждой фазы интегрированием $u - R i$ и сравните фазы между собой, оцените нулевой уровень токов и частоту переключений."""),
("code", """import make_cases
rec = make_cases.simulate_case("turn_fault", 1, 450, 12.0, np.random.default_rng(0), periods=2)  # пример
t, ia, ib, ic, ua, ub, uc = rec[:,0], rec[:,2], rec[:,3], rec[:,4], rec[:,5], rec[:,6], rec[:,7]
dt = t[1]-t[0]
for name, i_, u_ in (("A", ia, ua), ("B", ib, ub), ("C", ic, uc)):
    psi = np.maximum(np.cumsum(u_ - mot.R*i_)*dt, 0)
    print(name, "максимум потокосцепления", round(float(psi.max()), 3), "Вб, число переключений", int(np.count_nonzero(np.diff(u_))))"""),
]

for name, cells in NB.items():
    json.dump(nb(cells), open(os.path.join(HERE, name), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("Ноутбуки:", list(NB))
