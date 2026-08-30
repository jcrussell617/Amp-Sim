#include "dsp/AmpProcessor.h"
#include <cmath>
#include <iostream>

int main()
{
    if (std::abs(AmpProcessor::softClip(0.0f)) > 0.000001f)
        return 1;
    if (!(AmpProcessor::softClip(2.0f) < 1.0f && AmpProcessor::softClip(2.0f) > 0.0f))
        return 2;
    if (std::abs(AmpProcessor::softClip(-0.5f) + AmpProcessor::softClip(0.5f)) > 0.000001f)
        return 3;

    AmpProcessor processor;
    processor.prepare(48000.0, 512, 2);
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    buffer.setSample(0, 0, 0.5f);
    buffer.setSample(1, 0, 0.5f);
    processor.process(buffer);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (!std::isfinite(buffer.getSample(channel, sample)))
                return 4;

    std::cout << "AmpProcessor tests passed\n";
    return 0;
}

