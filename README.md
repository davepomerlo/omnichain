# OmniChain: modular multi-FX (VST3)

An 8-slot effects chain where any slot can hold any of 29 effects. You can reorder the chain by dragging,
and the plugin reports **0 samples of latency**. A full 8-slot chain uses about 1–2 % of one CPU core.

## Install in FL Studio (Windows 10/11, 64-bit)

1. If Windows marks the zip as downloaded from the internet, right-click it → **Properties** → tick **Unblock** → OK, then extract it.
2. Copy the whole **`OmniChain.vst3`** folder to `C:\Program Files\Common Files\VST3\`.
3. In FL Studio: **Options → Manage plugins → Find installed plugins** (or *Start scan*).
   OmniChain appears under *Effects*. Tick the star to add it to favourites.
4. In the Mixer, click an insert slot ▸ choose **OmniChain**.

`OmniChain.exe` is the same thing as a standalone app (plug your guitar interface in and play, no DAW needed).
Set the audio device under *Options → Audio/MIDI settings*.

## Using it

* **Pick an effect:** use the dropdown on any slot's header.
* **Reorder:** drag a slot by its header onto another slot, or use the `<` `>` buttons. Delay and reverb tails move with the slot.
* **Right-click a header:** reset, duplicate into the next empty slot, move to the start or end, or clear.
* **Power button:** bypass (click-free). **MIX:** per-slot wet/dry (parallel compression, etc.). **LEVEL:** per-slot output trim.
* **Global:** Input, Mix (whole-chain dry/wet) and Output, with in/out meters.
* **Knobs:** double-click resets, Ctrl/Cmd-drag gives fine control, and hovering shows the value.
* **Presets:** 20 factory presets. **Save/Load** writes `.ocpreset` files to `Documents\OmniChain Presets`.
  They appear in the preset list automatically. Factory presets are also exposed to the host as VST programs.
* **Automation:** every knob is automatable. Each slot exposes *Type, Bypass, Mix, Level, Knob 1–6*.
  The knob's text changes to match whichever effect is loaded.

## The effects

| Category | Effects |
|---|---|
| Dynamics | Noise Gate, Compressor (VCA → opto character), Transient Shaper, Limiter / Clipper, **Slow Swell** (auto volume swells) |
| Drive & Amp | Overdrive (screamer + clean blend), Distortion / Fuzz (Rat, Muff, Fuzz Face, Octave Fuzz + dying-battery bias), **Amp + Cab** (3 tube stages, tone stack, sag, 4 cabs), Tape Machine |
| Filter & EQ | 5-band EQ, Envelope Filter (auto-wah), Ladder Filter + LFO, **Talk Box** (A-E-I-O-U formant morph) |
| Modulation | Chorus (1–4 voices), Flanger (tempo sync), Phaser (2–12 stages), Tremolo / Pan (harmonic mode), Uni-Vibe, Rotary Speaker, Ring Mod |
| Pitch | Octaver (-1 / -2 / +1), Pitch Shifter / Harmonizer (2 voices + feedback spiral) |
| Time & Space | Delay (digital, analog, tape, ping-pong, **reverse**; host sync), Reverb (8-line FDN + **shimmer**), Spring Reverb |
| Creative | Bitcrusher, **Glitch Grid** (tempo-locked stutter / reverse / tape-stop), **Granular Cloud** |
| Utility | Stereo Imager (width, mono-bass, Haas, pan) |

### About "zero latency"
No effect uses lookahead or FFT blocks, so the plugin reports 0 samples to the DAW and nothing needs compensating.
A few effects delay the sound *on purpose*, the way their hardware versions do: chorus, flanger, tape wow, pitch shifting and granular.
That delay is part of the effect, not added processing latency.

## Building from source

The included `OmniChain.vst3` was cross-compiled with llvm-mingw. To build it yourself with Visual Studio 2022:

```
git clone --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target OmniChain_VST3 OmniChain_Standalone
```

### Automatic builds (GitHub Actions)

Every push to `main` runs the DSP stress test, builds the plugin with Visual Studio on GitHub's Windows machines,
and attaches **OmniChain-Windows** (VST3 + standalone + this README) to the run.
Download it from the **Actions** tab → latest run → *Artifacts* (you need to be signed in).

To publish a version anyone can download, create a tag such as `v1.0`. The zip then appears on the **Releases** page.

Code layout: `Source/dsp/` is the JUCE-free DSP engine (effects, chain, parameter catalogue). `Source/Plugin*` is the JUCE wrapper and GUI.
`tests/` holds an offline DSP stress test and a headless host test.

Adding an effect: add an entry to the `EffectType` enum and the `kInfos` table (`EffectInfo.cpp`),
write a class in `Effects.cpp`, and add it to `createEffect()`. The GUI and automation pick it up automatically.

Licence note: built on JUCE 7 under its GPLv3 option, which is fine for personal use and for sharing the source under GPLv3.
