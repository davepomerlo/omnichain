// OmniChain - modular 8-slot effect chain engine (no JUCE dependency)
//
// Threading model
//  * Audio thread: process(). Never allocates, never locks.
//  * Control thread (message thread timer): service(). Builds effect objects for slots whose
//    Type parameter changed, hands them to the audio thread through an atomic pointer, and
//    deletes effects the audio thread has retired.
//  * prepare() is called while audio is stopped and builds everything synchronously.
#pragma once
#include "Effects.h"
#include <atomic>
#include <mutex>
#include <array>

namespace oc
{
constexpr int kNumSlots = 8;

struct SlotParamRefs
{
    std::atomic<float>* type = nullptr;    // choice index
    std::atomic<float>* bypass = nullptr;  // 0 / 1
    std::atomic<float>* mix = nullptr;     // 0..100 %
    std::atomic<float>* level = nullptr;   // dB
    std::atomic<float>* p[kParamsPerSlot] = {};  // normalised 0..1
};

class Engine
{
public:
    Engine() = default;
    ~Engine();

    void setSlotParams (int slot, const SlotParamRefs& r) { refs[slot] = r; }
    void setGlobalParams (std::atomic<float>* inDb, std::atomic<float>* outDb, std::atomic<float>* mixPct) { gIn = inDb; gOut = outDb; gMix = mixPct; }

    void prepare (double sampleRate, int maxBlockSize);
    void service();
    void process (float* L, float* R, int numSamples, const Context& ctx);

    // Move slot 'from' so it ends up at index 'to' (insert semantics). Call BEFORE the host
    // parameters are rewritten so effect state (delay tails etc.) travels with the slot.
    void requestMove (int from, int to);

    // File-based effects (Cab IR, NAM): control thread only. The slot's effect is rebuilt with the
    // new data off the audio thread and crossfaded in. Resources travel with the slot on moves.
    void setSlotResource (int slot, std::shared_ptr<const Resource> r);
    std::shared_ptr<const Resource> getSlotResource (int slot);

    float getSlotMeter (int s) const { return slots[(size_t) s].meter.load (std::memory_order_relaxed); }
    float takeInPeak()  { return inPeak.exchange (0.0f); }
    float takeOutPeak() { return outPeak.exchange (0.0f); }
    double getSampleRate() const { return sr; }

private:
    struct Slot
    {
        Effect* active = nullptr;
        std::atomic<Effect*> ready { nullptr };
        std::atomic<int> activeType { 0 };
        std::atomic<uint32_t> activeResVer { 0 };
        float fade = 0.0f, byp = 1.0f;
        Smooth mix, lvl;
        float frozen[kParamsPerSlot] = {};
        bool wasIdle = true;
        std::atomic<float> meter { 0.0f };
    };

    void processChunk (float* L, float* R, int n, const Context& ctx);
    void retire (Effect* e);
    void applyMove (int from, int to);
    static void swapRuntime (Slot& a, Slot& b);
    int wantedType (int s) const;
    uint32_t wantedResVer (int s, int type) const { return typeUsesResource (type) ? slotResVer[s].load (std::memory_order_acquire) : 0u; }
    std::unique_ptr<Effect> build (int slot, int type);   // control thread

    std::array<Slot, kNumSlots> slots;
    SlotParamRefs refs[kNumSlots];
    std::atomic<float>* gIn = nullptr; std::atomic<float>* gOut = nullptr; std::atomic<float>* gMix = nullptr;
    Smooth inG, outG, gMixS;

    static constexpr int kQ = 64;
    Effect* retireBuf[kQ] = {};
    std::atomic<int> rHead { 0 }, rTail { 0 };   // audio writes head, control reads tail

    int moveFrom[16] = {}, moveTo[16] = {};
    std::atomic<int> mHead { 0 }, mTail { 0 };   // control writes head, audio reads tail

    std::shared_ptr<const Resource> slotRes[kNumSlots];   // control thread (under ctl)
    std::atomic<uint32_t> slotResVer[kNumSlots] {};
    uint32_t resCounter = 0;

    std::mutex ctl;
    double sr = 0.0;
    int maxBlock = 0;
    std::vector<float> wetL, wetR, dryL, dryR;
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
};
} // namespace oc
