// Headless integration test: drives the real OmniChainProcessor + editor
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

using namespace juce;

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { std::printf ("FAIL: %s\n", msg); ++failures; } } while (0)

static float runAudio (OmniChainProcessor& p, int blocks, int bs, int chans, double sr, bool& finite)
{
    AudioBuffer<float> buf (chans, bs); MidiBuffer midi; float peak = 0; static int64 t = 0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < bs; ++i, ++t)
        {
            const double ts = double (t) / sr; const double ph = std::fmod (ts, 0.5);
            float x = 0; for (int h = 1; h < 12; ++h) x += (float) std::sin (MathConstants<double>::twoPi * 146.8 * h * ts) / (float) h;
            x *= 0.25f * (float) std::exp (-ph * 5.0);
            for (int c = 0; c < chans; ++c) buf.setSample (c, i, x);
        }
        p.processBlock (buf, midi);
        for (int c = 0; c < chans; ++c)
            for (int i = 0; i < bs; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) finite = false;
                peak = jmax (peak, std::abs (v));
            }
        if (b % 8 == 0) MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    return peak;
}

static void snapshot (OmniChainProcessor& p, const String& file)
{
    std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
    ed->setVisible (true);
    for (int i = 0; i < 5; ++i) MessageManager::getInstance()->runDispatchLoopUntil (40);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    File f (file); f.deleteFile();
    FileOutputStream os (f); PNGImageFormat png; png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", file.toRawUTF8(), img.getWidth(), img.getHeight());
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String outDir = argc > 1 ? String (argv[1]) : String ("/tmp");
    const String namPath = argc > 2 ? String (argv[2]) : String();
    const double sr = 48000.0; const int bs = 512;

    OmniChainProcessor p;
    p.setPlayConfigDetails (2, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    CHECK (p.getLatencySamples() == 0, "latency must be 0");

    // 1. every factory preset renders sane audio
    const int np = p.getNumPrograms();
    for (int i = 0; i < np; ++i)
    {
        p.setCurrentProgram (i);
        p.engine.service();
        bool fin = true;
        runAudio (p, 8, bs, 2, sr, fin);           // let crossfades settle
        p.engine.service();
        const float pk = runAudio (p, 120, bs, 2, sr, fin);
        std::printf ("preset %-22s peak %.2f %s\n", p.getProgramName (i).toRawUTF8(), pk, fin ? "" : "NaN!");
        CHECK (fin, "non-finite output");
        CHECK (pk < 4.0f, "preset too loud");
        CHECK (i == 0 || pk > 0.005f, "preset silent");
    }

    // 2. state round-trip
    p.setCurrentProgram (4);
    MemoryBlock mb; p.getStateInformation (mb);
    OmniChainProcessor p2; p2.setPlayConfigDetails (2, 2, sr, bs); p2.prepareToPlay (sr, bs);
    p2.setStateInformation (mb.getData(), (int) mb.getSize());
    bool same = true;
    for (auto* prm : p.getParameters())
    {
        auto* a = dynamic_cast<RangedAudioParameter*> (prm);
        auto* b = p2.apvts.getParameter (a->getParameterID());
        if (b == nullptr || std::abs (a->getValue() - b->getValue()) > 1.0e-5f) same = false;
    }
    CHECK (same, "state round trip mismatch");
    CHECK (p2.currentPresetName == p.currentPresetName, "preset name not restored");
    std::printf ("state round-trip: %s, %d params, %d bytes\n", same ? "ok" : "MISMATCH", p.getParameters().size(), (int) mb.getSize());

    // 3. slot move keeps the chain
    p.setCurrentProgram (1); p.engine.service();
    const int t0 = p.getSlotType (0), t4 = p.getSlotType (4);
    p.moveSlot (0, 4);
    CHECK (p.getSlotType (4) == t0, "move: slot 0 should land at 4");
    CHECK (p.getSlotType (3) == t4, "move: neighbours shift down");
    { bool fin = true; runAudio (p, 50, bs, 2, sr, fin); CHECK (fin, "NaN after move"); }
    p.moveSlot (4, 0);
    CHECK (p.getSlotType (0) == t0, "move back");

    // 4. odd block sizes, mono layouts, sample rate change
    for (int bsz : { 1, 7, 33, 4096 })
    { bool fin = true; runAudio (p, bsz == 1 ? 3000 : 20, bsz, 2, sr, fin); CHECK (fin, "odd block size NaN"); }
    {
        OmniChainProcessor pm; pm.setPlayConfigDetails (1, 1, 44100.0, 256); pm.prepareToPlay (44100.0, 256);
        pm.setCurrentProgram (5); pm.engine.service(); bool fin = true; runAudio (pm, 8, 256, 1, 44100.0, fin); pm.engine.service();
        const float pk = runAudio (pm, 100, 256, 1, 44100.0, fin);
        CHECK (fin && pk > 0.001f, "mono layout");
        pm.prepareToPlay (96000.0, 1024); runAudio (pm, 50, 1024, 1, 96000.0, fin); CHECK (fin, "96k");
    }

    // 5. GUI snapshots
    p.setCurrentProgram (4); p.engine.service(); snapshot (p, outDir + "/omnichain_singing_lead.png");
    p.setCurrentProgram (10); p.engine.service();
    p.apvts.getParameter (OmniChainProcessor::pid (1, "byp"))->setValueNotifyingHost (1.0f);
    snapshot (p, outDir + "/omnichain_lofi.png");


    // 6. Cab IR + NAM file slots
    {
        // write a plausible cabinet IR (decaying, band-limited noise) as a 44.1 kHz stereo wav
        const File irFile = File (outDir).getChildFile ("Test Cab 4x12.wav");
        {
            irFile.deleteFile();
            AudioBuffer<float> ir (2, 2048); Random rnd (3); float lp1 = 0, lp2 = 0;
            for (int i = 0; i < 2048; ++i)
            {
                const float n = rnd.nextFloat() * 2 - 1;
                lp1 += 0.35f * (n - lp1); lp2 += 0.35f * (lp1 - lp2);
                const float v = lp2 * std::exp (-i / 180.0f) * 0.8f;
                ir.setSample (0, i, v); ir.setSample (1, i, v * 0.9f);
            }
            WavAudioFormat wav;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (irFile), 44100.0, 2, 24, {}, 0));
            w->writeFromAudioSampleBuffer (ir, 0, ir.getNumSamples());
        }
        const File namFile (namPath);
        p.setCurrentProgram (0); p.engine.service();
        CHECK (p.loadSlotFile (1, irFile), "load IR");
        CHECK (p.getSlotType (1) == oc::FxCabIR, "IR sets slot type");
        const bool namOk = p.loadSlotFile (0, namFile);
        CHECK (namOk, "load NAM"); { int sv = 0; std::printf ("NAM status: %s\n", p.getSlotFileText (0, sv).toRawUTF8()); }
        CHECK (p.getSlotType (0) == oc::FxNam, "NAM sets slot type");
        CHECK (! p.loadSlotFile (2, File (outDir).getChildFile ("nope.nam")), "missing file must fail");
        int sev = 0; const auto txt = p.getSlotFileText (2, sev);
        CHECK (sev == 2, "missing file shows error");
        p.engine.service(); bool fin = true; runAudio (p, 8, bs, 2, sr, fin); p.engine.service();
        const float pk = runAudio (p, 100, bs, 2, sr, fin);
        std::printf ("NAM + IR chain peak %.3f  slot1: '%s'\n", pk, p.getSlotFileText (1, sev).toRawUTF8());
        CHECK (fin && pk > 0.01f && pk < 4.0f, "NAM+IR level");

        // move: files must travel with their slots
        p.moveSlot (0, 3);
        CHECK (p.getSlotType (3) == oc::FxNam && p.getSlotFile (3) == namFile, "NAM file moved with slot");
        CHECK (p.getSlotType (0) == oc::FxCabIR && p.getSlotFile (0) == irFile, "IR shifted down");
        p.engine.service(); runAudio (p, 40, bs, 2, sr, fin); CHECK (fin, "NaN after file move");

        // project save / restore keeps the files
        MemoryBlock st; p.getStateInformation (st);
        OmniChainProcessor p3; p3.setPlayConfigDetails (2, 2, sr, bs); p3.prepareToPlay (sr, bs);
        p3.setStateInformation (st.getData(), (int) st.getSize());
        CHECK (p3.getSlotFile (3) == namFile && p3.engine.getSlotResource (3) != nullptr, "NAM restored from project");
        CHECK (p3.getSlotFile (0) == irFile && p3.engine.getSlotResource (0) != nullptr, "IR restored from project");
        bool fin3 = true; p3.engine.service(); runAudio (p3, 8, bs, 2, sr, fin3); p3.engine.service();
        const float pk3 = runAudio (p3, 60, bs, 2, sr, fin3);
        CHECK (fin3 && pk3 > 0.01f, "restored project makes sound");

        // changing the type away clears the file
        p.setSlotType (0, oc::FxDelay, true);
        CHECK (p.getSlotFile (0) == File() && p.engine.getSlotResource (0) == nullptr, "type change clears file");
        p.moveSlot (3, 0); p.loadSlotFile (1, irFile); p.engine.service();
        snapshot (p, outDir + "/omnichain_nam_ir.png");
    }


    // 7. showcase: all v1.2 effects in one chain
    {
        const int showcase[8] = { oc::FxSynth, oc::FxMelt, oc::FxTapEnsemble, oc::FxBarber, oc::FxRadio, oc::FxOrbit, oc::FxChaos, oc::FxGhostVerb };
        for (int s = 0; s < 8; ++s) p.setSlotType (s, showcase[s], true);
        p.currentPresetName = "v1.3 showcase";
        p.engine.service(); bool fin = true; runAudio (p, 8, bs, 2, sr, fin); p.engine.service();
        const float pk = runAudio (p, 200, bs, 2, sr, fin);
        std::printf ("showcase chain peak %.2f\n", pk);
        CHECK (fin && pk < 4.0f, "showcase chain");
        snapshot (p, outDir + "/omnichain_v13.png");
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("Holdsworth-Style Legato")) { p.setCurrentProgram (i); p.engine.service(); snapshot (p, outDir + "/omnichain_holdsworth.png"); }
    }


    // 8. standalone-app features: toolbar, tempo, recorder, tuner, metronome
    {
        AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Standalone);
        OmniChainProcessor ps;
        AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Undefined);
        CHECK (ps.isStandalone(), "standalone flavour");
        ps.setPlayConfigDetails (2, 2, sr, bs); ps.prepareToPlay (sr, bs);
        for (int i = 0; i < ps.getNumPrograms(); ++i) if (ps.getProgramName (i) == "Singing Lead") ps.setCurrentProgram (i);
        ps.hostlessBpm = 96.0f;
        ps.engine.service();
        // record two seconds
        CHECK (ps.startRecording(), "start recording");
        bool fin = true; runAudio (ps, int (2.0 * sr / bs), bs, 2, sr, fin);
        const double secs = ps.getRecordingSeconds();
        ps.stopRecording();
        const File rec = ps.lastRecording;
        std::unique_ptr<AudioFormatReader> rd (WavAudioFormat().createReaderFor (rec.createInputStream().release(), true));
        CHECK (rd != nullptr && std::abs ((double) rd->lengthInSamples / sr - secs) < 0.05 && secs > 1.9, "recording length");
        if (rd != nullptr) { Range<float> lv[2]; rd->readMaxLevels (0, rd->lengthInSamples, lv, 2); std::printf ("recorded peak %.2f\n", lv[0].getEnd()); CHECK (lv[0].getEnd() > 0.01f, "recording has audio"); }
        std::printf ("recorded %.2f s to %s (%lld samples in file)\n", secs, rec.getFileName().toRawUTF8(), rd ? (long long) rd->lengthInSamples : -1LL);
        rec.deleteFile();
        // metronome adds clicks even with silent input
        ps.metronomeOn = true;
        { AudioBuffer<float> b (2, bs); MidiBuffer m; float pk = 0; for (int k = 0; k < 40; ++k) { b.clear(); ps.processBlock (b, m); pk = jmax (pk, b.getMagnitude (0, bs)); }
          std::printf ("metronome click peak %.2f\n", pk); CHECK (pk > 0.05f, "metronome audible"); }
        ps.metronomeOn = false;
        // tuner: feed an A2 (110 Hz) and show the overlay
        ps.tunerOn = true;
        { AudioBuffer<float> b (2, bs); MidiBuffer m; int64 t = 0; float pk = 0;
          for (int k = 0; k < 40; ++k) { for (int i = 0; i < bs; ++i, ++t) { float x = 0; for (int h = 1; h < 8; ++h) x += std::sin (MathConstants<double>::twoPi * 110.6 * h * t / sr) / h; b.setSample (0, i, x * 0.2f); b.setSample (1, i, x * 0.2f); }
                                         ps.processBlock (b, m); pk = jmax (pk, b.getMagnitude (0, bs)); }
          CHECK (pk == 0.0f, "output muted while tuning"); }
        snapshot (ps, outDir + "/omnichain_standalone_tuner.png");
        ps.tunerOn = false;
        snapshot (ps, outDir + "/omnichain_standalone.png");
    }

    std::printf ("\n%s (%d failures)\n", failures ? "FAILED" : "ALL HOST TESTS OK", failures);
    return failures ? 1 : 0;
}
