# Architecture

## MVP signal chain

`audio input -> smoothed input gain -> tanh soft clipper -> bass shelf -> mid peak -> treble shelf -> presence peak -> smoothed output gain -> audio output`

The GUI writes parameter values into atomics. The audio callback reads those values and applies smoothing to gain changes. DSP state is owned by `AmpProcessor`, so it can later be reused in a plug-in if desired.

## Real-time rules

Code called by `process()` must not allocate memory, acquire locks, access files, print logs, or call GUI methods. All buffers and filters are prepared before playback. More expensive coefficient-update and oversampling decisions should be profiled before they become part of the MVP.

## Intentional limitations in version 0.1

- The input is mono and duplicated to stereo output.
- EQ center frequencies and Q values are fixed.
- The clipper is a normalized `tanh` waveshaper, not a model of a particular amplifier circuit.
- There is no cabinet simulation, which means the raw distortion may sound unusually bright.
- Presets and plug-in formats are outside the initial MVP.

