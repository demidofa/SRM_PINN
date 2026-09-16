# -*- coding: utf-8 -*-
"""Построение иллюстраций к рабочей программе в стиле учебника. Запуск: python make_figures.py"""
import os, numpy as np
import mstyle
from mstyle import plt, C, finish, lab, num
from matplotlib.patches import Rectangle
from matplotlib.colors import LinearSegmentedColormap
from mpl_toolkits.mplot3d.art3d import Line3DCollection
from srm_model import SRM, simulate_time, cycle_angle_domain

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "figs")
os.makedirs(OUT, exist_ok=True)
m = SRM()
deg = np.deg2rad
PARULA = LinearSegmentedColormap.from_list("parula", [
    (0.2081, 0.1663, 0.5292), (0.0116, 0.3875, 0.8820), (0.0779, 0.5040, 0.8383),
    (0.0226, 0.6117, 0.7690), (0.2178, 0.7250, 0.6108), (0.5270, 0.7740, 0.4165),
    (0.8185, 0.7327, 0.3498), (0.9956, 0.7862, 0.1924), (0.9763, 0.9831, 0.0538)])

def save(fig, name):
    fig.savefig(os.path.join(OUT, name), bbox_inches="tight", dpi=300)
    plt.close(fig)

# ---------------- 1. Структурная схема сквозного проекта ----------------
def fig_roadmap():
    fig, ax = plt.subplots(figsize=(7.0, 3.1))
    ax.set_xlim(0, 140); ax.set_ylim(-3, 62); ax.axis("off")
    kw = dict(fc="white", ec="black", lw=0.9)
    def block(x, y, w, h, head, sub):
        ax.add_patch(Rectangle((x, y), w, h, **kw))
        ax.text(x + w / 2, y + h - 4.2, head, ha="center", va="center", fontsize=10.5)
        ax.text(x + w / 2, y + (h - 8) / 2, sub, ha="center", va="center", fontsize=9, linespacing=1.25)
    def arrow(x0, y0, x1, y1):
        ax.annotate("", (x1, y1), (x0, y0),
                    arrowprops=dict(arrowstyle="-|>", color="black", lw=0.9, mutation_scale=10))
    # стенд
    block(1, 22, 17, 22, "Стенд", "осцилло-\nграммы\n$u(t)$, $i(t)$")
    # цифровой двойник
    ax.add_patch(Rectangle((23, 6), 94, 52, fc="none", ec="black", lw=0.8, ls=(0, (5, 3))))
    ax.text(70, 54.2, "Цифровой двойник вентильно-индукторного привода", ha="center", fontsize=10.5)
    w, h, y = 20.5, 38, 10
    xs = [25.5, 48.5, 71.5, 94.5]
    items = [("ЛР1", "поверхность\nнамагничи-\nвания $\\psi(\\theta, i)$:\nтаблица,\nИНС, ФИНС"),
             ("ЛР2", "двигатель и\nнесимметрич-\nный мост;\nуглы\nкоммутации"),
             ("ЛР3", "контуры\nтока и\nскорости;\nпрогнози-\nрование"),
             ("ЛР4", "торможение;\nоценка\nположения;\nдиагно-\nстика")]
    for x, (hd, sb) in zip(xs, items):
        block(x, y, w, h, hd, sb)
    arrow(18, 33, 25.5, 33)
    for k in range(3):
        arrow(xs[k] + w, 33, xs[k + 1], 33)
    block(122, 22, 17, 22, "Экзамен", "защита\nдвойника")
    arrow(117, 33, 122, 33)
    # обратная связь: адаптация модели в ЛР3 уточняет модель ЛР1
    ax.plot([81.75, 81.75, 35.75, 35.75], [10, 3, 3, 10], color="black", lw=0.8)
    ax.annotate("", (35.75, 10), (35.75, 5), arrowprops=dict(arrowstyle="-|>", color="black", lw=0.8, mutation_scale=9))
    ax.text(58.75, -1.2, "онлайн-идентификация уточняет модель", ha="center", fontsize=8.5, style="italic")
    save(fig, "fig_roadmap.png")

# ---------------- 2. Поверхность намагничивания ----------------
def mesh3d(ax, X, Y, Z, cmap=PARULA, lw=0.6):
    segs, cols = [], []
    norm = lambda z: (z - Z.min()) / (Z.max() - Z.min())
    for a in range(Z.shape[0]):
        for b in range(Z.shape[1]):
            for da, db in ((1, 0), (0, 1)):
                a2, b2 = a + da, b + db
                if a2 < Z.shape[0] and b2 < Z.shape[1]:
                    segs.append([(X[a, b], Y[a, b], Z[a, b]), (X[a2, b2], Y[a2, b2], Z[a2, b2])])
                    cols.append(cmap(norm(0.5 * (Z[a, b] + Z[a2, b2]))))
    ax.add_collection3d(Line3DCollection(segs, colors=cols, linewidths=lw))
    ax.set_xlim(X.min(), X.max()); ax.set_ylim(Y.min(), Y.max()); ax.set_zlim(0, Z.max() * 1.05)
    for axis in (ax.xaxis, ax.yaxis, ax.zaxis):
        axis.pane.set_facecolor("white"); axis.pane.set_edgecolor("#bdbdbd")
        axis._axinfo["grid"]["color"] = "#dedede"; axis._axinfo["grid"]["linewidth"] = 0.5

def fig_surface():
    fig = plt.figure(figsize=(7.0, 6.4))
    ax1 = fig.add_axes([0.08, 0.62, 0.38, 0.33]); ax2 = fig.add_axes([0.58, 0.62, 0.38, 0.33])
    th = np.linspace(0, 2 * np.pi, 400)
    ax1.plot(np.rad2deg(th), m.L_unsat(th) * 1e3, color=C[0])
    ax1.set_xlim(0, 360); ax1.set_xticks([0, 90, 180, 270, 360])
    ax1.set_xlabel(lab(r"\theta_{\mathrm{эл}}", "град")); ax1.set_ylabel(lab("L", "мГн"))
    i = np.linspace(0, 30, 200)
    for k, a in enumerate((0, 45, 90, 135, 180)):
        ax2.plot(i, m.psi(deg(a), i), color=C[k], label=f"{a}°")
    ax2.set_xlim(0, 30); ax2.set_xlabel(lab("i", "А")); ax2.set_ylabel(lab(r"\psi", "Вб"))
    ax2.legend(loc="upper left", fontsize=9)
    finish([ax1, ax2], labels=True, y=-0.33)
    ax3 = fig.add_axes([0.12, 0.0, 0.76, 0.46], projection="3d")
    ax3.patch.set_alpha(0)
    TH, I = np.meshgrid(np.linspace(0, 2 * np.pi, 37), np.linspace(0, 30, 16), indexing="ij")
    mesh3d(ax3, TH, I, m.psi(TH, I))
    ax3.view_init(elev=24, azim=-58)
    ax3.set_xticks([0, np.pi / 2, np.pi, 3 * np.pi / 2, 2 * np.pi])
    ax3.set_xticklabels(["0", "π/2", "π", "3π/2", "2π"])
    ax3.set_yticks([0, 10, 20, 30]); ax3.set_zticks([0, 0.5, 1.0, 1.5])
    ax3.set_zticklabels(["0", "0,5", "1", "1,5"])
    ax3.set_xlabel(lab(r"\theta_{\mathrm{эл}}", "рад")); ax3.set_ylabel(lab("i", "А")); ax3.set_zlabel(lab(r"\psi", "Вб"))
    ax3.text2D(0.5, -0.10, "$в$)", transform=ax3.transAxes, ha="center", fontsize=12)
    save(fig, "fig_surface.png")

# ---------------- 3. Цикл преобразования энергии ----------------
def energy(r):
    return np.trapezoid(r["i"][:, 0] * r["v"][:, 0] - m.R * r["i"][:, 0] ** 2, r["t"])

def fig_energy_loop():
    fig, ax = plt.subplots(1, 2, figsize=(7.0, 3.0), sharey=True)
    cases = [(600, deg(10), deg(150), 15), (3000, deg(0), deg(140), 40)]
    ii = np.linspace(0, 30, 200); Ws = []
    for a, (rpm, on, off, iref) in zip(ax, cases):
        r = simulate_time(m, rpm, on, off, iref, band=0.8, dt=2e-6, t_end=2 * 60 / rpm / m.Nr)
        n = len(r["t"]) // 2
        r = {k: (v[n:] if isinstance(v, np.ndarray) else v) for k, v in r.items()}
        i, psi = r["i"][:, 0], r["psi"][:, 0]
        a.plot(ii, m.psi(np.pi, ii), color="k", lw=0.7, ls="--")
        a.plot(ii, m.psi(0, ii), color="k", lw=0.7, ls="--")
        a.fill(i, psi, color=C[0], alpha=0.15, lw=0)
        a.plot(i, psi, color=C[0])
        Ws.append(energy(r))
        a.set_xlim(0, 30); a.set_ylim(0, 1.5); a.set_xlabel(lab("i", "А"))
    ax[0].set_ylabel(lab(r"\psi", "Вб"))
    finish(list(ax), labels=True)
    fig.tight_layout(); save(fig, "fig_energy_loop.png")
    return Ws

# ---------------- 4. Токи и момент при релейном регулировании ----------------
def fig_hysteresis():
    rpm = 600
    r = simulate_time(m, rpm, deg(10), deg(150), 15, band=0.8, dt=2e-6, t_end=2 * 60 / rpm / m.Nr)
    n = len(r["t"]) // 2
    t = (r["t"][n:] - r["t"][n]) * 1e3
    fig, ax = plt.subplots(2, 1, figsize=(7.0, 4.6))
    for p in range(3):
        ax[0].plot(t, r["i"][n:, p], color=C[p], label=f"${'ABC'[p]}$")
    ax[0].set_ylabel(lab("i", "А")); ax[0].set_xlabel(lab("t", "мс"))
    ax[0].legend(loc="upper right", ncol=3); ax[0].set_ylim(0, 25)
    Te = r["Te"][n:]
    ax[1].plot(t, Te, color=C[0], label="$M$")
    ax[1].plot(t, np.full_like(t, Te.mean()), color=C[1], label=r"$M_{\mathrm{ср}}$")
    ax[1].set_ylabel(lab("M", "Н·м")); ax[1].set_xlabel(lab("t", "мс")); ax[1].set_ylim(0, 50)
    ax[1].legend(loc="upper right", ncol=2)
    for a in ax: a.set_xlim(0, t[-1])
    finish(list(ax), labels=True, y=-0.36)
    fig.tight_layout(h_pad=2.2); save(fig, "fig_hysteresis.png")
    return Te.mean(), (Te.max() - Te.min()) / Te.mean() * 100

# ---------------- 5. Предельная механическая характеристика ----------------
def fig_limit_curve(i_max=20.0):
    rpms = np.array([200, 400, 700, 1000, 1300, 1600, 2000, 2500, 3000, 3500, 4000, 4500, 5000])
    res = {"mot": [], "gen": []}
    grids = {"mot": np.meshgrid(deg(np.arange(-60, 61, 2.5)), deg(np.arange(90, 181, 2.5))),
             "gen": np.meshgrid(deg(np.arange(100, 241, 2.5)), deg(np.arange(190, 381, 2.5)))}
    for rpm in rpms:
        for mode, (on, off) in grids.items():
            T, Ir, cont, pk = cycle_angle_domain(m, rpm, on, off, i_max)
            ok = (~cont) & (pk <= 1.10 * i_max)
            # критерий 1: максимум модуля момента; критерий 2 (в пределах 1 % от максимума):
            # минимум действующего тока, т.е. минимум потерь в меди
            Ta = np.where(ok, T if mode == "mot" else -T, -np.inf)
            near = Ta >= 0.99 * Ta.max()
            j = np.unravel_index(np.where(near, Ir, np.inf).argmin(), T.shape)
            res[mode].append((T[j], np.rad2deg(on[j]), np.rad2deg(off[j]), Ir[j]))
    mot, gen = np.array(res["mot"]), np.array(res["gen"])
    fig, ax = plt.subplots(1, 2, figsize=(7.0, 3.0))
    ax[0].plot(mot[:, 0], rpms, "-o", color=C[0], ms=3, label="двигательный")
    ax[0].plot(gen[:, 0], rpms, "-s", color=C[1], ms=3, label="генераторный")
    ax[0].axvline(0, color="k", lw=0.6)
    ax[0].set_xlabel(lab("M", "Н·м")); ax[0].set_ylabel(lab("n", "об/мин"))
    ax[0].set_xlim(-30, 30); ax[0].set_ylim(0, 5200); ax[0].legend(loc="center", fontsize=9)
    ax[1].plot(rpms, mot[:, 1], "-o", color=C[0], ms=3, label=r"$\theta_{\mathrm{вкл}}$")
    ax[1].plot(rpms, mot[:, 2], "-s", color=C[1], ms=3, label=r"$\theta_{\mathrm{откл}}$")
    ax[1].set_xlabel(lab("n", "об/мин")); ax[1].set_ylabel(lab(r"\theta_{\mathrm{эл}}", "град"))
    ax[1].set_xlim(0, 5200); ax[1].set_ylim(-10, 170); ax[1].legend(loc="center right")
    finish(list(ax), labels=True)
    fig.tight_layout(); save(fig, "fig_limit_curve.png")
    w = rpms * 2 * np.pi / 60
    np.savetxt(os.path.join(OUT, "limit_curve.csv"),
               np.column_stack([rpms, mot, gen, mot[:, 0] * w]), delimiter=";",
               header="rpm;T_mot;on_mot;off_mot;Irms_mot;T_gen;on_gen;off_gen;Irms_gen;P_mot_W",
               fmt="%.3f")
    return rpms, mot, gen

# ---------------- 6. Релейный регулятор и прогнозирование при одинаковом Ts ----------------
def fig_mpc():
    rpm, Ts = 600, 1e-4
    t_end = 60 / rpm / m.Nr
    kw = dict(dt=2e-6, t_end=t_end, Ts=Ts)
    rh = simulate_time(m, rpm, deg(10), deg(150), 15, controller="hyst", **kw)
    rm = simulate_time(m, rpm, deg(10), deg(150), 15, controller="mpc", **kw)
    fig, ax = plt.subplots(1, 2, figsize=(7.0, 2.9), sharey=True)
    stats = []
    for a, r in zip(ax, (rh, rm)):
        t = r["t"] * 1e3; i = r["i"][:, 0]
        on = (np.mod(r["t"] * r["w"] * m.Nr - deg(10), 2 * np.pi) < deg(140))
        a.plot(t, i, color=C[0], label="$i$")
        a.plot(t, np.where(on, 15.0, np.nan), color=C[1], label=r"$i_{\mathrm{зад}}$")
        a.set_xlabel(lab("t", "мс")); a.set_xlim(0, 14); a.set_ylim(0, 20)
        k0 = np.argmax(i > 15)
        mask = on & (np.arange(len(i)) > k0)
        stats.append((np.sqrt(np.mean((i[mask] - 15) ** 2)), np.count_nonzero(np.diff(r["v"][:, 0]) != 0)))
    ax[0].set_ylabel(lab("i", "А")); ax[1].legend(loc="lower center", ncol=2)
    finish(list(ax), labels=True)
    fig.tight_layout(); save(fig, "fig_mpc.png")
    return stats

# ---------------- 7. Бездатчиковая оценка положения ----------------
def fig_sensorless():
    thm = np.linspace(0, 2 * np.pi / m.Nr, 500, endpoint=False)
    th = m.Nr * thm
    L = np.array([m.L_unsat(m.phase_angle(thm, p)) for p in range(3)])
    La, Lb, Lc = L
    Lal = (2 * La - Lb - Lc) / 3; Lbe = (Lb - Lc) / np.sqrt(3)
    est = np.mod(np.arctan2(-Lbe, -Lal), 2 * np.pi)
    err = np.rad2deg(np.angle(np.exp(1j * (est - th))))
    fig, ax = plt.subplots(1, 2, figsize=(7.0, 2.9))
    for p in range(3):
        ax[0].plot(np.rad2deg(th), L[p] * 1e3, color=C[p], label=f"$L_{{{'ABC'[p]}}}$")
    ax[0].set_ylabel(lab("L", "мГн")); ax[0].set_ylim(0, 160); ax[0].legend(loc="upper center", ncol=3, fontsize=9)
    ax[1].plot(np.rad2deg(th), err, color=C[0])
    ax[1].set_ylabel(lab(r"\Delta\theta_{\mathrm{эл}}", "град")); ax[1].set_ylim(-3, 3)
    for a in ax:
        a.set_xticks([0, 90, 180, 270, 360]); a.set_xlim(0, 360)
        a.set_xlabel(lab(r"\theta_{\mathrm{эл}}", "град"))
    finish(list(ax), labels=True)
    fig.tight_layout(); save(fig, "fig_sensorless.png")
    return np.abs(err).max()

if __name__ == "__main__":
    fig_roadmap(); fig_surface()
    print("W, Дж:", [round(w, 2) for w in fig_energy_loop()])
    print("hyst:", fig_hysteresis())
    fig_limit_curve()
    print("mpc:", fig_mpc())
    print("sensorless max err:", fig_sensorless())
