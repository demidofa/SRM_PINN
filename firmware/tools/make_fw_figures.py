# -*- coding: utf-8 -*-
"""Рисунки к описанию прошивки: структурная схема и результаты проверки контура скорости."""
import os, sys, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
import mstyle
from mstyle import plt, C, finish, lab
from matplotlib.patches import Rectangle
OUT = os.path.join(HERE, "..", "..", "figs")

def structure():
    fig, ax = plt.subplots(figsize=(7.2, 3.9))
    ax.set_xlim(0, 150); ax.set_ylim(0, 82); ax.axis("off")
    def box(x, y, w, h, text, fs=9, ls="-"):
        ax.add_patch(Rectangle((x, y), w, h, fc="white", ec="black", lw=0.9, ls=ls))
        ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs, linespacing=1.2)
    def arr(x0, y0, x1, y1, col="black"):
        ax.annotate("", (x1, y1), (x0, y0), arrowprops=dict(arrowstyle="-|>", color=col, lw=0.9, mutation_scale=9))
    # обработчик прерывания
    ax.add_patch(Rectangle((24, 14), 96, 64, fc="none", ec="black", lw=0.9, ls=(0, (5, 3))))
    ax.text(72, 74.5, "Обработчик прерывания АЦП (10 кГц): srm_ctrl_step()", ha="center", fontsize=9.5)
    box(1, 52, 18, 14, "АЦП:\nтоки фаз,\n$U_{\\mathrm{ЗПТ}}$")
    box(1, 30, 18, 14, "Энкодер:\nугол,\nскорость")
    box(27, 56, 20, 14, "Защиты\nпо току и\nнапряжению")
    box(51, 56, 20, 14, "Регулятор\nскорости")
    box(75, 56, 20, 14, "Окно\nкоммутации\n$\\theta_{\\mathrm{вкл}}$, $\\theta_{\\mathrm{откл}}$")
    box(51, 36, 20, 13, "Наблюдатель\n$\\hat\\psi$")
    box(75, 36, 20, 13, "Таблица\n$\\Psi(\\theta, i)$")
    box(75, 17, 20, 12, "Коррекция\nузлов")
    box(124, 52, 25, 14, "TIM1: верхние\nключи\nTIM8: нижние")
    box(124, 30, 25, 14, "Силовой\nмодуль,\nВИД")
    arr(19, 59, 27, 63); arr(19, 37, 27, 60)
    arr(47, 63, 51, 63); arr(71, 63, 75, 63); arr(95, 63, 99, 63)
    arr(95, 42.5, 99, 42.5)                                   # таблица -> регулятор
    ax.plot([61, 61, 98], [49, 52.5, 52.5], color="black", lw=0.9); arr(98, 52.5, 99, 52.5)   # наблюдатель -> регулятор
    arr(85, 29, 85, 36)                                       # коррекция -> таблица
    arr(99, 23, 95, 23); ax.text(97, 24.2, "$e_i$", fontsize=9, ha="center")
    arr(37, 42.5, 51, 42.5); ax.text(44, 44, "$u$, $i$", fontsize=9, ha="center")
    arr(118, 59, 124, 59); arr(136.5, 52, 136.5, 44)
    ax.plot([136.5, 136.5, 10, 10], [30, 6, 6, 29], color="black", lw=0.9); arr(10, 29, 10, 30)
    ax.text(73, 7.5, "обратная связь по токам и положению ротора", ha="center", fontsize=8, style="italic")
    box(99, 17, 19, 53, "Прогнози-\nрующий\nрегулятор\nтока")
    fig.savefig(os.path.join(OUT, "fig_fw_structure.png"), dpi=300, bbox_inches="tight"); plt.close(fig)

def speed(csv):
    d = np.genfromtxt(csv, delimiter=",", names=True)
    fig, ax = plt.subplots(3, 1, figsize=(7.0, 6.6), sharex=False)
    t = d["t_s"]
    ax[0].plot(t, d["omega_ref_rpm"], color=C[1], label=r"$n_{\mathrm{зад}}$")
    ax[0].plot(t, d["n_rpm"], color=C[0], label="$n$")
    ax[0].set_ylabel(lab("n", "об/мин")); ax[0].set_ylim(0, 1200); ax[0].legend(loc="upper right", ncol=2)
    ax[1].plot(t, d["te_Nm"], color=C[0], lw=0.6, label="$M$")
    ax[1].plot(t, d["t_ref_Nm"], color=C[1], label=r"$M_{\mathrm{зад}}$")
    ax[1].set_ylabel(lab("M", "Н·м")); ax[1].set_ylim(-30, 60); ax[1].legend(loc="upper right", ncol=2)
    ax[2].plot(t, d["udc_V"], color=C[0])
    ax[2].set_ylabel(lab(r"U_{\mathrm{ЗПТ}}", "В"))
    for a in ax:
        a.set_xlim(0, 2.5); a.set_xlabel(lab("t", "с"))
    finish(list(ax), labels=True, y=-0.42)
    fig.tight_layout(h_pad=2.6)
    fig.savefig(os.path.join(OUT, "fig_fw_speed.png"), dpi=300, bbox_inches="tight"); plt.close(fig)

if __name__ == "__main__":
    structure()
    speed(os.path.join(HERE, "..", "sim", "speed_test_8_6.csv"))
    print("ok")
