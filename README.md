# OmniChain: modular multi-FX (VST3)

An 8-slot effects chain where any slot can hold any of 44 built-in effects, plus a **Cab IR loader**
and a **Neural Amp Modeler (NAM) loader** for free captures of real amps, pedals and cabinets. You can reorder the chain by dragging,
and the plugin reports **0 samples of latency**. A full 8-slot chain uses about 1–2 % of one CPU core.

**Download page: https://davepomerlo.github.io/omnichain/**

## Downloads: pick your system

Get the latest version from the **Releases** page (right-hand side of the repo page):

| Download | Contains | Use it in |
|---|---|---|
| **OmniChain-Windows.zip** | `OmniChain.vst3`, `OmniChain.clap`, `OmniChain.exe` | FL Studio, Ableton Live, Cubase, Reaper, Studio One, Bitwig, Cakewalk, Reason, … |
| **OmniChain-macOS.zip** (Apple Silicon + Intel, macOS 11+) | `OmniChain.component` (AU), `OmniChain.vst3`, `OmniChain.clap`, `OmniChain.app` | Logic Pro, GarageBand, MainStage (AU); Ableton, Cubase, Reaper, Studio One, Bitwig, FL Studio Mac (VST3) |
| **OmniChain-Linux.tar.gz** (x86-64) | `OmniChain.vst3`, `OmniChain.lv2`, `OmniChain.clap`, standalone `OmniChain` | Reaper, Bitwig, Ardour, Carla, Qtractor, … |

**Which format?** **VST3** works almost everywhere. **AU** is for Logic and GarageBand. **CLAP** is a newer open format
for hosts that support it (Bitwig, Reaper, Studio One 7). **LV2** is for Linux hosts like Ardour.
Install whichever your DAW uses. Installing several does no harm.

**Pro Tools:** Pro Tools only loads Avid's AAX format, which needs a paid Avid developer agreement and code-signing,
so it isn't included. To use OmniChain there, run it inside a free plugin "wrapper" that Pro Tools can load,
or as the standalone app routed into Pro Tools.

### Windows
1. Right-click the zip → **Properties** → tick **Unblock** → OK, then extract it.
2. Copy `OmniChain.vst3` (the whole folder) to `C:\Program Files\Common Files\VST3\`
   and `OmniChain.clap` to `C:\Program Files\Common Files\CLAP\` (create the folder if needed).
3. Rescan plugins in your DAW. In FL Studio: **Options → Manage plugins → Find installed plugins**.

### macOS
1. Double-click the zip to extract it.
2. Copy the files to these folders (in Finder use **Go → Go to Folder…**):
   * `OmniChain.component` → `~/Library/Audio/Plug-Ins/Components/`
   * `OmniChain.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   * `OmniChain.clap` → `~/Library/Audio/Plug-Ins/CLAP/`
3. The plugin isn't notarised by Apple (that needs a paid developer account), so macOS will quarantine it.
   Open **Terminal** and run this once:
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/OmniChain.component ~/Library/Audio/Plug-Ins/VST3/OmniChain.vst3 ~/Library/Audio/Plug-Ins/CLAP/OmniChain.clap
   ```
   For the app: right-click `OmniChain.app` → **Open** → **Open**.
4. Logic Pro: **Logic Pro → Settings → Plug-in Manager → Reset & Rescan Selection** if it doesn't appear.
   OmniChain is under **Audio FX → OmniChain**.

### Linux
```
tar xzf OmniChain-Linux.tar.gz && cd OmniChain-Linux
mkdir -p ~/.vst3 ~/.lv2 ~/.clap
cp -r OmniChain.vst3 ~/.vst3/ && cp -r OmniChain.lv2 ~/.lv2/ && cp OmniChain.clap ~/.clap/
```

The standalone app (`OmniChain.exe` / `OmniChain.app` / `OmniChain`) lets you plug your guitar interface in and play with no DAW needed.
Set the audio device under *Options → Audio/MIDI settings*.

## Real amps and cabinets for free: NAM captures + cab IRs

**NAM Capture** plays `.nam` files: AI captures of real amps and pedals made by the Neural Amp Modeler community.
**Cab IR Loader** plays `.wav` impulse responses: recordings of real speaker cabinets.

**Where to get them free**
* **TONE3000** (tone3000.com, formerly ToneHunt): the biggest free library, with hundreds of thousands of NAM captures and cab IRs.
  It needs a free account. Click *Download* on a tone page and unzip.
* Classic free cab IR packs: **God's Cab** (Wilkinson Audio), **Kalthallen Cabs**, the **Origin Effects IR Cab Library**,
  and the **Redwirez** free IR library.

**How to use them**
1. Unzip the downloads anywhere. OmniChain suggests `Documents\OmniChain NAM Models` and `Documents\OmniChain IRs`.
2. Drag a `.nam` or `.wav` file from Explorer onto any slot. The slot switches to the right effect and loads it.
   You can also pick *NAM Capture* or *Cab IR Loader* in a slot's menu and click **Load**.
3. Use the small `<` `>` buttons next to Load to step through the other files in the same folder. That's the quickest way to audition a pack.
4. A NAM capture tagged **"amp head"** or **"preamp"** has no speaker, so put a **Cab IR** slot right after it.
   **"Full rig"** captures already include the cab. Don't add another.

**Tips**
* Most NAM captures are made at **48 kHz**. Set FL Studio to 48 kHz (*Options → Audio settings*) for the exact tone.
  The slot shows a yellow note if the rates differ.
* **Normalize** (on by default) evens out the volume between captures. **Input** sets how hard you hit the model, like a guitar's volume knob.
* NAM is the heaviest effect: about 1–10 % of a CPU core per capture, depending on its size. Everything else stays light.
* Projects and presets remember the file *locations*. If you move or rename the files, the slot shows "File not found". Click Load to point it at the new place.

## The 1.2 effects in one line each

* **Wah Pedal:** Mode *Pedal* = you move it. *Auto* follows your picking, *LFO* sweeps freely, *Sync* sweeps in time with the song.
  In Auto/LFO/Sync, the Pedal knob sets how far the sweep travels.
* **Freeze Pad:** *Auto* grabs each new note and holds it as a pad. *Hold* freezes whatever is ringing at that moment
  (automate it, or map it to a footswitch). *Off* fades the pad away.
* **Step Filter:** 16-step filter patterns (rise, fall, zigzag, random) and trance-gate rhythms, locked to the song tempo.
* **Harmonic Arp:** your playing is pitch-shifted through an arpeggio (octaves, major, minor, fifths, cascade, random) on the beat.
* **Sympathetic Strings:** six virtual strings tuned to a key and chord. They ring in sympathy with your notes, like a sitar or harp.
* **Spiral Delay:** each echo is pitch-shifted again, climbing or falling forever. *Spin* bounces the spiral between speakers.
* **Ghost Verb:** a huge reverb that ducks out of the way while you play and blooms when you stop. *Swell* slows the bloom.
* **Amp Feedback:** hold a note and it blooms into singing octave (or fifth) feedback. Re-picking resets it, just like a real amp.

## The 1.3 effects and artist-style presets

* **Tap Ensemble:** up to 8 mono delay taps (15–50 ms by default), panned hard left to hard right. This is the
  "bunch of delay lines" chorus Allan Holdsworth described, which gives width with no seasick warble.
* **Guitar Synth:** follows the note you play and drives a synth voice (saw, square, organ or sub sine; -2 to +1 octave).
  Play single notes. Chords confuse any pitch tracker. *Attack* gives a slow, breathy swell, and *Sub Sine* at -1 octave makes a great bass sub.
* **Barber Pole:** a flanger or phaser that seems to rise (or fall) forever, the Shepard-tone illusion.
* **Radio Tuner:** AM radio, telephone, walkie-talkie or gramophone. *Detune* drifts you off-station into whistles and static, and *Scan* wanders the dial.
* **Orbit:** your sound circles your head with real Doppler pitch shift and head shadow. Best on headphones.
* **Melt:** every new note slowly sags in pitch and darkens, like warm tape. It resets on each new pick.
* **Chaos Echo:** four echoes whose times are steered by a Lorenz attractor. It never repeats the same way twice.

**Artist-style presets** (my own approximations from published rig descriptions, not official sounds):
* *Holdsworth-Style Legato Lead:* compression and a smooth, mid-forward Boogie-ish drive with the fizz rolled off, into an
  8-tap ensemble, a short delay and a little reverb. *Swell Chords:* auto volume swells into a clean amp and a wide tap ensemble.
* *SynthAxe-Style Lead:* Guitar Synth with a breathy attack, through the tap ensemble.
* *Cisneros-Style Sleep Fuzz Bass:* overdrive with clean blend, then Muff fuzz at 70 % mix (parallel, so the low end
  survives), into a cranked amp on the 4x10 cab with a low-mid push. *OM Dub Bass:* warm, compressed bass with tape echo and spring.
  On bass, adjust the Input knob so your bass hits the chain about as hard as a guitar would.

### Using an expression pedal with the Wah in FL Studio
Wiggle the Wah's **Pedal** knob, then choose **Tools → Last tweaked → Link to controller…** in FL Studio and move your MIDI
expression pedal. Any other knob can be linked the same way, for example Freeze Pad's *Mode* on a footswitch.

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
| New in 1.2 | **Wah Pedal** (Cry Baby, Vox, Wide, Bass; pedal, auto, LFO or tempo sweep), **Freeze Pad**, **Step Filter**, **Harmonic Arp**, **Sympathetic Strings**, **Spiral Delay**, **Ghost Verb**, **Amp Feedback** |
| New in 1.3 | **Tap Ensemble** (8-tap Holdsworth-style chorus), **Guitar Synth** (pitch-tracking synth), **Barber Pole**, **Radio Tuner**, **Orbit**, **Melt**, **Chaos Echo** |
| Loaders | **NAM Capture** (Neural Amp Modeler `.nam`), **Cab IR Loader** (`.wav/.aif/.flac`, zero-latency partitioned convolution) |

### About "zero latency"
No effect uses lookahead, so the plugin reports 0 samples to the DAW and nothing needs compensating.
The IR loader uses a head + partitioned-FFT design: the first 128 taps run directly, which hides the FFT block delay completely.
A few effects delay the sound *on purpose*, the way their hardware versions do: chorus, flanger, tape wow, pitch shifting and granular.
That delay is part of the effect, not added processing latency.

## Building from source

To build it yourself with Visual Studio (2022 or newer, with the "Desktop development with C++" workload):

```
git clone --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git JUCE
git clone --depth 1 --branch v0.6.0 https://github.com/sdatkinson/NeuralAmpModelerCore.git ThirdParty/NeuralAmpModelerCore
git clone --depth 1 --branch 3.4.1 https://gitlab.com/libeigen/eigen.git ThirdParty/eigen
git clone --recursive https://github.com/free-audio/clap-juce-extensions.git ThirdParty/clap-juce-extensions   # optional: CLAP
cmake -S . -B build -A x64
cmake --build build --config Release --target OmniChain_VST3 OmniChain_CLAP OmniChain_Standalone
# macOS adds OmniChain_AU, Linux adds OmniChain_LV2
```

### Automatic builds (GitHub Actions)

Every push to `main` runs the DSP stress test, then builds Windows (Visual Studio), macOS (universal, ad-hoc signed,
checked with Apple's `auval`) and Linux (checked with Steinberg's VST3 validator) in parallel, and attaches the three packages to the run.
Download it from the **Actions** tab → latest run → *Artifacts* (you need to be signed in).

To publish a version anyone can download: **Actions** tab → *Build OmniChain* → **Run workflow**, type a version such as `v1.1`, and press Run.
(Pushing a git tag like `v1.1` does the same.) The zip then appears on the **Releases** page.

Code layout: `Source/dsp/` is the JUCE-free DSP engine (effects, chain, parameter catalogue). `Source/Plugin*` is the JUCE wrapper and GUI.
`tests/` holds an offline DSP stress test and a headless host test.

Adding an effect: add an entry to the `EffectType` enum and the `kInfos` table (`EffectInfo.cpp`),
write a class in `Effects.cpp`, and add it to `createEffect()`. The GUI and automation pick it up automatically.

Licence note: built on JUCE 7 under its GPLv3 option, which is fine for personal use and for sharing the source under GPLv3.
NAM support uses NeuralAmpModelerCore (MIT, Steven Atkinson) and Eigen (MPL2). CLAP support uses clap-juce-extensions (MIT). NAM captures and IRs belong to their creators.
Check each pack's terms before redistributing it.
