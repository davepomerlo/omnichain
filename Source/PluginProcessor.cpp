#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static const char* const kSlotParamIds[] = { "type", "byp", "mix", "lvl", "p1", "p2", "p3", "p4", "p5", "p6" };
constexpr int kNumSlotParamIds = 10;

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
    startTimerHz (40);
}

OmniChainProcessor::~OmniChainProcessor() { stopTimer(); }

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
            grp->addChild (std::make_unique<AudioParameterFloat> (ParameterID { pid (s, "p" + String (k + 1)), 1 }, n + " Knob " + String (k + 1),
                                                                  NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                                  AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
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
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) ctx.bpm = *b;
            if (auto p = pos->getPpqPosition()) ctx.ppq = *p;
            ctx.playing = pos->getIsPlaying();
        }

    float* L = buffer.getWritePointer (0);
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
    state.setProperty ("formatVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void OmniChainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            currentPresetName = apvts.state.getProperty ("presetName", "Custom").toString();
            if (MessageManager::existsAndIsCurrentThread()) engine.service();
        }
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
    for (int s = 0; s < oc::kNumSlots; ++s)
        for (int j = 0; j < kNumSlotParamIds; ++j) setParamValue (param (s, kSlotParamIds[j]), v[(size_t) s][(size_t) j]);
}

void OmniChainProcessor::copySlot (int from, int to)
{
    if (from == to) return;
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
        }
        else setSlotType (s, oc::FxNone, true);
    }
    auto* in = apvts.getParameter ("in");   setParamValue (in, in->convertTo0to1 (p.inDb));
    auto* out = apvts.getParameter ("out"); setParamValue (out, out->convertTo0to1 (p.outDb));
    auto* mx = apvts.getParameter ("mix");  setParamValue (mx, 1.0f);
    currentPresetName = p.name;
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
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (f)) return false;
    currentPresetName = f.getFileNameWithoutExtension();
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
    currentPresetName = f.getFileNameWithoutExtension();
    return true;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OmniChainProcessor(); }
