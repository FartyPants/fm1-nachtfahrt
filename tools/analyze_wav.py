# SPDX-License-Identifier: GPL-3.0-only
"""A render, measured: levels, bands, the kick's timing, and a picture (spectrogram and level).

    .venv/bin/python3 tools/analyze_wav.py IN.wav [--png OUT.png] [--bpm 122] [--seg 8]

Prints the peak and RMS (dBFS), the share of each band (sub < 90 Hz, low 90-300, mid 300-2k,
high 2k-8k, air > 8k), the level per segment of --seg seconds, DC, the left/right correlation, and,
with --bpm, how far the low end's onsets fall from the beat grid. Needs numpy (and matplotlib for --png).
"""
import argparse
import wave

import numpy as np

BANDS = [("sub", 20, 90), ("low", 90, 300), ("mid", 300, 2000), ("high", 2000, 8000), ("air", 8000, 20000)]


def db(x):
    return 20 * np.log10(max(float(x), 1e-12))


def load(path):
    with wave.open(path) as w:
        sr, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
        x = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float32) / 32768.0
    return sr, x.reshape(-1, ch)


def band_shares(mono, sr):
    spec = np.abs(np.fft.rfft(mono * np.hanning(len(mono)))) ** 2
    f = np.fft.rfftfreq(len(mono), 1 / sr)
    tot = spec[(f >= 20)].sum() + 1e-20
    return {name: 10 * np.log10(spec[(f >= lo) & (f < hi)].sum() / tot + 1e-12) for name, lo, hi in BANDS}


def low_onsets(mono, sr):
    """Onsets of the low end (< 150 Hz): a one-pole low-pass, its envelope, rising edges."""
    # decimated by 8 after a box average first (the kick is far below 2.7 kHz)
    m = len(mono) // 8 * 8
    d = mono[:m].reshape(-1, 8).mean(axis=1)
    srd = sr / 8
    a = np.exp(-2 * np.pi * 150 / srd)
    y = np.zeros_like(d)
    for i in range(1, len(d)):
        y[i] = a * y[i - 1] + (1 - a) * d[i]
    env = np.abs(y)
    k = int(srd * 0.005)
    env = np.convolve(env, np.ones(k) / k, mode="same")
    thr = 0.35 * np.percentile(env, 99.5)
    on = np.where((env[1:] > thr) & (env[:-1] <= thr))[0]
    out, last = [], -1e9
    for i in on:
        if i - last > srd * 0.12:
            out.append(i / srd)
            last = i
    return np.array(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("--png")
    ap.add_argument("--bpm", type=float)
    ap.add_argument("--seg", type=float, default=8.0)
    a = ap.parse_args()
    sr, x = load(a.wav)
    mono = x.mean(axis=1)
    print(f"{a.wav}: {len(x) / sr:.1f} s")
    print(f"  peak {db(np.abs(x).max()):6.1f} dBFS   rms {db(np.sqrt((x ** 2).mean())):6.1f} dBFS   "
          f"dc {mono.mean():+.4f}   L/R corr {np.corrcoef(x[:, 0], x[:, 1])[0, 1]:.2f}")
    print("  bands (dB of the total): " + "  ".join(f"{k} {v:5.1f}" for k, v in band_shares(mono, sr).items()))
    n = int(a.seg * sr)
    segs = [mono[i:i + n] for i in range(0, len(mono) - n + 1, n)]
    print("  rms per %gs: " % a.seg + " ".join(f"{db(np.sqrt((s ** 2).mean())):.0f}" for s in segs))
    clip = int((np.abs(x) > 0.985).sum())
    if clip:
        print(f"  near full scale: {clip} samples")
    if a.bpm:
        on = low_onsets(mono, sr)
        step = 60.0 / a.bpm
        if len(on):
            ph = on / step
            ref = np.angle(np.exp(2j * np.pi * ph).mean()) / (2 * np.pi)   # the grid's phase, as the onsets agree
            err = (ph - ref - np.round(ph - ref)) * step * 1000
            print(f"  low onsets: {len(on)} ({len(on) / (len(mono) / sr) * step:.2f} a beat), "
                  f"off the grid: median {np.median(np.abs(err)):.1f} ms, max {np.abs(err).max():.1f} ms")
    if a.png:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots(2, 1, figsize=(12, 6), sharex=True, gridspec_kw={"height_ratios": [3, 1]})
        ax[0].specgram(mono, NFFT=2048, Fs=sr, noverlap=1024, cmap="magma", vmin=-120, vmax=-20, scale="dB")
        ax[0].set_yscale("symlog", linthresh=200)
        ax[0].set_ylim(30, 16000)
        ax[0].set_ylabel("Hz")
        hop = int(sr * 0.05)
        r = np.sqrt(np.convolve(mono ** 2, np.ones(hop) / hop, mode="valid")[::hop])
        ax[1].plot(np.arange(len(r)) * 0.05, 20 * np.log10(r + 1e-9), lw=0.6)
        ax[1].set_ylim(-60, 0)
        ax[1].set_ylabel("dBFS")
        ax[1].set_xlabel("s")
        fig.tight_layout()
        fig.savefig(a.png, dpi=80)
        print(f"  picture: {a.png}")


if __name__ == "__main__":
    main()
