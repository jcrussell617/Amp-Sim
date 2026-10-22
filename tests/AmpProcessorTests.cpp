#include "dsp/AmpProcessor.h"

#include <algorithm>
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

// Coherent one-second signals avoid FFT/window leakage at both sample rates.
// Each case uses a new processor and a full second of warmup. Parameters are
// set before prepare(), so gain smoothing does not contaminate measurements.
juce::AudioBuffer<float> renderSignal(double sampleRate, float inputDb,
                                    float gainDb, float outputDb,
                                    bool chord = false)
{
    const auto count = static_cast<int>(sampleRate);
    juce::AudioBuffer<float> result(2, count);
    juce::AudioBuffer<float> block(2, blockSize);
    AmpProcessor processor;
    auto& p = processor.parameters();
    p.gainDb.store(gainDb);
    p.outputDb.store(outputDb);
    p.bassDb.store(0.0f);
    p.midDb.store(0.0f);
    p.trebleDb.store(0.0f);
    p.presenceDb.store(0.0f);
    processor.prepare(sampleRate, blockSize, 2);
    const auto amplitude = std::pow(10.0, inputDb / 20.0);

    // First second warms up the processor; second second is measured.
    for (int offset = 0; offset < count * 2;)
    {
        const int length = std::min(blockSize, count * 2 - offset);
        block.setSize(2, length, false, false, true);
        for (int i = 0; i < length; ++i)
        {
            const double t = static_cast<double>(offset + i) / sampleRate;
            double value = std::sin(2.0 * pi * 200.0 * t);
            if (chord)
            {
                // Synthetic guitar-range triad; not a guitar-tone model.
                value = (std::sin(2.0 * pi * 110.0 * t)
                       + std::sin(2.0 * pi * 165.0 * t)
                       + std::sin(2.0 * pi * 220.0 * t)) / 3.0;
            }
            for (int channel = 0; channel < 2; ++channel)
                block.setSample(channel, i, static_cast<float>(amplitude * value));
        }
        processor.process(block);
        for (int i = 0; i < length; ++i)
            if (offset + i >= count)
                for (int channel = 0; channel < 2; ++channel)
                    result.setSample(channel, offset + i - count,
                                     block.getSample(channel, i));
        offset += length;
    }
    return result;
}

double harmonicAmplitude(const juce::AudioBuffer<float>& buffer,
                         double sampleRate, int harmonic)
{
    double sine = 0.0, cosine = 0.0;
    const int count = buffer.getNumSamples();
    for (int i = 0; i < count; ++i)
    {
        const double phase = 2.0 * pi * 200.0 * harmonic * i / sampleRate;
        const double sample = buffer.getSample(0, i);
        sine += sample * std::sin(phase);
        cosine += sample * std::cos(phase);
    }
    return 2.0 * std::hypot(sine, cosine) / count;
}

double measuredThd(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const double fundamental = harmonicAmplitude(buffer, sampleRate, 1);
    // Silence or invalid output must fail, rather than appear perfectly clean.
    if (!std::isfinite(fundamental) || fundamental < 1.0e-10)
        return -1.0;
    double power = 0.0;
    for (int harmonic = 2; harmonic <= 9; ++harmonic)
    {
        const double amplitude = harmonicAmplitude(buffer, sampleRate, harmonic);
        power += amplitude * amplitude;
    }
    return std::sqrt(power) / fundamental;
}

bool runDistortionTests()
{
    bool passed = true;
    const auto check = [&passed](bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            passed = false;
        }
    };

    for (const double sampleRate : { 44100.0, 48000.0 })
    {
        std::cout << "\nDistortion measurements at " << sampleRate << " Hz\n";
        // Peak levels of the generated input, not RMS levels.
        double weakThd = 0.0, strongThd = 0.0;
        for (const float inputDb : { -60.0f, -40.0f, -20.0f })
        {
            auto audio = renderSignal(sampleRate, inputDb, 36.0f, -12.0f);
            const double thd = measuredThd(audio, sampleRate);
            std::cout << "Input " << inputDb << " dBFS peak, gain 36 dB: "
                      << 100.0 * thd << "% THD (harmonics 2-9)\n";
            check(bufferContainsOnlyFiniteSamples(audio),
                  "Input-level sweep produced invalid output");
            check(thd >= 0.0 && std::isfinite(thd),
                  "Input-level sweep has no measurable fundamental");
            if (inputDb == -60.0f) weakThd = thd;
            if (inputDb == -20.0f) strongThd = thd;
        }
        // Baseline expectations for the current single tanh stage.
        // Deliberate changes to drive architecture may require updating these.
        check(weakThd >= 0.0 && weakThd < 0.01,
              "Very weak input should remain nearly clean at current max gain");
        check(strongThd > 0.20,
              "Max gain did not produce substantial distortion on -20 dBFS input");

        double previousThd = -1.0;
        for (const float gainDb : { 0.0f, 12.0f, 24.0f, 36.0f })
        {
            auto audio = renderSignal(sampleRate, -20.0f, gainDb, -12.0f);
            const double thd = measuredThd(audio, sampleRate);
            std::cout << "Gain " << gainDb << " dB, input -20 dBFS peak: "
                      << 100.0 * thd << "% THD\n";
            check(bufferContainsOnlyFiniteSamples(audio)
                      && std::isfinite(thd) && thd >= 0.0,
                  "Gain sweep produced invalid or silent output");
            check(thd > previousThd + 0.0001,
                  "Increasing gain did not increase harmonic distortion");
            previousThd = thd;
        }

        // Output volume should scale the distorted waveform without changing
        // its shape. This also catches output gain accidentally moved pre-clip.
        auto loud = renderSignal(sampleRate, -20.0f, 36.0f, 0.0f);
        auto quiet = renderSignal(sampleRate, -20.0f, 36.0f, -12.0f);
        const double scale = std::pow(10.0, -12.0 / 20.0);
        double maximumError = 0.0;
        for (int i = 0; i < loud.getNumSamples(); ++i)
            maximumError = std::max(maximumError,
                std::abs(quiet.getSample(0, i) - scale * loud.getSample(0, i)));
        check(bufferContainsOnlyFiniteSamples(loud)
                  && bufferContainsOnlyFiniteSamples(quiet)
                  && maximumError < 0.00001,
              "Output control changed the distortion waveform shape");
        check(std::abs(measuredThd(loud, sampleRate)
                     - measuredThd(quiet, sampleRate)) < 0.0001,
              "Output control changed the harmonic ratio");

        auto chord = renderSignal(sampleRate, -12.0f, 36.0f, -12.0f, true);
        check(bufferContainsOnlyFiniteSamples(chord)
                  && getAverageRms(chord) > 0.01f,
              "Synthetic chord produced invalid or silent output");
        check(channelsMatch(chord, 0.000001f),
              "Synthetic chord stereo channels differ");
        // This bound applies only to neutral EQ and -12 dB output.
        // Boosted EQ/output settings are allowed to exceed digital full scale.
        check(getMaximumMagnitude(chord) <= 0.252f,
              "Neutral-EQ chord exceeded the expected output bound");
    }
    return passed;
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

    if (!runDistortionTests())
        return 10;

    std::cout << "All AmpProcessor tests passed\n";
    return 0;
}
