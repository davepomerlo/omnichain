// OmniChain - shared DSP building blocks (header-only, no JUCE dependency)
#pragma once
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace oc
{
constexpr float kPi  = 3.14159265358979323846f;
constexpr float k2Pi = 6.28318530717958647692f;

inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain (float db) { return std::exp (db * 0.11512925465f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

// Pade-style tanh approximation, exact saturation at |x| >= 3
inline float fastTanh (float x)
{
    x = clampf (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// tan() approximation for 0..~1.5 rad (used for per-sample filter modulation)
inline float fastTan (float x)
{
    x = clampf (x, 0.0f, 1.55f);
    const float x2 = x * x;
    return x * (135135.0f + x2 * (-17325.0f + x2 * (378.0f - x2)))
             / (135135.0f + x2 * (-62370.0f + x2 * (3150.0f - 28.0f * x2)));
}

inline float fastSin2Pi (float phase01) // sine of 2*pi*phase, phase in [0,1)
{
    // Bhaskara-like parabolic approximation with extra precision step
    float x = phase01 - std::floor (phase01);
    x = x * 2.0f - 1.0f;                // -1..1  -> maps to -pi..pi
    float y = 4.0f * x * (1.0f - std::fabs (x));
    y = 0.225f * (y * std::fabs (y) - y) + y;
    return -y;
}

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni()  { return (next() >> 8) * (1.0f / 16777216.0f); }     // 0..1
    inline float bi()   { return uni() * 2.0f - 1.0f; }                      // -1..1
};

struct Smooth
{
    float cur = 0, tgt = 0, k = 0.001f;
    void setTime (float ms, double sr) { k = 1.0f - std::exp (-1.0f / (float (sr) * std::max (ms, 0.01f) * 0.001f)); }
    void reset (float v) { cur = tgt = v; }
    inline float next() { cur += k * (tgt - cur); return cur; }
};

inline float onePoleCoef (float ms, double sr) { return std::exp (-1.0f / (float (sr) * std::max (ms, 0.001f) * 0.001f)); }

struct OnePole
{
    float a = 0, z = 0;
    void setHz (float f, double sr) { a = std::exp (-k2Pi * clampf (f, 1.0f, float (sr) * 0.49f) / float (sr)); }
    inline float lp (float x) { z = x + a * (z - x); return z; }
    inline float hp (float x) { return x - lp (x); }
    void reset() { z = 0; }
};

struct DcBlock
{
    float x1 = 0, y1 = 0, r = 0.995f;
    void setup (double sr) { r = 1.0f - (k2Pi * 20.0f / float (sr)); }
    inline float tick (float x) { float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0; }
};

// RBJ biquad, transposed direct form II, stereo state
struct Biquad
{
    enum Type { LP, HP, BP, Notch, Peak, LowShelf, HighShelf, AllPass };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = { 0, 0 }, z2[2] = { 0, 0 };

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }

    void set (Type t, float f, float q, float gainDb, double sr)
    {
        f = clampf (f, 5.0f, float (sr) * 0.45f);
        q = std::max (q, 0.05f);
        const float w0 = k2Pi * f / float (sr);
        const float cw = std::cos (w0), sw = std::sin (w0);
        const float alpha = sw / (2.0f * q);
        const float A = std::pow (10.0f, gainDb / 40.0f);
        float B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
        switch (t)
        {
            case LP:   B0 = (1 - cw) * 0.5f; B1 = 1 - cw; B2 = B0; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case HP:   B0 = (1 + cw) * 0.5f; B1 = -(1 + cw); B2 = B0; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case BP:   B0 = alpha; B1 = 0; B2 = -alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case Notch:B0 = 1; B1 = -2 * cw; B2 = 1; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case AllPass: B0 = 1 - alpha; B1 = -2 * cw; B2 = 1 + alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case Peak: B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break;
            case LowShelf:
            {
                const float sa = 2.0f * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) - (A - 1) * cw + sa);
                B1 = 2 * A * ((A - 1) - (A + 1) * cw);
                B2 = A * ((A + 1) - (A - 1) * cw - sa);
                A0 = (A + 1) + (A - 1) * cw + sa;
                A1 = -2 * ((A - 1) + (A + 1) * cw);
                A2 = (A + 1) + (A - 1) * cw - sa;
                break;
            }
            case HighShelf:
            {
                const float sa = 2.0f * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) + (A - 1) * cw + sa);
                B1 = -2 * A * ((A - 1) + (A + 1) * cw);
                B2 = A * ((A + 1) + (A - 1) * cw - sa);
                A0 = (A + 1) - (A - 1) * cw + sa;
                A1 = 2 * ((A - 1) - (A + 1) * cw);
                A2 = (A + 1) - (A - 1) * cw - sa;
                break;
            }
        }
        const float inv = 1.0f / A0;
        b0 = B0 * inv; b1 = B1 * inv; b2 = B2 * inv; a1 = A1 * inv; a2 = A2 * inv;
    }

    inline float tick (float x, int c)
    {
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
};

// Zero-delay-feedback state variable filter (Cytomic / Zavalishin), mono state
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0, k = 1;
    float lp = 0, bp = 0, hp = 0;
    void reset() { ic1 = ic2 = 0; }
    inline void setG (float g, float q) // g = tan(pi f / sr)
    {
        k = 1.0f / std::max (q, 0.1f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void set (float f, float q, double sr) { setG (fastTan (kPi * clampf (f, 10.0f, float (sr) * 0.45f) / float (sr)), q); }
    inline void tick (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

// First-order allpass (for phasers / dispersion)
struct AllPass1
{
    float z = 0;
    inline float tick (float x, float a) { const float y = a * x + z; z = x - a * y; return y; }
    void reset() { z = 0; }
};

inline float allpassCoefFromHz (float f, double sr)
{
    const float t = fastTan (kPi * clampf (f, 10.0f, float (sr) * 0.45f) / float (sr));
    return (t - 1.0f) / (t + 1.0f);
}

// Power-of-two ring buffer delay line with fractional reads
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void init (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    int size() const { return mask + 1; }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float at (int samplesBack) const { return buf[(size_t) ((w - 1 - samplesBack) & mask)]; }

    // d = delay in samples (0 = newest)
    inline float read (float d) const
    {
        const float rp = float (w - 1) - d;
        const float fl = std::floor (rp);
        const int i = (int) fl;
        const float fr = rp - fl;
        const float a = buf[(size_t) (i & mask)];
        const float b = buf[(size_t) ((i + 1) & mask)];
        return a + fr * (b - a);
    }
    inline float readCubic (float d) const
    {
        d = std::max (d, 1.0f);
        const float rp = float (w - 1) - d;
        const float fl = std::floor (rp);
        const int i = (int) fl;
        const float t = rp - fl;
        const float xm1 = buf[(size_t) ((i - 1) & mask)];
        const float x0  = buf[(size_t) (i & mask)];
        const float x1  = buf[(size_t) ((i + 1) & mask)];
        const float x2  = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }
};

struct EnvFollower
{
    float env = 0, ka = 0, kr = 0;
    void setup (float attackMs, float releaseMs, double sr) { ka = onePoleCoef (attackMs, sr); kr = onePoleCoef (releaseMs, sr); }
    inline float tick (float x) { x = std::fabs (x); env = x > env ? x + ka * (env - x) : x + kr * (env - x); return env; }
    void reset() { env = 0; }
};

// Granular pitch shifter (two crossfaded taps), used by Pitch, Reverb shimmer, Delay reverse
struct GrainShifter
{
    DelayLine dl;
    float phase = 0;
    void init (int maxWindow) { dl.init (maxWindow + 16); phase = 0; }
    void clear() { dl.clear(); phase = 0; }
    // ratio: playback rate (2 = +12st, -1 = reverse). window in samples.
    inline float tick (float x, float ratio, float window)
    {
        dl.push (x);
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        const float d1 = 2.0f + phase * window;
        const float d2 = 2.0f + p2 * window;
        const float s1 = fastSin2Pi (phase * 0.5f);
        const float s2 = fastSin2Pi (p2 * 0.5f);
        return dl.read (d1) * s1 * s1 + dl.read (d2) * s2 * s2;
    }
};
} // namespace oc
