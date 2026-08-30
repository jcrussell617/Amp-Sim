# Research Sources

## Already collected

- JUCE tutorials and API documentation
- JUCE source repository and CMake example
- JUCE `AudioProcessorValueTreeState` tutorial (more relevant if a plug-in or presets are added)
- Robert Bristow-Johnson's Audio EQ Cookbook

## Still needed for the capstone literature review

Add complete citation metadata and notes, not only URLs.

1. A DSP textbook or university reference covering sampling, Nyquist, IIR filters, and nonlinear systems.
2. A paper or book chapter on virtual-analog amplifier/distortion modeling and waveshaping.
3. A source that explains aliasing caused by nonlinear processing and compares mitigation methods.
4. A real-time audio-programming source covering audio-thread constraints, latency, and parameter smoothing.
5. A measurement/testing source for frequency response, total harmonic distortion, CPU use, and latency.
6. If cabinet simulation enters scope: convolution, cabinet impulse responses, and the license/provenance of every IR file.

## Recommended next additions

- Ross Bencina, "Real-time audio programming 101: time waits for nothing" - practical audio-callback constraints: https://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
- Välimäki and Huovilainen, "Oscillator and Filter Algorithms for Virtual Analog Synthesis," *Computer Music Journal* 30(2), 2006 - peer-reviewed virtual-analog and antialiasing background: https://research.aalto.fi/en/publications/oscillator-and-filter-algorithms-for-virtual-analog-synthesis/
- Wilczek, Wright, Välimäki, and Habets, "Virtual Analog Modeling of Distortion Circuits Using Neural Ordinary Differential Equations" - useful literature-review comparison even though the MVP is conventional DSP: https://research.aalto.fi/files/89272276/Wilczek_et_alii_VIRTUAL_ANALOG_MODELING_OF_DISTORTION_CIRCUITS_USING_NEURAL_ORDINARY_DIFFERENTIAL_EQUATIONS.pdf
- JUCE `dsp::Oversampling` documentation - implementation reference and latency/filter tradeoffs: https://docs.juce.com/develop/classjuce_1_1dsp_1_1Oversampling.html
- JUCE `dsp::Convolution` documentation - implementation reference if cabinet IR becomes optional scope: https://docs.juce.com/develop/classjuce_1_1dsp_1_1Convolution.html

For the final report, also obtain one recognized DSP textbook through the university library. A book such as *DAFX: Digital Audio Effects* is stronger background evidence than a collection of web tutorials.

## Citation note

YouTube tutorials are useful learning aids and may appear in an appendix or resource list, but the literature review should rely mainly on papers, books, university material, and official documentation. Record each video's title, creator/channel, publication date, URL, and access date rather than pasting a bare link.
