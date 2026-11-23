#include "MidiMap.h"
#include "PluginProcessor.h"
#include "Presets.h"

using namespace juce;

// =====================================================================================
// queue (single producer: audio thread, single consumer: message thread)
void MidiMap::push (const MidiMessage& m, double timeMs)
{
    if (! (m.isController() || m.isNoteOnOrOff() || m.isProgramChange())) return;
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 < 1) return;                                     // full: drop rather than block audio
    auto& e = events[n1 > 0 ? s1 : s2];
    const auto* raw = m.getRawData();
    for (int i = 0; i < 3; ++i) e.b[i] = i < m.getRawDataSize() ? raw[i] : 0;
    e.timeMs = timeMs;
    fifo.finishedWrite (1);
}

int MidiMap::pop (MidiEvent* out, int max)
{
    int s1, n1, s2, n2;
    fifo.prepareToRead (max, s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) out[i] = events[s1 + i];
    for (int i = 0; i < n2; ++i) out[n1 + i] = events[s2 + i];
    fifo.finishedRead (n1 + n2);
    return n1 + n2;
}

// =====================================================================================
MidiBinding* MidiMap::find (int source, int channel, int number)
{
    for (auto& b : bindings)
        if (b.source == source && b.number == number && (b.channel == 0 || b.channel == channel)) return &b;
    return nullptr;
}

int MidiMap::indexForTarget (const String& target) const
{
    for (int i = 0; i < bindings.size(); ++i) if (bindings.getReference (i).target == target) return i;
    return -1;
}

void MidiMap::removeTarget (const String& target)
{
    for (int i = bindings.size(); --i >= 0;) if (bindings.getReference (i).target == target) bindings.remove (i);
}

String MidiMap::toXmlString() const
{
    XmlElement x ("OmniChainMidi");
    x.setAttribute ("pcPresets", programChangeSelectsPresets);
    for (auto& b : bindings)
    {
        auto* e = x.createNewChildElement ("Map");
        e->setAttribute ("src", b.source); e->setAttribute ("ch", b.channel); e->setAttribute ("num", b.number);
        e->setAttribute ("target", b.target); e->setAttribute ("latching", b.latching);
    }
    return x.toString (XmlElement::TextFormat().singleLine().withoutHeader());
}

void MidiMap::fromXmlString (const String& s)
{
    bindings.clear();
    auto x = parseXML (s);
    if (x == nullptr || ! x->hasTagName ("OmniChainMidi")) return;
    programChangeSelectsPresets = x->getBoolAttribute ("pcPresets", true);
    for (auto* e : x->getChildWithTagNameIterator ("Map"))
    {
        MidiBinding b;
        b.source = jlimit (0, 2, e->getIntAttribute ("src")); b.channel = jlimit (0, 16, e->getIntAttribute ("ch"));
        b.number = jlimit (0, 127, e->getIntAttribute ("num")); b.target = e->getStringAttribute ("target");
        b.latching = e->getBoolAttribute ("latching");
        if (b.target.isNotEmpty()) bindings.add (b);
    }
}

MidiTargetKind MidiMap::kindOf (const String& t)
{
    if (t.startsWith ("preset.") || t == "tempo.tap" || t.endsWith (".tap") || t.contains (".loop.")) return MidiTargetKind::Trigger;
    if (t == "tuner" || t == "metronome" || t == "record" || t.endsWith (".byp")) return MidiTargetKind::Toggle;
    if (t.endsWith (".twist")) return MidiTargetKind::Momentary;
    if (t == "g.in" || t == "g.out" || t == "g.mix" || t.endsWith (".mix")) return MidiTargetKind::Continuous;
    const auto what = t.fromFirstOccurrenceOf (".", false, false);
    if (t.startsWithChar ('s') && what.length() == 2 && what[0] == 'p' && what[1] >= '1' && what[1] <= '8') return MidiTargetKind::Continuous;
    return MidiTargetKind::Unknown;
}

String MidiMap::describeSource (const MidiBinding& b)
{
    String s;
    if (b.source == MidiBinding::CC) s << "CC " << b.number;
    else if (b.source == MidiBinding::Note) s << "Note " << MidiMessage::getMidiNoteName (b.number, true, true, 3);
    else s << "Program " << (b.number + 1);
    if (b.channel > 0) s << " (ch " << b.channel << ")";
    return s;
}

// =====================================================================================
// processor side
static int slotOf (const String& t) { return t.startsWithChar ('s') ? t.substring (1).getIntValue() - 1 : -1; }

String OmniChainProcessor::describeMidiTarget (const String& t)
{
    const int s = slotOf (t);
    if (isPositiveAndBelow (s, oc::kNumSlots))
    {
        const int type = getSlotType (s);
        const auto& info = oc::getInfo (type);
        String slot = "Slot " + String (s + 1) + " (" + info.name + ")";
        const auto what = t.fromFirstOccurrenceOf (".", false, false);
        if (what == "byp") return slot + ": on / off";
        if (what == "mix") return slot + ": mix";
        if (what == "tap") return slot + ": tap tempo";
        if (what == "twist") return slot + ": Twist (hold)";
        if (what == "loop.main") return slot + ": record / play / overdub";
        if (what == "loop.play") return slot + ": play / stop";
        if (what == "loop.undo") return slot + ": undo / redo";
        if (what == "loop.clear") return slot + ": clear";
        if (what.startsWithChar ('p'))
        {
            const int k = what.substring (1).getIntValue() - 1;
            if (isPositiveAndBelow (k, oc::kParamsPerSlot) && info.params[k].name != nullptr) return slot + ": " + info.params[k].name;
            return slot + ": knob " + String (k + 1);
        }
    }
    if (t == "g.in") return "Input level";
    if (t == "g.out") return "Output level";
    if (t == "g.mix") return "Global mix";
    if (t == "preset.next") return "Next preset";
    if (t == "preset.prev") return "Previous preset";
    if (t == "tuner") return "Tuner on / off";
    if (t == "metronome") return "Metronome on / off";
    if (t == "record") return "Record / stop recording";
    if (t == "tempo.tap") return "Tap tempo";
    return t;
}

void OmniChainProcessor::saveMidiMap() { setGlobalSetting ("midiMap", midiMap.toXmlString()); }

void OmniChainProcessor::loadMidiMap() { midiMap.fromXmlString (getGlobalSetting ("midiMap")); }

void OmniChainProcessor::startMidiLearn (const String& target) { midiMap.learnTarget = target; }

void OmniChainProcessor::forgetMidi (const String& target) { midiMap.removeTarget (target); saveMidiMap(); }

// presets in the same order as the preset list: factory, then your own
void OmniChainProcessor::stepPreset (int delta)
{
    const auto& f = getFactoryPresets();
    auto user = getUserPresetFolder().findChildFiles (File::findFiles, false, "*.ocpreset");
    user.sort();
    const int nF = (int) f.size(), total = nF + user.size();
    int cur = -1;
    for (int i = 0; i < total && cur < 0; ++i)
        if ((i < nF ? String (f[(size_t) i].name) : user[i - nF].getFileNameWithoutExtension()) == currentPresetName) cur = i;
    const int next = cur < 0 ? 0 : (cur + delta + total) % total;
    if (next < nF) setCurrentProgram (next); else loadPresetFile (user[next - nF]);
    updateHostDisplay();
}

void OmniChainProcessor::tapTempo (double nowMs)
{
    if (! tempoTaps.isEmpty() && nowMs - tempoTaps.getLast() > 2000.0) tempoTaps.clearQuick();
    tempoTaps.add (nowMs);
    while (tempoTaps.size() > 5) tempoTaps.remove (0);
    if (tempoTaps.size() >= 2)
        hostlessBpm = (float) jlimit (30.0, 300.0, std::round (6000000.0 / ((tempoTaps.getLast() - tempoTaps.getFirst()) / (tempoTaps.size() - 1))) / 100.0);
}

void OmniChainProcessor::runMidiAction (const MidiBinding& b, float value, bool press, bool down, double timeMs)
{
    const auto& t = b.target;
    const int s = slotOf (t);
    const auto what = t.fromFirstOccurrenceOf (".", false, false);
    auto setNorm = [this] (const String& id, float v) { if (auto* p = apvts.getParameter (id)) setParamValue (p, v); };
    switch (MidiMap::kindOf (t))
    {
        case MidiTargetKind::Continuous:
            if (t == "g.in" || t == "g.out" || t == "g.mix") setNorm (t.substring (2), value);
            else if (isPositiveAndBelow (s, oc::kNumSlots)) setNorm (pid (s, what), value);
            break;
        case MidiTargetKind::Toggle:
            if (! press) break;
            if (isPositiveAndBelow (s, oc::kNumSlots)) { auto* p = param (s, "byp"); setParamValue (p, p->getValue() > 0.5f ? 0.0f : 1.0f); }
            else if (t == "tuner") tunerOn = ! tunerOn.load();
            else if (t == "metronome") metronomeOn = ! metronomeOn.load();
            else if (t == "record") { if (isRecording()) stopRecording(); else startRecording(); }
            break;
        case MidiTargetKind::Momentary:
            if (isPositiveAndBelow (s, oc::kNumSlots)) setTwist (s, down);
            break;
        case MidiTargetKind::Trigger:
            if (! press) break;
            if (t == "preset.next") stepPreset (1);
            else if (t == "preset.prev") stepPreset (-1);
            else if (t == "tempo.tap") tapTempo (timeMs);
            else if (isPositiveAndBelow (s, oc::kNumSlots))
            {
                if (what == "tap") tapDelay (s, timeMs);
                else if (getSlotType (s) == oc::FxLooper)
                {
                    const int st = (int) engine.getSlotStatus (s, 0);
                    engine.sendCommand (s, what == "loop.main" ? 8 : what == "loop.play" ? ((st == 3 || st == 4) ? 2 : 3) : what == "loop.undo" ? 4 : 5);
                }
            }
            break;
        case MidiTargetKind::Unknown: break;
    }
}

// message thread, about 100 times a second
void OmniChainProcessor::handleMidiEvents()
{
    MidiEvent ev[64];
    const int n = midiMap.pop (ev, 64);
    for (int i = 0; i < n; ++i)
    {
        const MidiMessage m (ev[i].b[0], ev[i].b[1], ev[i].b[2]);
        const int ch = m.getChannel();
        int source = -1, number = 0; float value = 0.0f;
        if (m.isController())        { source = MidiBinding::CC; number = m.getControllerNumber(); value = (float) m.getControllerValue() / 127.0f; }
        else if (m.isNoteOn())       { source = MidiBinding::Note; number = m.getNoteNumber(); value = 1.0f; }
        else if (m.isNoteOff())      { source = MidiBinding::Note; number = m.getNoteNumber(); value = 0.0f; }
        else if (m.isProgramChange()) { source = MidiBinding::Program; number = m.getProgramChangeNumber(); value = 1.0f; }
        if (source < 0) continue;
        {
            MidiBinding tmp; tmp.source = source; tmp.number = number; tmp.channel = ch;
            midiMap.lastReceived = MidiMap::describeSource (tmp) + (source == MidiBinding::CC ? " = " + String (roundToInt (value * 127.0f)) : String());
        }

        // MIDI learn: the first press, pedal move or program change claims the armed control
        if (midiMap.learnTarget.isNotEmpty() && ! (source == MidiBinding::Note && value < 0.5f))
        {
            MidiBinding b; b.source = source; b.channel = ch; b.number = number; b.target = midiMap.learnTarget;
            midiMap.removeTarget (b.target);
            for (int k = midiMap.bindings.size(); --k >= 0;)
            {
                const auto& o = midiMap.bindings.getReference (k);
                if (o.source == source && o.number == number && (o.channel == 0 || o.channel == ch)) midiMap.bindings.remove (k);
            }
            b.last = -1.0f;                                         // fresh: the next press acts
            midiMap.bindings.add (b);
            midiMap.learnTarget.clear();
            saveMidiMap();
            continue;                                              // learning doesn't also fire the action
        }

        auto* b = midiMap.find (source, ch, number);
        if (b == nullptr)
        {
            if (source == MidiBinding::Program && midiMap.programChangeSelectsPresets)
            {
                const auto& f = getFactoryPresets();
                auto user = getUserPresetFolder().findChildFiles (File::findFiles, false, "*.ocpreset");
                user.sort();
                if (number < (int) f.size()) setCurrentProgram (number);
                else if (number - (int) f.size() < user.size()) loadPresetFile (user[number - (int) f.size()]);
            }
            continue;
        }
        // edges: a momentary pedal acts when pressed (rising past the middle); a latching one on every change
        const bool wasDown = b->last >= 0.5f, isDown = value >= 0.5f, first = b->last < 0.0f;
        const bool press = source == MidiBinding::Program ? true
                         : b->latching ? (first ? isDown : wasDown != isDown)
                                       : (isDown && (first || ! wasDown));
        b->last = value;
        bool down = isDown;
        if (MidiMap::kindOf (b->target) == MidiTargetKind::Momentary)
        {
            const int s = jlimit (0, oc::kNumSlots - 1, b->target.substring (1).getIntValue() - 1);
            if (b->latching) { if (! press) continue; down = twistLatched[s] = ! twistLatched[s]; }   // press on, press off
            else if (! first && wasDown == isDown) continue;                                          // held / released only
        }
        runMidiAction (*b, value, press, down, ev[i].timeMs);
    }
}
