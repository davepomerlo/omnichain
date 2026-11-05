// OmniChain - effect catalogue, parameter metadata and effect base class
#pragma once
#include "DspCommon.h"
#include <string>
#include <memory>

namespace oc
{
constexpr int kParamsPerSlot = 6;

struct Context
{
    double sampleRate = 48000.0;
    double bpm = 120.0;
    double ppq = 0.0;          // quarter-note position at start of block
    bool playing = false;
};

enum class Curve { Lin, Log, Sq, Int, Choice };

struct ParamInfo
{
    const char* name = nullptr;      // nullptr -> unused knob
    float min = 0, max = 1, def = 0;
    Curve curve = Curve::Lin;
    const char* unit = "";
    const char* const* choices = nullptr;
    int numChoices = 0;
};

enum Category { CatNone, CatDynamics, CatDrive, CatFilter, CatMod, CatPitch, CatTime, CatCreative, CatUtility, NumCategories };

// NOTE: order is grouped by category; this is also the order of the host "Type" choice parameter.
enum EffectType
{
    FxNone = 0,
    // Dynamics
    FxGate, FxCompressor, FxTransient, FxLimiter, FxSwell,
    // Drive
    FxOverdrive, FxDistortion, FxAmp, FxTape,
    // Filter / EQ
    FxEq, FxAutoWah, FxLadder, FxTalk,
    // Modulation
    FxChorus, FxFlanger, FxPhaser, FxTremolo, FxVibe, FxRotary, FxRingMod,
    // Pitch
    FxOctaver, FxPitch,
    // Time / Space
    FxDelay, FxReverb, FxSpring,
    // Creative / Lo-fi
    FxCrusher, FxGlitch, FxGranular,
    // Utility
    FxImager,
    // Added in 1.1 (appended so saved projects keep their effect numbers)
    FxCabIR, FxNam,
    // Added in 1.2
    FxWah, FxFreeze, FxStepFilter, FxArp, FxStrings, FxSpiral, FxGhostVerb, FxFeedback,
    // Added in 1.3
    FxTapEnsemble, FxSynth, FxBarber, FxRadio, FxOrbit, FxMelt, FxChaos,
    NumEffectTypes
};

struct EffectInfo
{
    EffectType type;
    const char* name;
    const char* shortName;
    Category category;
    float defaultMix;                 // 0..1
    const char* blurb;
    ParamInfo params[kParamsPerSlot];
};

const EffectInfo& getInfo (int type);
const char* categoryName (Category c);
uint32_t categoryColour (Category c);   // 0xRRGGBB

float paramToReal (const ParamInfo& p, float norm);
float paramToNorm (const ParamInfo& p, float real);
std::string formatParam (const ParamInfo& p, float real);
inline std::string formatParamNorm (int type, int idx, float norm)
{
    const auto& p = getInfo (type).params[idx];
    if (p.name == nullptr) return "-";
    return formatParam (p, paramToReal (p, norm));
}

// Tempo-sync division helper (beats per division)
extern const char* const kSyncNames[];
extern const float kSyncBeats[];
constexpr int kNumSync = 12;

// Data loaded from a file (impulse response, NAM model). Immutable once built, shared between instances.
struct Resource
{
    virtual ~Resource() = default;
    std::string name;            // file name for display
    double sampleRate = 0.0;     // native rate of the IR / model (0 = unknown)
};

struct IrResource final : Resource
{
    std::vector<float> ch[2];    // ch[1] empty for mono IRs
};

inline bool typeUsesResource (int t) { return t == FxCabIR || t == FxNam; }

class Effect
{
public:
    virtual ~Effect() = default;
    int type = FxNone;
    double sr = 48000.0;
    int maxBlock = 512;
    float meter = 0.0f;               // 0..1 activity / gain reduction for the UI
    std::shared_ptr<const Resource> resource;   // set before prepare() for file-based effects
    uint32_t resVer = 0;

    virtual void prepare (double sampleRate, int maxBlockSize) { sr = sampleRate; maxBlock = maxBlockSize; }
    virtual void reset() {}
    // v = real-valued parameters for this block (already mapped from normalised)
    virtual void process (float* L, float* R, int n, const float* v, const Context& ctx) = 0;
};

std::unique_ptr<Effect> createEffect (int type);
std::unique_ptr<Effect> createNamEffect();   // NamEffect.cpp
} // namespace oc
