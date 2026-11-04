#include "Engine.h"

namespace oc
{
Engine::~Engine()
{
    for (auto& s : slots)
    {
        delete s.active; s.active = nullptr;
        delete s.ready.exchange (nullptr);
    }
    while (rTail.load() != rHead.load()) { const int t = rTail.load(); delete retireBuf[t]; rTail.store ((t + 1) % kQ); }
}

int Engine::wantedType (int s) const
{
    if (refs[s].type == nullptr) return FxNone;
    const int t = (int) std::lround (refs[s].type->load (std::memory_order_relaxed));
    return (t < 0 || t >= NumEffectTypes) ? FxNone : t;
}

std::unique_ptr<Effect> Engine::build (int slot, int type)
{
    auto e = createEffect (type);
    if (typeUsesResource (type))
    {
        e->resource = slotRes[slot];
        e->resVer = slotResVer[slot].load();
    }
    e->prepare (sr, maxBlock);
    e->reset();
    return e;
}

void Engine::setSlotResource (int slot, std::shared_ptr<const Resource> r)
{
    std::lock_guard<std::mutex> lk (ctl);
    slotRes[slot] = std::move (r);
    slotResVer[slot].store (++resCounter, std::memory_order_release);
}

std::shared_ptr<const Resource> Engine::getSlotResource (int slot)
{
    std::lock_guard<std::mutex> lk (ctl);
    return slotRes[slot];
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    std::lock_guard<std::mutex> lk (ctl);
    sr = sampleRate;
    maxBlock = std::max (16, maxBlockSize);
    wetL.assign ((size_t) maxBlock, 0.0f); wetR.assign ((size_t) maxBlock, 0.0f);
    dryL.assign ((size_t) maxBlock, 0.0f); dryR.assign ((size_t) maxBlock, 0.0f);

    while (rTail.load() != rHead.load()) { const int t = rTail.load(); delete retireBuf[t]; rTail.store ((t + 1) % kQ); }
    mTail.store (mHead.load());

    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& s = slots[(size_t) i];
        delete s.active; s.active = nullptr;
        delete s.ready.exchange (nullptr);
        const int t = wantedType (i);
        auto e = build (i, t);
        s.activeResVer.store (e->resVer);
        s.active = e.release();
        s.activeType.store (t);
        s.fade = 1.0f;
        const bool b = refs[i].bypass && refs[i].bypass->load() > 0.5f;
        s.byp = b ? 1.0f : 0.0f;
        s.wasIdle = true;
        s.mix.setTime (20.0f, sr); s.mix.reset (refs[i].mix ? refs[i].mix->load() * 0.01f : 1.0f);
        s.lvl.setTime (20.0f, sr); s.lvl.reset (refs[i].level ? dbToGain (refs[i].level->load()) : 1.0f);
        const auto& info = getInfo (t);
        for (int k = 0; k < kParamsPerSlot; ++k)
            s.frozen[k] = info.params[k].name ? paramToReal (info.params[k], refs[i].p[k] ? refs[i].p[k]->load() : 0.0f) : 0.0f;
    }
    inG.setTime (20.0f, sr);  inG.reset (gIn ? dbToGain (gIn->load()) : 1.0f);
    outG.setTime (20.0f, sr); outG.reset (gOut ? dbToGain (gOut->load()) : 1.0f);
    gMixS.setTime (20.0f, sr); gMixS.reset (gMix ? gMix->load() * 0.01f : 1.0f);
}

void Engine::service()
{
    std::lock_guard<std::mutex> lk (ctl);
    if (sr <= 0.0) return;
    while (rTail.load (std::memory_order_acquire) != rHead.load (std::memory_order_acquire))
    {
        const int t = rTail.load();
        delete retireBuf[t]; retireBuf[t] = nullptr;
        rTail.store ((t + 1) % kQ, std::memory_order_release);
    }
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& s = slots[(size_t) i];
        const int want = wantedType (i);
        const uint32_t wantVer = wantedResVer (i, want);
        Effect* r = s.ready.load (std::memory_order_acquire);  // safe to inspect: only this thread deletes effects
        const bool inFlight = r != nullptr && r->type == want && r->resVer == wantVer;
        const bool activeOk = want == s.activeType.load (std::memory_order_acquire)
                           && wantVer == s.activeResVer.load (std::memory_order_acquire);
        if (! activeOk && ! inFlight)
            delete s.ready.exchange (build (i, want).release(), std::memory_order_acq_rel);
    }
}

void Engine::retire (Effect* e)
{
    if (e == nullptr) return;
    const int h = rHead.load (std::memory_order_relaxed);
    const int nh = (h + 1) % kQ;
    if (nh == rTail.load (std::memory_order_acquire)) return; // queue full: leak rather than free on audio thread
    retireBuf[h] = e;
    rHead.store (nh, std::memory_order_release);
}

void Engine::requestMove (int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= kNumSlots || to >= kNumSlots) return;
    const int h = mHead.load();
    const int nh = (h + 1) % 16;
    if (nh == mTail.load()) return;
    moveFrom[h] = from; moveTo[h] = to;
    {
        // control-side resources move immediately; the audio thread moves the live effects
        std::lock_guard<std::mutex> lk (ctl);
        auto rotate = [&] (auto& get, auto& set)
        {
            if (from < to) for (int i = from; i < to; ++i) { auto a = get (i); set (i, get (i + 1)); set (i + 1, a); }
            else           for (int i = from; i > to; --i) { auto a = get (i); set (i, get (i - 1)); set (i - 1, a); }
        };
        auto getR = [&] (int i) { return slotRes[i]; };
        auto setR = [&] (int i, std::shared_ptr<const Resource> v) { slotRes[i] = std::move (v); };
        auto getV = [&] (int i) { return slotResVer[i].load(); };
        auto setV = [&] (int i, uint32_t v) { slotResVer[i].store (v); };
        rotate (getR, setR);
        rotate (getV, setV);
    }
    mHead.store (nh, std::memory_order_release);
}

void Engine::swapRuntime (Slot& a, Slot& b)
{
    std::swap (a.active, b.active);
    const int ta = a.activeType.load(); a.activeType.store (b.activeType.load()); b.activeType.store (ta);
    const uint32_t va = a.activeResVer.load(); a.activeResVer.store (b.activeResVer.load()); b.activeResVer.store (va);
    std::swap (a.fade, b.fade); std::swap (a.byp, b.byp);
    std::swap (a.mix, b.mix); std::swap (a.lvl, b.lvl);
    for (int k = 0; k < kParamsPerSlot; ++k) std::swap (a.frozen[k], b.frozen[k]);
    std::swap (a.wasIdle, b.wasIdle);
    const float m = a.meter.load(); a.meter.store (b.meter.load()); b.meter.store (m);
}

void Engine::applyMove (int from, int to)
{
    if (from < to) for (int i = from; i < to; ++i) swapRuntime (slots[(size_t) i], slots[(size_t) i + 1]);
    else           for (int i = from; i > to; --i) swapRuntime (slots[(size_t) i], slots[(size_t) i - 1]);
}

void Engine::process (float* L, float* R, int numSamples, const Context& ctx)
{
    while (mTail.load (std::memory_order_acquire) != mHead.load (std::memory_order_acquire))
    {
        const int t = mTail.load();
        applyMove (moveFrom[t], moveTo[t]);
        mTail.store ((t + 1) % 16, std::memory_order_release);
    }
    if (maxBlock <= 0) return;
    Context c = ctx;
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - done);
        processChunk (L + done, R + done, n, c);
        c.ppq += double (n) * c.bpm / (60.0 * c.sampleRate);
        done += n;
    }
}

void Engine::processChunk (float* L, float* R, int n, const Context& ctx)
{
    // input gain + global dry copy
    inG.tgt = gIn ? dbToGain (gIn->load (std::memory_order_relaxed)) : 1.0f;
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = inG.next();
        L[i] *= g; R[i] *= g;
        dryL[(size_t) i] = L[i]; dryR[(size_t) i] = R[i];
        pk = std::max (pk, std::max (std::fabs (L[i]), std::fabs (R[i])));
    }
    if (pk > inPeak.load (std::memory_order_relaxed)) inPeak.store (pk, std::memory_order_relaxed);

    const float fadeStep = 1.0f / (0.006f * float (sr));
    const float bypStep  = 1.0f / (0.010f * float (sr));
    float* wl = wetL.data(); float* wr = wetR.data();

    for (int si = 0; si < kNumSlots; ++si)
    {
        auto& s = slots[(size_t) si];
        const auto& rf = refs[si];
        const int want = wantedType (si);

        // swap in a freshly built effect once the old one has faded out
        if (s.fade <= 0.0f || s.active == nullptr)
            if (Effect* r = s.ready.exchange (nullptr, std::memory_order_acq_rel))
            {
                retire (s.active);
                s.active = r;
                s.activeResVer.store (r->resVer, std::memory_order_release);
                s.activeType.store (r->type, std::memory_order_release);
                s.fade = 0.0f;
                s.wasIdle = false;
            }

        Effect* fx = s.active;
        if (fx == nullptr) continue;
        const bool match = s.activeType.load (std::memory_order_relaxed) == want
                        && s.activeResVer.load (std::memory_order_relaxed) == wantedResVer (si, want);
        const float fadeTgt = match ? 1.0f : 0.0f;
        const bool bypassed = rf.bypass && rf.bypass->load (std::memory_order_relaxed) > 0.5f;
        const float bypTgt = bypassed ? 1.0f : 0.0f;
        s.mix.tgt = rf.mix ? clampf (rf.mix->load (std::memory_order_relaxed) * 0.01f, 0.0f, 1.0f) : 1.0f;
        s.lvl.tgt = rf.level ? dbToGain (rf.level->load (std::memory_order_relaxed)) : 1.0f;

        if (fx->type == FxNone) { s.fade = fadeTgt; s.byp = bypTgt; s.meter.store (0.0f, std::memory_order_relaxed); continue; }

        const bool idle = (s.fade <= 0.0f && fadeTgt <= 0.0f) || (s.byp >= 1.0f && bypassed);
        if (idle) { s.wasIdle = true; s.meter.store (0.0f, std::memory_order_relaxed); if (! match) s.fade = 0.0f; continue; }
        if (s.wasIdle) { fx->reset(); s.wasIdle = false; }

        const auto& info = getInfo (fx->type);
        if (match)
            for (int k = 0; k < kParamsPerSlot; ++k)
                if (info.params[k].name && rf.p[k])
                    s.frozen[k] = paramToReal (info.params[k], rf.p[k]->load (std::memory_order_relaxed));

        std::memcpy (wl, L, sizeof (float) * (size_t) n);
        std::memcpy (wr, R, sizeof (float) * (size_t) n);
        fx->process (wl, wr, n, s.frozen, ctx);

        // safety net: a misbehaving effect never reaches the speakers
        float chk = 0.0f;
        for (int i = 0; i < n; i += 7) chk += wl[i] + wr[i];
        if (! std::isfinite (chk) || ! std::isfinite (wl[n - 1]) || ! std::isfinite (wr[n - 1]))
        {
            fx->reset();
            std::memcpy (wl, L, sizeof (float) * (size_t) n);
            std::memcpy (wr, R, sizeof (float) * (size_t) n);
        }

        for (int i = 0; i < n; ++i)
        {
            s.fade = s.fade < fadeTgt ? std::min (fadeTgt, s.fade + fadeStep) : std::max (fadeTgt, s.fade - fadeStep);
            s.byp  = s.byp  < bypTgt  ? std::min (bypTgt,  s.byp  + bypStep)  : std::max (bypTgt,  s.byp  - bypStep);
            const float m = s.mix.next() * s.fade * (1.0f - s.byp);
            const float lv = s.lvl.next();
            L[i] += m * (wl[i] * lv - L[i]);
            R[i] += m * (wr[i] * lv - R[i]);
        }
        s.meter.store (fx->meter, std::memory_order_relaxed);
    }

    // global mix + output gain
    gMixS.tgt = gMix ? clampf (gMix->load (std::memory_order_relaxed) * 0.01f, 0.0f, 1.0f) : 1.0f;
    outG.tgt = gOut ? dbToGain (gOut->load (std::memory_order_relaxed)) : 1.0f;
    pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = gMixS.next(), g = outG.next();
        L[i] = (dryL[(size_t) i] + m * (L[i] - dryL[(size_t) i])) * g;
        R[i] = (dryR[(size_t) i] + m * (R[i] - dryR[(size_t) i])) * g;
        pk = std::max (pk, std::max (std::fabs (L[i]), std::fabs (R[i])));
    }
    if (pk > outPeak.load (std::memory_order_relaxed)) outPeak.store (pk, std::memory_order_relaxed);
}
} // namespace oc
