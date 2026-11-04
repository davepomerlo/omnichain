#include "NamEffect.h"
#include "NAM/get_dsp.h"
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
};

std::filesystem::path pathFromUtf8 (const std::string& s)
{
    return std::filesystem::path (std::u8string (s.begin(), s.end()));
}

struct NamFx final : Effect
{
    std::unique_ptr<nam::DSP> model;
    std::vector<float> in, out;
    Biquad bass, mid, treb;
    float last[3] = { -99, -99, -99 };
    float normDb = 0.0f;
    bool hasLoud = false;

    void prepare (double s, int b) override
    {
        Effect::prepare (s, b);
        model.reset();
        auto r = std::dynamic_pointer_cast<const NamResource> (resource);
        if (r == nullptr) return;
        try
        {
            nam::DspLoadOptions o; o.prewarm = false;
            model = nam::get_dsp (r->config, o);
            model->ResetAndPrewarm (sr, maxBlock);
        }
        catch (...) { model.reset(); return; }
        in.assign ((size_t) maxBlock, 0.0f);
        out.assign ((size_t) maxBlock, 0.0f);
        hasLoud = r->hasLoudness;
        normDb = hasLoud ? (float) (-18.0 - r->loudness) : 0.0f;
        for (auto& x : last) x = -99;
        reset();
    }

    void reset() override { bass.reset(); mid.reset(); treb.reset(); }

    void process (float* L, float* R, int n, const float* v, const Context&) override
    {
        if (model == nullptr || n > (int) in.size()) { meter = 0; return; }
        if (v[2] != last[0]) { last[0] = v[2]; bass.set (Biquad::LowShelf, 120.0f, 0.7f, v[2], sr); }
        if (v[3] != last[1]) { last[1] = v[3]; mid.set (Biquad::Peak, 750.0f, 0.7f, v[3], sr); }
        if (v[4] != last[2]) { last[2] = v[4]; treb.set (Biquad::HighShelf, 3000.0f, 0.7f, v[4], sr); }
        const float inG = dbToGain (v[0]);
        const float outG = dbToGain (v[1] + ((v[5] > 0.5f && hasLoud) ? normDb : 0.0f));
        for (int i = 0; i < n; ++i) in[(size_t) i] = 0.5f * (L[i] + R[i]) * inG;
        float* ip = in.data(); float* op = out.data();
        model->process (&ip, &op, n);
        float pk = 0;
        for (int i = 0; i < n; ++i)
        {
            float y = op[i];
            y = treb.tick (mid.tick (bass.tick (y, 0), 0), 0) * outG;
            if (! std::isfinite (y)) y = 0.0f;
            L[i] = R[i] = y;
            pk = std::max (pk, std::fabs (y));
        }
        meter = std::min (1.0f, pk);
    }
};
} // namespace

ResourceLoadResult loadNamFile (const std::string& utf8Path)
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
        r->hasLoudness = dsp->HasLoudness();
        r->loudness = r->hasLoudness ? dsp->GetLoudness() : 0.0;
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
