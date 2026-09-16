# -*- coding: utf-8 -*-
"""Эталонные решения для самопроверки автопроверки (только для преподавателя)."""
import os, sys, json, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts")); sys.path.insert(0, os.path.join(HERE, "..", "autograder"))
from check_lab import grid
import lab1_pinn_demo as demo
from srm_model import SRM, simulate_time

def w(folder):
    p = os.path.join(HERE, "autograder_examples", folder); os.makedirs(p, exist_ok=True); return p

T, I = grid()
np.savetxt(os.path.join(HERE, "..", "autograder", "lab1_grid.csv"), np.column_stack([T.ravel(), I.ravel()]),
           delimiter=",", header="theta_rad,i_A", comments="", fmt="%.8f")
for name, lam in (("lab1_pinn", 1.0), ("lab1_plain_nn", 0.0)):
    wt = demo.train(lam, 0)
    np.savetxt(os.path.join(w(name), "lab1_predictions.csv"),
               np.column_stack([T.ravel(), I.ravel(), demo.net(wt, T.ravel(), I.ravel())]),
               delimiter=",", header="theta_rad,i_A,psi_Wb", comments="", fmt="%.8f")

m = SRM(); rpm = 1500; Tel = 60 / rpm / m.Nr
for name, on, off in (("lab2_ok", 20, 145), ("lab2_fail", 10, 150)):
    r = simulate_time(m, rpm, np.deg2rad(on), np.deg2rad(off), 20 if name == "lab2_ok" else 15,
                      band=0.8, t_end=2 * Tel, dt=5e-6)
    n = len(r["t"]) // 2
    W = np.trapezoid(r["i"][n:, 0] * r["v"][n:, 0] - m.R * r["i"][n:, 0] ** 2, r["t"][n:])
    json.dump({"theta_on_deg": on, "theta_off_deg": off, "i_ref": 20 if name == "lab2_ok" else 15,
               "band": 0.8, "soft": True, "T_from_W": W * m.m * m.Nr / (2 * np.pi)},
              open(os.path.join(w(name), "lab2_params.json"), "w"), indent=1)

for name, ctrl in (("lab3_mpc", "mpc"), ("lab3_hyst", "hyst")):
    r = simulate_time(m, 600, np.deg2rad(10), np.deg2rad(150), 15, Ts=1e-4, dt=2e-6,
                      t_end=60 / 600 / m.Nr, controller=ctrl)
    act = (np.mod(r["t"] * r["w"] * m.Nr - np.deg2rad(10), 2 * np.pi) < np.deg2rad(140))
    np.savetxt(os.path.join(w(name), "lab3_trace.csv"),
               np.column_stack([r["t"], r["i"][:, 0], np.full(len(act), 15.0), act]),
               delimiter=",", header="t_s,i_A,i_ref_A,active", comments="", fmt="%.6f")
    json.dump({"k": 1e-3, "periods_to_1A": 2}, open(os.path.join(w(name), "lab3_adapt.json"), "w"))

open(os.path.join(w("lab4_clarke"), "lab4_estimator.py"), "w").write(
'''import numpy as np
def estimate(La, Lb, Lc):
    """Оценка электрического угла по индуктивностям фаз через преобразование Кларк."""
    l_al = (2 * La - Lb - Lc) / 3
    l_be = (Lb - Lc) / np.sqrt(3)
    return np.mod(np.arctan2(-l_be, -l_al), 2 * np.pi)
''')
print("Эталонные примеры сформированы")
