// OmniChain - loads the REAL built VST3 the way a DAW does, puts a NAM capture and a cab IR in it,
// and checks that both actually change the sound. Catches problems that only show up in the shipped
// plugin binary (for example the linker dropping NAM's model types). Usage:
//   OmniChainPluginTest <path to OmniChain.vst3> <capture.nam> <cab.wav>
#include <JuceHeader.h>
#include "../Source/dsp/Effects.h"

using namespace juce;

static int failures = 0;
#define CHECK(c, what) do { if (! (c)) { std::printf ("FAIL: %s\n", what); ++failures; } else std::printf ("ok: %s\n", what); } while (0)

// Run audio through the plugin; returns the largest difference from the input and the output peak
static void run (AudioPluginInstance& p, int blocks, int bs, double sr, float& diff, float& peak, bool& finite)
{
    AudioBuffer<float> buf (2, bs); MidiBuffer midi; static int64 t = 0;
    diff = 0; peak = 0; finite = true;
    for (int b = 0; b < blocks; ++b)
    {
        AudioBuffer<float> in (2, bs);
        for (int i = 0; i < bs; ++i, ++t)
        {
            const float x = 0.2f * (float) std::sin (MathConstants<double>::twoPi * 196.0 * (double) t / sr)
                          + 0.05f * (float) std::sin (MathConstants<double>::twoPi * 1250.0 * (double) t / sr);
            for (int c = 0; c < 2; ++c) { buf.setSample (c, i, x); in.setSample (c, i, x); }
        }
        p.processBlock (buf, midi);
        if (b < blocks / 2) continue;                      // let it settle first
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < bs; ++i)
            {
                const float y = buf.getSample (c, i);
                if (! std::isfinite (y)) finite = false;
                diff = jmax (diff, std::abs (y - in.getSample (c, i)));
                peak = jmax (peak, std::abs (y));
            }
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juce;
    if (argc < 4) { std::printf ("usage: OmniChainPluginTest <OmniChain.vst3> <capture.nam> <cab.wav>\n"); return 2; }
    const String vst3 (argv[1]); const File nam (File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    const File cab (File::getCurrentWorkingDirectory().getChildFile (argv[3]));
    CHECK (nam.existsAsFile() && cab.existsAsFile(), "test files exist");

    VST3PluginFormat fmt;
    OwnedArray<PluginDescription> descs;
    fmt.findAllTypesForFile (descs, File::getCurrentWorkingDirectory().getChildFile (vst3).getFullPathName());
    CHECK (descs.size() > 0, "plugin found");
    if (descs.isEmpty()) return 1;
    const double sr = 48000.0; const int bs = 512;
    String err;
    auto plugin = fmt.createInstanceFromDescription (*descs[0], sr, bs, err);
    CHECK (plugin != nullptr, "plugin instance");
    if (plugin == nullptr) { std::printf ("%s\n", err.toRawUTF8()); return 1; }
    plugin->setPlayConfigDetails (2, 2, sr, bs);
    plugin->prepareToPlay (sr, bs);

    // empty chain passes the sound through unchanged (baseline)
    float diff = 0, peak = 0; bool fin = true;
    run (*plugin, 20, bs, sr, diff, peak, fin);
    std::printf ("empty chain: max difference %.6f\n", diff);
    CHECK (diff < 1.0e-4f, "empty chain is transparent");

    // a project with a NAM capture in slot 1, the same way a DAW restores it
    auto chain = [&] (int slotType, const File& file)
    {
        // a VST3 hosted by JUCE saves <VST3PluginState><IComponent>base64 of the plugin's own state</IComponent>...
        MemoryBlock mb; plugin->getStateInformation (mb);
        auto outer = AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        if (outer == nullptr || outer->getChildByName ("IComponent") == nullptr) return false;
        MemoryBlock inner; inner.fromBase64Encoding (outer->getChildByName ("IComponent")->getAllSubText());
        auto xml = AudioProcessor::getXmlFromBinary (inner.getData(), (int) inner.getSize());
        if (xml == nullptr) return false;
        for (auto* e : xml->getChildIterator())
        {
            const auto id = e->getStringAttribute ("id");
            if (id == "s1_type") e->setAttribute ("value", slotType);
            if (id == "s1_byp")  e->setAttribute ("value", 0);
            if (id == "s1_mix")  e->setAttribute ("value", 100);
        }
        xml->setAttribute ("res1", file.getFullPathName());
        MemoryBlock newInner; AudioProcessor::copyXmlToBinary (*xml, newInner);
        outer->getChildByName ("IComponent")->deleteAllTextElements();
        outer->getChildByName ("IComponent")->addTextElement (newInner.toBase64Encoding());
        if (auto* ec = outer->getChildByName ("IEditController")) outer->removeChildElement (ec, true);
        MemoryBlock out; AudioProcessor::copyXmlToBinary (*outer, out);
        plugin->setStateInformation (out.getData(), (int) out.getSize());
        for (int k = 0; k < 10; ++k) { MessageManager::getInstance()->runDispatchLoopUntil (30); run (*plugin, 2, bs, sr, diff, peak, fin); }
        return true;
    };

    CHECK (chain (oc::FxNam, nam), "project with a NAM capture restored");
    run (*plugin, 40, bs, sr, diff, peak, fin);
    std::printf ("NAM capture %s: max difference %.4f, peak %.3f\n", nam.getFileName().toRawUTF8(), diff, peak);
    CHECK (fin && diff > 0.01f && peak > 0.005f && peak < 8.0f, "NAM capture changes the sound inside the real plugin");

    CHECK (chain (oc::FxCabIR, cab), "project with a cab IR restored");
    run (*plugin, 40, bs, sr, diff, peak, fin);
    std::printf ("cab IR %s: max difference %.4f, peak %.3f\n", cab.getFileName().toRawUTF8(), diff, peak);
    CHECK (fin && diff > 0.01f && peak > 0.005f && peak < 8.0f, "cab IR changes the sound inside the real plugin");

    plugin->releaseResources();
    plugin.reset();
    std::printf ("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PLUGIN TESTS OK", failures);
    return failures ? 1 : 0;
}
