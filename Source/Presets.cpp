#include "Presets.h"

using namespace oc;

const std::vector<PresetDef>& getFactoryPresets()
{
    const float D = NAN; // "use default"
    static const std::vector<PresetDef> presets =
    {
        { "Init (empty chain)", {} },

        { "Clean Sparkle", {
            { FxCompressor, 100, 0, { -24, 3, 10, 150, 4, 50 } },
            { FxAmp, 100, 0, { 15, 5.5f, 4.5f, 6.5f, 6, 1 } },
            { FxChorus, 45, 0, { 0.5f, 45, 8, 2, 80, 12000 } },
            { FxDelay, 25, 0, { D, 8, 30, 5000, 0, 20 } },
            { FxReverb, 25, 0, { 60, 2.2f, 50, 15, 30, 0 } } } },

        { "Classic Crunch", {
            { FxGate, 100, 0, { -60, 1, 50, 120, -80, 4 } },
            { FxOverdrive, 100, 0, { 20, 55, 720, 0, 20, -4 } },
            { FxAmp, 100, 0, { 55, 5, 6, 6, 5.5f, 2 } },
            { FxDelay, 20, 0, { 320, 0, 25, 3500, 1, 20 } },
            { FxReverb, 18, 0, { 35, 1.4f, 55, 10, 20, 0 } } } },

        { "Modern Metal Rhythm", {
            { FxGate, 100, 0, { -48, 0.5f, 30, 60, -80, 6 } },
            { FxOverdrive, 100, 0, { 5, 65, 900, 0, 10, 0 } },
            { FxAmp, 100, 0, { 85, 6, 4, 6.5f, 6.5f, 3 } },
            { FxEq, 100, 0, { 70, 1.5f, 500, -3, 1, 11000 } },
            { FxLimiter, 100, 0, { 0, -0.5f, 60, 60, 2 } } } },

        { "Singing Lead", {
            { FxCompressor, 100, 0, { -20, 4, 15, 200, 3, 60 } },
            { FxDistortion, 100, 0, { 55, 55, 1, 0, 3, -17 } },
            { FxAmp, 100, 0, { 45, 5, 6.5f, 5.5f, 5, 2 } },
            { FxDelay, 28, 0, { D, 4, 35, 4000, 2, 30 } },
            { FxReverb, 25, 0, { 75, 3.5f, 45, 25, 40, 0 } } } },

        { "Shimmer Cathedral", {
            { FxSwell, 100, 0, { -50, 600, 50, 9 } },
            { FxChorus, 40, 0, { 0.3f, 40, 12, 3, 100, 9000 } },
            { FxDelay, 35, 0, { D, 5, 45, 5000, 3, 25 } },
            { FxReverb, 55, 0, { 95, 9, 35, 40, 60, 55 } } } },

        { "Psychedelic Vibe", {
            { FxDistortion, 100, 0, { 60, 60, 2, 0, 0, -15 } },
            { FxVibe, 100, 0, { 3, 80, 0, 35, 30 } },
            { FxAmp, 100, 0, { 40, 5, 5, 6, 6, 4 } },
            { FxSpring, 30, 0, { 2.5f, 3500, 60, 40, 40 } } } },

        { "Surf Drip", {
            { FxAmp, 100, 0, { 20, 5, 4.5f, 7, 6, 1 } },
            { FxTremolo, 100, 0, { 6, 50, 20, 0, 0, 0 } },
            { FxSpring, 45, 0, { 3, 4500, 80, 45, 80 } } } },

        { "Funk Quack", {
            { FxCompressor, 100, 0, { -26, 6, 2, 100, 6, 20 } },
            { FxAutoWah, 100, 0, { 70, 350, 3.2f, 5, 70, 1 } },
            { FxAmp, 100, 0, { 15, 5, 5, 6.5f, 6, 1 } },
            { FxReverb, 15, 0, { 30, 1.0f, 50, 5, 20, 0 } } } },

        { "Leslie Organ Drive", {
            { FxOverdrive, 100, 0, { 30, 45, 300, 20, 20, -8 } },
            { FxRotary, 100, 0, { 1, 1.2f, 55, 30, 80, 75 } },
            { FxReverb, 20, 0, { 45, 1.8f, 50, 10, 20, 0 } } } },

        { "Lo-Fi Tape Dream", {
            { FxTape, 100, 0, { 8, 45, 30, 25, 35, 40 } },
            { FxCrusher, 50, 0, { 10, 16000, 0, 15, 9000 } },
            { FxLadder, 100, 0, { 3000, 25, 3, 0, 0.15f, 0.8f } },
            { FxReverb, 30, 0, { 50, 2, 60, 20, 40, 0 } } } },

        { "Glitch Lab", {
            { FxGlitch, 100, 0, { 3, 55, 60, 30, 25, 100 } },
            { FxLadder, 100, 0, { 2500, 45, 6, 0, 0.25f, 1.5f } },
            { FxDelay, 25, 0, { D, 9, 40, 6000, 3, 10 } },
            { FxReverb, 20, 0, { 55, 2.5f, 40, 10, 30, 0 } } } },

        { "Robot Voice", {
            { FxRingMod, 100, 0, { 90, 0, 0.2f, 10, 8000 } },
            { FxTalk, 100, 0, { 1, 1.5f, 60, 20, 60, 0 } },
            { FxDelay, 20, 0, { 90, 0, 10, 8000, 0, 0 } } } },

        { "Granular Haze", {
            { FxGranular, 60, 0, { 18, 180, 60, 3, 30, 45 } },
            { FxReverb, 45, 0, { 85, 6, 40, 30, 50, 20 } } } },

        { "Octave Doom", {
            { FxGate, 100, 0, { -55, 1, 40, 100, -80, 5 } },
            { FxOctaver, 100, 0, { 80, 40, 0, 70, 900, 60 } },
            { FxDistortion, 100, 0, { 75, 40, 1, 0, -4, -17 } },
            { FxAmp, 100, 0, { 70, 7, 4, 5, 5, 3 } } } },

        { "Jet Flange Rock", {
            { FxOverdrive, 100, 0, { 40, 55, 720, 0, 20, -10 } },
            { FxAmp, 100, 0, { 50, 5, 6, 6, 5.5f, 2 } },
            { FxFlanger, 100, 0, { 0.12f, 80, 1, 70, 50, 0 } },
            { FxReverb, 15, 0, { 40, 1.5f, 50, 10, 20, 0 } } } },

        { "Octave Harmony Lead", {
            { FxPitch, 40, 0, { 12, 0, -12, 40, 35, 0 } },
            { FxDistortion, 100, 0, { 50, 55, 0, 0, 2, -17 } },
            { FxAmp, 100, 0, { 40, 5, 6, 5.5f, 5, 2 } },
            { FxDelay, 25, 0, { D, 7, 30, 4500, 1, 20 } } } },

        { "Mix Bus Glue", {
            { FxEq, 100, 0, { 30, 1, 3000, 0.5f, 1.5f, 20000 } },
            { FxCompressor, 100, 0, { -14, 2, 30, 200, 1.5f, 40 } },
            { FxTape, 100, 0, { 3, 0, 0, 0, 70, 0 } },
            { FxImager, 100, 0, { 115, 120, 0, 0 } },
            { FxLimiter, 100, 0, { 2, -0.3f, 80, 50, 0 } } } },

        { "Drum Smash", {
            { FxTransient, 100, 0, { 40, -20, 15, 20 } },
            { FxCompressor, 45, 0, { -30, 8, 1, 80, 8, 70 } },
            { FxTape, 100, 0, { 9, 0, 0, 0, 55, 0 } },
            { FxLimiter, 100, 0, { 3, -0.5f, 50, 60, 2 } } } },

        { "Wide Stereo Pad", {
            { FxChorus, 50, 0, { 0.25f, 60, 14, 4, 100, 10000 } },
            { FxImager, 100, 0, { 160, 150, 12, 0 } },
            { FxReverb, 40, 0, { 80, 4.5f, 45, 30, 60, 10 } } } },
    };
    return presets;
}
