// OmniChain - YIN pitch detector for the standalone tuner (runs on the GUI thread, not the audio thread)
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace oc
{
struct TunerReading
{
    float hz = 0.0f;        // 0 = no clear pitch
    float clarity = 0.0f;   // 0..1
    int midi = 0;
    float cents = 0.0f;     // -50..+50 from the nearest note
};

class YinTuner
{
public:
    // x: most recent mono samples (oldest first). Uses at most 4096 samples.
    TunerReading detect (const float* x, int n, double sampleRate)
    {
        TunerReading r;
        // decimate high sample rates to keep the cost flat
        int dec = sampleRate > 60000.0 ? (sampleRate > 120000.0 ? 4 : 2) : 1;
        const double sr = sampleRate / dec;
        buf.resize ((size_t) (n / dec));
        float peak = 0.0f;
        for (size_t i = 0; i < buf.size(); ++i)
        {
            float s = 0.0f;
            for (int k = 0; k < dec; ++k) s += x[i * (size_t) dec + (size_t) k];
            buf[i] = s / (float) dec;
            peak = std::max (peak, std::fabs (buf[i]));
        }
        if (peak < 0.003f) return r;    // too quiet

        const int N = (int) buf.size();
        const int maxLag = std::min (N / 2, (int) (sr / 38.0));
        const int minLag = std::max (2, (int) (sr / 1400.0));
        const int W = N - maxLag;
        if (W < 256 || maxLag <= minLag) return r;

        d.assign ((size_t) maxLag + 2, 0.0f);
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double s = 0.0;
            const float* a = buf.data();
            const float* b = buf.data() + tau;
            for (int j = 0; j < W; ++j) { const float df = a[j] - b[j]; s += df * df; }
            d[(size_t) tau] = (float) s;
        }
        // cumulative mean normalised difference
        double running = 0.0;
        d[0] = 1.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            running += d[(size_t) tau];
            d[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
        }
        int best = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
            if (d[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxLag && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {
            best = minLag;
            for (int tau = minLag; tau < maxLag; ++tau) if (d[(size_t) tau] < d[(size_t) best]) best = tau;
            if (d[(size_t) best] > 0.35f) return r;   // not a clear pitch
        }
        // parabolic interpolation
        float t = (float) best;
        if (best > 1 && best < maxLag)
        {
            const float y0 = d[(size_t) best - 1], y1 = d[(size_t) best], y2 = d[(size_t) best + 1];
            const float den = y0 - 2.0f * y1 + y2;
            if (std::fabs (den) > 1.0e-9f) t += 0.5f * (y0 - y2) / den;
        }
        r.hz = (float) (sr / t);
        r.clarity = std::max (0.0f, 1.0f - d[(size_t) best]);
        const float m = 69.0f + 12.0f * std::log2 (r.hz / 440.0f);
        r.midi = (int) std::lround (m);
        r.cents = (m - (float) r.midi) * 100.0f;
        return r;
    }

    static const char* noteName (int midi)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((midi % 12) + 12) % 12];
    }

private:
    std::vector<float> buf, d;
};
} // namespace oc
