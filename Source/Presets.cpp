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
        // ---- v1.2 showcase presets ----
        { "Wah Funk Machine", {
            { FxCompressor, 100, 0, { -26, 6, 2, 100, 6, 20 } },
            { FxWah, 100, 0, { 100, 1, 0, 60, 3, 65 } },
            { FxAmp, 100, 0, { 15, 5, 5, 6.5f, 6, 1 } },
            { FxSpring, 25, 0, { 2, 4000, 60, 40, 50 } } } },

        { "Wah Solo (automate Pedal)", {
            { FxWah, 100, 0, { 50, 0, 0, 55, 2, 60 } },
            { FxDistortion, 100, 0, { 55, 55, 1, 0, 2, -17 } },
            { FxAmp, 100, 0, { 45, 5, 6, 5.5f, 5, 2 } },
            { FxDelay, 25, 0, { D, 7, 35, 4500, 2, 25 } } } },

        { "Frozen Lake", {
            { FxFreeze, 100, 0, { 0, 70, 300, 2.5f, 30, 5000 } },
            { FxChorus, 40, 0, { 0.3f, 50, 12, 3, 100, 9000 } },
            { FxGhostVerb, 50, 0, { 85, 8, 70, 500, 50, 30 } } } },

        { "Spiral Staircase", {
            { FxOverdrive, 100, 0, { 15, 55, 500, 30, 20, -9 } },
            { FxAmp, 100, 0, { 30, 5, 5.5f, 6, 5.5f, 2 } },
            { FxSpiral, 45, 0, { D, 2, 4, 60, 6000, 60 } },
            { FxReverb, 25, 0, { 70, 3, 45, 20, 40, 0 } } } },

        { "Sitar Garden", {
            { FxStrings, 45, 0, { 4, 6, 4, 70, 50, 1 } },
            { FxChorus, 35, 0, { 0.4f, 40, 10, 2, 80, 10000 } },
            { FxReverb, 30, 0, { 65, 2.8f, 45, 20, 35, 0 } } } },

        { "Arp Machine", {
            { FxArp, 55, 0, { 3, 1, 1, 10, 30, 0 } },
            { FxDelay, 30, 0, { D, 8, 40, 5000, 3, 15 } },
            { FxReverb, 25, 0, { 60, 2.5f, 45, 15, 35, 0 } } } },

        { "Step Pulse", {
            { FxStepFilter, 100, 0, { 3, 5, 400, 3.5f, 40, 15 } },
            { FxDelay, 25, 0, { D, 8, 35, 6000, 3, 10 } },
            { FxReverb, 20, 0, { 55, 2, 45, 10, 30, 0 } } } },

        { "Feedback Hero", {
            { FxDistortion, 100, 0, { 65, 50, 0, 0, 2, -17 } },
            { FxAmp, 100, 0, { 70, 5.5f, 5.5f, 6, 6, 3 } },
            { FxFeedback, 100, 0, { 1, 1.2f, 70, -42, 2500, 25 } },
            { FxDelay, 22, 0, { D, 4, 30, 4000, 1, 20 } },
            { FxReverb, 20, 0, { 60, 2.2f, 50, 15, 30, 0 } } } },
        // ---- v1.3: artist-style + new-effect presets ----
        { "Holdsworth-Style Legato Lead", {
            { FxCompressor, 100, 0, { -24, 4, 20, 250, 5, 70 } },
            { FxOverdrive, 100, 0, { 35, 45, 400, 0, 15, -10 } },
            { FxAmp, 100, 0, { 62, 5, 7, 4.5f, 4.5f, 3 } },
            { FxEq, 100, 0, { 80, 0, 900, 2, -3, 6500 } },
            { FxTapEnsemble, 35, 0, { 8, 15, 5, 100, 8, 7000 } },
            { FxDelay, 18, 0, { 420, 0, 25, 4000, 0, 10 } },
            { FxReverb, 18, 0, { 60, 2.2f, 50, 25, 30, 0 } } } },

        { "Holdsworth-Style Swell Chords", {
            { FxSwell, 100, 0, { -50, 450, 60, 9 } },
            { FxCompressor, 100, 0, { -30, 3, 15, 200, 6, 50 } },
            { FxAmp, 100, 0, { 12, 5, 5, 5.5f, 5, 2 } },
            { FxTapEnsemble, 55, 0, { 8, 15, 5, 100, 15, 9000 } },
            { FxDelay, 22, 0, { 520, 0, 30, 4500, 0, 10 } },
            { FxReverb, 25, 0, { 75, 3.5f, 45, 30, 40, 0 } } } },

        { "SynthAxe-Style Lead", {
            { FxCompressor, 100, 0, { -26, 4, 10, 200, 4, 50 } },
            { FxSynth, 65, 6, { 0, 2, 2200, 40, 40, 80 } },
            { FxTapEnsemble, 40, 0, { 8, 15, 5, 100, 12, 8000 } },
            { FxDelay, 20, 0, { 450, 0, 30, 4000, 0, 10 } },
            { FxReverb, 30, 0, { 70, 3, 45, 25, 40, 0 } } } },

        { "Cisneros-Style Sleep Fuzz Bass", {
            { FxCompressor, 100, 0, { -20, 3, 25, 200, 2, 60 } },
            { FxOverdrive, 100, 0, { 55, 40, 60, 45, 25, -8 } },
            { FxDistortion, 70, 0, { 70, 35, 1, 0, 4, -17 } },
            { FxAmp, 100, 0, { 75, 7, 6.5f, 4, 4, 4 } },
            { FxEq, 100, 0, { 30, 3, 700, 2, -2, 5000 } } } },

        { "Cisneros-Style OM Dub Bass", {
            { FxCompressor, 100, 0, { -22, 4, 30, 250, 4, 70 } },
            { FxAmp, 100, 0, { 35, 7.5f, 5.5f, 4, 4, 4 } },
            { FxEq, 100, 0, { 30, 3, 600, 1.5f, -4, 4000 } },
            { FxDelay, 25, 0, { D, 5, 45, 2000, 2, 30 } },
            { FxSpring, 25, 0, { 3, 3000, 70, 45, 30 } } } },

        { "Sub Synth Bass", {
            { FxCompressor, 100, 0, { -22, 4, 15, 150, 4, 40 } },
            { FxSynth, 45, 0, { 3, 1, 800, 10, 10, 5 } },
            { FxEq, 100, 0, { 25, 2, 800, 0, -1, 9000 } } } },

        { "Endless Ascent", {
            { FxOverdrive, 100, 0, { 20, 55, 500, 30, 20, -9 } },
            { FxAmp, 100, 0, { 30, 5, 5.5f, 6, 5.5f, 2 } },
            { FxBarber, 100, 0, { 2, 0.12f, 6, 55, 60 } },
            { FxReverb, 25, 0, { 65, 2.5f, 45, 20, 35, 0 } } } },

        { "Midnight Radio", {
            { FxAmp, 100, 0, { 25, 5, 6, 5, 5, 1 } },
            { FxRadio, 100, 0, { 0, 20, 35, 30, 30 } },
            { FxSpring, 30, 0, { 2.5f, 3500, 60, 40, 40 } } } },

        { "Orbiting Satellite", {
            { FxOrbit, 100, 0, { 0.35f, 0, 60, 60, 90 } },
            { FxChaos, 35, 0, { 380, 50, 0.4f, 55, 5000, 90 } },
            { FxReverb, 30, 0, { 75, 4, 40, 30, 50, 15 } } } },

        { "Melting Clock", {
            { FxMelt, 100, 0, { 3, 2, 40, 50, 9 } },
            { FxTape, 100, 0, { 6, 30, 20, 15, 40, 20 } },
            { FxReverb, 30, 0, { 60, 2.5f, 55, 20, 40, 0 } } } },

        { "Chaos Garden", {
            { FxFreeze, 100, 0, { 0, 50, 300, 2, 20, 5000 } },
            { FxChaos, 45, 0, { 450, 60, 0.6f, 65, 4500, 90 } },
            { FxGhostVerb, 40, 0, { 80, 6, 60, 500, 40, 20 } } } },
    };
    return presets;
}
