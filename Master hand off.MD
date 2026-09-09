# Master Context Hand-Off: S-116 (Simple116) Flagship Synthesizer v2.0

---

## 1. Project Identity & Multi-Platform Target Environment
* **Project Name:** S-116 (Flagship 16-Voice Bi-Timbral Synthesizer Plugin & Standalone).
* **Local Project Root:** `/home/Atlin/Simple116/`
* **Development & Build Framework:** JUCE 8.0.6, C++17, CMake (Ninja / Make).
* **Target Platforms:**
  * **Linux (x86_64):** Fedora, Ubuntu, Arch; GCC 14 / Clang; ALSA / PipeWire / JACK.
  * **Windows (x86_64, ARM64):** MSVC 2022 (`/utf-8` flag); ASIO / WASAPI.
  * **macOS (Universal Binary):** Apple Clang / Xcode; CoreAudio; AU, VST3, Standalone (Hardened Runtime).
* **UI Canvas Footprint:** Fixed `1360 x 820` pixel console architecture.

---

## 2. Complete Project File Structure & Role Map

```text
/home/Atlin/Simple116/
│
├── CMakeLists.txt                      # JUCE 8 multi-platform CMake config with ASIO & platform flags
├── EXP-00.s116card                     # Shipped beta test expansion container (8 calibrated presets)
│
├── Source/
│   ├── PluginProcessor.h & .cpp       # 16-voice DSP engine, APVTS parameters, Bi-Timbral Proxy, 
│   │                                   # XML state serialization, patch/expansion disk handling.
│   ├── PluginEditor.h & .cpp          # Master UI (1360x820): Workstation LCD, Vector ADSR LCD,
│   │                                   # Vector Sequencer LCD, FX deck, keybed, theme dispatch.
│   │
│   ├── DSP/
│   │   ├── SequencerEngine.h          # Dual-track 64-step sequencer, per-step gate lengths, ratchets,
│   │   │                               # Elektron P-Lock matrix (ParamLocks), QWERTY step-edit API.
│   │   ├── ExpansionCardManager.h     # Virtual Cartridge Engine: .s116card parser, live in-plugin
│   │   │                               # cartridge burning (Workflow A), folder auto-discovery.
│   │   ├── PresetBank.h               # 64 Calibrated Factory Presets (Dance Classics, Acids, Pads).
│   │   ├── ModulationMatrix.h         # 16-slot bi-timbral modulation routing engine.
│   │   ├── VoiceManager.h             # 16-voice allocator, 9 voice modes, polyphonic gain staging.
│   │   ├── SynthVoice.h               # Dual DCOs, audio-rate FM, wavefolder, JP-8000 supersaw,
│   │   │                               # ZDF IR3109 cascade filter + SEM SVF 12dB filter.
│   │   ├── LFO.h                      # Multi-waveform LFO generators.
│   │   ├── StereoChorus.h             # Roland Juno I/II & Dimension D stereo chorus/flanger.
│   │   ├── StereoDelay.h              # Tempo-synced ping-pong stereo delay with damping.
│   │   └── StereoReverb.h             # High-density algorithmic stereo plate/hall reverb.
│   │
│   └── UI/
│       ├── SkinManager.h              # Hardware chassis & LED theme definitions, LookAndFeel,
│       │                               # persistent settings engine (settings.xml auto-save).
│       └── OscilloscopeComponent.h    # Phosphor vector CRT oscilloscope with trigger stabilization
│                                       # and 6 color palettes matching LED themes + White.
```

---

## 3. Core Directives & Agent Rules of Engagement (NON-NEGOTIABLE)

1. **USER PROFILE & ASSUMPTION PROTOCOL (CRITICAL):**
   * The user is an expert musician, audio engineer, and sound designer, but has **little to no programming experience**.
   * **DO NOT ASSUME OR PRODUCE CODE PREEMPTIVELY.** Agents must **NEVER** output file edits or code files without being explicitly instructed to do so. Always discuss design, verify ideas, provide ASCII mockups when UI is involved, and wait for user approval before writing code.
2. **STRICT ZERO-SNIPPET RULE:**
   * When asked to produce a file, **ALWAYS output 100% COMPLETE, fully working source files** from `#pragma once` / line 1 to the final closing brace.
   * **NEVER** output partial diffs, code fragments, or placeholder comments (`// ... rest unchanged ...`). The user copies and replaces entire files directly into their environment; any missing snippet will break their compilation.
3. **SINGLE-FILE PROTOCOL:**
   * Deliver a maximum of **ONE (1) source file per response** to stay well within token ceilings and prevent file truncation.
4. **AUDIO-THREAD REALTIME SAFETY:**
   * Strictly **zero heap allocations** (`malloc`, `new`, `std::vector::push_back`, `juce::String` building) inside `processBlock`, voice allocation, or clock routines. Use fixed-size stack arrays only.
   * Maintain `juce::ScopedNoDenormals noDenormals;` at the start of `processBlock`.
   * Master saturation uses pure `std::tanh(x)` without multipliers exceeding 1.0 (never hard-clip with `std::clamp`).
   * Polyphonic gain compensation remains constant (`0.48f` Poly/Mono, `0.35f` Unison).
5. **FOCUS INTEGRITY:**
   * All on-screen knobs, sliders, and standard buttons must have `setWantsKeyboardFocus(false)` and `setMouseClickGrabsKeyboardFocus(false)` so tweaking sound design parameters never steals window focus from QWERTY keyboard playing.
   * Keyboard focus is reserved specifically for the on-screen keybed and the new Vector Sequencer LCD (`seqLcd`).
6. **THEME DECOUPLING:**
   * Hardware Chassis Theme (`guiTheme`), LED Theme (`ledTheme`), and CRT Theme are persistent user-environment settings. They must **NEVER** be saved into or overwritten by patch XML files (`createPatchXml` / `loadPatchXml`).
   * System themes are auto-persisted to `Documents/Fadedlimes/S-116/settings.xml`.

---

## 4. Completed Architectural Accomplishments

* **Sequencer LCD Redesign (Command LCD v2.0):**
  * Replaced the 16 text buttons with an interactive `438 x 96 px` Vector LCD inside the `458 x 236 px` panel.
  * Real-time playhead beam with phosphor persistence, vertical velocity bars, note pitch readouts, ratchet burst tags (`2x`, `3x`, `4x`), and gold P-Lock diamonds (`◆`).
  * **Per-Step Gate Length:** Individual note duration stripes per step, adjustable via a dedicated `STEP GATE` hardware dial (`TRK` global inheritance or `5%` to `200%` staccato/legato).
  * **QWERTY Pattern-Entry Mode:** High-contrast cursor bracket (`╔══╗`) around the selected step. Users navigate steps via `←`/`→` (with automatic page flipping across 64 steps), toggle triggers and audition via `Spacebar`, and transpose semitones via `↑`/`↓` (`Shift + ↑/↓` for octave jumps).
  * **Front-Panel Patch BPM & Host Sync:** Dedicated physical `BPM` dial (20–300 BPM) and `[HOST]` sync toggle.
* **Virtual Cartridge Burning Engine (Workflow A):**
  * Hardware-style expansion system based on `.s116card` container files in `Documents/Fadedlimes/S-116/Expansions/`.
  * **Live Cartridge Creation:** `CARD` -> `Create New Blank Cartridge...` formats and mounts an expansion in real time.
  * **In-Place Patch Burning:** When a cartridge is mounted, hitting `[SAVE]` allows burning patches directly into the mounted card file on disk or saving as local user presets.
  * **EXP-00 Beta Expansion:** Shipped with 8 verified factory test presets testing acid lines, ratchets, and bi-timbral splits.
* **CRT Vector Oscilloscope Overhaul:**
  * Harmonized oscilloscope color palettes 1:1 with the synth's LED console themes: `Cyan`, `Phosphor Green`, `Amber`, `Ruby Red`, `Ultra Violet`, plus clean `Ice White`.
  * Fully persisted across synth restarts/reboots via `Documents/Fadedlimes/S-116/settings.xml`.
* **Performance Deck Ergonomics:**
  * Restored Pitch Bend and Mod Wheel to physical vertical linear faders (`LinearVertical`).
  * Added instant double-click snap-back to `0.0` for wheels and default recall for console knobs.

---

## 5. Current State & Post-1.0 Hand-Off Readiness

```text
[CURRENT STATUS] v1.0 MILESTONE COMPLETED & VERIFIED CLEAN BUILD
    │
    ├── Compilation: Clean build on Linux GCC 14 / Clang (Release) via CMake
    ├── Sequencer: Vector LCD + QWERTY step-entry + per-step gate length verified
    ├── Expansion Bus: .s116card mounting, live burning & EXP-00 validated
    └── UI & Hardware Controls: Fully calibrated & themed
```

### Readiness for the Next Agent:
The codebase is in a stable, fully compiling state with clean modularity between DSP, data structures, and the UI. The next agent will be assisting with **post-1.0 fixes, quality-of-life enhancements, soundbank expansion, or new DSP additions**.

**Remember:** 
1. Greet the user, confirm readiness, and ask what they would like to work on.
2. Discuss the design and outline the approach first.
3. Wait for explicit user confirmation before writing or replacing any files.
