#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "dsp/Tuner.h"

namespace ocui
{
const juce::Colour bg0 { 0xff111318 }, bg1 { 0xff1a1d25 }, card { 0xff20242d }, cardHi { 0xff272c37 },
                   knobBody { 0xff2b303b }, track { 0xff3a404d }, text { 0xffe6e9ef }, textDim { 0xff8b93a5 };

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (13.0f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override { return juce::Font (14.0f); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class Knob : public juce::Component
{
public:
    Knob();
    void resized() override;
    juce::Slider slider;
    juce::Label name, value;
    void setAccent (juce::Colour c);
};
} // namespace ocui

class OmniChainEditor;

class SlotCard : public juce::Component, public juce::DragAndDropTarget, public juce::FileDragAndDropTarget
{
public:
    SlotCard (OmniChainProcessor&, OmniChainEditor&, int index);
    ~SlotCard() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void refresh();           // called from the editor timer

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void openFileChooser();

private:
    void rebuildForType (int type);
    void updateValueLabels();
    void showMenu();
    juce::Rectangle<int> headerArea() const { return getLocalBounds().removeFromTop (34); }

    OmniChainProcessor& proc;
    OmniChainEditor& editor;
    const int index;
    int shownType = -1;
    float meter = 0;
    bool dragOver = false;
    juce::Colour accent;

    juce::ComboBox typeBox;
    juce::TextButton power { "" }, left { "<" }, right { ">" };
    ocui::Knob knobs[oc::kParamsPerSlot];
    juce::Slider mix, level;
    juce::Label mixLabel, levelLabel, blurb, fileLabel;
    juce::TextButton loadBtn { "Load" }, filePrev { "<" }, fileNext { ">" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String shownFileText;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> knobAtt[oc::kParamsPerSlot], mixAtt, levelAtt;
    std::unique_ptr<BA> powerAtt;
};

// Big tuner readout shown over the chain while the tuner is on (standalone app)
class TunerOverlay : public juce::Component, private juce::Timer
{
public:
    explicit TunerOverlay (OmniChainProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { proc.tunerOn = false; }
    void visibilityChanged() override { if (isVisible()) startTimerHz (20); else stopTimer(); }
private:
    void timerCallback() override;
    OmniChainProcessor& proc;
    oc::YinTuner yin;
    std::vector<float> scratch = std::vector<float> (4096, 0.0f);
    oc::TunerReading shown; float needle = 0.0f; int holdFrames = 0;
};

// Toolbar shown only in the standalone app: audio settings, input mute, tempo, metronome, tuner, recorder
class StandaloneBar : public juce::Component, private juce::Timer
{
public:
    explicit StandaloneBar (OmniChainProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void tap();
    OmniChainProcessor& proc;
    juce::TextButton audioBtn { "Audio settings" }, inputBtn { "Input" }, tapBtn { "Tap" }, metroBtn { "Metronome" },
                     tunerBtn { "Tuner" }, recBtn { "Record" }, folderBtn { "Recordings" };
    juce::Slider bpm, metroVol;
    juce::Label recTime;
    juce::Array<double> taps;
};

class OmniChainEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit OmniChainEditor (OmniChainProcessor&);
    ~OmniChainEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildPresetMenu();
    void stepPreset (int delta);
    void savePreset();
    void loadPreset();

    OmniChainProcessor& proc;
    ocui::LookAndFeel lnf;
    std::unique_ptr<SlotCard> cards[oc::kNumSlots];
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "Save" }, loadBtn { "Load" };
    ocui::Knob inKnob, mixKnob, outKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt, mixAtt, outAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> userPresets;
    float inLvl = 0, outLvl = 0;
    juce::String shownPreset;
    juce::TooltipWindow tooltips { this, 700 };
    std::unique_ptr<StandaloneBar> standaloneBar;
    TunerOverlay tunerOverlay { proc };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmniChainEditor)
};
