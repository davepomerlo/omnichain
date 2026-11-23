# OmniChain: modular multi-FX (VST3)

An 8-slot effects chain where any slot can hold any of 63 built-in effects, plus a **Cab IR loader**
and a **Neural Amp Modeler (NAM) loader** for free captures of real amps, pedals and cabinets. You can reorder the chain by dragging,
and the plugin reports **0 samples of latency**. A full 8-slot chain uses about 1–2 % of one CPU core.

**Download page: https://davepomerlo.github.io/omnichain/**

## Play without a DAW: the OmniChain app

OmniChain also runs as its own app on a Windows or Mac laptop. Plug in your guitar interface and play.
* **Windows:** `OmniChain-Windows-Setup.exe` installs the app with a Start-menu shortcut (and optionally the VST3/CLAP plugins).
* **Mac:** open `OmniChain-macOS-App.dmg` and drag OmniChain into Applications. The first time, right-click the app and choose **Open**.

The app adds a toolbar the plugin doesn't need:
* **Audio settings:** pick your interface, input channel, sample rate and buffer size. On Windows, choose your interface's
  **ASIO** driver for the lowest latency (or "Windows Audio (Exclusive Mode)" if it has none). Smaller buffers mean lower latency.
* **Input:** your guitar input starts *muted* so laptop speakers can't feed back. Click it to turn it on. Use headphones or an interface.
* **Tempo + Tap:** sets the tempo for synced delays, tremolos, glitch, step filter and arpeggiator. Tap four times in time.
* **Metronome** with volume. It's not recorded.
* **Tuner:** a big, accurate tuner (within 0.2 cents in testing). The output is muted while it's open.
* **Record:** saves what you play, with all effects, as a 24-bit WAV in `Documents/OmniChain Recordings`. **Recordings** opens the folder.

The app remembers your last sound, audio device and tempo between launches.

## Downloads: pick your system

Get the latest version from the **Releases** page (right-hand side of the repo page):

| Download | Contains | Use it in |
|---|---|---|
| **OmniChain-Windows-Setup.exe** | Installer: app + optional VST3/CLAP | Easiest way on Windows |
| **OmniChain-macOS-App.dmg** | The standalone Mac app | Play without a DAW on a Mac |
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


**Installer error 183** ("cannot create a file when that file already exists"): an older, hand-copied OmniChain
was in the way, usually a single `OmniChain.vst3` file in `C:\Program Files\Common Files\VST3`. Installers from
v2.3.2 on clear that up by themselves. With an older installer, close your DAW, delete that `OmniChain.vst3` file
(not a folder of the same name), and run the installer again.
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

## New in 2.4

* **MIDI footswitches and expression pedals in the OmniChain app.** Plug in any MIDI foot controller (or a MIDI keyboard
  or knob box) and map it in a few seconds:
  1. **Right-click** the control you want: a slot's power button, any knob, the Delay's **Tap** or **Twist**, the Looper's
     buttons, the preset **<** **>** arrows, Input / Mix / Output, or Tap, Metronome, Tuner and Record in the top bar.
  2. Choose **MIDI learn**, then press the footswitch or move the pedal. Done.
  * **Footswitches** switch effects on and off, step presets, tap tempo, and drive the Looper. Twist is held as long as you hold the switch.
  * **Expression pedals** sweep any knob, for example the Wah, a delay's Feedback, or the global Mix.
  * **Latching pedals** (the ones that send *on* with one press and *off* with the next): right-click the control, or open
    **MIDI** in the top bar, and tick **Latching pedal**.
  * **Program Change** messages pick presets (factory first, then yours). Switch this off in the **MIDI** menu.
  * The **MIDI** button lists every mapping (change or remove them) and shows the last MIDI message received, which helps
    when a pedal doesn't seem to respond. MIDI devices are switched on automatically; you can see them under Audio settings.
  * Mappings belong to your rig, not to a preset: they stay the same whichever preset you load, and are remembered next time.
  * Inside a DAW, use the DAW's own MIDI learn / automation as before. Every knob, power button and the Looper's
    Footswitch are automatable there.

## New in 2.3

* **Type exact values.** Click the value under any knob and type it: `375`, `437.5 ms`, `1.5 s`, `6k`, `-3 dB`, `40%`,
  or a name like `Tape` or `1/8D`. The units follow the knob, and typing works in your DAW's own parameter boxes too.
  Hold **Shift** while dragging a knob for fine moves.
* **Delay read-out you can type into.** Click the time next to Tap / Twist and type a time (`375 ms`, `1.25 s`),
  a division (`1/8D`) or a tempo (`92.5 bpm`). When synced, it shows the whole sum, for example *1/4D @ 120 BPM = 750 ms*.
* **Precise tempo in the app:** the tempo box takes two decimals (for example `92.50 BPM`). Click it to type.
* **TONE3000, offline-first.** You download tones by hand on tone3000.com. OmniChain picks them up and never needs the internet to play:
  1. In a NAM or IR slot, choose **Load → Browse TONE3000** and download what you like (the zip is fine).
  2. Come back to OmniChain. The **Load** button turns blue when new captures or IRs are waiting in your Downloads folder.
  3. Choose **Load → Import new downloads**. They're copied into `Documents\OmniChain NAM Models` / `OmniChain IRs`
     and appear under **Load → Your captures / Your IRs**. Your downloads stay where they were.
  * If your browser saves somewhere else, pick that folder under **More → Downloads folder to watch**.
  * OmniChain only looks at files on your computer. It never connects to TONE3000 or anything else.

## New in 2.2

* **Delay, rebuilt after the TC Flashback 2 X4 and the BOSS DD-8.** 15 echo types (original voicings inspired by those
  pedals, not copies): **Digital**, **Analog** (longer times get darker, like a real bucket-brigade chip), **Tape**,
  **Ping-Pong**, **Reverse**, **Digital Mod**, **Dynamic** (echoes duck while you play), **Crystal** (repeats sparkle an
  octave up), **Shimmer** (every repeat climbs), **Lo-Fi**, **Warm**, **Multi-Head** (three tape heads), **Echo + Verb**,
  **Warp** (drifting, blooming ambience) and **Glitch** (tempo-locked stutters, reverse and octave slices).
  * Delay time now goes up to **10 seconds**.
  * Changing the time on the digital types crossfades cleanly; the analog and tape types bend the pitch like the real thing.
  * **Color** sets each type's character: modulation depth, shimmer amount, crush, reverb, heads or glitch density.
  * **Tap** tempo on the slot: tap twice or more. With Sync off it sets the time; with Sync on it sets the tempo.
  * **Twist** (hold): the repeats run away into oscillation, freeze (Reverse, Glitch, Ping-Pong) or bloom into a wash
    (Crystal, Shimmer, Echo + Verb, Warp). Let go and they settle back.
  * **Trails:** bypassing a Delay, Reverb or Spring Reverb lets the echoes ring out instead of cutting them.
    Switch it off under **More → Delay & reverb trails after bypass**.
  * 12 new **Echo:** presets show off the types. Old projects, presets and effect patches load exactly as before.
  * Still zero latency, and every type uses under 1% of a CPU core.
* **Lighter NAM captures:**
  * Each NAM slot now shows the capture's size (nano, feather, lite, standard, or A2 Lite / Full) and **what it really
    costs on your computer** (measured when it loads). Heavy ones turn yellow.
  * **NAM A2 captures** (the newest TONE3000 format) hold a Full and a Lite model in one file. OmniChain runs the
    **Lite** one by default to save CPU. Change it under **More → NAM: run A2 captures at their Lite size**.
* **Easier TONE3000 downloads:** in a NAM or IR slot, **Load → Import downloads** takes `.nam`, `.wav` or the `.zip`
  TONE3000 gives you. It copies them into your own folder and loads the first one. Everything you import shows up under
  **Load → Your captures / Your IRs**. You can also drop a `.zip` straight onto a slot. **Load → Find more on TONE3000** opens the site.

## New in 2.1

* **Resizable window** in every DAW and in the app: drag the bottom-right corner, or pick **Size** (75–200%) in the header.
  OmniChain remembers the size for that project and as the default for new windows.
* **Prism Shifter** (inspired by the Rainbow Machine): polyphonic pitch shifting (play chords), a second voice,
  and **Magic** feedback that sends the shifted sound back through itself for cascading, rising tones. *Swoop* mode
  slides every note up to the interval as you pick.
* **Dream Loop** (inspired by the MOOD MKII): a wet channel (Reverb, Delay or Slip) plus an always-listening micro-looper.
  It keeps replaying your last 0.25–8 seconds, in *Env* (follows your playing), *Tape* (speed and reverse) or
  *Stretch* (slower or faster without changing pitch). **Capture: Hold** freezes the loop, and **Clock** makes it gritty and lo-fi.
* **Microverse** (inspired by the Microcosm): ten tempo-locked algorithms (Mosaic, Glide, Haze, Tunnel, Strum, Blocks,
  Interrupt, Arp, Pattern, Warp) with four variations each, plus Activity, Shape, Repeats, Filter and Space.
* **Easier patches:**
  * **Save** asks for a name and saves straight into the preset list. Changed presets show **(edited)**.
  * **More** lets you rename, delete, revert, import or export.
  * Drag a `.ocpreset` file onto the window to import it.
* **Effect patches:** right-click any slot header and choose **Save effect patch** to keep just that effect's settings.
  **Load effect patch** puts them in any slot. You can also drop `.ocslot` files onto a slot.
* **New Looper:** Record / Close loop / Overdub on one big button, plus Play/Stop, **Undo/Redo** and Clear, with a timer and
  progress bar. **Start: On first note** waits until you play. To use a single MIDI footswitch, link it to the **Footswitch**
  knob: press cycles record, play and overdub; double-tap stops; hold 1 s undoes.
* **Cleaner pitch shifting:** the shared pitch engine now aligns its grains (SOLA), which removes the warble. This improves
  Pitch Shifter, Harmonic Arp, Spiral Delay, Hyper Unison, Amp Feedback and the reverb shimmer. Measured warble sidebands dropped to about -46 to -50 dB.

## Factory library (new in 2.0)

**Can't see the captures and IRs?** They're not effects in the effect list. Pick **NAM Capture** (or **Cab IR Loader**)
in a slot, then click **Load**: the library is the menu that opens, sorted into folders.
If that menu says **"Factory library: not found"**, OmniChain set it up for you from there (v2.3.1 and later):
* **Install it now from OmniChain-...zip** appears when the download zip is in your Downloads or on your Desktop. One click unpacks it to Documents.
* **I have it: pick the zip or the OmniChain Library folder** works wherever you saved it.
* **Download it** gets `OmniChain-Library.zip` (10 MB). Then choose **Load** again.

OmniChain also looks next to the plugin, in a OneDrive Documents folder, and in an unzipped download in Downloads or on the Desktop.
**More → Open the factory library folder** shows where the one it uses is.

OmniChain now comes with a free library: **58 NAM captures** (clean, crunch and high-gain amps, pedals,
full rigs, power amps and a bass preamp) and **102 impulse responses** (13 cabinets with 5–7 mic positions each,
spaces, acoustic bodies and oddities like telephone and megaphone).
* Click **Load** on a NAM Capture or Cab IR Loader slot. The library appears as a menu, sorted into folders.
* **Windows installer:** included automatically. **Zips, DMG, Linux:** copy the `OmniChain Library` folder to your
  **Documents** folder. The Mac app also carries its own copy inside the app.
* The "Library: ..." presets load complete rigs, for example *Library: JCM2000 Crunch + Green 4x12*.
* The library also downloads separately as `OmniChain-Library.zip` from the Releases page.

The NAM captures come from the community collection at github.com/pelennor2170/NAM_models (GPLv3, credited in
`CREDITS.txt`). The IRs were designed mathematically for OmniChain from speaker, microphone and room models. They are
voiced after classic cabinet styles, but they are not recordings of real cabinets. They're CC0, so use them for anything.

## Real amps and cabinets for free: NAM captures + cab IRs

**NAM Capture** plays `.nam` files: AI captures of real amps and pedals made by the Neural Amp Modeler community.
**Cab IR Loader** plays `.wav` impulse responses: recordings of real speaker cabinets.

**Where to get them free**
* **TONE3000** (tone3000.com, formerly ToneHunt): the biggest free library, with hundreds of thousands of NAM captures and cab IRs.
  It needs a free account. Click *Download* on a tone page, then use **Load → Import downloads** in OmniChain (no need to unzip).
  To keep things light, pick the **lite**, **feather** or **nano** size of a capture, or an **A2** capture (OmniChain runs its Lite size).
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
* NAM is the heaviest effect. The slot shows each capture's size and measured CPU. Feather and nano captures cost a small fraction of a standard one. Everything else stays light.
* Projects and presets remember the file *locations*. If you move or rename the files, the slot shows "File not found". Click Load to point it at the new place.

## What improved in 2.0

* **Cleaner distortion:** Overdrive, Distortion, Amp, Tape and the Ladder filter now run 2x or 4x oversampled, with no added
  latency. Measured aliasing dropped by 12–28 dB, so high notes and heavy gain sound smoother and less fizzy.
* **Amp + Cab:** a new **Master** knob and five **Voicings** (Brit Classic, US Clean, Tweed, Modern Gain, Bass Amp).
* **Overdrive:** four **Voices** (Screamer, Transparent, Blues, Treble Boost). **Distortion:** two new modes, DS hard-clip and HM chainsaw.
* **Delay:** **Ducking** (echoes step back while you play) and stereo **Spread**. **Reverb:** **Low Cut** and **Width**.
* **Compressor and Gate:** sidechain low cut, so low notes don't pump them. **EQ:** mid Q and output. **Tape:** 7.5, 15 and 30 ips speeds.
* **NAM Capture:** captures now play correctly at any project sample rate (they're converted around the capture, not
  run at the wrong speed). Every capture is auto-levelled, and there's a built-in noise **Gate**.
* **Smoother knobs:** every continuous knob glides, so automation and fast knob moves no longer zipper.
* Effects with more than six controls show them in a four-column layout. Projects and presets from 1.x load unchanged.

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
| Time & Space | **Delay** (15 pedal-style types up to 10 s, tap tempo, Twist, trails; host sync), Reverb (8-line FDN + **shimmer**), Spring Reverb |
| Creative | Bitcrusher, **Glitch Grid** (tempo-locked stutter / reverse / tape-stop), **Granular Cloud** |
| Utility | Stereo Imager (width, mono-bass, Haas, pan) |
| New in 1.2 | **Wah Pedal** (Cry Baby, Vox, Wide, Bass; pedal, auto, LFO or tempo sweep), **Freeze Pad**, **Step Filter**, **Harmonic Arp**, **Sympathetic Strings**, **Spiral Delay**, **Ghost Verb**, **Amp Feedback** |
| New in 1.3 | **Tap Ensemble** (8-tap Holdsworth-style chorus), **Guitar Synth** (pitch-tracking synth), **Barber Pole**, **Radio Tuner**, **Orbit**, **Melt**, **Chaos Echo** |
| New in 2.0: compressors | **FET 76** (with All-In mode), **Opto Leveler**, **Bus Compressor**, **Pedal Compressor**, **Tube Vari-Mu**, **Multiband** (3 bands, phase-aligned) |
| New in 2.0: effects | **Looper** (45 s, overdub, half/double speed, reverse, bar-synced), **Frequency Shifter**, **Wavefolder**, **Hyper Unison**, **Glass Bells**, **Vinyl**, **Underwater**, **Exciter**, **Doubler**, **Acoustic Simulator** |
| New in 2.1 | **Prism Shifter**, **Dream Loop**, **Microverse**, plus the redesigned **Looper** |
| New in 2.2 | **Delay** rebuilt with 15 types (Flashback 2 X4 / DD-8 inspired), Tap, Twist and trails |
| New in 2.3 | Typed knob values, typed delay time / tempo, offline-first TONE3000 downloads inbox |
| New in 2.4 | MIDI learn for footswitches and expression pedals in the standalone app |
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
NAM support uses NeuralAmpModelerCore (MIT, Steven Atkinson) and Eigen (MPL2). CLAP support uses clap-juce-extensions (MIT).
The Windows app uses the Steinberg ASIO SDK under its GPLv3 licence option. ASIO is a trademark of Steinberg Media Technologies GmbH. NAM captures and IRs belong to their creators.
Check each pack's terms before redistributing it.
