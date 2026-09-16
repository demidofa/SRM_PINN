# -*- coding: utf-8 -*-
"""
Автоматическая проверка технических требований лабораторных работ.

Проверка выполняется по эталонной модели: там, где возможно, параметры,
заявленные обучающимся, заново моделируются, поэтому результат нельзя
«подогнать» в отчете. Проверяются только численные требования; анализ,
отчет и ответы на вопросы оценивает преподаватель.

Запуск:
  python check_lab.py lab1 путь/к/папке_с_результатами
  python check_lab.py lab2 ...   (аналогично lab3, lab4)
Файлы, которые должна содержать папка:
  lab1: lab1_predictions.csv  (столбцы theta_rad,i_A,psi_Wb на сетке lab1_grid.csv)
  lab2: lab2_params.json      {"theta_on_deg", "theta_off_deg", "i_ref", "band", "soft", "T_from_W"}
  lab3: lab3_trace.csv        (t_s,i_A,i_ref_A,active)  и  lab3_adapt.json {"k", "periods_to_1A"}
  lab4: lab4_estimator.py     функция estimate(La, Lb, Lc) -> электрический угол, рад
Код возврата 0 — все требования выполнены.
"""
import os, sys, json, importlib.util, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts"))
from srm_model import SRM, simulate_time

MOT = SRM()

def grid():
    th = np.linspace(0, np.pi, 31); i = np.linspace(0, 20, 41)
    T, I = np.meshgrid(th, i, indexing="ij")
    return T, I

def req(name, value, limit, ok, unit=""):
    return {"требование": name, "значение": value, "норма": limit, "выполнено": bool(ok), "ед": unit}

def lab1(d):
    T, I = grid()
    data = np.loadtxt(os.path.join(d, "lab1_predictions.csv"), delimiter=",", skiprows=1)
    P = data[:, 2].reshape(T.shape)
    if not (np.allclose(data[:, 0], T.ravel()) and np.allclose(data[:, 1], I.ravel())):
        raise ValueError("сетка lab1_predictions.csv не совпадает с lab1_grid.csv")
    ref = MOT.psi(T, I); low = I < 3
    e_low = float(np.sqrt(np.mean((P[low] - ref[low]) ** 2)))
    e0 = float(np.abs(P[:, 0]).max())
    neg = float(np.mean(np.diff(P, axis=1) < -1e-6) * 100)
    return [req("СКО в области i < 3 А", e_low, "<= 0,01", e_low <= 0.01, "Вб"),
            req("|psi(theta, 0)|", e0, "<= 0,005", e0 <= 0.005, "Вб"),
            req("доля точек с убыванием psi по току", neg, "0", neg == 0, "%")]

def lab2(d):
    p = json.load(open(os.path.join(d, "lab2_params.json"), encoding="utf-8"))
    rpm = 1500; Tel = 60 / rpm / MOT.Nr
    r = simulate_time(MOT, rpm, np.deg2rad(p["theta_on_deg"]), np.deg2rad(p["theta_off_deg"]),
                      p["i_ref"], band=p.get("band", 0.8), soft=p.get("soft", True),
                      t_end=2 * Tel, dt=5e-6)
    n = len(r["Te"]) // 2
    Te = r["Te"][n:]; ia = r["i"][n:, 0]
    T = float(Te.mean()); pk = float(ia.max()); rms = float(np.sqrt(np.mean(ia ** 2)))
    dev = abs(p["T_from_W"] - T) / T * 100
    return [req("средний момент", T, ">= 22", T >= 22, "Н·м"),
            req("пиковый ток фазы", pk, "<= 21", pk <= 21, "А"),
            req("действующий ток фазы", rms, "<= 12,5", rms <= 12.5, "А"),
            req("расхождение момента по энергии цикла", dev, "<= 3", dev <= 3, "%")]

def lab3(d):
    tr = np.loadtxt(os.path.join(d, "lab3_trace.csv"), delimiter=",", skiprows=1)
    act = tr[:, 3] > 0.5
    k0 = np.argmax(tr[:, 1] >= tr[:, 2])          # после первого достижения задания
    m = act & (np.arange(len(tr)) > k0)
    e = float(np.sqrt(np.mean((tr[m, 1] - tr[m, 2]) ** 2)))
    a = json.load(open(os.path.join(d, "lab3_adapt.json"), encoding="utf-8"))
    import srm_adapt
    rr = srm_adapt.run(k=a["k"], cycles=30)["rms"]
    below = np.nonzero(rr <= 1.0)[0]
    n_ref = int(below[0]) + 1 if len(below) else None
    ok_conv = n_ref is not None and abs(a["periods_to_1A"] - n_ref) <= 1
    return [req("СКО тока, период 100 мкс", e, "<= 1,0", e <= 1.0, "А"),
            req("заявленное число периодов до СКО <= 1 А", a["periods_to_1A"],
                f"эталон {n_ref} ± 1", ok_conv, "")]

def lab4(d):
    spec = importlib.util.spec_from_file_location("est", os.path.join(d, "lab4_estimator.py"))
    est = importlib.util.module_from_spec(spec); spec.loader.exec_module(est)
    thm = np.linspace(0, 2 * np.pi / MOT.Nr, 720, endpoint=False)
    L = [MOT.L_unsat(MOT.phase_angle(thm, p)) for p in range(3)]
    th_hat = np.array([est.estimate(L[0][k], L[1][k], L[2][k]) for k in range(len(thm))])
    err = np.rad2deg(np.angle(np.exp(1j * (th_hat - MOT.Nr * thm))))
    amp = float(np.abs(err).max())
    return [req("амплитуда ошибки оценки угла", amp, "<= 2,5", amp <= 2.5, "эл. град"),
            req("исследовательский уровень: амплитуда", amp, "<= 0,5", amp <= 0.5, "эл. град")]

LABS = {"lab1": lab1, "lab2": lab2, "lab3": lab3, "lab4": lab4}

if __name__ == "__main__":
    lab, folder = sys.argv[1], sys.argv[2]
    res = LABS[lab](folder)
    print(f"Проверка {lab}: {folder}")
    for r in res:
        v = r["значение"]; v = f"{v:.4g}" if isinstance(v, float) else str(v)
        print(f"  [{'выполнено' if r['выполнено'] else 'НЕ выполнено'}] {r['требование']}: {v} {r['ед']} (норма {r['норма']})")
    with open(os.path.join(folder, f"{lab}_report.json"), "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=1)
    basic = [r for r in res if not r["требование"].startswith("исследовательский")]
    sys.exit(0 if all(r["выполнено"] for r in basic) else 1)
