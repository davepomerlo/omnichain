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
    float det = 0, g = 0; int hold = 0; bool open = false;
    void reset() override { det = 0; g = 0; hold = 0; open = false; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float thrO = dbToGain (v[0]), thrC = dbToGain (v[0] - v[5]);
        const float kA = 1.0f - onePoleCoef (v[1], sr), kR = 1.0f - onePoleCoef (v[3], sr);
        const int holdS = int (v[2] * 0.001f * float (sr));
        const float floorG = v[4] <= -79.5f ? 0.0f : dbToGain (v[4]);
        const float detRel = onePoleCoef (12.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float x = maxAbs (L[i], R[i]);
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
    float gr = 0, rms = 0;
    void reset() override { gr = 0; rms = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float thr = v[0], ratio = v[1], makeup = v[4], ch = v[5] * 0.01f;
        const float knee = 3.0f + ch * 9.0f;
        const float slope = 1.0f / ratio - 1.0f;
        const float aA = onePoleCoef (v[2], sr);
        const float aR1 = onePoleCoef (v[3], sr), aR2 = onePoleCoef (v[3] * 3.5f, sr);
        const float rmsC = onePoleCoef (12.0f, sr);
        const float satAmt = ch * 0.6f;
        for (int i = 0; i < n; ++i)
        {
            const float lvl = maxAbs (L[i], R[i]);
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
        return ceil * lerp (clampf (u, -1.0f, 1.0f), fastTanh (u * 1.3f) * 1.1605f, soft);
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
                l = clampf (clipper (l, ceil, soft), -ceil, ceil);
                r = clampf (clipper (r, ceil, soft), -ceil, ceil);
            }
            else { l = clampf (l, -ceil, ceil); r = clampf (r, -ceil, ceil); }
            L[i] = l; R[i] = r;
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
    OnePole preLP[2], tightHP[2], toneLP[2]; DcBlock dc[2]; Biquad hump;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (int c = 0; c < 2; ++c) { preLP[c].setHz (8000.0f, sr); dc[c].setup (sr); }
        hump.set (Biquad::Peak, 720.0f, 0.8f, 4.0f, sr);
        reset();
    }
    void reset() override { for (int c = 0; c < 2; ++c) { preLP[c].reset(); tightHP[c].reset(); toneLP[c].reset(); dc[c].reset(); } hump.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float drive = v[0] * 0.01f, tone = v[1] * 0.01f, clean = v[3] * 0.01f, asym = v[4] * 0.01f;
        const float outG = dbToGain (v[5]);
        const float gain = dbToGain (6.0f + drive * 40.0f);
        const float bias = asym * 0.4f, biasOff = fastTanh (bias);
        for (int c = 0; c < 2; ++c) { tightHP[c].setHz (v[2], sr); toneLP[c].setHz (600.0f * std::pow (20.0f, tone), sr); }
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                const float s = tightHP[c].hp (preLP[c].lp (x)) * gain;
                float y = fastTanh (s + bias) - biasOff;
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
    OnePole aaLP[2], inHP[2], toneLP[2], muffLP[2], muffHP[2]; DcBlock dc[2]; Biquad mids; EnvFollower env[2];
    float lastMid = -999;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (int c = 0; c < 2; ++c) { aaLP[c].setHz (9000.0f, sr); inHP[c].setHz (80.0f, sr); muffLP[c].setHz (700.0f, sr); muffHP[c].setHz (1300.0f, sr); dc[c].setup (sr); env[c].setup (1.0f, 60.0f, sr); }
        lastMid = -999; reset();
    }
    void reset() override { for (int c = 0; c < 2; ++c) { aaLP[c].reset(); inHP[c].reset(); toneLP[c].reset(); muffLP[c].reset(); muffHP[c].reset(); dc[c].reset(); env[c].reset(); } mids.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float g = v[0] * 0.01f, tone = v[1] * 0.01f, bias = v[3] * 0.01f, outG = dbToGain (v[5]);
        const int mode = (int) v[2];
        if (v[4] != lastMid) { mids.set (Biquad::Peak, 500.0f, 0.7f, v[4], sr); lastMid = v[4]; }
        float gain;
        switch (mode)
        {
            case 0:  gain = dbToGain (10.0f + g * 50.0f); break;
            case 1:  gain = dbToGain (6.0f + g * 30.0f); break;
            case 2:  gain = dbToGain (6.0f + g * 40.0f); break;
            default: gain = dbToGain (6.0f + g * 36.0f); break;
        }
        const float toneHz = mode == 0 ? 400.0f * std::pow (40.0f, tone) : 1500.0f * std::pow (7.0f, tone);
        for (int c = 0; c < 2; ++c) toneLP[c].setHz (toneHz, sr);
        const float b = -bias * 1.2f;
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = inHP[c].hp (aaLP[c].lp (ch[c][i]));
                const float e = env[c].tick (x);
                float y;
                switch (mode)
                {
                    case 0: { const float u = x * gain + b; y = fastTanh (u) * 0.6f + clampf (u, -1, 1) * 0.4f - (fastTanh (b) * 0.6f + clampf (b, -1, 1) * 0.4f); y = toneLP[c].lp (y); break; }
                    case 1:
                    {
                        float u = fastTanh (x * gain + b) - fastTanh (b);
                        u = fastTanh (u * gain * 0.35f + 0.05f);
                        y = lerp (muffLP[c].lp (u), muffHP[c].hp (u), tone) * 1.6f;
                        break;
                    }
                    case 2:
                    {
                        const float u = x * gain + b;
                        const float s = u > 0 ? fastTanh (u * 1.2f) : fastTanh (u * 0.6f) * 1.25f;
                        const float s0 = b > 0 ? fastTanh (b * 1.2f) : fastTanh (b * 0.6f) * 1.25f;
                        y = toneLP[c].lp (s - s0);
                        break;
                    }
                    default:
                    {
                        const float u = std::fabs (x) * 2.0f - e * 1.2f; // full-wave rectifier = octave up
                        y = toneLP[c].lp (fastTanh (u * gain + b) - fastTanh (b));
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
    OnePole inHP[2], s1LP[2], c1HP[2], s2LP[2], c2HP[2];
    Biquad bass, mid, treb, pres, cab[6];
    EnvFollower sag[2];
    int nCab = 0;
    float last[6] = { -1, -1, -1, -1, -1, -1 };
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (int c = 0; c < 2; ++c)
        {
            inHP[c].setHz (40.0f, sr); s1LP[c].setHz (12000.0f, sr); c1HP[c].setHz (30.0f, sr);
            s2LP[c].setHz (9000.0f, sr); c2HP[c].setHz (110.0f, sr); sag[c].setup (10.0f, 150.0f, sr);
        }
        for (auto& x : last) x = -1;
        reset();
    }
    void reset() override
    {
        for (int c = 0; c < 2; ++c) { inHP[c].reset(); s1LP[c].reset(); c1HP[c].reset(); s2LP[c].reset(); c2HP[c].reset(); sag[c].reset(); }
        bass.reset(); mid.reset(); treb.reset(); pres.reset(); for (auto& q : cab) q.reset();
    }
    static inline float tube (float x) { return fastTanh (x + 0.15f) - 0.148885f; }
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
        if (v[1] != last[1]) bass.set (Biquad::LowShelf, 100.0f, 0.7f, (v[1] - 5.0f) * 2.4f, sr);
        if (v[2] != last[2]) mid.set (Biquad::Peak, 650.0f, 0.8f, (v[2] - 5.0f) * 2.4f, sr);
        if (v[3] != last[3]) treb.set (Biquad::HighShelf, 2800.0f, 0.7f, (v[3] - 5.0f) * 2.4f, sr);
        if (v[4] != last[4]) pres.set (Biquad::HighShelf, 3800.0f, 0.7f, (v[4] - 5.0f) * 1.8f, sr);
        if (v[5] != last[5]) setCab ((int) v[5]);
        for (int k = 0; k < 6; ++k) last[k] = v[k];
        const float g = v[0] * 0.01f;
        const float pre1 = dbToGain (g * 24.0f) * 2.0f;
        const float pre2 = dbToGain (clampf ((g - 0.3f) / 0.7f, 0, 1) * 30.0f) * 1.5f;
        const float pre3 = dbToGain (clampf ((g - 0.6f) / 0.4f, 0, 1) * 18.0f) * 1.2f;
        const float outG = 0.3f / (1.0f + g * 0.6f);
        c2HP[0].setHz (30.0f + g * 90.0f, sr); c2HP[1].a = c2HP[0].a;
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = inHP[c].hp (ch[c][i]);
                float a = tube (x * pre1);
                a = c1HP[c].hp (s1LP[c].lp (a));
                float b = tube (a * pre2);
                b = c2HP[c].hp (s2LP[c].lp (b));
                float s = tube (b * pre3);
                s = treb.tick (mid.tick (bass.tick (s, c), c), c);
                const float e = sag[c].tick (s);
                const float pg = 1.0f / (1.0f + e * 0.5f * g);
                float p = fastTanh (s * 1.3f * pg);
                p = pres.tick (p, c);
                for (int k = 0; k < nCab; ++k) p = cab[k].tick (p, c);
                ch[c][i] = p * outG;
            }
    }
};

// =====================================================================================
struct TapeFx final : Effect
{
    DelayLine dl[2]; Biquad preE, deE, bump, hf; Rng rng { 1234u };
    float wowPh = 0, flPh = 0, drift = 0, driftTgt = 0; int driftCtr = 0;
    Smooth base; float dropG = 1; int dropLeft = 0; float dropDepth = 0;
    float lastTone = -1, lastAge = -1;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& d : dl) d.init (int (0.03 * sr));
        preE.set (Biquad::HighShelf, 3000.0f, 0.7f, 6.0f, sr);
        deE.set (Biquad::HighShelf, 3000.0f, 0.7f, -6.0f, sr);
        bump.set (Biquad::Peak, 90.0f, 1.0f, 2.5f, sr);
        base.setTime (200.0f, sr); base.reset (float (0.001 * sr) + 2.0f);
        lastTone = lastAge = -1;
        reset();
    }
    void reset() override { for (auto& d : dl) d.clear(); preE.reset(); deE.reset(); bump.reset(); hf.reset(); dropG = 1; dropLeft = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        const float g = dbToGain (v[0]), comp = std::pow (g, -0.6f);
        const float wow = v[1] * 0.01f, flut = v[2] * 0.01f, hiss = v[3] * 0.01f, tone = v[4] * 0.01f, age = v[5] * 0.01f;
        if (tone != lastTone || age != lastAge)
        {
            hf.set (Biquad::LP, 20000.0f * std::pow (0.3f, 1.0f - tone) * (1.0f - age * 0.5f), 0.7f, 0.0f, sr);
            lastTone = tone; lastAge = age;
        }
        const float fs = float (sr);
        const float wowD = wow * (0.0025f + age * 0.003f) * fs;
        const float flD = flut * 0.00012f * fs;
        base.tgt = 0.001f * fs + wowD + flD + 2.0f;
        const float wowInc = (0.55f + age * 0.3f) / fs, flInc = 7.3f / fs;
        const float dropKg = 1.0f - onePoleCoef (5.0f, sr);
        const float dropProb = age * 1.5f / fs;
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
                x = fastTanh (x * g * 0.8f) * comp * 1.25f;
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
    Biquad lc, lo, mi, hi, hc; float last[6] = { -1, -1, -1, -1, -1, -1 };
    bool useLc = false, useLo = false, useMi = false, useHi = false, useHc = false;
    void prepare (double s, int b) override { Effect::prepare (s, b); for (auto& x : last) x = -1; reset(); }
    void reset() override { lc.reset(); lo.reset(); mi.reset(); hi.reset(); hc.reset(); }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        bool changed = false;
        for (int k = 0; k < 6; ++k) if (v[k] != last[k]) { changed = true; last[k] = v[k]; }
        if (changed)
        {
            useLc = v[0] > 20.5f;  if (useLc) lc.set (Biquad::HP, v[0], 0.707f, 0, sr);
            useLo = std::fabs (v[1]) > 0.05f; if (useLo) lo.set (Biquad::LowShelf, 100.0f, 0.7f, v[1], sr);
            useMi = std::fabs (v[3]) > 0.05f; if (useMi) mi.set (Biquad::Peak, v[2], 0.9f, v[3], sr);
            useHi = std::fabs (v[4]) > 0.05f; if (useHi) hi.set (Biquad::HighShelf, 8000.0f, 0.7f, v[4], sr);
            useHc = v[5] < 19600.0f; if (useHc) hc.set (Biquad::LP, v[5], 0.707f, 0, sr);
        }
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
                ch[c][i] = x;
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
    float s[2][4] = {}; float ph = 0; Smooth cut;
    void prepare (double sm, int b) override { Effect::prepare (sm, b); cut.setTime (8.0f, sr); cut.reset (2000.0f); reset(); }
    void reset() override { for (auto& c : s) for (auto& x : c) x = 0; }
    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        cut.tgt = v[0];
        const float k = v[1] * 0.01f * 3.9f, drive = dbToGain (v[2]);
        const int mode = (int) v[3];
        const float inc = v[4] / float (sr), depth = v[5];
        const float comp = (mode <= 1 ? 1.0f + k * 0.35f : 1.0f) / std::sqrt (drive);
        const float nyq = float (sr) * 0.45f;
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            const float c0 = cut.next();
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = depth > 0 ? fastSin2Pi (ph + 0.25f * float (c)) : 0.0f;
                const float fc = clampf (c0 * std::exp2 (depth * lfo), 20.0f, nyq);
                const float g = fastTan (kPi * fc / float (sr));
                const float G = g / (1.0f + g), inv = 1.0f / (1.0f + g);
                float* st = s[c];
                const float S = (G * G * G * st[0] + G * G * st[1] + G * st[2] + st[3]) * inv;
                const float G4 = G * G * G * G;
                float u = (ch[c][i] * drive - k * S) / (1.0f + k * G4);
                u = fastTanh (u);
                float y[4]; float in = u;
                for (int p = 0; p < 4; ++p) { const float vv = (in - st[p]) * G; y[p] = vv + st[p]; st[p] = y[p] + vv; in = y[p]; }
                float out;
                switch (mode)
                {
                    case 0:  out = y[3]; break;
                    case 1:  out = y[1]; break;
                    case 2:  out = (y[0] - y[1]) * 3.0f; break;
                    default: out = u - 4 * y[0] + 6 * y[1] - 4 * y[2] + y[3]; break;
                }
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
    DelayLine dl[2]; Smooth time; OnePole tone[2], hp[2]; float ph = 0, fph = 0, rph = 0;
    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        for (auto& d : dl) d.init (int (2.1 * sr) + 256);
        time.setTime (120.0f, sr); time.reset (0.375f * float (sr));
        hp[0].setHz (90.0f, sr); hp[1].setHz (90.0f, sr);
        reset();
    }
    void reset() override { for (auto& d : dl) d.clear(); tone[0].reset(); tone[1].reset(); hp[0].reset(); hp[1].reset(); rph = 0; }
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
        const float fb = v[2] * 0.01f, mod = v[5] * 0.01f;
        const float toneHz = mode == 1 ? v[3] * 0.6f : v[3];
        tone[0].setHz (toneHz, sr); tone[1].setHz (toneHz, sr);
        const float modMs = mode == 2 ? 2.5f : (mode == 1 ? 1.2f : 0.6f);
        const float modS = mod * modMs * 0.001f * float (sr);
        const float flS = mode == 2 ? mod * 0.00008f * float (sr) : 0.0f;
        const float pinc = 0.5f / float (sr), finc = 6.5f / float (sr);
        const float maxD = float (dl[0].size() - 8);
        for (int i = 0; i < n; ++i)
        {
            ph += pinc; if (ph >= 1) ph -= 1;
            fph += finc; if (fph >= 1) fph -= 1;
            const float T = time.next();
            const float d = clampf (T + modS * (fastSin2Pi (ph) + 1.0f) + flS * fastSin2Pi (fph), 2.0f, maxD);
            if (mode == 3)
            {
                const float in = (L[i] + R[i]) * 0.5f;
                const float yl = dl[0].read (d), yr = dl[1].read (d);
                dl[0].push (in + fb * loopFx (yr, 0, 0));
                dl[1].push (fb * loopFx (yl, 1, 0));
                L[i] = yl; R[i] = yr;
            }
            else if (mode == 4)
            {
                // reverse: two crossfaded taps sweeping backwards through the last T samples
                const float W = std::max (T, 64.0f);
                rph += 2.0f / W; rph -= std::floor (rph);
                float p2 = rph + 0.5f; if (p2 >= 1) p2 -= 1;
                const float w1 = fastSin2Pi (rph * 0.5f), w2 = fastSin2Pi (p2 * 0.5f);
                float* ch[2] = { L, R };
                for (int c = 0; c < 2; ++c)
                {
                    const float y = dl[c].read (2.0f + rph * W) * w1 * w1 + dl[c].read (2.0f + p2 * W) * w2 * w2;
                    dl[c].push (ch[c][i] + fb * loopFx (y, c, 0));
                    ch[c][i] = y;
                }
            }
            else
            {
                float* ch[2] = { L, R };
                for (int c = 0; c < 2; ++c)
                {
                    const float y = dl[c].read (d);
                    dl[c].push (ch[c][i] + fb * loopFx (y, c, mode));
                    ch[c][i] = y;
                }
            }
        }
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
    GrainShifter shim; float lph[N] = {}; float shimSig = 0;
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
        shim.clear(); shimSig = 0;
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
            const float outL = (o[0] - o[1] + o[2] - o[3] + o[4] - o[5] + o[6] - o[7]) * 0.4f;
            const float outR = (o[0] + o[1] - o[2] - o[3] + o[4] + o[5] - o[6] - o[7]) * 0.4f;
            if (shimmer > 0) shimSig = shim.tick ((outL + outR) * 0.5f, 2.0f, win) * shimmer * 0.35f;
            else shimSig = 0;
            L[i] = outL; R[i] = outR;
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
        default:           e.reset (new NoneFx()); type = FxNone; break;
    }
    e->type = type;
    return e;
}
} // namespace oc
