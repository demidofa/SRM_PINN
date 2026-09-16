# -*- coding: utf-8 -*-
"""
«Неисправный привод»: генератор диагностических задач.

Для каждого варианта моделируется трехфазный ВИД при постоянной скорости с одной
скрытой неисправностью (или без нее). Обучающийся получает только «осциллограмму»
в CSV: время, угол по датчику положения, измеренные токи и напряжения фаз, и должен
установить вид неисправности и обосновать вывод расчетом.

Неисправности (упрощенные модели):
  healthy        — исправный привод;
  open_phase     — обрыв цепи одной фазы;
  turn_fault     — межвитковое замыкание: потокосцепление фазы снижено на 25 %
                   (упрощенно, без учета тока в короткозамкнутом контуре);
  encoder_offset — смещение датчика положения на 15 эл. град;
  current_offset — смещение нуля датчика тока одной фазы на +2 А;
  high_resistance— плохой контакт: сопротивление цепи фазы увеличено в 6 раз.

Запуск (преподаватель):
  python make_cases.py --n 30 --seed 2026 --out cases
Создаются cases/case_XX.csv для обучающихся и cases/answers_instructor.json.
"""
import os, sys, json, argparse, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts"))
from srm_model import SRM, in_window

FAULTS = ["healthy", "open_phase", "turn_fault", "encoder_offset",
          "current_offset", "high_resistance"]

def simulate_case(fault, phase, rpm, i_ref, rng, dt=1e-5, periods=3):
    mot = SRM()
    on, off, band = np.deg2rad(10), np.deg2rad(150), 0.8
    w = rpm * 2 * np.pi / 60
    n = int(round(periods * 2 * np.pi / (mot.Nr * w) / dt))
    stroke = 2 * np.pi / (mot.m * mot.Nr)
    kpsi = np.ones(3); R = np.full(3, mot.R); ioff = np.zeros(3)
    th_off_sensor = 0.0; open_ph = -1
    if fault == "open_phase": open_ph = phase
    if fault == "turn_fault": kpsi[phase] = 0.75
    if fault == "high_resistance": R[phase] = 6 * mot.R
    if fault == "current_offset": ioff[phase] = 2.0
    if fault == "encoder_offset": th_off_sensor = np.deg2rad(15) / mot.Nr   # мех. рад
    psi = np.zeros(3); i = np.zeros(3); v = np.zeros(3)
    rec = np.zeros((n, 8))
    for k in range(n):
        thm = w * k * dt
        thm_meas = thm + th_off_sensor
        i_meas = i + ioff + 0.05 * rng.standard_normal(3)
        for p in range(3):
            th_c = np.mod(mot.Nr * (thm_meas - p * stroke), 2 * np.pi)   # угол для управления
            if in_window(th_c, on, off):
                if i_meas[p] > i_ref + band: v[p] = 0.0
                elif i_meas[p] < i_ref - band: v[p] = mot.Udc
            else:
                v[p] = -mot.Udc if i_meas[p] > 0.3 else 0.0
            if p == open_ph:
                v[p] = 0.0   # цепь разомкнута: ток не протекает, напряжение на фазе не формируется
        for p in range(3):
            th = np.mod(mot.Nr * (thm - p * stroke), 2 * np.pi)
            if p == open_ph:
                psi[p] = 0.0; i[p] = 0.0; continue
            psi[p] = max(psi[p] + (v[p] - R[p] * i[p]) * dt, 0.0)
            i[p] = mot.current(th, psi[p] / kpsi[p], i[p])
        rec[k] = [k * dt, np.mod(thm_meas, 2 * np.pi), *(i + ioff + 0.05 * rng.standard_normal(3)), *v]
    return rec

def figure(path):
    import mstyle
    from mstyle import plt, C, finish, lab
    fig, axs = plt.subplots(3, 2, figsize=(7.0, 7.4), sharey=True)
    for ax, fault in zip(axs.ravel(), FAULTS):
        rec = simulate_case(fault, 1, 450, 12.0, np.random.default_rng(0), periods=2)
        t = rec[:, 0] * 1e3; half = len(t) // 2
        for j in range(3):
            ax.plot(t[half:] - t[half], rec[half:, 2 + j], color=C[j], lw=0.8, label=f"${'ABC'[j]}$")
        ax.set_xlim(0, t[-1] - t[half]); ax.set_ylim(-1, 20)
        ax.set_xlabel(lab("t", "мс"))
    for ax in axs[:, 0]: ax.set_ylabel(lab("i", "А"))
    axs[0, 0].legend(loc="upper right", ncol=3, fontsize=9)
    finish(list(axs.ravel()), labels=True, y=-0.34)
    fig.tight_layout(h_pad=2.4); fig.savefig(path, dpi=300, bbox_inches="tight")


if __name__ == "__main__":
    if "--figure" in sys.argv:
        figure(os.path.join(HERE, "..", "figs", "fig_faults.png")); sys.exit()
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=30)
    ap.add_argument("--seed", type=int, default=2026)
    ap.add_argument("--out", default=os.path.join(HERE, "cases"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rng = np.random.default_rng(a.seed)
    answers = {}
    for c in range(1, a.n + 1):
        fault = FAULTS[c % len(FAULTS)] if c <= len(FAULTS) else str(rng.choice(FAULTS))
        phase = int(rng.integers(0, 3))
        rpm = int(rng.choice([300, 450, 600])); i_ref = float(rng.choice([10, 12, 15]))
        rec = simulate_case(fault, phase, rpm, i_ref, rng)
        name = f"case_{c:02d}.csv"
        hdr = (f"Вариант {c}. Скорость {rpm} об/мин, задание тока {i_ref:.0f} А, углы 10/150 эл. град, "
               f"Udc 540 В, R фазы 0,6 Ом (паспорт)\n"
               "t_s,theta_mech_sensor_rad,ia_A,ib_A,ic_A,ua_V,ub_V,uc_V")
        np.savetxt(os.path.join(a.out, name), rec[::2], delimiter=",", header=hdr,
                   fmt=["%.6f", "%.5f", "%.3f", "%.3f", "%.3f", "%.0f", "%.0f", "%.0f"],
                   encoding="utf-8")
        answers[name] = {"fault": fault, "phase": "ABC"[phase] if fault != "healthy" and fault != "encoder_offset" else None}
        print(name, answers[name])
    with open(os.path.join(a.out, "answers_instructor.json"), "w", encoding="utf-8") as f:
        json.dump(answers, f, ensure_ascii=False, indent=1)
