# Planet-8

A famous Japanese 80's analog poly-synth inspired polyphonic software synthesizer built with the JUCE framework. Available as a VST3 plugin and standalone application for Windows.

---

## Features

### Oscillators
- Dual oscillators (OSC1 & OSC2) with 6 waveforms: Sawtooth, Pulse, Triangle, Sine, Square, Noise
- Per-oscillator octave switching (16', 8', 4', 2') and fine-tune control
- Pulse width modulation via manual control, LFO, or envelope
- Oscillator cross-modulation and OSC2 hard sync

### Voice Engine
- 8-voice polyphony with voice stealing
- Three play modes: **Poly**, **Mono**, **Unison** (all 8 voices detuned)
- Portamento with configurable time, works across all play modes
- Legato mode and velocity sensitivity

### Filter
- 4-pole ZDF ladder filter
- Switchable 12 dB/oct and 24 dB/oct slopes
- High-pass filter (HPF) for tonal shaping
- Filter key follow and velocity sensitivity
- 4× oversampling and analog drift simulation

### Modulation
- **LFO**: Sine, Triangle, Sawtooth, Square, Sample & Hold — with delay/fade-in ramp and key retrigger
- **ENV 1 & ENV 2**: Independent ADSR envelopes with exponential decay/release
- **OSC MOD**: Routes LFO to oscillator pitch, filter cutoff, or VCA
- **KEYBRD MOD**: Pitch bend and mod wheel routing with independent OSC and filter targets

### Arpeggiator
- Modes: Up, Down, Up+Down, Random
- Octave range 1–4
- Internal BPM (40–240) or DAW host sync
- Step divisions: 1/4, 1/8, 1/16, 1/32

### Patch Memory
- Factory and user patch banks
- Searchable patch browser with context menu
- Save, initialize, and delete user patches
- Compare mode: toggle between saved state and current edits
- Full undo/redo (Ctrl+Z / Ctrl+Y)
- DAW state recall (plugin state saved with project)

---

## Building from Source

### Prerequisites
- [JUCE](https://juce.com/) (place at `../JUCE` relative to project root, or update paths in `Planet-8.jucer`)
- [Projucer](https://juce.com/discover/projucer) (included with JUCE)
- Visual Studio 2026 or later with C++ Desktop Development workload

### Steps

1. Open `Planet-8.jucer` in Projucer.
2. Verify JUCE module paths point to your JUCE installation.
3. Click **Save Project and Open in IDE**.
4. Build in Visual Studio using the **Release** configuration.
5. Compiled outputs land in `Builds/VisualStudio2026/x64/Release/`.

---

## Installation

### VST3 Plugin
1. Build the project or download a release.
2. Copy `Planet-8.vst3` to your VST3 plugin directory:
   - Windows: `C:\Program Files\Common Files\VST3\`
3. Rescan plugins in your DAW.

### Standalone
Run `Planet-8.exe` directly — no installation required. User patches are saved to `~/Documents/Planet-8/UserPatches.xml`.

---

## Project Structure

```
planet-8/
├── Planet-8.jucer          # JUCE project configuration
├── Source/
│   ├── PluginProcessor.h/cpp   # DSP engine, 8-voice management, MIDI, APVTS
│   ├── PluginEditor.h/cpp      # GUI layout and controls
│   ├── PatchManager.h/cpp      # Factory + user patch banks, undo/redo
│   ├── Oscillator.h/cpp        # polyBLEP oscillator with drift
│   ├── Filter.h                # ZDF ladder filter
│   ├── Envelope.h              # ADSR with exponential decay/release
│   └── LFO.h                   # LFO with delay ramp and key trigger
```
