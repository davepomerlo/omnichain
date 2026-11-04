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

    juce::AudioProcessorValueTreeState apvts;
    oc::Engine engine;
    juce::String currentPresetName { "Init (empty chain)" };

private:
    void timerCallback() override { engine.service(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamValue (juce::RangedAudioParameter* p, float normalised);

    int currentProgram = 0;
    std::vector<float> monoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmniChainProcessor)
};
