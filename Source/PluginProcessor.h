#pragma once
#include <JuceHeader.h>
#include "dsp/Engine.h"
#include "Presets.h"

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
    bool acceptsMidi() const override { return false; }
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

    // ---- file-based slots (Cab IR / NAM) ----
    static bool isIrFile (const juce::File& f)  { return f.hasFileExtension ("wav;aif;aiff;flac"); }
    static bool isNamFile (const juce::File& f) { return f.hasFileExtension ("nam"); }
    bool loadSlotFile (int slot, const juce::File& f);          // sets the slot type if needed
    void stepSlotFile (int slot, int delta);                    // next / previous file in the same folder
    juce::File getSlotFile (int slot) const;
    juce::String getSlotFileText (int slot, int& severity);     // severity: 0 ok, 1 warning, 2 error
    static juce::File getContentFolder (bool nam);
    juce::File lastFileFolder;

    // ---- standalone-app helpers (also harmless inside a DAW) ----
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }
    std::atomic<float> hostlessBpm { 120.0f };   // tempo used when the host gives none (standalone app)
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

    juce::AudioProcessorValueTreeState apvts;
    oc::Engine engine;
    juce::String currentPresetName { "Init (empty chain)" };

private:
    void timerCallback() override { engine.service(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamValue (juce::RangedAudioParameter* p, float normalised);
    void setSlotPath (int slot, const juce::String& path);
    void clearSlotResource (int slot);
    void reloadResourcesFromState();
    std::shared_ptr<const oc::Resource> loadIr (const juce::File& f, juce::String& error);
    juce::String slotError[oc::kNumSlots];
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
