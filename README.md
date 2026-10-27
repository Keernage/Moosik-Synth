# Moosik Synth

A polyphonic wavetable synthesizer plugin (VST3 + Standalone) written in C++ with [JUCE](https://juce.com).

## Features

- **16-voice polyphony**, each voice with two oscillators
- **Oscillators:** sine, band-limited saw and square (PolyBLEP anti-aliasing), and a morphing wavetable
- **Built-in wavetable:** 16 frames morphing sine → triangle → saw → square → pulse → formant, generated additively with 10 mip levels so high notes stay alias-free
- **Unison:** up to 8 stacked oscillators per note with detune and stereo spread
- **Filter:** stereo state-variable (TPT) low-pass with resonance and a dedicated ADSR envelope (±6 octaves)
- **LFO:** sine / triangle / saw / square, routable to cutoff, pitch, wavetable position or volume
- **Amp ADSR**, output gain, full host automation and preset state save/restore
- **Custom UI:** dark theme with arc-style knobs and an on-screen MIDI keyboard

## Building

Requires CMake 3.22+ and a C++17 compiler (Visual Studio 2022 on Windows).

```sh
git clone --recurse-submodules https://github.com/Keernage/Moosik-Synth.git
cd Moosik-Synth
cmake -B build
cmake --build build --config Release
```

Built plugins land in `build/MoosikSynth_artefacts/Release/` (`VST3/` and `Standalone/`).

If you cloned without `--recurse-submodules`, run `git submodule update --init` first.

## Project layout

| File | Purpose |
| --- | --- |
| `Source/PluginProcessor.*` | Parameter layout, MIDI handling, voice management, state |
| `Source/SynthVoice.h` | Per-voice DSP: oscillators, unison, filter, envelopes, LFO |
| `Source/Wavetable.h` | Additive, mip-mapped morphing wavetable |
| `Source/PluginEditor.*` | UI, look-and-feel, on-screen keyboard |
