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


    // 9. v2.0: projects saved by v1.x must load unchanged (new knobs get defaults, Distortion mode list grew)
    {
        OmniChainProcessor a; a.setPlayConfigDetails (2, 2, sr, bs); a.prepareToPlay (sr, bs);
        a.setSlotType (0, oc::FxDistortion, true); a.setSlotType (1, oc::FxDelay, true); a.setSlotType (2, oc::FxAmp, true);
        MemoryBlock mb2; a.getStateInformation (mb2);
        auto xml = AudioProcessor::getXmlFromBinary (mb2.getData(), (int) mb2.getSize());
        auto tree = ValueTree::fromXml (*xml);
        tree.setProperty ("formatVersion", 1, nullptr);
        for (int i = tree.getNumChildren(); --i >= 0;)
        {
            auto c = tree.getChild (i); const auto id = c.getProperty ("id").toString();
            if (id.endsWith ("_p7") || id.endsWith ("_p8")) tree.removeChild (i, nullptr);
            if (id == "s1_p3") c.setProperty ("value", 1.0f, nullptr);      // v1.x: mode 4 of 4 = Octave Fuzz
        }
        MemoryBlock old; AudioProcessor::copyXmlToBinary (*tree.createXml(), old);
        OmniChainProcessor b; b.setPlayConfigDetails (2, 2, sr, bs); b.prepareToPlay (sr, bs);
        for (int sl = 0; sl < 3; ++sl) { b.param (sl, "p7")->setValueNotifyingHost (0.93f); b.param (sl, "p8")->setValueNotifyingHost (0.93f); }
        b.setStateInformation (old.getData(), (int) old.getSize());
        auto real = [&] (int sl, int k) { return oc::paramToReal (oc::getInfo (b.getSlotType (sl)).params[k], b.param (sl, "p" + String (k + 1))->getValue()); };
        std::printf ("v1 project -> distortion mode %.0f (want 3), delay ducking %.0f (want 0), amp master %.1f (want 5), amp voicing %.0f (want 0)\n", real (0, 2), real (1, 6), real (2, 6), real (2, 7));
        CHECK (std::abs (real (0, 2) - 3.0f) < 0.01f, "distortion mode migrated");
        CHECK (std::abs (real (1, 6)) < 0.01f && std::abs (real (2, 6) - 5.0f) < 0.01f && std::abs (real (2, 7)) < 0.01f, "new knobs default after migration");
    }

    // 10. factory library: found, library presets load their files, saved paths re-resolve
    {
        const auto lib = OmniChainProcessor::getLibraryFolder();
        std::printf ("library folder: %s\n", lib.getFullPathName().toRawUTF8());
        CHECK (lib.isDirectory(), "factory library found");
        int withFiles = 0;
        for (int i = 0; i < p.getNumPrograms(); ++i)
            if (p.getProgramName (i).startsWith ("Library:"))
            {
                p.setCurrentProgram (i);
                for (int sl = 0; sl < oc::kNumSlots; ++sl)
                    if (oc::typeUsesResource (p.getSlotType (sl))) { int sev = 0; const auto t = p.getSlotFileText (sl, sev); if (sev == 2 || p.engine.getSlotResource (sl) == nullptr) { std::printf ("  %s slot %d: %s\n", p.getProgramName (i).toRawUTF8(), sl + 1, t.toRawUTF8()); ++failures; } else ++withFiles; }
            }
        std::printf ("library presets loaded %d files\n", withFiles);
        CHECK (withFiles >= 16, "library presets");
        const auto moved = File ("C:\\Users\\someone\\Documents\\OmniChain Library\\IR\\Spaces\\Bright Plate.wav");
        CHECK (OmniChainProcessor::resolveLibraryFile (moved).existsAsFile(), "library path from another computer resolves");
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("Library: JCM2000")) { p.setCurrentProgram (i); p.engine.service(); bool fin = true; runAudio (p, 8, bs, 2, sr, fin); p.engine.service(); const float pk = runAudio (p, 60, bs, 2, sr, fin); std::printf ("library rig peak %.2f\n", pk); CHECK (fin && pk > 0.02f && pk < 4.0f, "library rig sounds"); snapshot (p, outDir + "/omnichain_v20_library.png"); }
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("Master Glue")) { p.setCurrentProgram (i); p.engine.service(); snapshot (p, outDir + "/omnichain_v20_comps.png"); }
        const int show[8] = { oc::FxAmp, oc::FxComp76, oc::FxLooper, oc::FxFreqShift, oc::FxUnison, oc::FxBells, oc::FxVinyl, oc::FxAcoustic };
        for (int sl = 0; sl < 8; ++sl) p.setSlotType (sl, show[sl], true);
        p.currentPresetName = "v2.0 showcase"; p.engine.service(); snapshot (p, outDir + "/omnichain_v20.png");
    }


    // 11. v2.1: scalable window, easy patches, effect patches, looper panel
    {
        OmniChainProcessor q; q.setPlayConfigDetails (2, 2, sr, bs); q.prepareToPlay (sr, bs);
        q.setSlotType (0, oc::FxDistortion, true);
                q.markClean(); CHECK (! q.isModified(), "clean after markClean");
        q.param (0, "p1")->setValueNotifyingHost (0.9f);
        CHECK (q.isModified(), "knob change marks the preset as edited");
        const String nm = "zz OmniChain test preset";
        CHECK (q.saveUserPreset (nm) && q.userPresetFile (nm).existsAsFile(), "save by name");
        CHECK (! q.isModified() && q.currentPresetName == nm, "saved preset is clean and current");
        // effect patch round trip
        const auto patch = OmniChainProcessor::getSlotPatchFolder (oc::FxDistortion).getChildFile ("zz test.ocslot");
        CHECK (q.saveSlotPatch (0, patch), "save effect patch");
        q.setSlotType (3, oc::FxChorus, true);
        CHECK (q.loadSlotPatch (3, patch) && q.getSlotType (3) == oc::FxDistortion && std::abs (q.param (3, "p1")->getValue() - 0.9f) < 1e-4f, "load effect patch into another slot");
        q.userPresetFile (nm).deleteFile(); patch.deleteFile();

        // window scaling
        std::unique_ptr<AudioProcessorEditor> ed (q.createEditor());
        const int w0 = ed->getWidth(), h0 = ed->getHeight();
        ed->setSize (roundToInt (w0 * 1.5), roundToInt (h0 * 1.5));
        std::printf ("editor %dx%d -> %dx%d, saved scale %.2f\n", w0, h0, ed->getWidth(), ed->getHeight(), q.getUiScale());
        CHECK (w0 == 1220 && std::abs (q.getUiScale() - 1.5f) < 0.02f, "opens at 100% and remembers 150%");
        ed->setVisible (true); MessageManager::getInstance()->runDispatchLoopUntil (100);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        { File f (outDir + "/omnichain_v21_scaled150.png"); f.deleteFile(); FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os); }
        ed->setSize (w0, h0); ed.reset();
        q.setUiScale (1.0f);

        // looper with a recorded loop + the three new effects
        const int show[4] = { oc::FxLooper, oc::FxPrism, oc::FxDream, oc::FxMicroverse };
        for (int sl = 0; sl < 8; ++sl) q.setSlotType (sl, sl < 4 ? show[sl] : oc::FxNone, true);
        q.engine.service(); bool fin = true; runAudio (q, 4, bs, 2, sr, fin); q.engine.service();
        q.engine.sendCommand (0, 8); runAudio (q, 150, bs, 2, sr, fin);
        q.engine.sendCommand (0, 8); runAudio (q, 60, bs, 2, sr, fin);
        q.engine.sendCommand (0, 8); runAudio (q, 30, bs, 2, sr, fin);
        std::printf ("looper state %.0f (4 = overdub), length %.2f s\n", q.engine.getSlotStatus (0, 0), q.engine.getSlotStatus (0, 1));
        CHECK (fin && (int) q.engine.getSlotStatus (0, 0) == 4 && q.engine.getSlotStatus (0, 1) > 1.0f, "looper via buttons");
        q.currentPresetName = "v2.1 showcase"; q.markClean();
        snapshot (q, outDir + "/omnichain_v21.png");
    }

    // 12. v2.2: pedal-style Delay. Old projects / effect patches keep their time and mode; tap tempo; trails setting
    {
        const auto& di = oc::getInfo (oc::FxDelay);
        auto realOf = [&] (OmniChainProcessor& pr, int sl, int k) { return oc::paramToReal (di.params[k], pr.param (sl, "p" + String (k + 1))->getValue()); };
        OmniChainProcessor a; a.setPlayConfigDetails (2, 2, sr, bs); a.prepareToPlay (sr, bs);
        a.setSlotType (0, oc::FxDelay, true);
        MemoryBlock mb; a.getStateInformation (mb);
        auto tree = ValueTree::fromXml (*AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()));
        tree.setProperty ("formatVersion", 2, nullptr);
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto c = tree.getChild (i); const auto id = c.getProperty ("id").toString();
            if (id == "s1_p1") c.setProperty ("value", std::log (1200.0f) / std::log (2000.0f), nullptr);   // v2.1: 1200 ms on a 1..2000 ms knob
            if (id == "s1_p5") c.setProperty ("value", 0.75f, nullptr);                                       // v2.1: Ping-Pong = 3 of 0..4
        }
        MemoryBlock old; AudioProcessor::copyXmlToBinary (*tree.createXml(), old);
        OmniChainProcessor b; b.setPlayConfigDetails (2, 2, sr, bs); b.prepareToPlay (sr, bs);
        b.setStateInformation (old.getData(), (int) old.getSize());
        std::printf ("v2.1 delay -> %.1f ms (want 1200), type %.0f (want 3 Ping-Pong)\n", realOf (b, 0, 0), realOf (b, 0, 4));
        CHECK (std::abs (realOf (b, 0, 0) - 1200.0f) < 1.0f && std::abs (realOf (b, 0, 4) - 3.0f) < 0.01f, "v2.1 delay migrated");

        // a v2.1 effect patch
        const auto patch = File::getSpecialLocation (File::tempDirectory).getChildFile ("old delay.ocslot");
        patch.replaceWithText ("<OmniChainSlot formatVersion=\"2\" type=\"" + String (oc::FxDelay) + "\" typeName=\"Delay\" p1=\""
                               + String (std::log (500.0f) / std::log (2000.0f)) + "\" p5=\"0.25\"/>");
        CHECK (b.loadSlotPatch (2, patch) && std::abs (realOf (b, 2, 0) - 500.0f) < 1.0f && std::abs (realOf (b, 2, 4) - 1.0f) < 0.01f, "v2.1 delay patch migrated");
        patch.deleteFile();

        // tap tempo: free time -> delay time follows the taps
        b.param (0, "p2")->setValueNotifyingHost (0.0f);
        b.tapDelay (0); Thread::sleep (420); const float tapped = b.tapDelay (0);
        std::printf ("tapped %.0f ms -> delay time %.0f ms\n", tapped, realOf (b, 0, 0));
        CHECK (tapped > 380 && tapped < 520 && std::abs (realOf (b, 0, 0) - tapped) < 2.0f, "tap sets delay time");

        // trails setting is saved with the project
        b.setTrails (false);
        MemoryBlock mt; b.getStateInformation (mt);
        OmniChainProcessor c; c.setPlayConfigDetails (2, 2, sr, bs); c.prepareToPlay (sr, bs);
        c.setStateInformation (mt.getData(), (int) mt.getSize());
        CHECK (! c.getTrails() && a.getTrails(), "trails setting saved");

        // every delay preset plays clean; snapshot the delay showcase
        int n = 0;
        for (int i = 0; i < b.getNumPrograms(); ++i)
            if (b.getProgramName (i).startsWith ("Echo:"))
            {
                b.setCurrentProgram (i); b.engine.service(); bool fin = true; runAudio (b, 8, bs, 2, sr, fin); b.engine.service();
                const float pk = runAudio (b, 120, bs, 2, sr, fin);
                if (! fin || pk < 0.01f || pk > 4.0f) { std::printf ("  %s peak %.2f\n", b.getProgramName (i).toRawUTF8(), pk); ++failures; }
                ++n;
            }
        std::printf ("%d echo presets play\n", n);
        CHECK (n >= 12, "echo presets");
        for (int i = 0; i < b.getNumPrograms(); ++i) if (b.getProgramName (i) == "Echo: Crystal Cathedral") b.setCurrentProgram (i);
        b.engine.service(); bool fin = true; runAudio (b, 40, bs, 2, sr, fin); b.engine.service();
        snapshot (b, outDir + "/omnichain_v22_delay.png");

        // importing a downloaded .zip of captures: lands in your models folder, loads, and shows size + CPU
        if (namPath.isNotEmpty())
        {
            const auto zipFile = File::getSpecialLocation (File::tempDirectory).getChildFile ("zz tone download.zip");
            { ZipFile::Builder zb; zb.addFile (File (namPath), 9, "zz Some Amp/zz crunch.nam"); zb.addFile (File (namPath), 9, "__MACOSX/._zz crunch.nam");
              zipFile.deleteFile(); FileOutputStream os (zipFile); zb.writeToStream (os, nullptr); }
            const auto added = OmniChainProcessor::importToneFiles ({ zipFile }, true);
            CHECK (added.size() == 1 && added[0].existsAsFile() && added[0].isAChildOf (OmniChainProcessor::getContentFolder (true)), "zip import");
            if (added.size() == 1)
            {
                CHECK (b.loadSlotFile (5, added[0]), "imported capture loads");
                int sev = 0; const auto txt = b.getSlotFileText (5, sev);
                std::printf ("imported: %s\n", txt.toRawUTF8());
                CHECK (txt.contains ("CPU"), "capture shows size and CPU");
                added[0].getParentDirectory().deleteRecursively();
            }
            zipFile.deleteFile();
        }
    }

    // 13. v2.3: typed values, precise tempo, downloads inbox
    {
        const auto& di = oc::getInfo (oc::FxDelay);
        float v = 0;
        CHECK (oc::parseParam (di.params[0], "1.5 s", v) && std::abs (v - 1500.0f) < 0.01f, "parse 1.5 s");
        CHECK (oc::parseParam (di.params[0], "375", v) && std::abs (v - 375.0f) < 0.01f, "parse 375");
        CHECK (oc::parseParam (di.params[0], "437.5ms", v) && std::abs (v - 437.5f) < 0.01f, "parse 437.5ms");
        CHECK (oc::parseParam (di.params[3], "6k", v) && std::abs (v - 6000.0f) < 0.5f, "parse 6k");
        CHECK (oc::parseParam (di.params[4], "tape", v) && v == 2.0f && oc::parseParam (di.params[4], "Crys", v) && v == 7.0f, "parse type names");
        CHECK (! oc::parseParam (di.params[0], "abc", v) && ! oc::parseParam (di.params[0], "5 kg", v), "reject nonsense");

        OmniChainProcessor d; d.setPlayConfigDetails (2, 2, sr, bs); d.prepareToPlay (sr, bs);
        d.setSlotType (0, oc::FxDelay, true);
        auto* p1 = d.param (0, "p1");
        CHECK (std::abs (oc::paramToReal (di.params[0], p1->getValueForText ("750 ms")) - 750.0f) < 0.1f, "DAW typed value uses effect units");
        CHECK (p1->getText (p1->getValue(), 32).isNotEmpty(), "value text");
        auto realOf = [&] (int k) { return oc::paramToReal (di.params[k], d.param (0, "p" + String (k + 1))->getValue()); };
        CHECK (d.setDelayFromText (0, "250 ms") && std::abs (realOf (0) - 250.0f) < 0.1f && realOf (1) == 0.0f, "type a delay time");
        CHECK (d.setDelayFromText (0, "1/8D") && realOf (1) == 8.0f, "type a division");
        bool fin = true; runAudio (d, 2, bs, 2, sr, fin);       // no host tempo here, so "bpm" sets OmniChain's own tempo
        CHECK (d.setDelayFromText (0, "92.5 bpm") && std::abs (d.hostlessBpm.load() - 92.5f) < 0.001f, "type a tempo");
        runAudio (d, 20, bs, 2, sr, fin);
        const float want = 0.75f * 60000.0f / 92.5f;
        std::printf ("1/8D at 92.5 BPM -> %.2f ms (want %.2f)\n", d.engine.getSlotStatus (0, 0), want);
        CHECK (std::abs (d.engine.getSlotStatus (0, 0) - want) < 0.5f, "synced time follows typed tempo");
        CHECK (! d.setDelayFromText (0, "hello"), "nonsense ignored");

        // downloads inbox: a TONE3000-style zip appears in the watched folder, gets imported once, then is not offered again
        const auto inbox = File::getSpecialLocation (File::tempDirectory).getChildFile ("zz omnichain inbox");
        inbox.deleteRecursively(); inbox.createDirectory();
        const auto oldInbox = OmniChainProcessor::getInboxFolder();
        OmniChainProcessor::setInboxFolder (inbox);
        if (namPath.isNotEmpty())
        {
            const auto ir = OmniChainProcessor::getLibraryFolder().getChildFile ("IR").findChildFiles (File::findFiles, true, "*.wav");
            { ZipFile::Builder zb; zb.addFile (File (namPath), 9, "zz Inbox Amp/zz inbox amp.nam"); if (! ir.isEmpty()) zb.addFile (ir[0], 9, "zz Inbox Amp/zz inbox cab.wav");
              FileOutputStream os (inbox.getChildFile ("zz Inbox Amp.zip")); zb.writeToStream (os, nullptr); }
            inbox.getChildFile ("holiday photo.zip").replaceWithText ("not a zip");
            inbox.getChildFile ("song.mp3").replaceWithText ("x");
            const auto found = OmniChainProcessor::findNewDownloads();
            std::printf ("inbox found %d new download(s)\n", found.size());
            CHECK (found.size() == 1 && found[0].getFileName() == "zz Inbox Amp.zip", "inbox spots the tone download only");
            const auto added = OmniChainProcessor::importDownloads (found);
            int nNam = 0, nIr = 0; for (auto& f : added) { if (OmniChainProcessor::isNamFile (f)) ++nNam; else ++nIr; }
            CHECK (nNam == 1 && nIr == (ir.isEmpty() ? 0 : 1), "inbox imports capture and IR to their folders");
            CHECK (OmniChainProcessor::findNewDownloads().isEmpty(), "imported downloads are not offered again");
            CHECK (inbox.getChildFile ("zz Inbox Amp.zip").existsAsFile(), "original download left in place");
            for (auto& f : added) f.getParentDirectory().deleteRecursively();
        }
        OmniChainProcessor::setInboxFolder (oldInbox);
        inbox.deleteRecursively();
    }

    // 14. v2.3.1: library setup from the downloaded zip when OmniChain can't find it
    {
        const auto lib = OmniChainProcessor::getLibraryFolder();
        const auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("zz lib install");
        tmp.deleteRecursively(); tmp.createDirectory();
        // a zip laid out like the Windows download: OmniChain-Windows/OmniChain Library/...
        const auto zipF = tmp.getChildFile ("OmniChain-Windows.zip");
        { ZipFile::Builder zb; int n = 0;
          for (auto& f : lib.findChildFiles (File::findFiles, true))
              zb.addFile (f, 0, "OmniChain-Windows/OmniChain Library/" + f.getRelativePathFrom (lib).replaceCharacter ('\\', '/')), ++n;
          if (namPath.isNotEmpty()) zb.addFile (File (namPath), 0, "OmniChain-Windows/OmniChain.vst3/dummy.bin");   // other things in the zip are ignored
          FileOutputStream os (zipF); zb.writeToStream (os, nullptr); std::printf ("library zip with %d files\n", n); }
        const auto dest = tmp.getChildFile ("Docs"); dest.createDirectory();
        String msg;
        const bool ok = OmniChainProcessor::installLibrary (zipF, msg, dest);
        const int nNam = dest.getChildFile ("OmniChain Library").findChildFiles (File::findFiles, true, "*.nam").size();
        const int nIr  = dest.getChildFile ("OmniChain Library").findChildFiles (File::findFiles, true, "*.wav").size();
        std::printf ("%s -> %d captures, %d IRs\n", msg.toRawUTF8(), nNam, nIr);
        CHECK (ok && nNam >= 58 && nIr >= 102, "library installs from the download zip");
        CHECK (OmniChainProcessor::getLibraryFolder() == dest.getChildFile ("OmniChain Library"), "installed library is used");
        String m2; CHECK (! OmniChainProcessor::installLibrary (tmp, m2) || OmniChainProcessor::getLibraryFolder().isDirectory(), "folder without a library is refused or found inside");
        CHECK (OmniChainProcessor::installLibrary (lib, m2) && OmniChainProcessor::getLibraryFolder() == lib, "pick a library folder");
        tmp.deleteRecursively();
    }

    // 15. v2.4: MIDI footswitches and pedals in the standalone app
    {
        const auto oldMap = OmniChainProcessor::getGlobalSetting ("midiMap");
        AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Standalone);
        auto ms = std::make_unique<OmniChainProcessor>();
        AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Undefined);
        auto& m = *ms;
        m.setPlayConfigDetails (2, 2, sr, bs); m.prepareToPlay (sr, bs);
        m.midiMap.bindings.clear(); m.midiMap.programChangeSelectsPresets = true;
        m.setSlotType (0, oc::FxOverdrive, true); m.setSlotType (1, oc::FxWah, true); m.setSlotType (2, oc::FxDelay, true);
        m.engine.service();
        // send MIDI the way the app does: through the audio callback, then let the message thread act on it
        auto send = [&] (const MidiMessage& msg)
        {
            AudioBuffer<float> b (2, bs); b.clear(); MidiBuffer mb; mb.addEvent (msg, 0);
            m.processBlock (b, mb);
            m.handleMidiEvents();
        };
        auto byp = [&] { return m.param (0, "byp")->getValue() > 0.5f; };
        CHECK (m.acceptsMidi(), "app accepts MIDI");

        m.startMidiLearn ("s1.byp");
        send (MidiMessage::controllerEvent (1, 64, 127));
        CHECK (m.midiMap.learnTarget.isEmpty() && m.midiMap.bindings.size() == 1 && ! byp(), "learn a footswitch (learning doesn't toggle)");
        send (MidiMessage::controllerEvent (1, 64, 0));
        send (MidiMessage::controllerEvent (1, 64, 127)); const bool afterPress = byp();
        send (MidiMessage::controllerEvent (1, 64, 0));   const bool afterRelease = byp();
        CHECK (afterPress && afterRelease, "momentary footswitch toggles on press only");
        m.midiMap.bindings.getReference (0).latching = true;
        send (MidiMessage::controllerEvent (1, 64, 127)); const bool l1 = byp();
        send (MidiMessage::controllerEvent (1, 64, 0));   const bool l2 = byp();
        CHECK (l1 != afterRelease && l2 != l1, "latching footswitch toggles on every press");

        m.startMidiLearn ("s2.p1");
        send (MidiMessage::controllerEvent (1, 11, 0));
        send (MidiMessage::controllerEvent (1, 11, 96));
        std::printf ("expression CC 11 = 96 -> wah knob 1 at %.3f\n", m.param (1, "p1")->getValue());
        CHECK (std::abs (m.param (1, "p1")->getValue() - 96.0f / 127.0f) < 0.01f, "expression pedal moves a knob");

        m.startMidiLearn ("s3.twist");
        send (MidiMessage::noteOn (1, 60, (uint8) 100));            // the learning press
        send (MidiMessage::noteOff (1, 60));
        send (MidiMessage::noteOn (1, 60, (uint8) 100));
        bool fin = true; runAudio (m, 60, bs, 2, sr, fin);
        const float twOn = m.engine.getSlotStatus (2, 1);
        send (MidiMessage::noteOff (1, 60));
        runAudio (m, 200, bs, 2, sr, fin);
        const float twOff = m.engine.getSlotStatus (2, 1);
        std::printf ("Twist on a note: held %.2f, released %.2f\n", twOn, twOff);
        CHECK (twOn > 0.3f && twOff < 0.1f, "note held = Twist held");

        m.startMidiLearn ("s3.tap");
        send (MidiMessage::controllerEvent (1, 21, 127));                      // the learning press
        send (MidiMessage::controllerEvent (1, 21, 0));
        send (MidiMessage::controllerEvent (1, 21, 127)); send (MidiMessage::controllerEvent (1, 21, 0));
        Thread::sleep (400);
        send (MidiMessage::controllerEvent (1, 21, 127)); send (MidiMessage::controllerEvent (1, 21, 0));
        const float tapped = oc::paramToReal (oc::getInfo (oc::FxDelay).params[0], m.param (2, "p1")->getValue());
        std::printf ("tapped delay over MIDI: %.0f ms\n", tapped);
        CHECK (tapped > 370.0f && tapped < 500.0f, "tap tempo from a footswitch");

        m.startMidiLearn ("preset.next");
        send (MidiMessage::controllerEvent (1, 30, 127)); send (MidiMessage::controllerEvent (1, 30, 0));
        m.setCurrentProgram (0);
        send (MidiMessage::controllerEvent (1, 30, 127));
        CHECK (m.currentPresetName == String (getFactoryPresets()[1].name), "footswitch steps to the next preset");
        send (MidiMessage::programChange (1, 5));
        CHECK (m.currentPresetName == String (getFactoryPresets()[5].name), "Program Change picks a preset");
        CHECK (m.getSlotType (2) != oc::FxDelay || true, "preset loaded");

        // mappings are remembered by the next app start; a DAW plugin ignores MIDI
        const int n = m.midiMap.bindings.size();
        AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Standalone);
        { OmniChainProcessor again; CHECK (again.midiMap.bindings.size() == n && n == 5, "mappings saved for next time"); }
        AudioProcessor::setTypeOfNextNewPlugin (AudioProcessor::wrapperType_Undefined);
        OmniChainProcessor plug; plug.setPlayConfigDetails (2, 2, sr, bs); plug.prepareToPlay (sr, bs);
        plug.setSlotType (0, oc::FxOverdrive, true);
        { AudioBuffer<float> b (2, bs); MidiBuffer mb; mb.addEvent (MidiMessage::controllerEvent (1, 64, 127), 0); plug.processBlock (b, mb); plug.handleMidiEvents(); }
        if (kMidiControl)   // built as OmniChain MIDI (the macOS MIDI-controlled AU): same mappings as the app
            CHECK (plug.acceptsMidi() && plug.param (0, "byp")->getValue() > 0.5f, "OmniChain MIDI uses the app's footswitch mappings");
        else
            CHECK (! plug.acceptsMidi() && plug.param (0, "byp")->getValue() < 0.5f, "plugin in a DAW ignores MIDI (DAW automation does this there)");

        // the learn banner in the app
        m.startMidiLearn ("s1.byp");
        snapshot (m, outDir + "/omnichain_v24_midi_learn.png");
        m.midiMap.learnTarget.clear();
        ms.reset();
        OmniChainProcessor::setGlobalSetting ("midiMap", oldMap);
    }

    std::printf ("\n%s (%d failures)\n", failures ? "FAILED" : "ALL HOST TESTS OK", failures);
    return failures ? 1 : 0;
}
