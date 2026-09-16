# -*- coding: utf-8 -*-
"""
ЛР1, демонстрационный пример: нейросеть без физических ограничений и
физически информированная нейросеть (ФИНС) для поверхности намагничивания ВИД.

Данные синтезируются из учебной модели srm_model.SRM так, как их дал бы
стенд с заторможенным ротором: 17 угловых положений, токи от 3 до 20 А
(ниже 3 А измерение ненадежно), шум датчиков.

Физические ограничения ФИНС, проверяемые в точках коллокации по всей области,
включая неизмеренную область малых токов:
  1) psi(theta, 0) = 0 — нет потокосцепления без тока;
  2) d psi / d i >= 0 — потокосцепление не убывает с ростом тока.

Для простоты используется однослойная сеть и оптимизатор L-BFGS из SciPy
(только NumPy и SciPy, без библиотек глубокого обучения). В лабораторной работе
обучающиеся переносят пример на PyTorch и на экспериментальные данные стенда.
"""
import os, numpy as np
from scipy.optimize import minimize
from srm_model import SRM

rng = np.random.default_rng(7)
mot = SRM()
I_MAX = 20.0
H = 12  # число нейронов скрытого слоя

# ---------- «экспериментальные» данные ----------
th_meas = np.linspace(0, np.pi, 17)                  # половина периода, симметрия
i_meas = np.linspace(3, 20, 12)
TH, II = np.meshgrid(th_meas, i_meas)
PSI = mot.psi(TH, II) * (1 + 0.01 * rng.standard_normal(TH.shape)) \
      + 0.01 * rng.standard_normal(TH.shape)
X_d = np.column_stack([TH.ravel(), II.ravel()]); y_d = PSI.ravel()

# точки коллокации (без измерений)
th_c = rng.uniform(0, np.pi, 400); i_c = rng.uniform(0, I_MAX, 400)
th_0 = np.linspace(0, np.pi, 40)

def feats(th, i):
    return np.column_stack([np.cos(th), i / I_MAX])

def unpack(w):
    W = w[:2 * H].reshape(H, 2); b = w[2 * H:3 * H]
    a = w[3 * H:4 * H]; c = w[4 * H]
    return W, b, a, c

def net(w, th, i):
    W, b, a, c = unpack(w)
    h = np.tanh(feats(th, i) @ W.T + b)
    return h @ a + c

def dnet_di(w, th, i):
    W, b, a, c = unpack(w)
    h = np.tanh(feats(th, i) @ W.T + b)
    return ((1 - h**2) * W[:, 1]) @ a / I_MAX

def loss(w, lam):
    L = np.mean((net(w, X_d[:, 0], X_d[:, 1]) - y_d) ** 2)
    if lam > 0:
        L += lam * np.mean(net(w, th_0, np.zeros_like(th_0)) ** 2)
        L += lam * np.mean(np.minimum(dnet_di(w, th_c, i_c), 0.0) ** 2)
    return L

def train(lam, seed):
    w0 = np.random.default_rng(seed).normal(0, 0.8, 4 * H + 1)
    r = minimize(loss, w0, args=(lam,), method="L-BFGS-B",
                 options=dict(maxiter=3000, maxfun=10**7))
    return r.x

def evaluate(w):
    th = np.linspace(0, np.pi, 60); i = np.linspace(0, I_MAX, 81)
    T, I = np.meshgrid(th, i)
    ref = mot.psi(T, I); pred = net(w, T.ravel(), I.ravel()).reshape(T.shape)
    low = I < 3
    d = dnet_di(w, T.ravel(), I.ravel())
    return dict(rmse_all=np.sqrt(np.mean((pred - ref) ** 2)),
                rmse_low=np.sqrt(np.mean((pred[low] - ref[low]) ** 2)),
                psi_at_zero=np.abs(pred[0]).max(),
                neg_slope_share=np.mean(d < 0) * 100)

if __name__ == "__main__":
    print("Параметров сети:", 4 * H + 1)
    rows = []
    for name, lam in (("ИНС без ограничений", 0.0), ("ФИНС", 1.0)):
        res = [evaluate(train(lam, s)) for s in range(5)]
        agg = {k: np.mean([r[k] for r in res]) for k in res[0]}
        rows.append((name, agg))
        print(f"{name:22s} СКО по области {agg['rmse_all']:.4f} Вб | "
              f"СКО при i<3 А {agg['rmse_low']:.4f} Вб | "
              f"max|psi(θ,0)| {agg['psi_at_zero']:.4f} Вб | "
              f"доля точек с dpsi/di<0 {agg['neg_slope_share']:.1f} %")
    # иллюстрация в стиле учебника
    import mstyle
    from mstyle import plt, C, finish, lab
    w_nn, w_pi = train(0.0, 0), train(1.0, 0)
    fig, ax = plt.subplots(1, 2, figsize=(7.0, 3.0))
    i = np.linspace(0, I_MAX, 200)
    for a, th, ylim in zip(ax, (np.deg2rad(22.5), np.pi), ((-0.04, 0.26), (-0.05, 1.3))):
        a.axvspan(0, 3, color="#f2f2f2", lw=0)
        a.plot(i, mot.psi(th, i), color="k", lw=1.2, label="модель")
        a.plot(i, net(w_nn, np.full_like(i, th), i), color=C[1], ls="--", label="ИНС")
        a.plot(i, net(w_pi, np.full_like(i, th), i), color=C[0], label="ФИНС")
        msk = np.isclose(X_d[:, 0], th)
        if msk.any(): a.plot(X_d[msk, 1], y_d[msk], "o", ms=3.5, mfc="white", mec="k", mew=0.7, label="данные")
        a.set_xlim(0, I_MAX); a.set_ylim(*ylim); a.set_xlabel(lab("i", "А"))
    ax[0].set_ylabel(lab(r"\psi", "Вб")); ax[0].legend(loc="lower right", fontsize=9)
    finish(list(ax), labels=True)
    fig.tight_layout()
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "figs", "fig_pinn.png")
    fig.savefig(out, dpi=300, bbox_inches="tight")
    print("Рисунок:", out)
