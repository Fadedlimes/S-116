<img width="1284" height="1007" alt="image" src="https://github.com/user-attachments/assets/a2eb3a5a-947e-49ec-9ea0-60cd21303d41" />

# S-116

S-116 is a software synthesizer plugin (VST3/AU/Standalone) built with JUCE. It is a dual-DCO polyphonic synth with a step sequencer, modulation matrix, and effects.

## Features

- Two DCOs with morph, PWM, wave folding, detune, and octave control
- Sub-oscillator and noise generator
- Hard sync between DCOs
- Juno-style resonant low-pass filter with routing options
- ADSR envelope with visual display
- LFO with multiple shapes and modulation targets
- Modulation matrix with multiple slots
- Chorus, stereo delay, and reverb
- 16-step sequencer with multiple pages and directions
- Arpeggiator and chord modes
- Preset save/load and expansion card support
- Oscilloscope display
- Multiple chassis, LED, and scope colour themes

## Building

Requires CMake 3.20+ and a C++17 compiler. JUCE is fetched automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Output formats are placed in `build/Simple116_artefacts/`.

## License

See repository for license details.
