# -*- coding: utf-8 -*-
"""Формирование таблиц поверхности намагничивания для прошивки (core/srm_lut_data.c)."""
import os, sys, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
from srm_model import SRM

NTH, NI, IMAX = 36, 21, 30.0

def table(mot):
    th = np.linspace(0, 2 * np.pi, NTH + 1)[:-1]
    i = np.linspace(0, IMAX, NI)
    T, I = np.meshgrid(th, i, indexing="ij")
    return mot.psi(T, I)

def c_array(name, P, comment):
    rows = ",\n".join("    { " + ", ".join(f"{v:.6f}f" for v in r) + " }" for r in P)
    return f"/* {comment} */\nconst float {name}[SRM_LUT_NTH][SRM_LUT_NI] = {{\n{rows}\n}};\n"

if __name__ == "__main__":
    true = SRM()
    err = SRM(La=true.Lu + 0.8 * (true.La - true.Lu))
    out = os.path.join(HERE, "..", "core", "srm_lut_data.c")
    with open(out, "w", encoding="utf-8") as f:
        f.write("/* Сформировано tools/gen_lut.py. Строки — электрический угол 0..2pi (шаг 10 град),\n"
                "   столбцы — ток 0..30 А (шаг 1,5 А), значения — потокосцепление, Вб. */\n")
        f.write('#include "srm_lut.h"\n\n')
        f.write(c_array("SRM_LUT_TRUE", table(true), "точная таблица учебной модели"))
        f.write("\n")
        f.write(c_array("SRM_LUT_INIT", table(err),
                        "таблица с индуктивностью в согласованном положении на 20 % ниже фактической"))
    print("Сформирован", os.path.abspath(out), f"({NTH}x{NI}, {NTH*NI*4} байт)")
