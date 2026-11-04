// Offline stress test for the OmniChain engine (native build, no JUCE)
#include "../Source/dsp/Engine.h"
#include <chrono>
#include <cstdio>
#include <xmmintrin.h>
#include <pmmintrin.h>

using namespace oc;

struct Params
{
    std::atomic<float> type[kNumSlots], byp[kNumSlots], mix[kNumSlots], lvl[kNumSlots], p[kNumSlots][kParamsPerSlot];
    std::atomic<float> in { 0 }, out { 0 }, gmix { 100 };
};

static void signal (float* L, float* R, int n, int64_t& t, Rng& rng, double sr)
{
    for (int i = 0; i < n; ++i, ++t)
    {
        const double ts = double (t) / sr;
        // plucked-ish guitar: decaying 110Hz saw-ish + noise bursts, retriggered every 0.5s
        const double ph = std::fmod (ts, 0.5);
        const float env = float (std::exp (-ph * 6.0));
        float x = env * 0.5f * float (std::sin (k2Pi * 110.0 * ts) + 0.5 * std::sin (k2Pi * 220.0 * ts) + 0.25 * std::sin (k2Pi * 330.7 * ts));
        x += rng.bi() * 0.02f * env;
        L[i] = x; R[i] = x * 0.9f + rng.bi() * 0.005f;
    }
}

int main()
{
    _mm_setcsr (_mm_getcsr() | 0x8040); // FTZ + DAZ
    const double sr = 48000.0; const int bs = 256;
    int failures = 0;

    // ---------- 1. per-effect stability + CPU ----------
    std::printf ("%-20s %10s %10s %12s\n", "effect", "peak", "rms", "ns/sample");
    for (int type = 1; type < NumEffectTypes; ++type)
    {
        double worstNs = 0; float worstPeak = 0; double rmsAcc = 0; long cnt = 0;
        for (int trial = 0; trial < 6; ++trial)
        {
            Params P; Engine eng; Rng rng (uint32_t (type * 977 + trial * 13 + 1));
            for (int s = 0; s < kNumSlots; ++s)
            {
                P.type[s] = s == 0 ? float (type) : 0.0f; P.byp[s] = 0; P.mix[s] = 100; P.lvl[s] = 0;
                const auto& info = getInfo (s == 0 ? type : 0);
                for (int k = 0; k < kParamsPerSlot; ++k)
                {
                    float nv = info.params[k].name ? paramToNorm (info.params[k], info.params[k].def) : 0.0f;
                    if (trial > 0) nv = rng.uni();            // random settings
                    if (trial == 5) nv = (k % 2) ? 1.0f : 0.0f;  // extremes
                    P.p[s][k] = nv;
                }
                SlotParamRefs r; r.type = &P.type[s]; r.bypass = &P.byp[s]; r.mix = &P.mix[s]; r.level = &P.lvl[s];
                for (int k = 0; k < kParamsPerSlot; ++k) r.p[k] = &P.p[s][k];
                eng.setSlotParams (s, r);
            }
            eng.setGlobalParams (&P.in, &P.out, &P.gmix);
            eng.prepare (sr, bs);
            std::vector<float> L (bs), R (bs); int64_t t = 0;
            Context ctx; ctx.sampleRate = sr; ctx.bpm = 128; ctx.playing = (trial % 2) == 0;
            const int blocks = int (sr * 4 / bs);
            auto t0 = std::chrono::high_resolution_clock::now();
            float peak = 0;
            for (int b = 0; b < blocks; ++b)
            {
                signal (L.data(), R.data(), bs, t, rng, sr);
                eng.process (L.data(), R.data(), bs, ctx);
                ctx.ppq += bs * ctx.bpm / 60.0 / sr;
                for (int i = 0; i < bs; ++i)
                {
                    if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) { std::printf ("  NaN in %s trial %d\n", getInfo (type).name, trial); ++failures; b = blocks; break; }
                    peak = std::max (peak, std::max (std::fabs (L[i]), std::fabs (R[i])));
                    rmsAcc += L[i] * L[i]; ++cnt;
                }
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            const double ns = std::chrono::duration<double, std::nano> (t1 - t0).count() / double (blocks * bs);
            worstNs = std::max (worstNs, ns);
            worstPeak = std::max (worstPeak, peak);
        }
        std::printf ("%-20s %10.3f %10.4f %12.1f%s\n", getInfo (type).name, worstPeak, std::sqrt (rmsAcc / std::max (1L, cnt)), worstNs,
                     worstPeak > 8.0f ? "   <-- LOUD" : "");
        if (worstPeak > 32.0f) ++failures;
    }

    // ---------- 2. full chain + type switching + moves (threaded like a host) ----------
    {
        Params P; Engine eng; Rng rng (99);
        const int chain[kNumSlots] = { FxGate, FxCompressor, FxOverdrive, FxAmp, FxChorus, FxDelay, FxReverb, FxLimiter };
        for (int s = 0; s < kNumSlots; ++s)
        {
            P.type[s] = float (chain[s]); P.byp[s] = 0; P.mix[s] = getInfo (chain[s]).defaultMix * 100; P.lvl[s] = 0;
            for (int k = 0; k < kParamsPerSlot; ++k) { const auto& pi = getInfo (chain[s]).params[k]; P.p[s][k] = pi.name ? paramToNorm (pi, pi.def) : 0; }
            SlotParamRefs r; r.type = &P.type[s]; r.bypass = &P.byp[s]; r.mix = &P.mix[s]; r.level = &P.lvl[s];
            for (int k = 0; k < kParamsPerSlot; ++k) r.p[k] = &P.p[s][k];
            eng.setSlotParams (s, r);
        }
        eng.setGlobalParams (&P.in, &P.out, &P.gmix);
        eng.prepare (sr, bs);
        std::vector<float> L (bs), R (bs); int64_t t = 0; Context ctx; ctx.sampleRate = sr;
        float maxJump = 0, prev = 0, peak = 0; double ns = 0; int blocks = int (sr * 20 / bs);
        for (int b = 0; b < blocks; ++b)
        {
            if (b % 40 == 0) // control thread work every ~200ms of audio
            {
                const int s = int (rng.next() % kNumSlots);
                const int action = int (rng.next() % 4);
                if (action == 0) P.type[s] = float (1 + rng.next() % (NumEffectTypes - 1));
                else if (action == 1) P.byp[s] = P.byp[s] > 0.5f ? 0.0f : 1.0f;
                else if (action == 2)
                {
                    const int to = int (rng.next() % kNumSlots);
                    eng.requestMove (s, to);
                    // rotate params like the plugin does
                    auto rot = [&] (std::atomic<float>* arr) { std::vector<float> v; for (int i = 0; i < kNumSlots; ++i) v.push_back (arr[i]); float x = v[(size_t) s]; v.erase (v.begin() + s); v.insert (v.begin() + to, x); for (int i = 0; i < kNumSlots; ++i) arr[i] = v[(size_t) i]; };
                    rot (P.type); rot (P.byp); rot (P.mix); rot (P.lvl);
                    for (int k = 0; k < kParamsPerSlot; ++k) { std::atomic<float> tmp[kNumSlots]; for (int i = 0; i < kNumSlots; ++i) tmp[i] = P.p[i][k].load(); rot (tmp); for (int i = 0; i < kNumSlots; ++i) P.p[i][k] = tmp[i].load(); }
                }
                else for (int k = 0; k < kParamsPerSlot; ++k) P.p[s][k] = rng.uni();
            }
            if (b % 4 == 0) eng.service();
            signal (L.data(), R.data(), bs, t, rng, sr);
            auto t0 = std::chrono::high_resolution_clock::now();
            eng.process (L.data(), R.data(), bs, ctx);
            ns += std::chrono::duration<double, std::nano> (std::chrono::high_resolution_clock::now() - t0).count();
            for (int i = 0; i < bs; ++i)
            {
                if (! std::isfinite (L[i])) { std::printf ("NaN in chain test\n"); ++failures; break; }
                peak = std::max (peak, std::fabs (L[i]));
                maxJump = std::max (maxJump, std::fabs (L[i] - prev)); prev = L[i];
            }
        }
        std::printf ("\nchain test: peak %.2f, max sample jump %.3f, %.1f ns/sample for 8 slots (%.2f%% of one core @48k)\n",
                     peak, maxJump, ns / (blocks * bs), ns / (blocks * bs) * 48000.0 / 1e9 * 100.0);
    }
    std::printf ("\n%s (%d failures)\n", failures ? "FAILED" : "ALL OK", failures);
    return failures ? 1 : 0;
}
