// OmniChain - zero-latency partitioned convolution (header-only, no dependencies)
//
// y = head FIR (first B taps, time domain, zero latency)
//   + tail (remaining taps, uniformly partitioned overlap-save FFT with a frequency-domain delay line).
// The tail's natural one-block delay lines up exactly with the B taps handled by the head, so the
// combined response is the full impulse response with no added latency.
// Left and right are transformed together as one complex signal (re = L, im = R): one FFT and one
// inverse FFT per block for both channels.
#pragma once
#include "DspCommon.h"

namespace oc
{
class Fft
{
public:
    void init (int size)
    {
        n = size;
        cosT.resize ((size_t) n / 2); sinT.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
        {
            const double a = -2.0 * 3.14159265358979323846 * k / n;
            cosT[(size_t) k] = (float) std::cos (a); sinT[(size_t) k] = (float) std::sin (a);
        }
        rev.resize ((size_t) n);
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
    }
    // in-place complex FFT; inverse is unscaled
    void run (float* re, float* im, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
        {
            const int r = rev[(size_t) i];
            if (r > i) { std::swap (re[i], re[r]); std::swap (im[i], im[r]); }
        }
        const float sgn = inverse ? -1.0f : 1.0f;
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const float wr = cosT[(size_t) (j * step)], wi = sgn * sinT[(size_t) (j * step)];
                    const int a = i + j, b = a + half;
                    const float vr = re[b] * wr - im[b] * wi;
                    const float vi = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - vr; im[b] = im[a] - vi;
                    re[a] += vr;        im[a] += vi;
                }
        }
    }
    int size() const { return n; }

private:
    int n = 0;
    std::vector<float> cosT, sinT;
    std::vector<int> rev;
};

class PartitionedConvolver
{
public:
    static constexpr int B = 128;          // block / head size
    static constexpr int N = 2 * B;        // FFT size
    static constexpr int K = B + 1;        // bins stored per channel

    // hL / hR: impulse responses of equal length (pass hR == hL for a mono IR). Not real-time safe.
    void setup (const float* hL, const float* hR, int len)
    {
        fft.init (N);
        len = std::max (len, 1);
        headL.assign (B, 0.0f); headR.assign (B, 0.0f);
        for (int j = 0; j < B; ++j)
        {
            const int m = B - 1 - j;   // reversed so the dot product runs oldest -> newest
            headL[(size_t) j] = m < len ? hL[m] : 0.0f;
            headR[(size_t) j] = m < len ? hR[m] : 0.0f;
        }
        P = std::max (0, (len - B + B - 1) / B);
        Pact = P;
        const size_t tot = (size_t) P * K;
        HLr.assign (tot, 0); HLi.assign (tot, 0); HRr.assign (tot, 0); HRi.assign (tot, 0);
        XLr.assign (tot, 0); XLi.assign (tot, 0); XRr.assign (tot, 0); XRi.assign (tot, 0);
        std::vector<float> re ((size_t) N), im ((size_t) N);
        for (int p = 0; p < P; ++p)
        {
            for (int i = 0; i < N; ++i)
            {
                const int idx = B + p * B + i;
                const bool ok = i < B && idx < len;
                re[(size_t) i] = ok ? hL[idx] : 0.0f;    // pack L into re and R into im, split afterwards
                im[(size_t) i] = ok ? hR[idx] : 0.0f;
            }
            fft.run (re.data(), im.data(), false);
            splitInto (re.data(), im.data(), &HLr[(size_t) p * K], &HLi[(size_t) p * K], &HRr[(size_t) p * K], &HRi[(size_t) p * K]);
        }
        histL.assign (2 * B, 0); histR.assign (2 * B, 0);
        frL.assign (N, 0); frR.assign (N, 0);
        tailL.assign (B, 0); tailR.assign (B, 0);
        wr.assign (N, 0); wi.assign (N, 0);
        aLr.assign (K, 0); aLi.assign (K, 0); aRr.assign (K, 0); aRi.assign (K, 0);
        reset();
    }

    void reset()
    {
        std::fill (histL.begin(), histL.end(), 0.0f); std::fill (histR.begin(), histR.end(), 0.0f);
        std::fill (frL.begin(), frL.end(), 0.0f); std::fill (frR.begin(), frR.end(), 0.0f);
        std::fill (tailL.begin(), tailL.end(), 0.0f); std::fill (tailR.begin(), tailR.end(), 0.0f);
        std::fill (XLr.begin(), XLr.end(), 0.0f); std::fill (XLi.begin(), XLi.end(), 0.0f);
        std::fill (XRr.begin(), XRr.end(), 0.0f); std::fill (XRi.begin(), XRi.end(), 0.0f);
        w = 0; pos = 0; fdl = 0;
    }

    void setActiveLength (int samples) { Pact = clampInt ((samples - B + B - 1) / B, 0, P); }
    int totalPartitions() const { return P; }

    void process (float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i], xr = R[i];
            histL[(size_t) w] = histL[(size_t) (w + B)] = xl;
            histR[(size_t) w] = histR[(size_t) (w + B)] = xr;
            const float* hl = &histL[(size_t) (w + 1)];
            const float* hr = &histR[(size_t) (w + 1)];
            float yl = 0.0f, yr = 0.0f;
            for (int j = 0; j < B; ++j) { yl += headL[(size_t) j] * hl[j]; yr += headR[(size_t) j] * hr[j]; }
            w = (w + 1) & (B - 1);
            frL[(size_t) (B + pos)] = xl; frR[(size_t) (B + pos)] = xr;
            L[i] = yl + tailL[(size_t) pos];
            R[i] = yr + tailR[(size_t) pos];
            if (++pos == B) { pos = 0; block(); }
        }
    }

private:
    static int clampInt (int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // separate the spectra of two real signals packed as re/im of one complex FFT
    static void splitInto (const float* re, const float* im, float* lr, float* li, float* rr, float* ri)
    {
        for (int k = 0; k < K; ++k)
        {
            const int m = (N - k) & (N - 1);
            lr[k] = 0.5f * (re[k] + re[m]);  li[k] = 0.5f * (im[k] - im[m]);
            rr[k] = 0.5f * (im[k] + im[m]);  ri[k] = -0.5f * (re[k] - re[m]);
        }
    }

    void block()
    {
        if (P > 0 && Pact > 0)
        {
            std::copy (frL.begin(), frL.end(), wr.begin());
            std::copy (frR.begin(), frR.end(), wi.begin());
            fft.run (wr.data(), wi.data(), false);
            const size_t s = (size_t) fdl * K;
            splitInto (wr.data(), wi.data(), &XLr[s], &XLi[s], &XRr[s], &XRi[s]);

            std::fill (aLr.begin(), aLr.end(), 0.0f); std::fill (aLi.begin(), aLi.end(), 0.0f);
            std::fill (aRr.begin(), aRr.end(), 0.0f); std::fill (aRi.begin(), aRi.end(), 0.0f);
            for (int p = 0; p < Pact; ++p)
            {
                int xi = fdl - p; if (xi < 0) xi += P;
                const float* xlr = &XLr[(size_t) xi * K]; const float* xli = &XLi[(size_t) xi * K];
                const float* xrr = &XRr[(size_t) xi * K]; const float* xri = &XRi[(size_t) xi * K];
                const float* hlr = &HLr[(size_t) p * K];  const float* hli = &HLi[(size_t) p * K];
                const float* hrr = &HRr[(size_t) p * K];  const float* hri = &HRi[(size_t) p * K];
                for (int k = 0; k < K; ++k)
                {
                    aLr[(size_t) k] += xlr[k] * hlr[k] - xli[k] * hli[k];
                    aLi[(size_t) k] += xlr[k] * hli[k] + xli[k] * hlr[k];
                    aRr[(size_t) k] += xrr[k] * hrr[k] - xri[k] * hri[k];
                    aRi[(size_t) k] += xrr[k] * hri[k] + xri[k] * hrr[k];
                }
            }
            // recombine Y = YL + i*YR over the full spectrum (YL, YR are Hermitian)
            for (int k = 0; k < K; ++k)
            {
                wr[(size_t) k] = aLr[(size_t) k] - aRi[(size_t) k];
                wi[(size_t) k] = aLi[(size_t) k] + aRr[(size_t) k];
            }
            for (int k = K; k < N; ++k)
            {
                const size_t m = (size_t) (N - k);
                wr[(size_t) k] = aLr[m] + aRi[m];
                wi[(size_t) k] = -aLi[m] + aRr[m];
            }
            fft.run (wr.data(), wi.data(), true);
            const float sc = 1.0f / (float) N;
            for (int j = 0; j < B; ++j) { tailL[(size_t) j] = wr[(size_t) (B + j)] * sc; tailR[(size_t) j] = wi[(size_t) (B + j)] * sc; }
            fdl = (fdl + 1) % P;
        }
        else
        {
            std::fill (tailL.begin(), tailL.end(), 0.0f); std::fill (tailR.begin(), tailR.end(), 0.0f);
        }
        std::copy (frL.begin() + B, frL.end(), frL.begin());   // current block becomes previous
        std::copy (frR.begin() + B, frR.end(), frR.begin());
    }

    Fft fft;
    int P = 0, Pact = 0, w = 0, pos = 0, fdl = 0;
    std::vector<float> headL, headR, histL, histR, frL, frR, tailL, tailR, wr, wi;
    std::vector<float> HLr, HLi, HRr, HRi, XLr, XLi, XRr, XRi, aLr, aLi, aRr, aRi;
};
} // namespace oc
