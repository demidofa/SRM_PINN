# -*- coding: utf-8 -*-
"""
Сравнение результатов прошивки с эталонной моделью Python.
  python compare_python.py [--motor 6_4|8_6]         — файлы equiv_*, полученные на ПК (make test)
  python compare_python.py [--motor 6_4|8_6] --m4    — файл qemu/equiv_k1e-3_*_m4.csv (эмулируемый Cortex-M4F)
"""
import os, sys, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
import srm_adapt
motor = sys.argv[sys.argv.index("--motor") + 1] if "--motor" in sys.argv else "6_4"
mot_kw = {"6_4": {}, "8_6": dict(Ns=8, Nr=6, m=4)}[motor]
if "--m4" in sys.argv:
    cases = ((1e-3, os.path.join(HERE, "qemu", f"equiv_k1e-3_{motor}_m4.csv")),)
else:
    cases = ((0.0, os.path.join(HERE, f"equiv_k0_{motor}.csv")), (1e-3, os.path.join(HERE, f"equiv_k1e-3_{motor}.csv")))
ok = True
for k, path in cases:
    fw = np.loadtxt(path, delimiter=",", skiprows=1)[:, 1]
    py = srm_adapt.run(k=k, cycles=30, adapt=k > 0, mot_kw=mot_kw)["rms"]
    d = np.abs(fw - py).max()
    good = d <= 0.02
    ok &= good
    print(f"ВИД {motor.replace('_', '/')}, k = {k:g}: максимальное расхождение СКО тока прошивка/Python {d:.4f} А "
          f"[{'выполнено' if good else 'НЕ выполнено'}, норма 0,02 А] ({os.path.basename(path)})")
    print("   прошивка:", np.round(fw[[0, 2, 5, 10, 29]], 3), "\n   Python:  ", np.round(py[[0, 2, 5, 10, 29]], 3))
sys.exit(0 if ok else 1)
