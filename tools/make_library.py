#!/usr/bin/env python3
"""Builds the "OmniChain Library" folder.

 * IR/...   original impulse responses DESIGNED mathematically for OmniChain (not recordings of real
            cabinets or rooms): speaker/mic/room models -> minimum-phase IRs. Released under CC0.
 * NAM/...  a curated selection from the community collection https://github.com/pelennor2170/NAM_models
            (GPLv3), copied unmodified and renamed. Pass the path to a clone of that repo as argv[1].
"""
import numpy as np, os, sys, shutil, struct, wave
SR = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "library")

def write_wav(path, data):
    data = np.atleast_2d(np.asarray(data, dtype=np.float64))          # (channels, n)
    data = data / (np.abs(data).max() + 1e-12) * 0.89
    os.makedirs(os.path.dirname(path), exist_ok=True)
    ints = np.clip(np.round(data.T * 8388607.0), -8388608, 8388607).astype(np.int32)   # (n, ch)
    b = ints.astype('<i4').tobytes()
    raw = np.frombuffer(b, dtype=np.uint8).reshape(-1, 4)[:, :3].tobytes()
    with wave.open(path, 'wb') as w:
        w.setnchannels(data.shape[0]); w.setsampwidth(3); w.setframerate(SR); w.writeframes(raw)

N = 16384
F = np.fft.rfftfreq(N, 1 / SR); F[0] = 1e-3
def db_peak(f0, g, q):       # RBJ peaking magnitude in dB
    A = 10 ** (g / 40); w0 = 2 * np.pi * f0 / SR; al = np.sin(w0) / (2 * q); z = np.exp(-1j * 2 * np.pi * F / SR)
    b = np.array([1 + al * A, -2 * np.cos(w0), 1 - al * A]); a = np.array([1 + al / A, -2 * np.cos(w0), 1 - al / A])
    H = (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z)
    return 20 * np.log10(np.abs(H) + 1e-12)
def db_hp(fc, order, q=0.707):
    s = F / fc
    if order == 1: return 20 * np.log10(s / np.sqrt(1 + s * s))
    return 20 * np.log10(s * s / np.sqrt((1 - s * s) ** 2 + (s / q) ** 2)) * (order / 2)
def db_lp(fc, order):  return -10 * np.log10(1 + (F / fc) ** (2 * order))
def db_shelf_lo(fc, g): return g / (1 + (F / fc) ** 2)
def db_shelf_hi(fc, g): return g * (F / fc) ** 2 / (1 + (F / fc) ** 2)
def db_tilt(f0, per_oct): return np.where(F > f0, per_oct * np.log2(F / f0), 0.0)
def minphase(mag_db, n_out):
    mag_db = np.clip(mag_db, -110, 40)
    c = np.fft.irfft(mag_db / 20 * np.log(10), N)
    w = np.zeros(N); w[0] = 1; w[1:N // 2] = 2; w[N // 2] = 1
    h = np.fft.irfft(np.exp(np.fft.rfft(c * w)), N)[:n_out]
    fade = int(n_out * 0.25); h[-fade:] *= np.cos(np.linspace(0, np.pi / 2, fade)) ** 2
    return h
def lp1(x, fc):
    a = np.exp(-2 * np.pi * fc / SR); y = np.zeros_like(x); z = 0.0
    for i, v in enumerate(x): z = v + a * (z - v); y[i] = z
    return y

# ---------------------------------------------------------------- speakers
# name: (resonance Hz, Q, open-back, [(f, gain dB, Q)...], top-end rolloff Hz, rolloff order, breakup ripple dB, seed)
SPEAKERS = {
 "British 4x12 - Green 25W":      (95, 1.3, False, [(130, 2, 1.2), (420, -2.5, 1), (1600, 2, 1.5), (2600, 6, 2), (3800, 3, 3)], 4800, 5, 3.0, 11),
 "British 4x12 - V-Type 60W":     (105, 1.4, False, [(120, 3, 1.3), (500, -3, .9), (900, -1, 1.5), (2200, 5, 1.8), (3600, 6, 2.5), (5200, 2, 4)], 5600, 5, 3.5, 12),
 "British 4x12 - Seventy-Five":   (90, 1.2, False, [(110, 3, 1.2), (450, -4, .8), (1000, -2, 1.2), (3200, 5, 2), (4500, 4, 3)], 6200, 5, 3.5, 13),
 "Modern Oversize 4x12":          (82, 1.5, False, [(95, 5, 1.4), (380, -3, 1), (2400, 5, 2), (3900, 5, 2.5)], 5400, 6, 3.0, 14),
 "American 2x12 Open Back":       (100, .9, True, [(650, -1.5, 1), (2000, 2, 1.5), (3200, 5, 2.2), (4800, 3, 3)], 6500, 4, 2.5, 15),
 "American 1x12 Open Back":       (115, .9, True, [(180, 2, 1.5), (2800, 5, 2), (4200, 3, 3)], 5800, 4, 2.5, 16),
 "Tweed 4x10 Open Back":          (120, .8, True, [(250, 2, 1.2), (1500, 2, 1.2), (3000, 5, 1.8), (5000, 3, 2.5)], 6800, 4, 2.5, 17),
 "Alnico Blue 2x12 Open Back":    (110, .9, True, [(800, 1.5, 1), (2300, 6, 1.6), (3500, 4, 2.5), (5500, 2, 3)], 6000, 4, 3.0, 18),
 "Boutique 2x12 Closed - Creamy": (92, 1.2, False, [(150, 2, 1.2), (700, 1.5, 1), (2000, 3, 1.5), (3000, 2.5, 2.5)], 4600, 5, 2.0, 19),
 "Small 1x8 Practice Combo":      (180, 1.1, True, [(400, 3, 1.5), (1200, 4, 1.5), (2500, 5, 2)], 4200, 4, 3.0, 20),
}
BASS = {
 "Bass 8x10 Fridge":              (58, 1.0, False, [(80, 3, 1), (250, -2, 1), (800, 2, 1), (2500, 4, 1.5)], 4500, 4, 2.0, 31),
 "Bass 1x15 Vintage":             (45, 1.1, False, [(70, 4, 1), (200, 1, 1), (600, -2, 1), (1800, 2, 1.5)], 3000, 4, 1.5, 32),
 "Bass 4x10 Modern with Tweeter": (50, 1.0, False, [(90, 2, 1), (400, -3, 1), (3000, 3, 1.2)], 9000, 3, 1.5, 33),
}
def speaker_db(spec):
    fs, q, openb, peaks, fc, order, rip, seed = spec
    d = db_hp(fs, 2, q) + db_lp(fc, order)
    if openb: d += db_hp(fs * 0.75, 1) + db_peak(fs * 2.6, -2.0, 1.0)       # back-wave cancellation
    for f0, g, qq in peaks: d += db_peak(f0, g, qq)
    rng = np.random.default_rng(seed)                                         # cone break-up modes
    for _ in range(14):
        d += db_peak(rng.uniform(1300, fc * 1.15), rng.uniform(-rip, rip), rng.uniform(5, 12))
    return d
MICS = {
 "Dynamic - Cap":       lambda: db_hp(80, 1) + db_shelf_lo(150, 3) + db_peak(400, -2, 1) + db_peak(5500, 5, 1.2) + db_lp(14000, 2),
 "Dynamic - Cap Edge":  lambda: db_hp(80, 1) + db_shelf_lo(150, 3) + db_peak(400, -1.5, 1) + db_peak(5000, 3.5, 1.2) + db_lp(13000, 2) + db_tilt(2000, -2.0),
 "Dynamic - Cone":      lambda: db_hp(80, 1) + db_shelf_lo(160, 3.5) + db_peak(700, 1.5, 0.8) + db_peak(4500, 2, 1.2) + db_lp(11000, 2) + db_tilt(1500, -4.5),
 "Ribbon":              lambda: db_hp(40, 1) + db_shelf_lo(120, 5) + db_peak(3000, -1.5, 1) + db_tilt(6000, -6.0) + db_tilt(1200, -1.0),
 "Condenser":           lambda: db_hp(35, 1) + db_shelf_lo(120, 1.5) + db_peak(10000, 2, 0.8) + db_lp(18000, 2) + db_tilt(2500, -1.0),
}
def cab_ir(spk_db, mic, spec, n_out=7200, room=False):
    h = minphase(spk_db + MICS[mic](), n_out)
    rng = np.random.default_rng(spec[7] * 7 + len(mic))
    refl = np.zeros(n_out)
    if spec[2]:  refl[int(0.0016 * SR)] = -0.18                                # open back: inverted rear wave
    else:        refl[int(0.0009 * SR)] = 0.07; refl[int(0.0021 * SR)] = -0.05  # box modes
    refl[int(0.0034 * SR) + rng.integers(0, 40)] = 0.05                        # floor bounce
    if room:
        for _ in range(8): refl[int(rng.uniform(0.004, 0.028) * SR)] += rng.uniform(-0.07, 0.07)
        tail = rng.standard_normal(n_out) * np.exp(-np.arange(n_out) / (0.045 * SR)) * 0.010
        tail[: int(0.006 * SR)] = 0
        refl += tail
    wet = np.convolve(h, refl)[:n_out]
    return h + lp1(wet, 2500 if not room else 5000)

def spaces():
    rng = np.random.default_rng(99)
    def space(t60, bright, er_ms, dense, n=None, predelay_ms=0, boxy=0.0):
        n = n or int(min(1.0, t60 * 1.15) * SR)
        out = []
        for ch in range(2):
            t = np.arange(n) / SR
            lo = lp1(rng.standard_normal(n), 900) * 3.0
            hi = rng.standard_normal(n) - lp1(rng.standard_normal(n), 900)
            tail = lo * np.exp(-6.9 * t / t60) + hi * np.exp(-6.9 * t / (t60 * bright)) * 0.9
            build = np.clip(t / (er_ms / 1000.0 * (1.0 - dense) + 1e-4), 0, 1) ** 2
            tail *= build * 0.25
            er = np.zeros(n)
            for k in range(int(6 + dense * 30)):
                er[int(rng.uniform(0.001, er_ms / 1000.0) * SR)] += rng.uniform(-1, 1) * 0.6
            er[0] = 1.0
            x = lp1(er, 9000) + tail
            if boxy: x = x + boxy * (lp1(x, 700) - lp1(x, 180))
            pd = int(predelay_ms / 1000 * SR)
            x = np.concatenate([np.zeros(pd), x])[:n]
            f = int(n * 0.15); x[-f:] *= np.cos(np.linspace(0, np.pi / 2, f)) ** 2
            out.append(x)
        return np.array(out)
    yield "Vocal Booth", space(0.22, 0.6, 12, 0.5)
    yield "Studio Room", space(0.45, 0.7, 22, 0.5)
    yield "Live Room", space(0.75, 0.75, 35, 0.6)
    yield "Garage", space(0.6, 0.55, 28, 0.4, boxy=1.5)
    yield "Tile Bathroom", space(0.8, 1.0, 14, 0.7)
    yield "Echo Chamber", space(0.95, 0.8, 30, 0.8, predelay_ms=8)
    yield "Bright Plate", space(1.0, 0.95, 4, 1.0)
    yield "Dark Plate", space(1.0, 0.45, 4, 1.0)
    yield "Small Hall", space(1.1, 0.6, 55, 0.6, predelay_ms=15)
    # spring tank: dispersive chirps
    n = SR; t = np.arange(n) / SR; x = np.zeros((2, n))
    for ch in range(2):
        for k in range(1, 14):
            t0 = k * (0.041 + 0.003 * ch); tt = t - t0; m = tt > 0
            f_inst = 4200 * np.exp(-tt[m] / 0.012) + 180
            x[ch, m] += np.sin(2 * np.pi * np.cumsum(f_inst) / SR) * np.exp(-tt[m] / 0.03) * (0.72 ** k)
        x[ch] = lp1(x[ch], 4500); x[ch, 0] = 0.6
    yield "Spring Tank", x

def modal(modes, n, direct=0.5):
    t = np.arange(n) / SR; h = np.zeros(n); h[0] = direct
    for f0, amp, t60 in modes: h += amp * np.sin(2 * np.pi * f0 * t) * np.exp(-6.9 * t / t60)
    return h
def body(res, scoop_db, air_db, n=9600):       # res: [(f, gain dB, Q)] body resonances
    d = db_hp(70, 2) + db_peak(1000, scoop_db, 0.7) + db_shelf_hi(5000, air_db) + db_lp(15000, 2)
    for f0, g, q in res: d += db_peak(f0, g, q)
    return minphase(d, n)
BODIES = {
 "Dreadnought":     ([(98, 7, 5), (196, 6, 6), (285, 4, 7), (410, 3, 8), (560, 2, 8)], -6, 7),
 "Jumbo":           ([(85, 8, 5), (170, 6, 6), (250, 4, 7), (390, 3, 8)], -6, 6),
 "Parlor":          ([(125, 5, 5), (245, 6, 6), (380, 5, 7), (620, 3, 8), (900, 2, 8)], -4, 7),
 "Classical Nylon": ([(105, 7, 5), (215, 5, 6), (310, 3, 7), (450, 2, 8)], -5, 2),
 "Jazz Archtop":    ([(140, 6, 4), (280, 5, 5), (450, 3, 6), (750, 2, 6)], -1, -4),
 "Metal Resonator": ([(320, 6, 9), (540, 7, 10), (910, 7, 10), (1480, 6, 10), (2350, 5, 9), (3300, 4, 8)], 0, 4),
}
def oddities():
    yield "Telephone", minphase(db_hp(350, 4) + db_lp(3200, 6) + db_peak(1800, 8, 2) + db_peak(900, 4, 3), 2400)
    yield "Megaphone", minphase(db_hp(500, 4) + db_lp(4000, 4) + db_peak(1300, 12, 3) + db_peak(2600, 8, 4), 3600)
    yield "Pocket AM Radio", minphase(db_hp(250, 3) + db_lp(2600, 5) + db_peak(700, 5, 1.5) + db_peak(2000, 6, 3), 2400)
    yield "Tin Can", modal([(730, .5, .05), (1560, .6, .06), (2480, .5, .05), (3900, .3, .04)], 4800, direct=0.3)
    yield "Toy Amp", minphase(db_hp(300, 3) + db_lp(3500, 3) + db_peak(1000, 9, 2.5), 2400)
    t = np.arange(9600) / SR; pipe = np.zeros(9600)
    for k in range(40): pipe[min(9599, int(k * 0.0049 * SR))] += (0.8 ** k) * (-1) ** k
    yield "Steel Pipe", lp1(pipe, 6000)
    tube = np.zeros(7200)
    for k in range(25): tube[min(7199, int(k * 0.0031 * SR))] += 0.7 ** k
    yield "Cardboard Tube", lp1(tube, 2500)

def build_irs():
    count = 0
    for group, table, mics in (("Guitar Cabs", SPEAKERS, list(MICS)), ("Bass Cabs", BASS, ["Dynamic - Cap", "Condenser"])):
        for name, spec in table.items():
            sdb = speaker_db(spec); made = {}
            for mic in mics:
                made[mic] = cab_ir(sdb, mic, spec)
                write_wav(f"{OUT}/IR/{group}/{name}/{name} - {mic}.wav", made[mic]); count += 1
            room = cab_ir(sdb, "Condenser", spec, n_out=14400, room=True)
            write_wav(f"{OUT}/IR/{group}/{name}/{name} - Room.wav", room); count += 1
            if "Ribbon" in made:
                a = made["Dynamic - Cap"]; b = np.concatenate([np.zeros(3), made["Ribbon"]])[: len(a)]
                write_wav(f"{OUT}/IR/{group}/{name}/{name} - Mix Dynamic + Ribbon.wav", a / np.abs(a).max() + 0.8 * b / np.abs(b).max()); count += 1
    for name, data in spaces(): write_wav(f"{OUT}/IR/Spaces/{name}.wav", data); count += 1
    for name, (res, scoop, air) in BODIES.items(): write_wav(f"{OUT}/IR/Acoustic Bodies/{name}.wav", body(res, scoop, air)); count += 1
    for name, data in oddities(): write_wav(f"{OUT}/IR/Oddities/{name}.wav", data); count += 1
    return count

def build_nam(src):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from nam_selection import P
    lines = []
    for stem, cat, name in P:
        who = " ".join(stem.split()[:2])
        dst = f"{OUT}/NAM/{cat}/{name} ({who}).nam"
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(os.path.join(src, stem + ".nam"), dst)
        lines.append(f"{cat}/{name} ({who}).nam\n    original file: {stem}.nam   captured by: {who}")
    shutil.copyfile(os.path.join(src, "COPYING"), f"{OUT}/NAM/COPYING (GPLv3).txt")
    return lines

if __name__ == "__main__":
    shutil.rmtree(OUT, ignore_errors=True)
    n_ir = build_irs()
    nam_lines = build_nam(sys.argv[1]) if len(sys.argv) > 1 else []
    with open(f"{OUT}/CREDITS.txt", "w") as f:
        f.write("OmniChain Library\n=================\n\n"
                "IR folder\n---------\nThese impulse responses were designed mathematically for OmniChain (speaker, microphone and room\n"
                "models). They are NOT recordings of real cabinets or rooms, and the names describe the style\n"
                "each one is voiced after. They are released under CC0: use them for anything.\n\n"
                "NAM folder\n----------\nNeural Amp Modeler captures made by the NAM community and shared in the collection\n"
                "https://github.com/pelennor2170/NAM_models under the GNU GPL v3 (see NAM/COPYING).\n"
                "The files are unmodified; only the file names were tidied. Product names describe the gear\n"
                "that was captured and belong to their owners. Thank you to every contributor:\n\n" + "\n".join(nam_lines) + "\n")
    print("IRs:", n_ir, " NAM:", len(nam_lines))
