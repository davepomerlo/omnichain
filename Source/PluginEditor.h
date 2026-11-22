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
    // click the value to type one; parser turns text into the slider's value (false = not understood)
    std::function<bool (const juce::String&, double&)> parser;
};
} // namespace ocui

class OmniChainPanel;

// Buttons, timer and progress bar for a Looper slot
class LooperPanel : public juce::Component, private juce::Timer
{
public:
    LooperPanel (OmniChainProcessor& p, int slotIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override { if (isVisible()) startTimerHz (20); else stopTimer(); }
private:
    void timerCallback() override;
    OmniChainProcessor& proc;
    const int slot;
    juce::TextButton mainBtn { "Record" }, playBtn { "Play" }, undoBtn { "Undo" }, clearBtn { "Clear" };
    int state = -1; float len = 0, pos = 0; int flags = 0;
};

// A button that reports press and release (for momentary actions like the delay's Twist)
class HoldButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    std::function<void (bool)> onHold;
    void buttonStateChanged() override { const bool d = isDown(); if (d != held) { held = d; if (onHold) onHold (d); } }
private:
    bool held = false;
};

// Tap tempo, hold-to-Twist and a time read-out for a Delay slot
class DelayBar : public juce::Component, private juce::Timer
{
public:
    DelayBar (OmniChainProcessor& p, int slotIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
private:
    void timerCallback() override;
    OmniChainProcessor& proc;
    const int slot;
    juce::TextButton tapBtn { "Tap" };
    HoldButton twistBtn { "Twist" };
    juce::Label readout;                 // click to type a time, a division or a tempo
    juce::String shownText;
    float shownTw = -1.0f, flash = 0.0f;
};

class SlotCard : public juce::Component, public juce::DragAndDropTarget, public juce::FileDragAndDropTarget
{
public:
    SlotCard (OmniChainProcessor&, OmniChainPanel&, int index);
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
    void openImportChooser();
    void importFiles (const juce::Array<juce::File>& files);
    void importInbox();
    void showLoadMenu();
    void addLibraryFolder (juce::PopupMenu&, const juce::File& dir, bool nam, const juce::File& current);
    juce::Array<juce::File> libraryFiles, slotPatchFiles;
    void saveSlotPatch();

private:
    void rebuildForType (int type);
    void updateValueLabels();
    void showMenu();
    juce::Rectangle<int> headerArea() const { return getLocalBounds().removeFromTop (34); }

    OmniChainProcessor& proc;
    OmniChainPanel& editor;
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
    std::unique_ptr<juce::Component> looperPanel, delayBar;
    juce::TextButton loadBtn { "Load" }, filePrev { "<" }, fileNext { ">" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String shownFileText;
    int shownInbox = -1;

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

// Everything you see, laid out at a fixed design size; OmniChainEditor scales it to any window size.
class OmniChainPanel : public juce::Component, public juce::DragAndDropContainer, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit OmniChainPanel (OmniChainProcessor&);
    ~OmniChainPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildPresetMenu();
    void stepPreset (int delta);
    void savePreset (bool askNewName = false);
    void doSave (const juce::String& name);
    void showMoreMenu();
    void importPreset (const juce::File& f);
    bool isUserPreset() const { return proc.userPresetFile (proc.currentPresetName).existsAsFile(); }
public:
    std::function<void (float)> onScaleChosen;
    int getNewDownloadCount() const { return newDownloads.size(); }
    const juce::Array<juce::File>& getNewDownloads() const { return newDownloads; }
    void rescanDownloads() { newDownloads = OmniChainProcessor::findNewDownloads(); }
    void showScale (float s);
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
private:

    OmniChainProcessor& proc;
    ocui::LookAndFeel lnf;
    std::unique_ptr<SlotCard> cards[oc::kNumSlots];
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "Save" }, loadBtn { "More" };
    juce::ComboBox sizeBox;
    ocui::Knob inKnob, mixKnob, outKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt, mixAtt, outAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> userPresets;
    float inLvl = 0, outLvl = 0;
    juce::String shownPreset;
    juce::Array<juce::File> newDownloads;  // captures / IRs waiting in the Downloads folder
    int scanTicks = 0;
    juce::TooltipWindow tooltips { this, 700 };
    std::unique_ptr<StandaloneBar> standaloneBar;
    TunerOverlay tunerOverlay { proc };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmniChainPanel)
};

// The plugin window: holds the panel and scales it (drag the corner, or pick a size in the header)
class OmniChainEditor : public juce::AudioProcessorEditor
{
public:
    explicit OmniChainEditor (OmniChainProcessor&);
    void paint (juce::Graphics& g) override { g.fillAll (ocui::bg0); }
    void resized() override;
private:
    OmniChainProcessor& proc;
    OmniChainPanel panel;
    int baseW = 1220, baseH = 790;
    bool ready = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmniChainEditor)
};
