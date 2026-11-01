"""SPDX-License-Identifier: GPL-3.0-only
Fits the Water Atmosphere (firmware/src/dsp/amb.c: WL_BQ, wl_tick): the author's recording of the water at Umeda is steady
Gaussian noise, so white noise through three parallel biquads and a one-pole low-pass matches its spectrum. Not part of
the build: run by hand (needs the 44.1 kHz recording, numpy, scipy); it prints the fit and writes two comparison WAVs;
the constants in amb.c are its parameters as biquad coefficients (see the numbers it prints, scaled to the recording's rms).
Originally: the Umeda water as shaped noise instead of a sampled loop.
Reads the 44.1 kHz reference, fits a cheap filter (parallel biquads on white noise), renders stereo files to compare."""
import numpy as np
from scipy.io import wavfile
from scipy.signal import welch, butter, sosfilt, lfilter, freqz
from scipy.optimize import least_squares
from scipy.stats import kurtosis

SR = 44100
OUT = 'I:/FM1/fm1-berlin-main/build/listen/window/'
rate, x = wavfile.read('I:/FM1/umeda_water_loop3.wav')
x = x.astype(float)
ref = (x.mean(1) if x.ndim == 2 else x) / 32768.0
assert rate == SR
rms_ref = ref.std()

# ---- the reference's smoothed spectrum, in dB, on a log grid
f, P = welch(ref, SR, nperseg=16384)
grid = 40.0 * 2 ** (np.arange(0, 8.6, 1 / 6))          # 1/6-octave points, 40 Hz .. ~14 kHz
grid = grid[grid < 14000]
def band(p, c, w=2 ** (1 / 8)):
    m = (f >= c / w) & (f < c * w)
    return p[m].mean()
target_db = np.array([10 * np.log10(band(P, c)) for c in grid])

# ---- the cheap model: parallel branches from one white noise: LP2(f1) g1 + BP2(f2, Q2) g2 + BP2(f3, Q3) g3, then a 1-pole LP(f4)
def biquad_lp(fc, q):
    w = 2 * np.pi * fc / SR; a = np.sin(w) / (2 * q); c = np.cos(w)
    b = np.array([(1 - c) / 2, 1 - c, (1 - c) / 2]); aa = np.array([1 + a, -2 * c, 1 - a]); return b / aa[0], aa / aa[0]
def biquad_bp(fc, q):
    w = 2 * np.pi * fc / SR; a = np.sin(w) / (2 * q); c = np.cos(w)
    b = np.array([a, 0, -a]); aa = np.array([1 + a, -2 * c, 1 - a]); return b / aa[0], aa / aa[0]
def onepole_lp(fc):
    k = 1 - np.exp(-2 * np.pi * fc / SR); return np.array([k]), np.array([1, -(1 - k)])
def model_h(p, freqs):
    g1, f1, q1, g2, f2, q2, g3, f3, q3, f4 = p
    w = 2 * np.pi * freqs / SR
    H = 0
    for (g, fn, fc, q) in ((g1, biquad_lp, f1, q1), (g2, biquad_bp, f2, q2), (g3, biquad_bp, f3, q3)):
        b, a = fn(fc, q); H = H + g * freqz(b, a, w)[1]
    b, a = onepole_lp(f4); H = H * freqz(b, a, w)[1]
    return H
# (the model is in amplitude, the target in power dB: both in dB of power below)
def resid_db(lp):
    p = np.exp(lp)
    H = model_h(p, grid)
    m = 20 * np.log10(np.abs(H) + 1e-12)
    return (m - m.mean()) - (target_db - target_db.mean())
best = None
rng = np.random.default_rng(1)
for trial in range(60):
    p0 = np.array([1.0, rng.uniform(50, 200), rng.uniform(0.5, 1.5), 0.5, rng.uniform(400, 900), rng.uniform(0.4, 1.2),
                   1.0, rng.uniform(1200, 2500), rng.uniform(0.5, 2.0), rng.uniform(3000, 7000)])
    try:
        r = least_squares(resid_db, np.log(p0), max_nfev=200)
    except Exception:
        continue
    if best is None or r.cost < best.cost:
        best = r
p = np.exp(best.x)
err = resid_db(best.x)
print('fit: rms error %.2f dB, worst %.2f dB over 40 Hz .. 14 kHz (1/6 octave points)' % (np.sqrt((err ** 2).mean()), np.abs(err).max()))
print('params: LP2 g %.2f f %.0f Q %.2f | BP2 g %.2f f %.0f Q %.2f | BP2 g %.2f f %.0f Q %.2f | LP1 f %.0f' % tuple(p))

def synth(n, seed, params=p):
    r = np.random.default_rng(seed)
    w = r.standard_normal(n)
    g1, f1, q1, g2, f2, q2, g3, f3, q3, f4 = params
    y = 0
    for (g, fn, fc, q) in ((g1, biquad_lp, f1, q1), (g2, biquad_bp, f2, q2), (g3, biquad_bp, f3, q3)):
        b, a = fn(fc, q); y = y + g * lfilter(b, a, w)
    b, a = onepole_lp(f4)
    return lfilter(b, a, y)

# ---- level-match to the reference, render
N = 20 * SR
L = synth(N, 11); R = synth(N, 22)
k = rms_ref / L.std()
L *= k; R *= k
print('stereo L/R correlation of the synthesis: %.3f' % np.corrcoef(L, R)[0, 1])

# the reference played as the firmware would: two read heads (right half a loop on, 1.3% faster), 44.1 kHz source here
def head(src, n, start, rate):
    pos = start + np.arange(n) * rate
    pos = pos % len(src)
    i = pos.astype(int); fr_ = pos - i
    return src[i] * (1 - fr_) + src[(i + 1) % len(src)] * fr_
RL = head(ref, N, 0, 1.0); RR = head(ref, N, len(ref) / 2, 1.0132)

def write(name, l, r_):
    s = np.stack([l, r_], 1); pk = np.abs(s).max()
    s = s / max(pk, 1e-9) * 0.5 if False else s
    wavfile.write(OUT + name, SR, np.round(np.clip(s, -1, 1) * 32767).astype('<i2'))
write('water_ref_stereo.wav', RL, RR)
write('water_synth_cheap.wav', L, R)

# ---- compare 1/3-octave levels of the three, in dB relative to the reference
def third(sig):
    ff, PP = welch(sig, SR, nperseg=8192); out = []
    c = 40.0
    cs = []
    while c < 10000:
        m = (ff >= c / 2 ** (1 / 6)) & (ff < c * 2 ** (1 / 6)); out.append(10 * np.log10(PP[m].sum() + 1e-20)); cs.append(c); c *= 2 ** (1 / 3)
    return np.array(cs), np.array(out)
cs, a = third(ref); _, b = third(L)
print('1/3-octave level, synthesis minus reference (dB):')
print(' '.join('%d:%+.1f' % (c, d) for c, d in zip(cs, b - a)))
# short-time level wander (0.25 s), and the 20 ms envelope's spread of the 1-3 kHz band
def wander(sig):
    n = int(0.25 * SR); m = len(sig) // n
    e = 20 * np.log10(np.array([sig[i * n:(i + 1) * n].std() for i in range(m)]) + 1e-9); return e.std()
print('0.25 s level wander (sd dB): reference %.2f, synthesis %.2f' % (wander(ref), wander(L[:len(ref)])))
sos = butter(4, [1000, 3000], btype='band', fs=SR, output='sos')
print('kurtosis of the 1-3 kHz band (Gaussian 3): reference %.2f, synthesis %.2f' % (kurtosis(sosfilt(sos, ref), fisher=False), kurtosis(sosfilt(sos, L), fisher=False)))
sos = butter(4, [40, 200], btype='band', fs=SR, output='sos')
print('kurtosis of the 40-200 Hz band: reference %.2f, synthesis %.2f' % (kurtosis(sosfilt(sos, ref), fisher=False), kurtosis(sosfilt(sos, L), fisher=False)))
np.save('I:/FM1/fm1-berlin-main/build/tmp/water_params.npy', p)
