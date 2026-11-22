#include "NamEffect.h"
#include "NAM/get_dsp.h"
#include "NAM/slimmable.h"
#include <chrono>
#include <fstream>

namespace oc
{
namespace
{
struct NamResource final : Resource
{
    nlohmann::json config;
    bool hasLoudness = false;
    double loudness = 0.0;
    double slimSize = 1.0;       // 0 = smallest size of a slimmable (A2) capture, 1 = full
};

void applySlim (nam::DSP& dsp, double size)
{
    if (auto* s = dynamic_cast<nam::SlimmableModel*> (&dsp)) s->SetSlimmableSize (size);
}

// Size class of a capture, read from its config (the names TONE3000 uses)
std::string sizeClass (const nlohmann::json& j, bool slimmable, double slim)
{
    if (slimmable) return slim < 0.5 ? "A2 Lite" : "A2 Full";
    try
    {
        const auto arch = j.value ("architecture", std::string());
        if (arch == "WaveNet")
        {
            const int ch = j.at ("config").at ("layers").at (0).value ("channels", 0);
            return ch >= 20 ? "large" : ch >= 16 ? "standard" : ch >= 12 ? "lite" : ch >= 8 ? "feather" : ch >= 1 ? "nano" : "WaveNet";
        }
        if (! arch.empty()) return arch;
    }
    catch (...) {}
    return "custom";
}

std::filesystem::path pathFromUtf8 (const std::string& s)
{
    return std::filesystem::path (std::u8string (s.begin(), s.end()));
}

inline float cubic (float xm1, float x0, float x1, float x2, float t)     // Catmull-Rom
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

struct NamFx final : Effect
{
    std::unique_ptr<nam::DSP> model;
    std::vector<float> in, out, xin, yq, mono, gcurve;
    Biquad bass, mid, treb, aaIn[2], aaOut[2];
    EnvFollower gateEnv; float gateG = 1.0f;
    float last[3] = { -99, -99, -99 };
    float normDb = 0.0f;
    bool hasLoud = false, resample = false;
    double ratio = 1.0, pIn = 0.0, pOut = 0.0;      // ratio = model rate / project rate
    int yCount = 0;

    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        model.reset();
        auto r = std::dynamic_pointer_cast<const NamResource> (resource);
        if (r == nullptr) return;
        const double modelRate = r->sampleRate > 0 ? r->sampleRate : 48000.0;
        ratio = modelRate / sr;
        resample = std::fabs (ratio - 1.0) > 1.0e-4;
        const int maxModel = resample ? (int) std::ceil (maxBlock * ratio) + 16 : maxBlock;
        try
        {
            nam::DspLoadOptions o; o.prewarm = false;
            model = nam::get_dsp (r->config, o);
            applySlim (*model, r->slimSize);
            model->ResetAndPrewarm (resample ? modelRate : sr, maxModel);
        }
        catch (...) { model.reset(); return; }
        in.assign ((size_t) maxModel, 0.0f);
        out.assign ((size_t) maxModel, 0.0f);
        xin.assign ((size_t) maxBlock + 4, 0.0f);
        mono.assign ((size_t) maxBlock, 0.0f); gcurve.assign ((size_t) maxBlock, 1.0f);
        yq.assign ((size_t) maxModel * 2 + 64, 0.0f);
        if (resample)
        {
            // the capture always runs at the rate it was trained at; the audio is converted in and out around it
            const float fc = 0.45f * (float) std::min (sr, modelRate);
            for (int k = 0; k < 2; ++k)
            {
                const float q = k == 0 ? 0.5412f : 1.3066f;
                aaIn[k].set (Biquad::LP, fc, q, 0, sr);
                aaOut[k].set (Biquad::LP, fc, q, 0, ratio > 1.0 ? modelRate : sr);
            }
        }
        hasLoud = r->hasLoudness;
        normDb = hasLoud ? (float) (-18.0 - r->loudness) : 0.0f;
        gateEnv.setup (1.0f, 60.0f, sr);
        for (auto& x : last) x = -99;
        reset();
    }

    void reset() override
    {
        bass.reset(); mid.reset(); treb.reset();
        for (int k = 0; k < 2; ++k) { aaIn[k].reset(); aaOut[k].reset(); }
        std::fill (xin.begin(), xin.end(), 0.0f); std::fill (yq.begin(), yq.end(), 0.0f);
        pIn = 0.0; pOut = 0.0; yCount = 6;           // a 6-sample cushion keeps the output interpolator fed
        gateEnv.reset(); gateG = 1.0f;
    }

    void runResampled (const float* x, float* y, int n)
    {
        // project rate -> model rate
        for (int i = 0; i < n; ++i) xin[(size_t) i + 3] = ratio < 1.0 ? aaIn[1].tick (aaIn[0].tick (x[i], 0), 0) : x[i];
        int m = 0;
        const double step = 1.0 / ratio;
        while (pIn < (double) n && m < (int) in.size())
        {
            const int j = (int) pIn; const float t = (float) (pIn - j);
            in[(size_t) m++] = cubic (xin[(size_t) j], xin[(size_t) j + 1], xin[(size_t) j + 2], xin[(size_t) j + 3], t);
            pIn += step;
        }
        pIn -= (double) n;
        xin[0] = xin[(size_t) n]; xin[1] = xin[(size_t) n + 1]; xin[2] = xin[(size_t) n + 2];
        if (m > 0)
        {
            float* ip = in.data(); float* op = out.data();
            model->process (&ip, &op, m);
            for (int k = 0; k < m && yCount < (int) yq.size(); ++k)
                yq[(size_t) yCount++] = ratio > 1.0 ? aaOut[1].tick (aaOut[0].tick (out[(size_t) k], 0), 0) : out[(size_t) k];
        }
        // model rate -> project rate
        for (int i = 0; i < n; ++i)
        {
            const int j = (int) pOut; const float t = (float) (pOut - j);
            float v = j + 3 < yCount ? cubic (yq[(size_t) j], yq[(size_t) j + 1], yq[(size_t) j + 2], yq[(size_t) j + 3], t)
                                     : yq[(size_t) std::max (0, yCount - 1)];
            if (j + 3 < yCount) pOut += ratio;
            y[i] = ratio < 1.0 ? aaOut[1].tick (aaOut[0].tick (v, 0), 0) : v;
        }
        const int used = std::min ((int) pOut, std::max (0, yCount - 4));
        if (used > 0)
        {
            std::memmove (yq.data(), yq.data() + used, sizeof (float) * (size_t) (yCount - used));
            yCount -= used; pOut -= (double) used;
        }
    }

    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        if (model == nullptr || n > maxBlock) { meter = 0; return; }
        if (v[2] != last[0]) { last[0] = v[2]; bass.set (Biquad::LowShelf, 120.0f, 0.7f, v[2], sr); }
        if (v[3] != last[1]) { last[1] = v[3]; mid.set (Biquad::Peak, 750.0f, 0.7f, v[3], sr); }
        if (v[4] != last[2]) { last[2] = v[4]; treb.set (Biquad::HighShelf, 3000.0f, 0.7f, v[4], sr); }
        const float inG = dbToGain (v[0]);
        const float outG = dbToGain (v[1] + ((v[5] > 0.5f && hasLoud) ? normDb : 0.0f));
        const bool gate = v[6] > -79.5f;
        const float gThr = v[6], kUp = 1.0f - onePoleCoef (2.0f, sr), kDn = 1.0f - onePoleCoef (90.0f, sr);

        float pk = 0;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            if (gate)
            {
                const float e = gainToDb (gateEnv.tick (m) + 1.0e-7f);
                const float tg = clampf ((e - gThr) / 6.0f + 1.0f, 0.0f, 1.0f);
                gateG += (tg - gateG) * (tg > gateG ? kUp : kDn);
            }
            else gateG = 1.0f;
            gcurve[(size_t) i] = gateG;
            mono[(size_t) i] = m * inG;
        }
        if (resample) runResampled (mono.data(), L, n);
        else
        {
            float* ip = mono.data(); float* op = L;
            model->process (&ip, &op, n);
        }
        for (int i = 0; i < n; ++i)
        {
            float y = treb.tick (mid.tick (bass.tick (L[i], 0), 0), 0) * outG * gcurve[(size_t) i];
            if (! std::isfinite (y)) y = 0.0f;
            L[i] = R[i] = y; pk = std::max (pk, std::fabs (y));
        }
        meter = std::min (1.0f, pk);
    }
};

// Older captures carry no loudness figure, so measure one: RMS output for a standard -20 dBFS test burst.
double measureLoudness (nam::DSP& dsp, double rate, float& cpuPct)
{
    const int bs = 512, total = (int) (rate * 0.7);
    dsp.ResetAndPrewarm (rate, bs);
    double busy = 0.0, timed = 0.0;
    std::vector<float> a ((size_t) bs), b ((size_t) bs);
    Rng rng (12345u); OnePole lp; lp.setHz (2500.0f, rate); OnePole hp; hp.setHz (90.0f, rate);
    double e = 0; long cnt = 0;
    for (int done = 0; done < total; done += bs)
    {
        for (int i = 0; i < bs; ++i)
        {
            const double t = (done + i) / rate;
            float x = hp.hp (lp.lp (rng.bi())) * 0.55f;                                   // guitar-band noise
            x += 0.07f * (float) std::sin (6.283185307 * 110.0 * t) + 0.05f * (float) std::sin (6.283185307 * 247.0 * t);
            a[(size_t) i] = x * 0.62f;                                                    // about -20 dBFS RMS
        }
        float* ip = a.data(); float* op = b.data();
        const auto t0 = std::chrono::steady_clock::now();
        dsp.process (&ip, &op, bs);
        if (done > (int) (rate * 0.1)) { busy += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count(); timed += bs / rate; }
        if (done > (int) (rate * 0.2)) for (int i = 0; i < bs; ++i) { e += (double) b[(size_t) i] * b[(size_t) i]; ++cnt; }
    }
    cpuPct = timed > 0.0 ? (float) (100.0 * busy / timed) : 0.0f;
    return 10.0 * std::log10 (e / (double) std::max (1L, cnt) + 1.0e-12);
}
} // namespace

ResourceLoadResult loadNamFile (const std::string& utf8Path, bool light)
{
    try
    {
        const auto fp = pathFromUtf8 (utf8Path);
        std::ifstream f (fp, std::ios::binary);
        if (! f) return { nullptr, "Could not open the file." };
        nlohmann::json j = nlohmann::json::parse (f);
        nam::DspLoadOptions o; o.prewarm = false;
        auto dsp = nam::get_dsp (j, o);   // validates version and weights
        if (dsp->NumInputChannels() != 1 || dsp->NumOutputChannels() != 1)
            return { nullptr, "Only mono-in / mono-out NAM models are supported." };
        auto r = std::make_shared<NamResource>();
        r->sampleRate = dsp->GetExpectedSampleRate() > 0 ? dsp->GetExpectedSampleRate() : 48000.0;
        r->hasLoudness = true;
        const bool slimmable = dynamic_cast<nam::SlimmableModel*> (dsp.get()) != nullptr;
        r->slimSize = (slimmable && light) ? 0.0 : 1.0;
        applySlim (*dsp, r->slimSize);
        // Always measure with the same test signal, so every capture lands at a similar volume
        // (loudness metadata in files is inconsistent between trainer versions, and older files have none).
        // The same run times the model, so the UI can show what it costs.
        r->loudness = measureLoudness (*dsp, r->sampleRate, r->cpuPct);
        r->detail = sizeClass (j, slimmable, r->slimSize);
        if (! std::isfinite (r->loudness) || r->loudness < -90.0) { r->hasLoudness = false; r->loudness = 0.0; }
        const auto u8 = fp.filename().u8string();
        r->name.assign (u8.begin(), u8.end());
        r->config = std::move (j);
        return { r, {} };
    }
    catch (const std::exception& e) { return { nullptr, std::string ("Not a valid NAM model: ") + e.what() }; }
    catch (...) { return { nullptr, "Not a valid NAM model." }; }
}

std::unique_ptr<Effect> createNamEffect() { return std::make_unique<NamFx>(); }
} // namespace oc
