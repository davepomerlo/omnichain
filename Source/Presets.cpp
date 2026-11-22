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
        // ---- v2.0: compressors, new effects, and factory-library rigs ----
        { "Country Squeeze", {
            { FxPedalComp, 100, 0, { 65, 0, 70, 20, 0 } },
            { FxAmp, 100, 0, { 22, 5, 5, 6.5f, 6, 1, 5, 1 } },
            { FxDelay, 22, 0, { 125, 0, 15, 5000, 1, 10 } },
            { FxSpring, 22, 0, { 2, 4500, 60, 40, 50 } } } },

        { "All-In Drum Crush", {
            { FxComp76, 60, 0, { 26, -8, 0.05f, 80, 4 } },
            { FxTape, 100, 0, { 6, 0, 0, 0, 60, 0, 2 } },
            { FxLimiter, 100, 0, { 2, -0.5f, 60, 60, 2 } } } },

        { "Master Glue (Bus + Multiband)", {
            { FxEq, 100, 0, { 28, 0.5f, 2500, 0, 1, 20000, 0.9f, 0 } },
            { FxMultiband, 100, 0, { 160, 3500, -22, -20, -24, 2.5f, 40, 2 } },
            { FxBusComp, 100, 0, { -14, 1, 4, 4, 1.5f, 80 } },
            { FxExciter, 100, 0, { 6000, 35, 60, 20, 70, 15 } },
            { FxLimiter, 100, 0, { 1.5f, -0.3f, 80, 50, 0 } } } },

        { "Smooth Opto Vocal / Bass", {
            { FxOpto, 100, 0, { 45, 5, 0, 20 } },
            { FxVariMu, 100, 0, { 4, -12, 3, 0, 40 } },
            { FxEq, 100, 0, { 60, 0, 3000, 1.5f, 2, 20000, 0.7f, 0 } } } },

        { "Looper Practice Rig", {
            { FxCompressor, 100, 0, { -22, 3, 10, 150, 3, 40, 20 } },
            { FxAmp, 100, 0, { 30, 5, 5.5f, 6, 5.5f, 2, 5, 0 } },
            { FxDelay, 18, 0, { D, 7, 25, 4500, 1, 15, 60, 0 } },
            { FxReverb, 18, 0, { 55, 2, 50, 15, 30, 0, 120, 100 } },
            { FxLooper, 100, 0, { 0, 100, 100, 1, 0, 3 } } } },

        { "Plugged-In Acoustic", {
            { FxAcoustic, 100, 0, { 0, 65, 55, 55, 35, 0 } },
            { FxCompressor, 100, 0, { -22, 2.5f, 15, 200, 3, 30, 20 } },
            { FxDoubler, 25, 0, { 0, 24, 6, 40, 100, 9000 } },
            { FxReverb, 20, 0, { 55, 1.8f, 45, 15, 25, 0, 150, 110 } } } },

        { "Twelve-Player Section", {
            { FxUnison, 70, 0, { 7, 22, 100, 15, 0, 11000 } },
            { FxDoubler, 40, 0, { 1, 28, 10, 50, 100, 9000 } },
            { FxReverb, 28, 0, { 70, 3, 45, 20, 40, 0, 150, 130 } } } },

        { "Bell Tower", {
            { FxBells, 50, 0, { 2, 1, 0, 4, 60, 80 } },
            { FxDelay, 25, 0, { D, 5, 40, 5000, 3, 15, 0, 0 } },
            { FxGhostVerb, 40, 0, { 85, 8, 50, 500, 40, 20 } } } },

        { "Ring-Bell Alien (Freq Shift)", {
            { FxFreqShift, 60, 0, { 180, 0, 55, 90, 100, 9000 } },
            { FxFlanger, 50, 0, { 0.1f, 60, 2, 50, 60, 0 } },
            { FxReverb, 30, 0, { 75, 4, 40, 25, 50, 0, 200, 140 } } } },

        { "West Coast Fold", {
            { FxCompressor, 100, 0, { -24, 4, 5, 120, 4, 30, 20 } },
            { FxWavefolder, 100, 0, { 55, 15, 60, 6000, -13 } },
            { FxLadder, 100, 0, { 3500, 35, 3, 0, 0.2f, 1.2f } },
            { FxDelay, 25, 0, { D, 8, 40, 5000, 3, 15, 0, 0 } } } },

        { "Dusty 45", {
            { FxAmp, 100, 0, { 22, 5, 6, 5, 5, 1, 5, 2 } },
            { FxVinyl, 100, 0, { 40, 35, 55, 25, 30, 1 } },
            { FxSpring, 25, 0, { 2.5f, 3500, 60, 40, 40 } } } },

        { "Twenty Thousand Leagues", {
            { FxUnderwater, 100, 0, { 65, 50, 45, 50 } },
            { FxChorus, 40, 0, { 0.25f, 60, 14, 3, 100, 6000 } },
            { FxGhostVerb, 45, 0, { 90, 9, 40, 600, 50, 0 } } } },

        { "Chainsaw (HM mode)", {
            { FxGate, 100, 0, { -48, 0.5f, 30, 60, -80, 6, 120 } },
            { FxDistortion, 100, 0, { 80, 55, 5, 0, 0, -17 } },
            { FxAmp, 100, 0, { 40, 6, 5, 6, 6, 3, 5, 3 } } } },

        { "Modern Tight Rhythm (new amp voicing)", {
            { FxGate, 100, 0, { -50, 0.5f, 30, 60, -80, 6, 120 } },
            { FxOverdrive, 100, 0, { 5, 65, 900, 0, 10, 0, 0 } },
            { FxAmp, 100, 0, { 70, 6, 4.5f, 6.5f, 6.5f, 3, 5, 3 } },
            { FxEq, 100, 0, { 70, 1.5f, 500, -2, 1, 11000, 0.9f, 0 } } } },

        { "Fat Bass Amp", {
            { FxOpto, 100, 0, { 40, 5, 0, 0 } },
            { FxAmp, 100, 0, { 35, 6.5f, 5.5f, 4.5f, 4, 4, 5, 4 } },
            { FxExciter, 100, 0, { 3000, 30, 50, 15, 70, 35 } } } },

        { "Library: Twin Clean + American 2x12", {
            { FxPedalComp, 100, 0, { 40, 0, 50, 0, 20 } },
            { FxNam, 100, 0, { 0, 0, 0, 0, 0, 1, -80 }, false, "NAM/Amps - Clean/Twin Reverb - Normal Bright (Tim R).nam" },
            { FxCabIR, 100, 0, { 70, 20000, 250, 0 }, false, "IR/Guitar Cabs/American 2x12 Open Back/American 2x12 Open Back - Mix Dynamic + Ribbon.wav" },
            { FxSpring, 22, 0, { 2, 4500, 60, 40, 40 } } } },

        { "Library: AC15 Chime + Alnico Blue", {
            { FxNam, 100, 0, { 0, 0, 0, 0, 0, 1, -80 }, false, "NAM/Amps - Clean/Vox AC15 - Top Boost (Phillipe P).nam" },
            { FxCabIR, 100, 0, { 70, 20000, 250, 0 }, false, "IR/Guitar Cabs/Alnico Blue 2x12 Open Back/Alnico Blue 2x12 Open Back - Dynamic - Cap Edge.wav" },
            { FxTremolo, 100, 0, { 5, 40, 0, 0, 0, 0 }, true },
            { FxReverb, 20, 0, { 50, 1.8f, 50, 10, 25, 0, 120, 100 } } } },

        { "Library: JCM2000 Crunch + Green 4x12", {
            { FxNam, 100, 0, { 0, 0, 0, 0, 0, 1, -70 }, false, "NAM/Amps - Crunch/JCM2000 - Crunch (Tim R).nam" },
            { FxCabIR, 100, 0, { 80, 20000, 250, 0 }, false, "IR/Guitar Cabs/British 4x12 - Green 25W/British 4x12 - Green 25W - Mix Dynamic + Ribbon.wav" },
            { FxDelay, 18, 0, { 340, 0, 25, 4000, 1, 15, 40, 0 } },
            { FxReverb, 15, 0, { 40, 1.5f, 50, 10, 20, 0, 120, 100 } } } },

        { "Library: 5150 Rhythm + V-Type 4x12", {
            { FxGate, 100, 0, { -52, 0.5f, 30, 60, -80, 6, 120 } },
            { FxNam, 100, 0, { 0, 0, 0, 0, 0, 1, -62 }, false, "NAM/Amps - High Gain/5150 Block Letter - Boosted (Helga B).nam" },
            { FxCabIR, 100, 0, { 90, 12000, 250, 0 }, false, "IR/Guitar Cabs/British 4x12 - V-Type 60W/British 4x12 - V-Type 60W - Dynamic - Cap Edge.wav" } } },

        { "Library: Mark IV Lead + Oversize 4x12", {
            { FxNam, 100, 0, { 0, 0, 0, 0, 0, 1, -62 }, false, "NAM/Amps - High Gain/Mesa Mark IV - Lead (Roman A).nam" },
            { FxCabIR, 100, 0, { 80, 14000, 250, 0 }, false, "IR/Guitar Cabs/Modern Oversize 4x12/Modern Oversize 4x12 - Mix Dynamic + Ribbon.wav" },
            { FxDelay, 22, 0, { D, 4, 30, 4000, 2, 25, 50, 0 } },
            { FxReverb, 20, 0, { 65, 2.5f, 45, 20, 35, 0, 150, 110 } } } },

        { "Library: Swedish Chainsaw", {
            { FxGate, 100, 0, { -50, 0.5f, 30, 60, -80, 6, 120 } },
            { FxNam, 100, 0, { 0, 0, 0, 0, 0, 1, -60 }, false, "NAM/Pedals/Boss HM-2 - Swedish Chainsaw (Peter N).nam" },
            { FxNam, 100, 8, { 0, 0, 0, 0, 0, 1, -80 }, false, "NAM/Amps - Crunch/JCM900 Dual Reverb - Ch A Gain 6 (Tim R).nam" },
            { FxCabIR, 100, 0, { 90, 11000, 250, 0 }, false, "IR/Guitar Cabs/British 4x12 - Seventy-Five/British 4x12 - Seventy-Five - Dynamic - Cap.wav" } } },

        { "Library: dUg Bass + 8x10 Fridge", {
            { FxOpto, 100, 0, { 35, 5, 0, 0 } },
            { FxNam, 100, 0, { 0, 0, 0, 0, 0, 1, -80 }, false, "NAM/Bass/Tech21 dUg DP-3X Bass Preamp (Jason Z).nam" },
            { FxCabIR, 100, 0, { 35, 9000, 250, 0 }, false, "IR/Bass Cabs/Bass 8x10 Fridge/Bass 8x10 Fridge - Dynamic - Cap.wav" } } },

        { "Library: Plate Reverb (IR)", {
            { FxAmp, 100, 0, { 20, 5, 5, 6, 5.5f, 1, 5, 1 } },
            { FxCabIR, 30, 0, { 150, 9000, 1000, 0 }, false, "IR/Spaces/Bright Plate.wav" } } },
        // ---- v2.1 ----
        { "Prism Cascade", {
            { FxCompressor, 100, 0, { -24, 3, 10, 150, 3, 40, 20 } },
            { FxPrism, 100, 0, { 12, 1, 7, 70, 40, 75, 60, 8000 } },
            { FxReverb, 30, 0, { 75, 4, 40, 25, 50, 0, 150, 130 } } } },

        { "Prism Swoop Lead", {
            { FxDistortion, 100, 0, { 55, 55, 0, 0, 2, -17 } },
            { FxAmp, 100, 0, { 40, 5, 6, 5.5f, 5, 2, 5, 0 } },
            { FxPrism, 100, 0, { 7, 3, 8, 60, 35, 15, 55, 9000 } },
            { FxDelay, 25, 0, { D, 7, 30, 4500, 1, 20, 50, 0 } } } },

        { "Dream Tape Loop", {
            { FxAmp, 100, 0, { 18, 5, 5, 6, 5.5f, 1, 5, 1 } },
            { FxDream, 55, 0, { 0, 60, 45, 2, 2.5f, 64, 70, 0 } } } },

        { "Dream Stretch Pad", {
            { FxDream, 60, 0, { 0, 80, 30, 3, 4, 20, 100, 0 } },
            { FxChorus, 35, 0, { 0.3f, 50, 12, 3, 100, 9000 } } } },

        { "Micro Mosaic", {
            { FxMicroverse, 55, 0, { 0, 0, 3, 55, 35, 40, 0, 35 } },
            { FxDelay, 20, 0, { D, 8, 30, 5000, 3, 10, 40, 0 } } } },

        { "Micro Haze Cloud", {
            { FxMicroverse, 60, 0, { 2, 1, 3, 70, 60, 45, -25, 55 } } } },

        { "Micro Glitch Blocks", {
            { FxOverdrive, 100, 0, { 25, 55, 600, 20, 20, -9, 0 } },
            { FxMicroverse, 60, 0, { 5, 3, 3, 65, 15, 20, 0, 15 } } } },

        { "Looper + Microverse", {
            { FxCompressor, 100, 0, { -22, 3, 10, 150, 3, 40, 20 } },
            { FxLooper, 100, 0, { 0, 100, 100, 1, 0, 0, 0, 1 } },
            { FxMicroverse, 45, 0, { 8, 0, 3, 50, 40, 35, 0, 30 } } } },

        // ---- v2.2: pedal-style delays (Delay knobs: Time, Sync, Feedback, Tone, Type, Color, Ducking, Spread) ----
        { "Echo: Studio Lead (Digital Mod)", {
            { FxCompressor, 100, 0, { -24, 3, 10, 150, 3, 40, 20 } },
            { FxOverdrive, 100, 0, { 25, 55, 720, 0, 20, -6 } },
            { FxAmp, 100, 0, { 45, 5, 6, 6, 5.5f, 2, 5, 0 } },
            { FxDelay, 28, 0, { D, 8, 35, 9000, 5, 30, 40, 40 } },
            { FxReverb, 20, 0, { 50, 2, 45, 15, 30, 0, 120, 110 } } } },

        { "Echo: Crystal Cathedral", {
            { FxAmp, 100, 0, { 18, 5, 5, 6.5f, 6, 1, 5, 1 } },
            { FxDelay, 35, 0, { D, 5, 45, 9000, 7, 70, 0, 50 } },
            { FxReverb, 35, 0, { 85, 6, 35, 30, 50, 0, 150, 130 } } } },

        { "Echo: Shimmer Ascension", {
            { FxSwell, 100, 0, { -50, 500, 50, 9 } },
            { FxDelay, 38, 0, { D, 4, 55, 7000, 8, 60, 0, 30 } },
            { FxReverb, 30, 0, { 80, 5, 40, 20, 40, 0, 150, 120 } } } },

        { "Echo: Multi-Head Dub", {
            { FxAmp, 100, 0, { 30, 6, 5, 5, 5, 1, 5, 1 } },
            { FxDelay, 35, 0, { D, 5, 55, 3500, 11, 100, 0, 20 } },
            { FxSpring, 30, 0, { 2.5f, 3000, 60, 40, 40 } } } },

        { "Echo: Rockabilly Slap", {
            { FxCompressor, 100, 0, { -20, 3, 10, 150, 3, 40, 20 } },
            { FxAmp, 100, 0, { 40, 5.5f, 6, 6.5f, 6, 4, 5, 0 } },
            { FxDelay, 32, 0, { 110, 0, 8, 5000, 2, 20, 0, 0 } },
            { FxSpring, 20, 0, { 1.8f, 3500, 50, 40, 30 } } } },

        { "Echo: Lo-Fi Memory", {
            { FxAmp, 100, 0, { 22, 5, 5, 5, 5, 1, 5, 1 } },
            { FxDelay, 35, 0, { D, 7, 45, 3000, 9, 60, 20, 0 } },
            { FxVinyl, 100, 0, { 30, 30, 45, 20, 25, 0 } } } },

        { "Echo: Warp Bloom", {
            { FxSwell, 100, 0, { -50, 700, 50, 9 } },
            { FxDelay, 42, 0, { D, 4, 60, 6000, 13, 70, 30, 40 } },
            { FxReverb, 30, 0, { 75, 4.5f, 40, 20, 50, 0, 150, 130 } } } },

        { "Echo: Glitch Grid", {
            { FxOverdrive, 100, 0, { 20, 55, 650, 0, 20, -6 } },
            { FxAmp, 100, 0, { 35, 5, 6, 6, 5.5f, 2, 5, 0 } },
            { FxDelay, 35, 0, { D, 4, 35, 8000, 14, 60, 0, 30 } } } },

        { "Echo: Verb Wash Pad", {
            { FxAmp, 100, 0, { 15, 5, 5, 6, 6, 1, 5, 1 } },
            { FxChorus, 30, 0, { 0.4f, 40, 10, 2, 80, 12000 } },
            { FxDelay, 38, 0, { D, 5, 50, 6500, 12, 70, 25, 40 } } } },

        { "Echo: Dynamic Solo", {
            { FxCompressor, 100, 0, { -24, 3, 10, 150, 3, 40, 20 } },
            { FxDistortion, 100, 0, { 45, 55, 0, 0, 2, -15 } },
            { FxAmp, 100, 0, { 40, 5, 6, 5.5f, 5, 2, 5, 0 } },
            { FxDelay, 30, 0, { D, 4, 40, 7000, 6, 10, 60, 30 } } } },

        { "Echo: Warm Ballad", {
            { FxAmp, 100, 0, { 20, 5, 5, 6, 6, 1, 5, 1 } },
            { FxDelay, 28, 0, { D, 4, 35, 8000, 10, 15, 30, 20 } },
            { FxReverb, 22, 0, { 60, 2.5f, 50, 15, 30, 0, 120, 110 } } } },

        { "Echo: Analog Long Throw", {
            { FxOverdrive, 100, 0, { 15, 55, 700, 0, 20, -4 } },
            { FxAmp, 100, 0, { 35, 5, 6, 6, 5.5f, 2, 5, 0 } },
            { FxDelay, 30, 0, { 1200, 0, 55, 5000, 1, 25, 30, 25 } } } },
    };
    return presets;
}
