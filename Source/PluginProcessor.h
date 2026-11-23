#pragma once
#include <JuceHeader.h>
#include "dsp/Engine.h"
#include "Presets.h"
#include <array>

// MIDI control (footswitch / expression pedal) is built into "OmniChain MIDI", the macOS Audio Unit that Logic Pro and
// MainStage list as a MIDI-controlled effect. The regular OmniChain builds take no MIDI, so they are unchanged.
#if defined (JucePlugin_WantsMidiInput) && JucePlugin_WantsMidiInput
 constexpr bool kMidiControl = true;
#else
 constexpr bool kMidiControl = false;
#endif

class OmniChainProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    OmniChainProcessor();
    ~OmniChainProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return kMidiControl; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- slot editing helpers (message thread) ----
    static juce::String pid (int slot, const juce::String& what) { return "s" + juce::String (slot + 1) + "_" + what; }
    juce::RangedAudioParameter* param (int slot, const juce::String& what) { return apvts.getParameter (pid (slot, what)); }
    int getSlotType (int slot) const;
    void setSlotType (int slot, int type, bool applyDefaults);
    void resetSlotParams (int slot);
    void moveSlot (int from, int to);
    void copySlot (int from, int to);
    void applyPreset (const PresetDef& p);
    bool loadPresetFile (const juce::File& f);
    bool savePresetFile (const juce::File& f);
    static juce::File getUserPresetFolder();

    // ---- easy patch management ----
    juce::File userPresetFile (const juce::String& name) const { return getUserPresetFolder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".ocpreset"); }
    bool saveUserPreset (const juce::String& name) { return savePresetFile (userPresetFile (name)); }
    double paramChecksum() const;
    void markClean() { cleanChecksum = paramChecksum(); }
    bool isModified() const { return std::abs (paramChecksum() - cleanChecksum) > 1.0e-6; }
    // single-effect patches (one slot's effect + settings + file), sorted into a folder per effect
    static juce::File getSlotPatchRoot();
    static juce::File getSlotPatchFolder (int type);
    bool saveSlotPatch (int slot, const juce::File& f);
    bool loadSlotPatch (int slot, const juce::File& f);
    // window size (saved with the project, and remembered as the default for new windows)
    float getUiScale();
    void setUiScale (float s);

    // ---- file-based slots (Cab IR / NAM) ----
    static bool isIrFile (const juce::File& f)  { return f.hasFileExtension ("wav;aif;aiff;flac"); }
    static bool isNamFile (const juce::File& f) { return f.hasFileExtension ("nam"); }
    bool loadSlotFile (int slot, const juce::File& f);          // sets the slot type if needed
    void stepSlotFile (int slot, int delta);                    // next / previous file in the same folder
    juce::File getSlotFile (int slot) const;
    juce::String getSlotFileText (int slot, int& severity);     // severity: 0 ok, 1 warning, 2 error
    static juce::File getContentFolder (bool nam);
    static juce::File getLibraryFolder();                       // factory NAM captures + IRs ({} if not installed)
    static juce::Array<juce::File> getLibrarySearchPlaces();
    static bool installLibrary (const juce::File& zipOrFolder, juce::String& message, const juce::File& destParent = {});
    static juce::File findLibraryZip();
    void reloadMissingFiles();
    static juce::File resolveLibraryFile (const juce::File& saved);
    static bool getNamLight();                                  // run NAM A2 captures at their Lite size (less CPU)
    void setNamLight (bool on);
    static juce::Array<juce::File> importToneFiles (const juce::Array<juce::File>& files, bool nam);   // .nam / .wav / .zip
    // offline-first downloads inbox: new captures / IRs you downloaded by hand (Downloads folder by default)
    static juce::File getInboxFolder();
    static void setInboxFolder (const juce::File& dir);
    static juce::Array<juce::File> findNewDownloads();
    static void markDownloadsSeen (const juce::Array<juce::File>& files);
    static juce::Array<juce::File> importDownloads (const juce::Array<juce::File>& files);
    juce::File lastFileFolder;

    // ---- standalone-app helpers (also harmless inside a DAW) ----
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }
    std::atomic<float> hostlessBpm { 120.0f };   // tempo used when the host gives none (standalone app)
    std::atomic<bool> hostHasTempo { false };
    std::atomic<float> currentBpm { 120.0f };     // tempo of the last audio block (DAW or app)

    // ---- delay helpers ----
    float tapDelay (int slot);                   // returns the tapped interval in ms (0 until two taps)
    bool setDelayFromText (int slot, const juce::String& text);   // "375 ms", "1.2 s", "1/8D", "92.5 bpm"
    void setTwist (int slot, bool on) { engine.sendCommand (slot, on ? 1 : 2); }
    bool getTrails() const { return engine.trails.load(); }
    void setTrails (bool on);                    // delays / reverbs keep ringing after bypass
    std::atomic<bool> metronomeOn { false };
    std::atomic<float> metronomeVol { 0.5f };
    std::atomic<bool> tunerOn { false };
    void copyTunerInput (float* dest, int n) const;  // newest n mono input samples (message thread)
    bool startRecording();
    void stopRecording();
    bool isRecording() const { return activeWriter.load() != nullptr; }
    double getRecordingSeconds() const { return getSampleRate() > 0 ? (double) recordedSamples.load() / getSampleRate() : 0.0; }
    juce::File lastRecording;
    static juce::File getRecordingsFolder();

    // ---- MIDI learn (only active in the OmniChain MIDI build; message thread unless noted) ----
    // Learn: touch a knob or switch in OmniChain, then press a footswitch / move a pedal (any MIDI CC).
    // Program Change n picks preset n: the factory presets first, then your own presets in name order.
    // Links are saved for this computer, so the same footswitch works in every project and preset.
    bool hasMidiControl() const { return kMidiControl; }
    void startMidiLearn() { midiLearn.store (kLearnWaitControl); }
    void cancelMidiLearn() { midiLearn.store (kLearnOff); }
    int getMidiLearnStage() const { const int t = midiLearn.load(); return t == kLearnOff ? 0 : (t == kLearnWaitControl ? 1 : 2); }
    juce::String getMidiLearnTargetName() const { const int t = midiLearn.load(); return t >= 0 ? describeParam (t) : juce::String(); }
    juce::String getMidiStatus();                   // short line for the header ("" when nothing to show)
    struct MidiLinkInfo { int cc; juce::String target; bool isSwitch, follow; };
    juce::Array<MidiLinkInfo> getMidiLinks() const;
    void forgetMidiLink (int cc);
    void forgetAllMidiLinks();
    void setMidiLinkFollow (int cc, bool follow);   // switches: true = follow a latching switch, false = each press toggles
    juce::String describeParam (int paramIndex) const;

    juce::AudioProcessorValueTreeState apvts;
    oc::Engine engine;
    juce::String currentPresetName { "Init (empty chain)" };
    double cleanChecksum = 0.0;

private:
    void timerCallback() override { engine.service(); serviceMidi(); }

    // MIDI learn state (shared with the audio thread)
    static constexpr int kLearnOff = -1, kLearnWaitControl = -2;
    std::atomic<int> midiLearn { kLearnOff };      // kLearnOff, kLearnWaitControl, or the parameter index waiting for a CC
    std::array<std::atomic<int>, 128> ccTarget;    // parameter index per CC number, -1 = none
    std::array<std::atomic<bool>, 128> ccFollow;   // switch links: follow the CC value instead of toggling on each press
    std::atomic<int> pendingProgram { -1 }, lastLearnedCc { -1 };
    std::atomic<bool> midiLinksDirty { false };
    juce::uint32 learnedShownUntil = 0;
    std::vector<char> paramIsBypass;               // per parameter index: a slot's on/off switch (filled once, read-only after)
    void handleMidi (const juce::MidiBuffer& midi);     // audio thread
    void serviceMidi();                                 // message thread (timer)
    void loadMidiLinks();
    void saveMidiLinks();
    bool isMidiLinkable (int paramIndex) const;
    struct GestureWatcher : juce::AudioProcessorListener
    {
        explicit GestureWatcher (OmniChainProcessor& p) : owner (p) {}
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override;
        OmniChainProcessor& owner;
    } gestureWatcher { *this };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamValue (juce::RangedAudioParameter* p, float normalised);
    void setSlotPath (int slot, const juce::String& path);
    void migrateOldState (int fromVersion);
    void migrateSlot (int slot, int fromVersion);
    void clearSlotResource (int slot);
    void reloadResourcesFromState();
    std::shared_ptr<const oc::Resource> loadIr (const juce::File& f, juce::String& error);
    juce::String slotError[oc::kNumSlots];
    juce::Array<double> tapTimes[oc::kNumSlots];
    juce::AudioFormatManager formats;

    int currentProgram = 0;
    std::vector<float> monoScratch;

    static constexpr int kTunerRing = 8192;
    std::vector<float> tunerRing = std::vector<float> ((size_t) kTunerRing, 0.0f);
    std::atomic<int> tunerWrite { 0 };
    double metroPos = 0.0; int metroBeat = -1; float clickEnv = 0.0f, clickPhase = 0.0f, clickFreq = 1400.0f;
    juce::TimeSliceThread writerThread { "OmniChain recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<juce::int64> recordedSamples { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmniChainProcessor)
};
