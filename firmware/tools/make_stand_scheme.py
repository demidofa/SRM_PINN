# -*- coding: utf-8 -*-
"""Структурная схема лабораторного стенда: ВИД 8/6, четыре несимметричных моста, TMS320F28379D."""
import os, sys, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
import mstyle
from mstyle import plt
from matplotlib.patches import Rectangle, Polygon, Circle, Arc

OUT = os.path.join(HERE, "..", "..", "figs", "fig_stand_scheme.png")
K = "black"; BL = "#0000FF"; RD = "#FF0000"; GR = "#00C800"
LW = 1.1

fig, ax = plt.subplots(figsize=(13.0, 4.5))
ax.set_xlim(-2, 372); ax.set_ylim(-6, 118); ax.set_aspect("equal"); ax.axis("off")

def line(xs, ys, c=K, lw=LW, **kw):
    ax.plot(xs, ys, color=c, lw=lw, solid_capstyle="round", **kw)

def dot(x, y):
    ax.add_patch(Circle((x, y), 0.9, color=K, zorder=5))

def arrow(x0, y0, x1, y1, c):
    ax.annotate("", (x1, y1), (x0, y0), arrowprops=dict(arrowstyle="-|>", color=c, lw=LW,
                mutation_scale=13, shrinkA=0, shrinkB=0))

def igbt(x, y, label, lx):
    """IGBT: коллектор сверху (y+6), эмиттер снизу (y-6), затвор слева."""
    line([x - 2.6, x - 2.6], [y - 3.2, y + 3.2], lw=1.6)
    line([x - 4.0, x - 4.0], [y - 2.6, y + 2.6], lw=1.6)
    line([x - 4.0, x - 9.0], [y, y])
    line([x - 2.6, x, x], [y + 1.6, y + 3.6, y + 6])
    line([x - 2.6, x, x], [y - 1.6, y - 3.6, y - 6])
    ax.annotate("", (x - 0.3, y - 3.4), (x - 2.0, y - 2.0),
                arrowprops=dict(arrowstyle="-|>", color=K, lw=LW, mutation_scale=8, shrinkA=0, shrinkB=0))
    ax.text(lx, y, label, fontsize=13, ha="left", va="center", style="italic")
    return (x - 9.0, y)                       # точка подключения затвора

def diode(x, y, label, lx, ha="left"):
    """Диод, катод сверху: анод y-6, катод y+6."""
    line([x, x], [y - 6, y - 2.6]); line([x, x], [y + 2.6, y + 6])
    ax.add_patch(Polygon([[x - 3.2, y - 2.6], [x + 3.2, y - 2.6], [x, y + 2.6]], closed=True,
                         fc="white", ec=K, lw=LW))
    line([x - 3.4, x + 3.4], [y + 2.6, y + 2.6], lw=1.6)
    ax.text(lx, y, label, fontsize=13, ha=ha, va="center", style="italic")

# шины звена постоянного тока
YP, YN = 64, 2
X0, XE = 10, 244
line([X0, XE], [YP, YP]); line([X0, XE], [YN, YN])
dot(X0, YP); dot(X0, YN)
arrow(X0, 20, X0, YP - 3, K); arrow(X0, 46, X0, YN + 3, K)
ax.text(1, 33, r"$U_{\mathrm{ЗПТ}}$", fontsize=14, ha="center", va="center")
xc = 20
dot(xc, YP); dot(xc, YN)
line([xc, xc], [YP, 34.5]); line([xc, xc], [31.5, YN])
line([xc - 5, xc + 5], [34.5, 34.5], lw=1.8); line([xc - 5, xc + 5], [31.5, 31.5], lw=1.8)
ax.text(xc - 1, 39.5, r"$C_{\mathrm{ЗПТ}}$", fontsize=14, ha="center")

# четыре несимметричных моста
pairs = [(38, 64), (98, 124), (158, 184), (218, 244)]
wire_hi = [44, 41, 38, 35]               # провода от узлов «верхний ключ — нижний диод»
wire_lo = [27, 24, 21, 18]               # провода от узлов «верхний диод — нижний ключ»
gates = []
XM = 300                                  # начало статора
for k, (xa, xb) in enumerate(pairs):
    n1, n2 = 2 * k + 1, 2 * k + 2
    # левая ветвь: верхний транзистор, нижний диод
    g1 = igbt(xa, 52, f"VT{n1}", xa + 2)
    diode(xa, 13, f"VD{n1}", xa - 4.5, ha="right")
    line([xa, xa], [YP, 58]); line([xa, xa], [46, 19]); line([xa, xa], [7, YN])
    dot(xa, YP); dot(xa, YN); dot(xa, wire_hi[k])
    # правая ветвь: верхний диод, нижний транзистор
    diode(xb, 52, f"VD{n2}", xb + 4)
    g2 = igbt(xb, 13, f"VT{n2}", xb + 2)
    line([xb, xb], [YP, 58]); line([xb, xb], [46, 19]); line([xb, xb], [7, YN])
    dot(xb, YP); dot(xb, YN); dot(xb, wire_lo[k])
    gates += [g1, g2]
    line([xa, XM - 12], [wire_hi[k], wire_hi[k]])
    line([xb, XM - 12], [wire_lo[k], wire_lo[k]])

# система управления
YB = 84
ax.add_patch(Rectangle((14, YB), 356, 32, fc="white", ec=K, lw=LW))
ax.text(192, 110, "Система управления на TMS320F28379D", fontsize=15, ha="center", va="center")
ax.add_patch(Rectangle((24, YB), 232, 16, fc="none", ec=K, lw=LW, ls=(0, (6, 3))))
ax.text(140, YB + 8, "ePWM1–ePWM4: управление ключами", fontsize=13.5, ha="center", va="center")
ax.add_patch(Rectangle((260, YB), 44, 16, fc="none", ec=K, lw=LW, ls=(0, (6, 3))))
ax.text(282, YB + 8, "АЦП: токи", fontsize=13.5, ha="center", va="center")
ax.add_patch(Rectangle((308, YB), 60, 16, fc="none", ec=K, lw=LW, ls=(0, (6, 3))))
ax.text(338, YB + 8, "eQEP: положение", fontsize=13.5, ha="center", va="center")

# сигналы управления ключами
for (gx, gy) in gates:
    line([gx, gx], [YB, gy + 0.2], c=BL)
    arrow(gx, gy + 6, gx, gy + 0.1, BL)

# датчики тока (перескок проводов) и измерение
for k, y in enumerate(wire_hi):
    xs = 266 + 10 * k
    arrow(xs, y, xs, YB, RD)
    ax.add_patch(Circle((xs, y), 0.9, color=RD, zorder=6))

# ВИД 8/6
CX, CY, RO = 338, 34, 32
ax.add_patch(Circle((CX, CY), RO, fc="white", ec=K, lw=LW))
for y in wire_hi + wire_lo:
    line([XM - 12, CX - np.sqrt(RO**2 - (y - CY) ** 2)], [y, y])
Ry, Rs, Rr, Rrr = 27.0, 14.5, 13.8, 9.0          # ярмо, расточка статора, ротор (зубец, впадина)
tw = np.deg2rad(11)                               # полуширина зубца статора
names = ["A", "B", "C", "D", "A’", "B’", "C’", "D’"]
for j in range(8):
    a = np.pi / 2 - j * np.pi / 4
    ca, sa = np.cos(a), np.sin(a)
    px, py = -sa, ca
    w = 3.3
    pts = [(CX + Ry * ca + w * px, CY + Ry * sa + w * py), (CX + Rs * ca + w * px, CY + Rs * sa + w * py),
           (CX + Rs * ca - w * px, CY + Rs * sa - w * py), (CX + Ry * ca - w * px, CY + Ry * sa - w * py)]
    ax.add_patch(Polygon(pts, closed=True, fc="white", ec=K, lw=LW))
    for s in (+1, -1):                            # катушки по обе стороны зубца
        c0 = (CX + 21.5 * ca + s * (w + 1.5) * px, CY + 21.5 * sa + s * (w + 1.5) * py)
        cp = [(c0[0] + dx * ca + dy * px, c0[1] + dx * sa + dy * py)
              for dx, dy in ((-3.5, -1.3), (3.5, -1.3), (3.5, 1.3), (-3.5, 1.3))]
        ax.add_patch(Polygon(cp, closed=True, fc="#9a9a9a", ec=K, lw=0.8, hatch="xxxx"))
    lb = a + np.deg2rad(14)
    ax.text(CX + 29.6 * np.cos(lb), CY + 29.6 * np.sin(lb), names[j], fontsize=10.5,
            ha="center", va="center", style="italic")
# ярмо статора
ax.add_patch(Circle((CX, CY), Ry, fc="none", ec=K, lw=LW))
# ротор с шестью зубцами
phi = np.linspace(0, 2 * np.pi, 721)
rel = np.mod(phi - np.pi / 2 + np.pi / 6, np.pi / 3) - np.pi / 6
r = np.where(np.abs(rel) < np.deg2rad(11), Rr, Rrr)
ax.add_patch(Polygon(np.column_stack([CX + r * np.cos(phi), CY + r * np.sin(phi)]), closed=True,
                     fc="white", ec=K, lw=LW))
line([CX - Ry, CX + Ry], [CY, CY], lw=0.7, ls=(0, (4, 3)))
line([CX, CX], [CY - Ry, CY + Ry], lw=0.7, ls=(0, (4, 3)))
# датчик положения
line([CX, CX + 10], [CY, YB - 0.5], c=GR)
arrow(CX + 8, CY + (YB - CY) * 8 / 10, CX + 10, YB, GR)

fig.savefig(OUT, dpi=300, bbox_inches="tight")
print(OUT)
