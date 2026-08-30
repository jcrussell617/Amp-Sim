# Test Plan

## Functional checks

- Confirm silence in produces silence out.
- Confirm the guitar input reaches both output channels.
- Turn each control to minimum, center, and maximum and record the audible result.
- Move controls rapidly and listen for clicks, pops, or zipper noise.
- Change the audio device, sample rate, and buffer size while the app is open.

## Required matrix

| OS/device | Sample rate | Buffer | Result | CPU | Round-trip latency | Notes |
| --- | ---: | ---: | --- | ---: | ---: | --- |
| TBD | 44.1 kHz | 128 | | | | |
| TBD | 44.1 kHz | 256 | | | | |
| TBD | 48 kHz | 128 | | | | |
| TBD | 48 kHz | 256 | | | | |

## DSP checks

- Run `ctest --preset debug` after each DSP change.
- Test each filter using a sine sweep, impulse, or offline frequency-response script.
- Compare distortion spectra at 1x and optional oversampled processing before accepting oversampling.
- Check all output samples for NaN, infinity, and unexpected levels.
- Measure release-build CPU usage under the smallest practical buffer.

## Demo safety

Begin with the output at -12 dB or lower. Use headphones or monitors positioned to prevent feedback. Do not connect a speaker-level amplifier output to a line or instrument input.

