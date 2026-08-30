# Amp Sim Capstone

A cross-platform, real-time guitar amplifier simulator built in C++ with JUCE.

## MVP

The standalone desktop application accepts live input from an audio interface and provides:

- input gain and `tanh` soft-clipping distortion;
- bass, mid, and treble equalization;
- a high-frequency presence control;
- output volume;
- a simple real-time graphical interface.

Cabinet impulse responses, oversampling, boost, and reverb are optional only after the MVP is stable.

## Requirements

- Git
- CMake 3.22 or newer
- Ninja (recommended) or an IDE-supported CMake generator
- A C++17 compiler
  - Windows: Visual Studio 2022 with **Desktop development with C++**
  - macOS: Xcode command-line tools

JUCE 9.0.1 is downloaded automatically during the first CMake configure.

## Build

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

On Windows, run `build/debug/AmpSim_artefacts/Debug/Amp Sim Capstone.exe`.
On macOS, open `build/debug/AmpSim_artefacts/Debug/Amp Sim Capstone.app`.

The app opens JUCE's audio settings dialog from the **Audio Settings** button. Select the audio interface input containing the guitar and the desired output. Start with headphones or monitor volume low to avoid feedback and hearing damage.

## Repository map

- `src/dsp/`: real-time DSP independent of the GUI
- `src/MainComponent.*`: audio-device callback and controls
- `tests/`: small automated DSP safety tests
- `docs/`: design, testing, milestones, and research notes

## Definition of done for the MVP

- Live guitar input reaches the output without dropouts at a practical buffer size.
- Every control audibly changes the intended part of the signal.
- Parameter changes do not create obvious clicks.
- No memory allocation or locks occur in the audio processing loop.
- The app is tested at 44.1 kHz and 48 kHz and at two buffer sizes.
- A clean checkout builds from the documented commands.
- User and developer documentation and a short demo are complete.

## License

Project licensing is not yet selected. JUCE has its own licensing terms; review them before distributing binaries.

