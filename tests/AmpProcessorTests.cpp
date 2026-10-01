#include "dsp/AmpProcessor.h"

#include <cmath>
#include <iostream>

namespace
{
constexpr int blockSize = 512;
constexpr double pi = 3.14159265358979323846;

void fillSineWave(juce::AudioBuffer<float>& buffer,
                  double sampleRate,
                  double frequency,
                  float amplitude)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto phase =
            2.0 * pi * frequency
            * static_cast<double>(sample)
            / sampleRate;

        const auto value =
            amplitude * static_cast<float>(std::sin(phase));

        for (int channel = 0;
             channel < buffer.getNumChannels();
             ++channel)
        {
            buffer.setSample(channel, sample, value);
        }
    }
}

bool bufferContainsOnlyFiniteSamples(
    const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0;
         channel < buffer.getNumChannels();
         ++channel)
    {
        for (int sample = 0;
             sample < buffer.getNumSamples();
             ++sample)
        {
            if (!std::isfinite(
                    buffer.getSample(channel, sample)))
            {
                return false;
            }
        }
    }

    return true;
}

float getMaximumMagnitude(
    const juce::AudioBuffer<float>& buffer)
{
    float maximum = 0.0f;

    for (int channel = 0;
         channel < buffer.getNumChannels();
         ++channel)
    {
        maximum = std::max(
            maximum,
            buffer.getMagnitude(
                channel,
                0,
                buffer.getNumSamples()));
    }

    return maximum;
}

float getAverageRms(
    const juce::AudioBuffer<float>& buffer)
{
    float total = 0.0f;

    for (int channel = 0;
         channel < buffer.getNumChannels();
         ++channel)
    {
        total += buffer.getRMSLevel(
            channel,
            0,
            buffer.getNumSamples());
    }

    return total
        / static_cast<float>(buffer.getNumChannels());
}

bool channelsMatch(
    const juce::AudioBuffer<float>& buffer,
    float tolerance)
{
    if (buffer.getNumChannels() < 2)
        return false;

    for (int sample = 0;
         sample < buffer.getNumSamples();
         ++sample)
    {
        const auto difference = std::abs(
            buffer.getSample(0, sample)
            - buffer.getSample(1, sample));

        if (difference > tolerance)
            return false;
    }

    return true;
}

int fail(int code, const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return code;
}
}

int main()
{
    // Test 1: basic soft-clipping behavior.
    if (std::abs(AmpProcessor::softClip(0.0f))
        > 0.000001f)
    {
        return fail(1, "Soft clip changed silence");
    }

    if (!(AmpProcessor::softClip(2.0f) < 1.0f
          && AmpProcessor::softClip(2.0f) > 0.0f))
    {
        return fail(
            2,
            "Soft clip did not limit a positive sample");
    }

    if (std::abs(
            AmpProcessor::softClip(-0.5f)
            + AmpProcessor::softClip(0.5f))
        > 0.000001f)
    {
        return fail(
            3,
            "Soft clip is not symmetric");
    }

    // Test 2: silence should remain silent.
    {
        AmpProcessor processor;
        processor.prepare(48000.0, blockSize, 2);

        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();

        processor.process(buffer);

        if (!bufferContainsOnlyFiniteSamples(buffer))
            return fail(
                4,
                "Silence produced a non-finite sample");

        if (getMaximumMagnitude(buffer) > 0.000001f)
            return fail(
                5,
                "Silence produced audible output");
    }

    // Test 3: extreme settings must remain numerically stable.
    {
        AmpProcessor processor;
        auto& parameters = processor.parameters();

        parameters.gainDb.store(36.0f);
        parameters.bassDb.store(12.0f);
        parameters.midDb.store(12.0f);
        parameters.trebleDb.store(12.0f);
        parameters.presenceDb.store(12.0f);
        parameters.outputDb.store(6.0f);

        processor.prepare(48000.0, blockSize, 2);

        juce::AudioBuffer<float> buffer(2, blockSize);
        fillSineWave(buffer, 48000.0, 440.0, 1.0f);

        processor.process(buffer);

        if (!bufferContainsOnlyFiniteSamples(buffer))
            return fail(
                6,
                "Extreme settings produced NaN or infinity");
    }

    // Test 4: a higher output setting should be louder.
    {
        AmpProcessor quietProcessor;
        AmpProcessor loudProcessor;

        quietProcessor.parameters().gainDb.store(0.0f);
        quietProcessor.parameters().outputDb.store(-24.0f);

        loudProcessor.parameters().gainDb.store(0.0f);
        loudProcessor.parameters().outputDb.store(0.0f);

        quietProcessor.prepare(48000.0, blockSize, 2);
        loudProcessor.prepare(48000.0, blockSize, 2);

        juce::AudioBuffer<float> quietBuffer(2, blockSize);
        juce::AudioBuffer<float> loudBuffer(2, blockSize);

        fillSineWave(
            quietBuffer, 48000.0, 440.0, 0.1f);

        fillSineWave(
            loudBuffer, 48000.0, 440.0, 0.1f);

        quietProcessor.process(quietBuffer);
        loudProcessor.process(loudBuffer);

        const auto quietRms = getAverageRms(quietBuffer);
        const auto loudRms = getAverageRms(loudBuffer);

        if (!(loudRms > quietRms))
            return fail(
                7,
                "Increasing output did not increase RMS level");
    }

    // Test 5: identical stereo inputs should stay identical.
    {
        AmpProcessor processor;
        processor.prepare(48000.0, blockSize, 2);

        juce::AudioBuffer<float> buffer(2, blockSize);
        fillSineWave(buffer, 48000.0, 220.0, 0.25f);

        processor.process(buffer);

        if (!channelsMatch(buffer, 0.000001f))
            return fail(
                8,
                "Identical stereo channels no longer match");
    }

    // Test 6: common sample rates should both work.
    {
        constexpr double sampleRates[] =
        {
            44100.0,
            48000.0
        };

        for (const auto sampleRate : sampleRates)
        {
            AmpProcessor processor;
            processor.prepare(
                sampleRate,
                blockSize,
                2);

            juce::AudioBuffer<float> buffer(
                2,
                blockSize);

            fillSineWave(
                buffer,
                sampleRate,
                440.0,
                0.25f);

            processor.process(buffer);

            if (!bufferContainsOnlyFiniteSamples(buffer))
                return fail(
                    9,
                    "A sample rate produced invalid output");
        }
    }

    std::cout << "All AmpProcessor tests passed\n";
    return 0;
}