#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/NamEffect.h"

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
    formats.registerBasicFormats();
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
            reloadResourcesFromState();
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
    for (int s = 0; s < oc::kNumSlots; ++s) setSlotPath (s, tree.getProperty ("res" + String (s + 1)).toString());
    reloadResourcesFromState();
    currentPresetName = f.getFileNameWithoutExtension();
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
        auto r = oc::loadNamFile (f.getFullPathName().toStdString());
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
        if (! loadSlotFile (s, f))
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
        return getSlotType (slot) == oc::FxNam ? "No model loaded - click Load or drop a .nam file here"
                                                : "No IR loaded - click Load or drop a .wav file here";
    String txt = String::fromUTF8 (res->name.c_str());
    const double sr = engine.getSampleRate();
    if (getSlotType (slot) == oc::FxNam && sr > 0 && std::abs (sr - res->sampleRate) > 1.0)
    {
        severity = 1;
        txt << "  |  model is " << String (res->sampleRate / 1000.0, 1) << " kHz, project " << String (sr / 1000.0, 1) << " kHz";
    }
    return txt;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OmniChainProcessor(); }
