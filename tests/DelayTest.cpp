// OmniChain - behaviour test for the pedal-style Delay: echo timing per type, Twist stability,
// click-free time changes, CPU per type, and trails after bypass through the engine.
#include "../Source/dsp/Engine.h"
#include "../Source/dsp/Effects.h"
#include "../Source/dsp/DspCommon.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>
#include <xmmintrin.h>
#include <pmmintrin.h>

using namespace oc;

static int failures = 0;
static void check (bool ok, const char* what) { if (! ok) { std::printf ("  FAIL: %s\n", what); ++failures; } }

static const double sr = 48000.0;
static const int bs = 256;

// real-valued parameter vector for the Delay with defaults
static std::vector<float> defaults()
{
    std::vector<float> v (kParamsPerSlot, 0.0f);
    for (int k = 0; k < kParamsPerSlot; ++k) { const auto& p = getInfo (FxDelay).params[k]; v[(size_t) k] = p.name ? p.def : 0.0f; }
    return v;
}

int main()
{
    _mm_setcsr (_mm_getcsr() | 0x8040); // FTZ + DAZ, as in the plugin
    const auto& info = getInfo (FxDelay);
    const int numTypes = info.params[4].numChoices;
    std::printf ("Delay: %d types, max time %.0f ms\n", numTypes, info.params[0].max);
    check (numTypes == 15, "15 delay types");
    check (std::string (info.params[4].choices[0]) == "Digital" && std::string (info.params[4].choices[4]) == "Reverse", "old type indices unchanged");

    Context ctx; ctx.sampleRate = sr; ctx.bpm = 120;
    std::printf ("\n%-12s %9s %9s %9s %10s %9s\n", "type", "echo ms", "level", "twistPk", "ns/sample", "jump");
    for (int type = 0; type < numTypes; ++type)
    {
        auto fx = createEffect (FxDelay);
        fx->prepare (sr, bs);
        auto v = defaults();
        v[0] = 300; v[2] = 30; v[3] = 12000; v[4] = (float) type; v[5] = 50; v[6] = 0; v[7] = 0;

        // 1) impulse: first echo should land near 300 ms (reverse / glitch / multi-head are exceptions)
        const int len = int (sr * 1.5);
        std::vector<float> outL ((size_t) len);
        double ns = 0;
        for (int pos = 0; pos < len; pos += bs)
        {
            float L[bs] = {}, R[bs] = {};
            if (pos == 0) { L[0] = 1.0f; R[0] = 1.0f; }
            auto t0 = std::chrono::high_resolution_clock::now();
            fx->process (L, R, bs, v.data(), ctx);
            ns += std::chrono::duration<double, std::nano> (std::chrono::high_resolution_clock::now() - t0).count();
            for (int i = 0; i < bs && pos + i < len; ++i) outL[(size_t) (pos + i)] = L[i];
        }
        // energy-weighted arrival of the first echo inside 100..520 ms
        int firstIdx = -1; float firstPk = 0;
        for (int i = int (0.1 * sr); i < int (0.52 * sr); ++i) if (std::fabs (outL[(size_t) i]) > firstPk) { firstPk = std::fabs (outL[(size_t) i]); firstIdx = i; }
        const float echoMs = firstIdx * 1000.0f / float (sr);

        // 2) Twist held for 3 s on a sustained tone at full feedback: must stay finite and bounded
        auto fx2 = createEffect (FxDelay); fx2->prepare (sr, bs);
        v[2] = 100;
        fx2->command (1);
        float twPk = 0; double ph = 0; bool finite = true;
        for (int pos = 0; pos < int (sr * 3); pos += bs)
        {
            float L[bs], R[bs];
            for (int i = 0; i < bs; ++i) { ph += 220.0 / sr; L[i] = R[i] = 0.5f * (float) std::sin (6.2831853 * ph) * (pos < sr ? 1.0f : 0.0f); }
            fx2->process (L, R, bs, v.data(), ctx);
            for (int i = 0; i < bs; ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false; twPk = std::max (twPk, std::fabs (L[i])); }
        }
        fx2->command (2);

        // 3) time change while echoes ring (digital family crossfades, tape family glides): no big jumps
        auto fx3 = createEffect (FxDelay); fx3->prepare (sr, bs);
        v[2] = 50; v[0] = 400;
        float prev = 0, jump = 0; ph = 0;
        for (int pos = 0; pos < int (sr * 3); pos += bs)
        {
            if (pos >= int (sr * 1.5)) v[0] = 250;
            float L[bs], R[bs];
            for (int i = 0; i < bs; ++i) { ph += 330.0 / sr; const double tt = (pos + i) / sr; const float env = tt < 0.9 ? 1.0f : (tt < 1.0 ? float ((1.0 - tt) * 10.0) : 0.0f);
                                          L[i] = R[i] = 0.3f * env * (float) std::sin (6.2831853 * ph); }
            fx3->process (L, R, bs, v.data(), ctx);
            if (pos > int (sr * 1.2)) for (int i = 0; i < bs; ++i) { jump = std::max (jump, std::fabs (L[i] - prev)); prev = L[i]; }
            else prev = L[bs - 1];
        }

        const char* name = info.params[4].choices[type];
        std::printf ("%-12s %9.1f %9.3f %9.2f %10.1f %9.3f\n", name, echoMs, firstPk, twPk, ns / len, jump);
        char msg[128];
        if (type != 4 && type != 14 && type != 11)   // reverse / glitch / multi-head place their energy differently
        {
            const float tol = (type == 2 || type == 5 || type == 13) ? 12.0f : 3.0f;   // modulated types wander a little
            std::snprintf (msg, sizeof msg, "%s first echo at 300 ms (got %.1f)", name, echoMs);
            check (std::fabs (echoMs - 300.0f) < tol, msg);
        }
        std::snprintf (msg, sizeof msg, "%s echo audible", name);            check (firstPk > 0.05f || type == 4 || type == 9, msg);   // a one-sample click can fall between Lo-Fi sample-and-hold steps
        std::snprintf (msg, sizeof msg, "%s twist finite", name);            check (finite, msg);
        std::snprintf (msg, sizeof msg, "%s twist bounded (%.2f)", name, twPk); check (twPk < 2.5f, msg);
        std::snprintf (msg, sizeof msg, "%s time change smooth (%.3f)", name, jump); check (jump < 0.12f, msg);
        std::snprintf (msg, sizeof msg, "%s CPU (%.0f ns/sample)", name, ns / len); check (ns / len < 600.0, msg);
    }

    // 4) 10 second echo
    {
        auto fx = createEffect (FxDelay); fx->prepare (sr, bs);
        auto v = defaults(); v[0] = 10000; v[2] = 0; v[4] = 0; v[5] = 0;
        int hit = -1;
        for (int pos = 0; pos < int (sr * 10.5) && hit < 0; pos += bs)
        {
            float L[bs] = {}, R[bs] = {};
            if (pos == 0) L[0] = R[0] = 1.0f;
            fx->process (L, R, bs, v.data(), ctx);
            for (int i = 0; i < bs; ++i) if (std::fabs (L[i]) > 0.2f && pos + i > 100) { hit = pos + i; break; }
        }
        std::printf ("\n10 s delay: echo at %.3f s\n", hit / sr);
        check (std::fabs (hit / sr - 10.0) < 0.01, "10 s echo time");
    }

    // 5) trails through the engine: bypass leaves the echoes ringing, trails off cuts them
    for (int trailsOn = 1; trailsOn >= 0; --trailsOn)
    {
        Engine eng;
        std::atomic<float> type { (float) FxDelay }, byp { 0 }, mix { 50 }, lvl { 0 }, p[kParamsPerSlot], gin { 0 }, gout { 0 }, gmix { 100 };
        auto v = defaults(); v[0] = 250; v[2] = 40;
        for (int k = 0; k < kParamsPerSlot; ++k) { const auto& pi = info.params[k]; p[k] = pi.name ? paramToNorm (pi, v[(size_t) k]) : 0.0f; }
        SlotParamRefs r; r.type = &type; r.bypass = &byp; r.mix = &mix; r.level = &lvl;
        for (int k = 0; k < kParamsPerSlot; ++k) r.p[k] = &p[k];
        eng.setSlotParams (0, r);
        eng.setGlobalParams (&gin, &gout, &gmix);
        eng.trails = trailsOn != 0;
        eng.prepare (sr, bs);
        for (int i = 0; i < 20; ++i) eng.service();
        double ph = 0; float tailPk = 0, dryPk = 0;
        for (int pos = 0; pos < int (sr * 3); pos += bs)
        {
            if (pos % (bs * 8) == 0) eng.service();
            float L[bs], R[bs];
            for (int i = 0; i < bs; ++i) { ph += 440.0 / sr; const double tt = (pos + i) / sr; const float env = tt < 0.45 ? 1.0f : (tt < 0.5 ? float ((0.5 - tt) * 20.0) : 0.0f);
                                          L[i] = R[i] = 0.4f * env * (float) std::sin (6.2831853 * ph); }
            if (pos >= int (sr * 0.6)) byp = 1;            // bypass just after playing stops
            eng.process (L, R, bs, ctx);
            for (int i = 0; i < bs; ++i)
            {
                if (pos > int (sr * 0.75) && pos < int (sr * 1.6)) tailPk = std::max (tailPk, std::fabs (L[i]));
                if (pos > int (sr * 2.9)) dryPk = std::max (dryPk, std::fabs (L[i]));
            }
        }
        std::printf ("trails %s: tail after bypass %.3f\n", trailsOn ? "on " : "off", tailPk);
        if (trailsOn) check (tailPk > 0.05f, "trails keep echoes after bypass");
        else          check (tailPk < 1.0e-3f, "trails off cuts echoes on bypass");
        check (dryPk < 1.0e-3f, "silence after the tail");
    }

    std::printf ("\n%s (%d failures)\n", failures ? "FAILED" : "ALL OK", failures);
    return failures ? 1 : 0;
}
