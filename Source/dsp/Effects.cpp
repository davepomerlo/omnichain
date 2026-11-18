// OmniChain - effect implementations. All effects are zero-latency (no lookahead, no FFT blocks).
#include "Effects.h"
#include "Convolver.h"

namespace oc
{
namespace
{
inline float maxAbs (float a, float b) { return std::max (std::fabs (a), std::fabs (b)); }
inline float sqr (float x) { return x * x; }

// =====================================================================================
struct NoneFx final : Effect
{
    void process (float*, float*, int, const float*, const Context&) override {}
};

// =====================================================================================
struct GateFx final : Effect
{
    float det = 0, g = 0; int hold = 0; bool open = false; OnePole scHP;
    void reset() override { det = 0; g = 0; hold = 0; open = false; scHP.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float thrO = dbToGain (v[0]), thrC = dbToGain (v[0] - v[5]);
        const float kA = 1.0f - onePoleCoef (v[1], sr), kR = 1.0f - onePoleCoef (v[3], sr);
        const int holdS = int (v[2] * 0.001f * float (sr));
        const float floorG = v[4] <= -79.5f ? 0.0f : dbToGain (v[4]);
        const float detRel = onePoleCoef (12.0f, sr);
        const bool useHP = v[6] > 20.5f;
        if (useHP) scHP.setHz (v[6], sr);
        for (int i = 0; i < n; ++i)
        {
            const float x = useHP ? std::fabs (scHP.hp ((L[i] + R[i]) * 0.5f)) * 1.2f : maxAbs (L[i], R[i]);
            det = x > det ? x : det * detRel;
            if (det > thrO) { open = true; hold = holdS; }
            else if (det < thrC) { if (hold > 0) --hold; else open = false; }
            const float tgt = open ? 1.0f : floorG;
            g += (tgt - g) * (tgt > g ? kA : kR);
            L[i] *= g; R[i] *= g;
        }
        meter = g;
    }
};

// =====================================================================================
struct CompFx final : Effect
{
    float gr = 0, rms = 0; OnePole scHP[2];
    void reset() override { gr = 0; rms = 0; scHP[0].reset(); scHP[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float thr = v[0], ratio = v[1], makeup = v[4], ch = v[5] * 0.01f;
        const float knee = 3.0f + ch * 9.0f;
        const float slope = 1.0f / ratio - 1.0f;
        const float aA = onePoleCoef (v[2], sr);
        const float aR1 = onePoleCoef (v[3], sr), aR2 = onePoleCoef (v[3] * 3.5f, sr);
        const float rmsC = onePoleCoef (12.0f, sr);
        const float satAmt = ch * 0.6f;
        const bool useHP = v[6] > 20.5f;
        if (useHP) { scHP[0].setHz (v[6], sr); scHP[1].a = scHP[0].a; }
        for (int i = 0; i < n; ++i)
        {
            const float lvl = useHP ? maxAbs (scHP[0].hp (L[i]), scHP[1].hp (R[i])) : maxAbs (L[i], R[i]);
            rms = lvl * lvl + rmsC * (rms - lvl * lvl);
            const float det = lerp (lvl, std::sqrt (rms) * 1.414f, ch * 0.7f);
            const float over = gainToDb (det) - thr;
            float tgt;
            if (2.0f * over < -knee) tgt = 0.0f;
            else if (2.0f * std::fabs (over) <= knee) tgt = slope * sqr (over + knee * 0.5f) / (2.0f * knee);
            else tgt = slope * over;
            if (tgt < gr) gr = tgt + aA * (gr - tgt);
            else
            {
                const float aR = lerp (aR1, aR2, ch * std::min (1.0f, -gr / 12.0f));
                gr = tgt + aR * (gr - tgt);
            }
            const float g = dbToGain (gr + makeup);
            float l = L[i] * g, r = R[i] * g;
            if (satAmt > 0.0f)
            {
                l = lerp (l, fastTanh (l * 1.2f) * 0.8333f, satAmt);
                r = lerp (r, fastTanh (r * 1.2f) * 0.8333f, satAmt);
            }
            L[i] = l; R[i] = r;
        }
        meter = clampf (-gr / 20.0f, 0.0f, 1.0f);
    }
};

// =====================================================================================
struct TransientFx final : Effect
{
    EnvFollower eFast, eSlowAtt, eSlowRel; float gs = 1;
    void reset() override { eFast.reset(); eSlowAtt.reset(); eSlowRel.reset(); gs = 1; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float atk = v[0] * 0.01f, sus = v[1] * 0.01f, spd = v[2], clip = v[3] * 0.01f;
        eFast.setup (0.1f, spd, sr);
        eSlowAtt.setup (spd * 2.5f, spd, sr);
        eSlowRel.setup (0.1f, spd * 8.0f, sr);
        const float ks = 1.0f - onePoleCoef (1.0f, sr);
        float m = 0;
        for (int i = 0; i < n; ++i)
        {
            const float x = maxAbs (L[i], R[i]) + 1.0e-7f;
            const float f = gainToDb (eFast.tick (x));
            const float a = gainToDb (eSlowAtt.tick (x));
            const float s = gainToDb (eSlowRel.tick (x));
            const float gDb = clampf (atk * std::max (0.0f, f - a) * 1.5f + sus * std::max (0.0f, s - f), -24.0f, 24.0f);
            gs += (dbToGain (gDb) - gs) * ks;
            float l = L[i] * gs, r = R[i] * gs;
            if (clip > 0) { l = lerp (l, fastTanh (l), clip); r = lerp (r, fastTanh (r), clip); }
            L[i] = l; R[i] = r;
            m = std::max (m, std::fabs (gDb));
        }
        meter = clampf (m / 18.0f, 0, 1);
    }
};

// =====================================================================================
struct LimiterFx final : Effect
{
    float env = 0;
    void reset() override { env = 0; }
    static inline float clipper (float x, float ceil, float soft)
    {
        const float u = x / ceil;
        return ceil * lerp (clampf (u, -1.0f, 1.0f), clampf (fastTanh (u * 1.3f) * 1.1605f, -1.0f, 1.0f), soft);
    }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float drive = dbToGain (v[0]), ceil = dbToGain (v[1]);
        const float relC = onePoleCoef (v[2], sr), soft = v[3] * 0.01f;
        const int mode = (int) v[4];
        const float limCeil = mode == 2 ? ceil * 1.26f : ceil;
        float minG = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i] * drive, r = R[i] * drive;
            if (mode != 1)
            {
                const float x = maxAbs (l, r);
                env = x > env ? x : x + relC * (env - x);
                const float g = env > limCeil ? limCeil / env : 1.0f;
                l *= g; r *= g; minG = std::min (minG, g);
            }
            if (mode != 0)
            {
                l = clipper (l, ceil, soft); r = clipper (r, ceil, soft);
            }
            L[i] = clampf (l, -ceil, ceil); R[i] = clampf (r, -ceil, ceil);
        }
        meter = clampf (-gainToDb (minG) / 12.0f, 0, 1);
    }
};

// =====================================================================================
struct SwellFx final : Effect
{
    EnvFollower fast, slow; float ramp = 1, g = 1; int refractory = 0;
    void reset() override { fast.reset(); slow.reset(); ramp = 1; g = 1; refractory = 0; }
    void prepare (double s, int b) override { Effect::prepare (s, b); fast.setup (0.5f, 40.0f, sr); slow.setup (40.0f, 250.0f, sr); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float thr = v[0], inc = 1.0f / (v[1] * 0.001f * float (sr)), curve = v[2] * 0.01f, retrig = v[3];
        const float kg = 1.0f - onePoleCoef (3.0f, sr);
        const int refr = int (0.06 * sr);
        for (int i = 0; i < n; ++i)
        {
            const float x = maxAbs (L[i], R[i]) + 1.0e-7f;
            const float fDb = gainToDb (fast.tick (x));
            const float sDb = gainToDb (slow.tick (x));
            if (refractory > 0) --refractory;
            if (fDb < thr) ramp = 0.0f;
            else if (fDb - sDb > retrig && refractory == 0) { ramp = 0.0f; refractory = refr; }
            ramp = std::min (1.0f, ramp + inc);
            const float shape = lerp (ramp, ramp * ramp * ramp, curve);
            g += (shape - g) * kg;
            L[i] *= g; R[i] *= g;
        }
        meter = g;
    }
};

// =====================================================================================
struct OverdriveFx final : Effect
{
    OnePole preLP[2], tightHP[2], toneLP[2]; DcBlock dc[2]; Biquad hump; Os2 os[2]; int lastVoice = -1;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (int c = 0; c < 2; ++c) { preLP[c].setHz (8000.0f, sr); dc[c].setup (sr); }
        lastVoice = -1;
        reset();
    }
    void reset() override { for (int c = 0; c < 2; ++c) { preLP[c].reset(); tightHP[c].reset(); toneLP[c].reset(); dc[c].reset(); os[c].reset(); } hump.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float humpF[4] = { 720.0f, 1000.0f, 500.0f, 1800.0f }, humpQ[4] = { 0.8f, 0.5f, 0.6f, 0.7f }, humpG[4] = { 4.0f, 1.5f, 2.5f, 5.0f };
        static const float gBase[4] = { 6.0f, 2.0f, 4.0f, 10.0f }, gRange[4] = { 40.0f, 30.0f, 34.0f, 34.0f };
        const int voice = std::max (0, std::min (3, (int) v[6]));
        if (voice != lastVoice) { hump.set (Biquad::Peak, humpF[voice], humpQ[voice], humpG[voice], sr); lastVoice = voice; }
        const float drive = v[0] * 0.01f, tone = v[1] * 0.01f, clean = v[3] * 0.01f + (voice == 1 ? 0.35f : 0.0f), asym = v[4] * 0.01f;
        const float outG = dbToGain (v[5]);
        const float gain = dbToGain (gBase[voice] + drive * gRange[voice]);
        const float bias = asym * 0.4f + (voice == 3 ? 0.2f : 0.0f);
        auto shape = [voice] (float u)
        {
            switch (voice)
            {
                case 1:  return 1.5f * u / (1.0f + std::fabs (u));                                    // transparent: wide soft knee
                case 2:  return u > 0.0f ? fastTanh (u) : fastTanh (u * 0.7f) * 1.15f;                  // blues: gentle asymmetry
                case 3:  return u > 0.0f ? fastTanh (u * 1.5f) * 0.8f : fastTanh (u * 0.5f) * 1.25f;    // germanium treble boost
                default: return fastTanh (u);                                                         // screamer
            }
        };
        const float off = shape (bias);
        const float tightHz = voice == 3 ? std::max (v[2], 600.0f) : v[2];
        for (int c = 0; c < 2; ++c) { tightHP[c].setHz (tightHz, sr); toneLP[c].setHz (600.0f * std::pow (20.0f, tone), sr); }
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                const float s = tightHP[c].hp (preLP[c].lp (x)) * gain;
                float y = os[c].process (s, [&] (float u) { return shape (u + bias) - off; });
                y = dc[c].tick (y);
                y = hump.tick (y, c);
                y = toneLP[c].lp (y);
                ch[c][i] = (y * 0.7f + x * clean * 1.5f) * outG;
            }
    }
};

// =====================================================================================
struct DistortionFx final : Effect
{
    OnePole aaLP[2], inHP[2], toneLP[2], muffLP[2], muffHP[2]; DcBlock dc[2]; Biquad mids, hmLo, hmMid; EnvFollower env[2]; Os4 os[2];
    float lastMid = -999;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (int c = 0; c < 2; ++c) { aaLP[c].setHz (9000.0f, sr); inHP[c].setHz (80.0f, sr); muffLP[c].setHz (700.0f, sr); muffHP[c].setHz (1300.0f, sr); dc[c].setup (sr); env[c].setup (1.0f, 60.0f, sr); }
        hmLo.set (Biquad::Peak, 95.0f, 1.0f, 10.0f, sr); hmMid.set (Biquad::Peak, 1100.0f, 0.9f, 12.0f, sr);
        lastMid = -999; reset();
    }
    void reset() override { for (int c = 0; c < 2; ++c) { aaLP[c].reset(); inHP[c].reset(); toneLP[c].reset(); muffLP[c].reset(); muffHP[c].reset(); dc[c].reset(); env[c].reset(); os[c].reset(); } mids.reset(); hmLo.reset(); hmMid.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float g = v[0] * 0.01f, tone = v[1] * 0.01f, bias = v[3] * 0.01f, outG = dbToGain (v[5]);
        const int mode = std::max (0, std::min (5, (int) v[2]));
        if (v[4] != lastMid) { mids.set (Biquad::Peak, 500.0f, 0.7f, v[4], sr); lastMid = v[4]; }
        static const float gB[6] = { 10, 6, 6, 6, 8, 12 }, gR[6] = { 50, 30, 40, 36, 44, 36 };
        const float gain = dbToGain (gB[mode] + g * gR[mode]);
        const float toneHz = mode == 0 ? 400.0f * std::pow (40.0f, tone) : (mode == 5 ? 2500.0f * std::pow (4.0f, tone) : 1500.0f * std::pow (7.0f, tone));
        for (int c = 0; c < 2; ++c) toneLP[c].setHz (toneHz, sr);
        const float b = -bias * 1.2f;
        auto rat  = [] (float u) { return fastTanh (u) * 0.6f + clampf (u, -1.0f, 1.0f) * 0.4f; };
        auto face = [] (float u) { return u > 0 ? fastTanh (u * 1.2f) : fastTanh (u * 0.6f) * 1.25f; };
        auto hard = [] (float u) { return clampf (u, -1.0f, 1.0f) * 0.85f + fastTanh (u) * 0.15f; };
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = inHP[c].hp (aaLP[c].lp (ch[c][i]));
                const float e = env[c].tick (x);
                float y;
                switch (mode)
                {
                    case 0:  y = toneLP[c].lp (os[c].process (x * gain, [&] (float u) { return rat (u + b) - rat (b); })); break;
                    case 1:
                    {
                        const float u2 = os[c].process (x * gain, [&] (float u) { const float a = fastTanh (u + b) - fastTanh (b); return fastTanh (a * gain * 0.35f + 0.05f); });
                        y = lerp (muffLP[c].lp (u2), muffHP[c].hp (u2), tone) * 1.6f;
                        break;
                    }
                    case 2:  y = toneLP[c].lp (os[c].process (x * gain, [&] (float u) { return face (u + b) - face (b); })); break;
                    case 3:  y = toneLP[c].lp (os[c].process (x, [&] (float u) { return fastTanh ((std::fabs (u) * 2.0f - e * 1.2f) * gain + b) - fastTanh (b); })); break;  // rectifier = octave up
                    case 4:  y = toneLP[c].lp (os[c].process (x * gain, [&] (float u) { return hard (u + b) - hard (b); })) * 0.8f; break;
                    default:
                    {
                        const float u2 = os[c].process (x * gain, [&] (float u) { const float a = face (u * 0.5f + b) - face (b); return hard (a * 6.0f); });
                        y = toneLP[c].lp (hmMid.tick (hmLo.tick (u2, c), c)) * 0.3f;   // the "chainsaw" double mid/low peaks
                        break;
                    }
                }
                if (bias > 0.0f)
                {
                    const float thr = bias * 0.25f / gain + 1.0e-6f;
                    const float gg = clampf (e / thr, 0.0f, 1.0f);
                    y *= gg * gg;
                }
                y = dc[c].tick (y);
                ch[c][i] = mids.tick (y, c) * outG;
            }
    }
};

// =====================================================================================
struct AmpFx final : Effect
{
    struct Voice { float inHP, s1LP, c1HP, s2LP, c2Lo, c2Hi, bassF, midF, trebF, midOff, g1, r1, g2, k2, r2, g3, k3, r3, bias, out; };
    OnePole inHP[2], s1LP[2], c1HP[2], s2LP[2], c2HP[2];
    Biquad bass, mid, treb, pres, cab[6];
    EnvFollower sag[2]; Os4 os[2];
    int nCab = 0, lastVoice = -1;
    float last[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    double osr = 192000.0;
    static const Voice& voiceTable (int i)
    {
        //                 inHP  s1LP   c1HP s2LP   c2Lo c2Hi bass mid  treb  midOff g1    r1  g2    k2    r2  g3    k3    r3  bias   out
        static const Voice t[5] = {
            /* Brit Classic */ { 40, 12000, 30, 9000, 30, 120, 100, 650, 2800, 0.0f, 2.0f, 24, 1.5f, 0.3f, 30, 1.2f, 0.6f, 18, 0.15f, 1.0f },
            /* US Clean     */ { 30, 14000, 20, 12000, 20, 60, 80, 450, 3500, -3.0f, 1.5f, 18, 1.0f, 0.5f, 14, 1.0f, 1.0f, 0, 0.08f, 1.5f },
            /* Tweed        */ { 60, 9000, 50, 7000, 60, 120, 120, 800, 2200, 2.0f, 2.0f, 22, 1.5f, 0.3f, 24, 1.2f, 0.7f, 10, 0.3f, 1.0f },
            /* Modern Gain  */ { 60, 11000, 120, 8000, 150, 300, 110, 750, 3200, -3.0f, 2.5f, 26, 2.0f, 0.2f, 34, 1.5f, 0.5f, 20, 0.12f, 0.8f },
            /* Bass Amp     */ { 25, 8000, 15, 6000, 20, 25, 70, 500, 2000, 0.0f, 1.5f, 20, 1.2f, 0.4f, 20, 1.0f, 0.8f, 8, 0.1f, 1.3f } };
        return t[std::max (0, std::min (4, i))];
    }
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        osr = sr * 4.0;
        for (auto& x : last) x = -1;
        lastVoice = -1;
        reset();
    }
    void reset() override
    {
        for (int c = 0; c < 2; ++c) { inHP[c].reset(); s1LP[c].reset(); c1HP[c].reset(); s2LP[c].reset(); c2HP[c].reset(); sag[c].reset(); os[c].reset(); }
        bass.reset(); mid.reset(); treb.reset(); pres.reset(); for (auto& q : cab) q.reset();
    }
    void setCab (int type)
    {
        struct F { Biquad::Type t; float f, q, g; };
        static const F cabs[4][6] = {
            { { Biquad::HP, 80, 0.7f, 0 }, { Biquad::Peak, 110, 1.5f, 3 }, { Biquad::Peak, 400, 1.0f, -2 }, { Biquad::Peak, 1800, 1.2f, 2 }, { Biquad::LP, 5500, 0.8f, 0 }, { Biquad::LP, 6500, 0.6f, 0 } },
            { { Biquad::HP, 75, 0.8f, 0 }, { Biquad::Peak, 120, 1.2f, 4 }, { Biquad::Peak, 500, 1.2f, -3 }, { Biquad::Peak, 2500, 1.5f, 4 }, { Biquad::LP, 5000, 0.9f, 0 }, { Biquad::LP, 6000, 0.6f, 0 } },
            { { Biquad::HP, 70, 0.9f, 0 }, { Biquad::Peak, 100, 1.0f, 5 }, { Biquad::Peak, 600, 0.9f, -5 }, { Biquad::Peak, 3000, 2.0f, 3 }, { Biquad::LP, 4500, 1.0f, 0 }, { Biquad::LP, 5500, 0.6f, 0 } },
            { { Biquad::HP, 50, 0.7f, 0 }, { Biquad::Peak, 90, 1.0f, 3 }, { Biquad::Peak, 800, 1.0f, -1 }, { Biquad::Peak, 2000, 1.2f, 3 }, { Biquad::LP, 4000, 0.8f, 0 }, { Biquad::LP, 5000, 0.6f, 0 } } };
        nCab = type > 0 ? 6 : 0;
        if (type > 0)
            for (int k = 0; k < 6; ++k) { const auto& f = cabs[type - 1][k]; cab[k].set (f.t, f.f, f.q, f.g, sr); }
    }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const int vi = std::max (0, std::min (4, (int) v[7]));
        const Voice& V = voiceTable (vi);
        const bool vch = vi != lastVoice;
        if (vch)
        {
            for (int c = 0; c < 2; ++c)
            {
                inHP[c].setHz (V.inHP, sr); s1LP[c].setHz (V.s1LP, osr); c1HP[c].setHz (V.c1HP, osr); s2LP[c].setHz (V.s2LP, osr);
                sag[c].setup (10.0f, 150.0f, osr);
            }
            lastVoice = vi;
        }
        // the tone stack runs inside the oversampled section, so it is designed for the 4x rate
        if (vch || v[1] != last[1]) bass.set (Biquad::LowShelf, V.bassF, 0.7f, (v[1] - 5.0f) * 2.4f, osr);
        if (vch || v[2] != last[2]) mid.set (Biquad::Peak, V.midF, 0.8f, (v[2] - 5.0f) * 2.4f + V.midOff, osr);
        if (vch || v[3] != last[3]) treb.set (Biquad::HighShelf, V.trebF, 0.7f, (v[3] - 5.0f) * 2.4f, osr);
        if (v[4] != last[4]) pres.set (Biquad::HighShelf, 3800.0f, 0.7f, (v[4] - 5.0f) * 1.8f, sr);
        if (v[5] != last[5]) setCab ((int) v[5]);
        for (int k = 0; k < 8; ++k) last[k] = v[k];
        const float g = v[0] * 0.01f;
        const float pre1 = dbToGain (g * V.r1) * V.g1;
        const float pre2 = dbToGain (clampf ((g - V.k2) / (1.0f - V.k2 + 1.0e-3f), 0, 1) * V.r2) * V.g2;
        const float pre3 = dbToGain (clampf ((g - V.k3) / (1.0f - V.k3 + 1.0e-3f), 0, 1) * V.r3) * V.g3;
        const float master = dbToGain ((v[6] - 5.0f) * 2.4f);
        const float pd = 1.3f * master;
        const float outG = 0.3f / (1.0f + g * 0.6f) * V.out / std::sqrt (std::max (master, 0.25f));
        const float bias = V.bias, bOff = fastTanh (bias);
        c2HP[0].setHz (lerp (V.c2Lo, V.c2Hi, g), osr); c2HP[1].a = c2HP[0].a;
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = inHP[c].hp (ch[c][i]);
                float p = os[c].process (x, [&] (float u)
                {
                    float a = fastTanh (u * pre1 + bias) - bOff;
                    a = c1HP[c].hp (s1LP[c].lp (a));
                    float b = fastTanh (a * pre2 + bias) - bOff;
                    b = c2HP[c].hp (s2LP[c].lp (b));
                    float s = fastTanh (b * pre3 + bias) - bOff;
                    s = treb.tick (mid.tick (bass.tick (s, c), c), c);
                    const float e = sag[c].tick (s);
                    return fastTanh (s * pd / (1.0f + e * 0.5f * g * master));     // power amp with supply sag
                });
                p = pres.tick (p, c);
                for (int k = 0; k < nCab; ++k) p = cab[k].tick (p, c);
                ch[c][i] = p * outG;
            }
    }
};

// =====================================================================================
struct TapeFx final : Effect
{
    DelayLine dl[2]; Biquad preE, deE, bump, hf; Rng rng { 1234u }; Os2 os[2];
    float wowPh = 0, flPh = 0, drift = 0, driftTgt = 0; int driftCtr = 0;
    Smooth base; float dropG = 1; int dropLeft = 0; float dropDepth = 0;
    float lastTone = -1, lastAge = -1; int lastSpeed = -1;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& d : dl) d.init (int (0.03 * sr));
        preE.set (Biquad::HighShelf, 3000.0f, 0.7f, 6.0f, sr);
        deE.set (Biquad::HighShelf, 3000.0f, 0.7f, -6.0f, sr);
        base.setTime (200.0f, sr); base.reset (float (0.001 * sr) + 2.0f);
        lastTone = lastAge = -1; lastSpeed = -1;
        reset();
    }
    void reset() override { for (auto& d : dl) d.clear(); preE.reset(); deE.reset(); bump.reset(); hf.reset(); os[0].reset(); os[1].reset(); dropG = 1; dropLeft = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float bumpF[3] = { 45.0f, 90.0f, 140.0f }, bumpG[3] = { 3.5f, 2.5f, 2.0f }, hfMul[3] = { 0.6f, 1.0f, 1.2f }, hissMul[3] = { 1.6f, 1.0f, 0.6f };
        const int speed = std::max (0, std::min (2, (int) v[6]));
        const float g = dbToGain (v[0]), comp = std::pow (g, -0.6f);
        const float wow = v[1] * 0.01f, flut = v[2] * 0.01f, hiss = v[3] * 0.01f * hissMul[speed], tone = v[4] * 0.01f, age = v[5] * 0.01f;
        if (speed != lastSpeed) bump.set (Biquad::Peak, bumpF[speed], 1.0f, bumpG[speed], sr);
        if (tone != lastTone || age != lastAge || speed != lastSpeed)
        {
            hf.set (Biquad::LP, std::min (20000.0f, 20000.0f * std::pow (0.3f, 1.0f - tone) * (1.0f - age * 0.5f) * hfMul[speed]), 0.7f, 0.0f, sr);
            lastTone = tone; lastAge = age;
        }
        lastSpeed = speed;
        const float fs = float (sr);
        const float wowD = wow * (0.0025f + age * 0.003f) * fs;
        const float flD = flut * 0.00012f * fs;
        base.tgt = 0.001f * fs + wowD + flD + 2.0f;
        const float wowInc = (0.55f + age * 0.3f) / fs, flInc = 7.3f / fs;
        const float dropKg = 1.0f - onePoleCoef (5.0f, sr);
        const float dropProb = age * 1.5f / fs;
        const float gd = g * 0.8f, gc = comp * 1.25f;
        auto sat = [gd, gc] (float u) { return fastTanh (u * gd) * gc; };
        for (int i = 0; i < n; ++i)
        {
            if (--driftCtr <= 0) { driftCtr = int (0.25f * fs); driftTgt = rng.bi(); }
            drift += (driftTgt - drift) * 0.00005f;
            wowPh += wowInc; if (wowPh >= 1) wowPh -= 1;
            flPh += flInc; if (flPh >= 1) flPh -= 1;
            const float b = base.next();
            const float d = b + wowD * (0.7f * fastSin2Pi (wowPh) + 0.3f * drift) + flD * fastSin2Pi (flPh);
            if (dropLeft > 0) --dropLeft;
            else if (age > 0 && rng.uni() < dropProb) { dropLeft = int ((0.02f + 0.1f * rng.uni()) * fs); dropDepth = age * (0.3f + 0.5f * rng.uni()); }
            dropG += ((dropLeft > 0 ? 1.0f - dropDepth : 1.0f) - dropG) * dropKg;
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                float x = preE.tick (ch[c][i], c);
                x = os[c].process (x, sat);
                x = hf.tick (bump.tick (deE.tick (x, c), c), c);
                dl[c].push (x);
                float y = dl[c].read (d) * dropG;
                if (hiss > 0) y += rng.bi() * hiss * 0.006f;
                ch[c][i] = y;
            }
        }
    }
};

// =====================================================================================
struct EqFx final : Effect
{
    Biquad lc, lo, mi, hi, hc; float last[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    bool useLc = false, useLo = false, useMi = false, useHi = false, useHc = false;
    void prepare (double s, int b) override { Effect::prepare (s, b); for (auto& x : last) x = -1; reset(); }
    void reset() override { lc.reset(); lo.reset(); mi.reset(); hi.reset(); hc.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        bool changed = false;
        for (int k = 0; k < 7; ++k) if (v[k] != last[k]) { changed = true; last[k] = v[k]; }
        if (changed)
        {
            useLc = v[0] > 20.5f;  if (useLc) lc.set (Biquad::HP, v[0], 0.707f, 0, sr);
            useLo = std::fabs (v[1]) > 0.05f; if (useLo) lo.set (Biquad::LowShelf, 100.0f, 0.7f, v[1], sr);
            useMi = std::fabs (v[3]) > 0.05f; if (useMi) mi.set (Biquad::Peak, v[2], v[6], v[3], sr);
            useHi = std::fabs (v[4]) > 0.05f; if (useHi) hi.set (Biquad::HighShelf, 8000.0f, 0.7f, v[4], sr);
            useHc = v[5] < 19600.0f; if (useHc) hc.set (Biquad::LP, v[5], 0.707f, 0, sr);
        }
        const float og = dbToGain (v[7]);
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                if (useLc) x = lc.tick (x, c);
                if (useLo) x = lo.tick (x, c);
                if (useMi) x = mi.tick (x, c);
                if (useHi) x = hi.tick (x, c);
                if (useHc) x = hc.tick (x, c);
                ch[c][i] = x * og;
            }
    }
};

// =====================================================================================
struct AutoWahFx final : Effect
{
    EnvFollower env; Svf f[2]; float fs = 300;
    void reset() override { env.reset(); f[0].reset(); f[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        env.setup (2.0f, v[4], sr);
        const float sensG = dbToGain (v[0] * 0.3f - 6.0f);
        const float fLow = v[1], range = v[2], q = v[3];
        const int mode = (int) v[5];
        const float ks = 1.0f - onePoleCoef (2.0f, sr);
        const float kq = 1.0f / q;
        const float outC = 1.0f / (1.0f + q * 0.12f);
        float e = 0;
        for (int i = 0; i < n; ++i)
        {
            e = 1.0f - std::exp (-env.tick ((L[i] + R[i]) * 0.5f) * sensG * 1.5f);
            const float pos = mode == 3 ? 1.0f - e : e;
            const float tgt = fLow * std::exp2 (range * std::sqrt (pos));
            fs += (tgt - fs) * ks;
            const float g = fastTan (kPi * clampf (fs, 20.0f, float (sr) * 0.45f) / float (sr));
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                f[c].setG (g, q);
                f[c].tick (ch[c][i]);
                float y;
                switch (mode)
                {
                    case 0:  y = f[c].lp * outC; break;
                    case 2:  y = f[c].hp * outC; break;
                    default: y = f[c].bp * kq * 3.5f; break;
                }
                ch[c][i] = y;
            }
        }
        meter = e;
    }
};

// =====================================================================================
struct LadderFx final : Effect
{
    float s[2][4] = {}; float ph = 0; Smooth cut; Os2 os[2];
    void prepare (double sm, int b) override { Effect::prepare (sm, b); cut.setTime (8.0f, sr); cut.reset (2000.0f); reset(); }
    void reset() override { for (auto& c : s) for (auto& x : c) x = 0; os[0].reset(); os[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        cut.tgt = v[0];
        const float k = v[1] * 0.01f * 3.9f, drive = dbToGain (v[2]);
        const int mode = (int) v[3];
        const float inc = v[4] / float (sr), depth = v[5];
        const float comp = (mode <= 1 ? 1.0f + k * 0.35f : 1.0f) / std::sqrt (drive);
        const float nyq = float (sr) * 0.45f, osr = float (sr) * 2.0f;
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            const float c0 = cut.next();
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = depth > 0 ? fastSin2Pi (ph + 0.25f * float (c)) : 0.0f;
                const float fc = clampf (c0 * std::exp2 (depth * lfo), 20.0f, nyq);
                const float g = fastTan (kPi * fc / osr);              // the filter core runs at 2x: cleaner resonance and drive
                const float G = g / (1.0f + g), inv = 1.0f / (1.0f + g);
                const float G4 = G * G * G * G;
                float* st = s[c];
                const float out = os[c].process (ch[c][i] * drive, [&] (float in0)
                {
                    const float S = (G * G * G * st[0] + G * G * st[1] + G * st[2] + st[3]) * inv;
                    const float u = fastTanh ((in0 - k * S) / (1.0f + k * G4));
                    float y[4]; float in = u;
                    for (int p = 0; p < 4; ++p) { const float vv = (in - st[p]) * G; y[p] = vv + st[p]; st[p] = y[p] + vv; in = y[p]; }
                    switch (mode)
                    {
                        case 0:  return y[3];
                        case 1:  return y[1];
                        case 2:  return (y[0] - y[1]) * 3.0f;
                        default: return u - 4 * y[0] + 6 * y[1] - 4 * y[2] + y[3];
                    }
                });
                ch[c][i] = out * comp;
            }
        }
    }
};

// =====================================================================================
struct TalkFx final : Effect
{
    Svf f[2][3]; EnvFollower env; float ph = 0; int ctr = 0;
    float g[3] = { 0.1f, 0.2f, 0.4f }, q = 8.0f;
    void reset() override { for (auto& a : f) for (auto& b : a) b.reset(); env.reset(); ctr = 0; }
    void prepare (double s, int b) override { Effect::prepare (s, b); env.setup (3.0f, 120.0f, sr); reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float F[5][3] = { { 800, 1150, 2900 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        static const float A[3] = { 1.0f, 0.55f, 0.3f };
        const float inc = v[1] / float (sr), lfoD = v[2] * 0.01f, envA = v[3] * 0.01f;
        q = 4.0f + v[4] * 0.16f;
        const float shift = std::exp2 (v[5] / 12.0f);
        const float kq = 1.0f / q;
        float e = 0;
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            e = env.tick ((L[i] + R[i]) * 0.5f);
            if (ctr-- <= 0)
            {
                ctr = 8;
                const float vp = clampf (v[0] + lfoD * 2.0f * fastSin2Pi (ph) + envA * 4.0f * std::min (1.0f, e * 4.0f), 0.0f, 3.999f);
                const int a = (int) vp; const float fr = vp - float (a);
                for (int k = 0; k < 3; ++k)
                {
                    const float fk = lerp (F[a][k], F[a + 1][k], fr) * shift;
                    g[k] = fastTan (kPi * clampf (fk, 60.0f, float (sr) * 0.45f) / float (sr));
                }
            }
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                float y = 0;
                for (int k = 0; k < 3; ++k) { f[c][k].setG (g[k], q); f[c][k].tick (ch[c][i]); y += f[c][k].bp * kq * A[k]; }
                ch[c][i] = y * 5.5f;
            }
        }
        meter = clampf (e * 4.0f, 0, 1);
    }
};

// =====================================================================================
struct ChorusFx final : Effect
{
    DelayLine dl[2]; float ph[4] = { 0, 0.25f, 0.5f, 0.75f }; OnePole tone[2]; Smooth base, dep;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& d : dl) d.init (int (0.045 * sr));
        base.setTime (60.0f, sr); base.reset (9.0f); dep.setTime (60.0f, sr); dep.reset (3.0f);
        reset();
    }
    void reset() override { for (auto& d : dl) d.clear(); tone[0].reset(); tone[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float rate = v[0], depth = v[1] * 0.01f;
        const int voices = std::max (1, std::min (4, (int) v[3]));
        const float spread = v[4] * 0.01f;
        base.tgt = v[2]; dep.tgt = depth * std::min (v[2] * 0.85f, 7.0f);
        for (int c = 0; c < 2; ++c) tone[c].setHz (v[5], sr);
        const float msToS = float (sr) * 0.001f;
        const float norm = 1.0f / std::sqrt (float (voices));
        for (int i = 0; i < n; ++i)
        {
            const float b = base.next(), d = dep.next();
            dl[0].push (L[i]); dl[1].push (R[i]);
            float yl = 0, yr = 0;
            for (int k = 0; k < voices; ++k)
            {
                ph[k] += rate * (1.0f + 0.07f * float (k)) / float (sr);
                if (ph[k] >= 1) ph[k] -= 1;
                const float lfoL = fastSin2Pi (ph[k]);
                const float lfoR = fastSin2Pi (ph[k] + spread * 0.25f + 0.5f * float (k & 1) * spread);
                yl += dl[0].readCubic ((b + d * lfoL) * msToS);
                yr += dl[1].readCubic ((b + d * lfoR) * msToS);
            }
            L[i] = tone[0].lp (yl * norm);
            R[i] = tone[1].lp (yr * norm);
        }
    }
};

// =====================================================================================
inline float syncedRate (int choice, const float* beatsTable, double bpm) { return float (bpm / 60.0) / beatsTable[choice - 1]; }
inline float syncedPhase (double ppq, float beats) { const double p = ppq / beats; return float (p - std::floor (p)); }

struct FlangerFx final : Effect
{
    DelayLine dl[2]; float ph = 0; float fb[2] = { 0, 0 }; Smooth man;
    void prepare (double s, int b) override { Effect::prepare (s, b); for (auto& d : dl) d.init (int (0.03 * sr)); man.setTime (40.0f, sr); man.reset (1.5f); reset(); }
    void reset() override { for (auto& d : dl) d.clear(); fb[0] = fb[1] = 0; }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float beats[] = { 16.0f, 8.0f, 4.0f, 2.0f, 1.0f };
        const int sync = (int) v[5];
        float rate = v[0];
        if (sync > 0) { rate = syncedRate (sync, beats, ctx.bpm); if (ctx.playing) ph = syncedPhase (ctx.ppq, beats[sync - 1]); }
        const float depth = v[1] * 0.01f, fbk = v[3] * 0.01f, spread = v[4] * 0.01f;
        man.tgt = v[2];
        const float msToS = float (sr) * 0.001f, maxD = 0.025f * float (sr);
        const float inc = rate / float (sr);
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            const float m = man.next();
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = 0.5f + 0.5f * fastSin2Pi (ph + spread * 0.5f * float (c));
                const float d = clampf (m * std::exp2 (depth * 3.0f * lfo) * msToS, 2.0f, maxD);
                const float y = dl[c].readCubic (d);
                const float x = ch[c][i];
                dl[c].push (x + fbk * fastTanh (y));
                ch[c][i] = 0.5f * (x + y);
            }
        }
    }
};

// =====================================================================================
struct PhaserFx final : Effect
{
    AllPass1 ap[2][12]; float last[2] = { 0, 0 }; float ph = 0;
    void reset() override { for (auto& a : ap) for (auto& b : a) b.reset(); last[0] = last[1] = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const int stagesT[] = { 2, 4, 6, 8, 12 };
        const int stages = stagesT[std::max (0, std::min (4, (int) v[2]))];
        const float inc = v[0] / float (sr), depth = v[1] * 0.01f * 2.0f, fbk = v[3] * 0.01f, center = v[4], spread = v[5] * 0.01f;
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = fastSin2Pi (ph + spread * 0.5f * float (c));
                const float a = allpassCoefFromHz (center * std::exp2 (depth * lfo), sr);
                const float x = ch[c][i];
                float y = x + fbk * last[c];
                for (int s = 0; s < stages; ++s) y = ap[c][s].tick (y, a);
                last[c] = y;
                ch[c][i] = 0.5f * (x + y);
            }
        }
    }
};

// =====================================================================================
struct TremoloFx final : Effect
{
    float ph = 0; OnePole xo[2];
    void prepare (double s, int b) override { Effect::prepare (s, b); xo[0].setHz (800.0f, sr); xo[1].setHz (800.0f, sr); }
    void reset() override { xo[0].reset(); xo[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float beats[] = { 2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
        const int sync = (int) v[4];
        float rate = v[0];
        if (sync > 0) { rate = syncedRate (sync, beats, ctx.bpm); if (ctx.playing) ph = syncedPhase (ctx.ppq, beats[sync - 1]); }
        const float depth = v[1] * 0.01f, shape = v[2] * 0.01f, stereo = v[3] * 0.01f, harm = v[5] * 0.01f;
        const float sh = 1.0f + shape * 12.0f, shN = 1.0f / fastTanh (sh);
        const float inc = rate / float (sr);
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            for (int c = 0; c < 2; ++c)
            {
                float lfo = fastSin2Pi (ph + stereo * 0.5f * float (c));
                lfo = fastTanh (lfo * sh) * shN;
                const float g = 1.0f - depth * (0.5f + 0.5f * lfo);
                const float x = ch[c][i];
                if (harm > 0.0f)
                {
                    const float gi = 1.0f - depth * (0.5f - 0.5f * lfo);
                    const float lo = xo[c].lp (x), hi = x - lo;
                    ch[c][i] = lerp (x * g, lo * g + hi * gi, harm);
                }
                else ch[c][i] = x * g;
            }
        }
    }
};

// =====================================================================================
struct VibeFx final : Effect
{
    AllPass1 ap[2][4]; float ph = 0; float lamp[2] = { 0, 0 };
    void reset() override { for (auto& a : ap) for (auto& b : a) b.reset(); lamp[0] = lamp[1] = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float baseF[4] = { 90.0f, 240.0f, 600.0f, 1400.0f };
        const float inc = v[0] / float (sr), inten = v[1] * 0.01f, throb = v[3] * 0.01f, width = v[4] * 0.01f;
        const bool vibrato = (int) v[2] == 1;
        const float kUp = 1.0f - onePoleCoef (5.0f, sr), kDn = 1.0f - onePoleCoef (40.0f, sr);
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            for (int c = 0; c < 2; ++c)
            {
                const float raw = 0.5f + 0.5f * fastSin2Pi (ph + width * 0.25f * float (c));
                lamp[c] += (raw - lamp[c]) * (raw > lamp[c] ? kUp : kDn);
                const float lm = lamp[c] * std::sqrt (std::max (lamp[c], 0.0f));
                const float mult = 1.0f + inten * lm * 5.0f;
                const float x = ch[c][i];
                float y = x;
                for (int s = 0; s < 4; ++s) y = ap[c][s].tick (y, allpassCoefFromHz (baseF[s] * mult, sr));
                y *= 1.0f - throb * 0.35f * lm;
                ch[c][i] = vibrato ? y : 0.5f * (x + y) * 1.2f;
            }
        }
    }
};

// =====================================================================================
struct RotaryFx final : Effect
{
    Biquad lp1, lp2, hp1, hp2; DelayLine horn, drum;
    float hRate = 0.83f, dRate = 0.67f, hPh = 0, dPh = 0;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        lp1.set (Biquad::LP, 800.0f, 0.707f, 0, sr); lp2 = lp1;
        hp1.set (Biquad::HP, 800.0f, 0.707f, 0, sr); hp2 = hp1;
        horn.init (int (0.01 * sr)); drum.init (int (0.01 * sr));
        reset();
    }
    void reset() override { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); horn.clear(); drum.clear(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const int speed = (int) v[0];
        const float hT = speed == 0 ? 0.83f : (speed == 1 ? 6.7f : 0.0f);
        const float dT = speed == 0 ? 0.67f : (speed == 1 ? 5.8f : 0.0f);
        const float kH = 1.0f - std::exp (-1.0f / (v[1] * float (sr))), kD = 1.0f - std::exp (-1.0f / (v[1] * 2.5f * float (sr)));
        const float bal = v[2] * 0.01f, drv = v[3] * 0.01f, width = v[4] * 0.01f, depth = v[5] * 0.01f;
        const float hG = std::min (1.0f, 2.0f * bal), dG = std::min (1.0f, 2.0f * (1.0f - bal));
        const float dGain = 1.0f + drv * 4.0f, dComp = 1.0f / (1.0f + drv * 1.5f);
        const float ms = float (sr) * 0.001f;
        for (int i = 0; i < n; ++i)
        {
            hRate += (hT - hRate) * kH; dRate += (dT - dRate) * kD;
            hPh += hRate / float (sr); if (hPh >= 1) hPh -= 1;
            dPh += dRate / float (sr); if (dPh >= 1) dPh -= 1;
            float m = (L[i] + R[i]) * 0.5f;
            if (drv > 0) m = fastTanh (m * dGain) * dComp * 1.3f;
            const float lo = lp2.tick (lp1.tick (m, 0), 0);
            const float hi = hp2.tick (hp1.tick (m, 0), 0);
            horn.push (hi); drum.push (lo);
            float out[2];
            for (int c = 0; c < 2; ++c)
            {
                const float off = width * 0.5f * float (c);
                const float hs = fastSin2Pi (hPh + off), hc = fastSin2Pi (hPh + off + 0.25f);
                const float ds = fastSin2Pi (dPh + off), dc = fastSin2Pi (dPh + off + 0.25f);
                const float hy = horn.readCubic ((1.5f + depth * 0.8f * hs) * ms) * (1.0f - depth * 0.45f * (0.5f - 0.5f * hc));
                const float dy = drum.readCubic ((1.0f + depth * 0.25f * ds) * ms) * (1.0f - depth * 0.3f * (0.5f - 0.5f * dc));
                out[c] = (hy * hG + dy * dG) * 0.7f;
            }
            L[i] = out[0]; R[i] = out[1];
        }
        meter = clampf (hRate / 6.7f, 0, 1);
    }
};

// =====================================================================================
struct RingModFx final : Effect
{
    float ph = 0, lph = 0; OnePole tone[2];
    void reset() override { tone[0].reset(); tone[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const int shape = (int) v[1];
        const float linc = v[2] / float (sr), ld = v[3] * 0.01f * 2.0f;
        tone[0].setHz (v[4], sr); tone[1].setHz (v[4], sr);
        for (int i = 0; i < n; ++i)
        {
            lph += linc; if (lph >= 1) lph -= 1;
            const float f = ld > 0 ? v[0] * std::exp2 (ld * fastSin2Pi (lph)) : v[0];
            ph += f / float (sr); ph -= std::floor (ph);
            float car;
            switch (shape)
            {
                case 0:  car = fastSin2Pi (ph); break;
                case 1:  car = 1.0f - 4.0f * std::fabs (ph - 0.5f); break;
                default: car = fastTanh (fastSin2Pi (ph) * 8.0f); break;
            }
            L[i] = tone[0].lp (L[i] * car);
            R[i] = tone[1].lp (R[i] * car);
        }
    }
};

// =====================================================================================
struct OctaverFx final : Effect
{
    Biquad tr1, tr2; EnvFollower env; DcBlock dc; OnePole t1, t2;
    int state = -1, ff1 = 1, ff2 = 1; float lastTrack = -1;
    void prepare (double s, int b) override { Effect::prepare (s, b); env.setup (1.0f, 30.0f, sr); dc.setup (sr); lastTrack = -1; reset(); }
    void reset() override { tr1.reset(); tr2.reset(); env.reset(); dc.reset(); t1.reset(); t2.reset(); state = -1; ff1 = ff2 = 1; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float s1 = v[0] * 0.01f, s2 = v[1] * 0.01f, up = v[2] * 0.01f, dry = v[3] * 0.01f;
        if (v[5] != lastTrack)
        {
            const float f = 1200.0f * std::pow (0.15f, v[5] * 0.01f);
            tr1.set (Biquad::LP, f, 0.707f, 0, sr); tr2 = tr1; tr2.reset(); lastTrack = v[5];
        }
        t1.setHz (v[4], sr); t2.setHz (v[4], sr);
        for (int i = 0; i < n; ++i)
        {
            const float m = (L[i] + R[i]) * 0.5f;
            const float e = env.tick (m);
            const float xt = tr2.tick (tr1.tick (m, 0), 0);
            const float h = e * 0.08f + 1.0e-4f;
            if (state < 0 && xt > h) { state = 1; ff1 = -ff1; if (ff1 > 0) ff2 = -ff2; }
            else if (state > 0 && xt < -h) state = -1;
            float wet = t2.lp (t1.lp ((float (ff1) * s1 + float (ff2) * s2) * e * 1.1f));
            if (up > 0) wet += fastTanh (dc.tick (std::fabs (m)) * 3.0f) * 0.6f * up;
            if (e < 1.0e-3f) wet *= e * 1000.0f;
            L[i] = L[i] * dry + wet;
            R[i] = R[i] * dry + wet;
        }
    }
};

// =====================================================================================
struct PitchFx final : Effect
{
    GrainShifter sh[2][2]; float last[2] = { 0, 0 }; Smooth win;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& a : sh) for (auto& g : a) g.init (int (0.11 * sr));
        win.setTime (50.0f, sr); win.reset (0.04f * float (sr));
        reset();
    }
    void reset() override { for (auto& a : sh) for (auto& g : a) g.clear(); last[0] = last[1] = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float r1 = std::exp2 ((v[0] + v[1] * 0.01f) / 12.0f), r2 = std::exp2 (v[2] / 12.0f);
        const float lvl2 = v[3] * 0.01f, fb = v[5] * 0.01f;
        win.tgt = v[4] * 0.001f * float (sr);
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float w = win.next();
            for (int c = 0; c < 2; ++c)
            {
                const float in = ch[c][i] + fb * last[c];
                const float y1 = sh[0][c].tick (in, r1, w);
                const float y2 = sh[1][c].tick (in, r2, w);
                const float out = y1 + y2 * lvl2;
                last[c] = fastTanh (out);
                ch[c][i] = out;
            }
        }
    }
};

// =====================================================================================
struct DelayFx final : Effect
{
    DelayLine dl[2]; Smooth time; OnePole tone[2], hp[2]; float ph = 0, fph = 0, rph = 0; EnvFollower duckEnv; float duckG = 1;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& d : dl) d.init (int (3.1 * sr) + 256);
        time.setTime (120.0f, sr); time.reset (0.375f * float (sr));
        hp[0].setHz (90.0f, sr); hp[1].setHz (90.0f, sr);
        duckEnv.setup (2.0f, 180.0f, sr);
        reset();
    }
    void reset() override { for (auto& d : dl) d.clear(); tone[0].reset(); tone[1].reset(); hp[0].reset(); hp[1].reset(); rph = 0; duckEnv.reset(); duckG = 1; }
    inline float loopFx (float y, int c, int mode)
    {
        y = tone[c].lp (y);
        switch (mode)
        {
            case 1:  return fastTanh (y * 1.1f) * 0.909f;
            case 2:  return fastTanh (hp[c].hp (y) * 1.4f) * 0.714f;
            default: return fastTanh (y * 0.5f) * 2.0f;
        }
    }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        const int sync = (int) v[1], mode = (int) v[4];
        float ms = v[0];
        if (sync > 0) ms = kSyncBeats[sync - 1] * 60000.0f / float (ctx.bpm);
        ms = clampf (ms, 1.0f, 2000.0f);
        time.tgt = ms * 0.001f * float (sr);
        const float fb = v[2] * 0.01f, mod = v[5] * 0.01f, duck = v[6] * 0.01f, spread = 1.0f + 0.5f * v[7] * 0.01f;
        const float toneHz = mode == 1 ? v[3] * 0.6f : v[3];
        tone[0].setHz (toneHz, sr); tone[1].setHz (toneHz, sr);
        const float modMs = mode == 2 ? 2.5f : (mode == 1 ? 1.2f : 0.6f);
        const float modS = mod * modMs * 0.001f * float (sr);
        const float flS = mode == 2 ? mod * 0.00008f * float (sr) : 0.0f;
        const float pinc = 0.5f / float (sr), finc = 6.5f / float (sr);
        const float maxD = float (dl[0].size() - 8);
        const float kDuck = 1.0f - onePoleCoef (15.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            ph += pinc; if (ph >= 1) ph -= 1;
            fph += finc; if (fph >= 1) fph -= 1;
            const float T = time.next();
            const float d = clampf (T + modS * (fastSin2Pi (ph) + 1.0f) + flS * fastSin2Pi (fph), 2.0f, maxD);
            if (duck > 0.0f)
            {
                const float e = duckEnv.tick (maxAbs (L[i], R[i]));
                const float tg = 1.0f - duck * clampf ((gainToDb (e) + 45.0f) / 30.0f, 0.0f, 1.0f);   // echoes step back while you play
                duckG += (tg - duckG) * kDuck;
            }
            else duckG = 1.0f;
            if (mode == 3)
            {
                const float in = (L[i] + R[i]) * 0.5f;
                const float yl = dl[0].read (d), yr = dl[1].read (d);
                dl[0].push (in + fb * loopFx (yr, 0, 0));
                dl[1].push (fb * loopFx (yl, 1, 0));
                L[i] = yl * duckG; R[i] = yr * duckG;
            }
            else if (mode == 4)
            {
                const float W = std::max (T, 64.0f);
                rph += 2.0f / W; rph -= std::floor (rph);
                float p2 = rph + 0.5f; if (p2 >= 1) p2 -= 1;
                const float w1 = fastSin2Pi (rph * 0.5f), w2 = fastSin2Pi (p2 * 0.5f);
                float* ch[2] = { L, R };
                for (int c = 0; c < 2; ++c)
                {
                    const float y = dl[c].read (2.0f + rph * W) * w1 * w1 + dl[c].read (2.0f + p2 * W) * w2 * w2;
                    dl[c].push (ch[c][i] + fb * loopFx (y, c, 0));
                    ch[c][i] = y * duckG;
                }
            }
            else
            {
                float* ch[2] = { L, R };
                for (int c = 0; c < 2; ++c)
                {
                    const float y = dl[c].read (c == 1 ? std::min (d * spread, maxD) : d);
                    dl[c].push (ch[c][i] + fb * loopFx (y, c, mode));
                    ch[c][i] = y * duckG;
                }
            }
        }
        meter = 1.0f - duckG;
    }
};

// =====================================================================================
struct SchroederAP
{
    DelayLine dl; int d = 1; float g = 0.6f;
    void init (int samples) { dl.init (samples + 2); d = std::max (1, samples); }
    inline float tick (float x) { const float z = dl.at (d - 1); const float w = x + g * z; dl.push (w); return z - g * w; }
};

struct ReverbFx final : Effect
{
    static constexpr int N = 8;
    DelayLine line[N], pre[2]; SchroederAP diff[2][4]; OnePole damp[N]; Smooth len[N];
    GrainShifter shim; float lph[N] = {}; float shimSig = 0; OnePole lcut[2];
    float gains[N] = {}; float lastSize = -1, lastDecay = -1, lastDamp = -1;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (int i = 0; i < N; ++i) { line[i].init (int (0.2 * sr)); len[i].setTime (300.0f, sr); len[i].reset (0.05f * float (sr)); lph[i] = float (i) / N; }
        for (auto& p : pre) p.init (int (0.21 * sr));
        static const float dms[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k) diff[c][k].init (int (dms[k] * (c ? 1.07f : 1.0f) * 0.001f * float (sr)));
        shim.init (int (0.09 * sr));
        lastSize = lastDecay = lastDamp = -1;
        reset();
    }
    void reset() override
    {
        for (auto& l : line) l.clear();
        for (auto& p : pre) p.clear();
        for (auto& c : diff) for (auto& d : c) d.dl.clear();
        for (auto& d : damp) d.reset();
        shim.clear(); shimSig = 0; lcut[0].reset(); lcut[1].reset();
    }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
        const float size = v[0] * 0.01f, decay = v[1], dampAmt = v[2] * 0.01f, mod = v[4] * 0.01f, shimmer = v[5] * 0.01f;
        const float sf = 0.35f + size * 1.4f;
        if (size != lastSize || decay != lastDecay)
        {
            for (int i = 0; i < N; ++i)
            {
                const float ls = baseMs[i] * sf * 0.001f;
                len[i].tgt = ls * float (sr);
                gains[i] = std::pow (10.0f, -3.0f * ls / decay);
            }
            lastSize = size; lastDecay = decay;
        }
        if (dampAmt != lastDamp) { for (auto& d : damp) d.setHz (18000.0f * std::pow (0.06f, dampAmt), sr); lastDamp = dampAmt; }
        const float preS = std::max (1.0f, v[3] * 0.001f * float (sr));
        const float modS = mod * 10.0f * float (sr / 48000.0);
        const float win = 0.08f * float (sr);
        const bool useLc = v[6] > 20.5f; const float width = v[7] * 0.01f;
        if (useLc) { lcut[0].setHz (v[6], sr); lcut[1].a = lcut[0].a; }
        for (int i = 0; i < n; ++i)
        {
            pre[0].push (L[i]); pre[1].push (R[i]);
            float dL = pre[0].read (preS), dR = pre[1].read (preS);
            for (int k = 0; k < 4; ++k) { dL = diff[0][k].tick (dL); dR = diff[1][k].tick (dR); }
            float o[N]; float sum = 0;
            for (int j = 0; j < N; ++j)
            {
                lph[j] += (0.3f + 0.08f * float (j)) / float (sr); if (lph[j] >= 1) lph[j] -= 1;
                const float d = len[j].next() - modS * (1.0f + fastSin2Pi (lph[j])) - 2.0f;
                o[j] = damp[j].lp (line[j].read (std::max (d, 2.0f)));
                sum += o[j];
            }
            sum *= 2.0f / N;
            for (int j = 0; j < N; ++j)
            {
                const float in = ((j & 1) ? dR : dL) * 0.5f + shimSig;
                const float x = in + gains[j] * (o[j] - sum);
                line[j].push (std::fabs (x) > 1.5f ? fastTanh (x * 0.5f) * 2.0f : x);
            }
            float outL = (o[0] - o[1] + o[2] - o[3] + o[4] - o[5] + o[6] - o[7]) * 0.4f;
            float outR = (o[0] + o[1] - o[2] - o[3] + o[4] + o[5] - o[6] - o[7]) * 0.4f;
            if (shimmer > 0) shimSig = shim.tick ((outL + outR) * 0.5f, 2.0f, win) * shimmer * 0.35f;
            else shimSig = 0;
            if (useLc) { outL = lcut[0].hp (outL); outR = lcut[1].hp (outR); }
            const float mm = 0.5f * (outL + outR), ss = 0.5f * (outL - outR) * width;
            L[i] = mm + ss; R[i] = mm - ss;
        }
    }
};

// =====================================================================================
struct SpringFx final : Effect
{
    static constexpr int K = 16;
    AllPass1 disp[2][K]; DelayLine loop[2]; OnePole tone[2], drip[2]; DcBlock dc[2]; float fb[2] = { 0, 0 };
    void prepare (double s, int b) override { Effect::prepare (s, b); for (int c = 0; c < 2; ++c) { loop[c].init (int (0.1 * sr)); dc[c].setup (sr); drip[c].setHz (1500.0f, sr); } reset(); }
    void reset() override { for (int c = 0; c < 2; ++c) { for (auto& a : disp[c]) a.reset(); loop[c].clear(); tone[c].reset(); drip[c].reset(); dc[c].reset(); fb[c] = 0; } }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float decay = v[0], a = 0.35f + v[2] * 0.004f, dripA = v[4] * 0.01f;
        float lenS[2] = { v[3] * 0.001f * float (sr), v[3] * 1.11f * 0.001f * float (sr) };
        float g[2];
        for (int c = 0; c < 2; ++c) { g[c] = std::pow (10.0f, -3.0f * (lenS[c] / float (sr)) / decay); tone[c].setHz (v[1], sr); }
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                const float y = tone[c].lp (loop[c].read (lenS[c]));
                const float xin = lerp (x * 0.5f, drip[c].hp (x) * 1.5f, dripA);
                float s = xin + g[c] * fb[c];
                for (int k = 0; k < K; ++k) s = disp[c][k].tick (s, a);
                loop[c].push (dc[c].tick (s));
                fb[c] = fastTanh (y);
                ch[c][i] = y * 2.2f;
            }
    }
};

// =====================================================================================
struct CrusherFx final : Effect
{
    float ph = 0, thr = 1, held[2] = { 0, 0 }; OnePole tone[2]; Rng rng { 777u };
    void reset() override { ph = 0; thr = 1; held[0] = held[1] = 0; tone[0].reset(); tone[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float levels = std::exp2 (v[0] - 1.0f), invL = 1.0f / levels;
        const float step = std::min (1.0f, v[1] / float (sr));
        const float drive = dbToGain (v[2]), comp = 1.0f / std::sqrt (drive), jit = v[3] * 0.01f;
        const bool useTone = v[4] < 19600.0f;
        tone[0].setHz (v[4], sr); tone[1].setHz (v[4], sr);
        for (int i = 0; i < n; ++i)
        {
            ph += step;
            if (ph >= thr)
            {
                ph -= thr;
                thr = 1.0f + jit * rng.bi() * 0.9f;
                held[0] = clampf (std::round (L[i] * drive * levels) * invL, -1.0f, 1.0f);
                held[1] = clampf (std::round (R[i] * drive * levels) * invL, -1.0f, 1.0f);
            }
            L[i] = (useTone ? tone[0].lp (held[0]) : held[0]) * comp;
            R[i] = (useTone ? tone[1].lp (held[1]) : held[1]) * comp;
        }
    }
};

// =====================================================================================
struct GlitchFx final : Effect
{
    enum { Pass = 0, Repeat, Reverse, TapeStop };
    std::vector<float> buf[2]; int mask = 0; int64_t wpos = 0;
    int cur = Pass, nxt = Pass, prevType = Pass;
    int64_t stepStart = 0; int S = 1, C = 1, t = 0; float tapePos = 0;
    int64_t lastStepIdx = -1; int internalCtr = 0; Rng rng { 4242u };
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        int sz = 1; while (sz < int (4.5 * sr)) sz <<= 1;
        buf[0].assign ((size_t) sz, 0.0f); buf[1].assign ((size_t) sz, 0.0f); mask = sz - 1;
        reset();
    }
    void reset() override { std::fill (buf[0].begin(), buf[0].end(), 0.0f); std::fill (buf[1].begin(), buf[1].end(), 0.0f); wpos = 0; cur = nxt = prevType = Pass; lastStepIdx = -1; internalCtr = 0; t = 0; }
    inline float rd (int c, int64_t idx) const { return buf[c][(size_t) (idx & mask)]; }
    inline float rdf (int c, double idx) const { const double f = std::floor (idx); const int64_t i = (int64_t) f; const float fr = float (idx - f); return lerp (rd (c, i), rd (c, i + 1), fr); }
    int decide (const float* v)
    {
        if (rng.uni() >= v[1] * 0.01f) return Pass;
        const float w1 = v[2], w2 = v[3], w3 = v[4], tot = w1 + w2 + w3;
        if (tot <= 0.0f) return Repeat;
        const float r = rng.uni() * tot;
        return r < w1 ? Repeat : (r < w1 + w2 ? Reverse : TapeStop);
    }
    void boundary (const float* v)
    {
        prevType = cur;
        cur = nxt;
        nxt = decide (v);
        stepStart = wpos;
        t = 0; tapePos = 0;
        static const int divs[4] = { 1, 2, 4, 8 };
        C = std::max (32, S / divs[rng.next() % 4]);
    }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float beatsT[] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
        const float beats = beatsT[std::max (0, std::min (5, (int) v[0]))];
        const double bpm = std::max (20.0, ctx.bpm);
        S = std::min ((int) (beats * 60.0 / bpm * sr), (mask + 1) / 3);
        const float gate = v[5] * 0.01f;
        const int fadeLen = std::max (16, int (0.0025 * sr));
        const double ppqPerSample = bpm / (60.0 * sr);
        int act = 0;
        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) (wpos & mask)] = L[i];
            buf[1][(size_t) (wpos & mask)] = R[i];
            bool isBoundary = false;
            if (ctx.playing)
            {
                const int64_t idx = (int64_t) std::floor ((ctx.ppq + double (i) * ppqPerSample) / beats);
                if (idx != lastStepIdx) { lastStepIdx = idx; isBoundary = true; }
            }
            else if (++internalCtr >= S) { internalCtr = 0; isBoundary = true; }
            if (isBoundary) boundary (v);

            float g = 1.0f;
            const bool fadeIn = cur != Pass || prevType != Pass;
            const bool fadeOut = cur != Pass || nxt != Pass;
            if (fadeIn && t < fadeLen) g = std::min (g, float (t) / float (fadeLen));
            if (fadeOut && S - t < fadeLen) g = std::min (g, float (std::max (0, S - t)) / float (fadeLen));
            if (cur != Pass && gate < 0.999f)
            {
                const int gEnd = int (gate * float (S));
                g = std::min (g, clampf (float (gEnd - t) / float (fadeLen), 0.0f, 1.0f));
            }
            float ol = L[i], or_ = R[i];
            switch (cur)
            {
                case Repeat:
                {
                    const int tc = t % C;
                    const int64_t idx = stepStart - C + tc;
                    const float e = std::min (1.0f, std::min (float (tc), float (C - tc)) / 48.0f);
                    ol = rd (0, idx) * e; or_ = rd (1, idx) * e; act = 1;
                    break;
                }
                case Reverse:
                {
                    const int64_t idx = stepStart - 1 - (t % std::max (1, S));
                    ol = rd (0, idx); or_ = rd (1, idx); act = 1;
                    break;
                }
                case TapeStop:
                {
                    const float speed = std::max (0.0f, 1.0f - float (t) / float (std::max (1, S)));
                    tapePos += speed * speed;
                    const double idx = double (stepStart) + tapePos - 1.0;
                    ol = rdf (0, idx); or_ = rdf (1, idx); act = 1;
                    break;
                }
                default: break;
            }
            L[i] = ol * g; R[i] = or_ * g;
            ++t; ++wpos;
        }
        meter = act ? 1.0f : 0.0f;
    }
};

// =====================================================================================
struct GranularFx final : Effect
{
    static constexpr int MaxG = 24;
    struct Grain { bool on = false; float back = 0, rate = 1; int len = 1, t = 0; float pl = 1, pr = 1; };
    std::vector<float> buf[2]; int mask = 0; int w = 0; Grain g[MaxG]; float hann[1025];
    int countdown = 0; float last[2] = { 0, 0 }; Rng rng { 9001u };
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        int sz = 1; while (sz < int (3.0 * sr)) sz <<= 1;
        buf[0].assign ((size_t) sz, 0.0f); buf[1].assign ((size_t) sz, 0.0f); mask = sz - 1;
        for (int i = 0; i <= 1024; ++i) hann[i] = 0.5f - 0.5f * std::cos (k2Pi * float (i) / 1024.0f);
        reset();
    }
    void reset() override { std::fill (buf[0].begin(), buf[0].end(), 0.0f); std::fill (buf[1].begin(), buf[1].end(), 0.0f); w = 0; for (auto& x : g) x.on = false; countdown = 0; last[0] = last[1] = 0; }
    float pickRate (int mode)
    {
        switch (mode)
        {
            case 1: return 2.0f;
            case 2: return 0.5f;
            case 3: { const float r[3] = { 0.5f, 1.0f, 2.0f }; return r[rng.next() % 3]; }
            case 4: { const float r[4] = { 1.0f, 1.5f, 2.0f, 0.75f }; return r[rng.next() % 4]; }
            case 5: return 1.0f + rng.bi() * 0.015f;
            default: return 1.0f;
        }
    }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float density = v[0], sizeS = v[1] * 0.001f * float (sr), spray = v[2] * 0.01f, revP = v[4] * 0.01f, fb = v[5] * 0.01f;
        const int mode = (int) v[3];
        const float norm = 1.2f / std::sqrt (std::max (1.0f, density * v[1] * 0.001f));
        const float maxBack = float (mask) - 8.0f;
        int active = 0;
        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) w] = L[i] + fb * last[0];
            buf[1][(size_t) w] = R[i] + fb * last[1];
            if (--countdown <= 0)
            {
                countdown = std::max (1, int (float (sr) / density * (0.5f + rng.uni())));
                for (auto& gr : g)
                    if (! gr.on)
                    {
                        gr.on = true; gr.t = 0;
                        gr.len = std::max (64, int (sizeS * (0.75f + 0.5f * rng.uni())));
                        float rate = pickRate (mode);
                        if (rng.uni() < revP) rate = -rate;
                        gr.rate = rate;
                        float back = 64.0f + rng.uni() * spray * 1.5f * float (sr);
                        if (rate > 1.0f) back += (rate - 1.0f) * float (gr.len);
                        back = std::min (back, maxBack - float (gr.len) * (1.0f + std::fabs (rate)));
                        gr.back = std::max (back, 2.0f);
                        const float pan = 0.5f + 0.45f * rng.bi();
                        gr.pl = std::sqrt (1.0f - pan) * 1.414f; gr.pr = std::sqrt (pan) * 1.414f;
                        break;
                    }
            }
            float ol = 0, orr = 0;
            for (auto& gr : g)
            {
                if (! gr.on) continue;
                const float pos = float (w) - gr.back;
                const float fl = std::floor (pos); const int ii = (int) fl; const float fr = pos - fl;
                const float wv = hann[(gr.t * 1024) / gr.len];
                const float a0 = buf[0][(size_t) (ii & mask)], a1 = buf[0][(size_t) ((ii + 1) & mask)];
                const float b0 = buf[1][(size_t) (ii & mask)], b1 = buf[1][(size_t) ((ii + 1) & mask)];
                ol += (a0 + fr * (a1 - a0)) * wv * gr.pl;
                orr += (b0 + fr * (b1 - b0)) * wv * gr.pr;
                gr.back += 1.0f - gr.rate;
                if (++gr.t >= gr.len || gr.back < 1.0f || gr.back > maxBack) gr.on = false;
                ++active;
            }
            ol *= norm; orr *= norm;
            last[0] = fastTanh (ol); last[1] = fastTanh (orr);
            L[i] = ol; R[i] = orr;
            w = (w + 1) & mask;
        }
        meter = clampf (float (active) / float (std::max (1, n) * 8), 0, 1);
    }
};

// =====================================================================================
struct ImagerFx final : Effect
{
    DelayLine haas; OnePole side; Smooth hd;
    void prepare (double s, int b) override { Effect::prepare (s, b); haas.init (int (0.035 * sr)); hd.setTime (30.0f, sr); hd.reset (0); reset(); }
    void reset() override { haas.clear(); side.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float width = v[0] * 0.01f;
        const bool mono = v[1] > 20.5f;
        side.setHz (v[1], sr);
        hd.tgt = v[2] * 0.001f * float (sr);
        const float p = clampf (v[3] * 0.01f, -1, 1);
        const float ang = (p + 1.0f) * kPi * 0.25f;
        const float gl = std::cos (ang) * 1.41421356f, gr = std::sin (ang) * 1.41421356f;
        for (int i = 0; i < n; ++i)
        {
            const float m = (L[i] + R[i]) * 0.5f;
            float s = (L[i] - R[i]) * 0.5f * width;
            if (mono) s -= side.lp (s);
            float l = m + s, r = m - s;
            haas.push (r);
            const float d = hd.next();
            if (d > 0.5f) r = haas.read (d);
            L[i] = l * std::min (gl, 1.0f);
            R[i] = r * std::min (gr, 1.0f);
        }
    }
};
// =====================================================================================
struct CabIrFx final : Effect
{
    PartitionedConvolver conv; Biquad lc, hc; bool loaded = false, useLc = false, useHc = false;
    float lastLc = -1, lastHc = -1;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        loaded = false; lastLc = lastHc = -1;
        auto ir = std::dynamic_pointer_cast<const IrResource> (resource);
        if (ir == nullptr || ir->ch[0].empty()) return;
        const double srcRate = ir->sampleRate > 0 ? ir->sampleRate : sr;
        const double ratio = sr / srcRate;
        const int inLen = (int) std::min<double> ((double) ir->ch[0].size(), srcRate * 2.0);
        const int outLen = std::max (1, (int) std::floor ((double) inLen * ratio));
        std::vector<float> h[2];
        for (int c = 0; c < 2; ++c)
        {
            const auto& src = (c == 1 && ! ir->ch[1].empty()) ? ir->ch[1] : ir->ch[0];
            h[c].resize ((size_t) outLen);
            for (int i = 0; i < outLen; ++i)
            {
                const double pos = (double) i / ratio;
                const int i0 = (int) pos; const float fr = (float) (pos - i0);
                const float a = i0 < inLen ? src[(size_t) i0] : 0.0f, bb = i0 + 1 < inLen ? src[(size_t) i0 + 1] : 0.0f;
                h[c][(size_t) i] = a + fr * (bb - a);
            }
        }
        double e0 = 0, e1 = 0;
        for (int i = 0; i < outLen; ++i) { e0 += (double) h[0][(size_t) i] * h[0][(size_t) i]; e1 += (double) h[1][(size_t) i] * h[1][(size_t) i]; }
        const double e = std::max (e0, e1);
        if (e <= 1.0e-12) return;
        const float g = (float) (0.5 / std::sqrt (e));
        for (auto& ch : h) for (auto& x : ch) x *= g;
        conv.setup (h[0].data(), h[1].data(), outLen);
        loaded = true;
        reset();
    }
    void reset() override { if (loaded) conv.reset(); lc.reset(); hc.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        if (! loaded) { meter = 0; return; }
        if (v[0] != lastLc) { lastLc = v[0]; useLc = v[0] > 20.5f; if (useLc) lc.set (Biquad::HP, v[0], 0.707f, 0, sr); }
        if (v[1] != lastHc) { lastHc = v[1]; useHc = v[1] < 19600.0f; if (useHc) hc.set (Biquad::LP, v[1], 0.707f, 0, sr); }
        conv.setActiveLength ((int) (v[2] * 0.001f * float (sr)));
        conv.process (L, R, n);
        const float g = dbToGain (v[3]);
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                if (useLc) x = lc.tick (x, c);
                if (useHc) x = hc.tick (x, c);
                ch[c][i] = x * g;
            }
        meter = 1.0f;
    }
};
// =====================================================================================
// v1.2 effects
// =====================================================================================
inline uint32_t hash32 (uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
inline float hash01 (int64_t i, uint32_t salt) { return float (hash32 ((uint32_t) i * 2654435761u ^ salt) >> 8) * (1.0f / 16777216.0f); }
inline int wrap16 (int64_t i) { return (int) (((i % 16) + 16) % 16); }
static const float kDivBeats[6] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };

// Tempo grid: step index for each sample (host transport when playing, free-running otherwise)
struct StepClock
{
    double freePos = 0.0;
    inline int64_t step (const Context& ctx, int i, double beatsPerStep, double sr)
    {
        const double ppqPerSample = ctx.bpm / (60.0 * sr);
        if (ctx.playing) return (int64_t) std::floor ((ctx.ppq + double (i) * ppqPerSample) / beatsPerStep);
        freePos += ppqPerSample / beatsPerStep;
        return (int64_t) std::floor (freePos);
    }
};

// =====================================================================================
struct WahFx final : Effect
{
    Svf f[2]; EnvFollower env; float pos = 0.5f, ph = 0.0f;
    void reset() override { f[0].reset(); f[1].reset(); env.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float fLo[4] = { 420.0f, 380.0f, 300.0f, 160.0f }, fHi[4] = { 2200.0f, 2500.0f, 3300.0f, 1200.0f };
        static const float qB[4] = { 6.5f, 4.5f, 3.0f, 6.0f }, lpMix[4] = { 0.25f, 0.15f, 0.1f, 0.35f };
        static const float syncBeats[3] = { 1.0f, 0.5f, 0.25f };
        const float depth = v[0] * 0.01f, reso = v[3] * 0.01f, speed = v[4], sens = v[5] * 0.01f;
        const int mode = (int) v[1], model = std::max (0, std::min (3, (int) v[2]));
        const float q = qB[model] * (0.5f + reso * 1.5f), kq = 1.0f / q;
        env.setup (3.0f, clampf (400.0f / speed, 20.0f, 1000.0f), sr);
        const float sensG = dbToGain (sens * 36.0f - 6.0f);
        float rate = speed;
        if (mode >= 3) { const float b = syncBeats[mode - 3]; rate = float (ctx.bpm / 60.0) / b; if (ctx.playing) { const double p = ctx.ppq / b; ph = float (p - std::floor (p)); } }
        const float inc = rate / float (sr), k = 1.0f - onePoleCoef (6.0f, sr);
        const float ratio = fHi[model] / fLo[model];
        for (int i = 0; i < n; ++i)
        {
            float tgt = depth;
            if (mode == 1) tgt = depth * (1.0f - std::exp (-env.tick ((L[i] + R[i]) * 0.5f) * sensG * 1.5f));
            else if (mode >= 2) { ph += inc; if (ph >= 1) ph -= 1; tgt = depth * (0.5f + 0.5f * fastSin2Pi (ph)); }
            pos += (tgt - pos) * k;
            const float fc = fLo[model] * std::pow (ratio, pos);
            const float g = fastTan (kPi * clampf (fc, 20.0f, float (sr) * 0.45f) / float (sr));
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                f[c].setG (g, q);
                f[c].tick (ch[c][i]);
                ch[c][i] = f[c].bp * kq * 3.6f + f[c].lp * lpMix[model] * 1.5f;
            }
        }
        meter = pos;
    }
};

// =====================================================================================
struct FreezeFx final : Effect
{
    struct Grain { bool on = false; int buf = 0; float pos = 0, rate = 1; int len = 1, t = 0; float gl = 1, gr = 1, amp = 1; };
    std::vector<float> rec[2]; int recMask = 0; int64_t w = 0;
    std::vector<float> frz[2][2]; int frzLen[2] = { 0, 0 }; int cur = 0; bool have = false;
    Grain g[12]; float hann[1025] = {}; int spawnCtr = 0, pending = -1, refr = 0, lastMode = -1, maxC = 1;
    float pad = 0.0f; EnvFollower fast, slow; OnePole tone[2]; Rng rng { 31337u };
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        int sz = 1; while (sz < int (1.0 * sr)) sz <<= 1;
        rec[0].assign ((size_t) sz, 0.0f); rec[1].assign ((size_t) sz, 0.0f); recMask = sz - 1;
        maxC = int (0.62 * sr);
        for (auto& bb : frz) for (auto& c : bb) c.assign ((size_t) maxC + 4, 0.0f);
        for (int i = 0; i <= 1024; ++i) hann[i] = 0.5f - 0.5f * std::cos (k2Pi * float (i) / 1024.0f);
        fast.setup (0.5f, 40.0f, sr); slow.setup (40.0f, 250.0f, sr);
        reset();
    }
    void reset() override { for (auto& gg : g) gg.on = false; have = false; pad = 0; pending = -1; refr = 0; lastMode = -1; fast.reset(); slow.reset(); tone[0].reset(); tone[1].reset(); }
    void capture (int C)
    {
        const int b = 1 - cur;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < C; ++i) frz[b][c][(size_t) i] = rec[c][(size_t) ((w - C + i) & recMask)];
        frzLen[b] = C; cur = b; have = true;
    }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const int mode = (int) v[0];
        const float blend = v[1] * 0.01f, shimmer = v[4] * 0.01f;
        const int C = std::max (2048, std::min (maxC, int (v[2] * 0.001f * float (sr))));
        const float kFade = 1.0f - std::exp (-1.0f / (v[3] * float (sr)));
        tone[0].setHz (v[5], sr); tone[1].setHz (v[5], sr);
        const int glen = std::max (512, C / 2), spawnEvery = std::max (64, glen / 3);
        if (mode == 1 && lastMode != 1 && w > C) capture (C);
        lastMode = mode;
        const float target = (mode == 2 || ! have) ? 0.0f : 1.0f;
        for (int i = 0; i < n; ++i)
        {
            rec[0][(size_t) (w & recMask)] = L[i]; rec[1][(size_t) (w & recMask)] = R[i]; ++w;
            if (mode == 0)
            {
                const float x = std::max (std::fabs (L[i]), std::fabs (R[i])) + 1.0e-7f;
                const float fd = gainToDb (fast.tick (x)), sd = gainToDb (slow.tick (x));
                if (refr > 0) --refr;
                if (fd > -50.0f && fd - sd > 8.0f && refr == 0 && pending < 0) { pending = C + int (0.05 * sr); refr = std::max (int (0.3 * sr), C); }
                if (pending > 0 && --pending == 0) { capture (C); pending = -1; }
            }
            pad += (target - pad) * kFade;
            float ol = 0, orr = 0;
            if (have && pad > 1.0e-4f)
            {
                if (--spawnCtr <= 0)
                {
                    spawnCtr = spawnEvery;
                    for (auto& gg : g)
                        if (! gg.on)
                        {
                            gg.on = true; gg.t = 0; gg.buf = cur;
                            gg.rate = (shimmer > 0 && rng.uni() < shimmer * 0.6f) ? 2.0f : 1.0f;
                            gg.len = glen; gg.amp = gg.rate > 1.0f ? 0.6f : 1.0f;
                            const float room = float (frzLen[cur]) - float (glen) * gg.rate - 2.0f;
                            gg.pos = room > 0 ? rng.uni() * room : 0.0f;
                            const float pan = 0.5f + 0.4f * rng.bi();
                            gg.gl = std::sqrt (1.0f - pan) * 1.414f; gg.gr = std::sqrt (pan) * 1.414f;
                            break;
                        }
                }
                for (auto& gg : g)
                {
                    if (! gg.on) continue;
                    const int i0 = (int) gg.pos; const float fr = gg.pos - float (i0);
                    const auto& a = frz[gg.buf][0]; const auto& b = frz[gg.buf][1];
                    const int lim = frzLen[gg.buf] - 1;
                    const int i1 = std::min (i0 + 1, lim), ii = std::min (i0, lim);
                    const float wv = hann[(gg.t * 1024) / gg.len] * gg.amp;
                    ol += (a[(size_t) ii] + fr * (a[(size_t) i1] - a[(size_t) ii])) * wv * gg.gl;
                    orr += (b[(size_t) ii] + fr * (b[(size_t) i1] - b[(size_t) ii])) * wv * gg.gr;
                    gg.pos += gg.rate;
                    if (++gg.t >= gg.len) gg.on = false;
                }
            }
            const float pg = pad * blend * 0.8f;
            L[i] += tone[0].lp (ol) * pg;
            R[i] += tone[1].lp (orr) * pg;
        }
        meter = pad;
    }
};

// =====================================================================================
struct StepFilterFx final : Effect
{
    Svf f[2]; float cut = 0.5f, gate = 1.0f; StepClock clk;
    void reset() override { f[0].reset(); f[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float pats[4][16] = {
            { 0, .07f, .13f, .2f, .27f, .33f, .4f, .47f, .53f, .6f, .67f, .73f, .8f, .87f, .93f, 1 },
            { 1, .93f, .87f, .8f, .73f, .67f, .6f, .53f, .47f, .4f, .33f, .27f, .2f, .13f, .07f, 0 },
            { 0, .6f, .2f, .8f, .4f, 1, .3f, .7f, .1f, .9f, .5f, .6f, .2f, .8f, .3f, 1 },
            { 0, 1, .5f, .75f, 0, .9f, .25f, .6f, 0, 1, .4f, .8f, .15f, .9f, .5f, .7f } };
        static const float gates[3][16] = {
            { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0 },
            { .25f, 1, .25f, 1, .25f, 1, .25f, 1, .25f, 1, .25f, 1, .25f, 1, 1, 1 },
            { 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0 } };
        const double beats = kDivBeats[std::max (0, std::min (5, (int) v[0]))];
        const int pat = (int) v[1];
        const float base = v[2], range = v[3], q = 0.7f + v[4] * 0.12f, glide = v[5] * 0.01f;
        const float stepSec = float (beats * 60.0 / std::max (20.0, ctx.bpm));
        const float kc = 1.0f - std::exp (-1.0f / ((0.001f + glide * 0.9f * stepSec) * float (sr)));
        const float kg = 1.0f - std::exp (-1.0f / ((0.002f + glide * 0.3f * stepSec) * float (sr)));
        const float nyq = float (sr) * 0.45f;
        const float comp = 1.0f / (1.0f + q * 0.08f);
        for (int i = 0; i < n; ++i)
        {
            const int64_t st = clk.step (ctx, i, beats, sr);
            const int s16 = wrap16 (st);
            float val = 1.0f, gt = 1.0f;
            if (pat < 4) val = pats[pat][s16];
            else if (pat == 4) val = hash01 (st, 0xA5u);
            else gt = gates[std::min (2, pat - 5)][s16];
            cut += (val - cut) * kc;
            gate += (gt - gate) * kg;
            const float g = fastTan (kPi * clampf (base * std::exp2 (range * cut), 20.0f, nyq) / float (sr));
            for (int c = 0; c < 2; ++c)
            {
                float* ch = c ? R : L;
                f[c].setG (g, q); f[c].tick (ch[i]);
                ch[i] = f[c].lp * gate * comp;
            }
        }
        meter = cut;
    }
};

// =====================================================================================
struct ArpFx final : Effect
{
    GrainShifter sh[2]; float semi = 0.0f, last[2] = { 0, 0 }; Smooth win; StepClock clk;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& x : sh) x.init (int (0.09 * sr));
        win.setTime (40.0f, sr); win.reset (0.035f * float (sr));
        reset();
    }
    void reset() override { for (auto& x : sh) x.clear(); last[0] = last[1] = 0; }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float pats[5][8] = {
            { 0, 12, 24, 12, 0, 12, 24, 12 }, { 0, 4, 7, 12, 16, 12, 7, 4 }, { 0, 3, 7, 12, 15, 12, 7, 3 },
            { 0, 7, 12, 19, 24, 19, 12, 7 }, { 24, 19, 16, 12, 7, 4, 0, -5 } };
        static const float penta[9] = { 0, 2, 4, 7, 9, 12, 14, 16, 19 };
        const double beats = kDivBeats[std::max (0, std::min (5, (int) v[0]))];
        const int pat = (int) v[1];
        const float oct = (v[2] - 1.0f) * 12.0f;
        const float stepSec = float (beats * 60.0 / std::max (20.0, ctx.bpm));
        const float kg = 1.0f - std::exp (-1.0f / ((0.001f + v[3] * 0.01f * 0.8f * stepSec) * float (sr)));
        win.tgt = v[4] * 0.001f * float (sr);
        const float fb = v[5] * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            const int64_t st = clk.step (ctx, i, beats, sr);
            const int s8 = (int) (((st % 8) + 8) % 8);
            const float tgt = oct + (pat < 5 ? pats[pat][s8] : penta[(int) (hash01 (st, 0x5Eu) * 8.999f)]);
            semi += (tgt - semi) * kg;
            const float ratio = std::exp2 (semi / 12.0f), wn = win.next();
            for (int c = 0; c < 2; ++c)
            {
                float* ch = c ? R : L;
                const float y = sh[c].tick (ch[i] + fb * last[c], ratio, wn);
                last[c] = fastTanh (y);
                ch[i] = y;
            }
        }
    }
};

// =====================================================================================
struct StringsFx final : Effect
{
    static constexpr int NS = 6;
    DelayLine dl[NS]; float lp[NS] = {}, len[NS] = {}, gfb[NS] = {}, pan[NS][2] = {};
    OnePole exHP; DcBlock dc[2]; float lastSig[5] = { -1, -1, -1, -1, -1 };
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& d : dl) d.init (int (sr / 25.0) + 8);
        exHP.setHz (150.0f, sr); dc[0].setup (sr); dc[1].setup (sr);
        for (auto& x : lastSig) x = -1;
        reset();
    }
    void reset() override { for (auto& d : dl) d.clear(); for (auto& x : lp) x = 0; exHP.reset(); dc[0].reset(); dc[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float chords[7][NS] = {
            { 0, 12, 24, 0, 12, 24 }, { 0, 7, 12, 16, 19, 24 }, { 0, 7, 12, 15, 19, 24 }, { 0, 5, 7, 12, 17, 19 },
            { 0, 7, 12, 19, 24, 31 }, { 0, 4, 7, 11, 12, 16 }, { 0, 7, 12, 12, 19, 24 } };
        if (v[0] != lastSig[0] || v[1] != lastSig[1] || v[2] != lastSig[2] || v[4] != lastSig[3] || v[5] != lastSig[4])
        {
            const int chord = std::max (0, std::min (6, (int) v[1]));
            const float root = 36.0f + v[0] + 12.0f * v[5];
            const float spread = v[4] * 0.01f;
            for (int s = 0; s < NS; ++s)
            {
                const float cents = (s & 1 ? 1.0f : -1.0f) * spread * 6.0f * float (1 + s % 3) / 3.0f;
                const float midi = root + chords[chord][s] + cents * 0.01f;
                const float f = 440.0f * std::exp2 ((midi - 69.0f) / 12.0f);
                len[s] = std::max (2.0f, float (sr) / f - 1.0f);
                gfb[s] = std::pow (10.0f, -3.0f * (len[s] / float (sr)) / v[2]);
                const float p = 0.5f + (s & 1 ? 0.5f : -0.5f) * spread * (0.4f + 0.2f * float (s % 3));
                pan[s][0] = std::sqrt (1.0f - p); pan[s][1] = std::sqrt (p);
            }
            lastSig[0] = v[0]; lastSig[1] = v[1]; lastSig[2] = v[2]; lastSig[3] = v[4]; lastSig[4] = v[5];
        }
        const float a = std::exp (-k2Pi * 1500.0f * std::pow (10.0f, v[3] * 0.01f) / float (sr));
        const float outG = 0.6f;
        for (int i = 0; i < n; ++i)
        {
            const float ex = exHP.hp ((L[i] + R[i]) * 0.5f) * 0.12f;
            float ol = 0, orr = 0;
            for (int s = 0; s < NS; ++s)
            {
                const float y = dl[s].read (len[s]);
                lp[s] = y + a * (lp[s] - y);
                dl[s].push (ex + fastTanh (lp[s] * gfb[s]));
                ol += y * pan[s][0]; orr += y * pan[s][1];
            }
            L[i] = dc[0].tick (ol) * outG;
            R[i] = dc[1].tick (orr) * outG;
        }
    }
};

// =====================================================================================
struct SpiralFx final : Effect
{
    DelayLine dl[2]; GrainShifter sh[2]; OnePole tone[2], hp[2]; Smooth time;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& d : dl) d.init (int (1.6 * sr) + 64);
        for (auto& x : sh) x.init (int (0.06 * sr));
        hp[0].setHz (90.0f, sr); hp[1].setHz (90.0f, sr);
        time.setTime (150.0f, sr); time.reset (0.32f * float (sr));
        reset();
    }
    void reset() override { for (auto& d : dl) d.clear(); for (auto& x : sh) x.clear(); for (int c = 0; c < 2; ++c) { tone[c].reset(); hp[c].reset(); } }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float syncBeats[5] = { 2.0f, 1.0f, 0.75f, 0.5f, 0.25f };
        static const float rl[6] = { 0.5f, 0.6674f, 1.0087f, 1.4983f, 2.0f, 2.0f };
        static const float rr[6] = { 0.5f, 0.6674f, 0.9913f, 1.4983f, 2.0f, 0.5f };
        const int sync = (int) v[1], iv = std::max (0, std::min (5, (int) v[2]));
        float ms = v[0];
        if (sync > 0) ms = syncBeats[sync - 1] * 60000.0f / float (ctx.bpm);
        time.tgt = clampf (ms, 20.0f, 1500.0f) * 0.001f * float (sr);
        const float fb = v[3] * 0.01f, spin = v[5] * 0.01f;
        tone[0].setHz (v[4], sr); tone[1].setHz (v[4], sr);
        const float win = 0.05f * float (sr);
        for (int i = 0; i < n; ++i)
        {
            const float d = time.next();
            const float yl = dl[0].read (d), yr = dl[1].read (d);
            const float fl = lerp (yl, yr, spin), fr = lerp (yr, yl, spin);
            const float sl = sh[0].tick (hp[0].hp (tone[0].lp (fl)), rl[iv], win);
            const float sr2 = sh[1].tick (hp[1].hp (tone[1].lp (fr)), rr[iv], win);
            dl[0].push (L[i] + fb * fastTanh (sl));
            dl[1].push (R[i] + fb * fastTanh (sr2));
            L[i] = yl; R[i] = yr;
        }
    }
};

// =====================================================================================
struct GhostVerbFx final : Effect
{
    ReverbFx verb; EnvFollower env; float wetG = 0; std::vector<float> gains;
    void prepare (double s, int b) override { Effect::prepare (s, b); verb.prepare (s, b); gains.assign ((size_t) b + 1, 0.0f); reset(); }
    void reset() override { verb.reset(); env.reset(); wetG = 0; }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        env.setup (3.0f, v[3], sr);
        const float duck = v[2] * 0.01f;
        const float kUp = 1.0f - std::exp (-1.0f / ((0.02f + v[4] * 0.01f * 2.5f) * float (sr)));
        const float kDown = 1.0f - std::exp (-1.0f / (0.03f * float (sr)));
        float m = 0;
        for (int i = 0; i < n; ++i)
        {
            const float e = env.tick (std::max (std::fabs (L[i]), std::fabs (R[i]))) + 1.0e-7f;
            const float tgt = 1.0f - duck * clampf ((gainToDb (e) + 50.0f) / 35.0f, 0.0f, 1.0f);
            wetG += (tgt - wetG) * (tgt > wetG ? kUp : kDown);
            gains[(size_t) i] = wetG; m = wetG;
        }
        const float rv[8] = { v[0], v[1], 45.0f, 15.0f, 50.0f, v[5], 20.0f, 100.0f };
        verb.process (L, R, n, rv, ctx);
        for (int i = 0; i < n; ++i) { L[i] *= gains[(size_t) i] * 2.2f; R[i] *= gains[(size_t) i] * 2.2f; }
        meter = m;
    }
};

// =====================================================================================
struct FeedbackFx final : Effect
{
    GrainShifter sh; EnvFollower env, fast, slow; OnePole t1, t2; float bloom = 0, ph = 0; int refr = 0;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        sh.init (int (0.06 * sr));
        env.setup (8.0f, 150.0f, sr); fast.setup (0.5f, 30.0f, sr); slow.setup (30.0f, 300.0f, sr);
        reset();
    }
    void reset() override { sh.clear(); env.reset(); fast.reset(); slow.reset(); t1.reset(); t2.reset(); bloom = 0; refr = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float harm[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
        const float h = harm[std::max (0, std::min (3, (int) v[0]))];
        const float kUp = 1.0f / (v[1] * float (sr)), kDown = 1.0f - std::exp (-1.0f / (0.08f * float (sr)));
        const float amt = v[2] * 0.01f, thr = v[3], wob = v[5] * 0.01f;
        t1.setHz (v[4], sr); t2.setHz (v[4], sr);
        const float win = 0.04f * float (sr), inc = 5.5f / float (sr);
        const int refrS = int (0.1 * sr);
        for (int i = 0; i < n; ++i)
        {
            const float m = (L[i] + R[i]) * 0.5f;
            const float e = env.tick (m);
            const float fd = gainToDb (fast.tick (m) + 1.0e-7f), sd = gainToDb (slow.tick (m) + 1.0e-7f);
            if (refr > 0) --refr;
            if (fd - sd > 9.0f && refr == 0) { bloom *= 0.15f; refr = refrS; }
            if (gainToDb (e + 1.0e-7f) > thr) bloom = std::min (1.0f, bloom + kUp);
            else bloom += (0.0f - bloom) * kDown;
            ph += inc; if (ph >= 1) ph -= 1;
            const float ratio = h * std::exp2 (wob * 0.25f * fastSin2Pi (ph) / 12.0f);
            const float c = m * std::min (40.0f, 0.25f / (e + 1.0e-5f));
            float s = sh.tick (c, ratio, win);
            s = fastTanh (t2.lp (t1.lp (s)) * 1.5f) * 0.6f;
            const float fbs = s * bloom * bloom * amt;
            L[i] += fbs; R[i] += fbs;
        }
        meter = bloom;
    }
};

// =====================================================================================
// v1.3 effects
// =====================================================================================
struct TapEnsembleFx final : Effect
{
    DelayLine dl; float ph[8] = {}; OnePole tone[2]; Smooth start, spacing;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        dl.init (int (0.16 * sr));
        start.setTime (60.0f, sr); start.reset (15.0f); spacing.setTime (60.0f, sr); spacing.reset (5.0f);
        for (int k = 0; k < 8; ++k) ph[k] = float (k) * 0.137f;
        reset();
    }
    void reset() override { dl.clear(); tone[0].reset(); tone[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const int taps = std::max (2, std::min (8, (int) v[0]));
        start.tgt = v[1]; spacing.tgt = v[2];
        const float spread = v[3] * 0.01f, modMs = v[4] * 0.01f * 1.5f;
        tone[0].setHz (v[5], sr); tone[1].setHz (v[5], sr);
        float gl[8], gr[8];
        for (int k = 0; k < taps; ++k)
        {
            const float p = (-1.0f + 2.0f * float (k) / float (taps - 1)) * spread;   // -1 (left) .. +1 (right)
            const float a = (p + 1.0f) * kPi * 0.25f;
            gl[k] = std::cos (a); gr[k] = std::sin (a);
        }
        const float norm = 2.4f / std::sqrt (float (taps));
        const float msToS = float (sr) * 0.001f;
        for (int i = 0; i < n; ++i)
        {
            dl.push ((L[i] + R[i]) * 0.5f);
            const float st = start.next(), sp = spacing.next();
            float ol = 0, orr = 0;
            for (int k = 0; k < taps; ++k)
            {
                float d = st + sp * float (k);
                if (modMs > 0.0f)
                {
                    ph[k] += (0.21f + 0.13f * float (k)) / float (sr); if (ph[k] >= 1) ph[k] -= 1;
                    d += modMs * fastSin2Pi (ph[k]);
                }
                const float y = dl.read (std::max (1.0f, d * msToS));
                ol += y * gl[k]; orr += y * gr[k];
            }
            L[i] = tone[0].lp (ol * norm);
            R[i] = tone[1].lp (orr * norm);
        }
    }
};

// =====================================================================================
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct SynthFx final : Effect
{
    Biquad hp, lp1, lp2; EnvFollower env, envX; Svf filt;
    double n = 0, lastCross = -1; bool armed = false; float prev = 0;
    float fEst = 110.0f, cand = 0.0f, logF = std::log2 (110.0f), ph = 0, amp = 0;
    float lastTrackHz = -1;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        hp.set (Biquad::HP, 45.0f, 0.707f, 0, sr);
        envX.setup (1.0f, 30.0f, sr);
        lastTrackHz = -1;
        reset();
    }
    void reset() override { hp.reset(); lp1.reset(); lp2.reset(); env.reset(); envX.reset(); filt.reset(); armed = false; lastCross = -1; n = 0; amp = 0; }
    void process (float* L, float* R, int nS, const float* v, const Context&) override
    {
        static const float octs[4] = { -2.0f, -1.0f, 0.0f, 1.0f };
        const int wave = std::max (0, std::min (3, (int) v[0]));
        const float oct = octs[std::max (0, std::min (3, (int) v[1]))];
        const float cutoff = v[2], envAmt = v[3] * 0.01f;
        const float kGlide = 1.0f - std::exp (-1.0f / ((0.001f + v[4] * 0.001f) * float (sr)));
        env.setup (v[5], 80.0f, sr);
        const float trackHz = clampf (fEst * 2.5f, 150.0f, 1400.0f);
        if (std::fabs (trackHz - lastTrackHz) > lastTrackHz * 0.1f)
        {
            lp1.set (Biquad::LP, trackHz, 0.6f, 0, sr); lp2.set (Biquad::LP, trackHz, 0.6f, 0, sr);
            lastTrackHz = trackHz;
        }
        const float minP = float (sr) / 1500.0f, maxP = float (sr) / 35.0f;
        for (int i = 0; i < nS; ++i, n += 1.0)
        {
            const float m = (L[i] + R[i]) * 0.5f;
            const float e = env.tick (m);
            const float x = lp2.tick (lp1.tick (hp.tick (m, 0), 0), 0);
            const float h = envX.tick (x) * 0.3f + 1.0e-5f;    // hysteresis relative to the filtered signal
            if (x < -h) armed = true;
            if (armed && x >= 0.0f && prev < 0.0f && e > 1.0e-3f)
            {
                const double tc = n - 1.0 + double (-prev / (x - prev + 1.0e-12f));
                if (lastCross >= 0.0)
                {
                    const float per = float (tc - lastCross);
                    if (per > minP && per < maxP)
                    {
                        const float f = float (sr) / per;
                        if (std::fabs (f / fEst - 1.0f) < 0.06f) fEst += 0.5f * (f - fEst);
                        else if (cand > 0 && std::fabs (f / cand - 1.0f) < 0.04f) fEst = f;   // two agreeing periods = new note
                        cand = f;
                    }
                }
                lastCross = tc; armed = false;
            }
            prev = x;
            logF += (std::log2 (fEst) + oct - logF) * kGlide;
            const float f0 = std::exp2 (logF);
            const float dt = std::min (0.45f, f0 / float (sr));
            ph += dt; if (ph >= 1.0f) ph -= 1.0f;
            float osc;
            switch (wave)
            {
                case 0:  osc = 2.0f * ph - 1.0f - polyBlep (ph, dt); break;
                case 1:  { float t2 = ph + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f; osc = (ph < 0.5f ? 1.0f : -1.0f) + polyBlep (ph, dt) - polyBlep (t2, dt); break; }
                case 2:  osc = 0.6f * fastSin2Pi (ph) + 0.3f * fastSin2Pi (ph * 2.0f) + 0.15f * fastSin2Pi (ph * 4.0f); break;
                default: osc = fastSin2Pi (ph); break;
            }
            const float a = clampf (e * 2.5f, 0.0f, 1.0f);
            amp += (a - amp) * 0.01f;
            const float fc = clampf (cutoff * (1.0f + envAmt * amp * 6.0f), 30.0f, float (sr) * 0.45f);
            filt.set (fc, 1.2f, sr);
            filt.tick (osc);
            const float y = (wave == 3 ? osc : filt.lp) * amp * 0.6f;
            L[i] = R[i] = y;
        }
        meter = amp;
    }
};

// =====================================================================================
struct BarberFx final : Effect
{
    DelayLine dl[2]; AllPass1 ap[2][2][6]; float ph = 0, wetPrev[2] = { 0, 0 };
    void prepare (double s, int b) override { Effect::prepare (s, b); for (auto& d : dl) d.init (int (0.02 * sr)); reset(); }
    void reset() override { for (auto& d : dl) d.clear(); for (auto& a : ap) for (auto& b : a) for (auto& c : b) c.reset(); wetPrev[0] = wetPrev[1] = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const int mode = (int) v[0];
        const bool up = (mode % 2) == 0, phaser = mode >= 2;
        const float inc = v[1] / float (sr), dmax = v[2] * 0.001f * float (sr), dmin = 0.0002f * float (sr), fb = v[3] * 0.01f, st = v[4] * 0.01f;
        const float ratioD = dmin / dmax;
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                const float base = ph + st * 0.25f * float (c);
                float wet = 0;
                if (! phaser)
                {
                    const float in = x + fb * wetPrev[c];
                    for (int k = 0; k < 2; ++k)
                    {
                        float r = base + 0.5f * float (k); r -= std::floor (r);
                        const float rr = up ? r : 1.0f - r;
                        const float d = std::max (2.0f, dmax * std::pow (ratioD, rr));
                        const float w = fastSin2Pi (r * 0.5f);
                        wet += dl[c].readCubic (d) * w * w;
                    }
                    dl[c].push (fastTanh (in));
                }
                else
                {
                    const float in = x + fb * wetPrev[c];
                    for (int k = 0; k < 2; ++k)
                    {
                        float r = base + 0.5f * float (k); r -= std::floor (r);
                        const float rr = up ? r : 1.0f - r;
                        const float a = allpassCoefFromHz (80.0f * std::exp2 (rr * 6.64f), sr);
                        float y = in;
                        for (int s = 0; s < 6; ++s) y = ap[c][k][s].tick (y, a);
                        const float w = fastSin2Pi (r * 0.5f);
                        wet += y * w * w;
                    }
                }
                wetPrev[c] = fastTanh (wet);
                ch[c][i] = 0.8f * (x + wet);
            }
        }
    }
};

// =====================================================================================
struct RadioFx final : Effect
{
    Biquad hp, lp, peak, nlp; Rng rng { 1977u }; float ph1 = 0, ph2 = 0, slow = 0, drift = 0, driftT = 0, crack = 0; int driftCtr = 0, lastBand = -1;
    void prepare (double s, int b) override { Effect::prepare (s, b); lastBand = -1; reset(); }
    void reset() override { hp.reset(); lp.reset(); peak.reset(); nlp.reset(); crack = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float hpF[4] = { 180, 380, 600, 250 }, lpF[4] = { 4200, 3200, 2600, 3800 }, satB[4] = { 1.5f, 3.0f, 8.0f, 1.2f };
        const int band = std::max (0, std::min (3, (int) v[0]));
        if (band != lastBand)
        {
            hp.set (Biquad::HP, hpF[band], 0.8f, 0, sr); lp.set (Biquad::LP, lpF[band], 0.9f, 0, sr);
            peak.set (Biquad::Peak, 1500.0f, 1.2f, band == 3 ? 6.0f : 2.0f, sr);
            nlp.set (Biquad::LP, lpF[band], 0.7f, 0, sr);
            lastBand = band;
        }
        const float detune = v[1] * 0.01f, stat = v[2] * 0.01f, scan = v[3] * 0.01f, drive = v[4] * 0.01f;
        const float g = 1.0f + drive * 8.0f, sat = satB[band] * g, satN = 1.0f / fastTanh (sat * 0.5f + 0.2f);
        const float crackRate = (band == 3 ? 25.0f : 4.0f) * stat / float (sr);
        const float crackDecay = std::exp (-1.0f / (0.002f * float (sr)));
        for (int i = 0; i < n; ++i)
        {
            if (--driftCtr <= 0) { driftCtr = int (0.4 * sr); driftT = rng.bi(); }
            drift += (driftT - drift) * 0.00002f;
            slow += 0.13f / float (sr); if (slow >= 1) slow -= 1;
            const float off = clampf (detune + scan * (0.35f * (0.5f + 0.5f * fastSin2Pi (slow)) + 0.25f * drift), 0.0f, 1.0f);
            const float m = (L[i] + R[i]) * 0.5f;
            float x = fastTanh (m * sat) * satN * 0.5f;
            x = peak.tick (lp.tick (hp.tick (x, 0), 0), 0);
            ph1 += (400.0f + 2600.0f * off) / float (sr); if (ph1 >= 1) ph1 -= 1;
            ph2 += (30.0f + 200.0f * off) / float (sr);   if (ph2 >= 1) ph2 -= 1;
            float y = x * (1.0f - 0.8f * off) + x * fastSin2Pi (ph2) * off * 0.6f;
            y += fastSin2Pi (ph1) * off * (1.0f - off) * 0.12f;
            if (rng.uni() < crackRate) crack += rng.bi() * (0.2f + 0.5f * stat);
            crack *= crackDecay;
            y += nlp.tick (rng.bi() * (stat * 0.025f + off * 0.06f), 0) + crack;
            y *= 1.0f - scan * 0.35f * (0.5f + 0.5f * fastSin2Pi (slow * 3.0f));
            L[i] = R[i] = y * 0.8f;
        }
    }
};

// =====================================================================================
struct OrbitFx final : Effect
{
    DelayLine dl; OnePole sh[2]; float ph = 0; int ctr = 0; float dL = 0, dR = 0, gL = 1, gR = 1;
    void prepare (double s, int b) override { Effect::prepare (s, b); dl.init (int (0.012 * sr)); reset(); }
    void reset() override { dl.clear(); sh[0].reset(); sh[1].reset(); ctr = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const int path = (int) v[1];
        const float inc = v[0] / float (sr), dop = v[2] * 0.01f, shadow = v[3] * 0.01f, width = v[4] * 0.01f;
        const float ms = float (sr) * 0.001f;
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            dl.push ((L[i] + R[i]) * 0.5f);
            if (ctr-- <= 0)
            {
                ctr = 16;
                float th;
                if (path == 2) th = kPi * 0.5f + kPi * 0.5f * fastSin2Pi (ph);
                else th = k2Pi * (path == 0 ? -ph : ph) + kPi * 0.5f;
                const float cx = std::cos (th), cy = std::sin (th);
                const float distL = std::sqrt ((cx + 0.15f) * (cx + 0.15f) + cy * cy);
                const float distR = std::sqrt ((cx - 0.15f) * (cx - 0.15f) + cy * cy);
                dL = (0.5f + dop * 3.0f * distL) * ms;
                dR = (0.5f + dop * 3.0f * distR) * ms;
                gL = std::sqrt (0.5f * (1.0f - width * cx)); gR = std::sqrt (0.5f * (1.0f + width * cx));
                const float behind = std::max (0.0f, -cy);
                const float fL = 18000.0f * std::exp2 (-shadow * (2.5f * (1.0f + cx) * 0.5f + 1.5f * behind));
                const float fR = 18000.0f * std::exp2 (-shadow * (2.5f * (1.0f - cx) * 0.5f + 1.5f * behind));
                sh[0].setHz (fL, sr); sh[1].setHz (fR, sr);
            }
            L[i] = sh[0].lp (dl.readCubic (std::max (2.0f, dL))) * gL * 1.65f;
            R[i] = sh[1].lp (dl.readCubic (std::max (2.0f, dR))) * gR * 1.65f;
        }
    }
};

// =====================================================================================
struct MeltFx final : Effect
{
    DelayLine dl[2]; EnvFollower fast, slow; OnePole dark[2];
    float d[2] = { 2, 2 }; int head = 0; float xf = 1.0f; float prog = 1.0f, ph = 0; int refr = 0, ctr = 0; float maxD = 0;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& x : dl) x.init (int (4.2 * sr));
        maxD = float (4.0 * sr);
        fast.setup (0.5f, 30.0f, sr); slow.setup (30.0f, 300.0f, sr);
        reset();
    }
    void reset() override { for (auto& x : dl) x.clear(); fast.reset(); slow.reset(); dark[0].reset(); dark[1].reset(); d[0] = d[1] = 2; head = 0; xf = 1; prog = 1; refr = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float sag = v[0], wob = v[2] * 0.01f, dk = v[3] * 0.01f, retrig = v[4];
        const float progInc = 1.0f / (v[1] * float (sr));
        const float xfInc = 1.0f / (0.012f * float (sr));
        const int refrS = int (0.08 * sr);
        for (int i = 0; i < n; ++i)
        {
            const float m = std::max (std::fabs (L[i]), std::fabs (R[i])) + 1.0e-7f;
            const float fd = gainToDb (fast.tick (m)), sd = gainToDb (slow.tick (m));
            if (refr > 0) --refr;
            if (fd - sd > retrig && fd > -60.0f && refr == 0)
            {
                // new note: start a fresh read head at zero delay and crossfade to it
                head = 1 - head; d[head] = 2.0f; xf = 0.0f; prog = 0.0f; refr = refrS;
            }
            prog = std::min (1.0f, prog + progInc);
            ph += 5.0f / float (sr); if (ph >= 1) ph -= 1;
            const float curve = prog * std::sqrt (prog);
            const float semis = -sag * curve + wob * curve * 0.35f * fastSin2Pi (ph);
            const float rate = std::exp2 (semis / 12.0f);
            d[head] = std::min (maxD, d[head] + (1.0f - rate));
            d[1 - head] = std::min (maxD, d[1 - head] + (1.0f - rate));
            xf = std::min (1.0f, xf + xfInc);
            if (ctr-- <= 0) { ctr = 32; const float fc = 18000.0f * std::exp2 (-dk * curve * 4.5f); dark[0].setHz (fc, sr); dark[1].setHz (fc, sr); }
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                dl[c].push (ch[c][i]);
                const float a = dl[c].readCubic (std::max (2.0f, d[head]));
                const float b = xf < 1.0f ? dl[c].readCubic (std::max (2.0f, d[1 - head])) : 0.0f;
                ch[c][i] = dark[c].lp (a * xf + b * (1.0f - xf));
            }
        }
        meter = prog;
    }
};

// =====================================================================================
struct ChaosFx final : Effect
{
    static constexpr int NL = 4;
    DelayLine line[NL]; OnePole damp[NL], hpf[NL]; Smooth t[NL];
    double lx = 0.1, ly = 0.0, lz = 0.0; int ctr = 0; float mod[NL] = {};
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (int k = 0; k < NL; ++k) { line[k].init (int (2.6 * sr)); t[k].setTime (90.0f, sr); t[k].reset (0.4f * float (sr)); hpf[k].setHz (60.0f, sr); }
        reset();
    }
    void reset() override { for (int k = 0; k < NL; ++k) { line[k].clear(); damp[k].reset(); hpf[k].reset(); } lx = 0.1; ly = 0; lz = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float ratios[NL] = { 1.0f, 1.37f, 0.71f, 1.83f };
        const float base = v[0] * 0.001f * float (sr), chaos = v[1] * 0.01f, fb = v[3] * 0.01f, spread = v[5] * 0.01f;
        const double dt = v[2] * 32.0 / sr * 0.6;
        for (int k = 0; k < NL; ++k) damp[k].setHz (v[4], sr);
        const float maxD = float (line[0].size() - 8);
        for (int i = 0; i < n; ++i)
        {
            if (ctr-- <= 0)
            {
                ctr = 32;
                const double dx = 10.0 * (ly - lx), dy = lx * (28.0 - lz) - ly, dz = lx * ly - 8.0 / 3.0 * lz;
                lx += dx * dt; ly += dy * dt; lz += dz * dt;
                mod[0] = float (lx / 20.0); mod[1] = float (ly / 27.0); mod[2] = float ((lz - 25.0) / 25.0); mod[3] = mod[0] * mod[1];
                for (int k = 0; k < NL; ++k) t[k].tgt = clampf (base * ratios[k] * (1.0f + chaos * 0.3f * clampf (mod[k], -1.2f, 1.2f)), 64.0f, maxD);
            }
            float o[NL]; float sum = 0;
            for (int k = 0; k < NL; ++k) { o[k] = hpf[k].hp (damp[k].lp (line[k].read (t[k].next()))); sum += o[k]; }
            sum *= 0.5f;
            const float inL = L[i], inR = R[i];
            for (int k = 0; k < NL; ++k) line[k].push (((k & 1) ? inR : inL) * 1.0f + fb * fastTanh (o[k] - sum));
            const float ol = 0.6f * (o[0] + o[2]), orr = 0.6f * (o[1] + o[3]), mid = 0.5f * (ol + orr);
            L[i] = lerp (mid, ol, spread);
            R[i] = lerp (mid, orr, spread);
        }
        meter = clampf (0.5f + 0.5f * mod[0], 0, 1);
    }
};

// =====================================================================================
// v2.0: compressor collection
// =====================================================================================
inline float softKneeGr (float over, float slope, float knee)     // gain change in dB for a level 'over' dB above threshold
{
    if (2.0f * over < -knee) return 0.0f;
    if (2.0f * std::fabs (over) <= knee) return slope * sqr (over + knee * 0.5f) / (2.0f * knee);
    return slope * over;
}

struct Comp76Fx final : Effect      // FET-style: fast, aggressive, coloured
{
    float gr = 0; Os2 os[2];
    void reset() override { gr = 0; os[0].reset(); os[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float ratios[5] = { 4, 8, 12, 20, 30 }, thrs[5] = { -18, -16, -14, -12, -22 };
        const int ri = std::max (0, std::min (4, (int) v[4]));
        const bool allIn = ri == 4;
        const float inG = dbToGain (v[0]), outG = dbToGain (v[1]);
        const float aA = onePoleCoef (v[2], sr);
        const float aR = onePoleCoef (allIn ? v[3] * 0.5f : v[3], sr), aRslow = onePoleCoef (v[3] * 3.0f, sr);
        const float slope = 1.0f / ratios[ri] - 1.0f, thr = thrs[ri], knee = allIn ? 8.0f : 3.0f;
        const float color = allIn ? 0.5f : 0.2f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i] * inG, r = R[i] * inG;
            const float tgt = softKneeGr (gainToDb (maxAbs (l, r)) - thr, slope, knee);
            if (tgt < gr) gr = tgt + aA * (gr - tgt);
            else gr = tgt + (gr < -10.0f ? aR : lerp (aRslow, aR, -gr * 0.1f)) * (gr - tgt);   // release speeds up when hit harder
            const float g = dbToGain (gr);
            const float c = color * std::min (1.0f, -gr / 12.0f);
            auto fet = [c] (float u) { return u + c * (fastTanh (u * 1.5f + 0.25f) - 0.2449f - u) * 0.5f; };   // even-order FET colour grows with reduction
            l = os[0].process (l * g, fet); r = os[1].process (r * g, fet);
            L[i] = l * outG; R[i] = r * outG;
        }
        meter = clampf (-gr / 20.0f, 0, 1);
    }
};

struct OptoFx final : Effect        // optical leveller: slow, smooth, program dependent
{
    float fast = 0, slow = 0; OnePole emph;
    void reset() override { fast = slow = 0; emph.reset(); }
    void prepare (double s, int b) override { Effect::prepare (s, b); emph.setHz (1500.0f, sr); reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float scG = dbToGain (v[0] * 0.42f - 6.0f), mk = dbToGain (v[1]);
        const bool limit = (int) v[2] == 1;
        const float em = v[3] * 0.01f * 2.5f;
        const float slope = (limit ? 1.0f / 10.0f : 1.0f / 3.0f) - 1.0f;
        const float aA = onePoleCoef (10.0f, sr), aRf = onePoleCoef (60.0f, sr), aSa = onePoleCoef (400.0f, sr), aSr = onePoleCoef (2500.0f, sr);
        float gr = 0;
        for (int i = 0; i < n; ++i)
        {
            float sc = (L[i] + R[i]) * 0.5f;
            if (em > 0.0f) sc += em * emph.hp (sc);
            const float x = std::fabs (sc) * scG;
            fast = x > fast ? x + aA * (fast - x) : x + aRf * (fast - x);          // the cell reacts in ~10 ms, half-recovers in ~60 ms
            slow = fast > slow ? fast + aSa * (slow - fast) : fast + aSr * (slow - fast);   // ...then remembers for seconds
            const float cell = std::max (fast, slow * 0.85f);
            gr = softKneeGr (gainToDb (cell) + 16.0f, slope, 10.0f);
            const float g = dbToGain (gr) * mk;
            L[i] *= g; R[i] *= g;
        }
        meter = clampf (-gr / 20.0f, 0, 1);
    }
};

struct BusCompFx final : Effect     // VCA bus compressor: glue
{
    float gr = 0, grSlow = 0, rms = 0; OnePole scHP[2];
    void reset() override { gr = grSlow = rms = 0; scHP[0].reset(); scHP[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float ratios[3] = { 2, 4, 10 }, atk[6] = { 0.1f, 0.3f, 1, 3, 10, 30 }, rel[5] = { 100, 300, 600, 1200, 150 };
        const int ri = std::max (0, std::min (2, (int) v[1])), ai = std::max (0, std::min (5, (int) v[2])), rl = std::max (0, std::min (4, (int) v[3]));
        const bool autoRel = rl == 4;
        const float slope = 1.0f / ratios[ri] - 1.0f, knee = ri == 0 ? 8.0f : (ri == 1 ? 4.0f : 1.5f);
        const float aA = onePoleCoef (atk[ai], sr), aR = onePoleCoef (rel[rl], sr), aRs = onePoleCoef (1500.0f, sr), aAs = onePoleCoef (atk[ai] * 20.0f + 20.0f, sr);
        const float mk = dbToGain (v[4]), rmsC = onePoleCoef (5.0f, sr);
        const bool useHP = v[5] > 20.5f;
        if (useHP) { scHP[0].setHz (v[5], sr); scHP[1].a = scHP[0].a; }
        for (int i = 0; i < n; ++i)
        {
            const float lv = useHP ? maxAbs (scHP[0].hp (L[i]), scHP[1].hp (R[i])) : maxAbs (L[i], R[i]);
            rms = lv * lv + rmsC * (rms - lv * lv);
            const float tgt = softKneeGr (gainToDb (std::sqrt (rms) * 1.414f) - v[0], slope, knee);
            gr = tgt < gr ? tgt + aA * (gr - tgt) : tgt + aR * (gr - tgt);
            float use = gr;
            if (autoRel) { grSlow = gr < grSlow ? gr + aAs * (grSlow - gr) : gr + aRs * (grSlow - gr); use = std::min (gr, grSlow * 0.7f + gr * 0.3f); }
            const float g = dbToGain (use) * mk;
            L[i] *= g; R[i] *= g;
        }
        meter = clampf (-gr / 20.0f, 0, 1);
    }
};

struct PedalCompFx final : Effect   // stompbox squeeze and sustain
{
    float gr = 0; Biquad tone; float lastTone = -999;
    void prepare (double s, int b) override { Effect::prepare (s, b); lastTone = -999; reset(); }
    void reset() override { gr = 0; tone.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float sus = v[0] * 0.01f, lvl = dbToGain (v[1]), blend = v[4] * 0.01f;
        const float thr = -20.0f - sus * 35.0f, slope = 1.0f / 12.0f - 1.0f;
        const float aA = onePoleCoef (lerp (12.0f, 0.5f, v[2] * 0.01f), sr), aR = onePoleCoef (450.0f, sr);
        const float mk = dbToGain (-thr * 0.62f - 6.0f);
        if (v[3] != lastTone) { tone.set (Biquad::HighShelf, 2000.0f, 0.7f, v[3] * 0.08f, sr); lastTone = v[3]; }
        for (int i = 0; i < n; ++i)
        {
            const float tgt = softKneeGr (gainToDb (maxAbs (L[i], R[i])) - thr, slope, 6.0f);
            gr = tgt < gr ? tgt + aA * (gr - tgt) : tgt + aR * (gr - tgt);
            const float g = dbToGain (gr) * mk;
            const float l = fastTanh (tone.tick (L[i] * g, 0) * 0.7f) * 1.4286f, r = fastTanh (tone.tick (R[i] * g, 1) * 0.7f) * 1.4286f;
            L[i] = lerp (l, L[i], blend) * lvl; R[i] = lerp (r, R[i], blend) * lvl;
        }
        meter = clampf (-gr / 30.0f, 0, 1);
    }
};

struct VariMuFx final : Effect      // tube variable-mu: the harder you hit it, the higher the ratio
{
    float gr = 0, grSlow = 0; DcBlock dc[2];
    void prepare (double s, int b) override { Effect::prepare (s, b); dc[0].setup (sr); dc[1].setup (sr); reset(); }
    void reset() override { gr = grSlow = 0; dc[0].reset(); dc[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float rec[5] = { 200, 400, 800, 2000, 300 };
        const int ri = std::max (0, std::min (4, (int) v[2]));
        const float inG = dbToGain (v[0]), outG = dbToGain (v[3]), warm = v[4] * 0.01f;
        const float aA = onePoleCoef (20.0f, sr), aR = onePoleCoef (rec[ri], sr), aAs = onePoleCoef (600.0f, sr), aRs = onePoleCoef (5000.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            float l = L[i] * inG, r = R[i] * inG;
            const float over = gainToDb (maxAbs (l, r)) - v[1] + 4.0f;
            const float o = std::max (0.0f, over);
            const float tgt = -o * (1.0f - 1.0f / (1.4f + o * 0.22f)) * std::min (1.0f, o * 0.25f);
            gr = tgt < gr ? tgt + aA * (gr - tgt) : tgt + aR * (gr - tgt);
            float use = gr;
            if (ri == 4) { grSlow = gr < grSlow ? gr + aAs * (grSlow - gr) : gr + aRs * (grSlow - gr); use = std::min (gr, grSlow); }
            const float g = dbToGain (use);
            l *= g; r *= g;
            if (warm > 0.0f)
            {
                l = dc[0].tick (lerp (l, fastTanh (l * 0.9f + 0.18f) * 1.15f - 0.2049f, warm));
                r = dc[1].tick (lerp (r, fastTanh (r * 0.9f + 0.18f) * 1.15f - 0.2049f, warm));
            }
            L[i] = l * outG; R[i] = r * outG;
        }
        meter = clampf (-gr / 20.0f, 0, 1);
    }
};

struct MultibandFx final : Effect   // 3 bands, Linkwitz-Riley crossovers (flat when idle, no latency)
{
    Biquad lp1[2], hp1[2], lp2[2], hp2[2], apL[2], apH[2];
    float gr[3] = { 0, 0, 0 }, lastX[2] = { -1, -1 };
    void prepare (double s, int b) override { Effect::prepare (s, b); lastX[0] = lastX[1] = -1; reset(); }
    void reset() override { for (int k = 0; k < 2; ++k) { lp1[k].reset(); hp1[k].reset(); lp2[k].reset(); hp2[k].reset(); apL[k].reset(); apH[k].reset(); } gr[0] = gr[1] = gr[2] = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float f2 = std::max (v[1], v[0] * 2.0f);
        if (v[0] != lastX[0] || f2 != lastX[1])
        {
            for (int k = 0; k < 2; ++k)
            {
                lp1[k].set (Biquad::LP, v[0], 0.7071f, 0, sr); hp1[k].set (Biquad::HP, v[0], 0.7071f, 0, sr);
                lp2[k].set (Biquad::LP, f2, 0.7071f, 0, sr);   hp2[k].set (Biquad::HP, f2, 0.7071f, 0, sr);
                apL[k].set (Biquad::LP, f2, 0.7071f, 0, sr);   apH[k].set (Biquad::HP, f2, 0.7071f, 0, sr);
            }
            lastX[0] = v[0]; lastX[1] = f2;
        }
        const float slope = 1.0f / v[5] - 1.0f, sp = v[6] * 0.01f, mk = dbToGain (v[7]);
        const float atk[3] = { lerp (40.0f, 3.0f, sp), lerp (25.0f, 1.0f, sp), lerp (10.0f, 0.3f, sp) };
        const float rel[3] = { lerp (500.0f, 90.0f, sp), lerp (350.0f, 60.0f, sp), lerp (200.0f, 35.0f, sp) };
        float aA[3], aR[3];
        for (int k = 0; k < 3; ++k) { aA[k] = onePoleCoef (atk[k], sr); aR[k] = onePoleCoef (rel[k], sr); }
        float* ch[2] = { L, R };
        float m = 0;
        for (int i = 0; i < n; ++i)
        {
            float band[2][3];
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                const float lo = lp1[1].tick (lp1[0].tick (x, c), c), hi = hp1[1].tick (hp1[0].tick (x, c), c);
                // the low band passes through the upper crossover's allpass so all three bands stay phase-aligned
                const float a = apL[0].tick (lo, c), bq = apH[0].tick (lo, c);
                band[c][0] = apL[1].tick (a, c) + apH[1].tick (bq, c);
                band[c][1] = lp2[1].tick (lp2[0].tick (hi, c), c);
                band[c][2] = hp2[1].tick (hp2[0].tick (hi, c), c);
            }
            float outL = 0, outR = 0;
            for (int k = 0; k < 3; ++k)
            {
                const float tgt = softKneeGr (gainToDb (maxAbs (band[0][k], band[1][k])) - v[2 + k], slope, 6.0f);
                gr[k] = tgt < gr[k] ? tgt + aA[k] * (gr[k] - tgt) : tgt + aR[k] * (gr[k] - tgt);
                const float g = dbToGain (gr[k]);
                outL += band[0][k] * g; outR += band[1][k] * g;
                m = std::max (m, -gr[k]);
            }
            L[i] = outL * mk; R[i] = outR * mk;
        }
        meter = clampf (m / 20.0f, 0, 1);
    }
};

// =====================================================================================
// v2.0: new effects
// =====================================================================================
struct LooperFx final : Effect
{
    // states shown in status[0]
    enum { Empty = 0, Armed, Recording, Playing, Overdubbing, Stopped };
    // commands from the UI
    enum { CmdPress = 1, CmdStop, CmdPlay, CmdUndo, CmdClear, CmdRec, CmdDub };
    std::vector<float> buf[2], undo[2]; std::vector<uint16_t> stamp;
    int maxLen = 0, len = 0, state = Empty, lastMode = 0, lastPedal = 0, undoPos = -1;
    uint16_t curStamp = 1; bool hasUndo = false, undoneState = false;
    double pos = 0; int64_t clock = 0, pedalDownAt = -1, lastPressAt = -1000000;
    EnvFollower env;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        maxLen = int (45.0 * sr);
        for (int c = 0; c < 2; ++c) { buf[c].assign ((size_t) maxLen + 4, 0.0f); undo[c].assign ((size_t) maxLen + 4, 0.0f); }
        stamp.assign ((size_t) maxLen + 4, 0);
        env.setup (0.5f, 50.0f, sr);
        len = 0; pos = 0; state = Empty; hasUndo = false; undoPos = -1;
    }
    void reset() override {}     // keep the loop when the slot is bypassed
    void startRecording() { len = 0; pos = 0; state = Recording; hasUndo = false; undoPos = -1; ++curStamp; }
    void closeLoop (bool thenDub)
    {
        if (len < int (0.05 * sr)) { state = Empty; len = 0; return; }
        const int f = std::min (len / 4, int (0.006 * sr));       // short fades so the loop point never clicks
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < f; ++i)
            {
                const float w = float (i) / float (f);
                buf[c][(size_t) i] *= w; buf[c][(size_t) (len - 1 - i)] *= w;
                undo[c][(size_t) i] = buf[c][(size_t) i]; undo[c][(size_t) (len - 1 - i)] = buf[c][(size_t) (len - 1 - i)];
            }
        pos = 0; state = thenDub ? Overdubbing : Playing;
        if (thenDub) ++curStamp;
    }
    void startDub() { ++curStamp; hasUndo = true; undoneState = false; state = Overdubbing; }
    void press (int64_t now)
    {
        // a second press within 0.45 s = stop (like most looper pedals)
        if (now - lastPressAt < (int64_t) (0.45 * sr) && (state == Playing || state == Overdubbing))
        {
            state = Stopped; lastPressAt = -1000000; return;
        }
        lastPressAt = now;
        switch (state)
        {
            case Empty:       state = Armed; break;
            case Armed:       startRecording(); break;
            case Recording:   closeLoop (false); break;
            case Playing:     startDub(); break;
            case Overdubbing: state = Playing; break;
            case Stopped:     pos = 0; state = Playing; break;
            default: break;
        }
    }
    void command (int c) override
    {
        switch (c)
        {
            case CmdPress: press (clock); break;
            case 8:        lastPressAt = -1000000; press (clock); lastPressAt = -1000000; break;   // on-screen button: never double-taps
            case CmdStop:  if (state == Recording) closeLoop (false); if (state == Armed) state = Empty; else if (len > 0) state = Stopped; break;
            case CmdPlay:  if (state == Recording) closeLoop (false); else if (len > 0) { if (state == Stopped) pos = 0; state = Playing; } break;
            case CmdUndo:  if (hasUndo && undoPos < 0 && state != Recording) { if (state == Overdubbing) state = Playing; undoPos = 0; } break;
            case CmdClear: state = Empty; len = 0; pos = 0; hasUndo = false; undoPos = -1; break;
            case CmdRec:   if (state == Recording) closeLoop (false); else if (state == Empty || state == Stopped) state = Armed; else if (state == Armed) startRecording(); break;
            case CmdDub:   if (state == Recording) closeLoop (true); else if (state == Overdubbing) state = Playing; else if (len > 0) { if (state == Stopped) pos = 0; startDub(); } break;
            default: break;
        }
    }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float bars[5] = { 0, 1, 2, 4, 8 };
        // Mode knob (for automation / old presets): acts as a command when it changes
        const int mode = (int) v[0];
        if (mode != lastMode)
        {
            if (mode == 0) command (CmdStop);
            else if (mode == 1) { if (state != Recording) startRecording(); }
            else if (mode == 2) command (CmdPlay);
            else command (CmdDub);
            lastMode = mode;
        }
        // Footswitch knob (link it to a MIDI pedal): each change Up->Down is a press; holding 1 s = undo
        const int pedal = (int) v[6];
        if (pedal != lastPedal)
        {
            if (pedal == 1) { pedalDownAt = clock; press (clock); }
            else if (pedalDownAt >= 0 && clock - pedalDownAt > (int64_t) sr) { if (state == Overdubbing) state = Playing; command (CmdUndo); }
            lastPedal = pedal;
        }
        const float level = v[1] * 0.01f, keep = v[2] * 0.01f;
        const double speed = ((int) v[3] == 0 ? 0.5 : ((int) v[3] == 2 ? 2.0 : 1.0)) * ((int) v[4] == 1 ? -1.0 : 1.0);
        const int barLen = (int) std::min<double> (maxLen, bars[std::max (0, std::min (4, (int) v[5]))] * 4.0 * 60.0 / std::max (20.0, ctx.bpm) * sr);
        const bool firstNote = (int) v[7] == 1;

        // undo / redo runs in slices so it never spikes the CPU
        if (undoPos >= 0)
        {
            const int end = std::min (len, undoPos + 32768);
            for (int i = undoPos; i < end; ++i)
                if (stamp[(size_t) i] == curStamp) for (int c = 0; c < 2; ++c) std::swap (buf[c][(size_t) i], undo[c][(size_t) i]);
            undoPos = end >= len ? -1 : end;
            if (undoPos < 0) undoneState = ! undoneState;
        }
        for (int i = 0; i < n; ++i, ++clock)
        {
            const float inL = L[i], inR = R[i];
            const float e = env.tick (std::max (std::fabs (inL), std::fabs (inR)));
            if (state == Armed && (! firstNote || e > 0.012f)) startRecording();
            if (state == Recording)
            {
                buf[0][(size_t) len] = undo[0][(size_t) len] = inL;
                buf[1][(size_t) len] = undo[1][(size_t) len] = inR;
                stamp[(size_t) len] = 0;
                if (++len >= maxLen || (barLen > 0 && len >= barLen)) closeLoop (false);
                continue;
            }
            if ((state != Playing && state != Overdubbing) || len <= 0) continue;
            const int i0 = std::min ((int) pos, len - 1); const float fr = float (pos - std::floor (pos));
            const int i1 = i0 + 1 >= len ? 0 : i0 + 1;
            const float yl = buf[0][(size_t) i0] + fr * (buf[0][(size_t) i1] - buf[0][(size_t) i0]);
            const float yr = buf[1][(size_t) i0] + fr * (buf[1][(size_t) i1] - buf[1][(size_t) i0]);
            if (state == Overdubbing && undoPos < 0)
            {
                if (stamp[(size_t) i0] != curStamp) { stamp[(size_t) i0] = curStamp; undo[0][(size_t) i0] = buf[0][(size_t) i0]; undo[1][(size_t) i0] = buf[1][(size_t) i0]; }
                buf[0][(size_t) i0] = buf[0][(size_t) i0] * keep + inL;
                buf[1][(size_t) i0] = buf[1][(size_t) i0] * keep + inR;
            }
            L[i] = inL + yl * level; R[i] = inR + yr * level;
            pos += speed;
            if (pos >= len) pos -= len; else if (pos < 0) pos += len;
        }
        status[0] = (float) state;
        status[1] = (float) (len / sr);
        status[2] = state == Recording ? (barLen > 0 ? float (len) / float (barLen) : 0.0f) : (len > 0 ? float (pos / len) : 0.0f);
        status[3] = (hasUndo ? 1.0f : 0.0f) + (undoneState ? 2.0f : 0.0f);
        meter = state == Recording || state == Overdubbing ? 1.0f : (state == Playing ? 0.5f : 0.0f);
    }
};

struct FreqShiftFx final : Effect
{
    Hilbert hb[2]; DelayLine dl[2]; OnePole tone[2]; double ph[2] = { 0, 0 }; Smooth dly;
    void prepare (double s, int b) override { Effect::prepare (s, b); for (auto& d : dl) d.init (int (0.32 * sr)); dly.setTime (60.0f, sr); dly.reset (0.02f * float (sr)); reset(); }
    void reset() override { hb[0].reset(); hb[1].reset(); dl[0].clear(); dl[1].clear(); tone[0].reset(); tone[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float shift = v[0] + v[1], fb = v[2] * 0.01f, st = v[4] * 0.01f;
        dly.tgt = v[3] * 0.001f * float (sr);
        tone[0].setHz (v[5], sr); tone[1].a = tone[0].a;
        const double inc[2] = { shift / sr, shift * (1.0 - 2.0 * st) / sr };     // Stereo: the right side shifts the other way
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float d = std::max (2.0f, dly.next());
            for (int c = 0; c < 2; ++c)
            {
                float I, Q;
                hb[c].tick (ch[c][i] + fb * dl[c].read (d), I, Q);
                ph[c] += inc[c]; ph[c] -= std::floor (ph[c]);
                const float p = (float) ph[c];
                const float y = tone[c].lp (I * fastSin2Pi (p + 0.25f) + Q * fastSin2Pi (p));
                dl[c].push (fastTanh (y));
                ch[c][i] = y;
            }
        }
    }
};

struct WavefolderFx final : Effect
{
    Os4 os[2]; OnePole tone[2]; DcBlock dc[2];
    void prepare (double s, int b) override { Effect::prepare (s, b); dc[0].setup (sr); dc[1].setup (sr); reset(); }
    void reset() override { for (int c = 0; c < 2; ++c) { os[c].reset(); tone[c].reset(); dc[c].reset(); } }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float gain = dbToGain (v[0] * 0.36f), bias = v[1] * 0.01f, shape = v[2] * 0.01f, outG = dbToGain (v[4]);
        tone[0].setHz (v[3], sr); tone[1].a = tone[0].a;
        auto fold = [=] (float x)
        {
            const float u = x * gain + bias;
            float t = u * 0.25f + 0.25f; t -= std::floor (t);
            const float tri = 1.0f - 4.0f * std::fabs (t - 0.5f);
            return lerp (tri, fastSin2Pi (u * 0.25f), shape);
        };
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                ch[c][i] = tone[c].lp (dc[c].tick (os[c].process (ch[c][i], fold))) * outG;
    }
};

struct UnisonFx final : Effect
{
    GrainShifter sh[9]; OnePole tone[2];
    void prepare (double s, int b) override { Effect::prepare (s, b); for (auto& x : sh) x.init (int (0.07 * sr)); reset(); }
    void reset() override { for (auto& x : sh) x.clear(); tone[0].reset(); tone[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const int V = std::max (2, std::min (7, (int) v[0]));
        const float det = v[1], spread = v[2] * 0.01f, up = v[3] * 0.01f, sub = v[4] * 0.01f;
        tone[0].setHz (v[5], sr); tone[1].a = tone[0].a;
        float ratio[7], gl[7], gr[7];
        for (int k = 0; k < V; ++k)
        {
            const float pos = V > 1 ? (float (k) / float (V - 1)) * 2.0f - 1.0f : 0.0f;
            ratio[k] = std::exp2 (det * pos * (1.0f + 0.13f * float (k % 3)) / 1200.0f);
            const float a = ((k & 1 ? 1.0f : -1.0f) * spread * (0.4f + 0.6f * std::fabs (pos)) + 1.0f) * kPi * 0.25f;
            gl[k] = std::cos (a); gr[k] = std::sin (a);
        }
        const float norm = 1.5f / std::sqrt (float (V));
        const float fs = float (sr);
        for (int i = 0; i < n; ++i)
        {
            const float m = (L[i] + R[i]) * 0.5f;
            float ol = 0, orr = 0;
            for (int k = 0; k < V; ++k)
            {
                const float y = sh[k].tick (m, ratio[k], (0.038f + 0.004f * float (k)) * fs);
                ol += y * gl[k]; orr += y * gr[k];
            }
            ol *= norm; orr *= norm;
            if (up > 0.0f)  { const float y = sh[7].tick (m, 2.0f, 0.05f * fs) * up * 0.6f; ol += y; orr += y; }
            if (sub > 0.0f) { const float y = sh[8].tick (m, 0.5f, 0.06f * fs) * sub * 0.8f; ol += y; orr += y; }
            L[i] = tone[0].lp (ol); R[i] = tone[1].lp (orr);
        }
    }
};

struct BellsFx final : Effect
{
    static constexpr int NP = 8;
    Svf res[NP]; OnePole exLP; float amp[NP] = {}, gl[NP] = {}, gr[NP] = {}, inG[NP] = {}; bool on[NP] = {}; float last[6] = { -9, -9, -9, -9, -9, -9 };
    void prepare (double s, int b) override { Effect::prepare (s, b); exLP.setHz (250.0f, sr); for (auto& x : last) x = -9; reset(); }
    void reset() override { for (auto& r : res) r.reset(); exLP.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float ratios[5][NP] = {
            { 0.5f, 1.0f, 1.183f, 1.506f, 2.0f, 2.514f, 2.662f, 3.011f },      // church bell
            { 1.0f, 1.59f, 2.32f, 3.14f, 4.25f, 5.4f, 6.63f, 8.2f },           // glass
            { 1.0f, 1.52f, 2.0f, 2.64f, 3.03f, 3.97f, 4.56f, 5.41f },          // gamelan
            { 0.5f, 1.0f, 1.47f, 2.09f, 2.76f, 3.5f, 4.4f, 5.4f },             // tubular
            { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f } };              // harmonic
        bool ch = false;
        for (int k = 0; k < 6; ++k) if (v[k] != last[k]) { ch = true; last[k] = v[k]; }
        if (ch)
        {
            const float root = 261.63f * std::exp2 (v[0] / 12.0f) * ((int) v[1] == 0 ? 0.5f : ((int) v[1] == 2 ? 2.0f : 1.0f));
            const int mat = std::max (0, std::min (4, (int) v[2]));
            const float bright = 0.35f + v[4] * 0.0065f, spread = v[5] * 0.01f;
            for (int k = 0; k < NP; ++k)
            {
                const float f = root * ratios[mat][k];
                on[k] = f < float (sr) * 0.42f;
                if (! on[k]) continue;
                const float t60 = v[3] / (1.0f + 0.25f * float (k));                 // upper partials die sooner
                const float q = std::max (2.0f, kPi * f * t60 / 6.9f);
                res[k].set (f, q, sr);
                inG[k] = 45.0f / q * std::sqrt (f / 500.0f);
                amp[k] = std::pow (bright, float (k));
                const float a = (((k % 4 == 1 || k % 4 == 2) ? 1.0f : -1.0f) * spread * 0.8f + 1.0f) * kPi * 0.25f;
                gl[k] = std::cos (a) * amp[k]; gr[k] = std::sin (a) * amp[k];
            }
        }
        for (int i = 0; i < n; ++i)
        {
            const float m = (L[i] + R[i]) * 0.5f;
            const float ex = m - exLP.lp (m);                                        // the pick attack strikes the bells
            float ol = 0, orr = 0;
            for (int k = 0; k < NP; ++k)
            {
                if (! on[k]) continue;
                res[k].tick (ex * inG[k]);
                ol += res[k].bp * gl[k]; orr += res[k].bp * gr[k];
            }
            L[i] = fastTanh (ol); R[i] = fastTanh (orr);
        }
    }
};

struct VinylFx final : Effect
{
    DelayLine dl[2]; Biquad hp, lp, honk; OnePole rumLP, crackLP; Rng rng { 3333u }; Smooth base;
    float ph = 0, crack = 0, pop = 0, hum = 0, last[2] = { -1, -1 }; int lastRpm = -1;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& d : dl) d.init (int (0.03 * sr));
        rumLP.setHz (35.0f, sr); crackLP.setHz (6000.0f, sr);
        base.setTime (200.0f, sr); base.reset (0.004f * float (sr));
        last[0] = last[1] = -1; lastRpm = -1; reset();
    }
    void reset() override { dl[0].clear(); dl[1].clear(); hp.reset(); lp.reset(); honk.reset(); rumLP.reset(); crackLP.reset(); crack = pop = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float rate[3] = { 0.555f, 0.75f, 1.3f };
        const int rpm = std::max (0, std::min (2, (int) v[5]));
        const float crk = v[0] * 0.01f, warp = v[1] * 0.01f, age = v[2] * 0.01f + (rpm == 2 ? 0.3f : 0.0f), rum = v[3] * 0.01f, wear = v[4] * 0.01f;
        if (age != last[0] || rpm != lastRpm)
        {
            hp.set (Biquad::HP, 30.0f + age * 140.0f, 0.707f, 0, sr);
            lp.set (Biquad::LP, 17000.0f * std::pow (0.2f, std::min (1.0f, age)), 0.707f, 0, sr);
            honk.set (Biquad::Peak, 1200.0f, 0.8f, age * 5.0f, sr);
            last[0] = age; lastRpm = rpm;
        }
        const float fs = float (sr), wD = warp * 0.0022f * fs;
        base.tgt = 0.001f * fs + wD + 2.0f;
        const float inc = rate[rpm] / fs, crackP = crk * 28.0f / fs, popP = crk * 1.2f / fs;
        const float cDec = std::exp (-1.0f / (0.0006f * fs)), pDec = std::exp (-1.0f / (0.004f * fs));
        const float dg = 1.0f + wear * 5.0f;
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            const float d = base.next() + wD * (0.8f * fastSin2Pi (ph) + 0.2f * fastSin2Pi (ph * 2.0f + 0.3f));   // off-centre record warp
            if (rng.uni() < crackP) crack += rng.bi() * 0.25f;
            if (rng.uni() < popP) pop += rng.bi() * 0.7f;
            crack *= cDec; pop *= pDec;
            hum += 50.0f / fs; if (hum >= 1) hum -= 1;
            const float noise = crackLP.lp (crack + pop) * crk
                              + (rumLP.lp (rng.bi()) * 6.0f + fastSin2Pi (hum) * 0.02f) * rum * 0.12f + rng.bi() * crk * 0.0015f;
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                dl[c].push (ch[c][i]);
                float y = dl[c].readCubic (d);
                if (wear > 0.0f) y = fastTanh (y * dg + 0.1f * wear) / dg * (1.0f + wear * 0.5f);   // worn groove: soft, slightly lopsided
                ch[c][i] = lp.tick (honk.tick (hp.tick (y, c), c), c) + noise;
            }
        }
    }
};

struct UnderwaterFx final : Effect
{
    Svf f1[2], f2[2]; DelayLine dl[2]; Rng rng { 808u };
    float ph1 = 0, ph2 = 0, drift = 0, driftT = 0; int ctr = 0;
    struct Bubble { bool on = false; float ph = 0, f = 400, env = 0, pan = 0.5f, rise = 1; } bub[6];
    void prepare (double s, int b) override { Effect::prepare (s, b); for (auto& d : dl) d.init (int (0.03 * sr)); reset(); }
    void reset() override { for (int c = 0; c < 2; ++c) { f1[c].reset(); f2[c].reset(); dl[c].clear(); } for (auto& b : bub) b.on = false; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float depth = v[0] * 0.01f, cur = v[1] * 0.01f, bubbles = v[2] * 0.01f, reso = 0.6f + v[3] * 0.035f;
        const float fs = float (sr), baseF = 2800.0f * std::pow (0.09f, depth);
        const float prob = bubbles * 7.0f / fs, envDec = std::exp (-1.0f / (0.05f * fs));
        for (int i = 0; i < n; ++i)
        {
            if (--ctr <= 0) { ctr = int (0.3f * fs); driftT = rng.bi(); }
            drift += (driftT - drift) * 0.00008f;
            ph1 += 0.31f / fs; if (ph1 >= 1) ph1 -= 1;
            ph2 += 0.83f / fs; if (ph2 >= 1) ph2 -= 1;
            const float wob = 0.6f * fastSin2Pi (ph1) + 0.4f * fastSin2Pi (ph2);
            const float fc = baseF * std::exp2 (cur * (0.5f * wob + 0.7f * drift));
            const float g = fastTan (kPi * clampf (fc, 60.0f, fs * 0.4f) / fs);
            const float d = (0.006f + cur * 0.0025f * wob) * fs;
            if (bubbles > 0.0f && rng.uni() < prob)
                for (auto& b : bub) if (! b.on) { b.on = true; b.ph = 0; b.f = 300.0f + 900.0f * rng.uni(); b.env = 0.5f + 0.5f * rng.uni(); b.pan = rng.uni(); b.rise = 1.0f + (4.0f + 8.0f * rng.uni()) / fs; break; }
            float bl = 0, br = 0;
            for (auto& b : bub)
            {
                if (! b.on) continue;
                b.ph += b.f / fs; if (b.ph >= 1) b.ph -= 1;
                b.f *= b.rise; b.env *= envDec;
                const float s = fastSin2Pi (b.ph) * b.env * 0.06f * bubbles;
                bl += s * (1.0f - b.pan); br += s * b.pan;
                if (b.env < 0.002f) b.on = false;
            }
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                dl[c].push (ch[c][i]);
                float y = dl[c].readCubic (d * (c ? 1.07f : 1.0f));
                f1[c].setG (g, reso); f1[c].tick (y);
                f2[c].setG (g, 0.7f); f2[c].tick (f1[c].lp);
                ch[c][i] = f2[c].lp * (1.0f + 0.2f * depth) + (c ? br : bl);
            }
        }
    }
};

struct ExciterFx final : Effect
{
    Biquad hp1, hp2, lpL, lpL2; Os2 os[2]; float last[2] = { -1, -1 };
    void prepare (double s, int b) override { Effect::prepare (s, b); last[0] = last[1] = -1; reset(); }
    void reset() override { hp1.reset(); hp2.reset(); lpL.reset(); lpL2.reset(); os[0].reset(); os[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        if (v[0] != last[0]) { hp1.set (Biquad::HP, v[0], 0.707f, 0, sr); hp2.set (Biquad::HP, v[0] * 1.2f, 0.707f, 0, sr); last[0] = v[0]; }
        if (v[4] != last[1]) { lpL.set (Biquad::LP, v[4], 0.707f, 0, sr); lpL2.set (Biquad::LP, v[4] * 2.5f, 0.707f, 0, sr); last[1] = v[4]; }
        const float drive = dbToGain (6.0f + v[1] * 0.3f), odd = v[2] * 0.01f, amt = v[3] * 0.01f * 0.9f, lowAmt = v[5] * 0.01f;
        auto shape = [=] (float u) { u *= drive; return lerp (fastTanh (u + 0.5f) - 0.4621f, fastTanh (u), odd); };
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                float y = x;
                if (amt > 0.0f) y += hp2.tick (os[c].process (hp1.tick (x, c), shape), c) * amt / std::sqrt (drive);
                if (lowAmt > 0.0f) y += lpL2.tick (fastTanh (lpL.tick (x, c) * 5.0f) * 0.2f, c) * lowAmt;
                ch[c][i] = y;
            }
    }
};

struct DoublerFx final : Effect
{
    DelayLine dl; OnePole tone[2], walkLP[4]; Rng rng { 2112u }; float ph[4] = { 0, 0.3f, 0.6f, 0.8f }; Smooth base;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        dl.init (int (0.12 * sr));
        for (auto& w : walkLP) w.setHz (0.7f, sr);
        base.setTime (80.0f, sr); base.reset (22.0f);
        reset();
    }
    void reset() override { dl.clear(); tone[0].reset(); tone[1].reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const int V = (int) v[0] == 1 ? 4 : 2;
        base.tgt = v[1];
        const float cents = v[2], human = v[3] * 0.01f, width = v[4] * 0.01f;
        tone[0].setHz (v[5], sr); tone[1].a = tone[0].a;
        const float ms = float (sr) * 0.001f;
        static const float mult[4] = { 1.0f, 1.17f, 1.41f, 0.83f };
        const float norm = V == 4 ? 0.6f : 0.8f;
        for (int i = 0; i < n; ++i)
        {
            dl.push ((L[i] + R[i]) * 0.5f);
            const float b = base.next();
            float ol = 0, orr = 0;
            for (int k = 0; k < V; ++k)
            {
                const float f = 0.37f + 0.11f * float (k);
                ph[k] += f / float (sr); if (ph[k] >= 1) ph[k] -= 1;
                const float depthMs = cents * 5.78e-4f / (k2Pi * f) * 1000.0f;                 // LFO depth that gives +/- 'cents' of pitch drift
                const float walk = walkLP[k].lp (rng.bi()) * 60.0f * human * 3.0f;             // each "take" wanders in timing
                const float d = clampf (b * mult[k] + depthMs * fastSin2Pi (ph[k]) + walk, 1.0f, 110.0f) * ms;
                const float y = dl.readCubic (d);
                const float side = (k & 1) ? 1.0f : -1.0f;
                const float a = (side * width * (k < 2 ? 1.0f : 0.5f) + 1.0f) * kPi * 0.25f;
                ol += y * std::cos (a) * 1.414f; orr += y * std::sin (a) * 1.414f;
            }
            L[i] = tone[0].lp (ol * norm); R[i] = tone[1].lp (orr * norm);
        }
    }
};

struct AcousticFx final : Effect
{
    Biquad hp, b1, b2, b3, scoop, air, lp; EnvFollower ef, es; float last[4] = { -9, -9, -9, -9 };
    void prepare (double s, int b) override { Effect::prepare (s, b); ef.setup (0.3f, 25.0f, sr); es.setup (15.0f, 25.0f, sr); for (auto& x : last) x = -9; reset(); }
    void reset() override { hp.reset(); b1.reset(); b2.reset(); b3.reset(); scoop.reset(); air.reset(); lp.reset(); ef.reset(); es.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float modes[4][3] = { { 98, 196, 400 }, { 85, 170, 380 }, { 125, 245, 520 }, { 105, 215, 330 } };
        bool ch = false;
        for (int k = 0; k < 4; ++k) if (v[k] != last[k]) { ch = true; last[k] = v[k]; }
        if (ch)
        {
            const int bd = std::max (0, std::min (3, (int) v[0]));
            const float am = v[1] * 0.01f;
            hp.set (Biquad::HP, 70.0f, 0.707f, 0, sr);
            b1.set (Biquad::Peak, modes[bd][0], 4.0f, 8.0f * am, sr);
            b2.set (Biquad::Peak, modes[bd][1], 5.0f, 6.0f * am, sr);
            b3.set (Biquad::Peak, modes[bd][2], 6.0f, 3.5f * am, sr);
            scoop.set (Biquad::Peak, 900.0f, 0.6f, -v[3] * 0.1f, sr);
            air.set (Biquad::HighShelf, 4500.0f, 0.7f, v[2] * (bd == 3 ? 0.07f : 0.14f), sr);
            lp.set (Biquad::LP, 14000.0f, 0.707f, 0, sr);
        }
        const float atk = v[4] * 0.01f, og = dbToGain (v[5] - 3.0f);
        for (int i = 0; i < n; ++i)
        {
            const float m = maxAbs (L[i], R[i]) + 1.0e-6f;
            const float tr = clampf (gainToDb (ef.tick (m)) - gainToDb (es.tick (m)), 0.0f, 12.0f);
            const float g = dbToGain (tr * atk * 0.6f) * og;                       // sharpen the pick attack like a flat-top
            float* c2[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                float x = c2[c][i];
                x = lp.tick (air.tick (scoop.tick (b3.tick (b2.tick (b1.tick (hp.tick (x, c), c), c), c), c), c), c);
                c2[c][i] = x * g;
            }
        }
    }
};

// =====================================================================================
// v2.1: Prism Shifter, Dream Loop, Microverse
// =====================================================================================
struct PrismFx final : Effect      // polyphonic pitch shifting with regenerating feedback
{
    GrainShifter sh1[2], sh2[2]; OnePole tone[2], fbTone[2]; float fb[2] = { 0, 0 };
    EnvFollower fast, slow; float cur = 0, swoop = 1; int refr = 0; Smooth win;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (int c = 0; c < 2; ++c) { sh1[c].init (int (0.1 * sr)); sh2[c].init (int (0.1 * sr)); fbTone[c].setHz (5000.0f, sr); }
        fast.setup (0.5f, 30.0f, sr); slow.setup (30.0f, 300.0f, sr);
        win.setTime (40.0f, sr); win.reset (0.04f * float (sr));
        reset();
    }
    void reset() override { for (int c = 0; c < 2; ++c) { sh1[c].clear(); sh2[c].clear(); tone[c].reset(); fbTone[c].reset(); fb[c] = 0; } fast.reset(); slow.reset(); swoop = 1; }
    static float quantize (float st, int mode)
    {
        if (mode == 1) return std::round (st);
        if (mode == 2)
        {
            static const float q[] = { -12, -7, -5, 0, 5, 7, 12 };
            float best = 0; for (float x : q) if (std::fabs (x - st) < std::fabs (best - st)) best = x;
            return best;
        }
        return st;
    }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        static const float sec[11] = { 0, -12, -7, -5, 3, 4, 5, 7, 12, 24, 0.12f };
        const int mode = std::max (0, std::min (3, (int) v[1])), si = std::max (0, std::min (10, (int) v[2]));
        const float target = quantize (v[0], mode);
        const float p1 = v[3] * 0.01f, p2 = si == 0 ? 0.0f : v[4] * 0.01f, magic = v[5] * 0.01f;
        win.tgt = lerp (0.012f, 0.09f, v[6] * 0.01f) * float (sr);       // low Tracking = glitchy, chirpy artefacts
        tone[0].setHz (v[7], sr); tone[1].a = tone[0].a;
        const float kGlide = 1.0f - std::exp (-1.0f / ((mode == 0 ? 0.03f : 0.004f) * float (sr)));
        const float kSwoop = 1.0f - std::exp (-1.0f / (0.25f * float (sr)));
        for (int i = 0; i < n; ++i)
        {
            const float m = std::max (std::fabs (L[i]), std::fabs (R[i])) + 1.0e-7f;
            const float fd = gainToDb (fast.tick (m)), sd = gainToDb (slow.tick (m));
            if (refr > 0) --refr;
            if (mode == 3 && fd - sd > 9.0f && fd > -55.0f && refr == 0) { swoop = 0.0f; refr = int (0.08 * sr); }   // every pick swoops up to the interval
            swoop += (1.0f - swoop) * kSwoop;
            cur += ((mode == 3 ? std::round (target) * swoop : target) - cur) * kGlide;
            const float r1 = std::exp2 (cur / 12.0f), r2 = std::exp2 ((cur + sec[si]) / 12.0f);
            const float w = win.next();
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                const float y1 = sh1[c].tick (x + magic * fb[c], r1, w * (c ? 1.07f : 1.0f));
                const float y2 = p2 > 0.0f ? sh2[c].tick (x, r2, w * (c ? 0.93f : 1.0f)) : 0.0f;
                fb[c] = fbTone[c].lp (fastTanh (y1 * 1.2f)) * 0.95f;     // each pass shifts again: cascading, rising tones
                ch[c][i] = tone[c].lp (x * 0.8f + y1 * p1 + y2 * p2);
            }
        }
        meter = clampf (magic, 0, 1);
    }
};

struct DreamFx final : Effect      // wet channel (reverb / delay / slip) + always-listening micro-looper + clock
{
    ReverbFx verb; DelayLine dl[2]; GrainShifter slip[2]; OnePole dTone[2], lpOut[2];
    std::vector<float> lbuf[2]; int lmask = 0; int64_t w = 0, base = 0; double lp = 0; float stretchPos = 0, g[2] = { 0, 0.5f }, gs[2] = { 0, 0 };
    EnvFollower env; float envG = 1, held[2] = { 0, 0 }, clk = 0; Smooth dTime;
    std::vector<float> wl, wr;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        verb.prepare (s, b);
        for (int c = 0; c < 2; ++c) { dl[c].init (int (1.6 * sr)); slip[c].init (int (0.12 * sr)); dTone[c].setHz (4500.0f, sr); }
        int sz = 1; while (sz < int (18.0 * sr)) sz <<= 1;
        lbuf[0].assign ((size_t) sz, 0.0f); lbuf[1].assign ((size_t) sz, 0.0f); lmask = sz - 1;
        env.setup (2.0f, 120.0f, sr);
        dTime.setTime (150.0f, sr); dTime.reset (0.4f * float (sr));
        wl.assign ((size_t) b, 0.0f); wr.assign ((size_t) b, 0.0f);
        reset();
    }
    void reset() override
    {
        verb.reset(); for (int c = 0; c < 2; ++c) { dl[c].clear(); slip[c].clear(); dTone[c].reset(); lpOut[c].reset(); }
        std::fill (lbuf[0].begin(), lbuf[0].end(), 0.0f); std::fill (lbuf[1].begin(), lbuf[1].end(), 0.0f);
        w = 0; base = 0; lp = 0; stretchPos = 0;
    }
    inline float rd (int c, double idx) const
    {
        const double f = std::floor (idx); const int64_t i = (int64_t) f; const float t = float (idx - f);
        const float a = lbuf[c][(size_t) (i & lmask)], b = lbuf[c][(size_t) ((i + 1) & lmask)];
        return a + t * (b - a);
    }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float tapeSp[7] = { -2.0f, -1.0f, -0.5f, 0.5f, 1.0f, 1.5f, 2.0f };
        const int wetMode = std::max (0, std::min (2, (int) v[0])), loopMode = std::max (0, std::min (3, (int) v[3]));
        const float time = v[1] * 0.01f, mod = v[2] * 0.01f, lmod = v[5] * 0.01f;
        const bool hold = (int) v[7] == 1;
        const int64_t Ls = std::max<int64_t> (256, (int64_t) (v[4] * sr));
        if (n > (int) wl.size()) return;
        // ---- wet channel
        std::copy (L, L + n, wl.begin()); std::copy (R, R + n, wr.begin());
        if (wetMode == 0)
        {
            const float rv[8] = { 30.0f + time * 70.0f, 0.4f + time * time * 9.0f, 20.0f + (1.0f - mod) * 60.0f, 10.0f, mod * 100.0f, 0.0f, 120.0f, 120.0f };
            verb.process (wl.data(), wr.data(), n, rv, ctx);
        }
        else
        {
            dTime.tgt = (0.04f * std::pow (30.0f, time)) * float (sr);
            const float fbk = wetMode == 1 ? mod * 0.95f : 0.62f;
            const float ratio = std::exp2 ((mod - 0.5f) * 2.0f);        // Slip: each repeat drifts up or down in pitch
            for (int i = 0; i < n; ++i)
            {
                const float d = dTime.next();
                for (int c = 0; c < 2; ++c)
                {
                    float* x = c ? wr.data() : wl.data();
                    float y = dl[c].read (std::max (2.0f, d * (c ? 1.03f : 1.0f)));
                    if (wetMode == 2) y = slip[c].tick (y, ratio, 0.06f * float (sr));
                    dl[c].push (x[i] + fbk * fastTanh (dTone[c].lp (y)));
                    x[i] = y;
                }
            }
        }
        // ---- loop channel
        const float kEnv = 1.0f - onePoleCoef (25.0f, sr);
        const double sp = loopMode == 2 ? (double) lerp (tapeSp[(int) std::min (5.0f, lmod * 6.0f)], tapeSp[(int) std::min (6.0f, lmod * 6.0f + 1.0f)], lmod * 6.0f - std::floor (lmod * 6.0f))
                                        : 1.0;
        const float stretch = std::exp2 ((lmod - 0.5f) * 4.0f);
        const float G = float (0.09 * sr), fade = float (0.01 * sr);
        const float cstep = v[6] >= 99.5f ? 1.0f : float (std::min (1.0, (1500.0 * std::pow (30.0, v[6] * 0.01)) / sr));
        for (int i = 0; i < n; ++i)
        {
            if (! hold) { lbuf[0][(size_t) (w & lmask)] = L[i]; lbuf[1][(size_t) (w & lmask)] = R[i]; }
            ++w;
            float ol = 0, orr = 0;
            if (loopMode > 0)
            {
                const double Ld = (double) Ls;
                bool wrapped = false;
                if (loopMode == 3)
                {
                    stretchPos += stretch;
                    if (stretchPos >= Ld) { stretchPos -= (float) Ld; wrapped = true; }
                    for (int k = 0; k < 2; ++k)
                    {
                        g[k] += 1.0f / G;
                        if (g[k] >= 1.0f) { g[k] -= 1.0f; gs[k] = stretchPos; }
                        const float hw = 0.5f - 0.5f * std::cos (k2Pi * g[k]);
                        const double idx = (double) base + std::fmod ((double) gs[k] + g[k] * G, Ld);
                        ol += rd (0, idx) * hw; orr += rd (1, idx) * hw;
                    }
                }
                else
                {
                    lp += sp;
                    if (lp >= Ld) { lp -= Ld; wrapped = true; } else if (lp < 0) { lp += Ld; wrapped = true; }
                    const double idx = (double) base + lp;
                    const float fw = std::min (1.0f, std::min ((float) lp, (float) (Ld - lp)) / fade);
                    ol = rd (0, idx) * fw; orr = rd (1, idx) * fw;
                }
                if (wrapped && ! hold) base = w - Ls;            // Listen: each pass replays the latest Length seconds
                if (loopMode == 1)
                {
                    const float e = clampf (env.tick (std::max (std::fabs (L[i]), std::fabs (R[i]))) * 12.0f, 0.0f, 1.0f);
                    envG += (lerp (1.0f, e, lmod) - envG) * kEnv;     // Env: the loop swells with your playing
                    ol *= envG; orr *= envG;
                }
            }
            float yl = wl[(size_t) i] + ol * 0.9f, yr = wr[(size_t) i] + orr * 0.9f;
            if (cstep < 1.0f)                                      // Clock: lower sample rate, gritty and dark
            {
                clk += cstep;
                if (clk >= 1.0f) { clk -= 1.0f; held[0] = yl; held[1] = yr; }
                yl = held[0]; yr = held[1];
                lpOut[0].setHz (float (sr) * cstep * 0.45f, sr); lpOut[1].a = lpOut[0].a;
                yl = lpOut[0].lp (yl); yr = lpOut[1].lp (yr);
            }
            L[i] = yl; R[i] = yr;
        }
        status[0] = hold ? 1.0f : 0.0f;
        meter = loopMode > 0 ? 0.5f + 0.5f * (hold ? 1.0f : 0.0f) : 0.0f;
    }
};

struct MicroverseFx final : Effect // granular / glitch / multi-delay algorithms around one shared buffer
{
    enum { Mosaic, Glide, Haze, Tunnel, Strum, Blocks, Interrupt, Arp, Pattern, Warp };
    struct Grain { bool on = false; double pos = 0; float rate = 1, rateEnd = 1; int len = 1, t = 0, delay = 0; float gl = 1, gr = 1, amp = 1; };
    static constexpr int NG = 24;
    std::vector<float> buf[2]; int mask = 0; int64_t w = 0; Grain gv[NG]; Rng rng { 777u };
    StepClock clk; int64_t lastStep = -1; float spawnAcc = 0; int64_t anchor = 0; int arpIdx = 0;
    EnvFollower fast, slow; int refr = 0; float last[2] = { 0, 0 }; Svf filt[2]; ReverbFx verb; std::vector<float> vl, vr;
    float wph = 0;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        int sz = 1; while (sz < int (10.0 * sr)) sz <<= 1;
        buf[0].assign ((size_t) sz, 0.0f); buf[1].assign ((size_t) sz, 0.0f); mask = sz - 1;
        fast.setup (0.5f, 30.0f, sr); slow.setup (30.0f, 300.0f, sr);
        verb.prepare (s, b); vl.assign ((size_t) b, 0.0f); vr.assign ((size_t) b, 0.0f);
        reset();
    }
    void reset() override
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f); std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        for (auto& g : gv) g.on = false;
        w = 0; lastStep = -1; last[0] = last[1] = 0; filt[0].reset(); filt[1].reset(); verb.reset();
    }
    inline float rd (int c, double idx) const
    {
        const double f = std::floor (idx); const int64_t i = (int64_t) f; const float t = float (idx - f);
        return buf[c][(size_t) (i & mask)] + t * (buf[c][(size_t) ((i + 1) & mask)] - buf[c][(size_t) (i & mask)]);
    }
    void spawn (double back, int len, float rate, float rateEnd, float amp, int delay = 0)
    {
        for (auto& g : gv)
            if (! g.on)
            {
                back = std::min (back, double (mask) - std::fabs (rate) * len - 64.0);
                g.on = true; g.t = 0; g.len = std::max (64, len); g.delay = delay;
                g.pos = double (w) - back + (rate < 0 ? -rate * len : 0.0);
                g.rate = rate; g.rateEnd = rateEnd; g.amp = amp;
                const float pan = 0.5f + 0.45f * rng.bi();
                g.gl = std::sqrt (1.0f - pan) * 1.41f; g.gr = std::sqrt (pan) * 1.41f;
                return;
            }
    }
    float pick (const float* set, int n) { return set[rng.next() % (uint32_t) n]; }
    void process (float* L, float* R, int n, const float* v, const Context& ctx) override
    {
        static const float timeBeats[6] = { 4.0f, 2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f };
        static const float mosaicR[4][3] = { { 1, 2, 1 }, { 1, 0.5f, 1 }, { 2, 4, 2 }, { 1, 2, -1 } };
        static const float chords[4][4] = { { 1, 1.26f, 1.5f, 2 }, { 1, 1.19f, 1.5f, 2 }, { 1, 1.5f, 2, 3 }, { 2, 1.5f, 1.26f, 1 } };
        static const float arps[4][6] = { { 1, 1.26f, 1.5f, 2, 1.5f, 1.26f }, { 1, 1.19f, 1.5f, 2, 1.5f, 1.19f }, { 1, 1.5f, 2, 3, 2, 1.5f }, { 0.5f, 1, 1.5f, 2, 3, 4 } };
        static const float patt[4][4] = { { 1, 2, 3, 4 }, { 0.75f, 1.5f, 2.25f, 3 }, { 1, 1.5f, 2.5f, 3 }, { 0.5f, 1.25f, 2, 3.25f } };
        const int algo = std::max (0, std::min (9, (int) v[0])), var = std::max (0, std::min (3, (int) v[1]));
        const double beats = timeBeats[std::max (0, std::min (5, (int) v[2]))];
        const float act = v[3] * 0.01f, shape = v[4] * 0.01f, rep = v[5] * 0.01f, flt = v[6] * 0.01f, space = v[7] * 0.01f;
        const double bpm = std::max (20.0, ctx.bpm);
        const int step = (int) std::min<double> (beats * 60.0 / bpm * sr, mask / 4);
        if (n > (int) vl.size()) return;
        // filter: left = low-pass, right = high-pass
        const float fc = flt < 0 ? 20000.0f * std::pow (0.02f, -flt) : 20.0f * std::pow (150.0f, flt);
        for (int c = 0; c < 2; ++c) filt[c].set (fc, 0.8f, sr);
        const float attack = 0.01f + 0.6f * std::pow (shape, 1.5f), decayPow = 1.0f + (1.0f - shape) * 2.5f;
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            buf[0][(size_t) (w & mask)] = inL + rep * last[0];
            buf[1][(size_t) (w & mask)] = inR + rep * last[1];
            ++w;
            const float m = std::max (std::fabs (inL), std::fabs (inR)) + 1.0e-7f;
            const float fd = gainToDb (fast.tick (m)), sd = gainToDb (slow.tick (m));
            if (refr > 0) --refr;
            const bool onset = fd - sd > 8.0f && fd > -50.0f && refr == 0;
            if (onset) { refr = int (0.1 * sr); anchor = w; }
            const int64_t st = clk.step (ctx, i, beats, sr);
            const bool tick = st != lastStep; lastStep = st;
            float yl = 0, yr = 0;
            switch (algo)
            {
                case Mosaic: if (tick) { const int k = 1 + (int) (act * 2.99f); for (int j = 0; j < k; ++j) { const float r = pick (mosaicR[var], 3); spawn (step * (1 + rng.next() % 4), step * (1 + (int) (rng.uni() * 1.5f)), r, r, 0.8f); } } break;
                case Glide:  if (tick) { static const float ge[4][2] = { { 1, 2 }, { 1, 0.5f }, { 0.5f, 2 }, { 2, 0.5f } }; float a = ge[var][0], b = ge[var][1]; if (var == 3 && rng.uni() < 0.5f) std::swap (a, b); spawn (step * 2, step * 2, a, b, 0.8f); } break;
                case Haze:
                {
                    spawnAcc += (8.0f + act * 52.0f) / float (sr);
                    if (spawnAcc >= 1.0f) { spawnAcc -= 1.0f; static const float hr[4][2] = { { 1, 1 }, { 1, 2 }, { 0.5f, 1 }, { 0.99f, 1.01f } }; const float r = var == 3 ? lerp (0.988f, 1.012f, rng.uni()) : pick (hr[var], 2); spawn (rng.uni() * 1.5 * sr + 256, int ((0.04f + 0.14f * rng.uni()) * sr), r, r, 1.1f); }
                    break;
                }
                case Tunnel:
                {
                    spawnAcc += (3.0f + act * 9.0f) / float (sr);
                    if (w - anchor > (int64_t) (step * 8)) anchor = w;
                    if (spawnAcc >= 1.0f) { spawnAcc -= 1.0f; static const float tr[4][3] = { { 1, 1, 1 }, { 0.5f, 0.5f, 0.5f }, { 1, 1.5f, 1 }, { 1, 2, 0.5f } }; const float r = pick (tr[var], 3); spawn (double (w - anchor) + 2000 + rng.uni() * 0.2 * sr, int ((0.3f + 0.6f * rng.uni()) * sr), r, r, 1.0f); }
                    break;
                }
                case Strum: if (onset) { const int k = 3 + (int) (act * 5.0f); for (int j = 0; j < k; ++j) spawn (int (0.02 * sr), int (0.35 * sr), chords[var][j % 4] * (j >= 4 ? 2.0f : 1.0f), chords[var][j % 4] * (j >= 4 ? 2.0f : 1.0f), 1.2f, j * std::max (1, step / 4)); } break;
                case Blocks: if (tick && rng.uni() < 0.3f + act * 0.7f) { static const float br[4] = { 1, -1, 2, 0 }; float r = br[var]; if (var == 3) { const float o[3] = { 1, -1, 2 }; r = pick (o, 3); } for (int j = 0; j < 1 + (int) (act * 3); ++j) spawn (step, step / (1 + j), r, r, 0.9f, j * step / (1 + j)); } break;
                case Interrupt: if (tick && rng.uni() < 0.35f + act * 0.6f) { const float r = var == 1 ? -1.0f : (var == 2 ? 0.5f : 1.0f); spawn (step * (1 + rng.next() % 8), step, var == 3 && rng.uni() < 0.5f ? -r : r, var == 3 && rng.uni() < 0.5f ? -r : r, 1.0f); } break;
                case Arp:    if (tick) { const float r = arps[var][arpIdx++ % 6]; spawn (step, step, r, r, 0.85f); } break;
                default: break;
            }
            if (algo == Pattern || algo == Warp)
            {
                // rhythmic multi-tap delay; Warp bends every tap in pitch like a stretched tape
                const int taps = 1 + (int) (act * 3.99f);
                wph += (0.15f + 0.2f * float (var)) / float (sr); if (wph >= 1) wph -= 1;
                for (int k = 0; k < taps; ++k)
                {
                    float d = std::min (float (mask - 64), float (step) * patt[var][k]);
                    if (algo == Warp) d += float (sr) * (0.004f + 0.003f * float (var)) * (1.0f + fastSin2Pi (wph + 0.25f * float (k)));
                    const float gk = std::pow (0.75f, float (k));
                    const float pan = (k & 1) ? 0.8f : 0.2f;
                    yl += rd (0, double (w) - d) * gk * (1.0f - pan) * 0.8f;
                    yr += rd (1, double (w) - d) * gk * pan * 0.8f;
                }
            }
            int active = 0;
            for (auto& g : gv)
            {
                if (! g.on) continue;
                if (g.delay > 0) { --g.delay; g.pos += 1.0; continue; }
                const float t = float (g.t) / float (g.len);
                const float e = t < attack ? t / attack : std::pow (std::max (0.0f, (1.0f - t) / (1.0f - attack)), decayPow);
                yl += rd (0, g.pos) * e * g.amp * g.gl; yr += rd (1, g.pos) * e * g.amp * g.gr;
                g.pos += (double) lerp (g.rate, g.rateEnd, t);
                if (++g.t >= g.len) g.on = false;
                ++active;
            }
            if (active > 1) { const float nrm = 1.0f / std::sqrt ((float) active * 0.5f); yl *= nrm; yr *= nrm; }
            if (algo < Pattern) { yl *= 1.5f; yr *= 1.5f; }
            filt[0].tick (yl); filt[1].tick (yr);
            yl = flt < 0 ? filt[0].lp : filt[0].hp; yr = flt < 0 ? filt[1].lp : filt[1].hp;
            last[0] = fastTanh (yl); last[1] = fastTanh (yr);
            vl[(size_t) i] = yl; vr[(size_t) i] = yr;
        }
        std::copy (vl.begin(), vl.begin() + n, L); std::copy (vr.begin(), vr.begin() + n, R);
        if (space > 0.0f)
        {
            const float rv[8] = { 75.0f, 0.6f + space * 6.0f, 45.0f, 10.0f, 50.0f, 0.0f, 150.0f, 130.0f };
            verb.process (vl.data(), vr.data(), n, rv, ctx);
            for (int i = 0; i < n; ++i) { L[i] = L[i] * (1.0f - 0.5f * space) + vl[(size_t) i] * space; R[i] = R[i] * (1.0f - 0.5f * space) + vr[(size_t) i] * space; }
        }
        meter = clampf (rep, 0, 1);
    }
};

} // namespace

std::unique_ptr<Effect> createEffect (int type)
{
    std::unique_ptr<Effect> e;
    switch (type)
    {
        case FxGate:       e.reset (new GateFx()); break;
        case FxCompressor: e.reset (new CompFx()); break;
        case FxTransient:  e.reset (new TransientFx()); break;
        case FxLimiter:    e.reset (new LimiterFx()); break;
        case FxSwell:      e.reset (new SwellFx()); break;
        case FxOverdrive:  e.reset (new OverdriveFx()); break;
        case FxDistortion: e.reset (new DistortionFx()); break;
        case FxAmp:        e.reset (new AmpFx()); break;
        case FxTape:       e.reset (new TapeFx()); break;
        case FxEq:         e.reset (new EqFx()); break;
        case FxAutoWah:    e.reset (new AutoWahFx()); break;
        case FxLadder:     e.reset (new LadderFx()); break;
        case FxTalk:       e.reset (new TalkFx()); break;
        case FxChorus:     e.reset (new ChorusFx()); break;
        case FxFlanger:    e.reset (new FlangerFx()); break;
        case FxPhaser:     e.reset (new PhaserFx()); break;
        case FxTremolo:    e.reset (new TremoloFx()); break;
        case FxVibe:       e.reset (new VibeFx()); break;
        case FxRotary:     e.reset (new RotaryFx()); break;
        case FxRingMod:    e.reset (new RingModFx()); break;
        case FxOctaver:    e.reset (new OctaverFx()); break;
        case FxPitch:      e.reset (new PitchFx()); break;
        case FxDelay:      e.reset (new DelayFx()); break;
        case FxReverb:     e.reset (new ReverbFx()); break;
        case FxSpring:     e.reset (new SpringFx()); break;
        case FxCrusher:    e.reset (new CrusherFx()); break;
        case FxGlitch:     e.reset (new GlitchFx()); break;
        case FxGranular:   e.reset (new GranularFx()); break;
        case FxImager:     e.reset (new ImagerFx()); break;
        case FxCabIR:      e.reset (new CabIrFx()); break;
        case FxNam:        e = createNamEffect(); break;
        case FxWah:        e.reset (new WahFx()); break;
        case FxFreeze:     e.reset (new FreezeFx()); break;
        case FxStepFilter: e.reset (new StepFilterFx()); break;
        case FxArp:        e.reset (new ArpFx()); break;
        case FxStrings:    e.reset (new StringsFx()); break;
        case FxSpiral:     e.reset (new SpiralFx()); break;
        case FxGhostVerb:  e.reset (new GhostVerbFx()); break;
        case FxFeedback:   e.reset (new FeedbackFx()); break;
        case FxTapEnsemble:e.reset (new TapEnsembleFx()); break;
        case FxSynth:      e.reset (new SynthFx()); break;
        case FxBarber:     e.reset (new BarberFx()); break;
        case FxRadio:      e.reset (new RadioFx()); break;
        case FxOrbit:      e.reset (new OrbitFx()); break;
        case FxMelt:       e.reset (new MeltFx()); break;
        case FxChaos:      e.reset (new ChaosFx()); break;
        case FxComp76:     e.reset (new Comp76Fx()); break;
        case FxOpto:       e.reset (new OptoFx()); break;
        case FxBusComp:    e.reset (new BusCompFx()); break;
        case FxPedalComp:  e.reset (new PedalCompFx()); break;
        case FxVariMu:     e.reset (new VariMuFx()); break;
        case FxMultiband:  e.reset (new MultibandFx()); break;
        case FxLooper:     e.reset (new LooperFx()); break;
        case FxFreqShift:  e.reset (new FreqShiftFx()); break;
        case FxWavefolder: e.reset (new WavefolderFx()); break;
        case FxUnison:     e.reset (new UnisonFx()); break;
        case FxBells:      e.reset (new BellsFx()); break;
        case FxVinyl:      e.reset (new VinylFx()); break;
        case FxUnderwater: e.reset (new UnderwaterFx()); break;
        case FxExciter:    e.reset (new ExciterFx()); break;
        case FxDoubler:    e.reset (new DoublerFx()); break;
        case FxAcoustic:   e.reset (new AcousticFx()); break;
        case FxPrism:      e.reset (new PrismFx()); break;
        case FxDream:      e.reset (new DreamFx()); break;
        case FxMicroverse: e.reset (new MicroverseFx()); break;
        default:           e.reset (new NoneFx()); type = FxNone; break;
    }
    e->type = type;
    return e;
}
} // namespace oc
