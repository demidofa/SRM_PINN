# -*- coding: utf-8 -*-
"""
Оформление графиков в стиле иллюстраций учебника «Вентильно-индукторный электропривод»:
шрифт с метрикой Times New Roman, рамка осей, светлая сетка, цвета MATLAB,
десятичная запятая, курсивные обозначения величин, подписи панелей «а)», «б)» под осями.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from cycler import cycler

C = ["#0072BD", "#D95319", "#EDB120", "#7E2F8E", "#77AC30", "#4DBEEE", "#A2142F"]
SERIF = ["Times New Roman", "Liberation Serif", "DejaVu Serif"]

plt.rcParams.update({
    "font.family": "serif", "font.serif": SERIF, "font.size": 11,
    "mathtext.fontset": "custom", "mathtext.rm": SERIF[1], "mathtext.it": SERIF[1] + ":italic",
    "mathtext.bf": SERIF[1] + ":bold",
    "axes.prop_cycle": cycler(color=C), "axes.linewidth": 0.6, "axes.edgecolor": "#262626",
    "axes.grid": True, "grid.color": "#dedede", "grid.linewidth": 0.5,
    "axes.spines.top": True, "axes.spines.right": True, "axes.titlesize": 11,
    "xtick.direction": "in", "ytick.direction": "in", "xtick.top": True, "ytick.right": True,
    "xtick.major.size": 3, "ytick.major.size": 3, "xtick.major.width": 0.6, "ytick.major.width": 0.6,
    "lines.linewidth": 1.0, "legend.frameon": True, "legend.fancybox": False,
    "legend.edgecolor": "#262626", "legend.framealpha": 1.0, "legend.fontsize": 10,
    "legend.borderpad": 0.4, "figure.dpi": 150, "savefig.dpi": 300,
})

def _comma(x, pos):
    s = f"{x:.10g}"
    return s.replace(".", ",").replace("-", "\u2212")

def finish(ax_list, labels=None, y=-0.30):
    """Десятичная запятая на осях и подписи панелей а), б), ... под осями."""
    if not isinstance(ax_list, (list, tuple)):
        ax_list = list(getattr(ax_list, "ravel", lambda: [ax_list])())
    letters = "абвгдежзик"
    for k, ax in enumerate(ax_list):
        ax.xaxis.set_major_formatter(FuncFormatter(_comma))
        ax.yaxis.set_major_formatter(FuncFormatter(_comma))
        if labels:
            ax.text(0.5, y, f"${letters[k]}$)", transform=ax.transAxes,
                    ha="center", va="top", fontsize=12)

def lab(var, unit=None):
    """Подпись оси: курсивное обозначение и единица, например lab('t', 'мс') -> 't, мс'."""
    return f"${var}$" + (f", {unit}" if unit else "")

def num(x, d=1):
    return f"{x:.{d}f}".replace(".", ",")
