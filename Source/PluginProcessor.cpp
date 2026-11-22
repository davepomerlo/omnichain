#include "PluginProcessor.h"
#include <map>
#include <mutex>
static juce::PropertiesFile& globalSettings();
#include "PluginEditor.h"
#include "dsp/NamEffect.h"

using namespace juce;

static const char* const kSlotParamIds[] = { "type", "byp", "mix", "lvl", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8" };
constexpr int kNumSlotParamIds = 12;
constexpr int kStateFormat = 3;   // 1 = v1.x (6 knobs per slot), 2 = v2.0 (8 knobs, longer Distortion mode list), 3 = v2.2 (10 s Delay, 15 types)

OmniChainProcessor::OmniChainProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OmniChainState", createLayout())
{
    for (int s = 0; s < oc::kNumSlots; ++s)
    {
        oc::SlotParamRefs r;
        r.type   = apvts.getRawParameterValue (pid (s, "type"));
        r.bypass = apvts.getRawParameterValue (pid (s, "byp"));
        r.mix    = apvts.getRawParameterValue (pid (s, "mix"));
        r.level  = apvts.getRawParameterValue (pid (s, "lvl"));
        for (int k = 0; k < oc::kParamsPerSlot; ++k) r.p[k] = apvts.getRawParameterValue (pid (s, "p" + String (k + 1)));
        engine.setSlotParams (s, r);
    }
    engine.setGlobalParams (apvts.getRawParameterValue ("in"), apvts.getRawParameterValue ("out"), apvts.getRawParameterValue ("mix"));
    formats.registerBasicFormats();
    markClean();
    startTimerHz (40);
}

OmniChainProcessor::~OmniChainProcessor()
{
    stopTimer();
    stopRecording();
}

AudioProcessorValueTreeState::ParameterLayout OmniChainProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    StringArray typeNames;
    for (int t = 0; t < oc::NumEffectTypes; ++t) typeNames.add (oc::getInfo (t).name);

    auto dbText = [] (float v, int) { return String (v, 1) + " dB"; };
    auto pctText = [] (float v, int) { return String (roundToInt (v)) + "%"; };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "in", 1 }, "Input", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "out", 1 }, "Output", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Global Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pctText)));

    for (int s = 0; s < oc::kNumSlots; ++s)
    {
        const String n = "Slot " + String (s + 1);
        auto grp = std::make_unique<AudioProcessorParameterGroup> ("slot" + String (s + 1), n, "|");
        auto type = std::make_unique<AudioParameterChoice> (ParameterID { pid (s, "type"), 1 }, n + " Type", typeNames, 0);
        auto* typePtr = type.get();
        grp->addChild (std::move (type));
        grp->addChild (std::make_unique<AudioParameterBool> (ParameterID { pid (s, "byp"), 1 }, n + " Bypass", false));
        grp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { pid (s, "mix"), 1 }, n + " Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                              AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pctText)));
        grp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { pid (s, "lvl"), 1 }, n + " Level", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                              AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));
        for (int k = 0; k < oc::kParamsPerSlot; ++k)
        {
            auto fmt = [typePtr, k] (float v, int) { return String (oc::formatParamNorm (typePtr->getIndex(), k, v)); };
            // typing a value (in OmniChain or in the DAW) uses the loaded effect's own units: "375 ms", "1.5 s", "Tape"...
            auto parse = [typePtr, k] (const String& t) -> float
            {
                const auto& pi = oc::getInfo (typePtr->getIndex()).params[k];
                float real = 0.0f;
                if (oc::parseParam (pi, t.toStdString(), real)) return oc::paramToNorm (pi, real);
                return oc::paramToNorm (pi, pi.def);      // not understood: back to the default
            };
            grp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { pid (s, "p" + String (k + 1)), 1 }, n + " Knob " + String (k + 1),
                                                                  NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                                  AudioParameterFloatAttributes().withStringFromValueFunction (fmt).withValueFromStringFunction (parse)));
        }
        layout.add (std::move (grp));
    }
    return layout;
}

void OmniChainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    monoScratch.assign ((size_t) jmax (samplesPerBlock, 4096), 0.0f);
    setLatencySamples (0);
}

bool OmniChainProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    if (in != AudioChannelSet::mono() && in != AudioChannelSet::stereo()) return false;
    return in.size() <= out.size();
}

void OmniChainProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, n);
    if (numOut == 0 || n == 0) return;

    oc::Context ctx;
    ctx.sampleRate = getSampleRate();
    bool hostTempo = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { ctx.bpm = *b; hostTempo = true; }
            if (auto p = pos->getPpqPosition()) ctx.ppq = *p;
            ctx.playing = pos->getIsPlaying();
        }
    if (! hostTempo) ctx.bpm = jlimit (30.0, 300.0, (double) hostlessBpm.load());
    hostHasTempo.store (hostTempo, std::memory_order_relaxed);
    currentBpm.store ((float) ctx.bpm, std::memory_order_relaxed);

    float* L = buffer.getWritePointer (0);
    const bool tuning = tunerOn.load (std::memory_order_relaxed);
    if (tuning)
    {
        const float* inR = numIn > 1 ? buffer.getReadPointer (1) : L;
        int w = tunerWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) { tunerRing[(size_t) w] = 0.5f * (L[i] + inR[i]); w = (w + 1) & (kTunerRing - 1); }
        tunerWrite.store (w, std::memory_order_release);
    }
    if (numOut >= 2)
    {
        float* R = buffer.getWritePointer (1);
        if (numIn == 1) FloatVectorOperations::copy (R, L, n);
        engine.process (L, R, n, ctx);
    }
    else
    {
        int done = 0;
        while (done < n)
        {
            const int m = jmin (n - done, (int) monoScratch.size());
            float* R = monoScratch.data();
            FloatVectorOperations::copy (R, L + done, m);
            engine.process (L + done, R, m, ctx);
            for (int i = 0; i < m; ++i) L[done + i] = 0.5f * (L[done + i] + R[i]);
            ctx.ppq += double (m) * ctx.bpm / (60.0 * ctx.sampleRate);
            done += m;
        }
    }

    // recorder: the chain's output (before the metronome click)
    if (activeWriter.load (std::memory_order_acquire) != nullptr)
    {
        const ScopedTryLock sl (writerLock);
        if (sl.isLocked())
            if (auto* wr = activeWriter.load())
            {
                const float* chans[2] = { buffer.getReadPointer (0), buffer.getReadPointer (numOut > 1 ? 1 : 0) };
                wr->write (chans, n);
                recordedSamples += n;
            }
    }

    if (tuning) { buffer.clear(); return; }    // mute while tuning

    if (metronomeOn.load (std::memory_order_relaxed))
    {
        const double spb = 60.0 / ctx.bpm * ctx.sampleRate;
        const float vol = metronomeVol.load() * 0.5f;
        const float decay = std::exp (-1.0f / (0.018f * (float) ctx.sampleRate));
        for (int i = 0; i < n; ++i)
        {
            metroPos += 1.0;
            if (metroBeat < 0 || metroPos >= spb)
            {
                metroPos = metroBeat < 0 ? 0.0 : metroPos - spb;
                metroBeat = (metroBeat + 1) % 4;
                clickEnv = 1.0f; clickPhase = 0.0f; clickFreq = metroBeat == 0 ? 2000.0f : 1400.0f;
            }
            if (clickEnv > 1.0e-4f)
            {
                clickPhase += clickFreq / (float) ctx.sampleRate;
                const float c = std::sin (MathConstants<float>::twoPi * clickPhase) * clickEnv * vol;
                clickEnv *= decay;
                for (int ch = 0; ch < numOut; ++ch) buffer.getWritePointer (ch)[i] += c;
            }
        }
    }
    else metroBeat = -1;
}

AudioProcessorEditor* OmniChainProcessor::createEditor() { return new OmniChainEditor (*this); }

// ------------------------------------------------------------------ programs
int OmniChainProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }
const String OmniChainProcessor::getProgramName (int i)
{
    const auto& p = getFactoryPresets();
    return isPositiveAndBelow (i, (int) p.size()) ? String (p[(size_t) i].name) : String();
}
void OmniChainProcessor::setCurrentProgram (int i)
{
    const auto& p = getFactoryPresets();
    if (! isPositiveAndBelow (i, (int) p.size())) return;
    currentProgram = i;
    applyPreset (p[(size_t) i]);
}

// ------------------------------------------------------------------ state
void OmniChainProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("formatVersion", kStateFormat, nullptr);
    state.setProperty ("bpm", hostlessBpm.load(), nullptr);
    state.setProperty ("metronomeVol", metronomeVol.load(), nullptr);
    state.setProperty ("trails", engine.trails.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void OmniChainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            currentPresetName = apvts.state.getProperty ("presetName", "Custom").toString();
            hostlessBpm = (float) (double) apvts.state.getProperty ("bpm", 120.0);
            metronomeVol = (float) (double) apvts.state.getProperty ("metronomeVol", 0.5);
            engine.trails = (bool) apvts.state.getProperty ("trails", true);
            migrateOldState ((int) apvts.state.getProperty ("formatVersion", 1));
            reloadResourcesFromState();
            markClean();
            if (MessageManager::existsAndIsCurrentThread()) engine.service();
        }
}

// Projects and presets saved by older versions: give new knobs their defaults and re-map ranges / choice lists that grew.
void OmniChainProcessor::migrateOldState (int fromVersion)
{
    if (fromVersion >= kStateFormat) return;
    for (int s = 0; s < oc::kNumSlots; ++s) migrateSlot (s, fromVersion);
    apvts.state.setProperty ("formatVersion", kStateFormat, nullptr);
}

void OmniChainProcessor::migrateSlot (int s, int fromVersion)
{
    const int type = getSlotType (s);
    const auto& info = oc::getInfo (type);
    if (fromVersion < 2)
    {
        for (int k = 6; k < oc::kParamsPerSlot; ++k)
            setParamValue (param (s, "p" + String (k + 1)), info.params[k].name ? oc::paramToNorm (info.params[k], info.params[k].def) : 0.5f);
        if (type == oc::FxDistortion)   // Mode had 4 choices, now 6
        {
            auto* m = param (s, "p3");
            const int oldIdx = roundToInt (m->getValue() * 3.0f);
            setParamValue (m, (float) oldIdx / 5.0f);
        }
    }
    if (fromVersion < 3 && type == oc::FxDelay)
    {
        // Time was 1..2000 ms (log), now 1..10000 ms; Type had 5 choices, now 15 (the first five kept their place)
        auto* t = param (s, "p1");
        const float ms = std::pow (2000.0f, jlimit (0.0f, 1.0f, t->getValue()));
        setParamValue (t, oc::paramToNorm (info.params[0], ms));
        auto* m = param (s, "p5");
        const int oldIdx = roundToInt (m->getValue() * 4.0f);
        setParamValue (m, oc::paramToNorm (info.params[4], (float) oldIdx));
    }
}

// ------------------------------------------------------------------ delay tap tempo
// Tap twice or more: free time -> the taps set the delay time; synced -> they set the tempo
// (or, when the host owns the tempo, the delay switches to free time at tapped quarter x subdivision).
float OmniChainProcessor::tapDelay (int slot)
{
    if (getSlotType (slot) != oc::FxDelay) return 0.0f;
    const double now = Time::getMillisecondCounterHiRes();
    auto& t = tapTimes[slot];
    if (! t.isEmpty() && now - t.getLast() > 2500.0) t.clearQuick();
    t.add (now);
    while (t.size() > 5) t.remove (0);
    if (t.size() < 2) return 0.0f;
    const float ms = (float) ((t.getLast() - t.getFirst()) / (t.size() - 1));
    const auto& info = oc::getInfo (oc::FxDelay);
    auto* sync = param (slot, "p2");
    auto* time = param (slot, "p1");
    const int si = roundToInt (oc::paramToReal (info.params[1], sync->getValue()));
    if (si <= 0)
        setParamValue (time, oc::paramToNorm (info.params[0], jlimit (1.0f, 10000.0f, ms)));
    else if (! hostHasTempo.load())
        hostlessBpm = jlimit (30.0f, 300.0f, 60000.0f / ms);
    else
    {
        setParamValue (time, oc::paramToNorm (info.params[0], jlimit (1.0f, 10000.0f, ms * oc::kSyncBeats[si - 1])));
        setParamValue (sync, 0.0f);
    }
    return ms;
}

// Typed into the delay read-out: "375", "375 ms", "1.25 s" set the time (free), "1/8D" picks a sync division,
// "92.5 bpm" sets the tempo (or, when the DAW owns the tempo, the time for one beat at that tempo x the division).
bool OmniChainProcessor::setDelayFromText (int slot, const String& text)
{
    if (getSlotType (slot) != oc::FxDelay) return false;
    const auto& info = oc::getInfo (oc::FxDelay);
    auto* sync = param (slot, "p2");
    auto* time = param (slot, "p1");
    const auto t = text.trim().toLowerCase().removeCharacters (" ");
    if (t.isEmpty()) return false;
    if (t == "off" || t == "free") { setParamValue (sync, 0.0f); return true; }
    for (int i = 0; i < oc::kNumSync; ++i)
        if (t == String (oc::kSyncNames[i]).toLowerCase()) { setParamValue (sync, oc::paramToNorm (info.params[1], (float) (i + 1))); return true; }
    if (t.endsWith ("bpm"))
    {
        const float bpm = t.dropLastCharacters (3).getFloatValue();
        if (bpm < 30.0f || bpm > 300.0f) return false;
        const int si = roundToInt (oc::paramToReal (info.params[1], sync->getValue()));
        if (! hostHasTempo.load()) { hostlessBpm = bpm; if (si <= 0) setParamValue (sync, oc::paramToNorm (info.params[1], 4.0f)); }   // 4 = 1/4
        else
        {
            const float beats = si > 0 ? oc::kSyncBeats[si - 1] : 1.0f;
            setParamValue (time, oc::paramToNorm (info.params[0], jlimit (1.0f, 10000.0f, 60000.0f / bpm * beats)));
            setParamValue (sync, 0.0f);
        }
        return true;
    }
    float ms = 0.0f;
    if (! oc::parseParam (info.params[0], t.toStdString(), ms)) return false;
    setParamValue (time, oc::paramToNorm (info.params[0], ms));
    setParamValue (sync, 0.0f);
    return true;
}

void OmniChainProcessor::setTrails (bool on)
{
    engine.trails = on;
    apvts.state.setProperty ("trails", on, nullptr);
}

// ------------------------------------------------------------------ slot helpers
void OmniChainProcessor::setParamValue (RangedAudioParameter* p, float normalised)
{
    if (p == nullptr) return;
    normalised = jlimit (0.0f, 1.0f, normalised);
    if (std::abs (p->getValue() - normalised) < 1.0e-7f) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (normalised);
    p->endChangeGesture();
}

int OmniChainProcessor::getSlotType (int slot) const
{
    return jlimit (0, oc::NumEffectTypes - 1, (int) std::lround (apvts.getRawParameterValue (pid (slot, "type"))->load()));
}

void OmniChainProcessor::resetSlotParams (int slot)
{
    const auto& info = oc::getInfo (getSlotType (slot));
    for (int k = 0; k < oc::kParamsPerSlot; ++k)
        setParamValue (param (slot, "p" + String (k + 1)), info.params[k].name ? oc::paramToNorm (info.params[k], info.params[k].def) : 0.5f);
    auto* mix = param (slot, "mix"); setParamValue (mix, mix->convertTo0to1 (info.defaultMix * 100.0f));
    auto* lvl = param (slot, "lvl"); setParamValue (lvl, lvl->convertTo0to1 (0.0f));
}

void OmniChainProcessor::setSlotType (int slot, int type, bool applyDefaults)
{
    auto* tp = param (slot, "type");
    if (type != getSlotType (slot)) clearSlotResource (slot);
    setParamValue (tp, tp->convertTo0to1 ((float) type));
    if (applyDefaults)
    {
        resetSlotParams (slot);
        setParamValue (param (slot, "byp"), 0.0f);
    }
}

void OmniChainProcessor::moveSlot (int from, int to)
{
    if (from == to || ! isPositiveAndBelow (from, oc::kNumSlots) || ! isPositiveAndBelow (to, oc::kNumSlots)) return;
    engine.requestMove (from, to);
    std::vector<std::array<float, kNumSlotParamIds>> v ((size_t) oc::kNumSlots);
    for (int s = 0; s < oc::kNumSlots; ++s)
        for (int j = 0; j < kNumSlotParamIds; ++j) v[(size_t) s][(size_t) j] = param (s, kSlotParamIds[j])->getValue();
    const auto item = v[(size_t) from];
    v.erase (v.begin() + from);
    v.insert (v.begin() + to, item);
    StringArray paths, errs;
    for (int s = 0; s < oc::kNumSlots; ++s) { paths.add (getSlotFile (s).getFullPathName()); errs.add (slotError[s]); }
    paths.move (from, to); errs.move (from, to);
    for (int s = 0; s < oc::kNumSlots; ++s) { setSlotPath (s, paths[s]); slotError[s] = errs[s]; }
    for (int s = 0; s < oc::kNumSlots; ++s)
        for (int j = 0; j < kNumSlotParamIds; ++j) setParamValue (param (s, kSlotParamIds[j]), v[(size_t) s][(size_t) j]);
}

void OmniChainProcessor::copySlot (int from, int to)
{
    if (from == to) return;
    engine.setSlotResource (to, engine.getSlotResource (from));
    setSlotPath (to, getSlotFile (from).getFullPathName());
    slotError[to] = slotError[from];
    for (int j = 0; j < kNumSlotParamIds; ++j)
        setParamValue (param (to, kSlotParamIds[j]), param (from, kSlotParamIds[j])->getValue());
}

void OmniChainProcessor::applyPreset (const PresetDef& p)
{
    for (int s = 0; s < oc::kNumSlots; ++s)
    {
        if (s < (int) p.slots.size())
        {
            const auto& ps = p.slots[(size_t) s];
            setSlotType (s, ps.type, true);
            const auto& info = oc::getInfo (ps.type);
            for (int k = 0; k < (int) ps.values.size() && k < oc::kParamsPerSlot; ++k)
                if (! std::isnan (ps.values[(size_t) k]) && info.params[k].name)
                    setParamValue (param (s, "p" + String (k + 1)), oc::paramToNorm (info.params[k], ps.values[(size_t) k]));
            if (! std::isnan (ps.mix)) { auto* m = param (s, "mix"); setParamValue (m, m->convertTo0to1 (ps.mix)); }
            auto* l = param (s, "lvl"); setParamValue (l, l->convertTo0to1 (ps.level));
            setParamValue (param (s, "byp"), ps.bypass ? 1.0f : 0.0f);
            if (ps.file != nullptr)
            {
                const auto lib = getLibraryFolder();
                const auto f = lib.getChildFile (String::fromUTF8 (ps.file));
                if (lib.isDirectory() && f.existsAsFile()) loadSlotFile (s, f);
                else slotError[s] = "Needs the OmniChain Library: " + String::fromUTF8 (ps.file).fromLastOccurrenceOf ("/", false, false);
            }
        }
        else setSlotType (s, oc::FxNone, true);
    }
    auto* in = apvts.getParameter ("in");   setParamValue (in, in->convertTo0to1 (p.inDb));
    auto* out = apvts.getParameter ("out"); setParamValue (out, out->convertTo0to1 (p.outDb));
    auto* mx = apvts.getParameter ("mix");  setParamValue (mx, 1.0f);
    currentPresetName = p.name;
    markClean();
}

File OmniChainProcessor::getUserPresetFolder()
{
    auto f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("OmniChain Presets");
    if (! f.exists()) f.createDirectory();
    return f;
}

bool OmniChainProcessor::savePresetFile (const File& f)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", f.getFileNameWithoutExtension(), nullptr);
    state.setProperty ("formatVersion", kStateFormat, nullptr);
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (f)) return false;
    currentPresetName = f.getFileNameWithoutExtension();
    markClean();
    return true;
}

bool OmniChainProcessor::loadPresetFile (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto tree = ValueTree::fromXml (*xml);
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto c = tree.getChild (i);
        if (! c.hasProperty ("id")) continue;
        if (auto* p = apvts.getParameter (c.getProperty ("id").toString()))
            setParamValue (p, p->convertTo0to1 ((float) c.getProperty ("value")));
    }
    for (int s = 0; s < oc::kNumSlots; ++s) setSlotPath (s, tree.getProperty ("res" + String (s + 1)).toString());
    migrateOldState ((int) tree.getProperty ("formatVersion", 1));
    reloadResourcesFromState();
    currentPresetName = f.getFileNameWithoutExtension();
    markClean();
    return true;
}

// ------------------------------------------------------------------ file-based slots
void OmniChainProcessor::setSlotPath (int slot, const String& path)
{
    apvts.state.setProperty ("res" + String (slot + 1), path, nullptr);
}

File OmniChainProcessor::getSlotFile (int slot) const
{
    const auto p = apvts.state.getProperty ("res" + String (slot + 1)).toString();
    return p.isEmpty() ? File() : File (p);
}

void OmniChainProcessor::clearSlotResource (int slot)
{
    engine.setSlotResource (slot, nullptr);
    setSlotPath (slot, {});
    slotError[slot].clear();
}

// The factory library (NAM captures + IRs) can live in several places; the first one found wins.
// After the library appears: retry slots that were waiting for it (a factory preset is simply loaded again)
void OmniChainProcessor::reloadMissingFiles()
{
    bool missing = false;
    for (int s = 0; s < oc::kNumSlots; ++s) missing |= slotError[s].isNotEmpty();
    if (! missing) return;
    for (int i = 0; i < getNumPrograms(); ++i)
        if (getProgramName (i) == currentPresetName && ! isModified()) { setCurrentProgram (i); return; }
    reloadResourcesFromState();
}

// A folder counts as the library when it holds the NAM and IR sub-folders.
static bool looksLikeLibrary (const File& f) { return f.isDirectory() && (f.getChildFile ("NAM").isDirectory() || f.getChildFile ("IR").isDirectory()); }

Array<File> OmniChainProcessor::getLibrarySearchPlaces()
{
    const String name ("OmniChain Library");
    Array<File> c;
    const File chosen (globalSettings().getValue ("libraryFolder"));
    if (chosen.getFullPathName().isNotEmpty()) c.add (chosen);
    const auto home = File::getSpecialLocation (File::userHomeDirectory);
    c.add (File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (name));
    c.add (home.getChildFile ("Documents").getChildFile (name));
    // Windows 11 often keeps Documents inside OneDrive ("OneDrive" or "OneDrive - Company")
    for (auto& od : home.findChildFiles (File::findDirectories, false, "OneDrive*"))
        for (auto* sub : { "Documents", "Dokumente", "Documentos", "Documenti", "ドキュメント" })
            c.add (od.getChildFile (sub).getChildFile (name));
    c.add (File::getSpecialLocation (File::commonApplicationDataDirectory).getChildFile ("OmniChain").getChildFile (name));   // Windows installer
    c.add (File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("OmniChain").getChildFile (name));
   #if JUCE_MAC
    c.add (File ("~/Library/Application Support/OmniChain").getChildFile (name));
    c.add (File ("/Library/Application Support/OmniChain").getChildFile (name));
   #endif
    // next to the app or plugin, up to a few folders above it (e.g. the unzipped download kept as it is)
    for (auto start : { File::getSpecialLocation (File::currentExecutableFile), File::getSpecialLocation (File::currentApplicationFile) })
    {
        auto d = start.getParentDirectory();
        for (int up = 0; up < 5 && d.getParentDirectory() != d; ++up, d = d.getParentDirectory())
        {
            c.add (d.getChildFile (name));
            c.add (d.getChildFile ("Resources").getChildFile (name));
        }
    }
    // an unzipped download left in Downloads or on the Desktop
    for (auto* where : { "Downloads", "Desktop" })
    {
        const auto base = home.getChildFile (where);
        c.add (base.getChildFile (name));
        for (auto& sub : base.findChildFiles (File::findDirectories, false, "OmniChain*"))
        {
            c.add (sub.getChildFile (name));
            for (auto& sub2 : sub.findChildFiles (File::findDirectories, false, "OmniChain*")) c.add (sub2.getChildFile (name));
        }
    }
    return c;
}

static std::mutex libMutex;
static File libCached;
static uint32 libLastLook = 0;

File OmniChainProcessor::getLibraryFolder()
{
    // remembered once found; looked for again at most every 2 s while missing
    std::lock_guard<std::mutex> lock (libMutex);
    auto& cached = libCached; auto& lastLook = libLastLook;
    if (looksLikeLibrary (cached)) return cached;
    const auto now = Time::getMillisecondCounter();
    if (lastLook != 0 && now - lastLook < 2000) return {};
    lastLook = now;
    for (auto& f : getLibrarySearchPlaces()) if (looksLikeLibrary (f)) { cached = f; return f; }
    cached = File();
    return {};
}

// Set up the library from a downloaded zip (OmniChain-Library.zip or a whole OmniChain download zip) or a folder.
// A zip is unpacked into Documents/OmniChain Library; a folder is used where it is.
bool OmniChainProcessor::installLibrary (const File& src, String& message, const File& destParent)
{
    const String name ("OmniChain Library");
    if (src.isDirectory())
    {
        File lib = looksLikeLibrary (src) ? src : src.getChildFile (name);
        if (! looksLikeLibrary (lib))
            for (auto& d : src.findChildFiles (File::findDirectories, true, name)) if (looksLikeLibrary (d)) { lib = d; break; }
        if (! looksLikeLibrary (lib)) { message = "That folder doesn't contain the OmniChain Library (it should hold NAM and IR folders)."; return false; }
        globalSettings().setValue ("libraryFolder", lib.getFullPathName());
        globalSettings().saveIfNeeded();
        { std::lock_guard<std::mutex> lock (libMutex); libCached = lib; }
        message = "Using the library in " + lib.getFullPathName();
        return true;
    }
    if (! src.hasFileExtension ("zip")) { message = "Pick OmniChain-Library.zip (or the OmniChain download zip), or the unzipped OmniChain Library folder."; return false; }
    ZipFile zip (src);
    const auto dest = (destParent.isDirectory() ? destParent : File::getSpecialLocation (File::userDocumentsDirectory)).getChildFile (name);
    int written = 0;
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* e = zip.getEntry (i);
        if (e == nullptr || e->isSymbolicLink) continue;
        const auto path = e->filename.replaceCharacter ('\\', '/');
        const int at = path.indexOf (name + "/");
        if (at < 0 || path.endsWithChar ('/') || path.contains ("__MACOSX")) continue;
        const auto rel = path.substring (at + name.length() + 1);
        if (rel.isEmpty() || rel.contains ("..")) continue;
        const auto out = dest.getChildFile (rel);
        out.getParentDirectory().createDirectory();
        std::unique_ptr<InputStream> in (zip.createStreamForEntry (i));
        if (in == nullptr) continue;
        out.deleteFile();
        FileOutputStream os (out);
        if (os.openedOk() && os.writeFromInputStream (*in, -1) >= 0) { os.flush(); ++written; }
    }
    if (written == 0 || ! looksLikeLibrary (dest)) { message = "No OmniChain Library was found inside " + src.getFileName() + "."; return false; }
    globalSettings().setValue ("libraryFolder", dest.getFullPathName());
    globalSettings().saveIfNeeded();
    { std::lock_guard<std::mutex> lock (libMutex); libCached = dest; }
    message = "Installed " + String (written) + " files to " + dest.getFullPathName();
    return true;
}

// A downloaded zip that holds the library, if one is lying in Downloads or on the Desktop
File OmniChainProcessor::findLibraryZip()
{
    const auto home = File::getSpecialLocation (File::userHomeDirectory);
    Array<File> zips;
    for (auto* where : { "Downloads", "Desktop" })
        zips.addArray (home.getChildFile (where).findChildFiles (File::findFiles, false, "OmniChain*.zip"));
    File best;
    for (auto& z : zips)
    {
        ZipFile zip (z);
        bool has = false;
        for (int i = 0; i < zip.getNumEntries() && ! has; ++i) if (auto* e = zip.getEntry (i)) has = e->filename.contains ("OmniChain Library/NAM/");
        if (has && (best == File() || z.getLastModificationTime() > best.getLastModificationTime())) best = z;
    }
    return best;
}

// A path saved on another computer (or before the library moved) is re-found inside the local library.
File OmniChainProcessor::resolveLibraryFile (const File& saved)
{
    if (saved.existsAsFile()) return saved;
    const auto path = saved.getFullPathName().replaceCharacter ('\\', '/');
    const int i = path.indexOf ("OmniChain Library/");
    const auto lib = getLibraryFolder();
    if (i >= 0 && lib.isDirectory())
    {
        const auto f = lib.getChildFile (path.substring (i + 18));
        if (f.existsAsFile()) return f;
    }
    return saved;
}

File OmniChainProcessor::getContentFolder (bool nam)
{
    auto f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (nam ? "OmniChain NAM Models" : "OmniChain IRs");
    if (! f.exists()) f.createDirectory();
    return f;
}

std::shared_ptr<const oc::Resource> OmniChainProcessor::loadIr (const File& f, String& error)
{
    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr) { error = "Can't read this audio file."; return nullptr; }
    if (reader->sampleRate <= 0 || reader->lengthInSamples <= 0) { error = "Empty audio file."; return nullptr; }
    const int len = (int) jmin<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 2.0));
    const int nch = (int) jmin<unsigned int> (2u, reader->numChannels);
    AudioBuffer<float> buf (nch, len);
    reader->read (&buf, 0, len, 0, true, nch > 1);
    auto ir = std::make_shared<oc::IrResource>();
    ir->sampleRate = reader->sampleRate;
    ir->name = f.getFileName().toStdString();
    for (int c = 0; c < nch; ++c) ir->ch[c].assign (buf.getReadPointer (c), buf.getReadPointer (c) + len);
    return ir;
}

bool OmniChainProcessor::loadSlotFile (int slot, const File& f)
{
    const bool nam = isNamFile (f);
    if (! nam && ! isIrFile (f)) { slotError[slot] = "Unsupported file type: " + f.getFileName(); return false; }
    if (! f.existsAsFile()) { slotError[slot] = "File not found: " + f.getFileName(); return false; }
    std::shared_ptr<const oc::Resource> res;
    String err;
    if (nam)
    {
        auto r = oc::loadNamFile (f.getFullPathName().toStdString(), getNamLight());
        res = r.resource; err = String (r.error);
    }
    else res = loadIr (f, err);
    if (res == nullptr) { slotError[slot] = err; return false; }

    const int type = nam ? oc::FxNam : oc::FxCabIR;
    if (getSlotType (slot) != type) setSlotType (slot, type, true);
    engine.setSlotResource (slot, res);
    setSlotPath (slot, f.getFullPathName());
    slotError[slot].clear();
    lastFileFolder = f.getParentDirectory();
    return true;
}

void OmniChainProcessor::stepSlotFile (int slot, int delta)
{
    const auto cur = getSlotFile (slot);
    if (! cur.existsAsFile()) return;
    const bool nam = isNamFile (cur);
    auto files = cur.getParentDirectory().findChildFiles (File::findFiles, false, nam ? "*.nam" : "*.wav;*.aif;*.aiff;*.flac");
    if (files.size() < 2) return;
    files.sort();
    const int idx = files.indexOf (cur);
    loadSlotFile (slot, files[((idx < 0 ? 0 : idx) + delta + files.size()) % files.size()]);
}

void OmniChainProcessor::reloadResourcesFromState()
{
    for (int s = 0; s < oc::kNumSlots; ++s)
    {
        const auto f = getSlotFile (s);
        const int t = getSlotType (s);
        if (! oc::typeUsesResource (t) || f == File())
        {
            engine.setSlotResource (s, nullptr);
            if (! oc::typeUsesResource (t)) setSlotPath (s, {});
            slotError[s].clear();
            continue;
        }
        if (! loadSlotFile (s, resolveLibraryFile (f)))
        {
            engine.setSlotResource (s, nullptr);
            setSlotPath (s, f.getFullPathName());    // keep the path so the project can be fixed later
        }
    }
}

String OmniChainProcessor::getSlotFileText (int slot, int& severity)
{
    severity = 0;
    if (slotError[slot].isNotEmpty()) { severity = 2; return slotError[slot]; }
    const auto res = engine.getSlotResource (slot);
    if (res == nullptr)
    {
        if (! getLibraryFolder().isDirectory()) { severity = 1; return "Factory library not found - click Load to set it up"; }
        return getSlotType (slot) == oc::FxNam ? "No model loaded - click Load to pick one, or drop a .nam file here"
                                                : "No IR loaded - click Load to pick one, or drop a .wav file here";
    }
    String txt = String::fromUTF8 (res->name.c_str());
    if (getSlotType (slot) == oc::FxNam && ! res->detail.empty())
    {
        // size class and what it really costs on this computer, so heavy captures are easy to spot
        txt << "  |  " << String::fromUTF8 (res->detail.c_str()) << ", CPU " << String (jmax (1, roundToInt (res->cpuPct))) << "%";
        if (res->cpuPct > 12.0f) { severity = 1; txt << " (heavy: try a lite / feather version)"; }
    }
    const double sr = engine.getSampleRate();
    if (getSlotType (slot) == oc::FxNam && sr > 0 && std::abs (sr - res->sampleRate) > 1.0)
    {
        severity = 1;
        txt << "  |  model is " << String (res->sampleRate / 1000.0, 1) << " kHz, project " << String (sr / 1000.0, 1) << " kHz";
    }
    return txt;
}

// ------------------------------------------------------------------ standalone helpers
void OmniChainProcessor::copyTunerInput (float* dest, int n) const
{
    const int w = tunerWrite.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i) dest[i] = tunerRing[(size_t) ((w - n + i) & (kTunerRing - 1))];
}

File OmniChainProcessor::getRecordingsFolder()
{
    auto f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("OmniChain Recordings");
    if (! f.exists()) f.createDirectory();
    return f;
}

bool OmniChainProcessor::startRecording()
{
    stopRecording();
    const double sr = getSampleRate();
    if (sr <= 0) return false;
    auto file = getRecordingsFolder().getNonexistentChildFile ("OmniChain " + Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"), ".wav");
    auto stream = file.createOutputStream();
    if (stream == nullptr) return false;
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> os (stream.release());
    auto* writer = wav.createWriterFor (os.get(), sr, 2, 24, {}, 0);
    if (writer == nullptr) return false;
    os.release();   // the writer owns the stream now
    writerThread.startThread();
    threadedWriter = std::make_unique<AudioFormatWriter::ThreadedWriter> (writer, writerThread, 32768);
    recordedSamples = 0;
    lastRecording = file;
    const ScopedLock sl (writerLock);
    activeWriter = threadedWriter.get();
    return true;
}

void OmniChainProcessor::stopRecording()
{
    {
        const ScopedLock sl (writerLock);
        activeWriter = nullptr;
    }
    threadedWriter.reset();   // flushes and closes the file
}

// ------------------------------------------------------------------ patches
double OmniChainProcessor::paramChecksum() const
{
    double sum = 0.0; int i = 0;
    for (auto* prm : getParameters()) sum += (double) prm->getValue() * (1.0 + 0.6180339887 * (double) ++i);
    for (int s = 0; s < oc::kNumSlots; ++s) sum += (double) apvts.state.getProperty ("res" + String (s + 1)).toString().hashCode() * 1.0e-9;
    return sum;
}

File OmniChainProcessor::getSlotPatchRoot()
{
    auto f = getUserPresetFolder().getChildFile ("Effect Patches");
    if (! f.exists()) f.createDirectory();
    return f;
}

File OmniChainProcessor::getSlotPatchFolder (int type)
{
    return getSlotPatchRoot().getChildFile (File::createLegalFileName (oc::getInfo (type).name));
}

bool OmniChainProcessor::saveSlotPatch (int slot, const File& f)
{
    const int type = getSlotType (slot);
    if (type == oc::FxNone) return false;
    XmlElement x ("OmniChainSlot");
    x.setAttribute ("formatVersion", kStateFormat);
    x.setAttribute ("type", type);
    x.setAttribute ("typeName", oc::getInfo (type).name);
    for (int j = 1; j < kNumSlotParamIds; ++j) x.setAttribute (kSlotParamIds[j], (double) param (slot, kSlotParamIds[j])->getValue());
    x.setAttribute ("res", getSlotFile (slot).getFullPathName());
    f.getParentDirectory().createDirectory();
    return x.writeTo (f);
}

bool OmniChainProcessor::loadSlotPatch (int slot, const File& f)
{
    auto x = XmlDocument::parse (f);
    if (x == nullptr || ! x->hasTagName ("OmniChainSlot")) return false;
    int type = x->getIntAttribute ("type", -1);
    if (! isPositiveAndBelow (type, oc::NumEffectTypes) || x->getStringAttribute ("typeName") != oc::getInfo (type).name)
    {
        type = -1;                                          // the list grew: find the effect by its name instead
        for (int t = 0; t < oc::NumEffectTypes; ++t) if (x->getStringAttribute ("typeName") == oc::getInfo (t).name) type = t;
        if (type < 0) return false;
    }
    setSlotType (slot, type, true);
    for (int j = 1; j < kNumSlotParamIds; ++j)
        if (x->hasAttribute (kSlotParamIds[j])) setParamValue (param (slot, kSlotParamIds[j]), (float) x->getDoubleAttribute (kSlotParamIds[j]));
    const int ver = x->getIntAttribute ("formatVersion", 2);
    if (ver < kStateFormat) migrateSlot (slot, std::max (2, ver));
    const auto res = x->getStringAttribute ("res");
    if (oc::typeUsesResource (type) && res.isNotEmpty()) loadSlotFile (slot, resolveLibraryFile (File (res)));
    return true;
}

// ------------------------------------------------------------------ window size
static PropertiesFile& globalSettings()
{
    static std::unique_ptr<PropertiesFile> pf;
    if (pf == nullptr)
    {
        PropertiesFile::Options o;
        o.applicationName = "OmniChain"; o.filenameSuffix = ".settings"; o.folderName = "OmniChain";
       #if JUCE_LINUX
        o.folderName = ".config/OmniChain";
       #endif
        o.osxLibrarySubFolder = "Application Support"; o.millisecondsBeforeSaving = 500;
        pf = std::make_unique<PropertiesFile> (o);
    }
    return *pf;
}

float OmniChainProcessor::getUiScale()
{
    const float def = (float) globalSettings().getDoubleValue ("uiScale", 1.0);
    return jlimit (0.6f, 2.5f, (float) (double) apvts.state.getProperty ("uiScale", def));
}

void OmniChainProcessor::setUiScale (float s)
{
    s = jlimit (0.6f, 2.5f, s);
    if (std::abs ((float) (double) apvts.state.getProperty ("uiScale", 0.0) - s) < 0.001f) return;
    apvts.state.setProperty ("uiScale", s, nullptr);
    globalSettings().setValue ("uiScale", s);
}

// ------------------------------------------------------------------ NAM size / imports
bool OmniChainProcessor::getNamLight() { return globalSettings().getBoolValue ("namLight", true); }

void OmniChainProcessor::setNamLight (bool on)
{
    if (on == getNamLight()) return;
    globalSettings().setValue ("namLight", on);
    reloadResourcesFromState();      // A2 captures switch between their Lite and Full sizes
}

// Copy downloaded captures / IRs (loose files or the .zip a website gives you) into your own models folder.
// Returns the files that were added; nothing is changed in the files themselves.
Array<File> OmniChainProcessor::importToneFiles (const Array<File>& files, bool nam)
{
    Array<File> added;
    const auto dest = getContentFolder (nam).getChildFile ("Imported");
    dest.createDirectory();
    auto wanted = [nam] (const String& name)
    {
        const auto ext = name.fromLastOccurrenceOf (".", false, false).toLowerCase();
        return nam ? ext == "nam" : (ext == "wav" || ext == "aif" || ext == "aiff" || ext == "flac");
    };
    auto unique = [] (File f) { int n = 2; const auto base = f.getFileNameWithoutExtension(); while (f.exists()) f = f.getSiblingFile (base + " (" + String (n++) + ")" + f.getFileExtension()); return f; };
    for (auto& f : files)
    {
        if (f.hasFileExtension ("zip"))
        {
            ZipFile zip (f);
            const auto sub = dest.getChildFile (File::createLegalFileName (f.getFileNameWithoutExtension()));
            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                const auto* e = zip.getEntry (i);
                if (e == nullptr || e->isSymbolicLink || ! wanted (e->filename) || e->filename.contains ("__MACOSX")) continue;
                std::unique_ptr<InputStream> in (zip.createStreamForEntry (i));
                if (in == nullptr) continue;
                sub.createDirectory();
                auto out = unique (sub.getChildFile (File::createLegalFileName (e->filename.replaceCharacter ('\\', '/').fromLastOccurrenceOf ("/", false, false))));
                FileOutputStream os (out);
                if (os.openedOk() && os.writeFromInputStream (*in, -1) > 0) { os.flush(); added.add (out); }
            }
        }
        else if (f.existsAsFile() && wanted (f.getFileName()))
        {
            if (f.isAChildOf (getContentFolder (nam))) { added.add (f); continue; }   // already in your folder
            auto out = unique (dest.getChildFile (f.getFileName()));
            if (f.copyFileTo (out)) added.add (out);
        }
    }
    return added;
}

// ------------------------------------------------------------------ downloads inbox (offline-first TONE3000 workflow)
// You download tones by hand in your browser; OmniChain notices new captures / IRs in your Downloads folder and
// offers to copy them into your own folders. After that everything works offline. Nothing is ever fetched.
File OmniChainProcessor::getInboxFolder()
{
    const File custom (globalSettings().getValue ("inboxFolder"));
    if (custom.isDirectory()) return custom;
    return File::getSpecialLocation (File::userHomeDirectory).getChildFile ("Downloads");
}

void OmniChainProcessor::setInboxFolder (const File& dir) { globalSettings().setValue ("inboxFolder", dir.getFullPathName()); }

static String inboxKey (const File& f) { return f.getFileName() + "|" + String (f.getSize()) + "|" + String (f.getLastModificationTime().toMilliseconds()); }

static bool isToneDownloadUncached (const File& f);
static bool isToneDownload (const File& f)
{
    static std::map<String, bool> cache;           // zips are only opened once (message thread)
    const auto key = inboxKey (f);
    auto it = cache.find (key);
    if (it != cache.end()) return it->second;
    const bool r = isToneDownloadUncached (f);
    if (cache.size() > 2000) cache.clear();
    cache[key] = r;
    return r;
}

static bool isToneDownloadUncached (const File& f)
{
    if (OmniChainProcessor::isNamFile (f)) return true;
    if (OmniChainProcessor::isIrFile (f)) return f.getSize() < 2 * 1024 * 1024;          // a short IR, not a song or a sample pack
    if (! f.hasFileExtension ("zip") || f.getSize() > 200 * 1024 * 1024) return false;
    ZipFile zip (f);
    for (int i = 0; i < zip.getNumEntries(); ++i)
        if (auto* e = zip.getEntry (i))
        {
            const auto n = e->filename.toLowerCase();
            if (n.contains ("__macosx")) continue;
            if (n.endsWith (".nam") || ((n.endsWith (".wav") || n.endsWith (".aif") || n.endsWith (".aiff") || n.endsWith (".flac")) && e->uncompressedSize < 2 * 1024 * 1024))
                return true;
        }
    return false;
}

Array<File> OmniChainProcessor::findNewDownloads()
{
    Array<File> found;
    const auto dir = getInboxFolder();
    if (! dir.isDirectory()) return found;
    auto& gs = globalSettings();
    // the first time, only look at the last two weeks of downloads
    if (! gs.containsKey ("inboxSince")) gs.setValue ("inboxSince", (Time::getCurrentTime() - RelativeTime::days (14)).toMilliseconds());
    const Time since (gs.getValue ("inboxSince").getLargeIntValue());
    StringArray seen; seen.addLines (gs.getValue ("inboxSeen"));
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.nam;*.zip;*.wav;*.aif;*.aiff;*.flac"))
        if (f.getLastModificationTime() > since && ! seen.contains (inboxKey (f)) && isToneDownload (f)) found.add (f);
    return found;
}

void OmniChainProcessor::markDownloadsSeen (const Array<File>& files)
{
    auto& gs = globalSettings();
    StringArray seen; seen.addLines (gs.getValue ("inboxSeen"));
    for (auto& f : files) seen.addIfNotAlreadyThere (inboxKey (f));
    while (seen.size() > 800) seen.remove (0);
    gs.setValue ("inboxSeen", seen.joinIntoString ("\n"));
}

// copy every capture and IR in these downloads into your folders (both kinds, so a mixed zip works)
Array<File> OmniChainProcessor::importDownloads (const Array<File>& files)
{
    auto added = importToneFiles (files, true);
    added.addArray (importToneFiles (files, false));
    markDownloadsSeen (files);
    return added;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OmniChainProcessor(); }
