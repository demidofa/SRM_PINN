# -*- coding: utf-8 -*-
"""
От обучения к микроконтроллеру: экспорт обученной ФИНС в код на языке C.

1. Обучает ФИНС (пример ЛР1) и сохраняет веса в srm_flux_nn_weights.h (float32).
2. Формирует тестовые векторы test_vectors.csv по модели Python.
3. Код srm_flux_nn.c содержит вывод сети, ее производную по току, обращение
   psi -> i методом Ньютона и шаг прогнозирующего регулятора (формула учебника).
4. test_host.c компилируется на ПК (gcc) и проверяет совпадение с Python,
   время вычисления и объем памяти по сравнению с таблицей.

Запуск:  python export_to_c.py
         gcc -O2 -o test_host test_host.c srm_flux_nn.c -lm && ./test_host
"""
import os, sys, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts"))
import lab1_pinn_demo as demo

def fmt(a):
    return ", ".join(f"{float(x):.9e}f" for x in np.ravel(a))

if __name__ == "__main__":
    w = demo.train(1.0, 0)
    W, b, a, c = demo.unpack(w)
    H = demo.H
    with open(os.path.join(HERE, "srm_flux_nn_weights.h"), "w", encoding="utf-8") as f:
        f.write("/* Автоматически сформировано export_to_c.py. Не редактировать вручную. */\n")
        f.write("#ifndef SRM_FLUX_NN_WEIGHTS_H\n#define SRM_FLUX_NN_WEIGHTS_H\n")
        f.write(f"#define NN_HIDDEN {H}\n#define NN_I_MAX {demo.I_MAX:.1f}f\n")
        rows = ", ".join("{ " + fmt(r) + " }" for r in W)
        f.write(f"static const float NN_W[{H}][2] = {{ {rows} }};\n")
        f.write(f"static const float NN_B[{H}] = {{ {fmt(b)} }};\n")
        f.write(f"static const float NN_A[{H}] = {{ {fmt(a)} }};\n")
        f.write(f"static const float NN_C = {float(c):.9e}f;\n#endif\n")
    rng = np.random.default_rng(1)
    th = rng.uniform(0, 2 * np.pi, 10000); i = rng.uniform(0, demo.I_MAX, 10000)
    psi = demo.net(w, th, i); dpsi = demo.dnet_di(w, th, i)
    np.savetxt(os.path.join(HERE, "test_vectors.csv"),
               np.column_stack([th, i, psi, dpsi]), delimiter=",", fmt="%.10e")
    print("Параметров:", 4 * H + 1, "| память весов:", (4 * H + 1) * 4, "байт (float32)")
    print("Сформированы srm_flux_nn_weights.h и test_vectors.csv")
