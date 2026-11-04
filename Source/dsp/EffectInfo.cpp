// OmniChain - effect catalogue table and parameter value mapping
#include "Effects.h"
#include <cstdio>

namespace oc
{
#define CH(arr) arr, int (sizeof (arr) / sizeof (arr[0]))
#define P(n, mn, mx, d, c, u) ParamInfo { n, mn, mx, d, Curve::c, u, nullptr, 0 }
#define C(n, arr, d) ParamInfo { n, 0.0f, float (sizeof (arr) / sizeof (arr[0]) - 1), d, Curve::Choice, "", CH (arr) }
#define NONE ParamInfo {}

const char* const kSyncNames[] = { "1/1", "1/2", "1/2D", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/16T", "1/32" };
const float kSyncBeats[]       = { 4.0f, 2.0f, 3.0f, 1.0f, 1.5f, 2.0f / 3.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };

static const char* const kLimModes[]   = { "Limit", "Clip", "Limit+Clip" };
static const char* const kDistModes[]  = { "Rat", "Muff", "Fuzz Face", "Octave Fuzz" };
static const char* const kCabs[]       = { "Off", "1x12 Open", "2x12 Brit", "4x12 Modern", "4x10 Tweed" };
static const char* const kWahModes[]   = { "Low Pass", "Band Pass", "High Pass", "Band Down" };
static const char* const kLadderModes[]= { "LP 24", "LP 12", "Band", "High" };
static const char* const kFlangeSync[] = { "Free", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4" };
static const char* const kStages[]     = { "2", "4", "6", "8", "12" };
static const char* const kTremSync[]   = { "Free", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
static const char* const kVibeModes[]  = { "Chorus", "Vibrato" };
static const char* const kRotSpeed[]   = { "Slow", "Fast", "Brake" };
static const char* const kRingShapes[] = { "Sine", "Triangle", "Square" };
static const char* const kDelaySync[]  = { "Off", "1/1", "1/2", "1/2D", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/16T", "1/32" };
static const char* const kDelayModes[] = { "Digital", "Analog", "Tape", "Ping-Pong", "Reverse" };
static const char* const kGlitchDiv[]  = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
static const char* const kWahMode[]    = { "Pedal", "Auto", "LFO", "Sync 1/4", "Sync 1/8", "Sync 1/16" };
static const char* const kWahModel[]   = { "Cry Baby", "Vox", "Wide", "Bass" };
static const char* const kFreezeMode[] = { "Auto", "Hold", "Off" };
static const char* const kStepPat[]    = { "Rise", "Fall", "Zigzag", "Jump", "Random", "Trance Gate", "Offbeat Gate", "Euclid 5/8" };
static const char* const kArpPat[]     = { "Octaves", "Major", "Minor", "Fifths", "Cascade", "Random" };
static const char* const kOct3[]       = { "-1 oct", "0", "+1 oct" };
static const char* const kKeys[]       = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kStrChord[]   = { "Octaves", "Major", "Minor", "Sus4", "Power", "Maj7", "Drone" };
static const char* const kStrOct[]     = { "Low", "Mid", "High" };
static const char* const kSpiralSync[] = { "Off", "1/2", "1/4", "1/8D", "1/8", "1/16" };
static const char* const kSpiralInt[]  = { "Oct Down", "5th Down", "Detune", "5th Up", "Oct Up", "Up + Down" };
static const char* const kFbHarm[]     = { "Unison", "Octave", "Oct + 5th", "2 Octaves" };
static const char* const kOffOn[]      = { "Off", "On" };
static const char* const kGrainPitch[] = { "Unison", "Oct Up", "Oct Down", "Oct Random", "Fifths", "Detune" };

static const EffectInfo kInfos[NumEffectTypes] =
{
    { FxNone, "Empty", "---", CatNone, 1.0f, "Empty slot - pick an effect.", { NONE, NONE, NONE, NONE, NONE, NONE } },

    { FxGate, "Noise Gate", "GATE", CatDynamics, 1.0f, "Hysteresis gate with hold - kills hum between notes.",
      { P ("Threshold", -80, 0, -50, Lin, "dB"), P ("Attack", 0.1f, 50, 1, Log, "ms"), P ("Hold", 0, 500, 50, Sq, "ms"),
        P ("Release", 5, 1000, 100, Log, "ms"), P ("Range", -80, 0, -80, Lin, "dB"), P ("Hysteresis", 0, 12, 4, Lin, "dB") } },
    { FxCompressor, "Compressor", "COMP", CatDynamics, 1.0f, "Clean VCA to opto-style glue; Character adds program release + warmth. Lower Mix for parallel compression.",
      { P ("Threshold", -60, 0, -18, Lin, "dB"), P ("Ratio", 1, 20, 4, Log, ":1"), P ("Attack", 0.05f, 100, 5, Log, "ms"),
        P ("Release", 10, 1500, 120, Log, "ms"), P ("Makeup", 0, 24, 0, Lin, "dB"), P ("Character", 0, 100, 30, Lin, "%") } },
    { FxTransient, "Transient Shaper", "TRANS", CatDynamics, 1.0f, "Level-independent attack / sustain designer.",
      { P ("Attack", -100, 100, 0, Lin, "%"), P ("Sustain", -100, 100, 0, Lin, "%"), P ("Speed", 1, 100, 15, Log, "ms"),
        P ("Clip", 0, 100, 0, Lin, "%"), NONE, NONE } },
    { FxLimiter, "Limiter / Clipper", "LIMIT", CatDynamics, 1.0f, "Zero-lookahead brickwall limiter and soft clipper.",
      { P ("Drive", 0, 24, 0, Lin, "dB"), P ("Ceiling", -24, 0, -0.3f, Lin, "dB"), P ("Release", 1, 500, 50, Log, "ms"),
        P ("Softness", 0, 100, 50, Lin, "%"), C ("Mode", kLimModes, 2), NONE } },
    { FxSwell, "Slow Swell", "SWELL", CatDynamics, 1.0f, "Auto volume swell on every new note (violin / Slow Gear style).",
      { P ("Threshold", -70, -10, -45, Lin, "dB"), P ("Rise", 10, 3000, 400, Log, "ms"), P ("Curve", 0, 100, 50, Lin, "%"),
        P ("Retrigger", 3, 24, 9, Lin, "dB"), NONE, NONE } },

    { FxOverdrive, "Overdrive", "OD", CatDrive, 1.0f, "Mid-hump screamer with clean blend for transparent drive.",
      { P ("Drive", 0, 100, 50, Lin, "%"), P ("Tone", 0, 100, 50, Lin, "%"), P ("Tight", 50, 1000, 720, Log, "Hz"),
        P ("Clean", 0, 100, 0, Lin, "%"), P ("Asym", 0, 100, 20, Lin, "%"), P ("Output", -24, 12, -10, Lin, "dB") } },
    { FxDistortion, "Distortion / Fuzz", "DIST", CatDrive, 1.0f, "Rat, Muff, Fuzz Face and Octave Fuzz voicings. Bias = dying-battery sputter.",
      { P ("Gain", 0, 100, 60, Lin, "%"), P ("Tone", 0, 100, 50, Lin, "%"), C ("Mode", kDistModes, 0),
        P ("Bias", 0, 100, 0, Lin, "%"), P ("Mids", -12, 12, 0, Lin, "dB"), P ("Output", -30, 6, -17, Lin, "dB") } },
    { FxAmp, "Amp + Cab", "AMP", CatDrive, 1.0f, "Three-stage tube preamp, tone stack, sagging power amp and zero-latency cab model.",
      { P ("Gain", 0, 100, 40, Lin, "%"), P ("Bass", 0, 10, 5, Lin, ""), P ("Mid", 0, 10, 5, Lin, ""),
        P ("Treble", 0, 10, 5, Lin, ""), P ("Presence", 0, 10, 5, Lin, ""), C ("Cab", kCabs, 2) } },
    { FxTape, "Tape Machine", "TAPE", CatDrive, 1.0f, "Saturation, head bump, wow, flutter, hiss and worn-tape dropouts.",
      { P ("Drive", 0, 24, 6, Lin, "dB"), P ("Wow", 0, 100, 20, Lin, "%"), P ("Flutter", 0, 100, 15, Lin, "%"),
        P ("Hiss", 0, 100, 0, Lin, "%"), P ("Tone", 0, 100, 50, Lin, "%"), P ("Age", 0, 100, 0, Lin, "%") } },

    { FxEq, "EQ", "EQ", CatFilter, 1.0f, "Low cut, low shelf, sweepable mid, high shelf, high cut.",
      { P ("Low Cut", 20, 1000, 20, Log, "Hz<"), P ("Low", -15, 15, 0, Lin, "dB"), P ("Mid Freq", 200, 8000, 1000, Log, "Hz"),
        P ("Mid", -15, 15, 0, Lin, "dB"), P ("High", -15, 15, 0, Lin, "dB"), P ("High Cut", 1000, 20000, 20000, Log, "Hz>") } },
    { FxAutoWah, "Envelope Filter", "WAH", CatFilter, 1.0f, "Pick-sensitive auto-wah (Mu-Tron style).",
      { P ("Sensitivity", 0, 100, 60, Lin, "%"), P ("Low Freq", 100, 2000, 300, Log, "Hz"), P ("Range", 0, 5, 3, Lin, "oct"),
        P ("Resonance", 0.5f, 15, 4, Log, "Q"), P ("Release", 10, 500, 80, Log, "ms"), C ("Mode", kWahModes, 1) } },
    { FxLadder, "Ladder Filter", "LADDER", CatFilter, 1.0f, "Saturating 4-pole ladder filter with LFO.",
      { P ("Cutoff", 40, 18000, 2000, Log, "Hz"), P ("Resonance", 0, 100, 30, Lin, "%"), P ("Drive", 0, 24, 0, Lin, "dB"),
        C ("Mode", kLadderModes, 0), P ("LFO Rate", 0.05f, 20, 1, Log, "Hz"), P ("LFO Depth", 0, 4, 0, Lin, "oct") } },
    { FxTalk, "Talk Box", "TALK", CatFilter, 1.0f, "Formant vowel filter - morph A-E-I-O-U by knob, LFO or picking dynamics.",
      { P ("Vowel", 0, 4, 0, Lin, "vowel"), P ("LFO Rate", 0.05f, 10, 0.5f, Log, "Hz"), P ("LFO Depth", 0, 100, 0, Lin, "%"),
        P ("Envelope", 0, 100, 0, Lin, "%"), P ("Resonance", 0, 100, 50, Lin, "%"), P ("Shift", -12, 12, 0, Lin, "st") } },

    { FxChorus, "Chorus", "CHORUS", CatMod, 0.5f, "Lush 1-4 voice stereo chorus.",
      { P ("Rate", 0.05f, 8, 0.6f, Log, "Hz"), P ("Depth", 0, 100, 50, Lin, "%"), P ("Delay", 2, 30, 9, Log, "ms"),
        P ("Voices", 1, 4, 2, Int, ""), P ("Spread", 0, 100, 70, Lin, "%"), P ("Tone", 1000, 20000, 12000, Log, "Hz") } },
    { FxFlanger, "Flanger", "FLANGE", CatMod, 1.0f, "Jet flanger with +/- feedback and tempo-synced sweeps.",
      { P ("Rate", 0.02f, 5, 0.25f, Log, "Hz"), P ("Depth", 0, 100, 70, Lin, "%"), P ("Manual", 0.1f, 10, 1.5f, Log, "ms"),
        P ("Feedback", -95, 95, 50, Lin, "%"), P ("Spread", 0, 100, 50, Lin, "%"), C ("Sync", kFlangeSync, 0) } },
    { FxPhaser, "Phaser", "PHASE", CatMod, 1.0f, "2 to 12 stage phaser with feedback.",
      { P ("Rate", 0.02f, 8, 0.4f, Log, "Hz"), P ("Depth", 0, 100, 80, Lin, "%"), C ("Stages", kStages, 1),
        P ("Feedback", 0, 95, 40, Lin, "%"), P ("Center", 100, 4000, 700, Log, "Hz"), P ("Spread", 0, 100, 30, Lin, "%") } },
    { FxTremolo, "Tremolo / Pan", "TREM", CatMod, 1.0f, "Sine-to-square tremolo, auto-pan and harmonic (brownface) mode, tempo-syncable.",
      { P ("Rate", 0.1f, 20, 5, Log, "Hz"), P ("Depth", 0, 100, 60, Lin, "%"), P ("Shape", 0, 100, 0, Lin, "%"),
        P ("Stereo", 0, 100, 0, Lin, "%"), C ("Sync", kTremSync, 0), P ("Harmonic", 0, 100, 0, Lin, "%") } },
    { FxVibe, "Uni-Vibe", "VIBE", CatMod, 1.0f, "Lamp-driven staggered phaser - the Machine Gun sound.",
      { P ("Speed", 0.1f, 12, 2, Log, "Hz"), P ("Intensity", 0, 100, 75, Lin, "%"), C ("Mode", kVibeModes, 0),
        P ("Throb", 0, 100, 30, Lin, "%"), P ("Width", 0, 100, 30, Lin, "%"), NONE } },
    { FxRotary, "Rotary Speaker", "ROTARY", CatMod, 1.0f, "Horn + drum rotary cabinet with realistic spin-up / spin-down.",
      { C ("Speed", kRotSpeed, 0), P ("Accel", 0.2f, 5, 1.2f, Log, "s"), P ("Balance", 0, 100, 50, Lin, "%"),
        P ("Drive", 0, 100, 20, Lin, "%"), P ("Width", 0, 100, 80, Lin, "%"), P ("Depth", 0, 100, 70, Lin, "%") } },
    { FxRingMod, "Ring Modulator", "RING", CatMod, 1.0f, "Ring mod with carrier LFO - bells, robots, chaos.",
      { P ("Freq", 1, 5000, 440, Log, "Hz"), C ("Shape", kRingShapes, 0), P ("LFO Rate", 0.05f, 20, 0.5f, Log, "Hz"),
        P ("LFO Depth", 0, 100, 0, Lin, "%"), P ("Tone", 500, 20000, 12000, Log, "Hz"), NONE } },

    { FxOctaver, "Octaver", "OCTAVE", CatPitch, 1.0f, "Analog-style -1/-2 octave tracker plus octave-up fuzz.",
      { P ("Sub 1", 0, 100, 70, Lin, "%"), P ("Sub 2", 0, 100, 0, Lin, "%"), P ("Up", 0, 100, 0, Lin, "%"),
        P ("Dry", 0, 100, 70, Lin, "%"), P ("Tone", 100, 5000, 1200, Log, "Hz"), P ("Tracking", 0, 100, 50, Lin, "%") } },
    { FxPitch, "Pitch Shifter", "PITCH", CatPitch, 0.5f, "Two-voice harmonizer / whammy. Feedback = endless spiral.",
      { P ("Shift", -24, 24, 12, Int, "st"), P ("Fine", -100, 100, 0, Lin, "ct"), P ("Voice 2", -24, 24, -12, Int, "st"),
        P ("V2 Level", 0, 100, 0, Lin, "%"), P ("Window", 10, 100, 40, Log, "ms"), P ("Feedback", 0, 90, 0, Lin, "%") } },

    { FxDelay, "Delay", "DELAY", CatTime, 0.35f, "Digital, analog, tape, ping-pong and reverse delay, host-syncable.",
      { P ("Time", 1, 2000, 375, Log, "ms"), C ("Sync", kDelaySync, 0), P ("Feedback", 0, 110, 35, Lin, "%"),
        P ("Tone", 500, 20000, 6000, Log, "Hz"), C ("Mode", kDelayModes, 0), P ("Mod", 0, 100, 15, Lin, "%") } },
    { FxReverb, "Reverb", "VERB", CatTime, 0.3f, "8-line modulated FDN - room to infinite hall, with octave shimmer.",
      { P ("Size", 0, 100, 50, Lin, "%"), P ("Decay", 0.2f, 20, 2.5f, Log, "s"), P ("Damping", 0, 100, 40, Lin, "%"),
        P ("Predelay", 0, 200, 10, Sq, "ms"), P ("Mod", 0, 100, 30, Lin, "%"), P ("Shimmer", 0, 100, 0, Lin, "%") } },
    { FxSpring, "Spring Reverb", "SPRING", CatTime, 0.35f, "Dispersive spring tank - surf drip and boing.",
      { P ("Decay", 0.5f, 6, 2, Log, "s"), P ("Tone", 1000, 8000, 3500, Log, "Hz"), P ("Boing", 0, 100, 60, Lin, "%"),
        P ("Length", 20, 80, 40, Lin, "ms"), P ("Drip", 0, 100, 40, Lin, "%"), NONE } },

    { FxCrusher, "Bitcrusher", "CRUSH", CatCreative, 1.0f, "Bit depth and sample-rate reduction with clock jitter.",
      { P ("Bits", 1, 16, 8, Lin, "bit"), P ("Rate", 200, 48000, 12000, Log, "Hz"), P ("Drive", 0, 24, 0, Lin, "dB"),
        P ("Jitter", 0, 100, 0, Lin, "%"), P ("Tone", 500, 20000, 20000, Log, "Hz"), NONE } },
    { FxGlitch, "Glitch Grid", "GLITCH", CatCreative, 1.0f, "Tempo-locked random stutter, reverse and tape-stop machine.",
      { C ("Division", kGlitchDiv, 3), P ("Chance", 0, 100, 40, Lin, "%"), P ("Repeat", 0, 100, 60, Lin, "%"),
        P ("Reverse", 0, 100, 25, Lin, "%"), P ("Tape Stop", 0, 100, 20, Lin, "%"), P ("Gate", 10, 100, 100, Lin, "%") } },
    { FxGranular, "Granular Cloud", "GRAIN", CatCreative, 0.5f, "Scatters pitched / reversed grains of your playing into a cloud.",
      { P ("Density", 1, 50, 12, Log, "/s"), P ("Size", 20, 500, 120, Log, "ms"), P ("Spray", 0, 100, 40, Lin, "%"),
        C ("Pitch", kGrainPitch, 0), P ("Reverse", 0, 100, 20, Lin, "%"), P ("Feedback", 0, 90, 30, Lin, "%") } },

    { FxImager, "Stereo Imager", "IMAGE", CatUtility, 1.0f, "Mid/side width, mono-bass, Haas widening and pan.",
      { P ("Width", 0, 200, 100, Lin, "%"), P ("Mono Bass", 20, 500, 20, Log, "Hz<"), P ("Haas", 0, 30, 0, Lin, "ms"),
        P ("Pan", -100, 100, 0, Lin, "pan"), NONE, NONE } },

    { FxCabIR, "Cab IR Loader", "CAB IR", CatDrive, 1.0f, "Load any cabinet impulse response (.wav) - zero-latency convolution.",
      { P ("Low Cut", 20, 500, 70, Log, "Hz<"), P ("High Cut", 2000, 20000, 20000, Log, "Hz>"), P ("Length", 10, 1000, 250, Log, "ms"),
        P ("Output", -24, 24, 0, Lin, "dB"), NONE, NONE } },
    { FxNam, "NAM Capture", "NAM", CatDrive, 1.0f, "Neural Amp Modeler: real amps and pedals captured by AI (.nam files).",
      { P ("Input", -24, 24, 0, Lin, "dB"), P ("Output", -24, 24, 0, Lin, "dB"), P ("Bass", -12, 12, 0, Lin, "dB"),
        P ("Mid", -12, 12, 0, Lin, "dB"), P ("Treble", -12, 12, 0, Lin, "dB"), C ("Normalize", kOffOn, 1) } },

    { FxWah, "Wah Pedal", "WAH", CatFilter, 1.0f, "Rock the Pedal knob with an expression pedal, or let it sweep by itself.",
      { P ("Pedal", 0, 100, 50, Lin, "%"), C ("Mode", kWahMode, 0), C ("Model", kWahModel, 0),
        P ("Resonance", 0, 100, 50, Lin, "%"), P ("Speed", 0.1f, 10, 2, Log, "Hz"), P ("Sensitivity", 0, 100, 60, Lin, "%") } },
    { FxFreeze, "Freeze Pad", "FREEZE", CatCreative, 1.0f, "Captures your notes and sustains them forever as an ambient pad under your playing.",
      { C ("Mode", kFreezeMode, 0), P ("Blend", 0, 100, 60, Lin, "%"), P ("Capture", 80, 600, 250, Log, "ms"),
        P ("Fade", 0.05f, 8, 1.5f, Log, "s"), P ("Shimmer", 0, 100, 0, Lin, "%"), P ("Tone", 500, 16000, 6000, Log, "Hz") } },
    { FxStepFilter, "Step Filter", "STEP", CatFilter, 1.0f, "Tempo-synced 16-step filter and trance-gate sequencer.",
      { C ("Division", kGlitchDiv, 3), C ("Pattern", kStepPat, 0), P ("Base", 80, 4000, 300, Log, "Hz"),
        P ("Range", 0, 6, 3.5f, Lin, "oct"), P ("Resonance", 0, 100, 55, Lin, "%"), P ("Glide", 0, 100, 25, Lin, "%") } },
    { FxArp, "Harmonic Arp", "ARP", CatPitch, 0.5f, "Arpeggiates your playing through musical intervals in time with the song.",
      { C ("Division", kGlitchDiv, 3), C ("Pattern", kArpPat, 1), C ("Octave", kOct3, 1),
        P ("Glide", 0, 100, 10, Lin, "%"), P ("Window", 15, 80, 35, Log, "ms"), P ("Feedback", 0, 80, 0, Lin, "%") } },
    { FxStrings, "Sympathetic Strings", "STRINGS", CatCreative, 0.4f, "Six virtual strings tuned to a chord ring along with whatever you play.",
      { C ("Key", kKeys, 4), C ("Chord", kStrChord, 1), P ("Decay", 0.2f, 10, 3, Log, "s"),
        P ("Brightness", 0, 100, 55, Lin, "%"), P ("Spread", 0, 100, 60, Lin, "%"), C ("Octave", kStrOct, 1) } },
    { FxSpiral, "Spiral Delay", "SPIRAL", CatTime, 0.4f, "Every repeat climbs or falls in pitch - endless staircases of echoes.",
      { P ("Time", 20, 1500, 320, Log, "ms"), C ("Sync", kSpiralSync, 0), C ("Interval", kSpiralInt, 4),
        P ("Feedback", 0, 95, 55, Lin, "%"), P ("Tone", 800, 16000, 7000, Log, "Hz"), P ("Spin", 0, 100, 50, Lin, "%") } },
    { FxGhostVerb, "Ghost Verb", "GHOST", CatTime, 0.5f, "A reverb that hides while you play and blooms the moment you stop.",
      { P ("Size", 0, 100, 70, Lin, "%"), P ("Decay", 0.5f, 20, 5, Log, "s"), P ("Duck", 0, 100, 80, Lin, "%"),
        P ("Recovery", 30, 3000, 400, Log, "ms"), P ("Swell", 0, 100, 40, Lin, "%"), P ("Shimmer", 0, 100, 0, Lin, "%") } },
    { FxFeedback, "Amp Feedback", "FEEDBK", CatCreative, 1.0f, "Held notes bloom into singing harmonic feedback, like standing in front of a loud amp.",
      { C ("Harmonic", kFbHarm, 1), P ("Bloom", 0.2f, 6, 1.5f, Log, "s"), P ("Amount", 0, 100, 60, Lin, "%"),
        P ("Threshold", -60, -10, -40, Lin, "dB"), P ("Tone", 500, 8000, 2500, Log, "Hz"), P ("Wobble", 0, 100, 25, Lin, "%") } },
};

const EffectInfo& getInfo (int type)
{
    if (type < 0 || type >= NumEffectTypes) type = 0;
    return kInfos[type];
}

const char* categoryName (Category c)
{
    static const char* names[] = { "", "Dynamics", "Drive & Amp", "Filter & EQ", "Modulation", "Pitch", "Time & Space", "Creative & Lo-Fi", "Utility" };
    return names[c < NumCategories ? c : 0];
}

uint32_t categoryColour (Category c)
{
    static const uint32_t cols[] = { 0x5a6070, 0x4fc3f7, 0xff7043, 0xffca28, 0xab47bc, 0x66bb6a, 0x26c6da, 0xec407a, 0x90a4ae };
    return cols[c < NumCategories ? c : 0];
}

float paramToReal (const ParamInfo& p, float n)
{
    n = clampf (n, 0.0f, 1.0f);
    switch (p.curve)
    {
        case Curve::Lin:    return p.min + (p.max - p.min) * n;
        case Curve::Log:    return p.min * std::pow (p.max / p.min, n);
        case Curve::Sq:     return p.min + (p.max - p.min) * n * n;
        case Curve::Int:
        case Curve::Choice: return std::round (p.min + (p.max - p.min) * n);
    }
    return p.min;
}

float paramToNorm (const ParamInfo& p, float r)
{
    if (p.max == p.min) return 0.0f;
    r = clampf (r, std::min (p.min, p.max), std::max (p.min, p.max));
    switch (p.curve)
    {
        case Curve::Lin:
        case Curve::Int:
        case Curve::Choice: return (r - p.min) / (p.max - p.min);
        case Curve::Log:    return std::log (r / p.min) / std::log (p.max / p.min);
        case Curve::Sq:     return std::sqrt ((r - p.min) / (p.max - p.min));
    }
    return 0.0f;
}

std::string formatParam (const ParamInfo& p, float v)
{
    char b[64];
    const std::string u = p.unit ? p.unit : "";
    if (p.curve == Curve::Choice)
    {
        const int i = (int) clampf (v, 0.0f, float (p.numChoices - 1));
        return p.choices ? p.choices[i] : std::to_string (i);
    }
    if (u == "Hz" || u == "Hz<" || u == "Hz>")
    {
        if (u == "Hz<" && v <= p.min * 1.02f) return "Off";
        if (u == "Hz>" && v >= p.max * 0.98f) return "Off";
        if (v >= 1000.0f) std::snprintf (b, sizeof (b), "%.2f kHz", v / 1000.0f);
        else if (v >= 100.0f) std::snprintf (b, sizeof (b), "%.0f Hz", v);
        else if (v >= 10.0f) std::snprintf (b, sizeof (b), "%.1f Hz", v);
        else std::snprintf (b, sizeof (b), "%.2f Hz", v);
        return b;
    }
    if (u == "ms")
    {
        if (v >= 1000.0f) std::snprintf (b, sizeof (b), "%.2f s", v / 1000.0f);
        else if (v >= 100.0f) std::snprintf (b, sizeof (b), "%.0f ms", v);
        else if (v >= 10.0f) std::snprintf (b, sizeof (b), "%.1f ms", v);
        else std::snprintf (b, sizeof (b), "%.2f ms", v);
        return b;
    }
    if (u == "pan")
    {
        if (std::fabs (v) < 1.0f) return "C";
        std::snprintf (b, sizeof (b), "%s%.0f", v < 0 ? "L" : "R", std::fabs (v));
        return b;
    }
    if (u == "vowel")
    {
        static const char* vw[] = { "A", "E", "I", "O", "U" };
        const int i = (int) std::floor (clampf (v, 0, 3.999f));
        const float f = v - float (i);
        if (f < 0.12f) return vw[i];
        if (f > 0.88f) return vw[std::min (i + 1, 4)];
        std::snprintf (b, sizeof (b), "%s>%s", vw[i], vw[i + 1]);
        return b;
    }
    if (p.curve == Curve::Int) { std::snprintf (b, sizeof (b), "%+d %s", (int) v, u.c_str()); if (u.empty()) std::snprintf (b, sizeof (b), "%d", (int) v); return b; }
    if (u == "%")   { std::snprintf (b, sizeof (b), "%.0f%%", v); return b; }
    if (u == "dB")  { std::snprintf (b, sizeof (b), "%+.1f dB", v); return b; }
    if (u == "s")   { std::snprintf (b, sizeof (b), "%.2f s", v); return b; }
    if (u == ":1")  { std::snprintf (b, sizeof (b), "%.1f:1", v); return b; }
    if (u == "st")  { std::snprintf (b, sizeof (b), "%+.1f st", v); return b; }
    if (u == "ct")  { std::snprintf (b, sizeof (b), "%+.0f ct", v); return b; }
    if (u.empty())  { std::snprintf (b, sizeof (b), "%.1f", v); return b; }
    std::snprintf (b, sizeof (b), "%.2f %s", v, u.c_str());
    return b;
}
} // namespace oc
