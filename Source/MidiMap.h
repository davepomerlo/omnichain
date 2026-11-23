// OmniChain - MIDI footswitch / expression pedal mapping for the standalone app.
// MIDI arrives on the audio thread; it is queued without locks and acted on by a timer on the
// message thread (presets, files and the UI are message-thread things). Mappings are a rig setting:
// saved for this computer, not inside projects or presets.
#pragma once
#include <JuceHeader.h>

struct MidiBinding
{
    enum Source { CC = 0, Note = 1, Program = 2 };
    int source = CC;
    int channel = 0;            // 1..16, 0 = any
    int number = 0;             // CC number, note number or program number
    juce::String target;        // what it controls, see MidiMap::describeTarget (e.g. "s3.byp", "s3.p2", "preset.next")
    bool latching = false;      // pedal sends 127 on one press and 0 on the next (instead of 127 down / 0 up)
    float last = -1.0f;         // runtime: last value seen (for edge detection)
};

struct MidiEvent { juce::uint8 b[3] {}; double timeMs = 0.0; };

// What kind of control a target is
enum class MidiTargetKind { Continuous, Toggle, Momentary, Trigger, Unknown };

class MidiMap
{
public:
    // ---- audio thread ----
    void push (const juce::MidiMessage& m, double timeMs);

    // ---- message thread ----
    int pop (MidiEvent* out, int max);
    juce::Array<MidiBinding> bindings;
    bool programChangeSelectsPresets = true;
    juce::String learnTarget;                    // armed MIDI learn, empty when off
    juce::String lastReceived;                   // "CC 64 = 127 (ch 1)", for the MIDI menu

    MidiBinding* find (int source, int channel, int number);
    int indexForTarget (const juce::String& target) const;
    void removeTarget (const juce::String& target);

    juce::String toXmlString() const;
    void fromXmlString (const juce::String& s);

    static MidiTargetKind kindOf (const juce::String& target);
    static juce::String describeSource (const MidiBinding& b);

private:
    static constexpr int kSize = 512;
    juce::AbstractFifo fifo { kSize };
    MidiEvent events[kSize];
};
