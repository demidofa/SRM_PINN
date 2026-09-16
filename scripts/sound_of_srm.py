# -*- coding: utf-8 -*-
"""
«Как звучит пульсация момента»: озвучивание переменной составляющей момента ВИД.

Рассчитывается один электрический период при постоянной скорости, сигнал
момента без постоянной составляющей повторяется и записывается в WAV.
Важно: это озвучивание пульсаций момента, а не расчет акустического шума.
Реальный шум ВИД определяется в основном радиальными силами и собственными
частотами статора; обсуждение этого различия входит в задание.

Запуск: python sound_of_srm.py  (создает WAV-файлы в папке ../sound)
"""
import os, wave, numpy as np
from srm_model import SRM, simulate_time

FS = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sound")
os.makedirs(OUT, exist_ok=True)
mot = SRM(); deg = np.deg2rad

def to_wav(sig, dt, name, seconds=3.0):
    sig = sig - sig.mean()
    t_src = np.arange(len(sig)) * dt
    period = len(sig) * dt
    t = np.arange(int(seconds * FS)) / FS
    y = np.interp(np.mod(t, period), t_src, sig)
    fade = np.minimum(1, np.minimum(t, seconds - t) / 0.05)
    y = 0.8 * y / np.abs(y).max() * fade
    with wave.open(os.path.join(OUT, name), "wb") as f:
        f.setnchannels(1); f.setsampwidth(2); f.setframerate(FS)
        f.writeframes((y * 32767).astype(np.int16).tobytes())
    return os.path.join(OUT, name)

def steady_period(rpm, **kw):
    T_el = 60 / rpm / mot.Nr
    r = simulate_time(mot, rpm, deg(10), deg(150), 15, t_end=2 * T_el, dt=5e-6, **kw)
    n = len(r["Te"]) // 2
    return r["Te"][n:], 5e-6

if __name__ == "__main__":
    rpm = 900
    print("Частота коммутации фаз:", mot.m * mot.Nr * rpm / 60, "Гц")
    for name, kw in (("hyst_band0p8A.wav", dict(band=0.8)),
                     ("hyst_band3A.wav", dict(band=3.0)),
                     ("mpc_Ts100us.wav", dict(controller="mpc", Ts=1e-4))):
        Te, dt = steady_period(rpm, **kw)
        print(name, "размах пульсаций, Н·м:", round(Te.max() - Te.min(), 1),
              "->", to_wav(Te, dt, name))
