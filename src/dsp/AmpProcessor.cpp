#include "AmpProcessor.h"
#include <algorithm>
#include <cmath>

void AmpProcessor::Biquad::setNormalized(double b0In, double b1In, double b2In,
                                         double a0In, double a1In, double a2In) noexcept
{
    b0 = static_cast<float>(b0In / a0In);
    b1 = static_cast<float>(b1In / a0In);
    b2 = static_cast<float>(b2In / a0In);
    a1 = static_cast<float>(a1In / a0In);
    a2 = static_cast<float>(a2In / a0In);
}

void AmpProcessor::Biquad::setPeak(double sr, double frequency, double q, float gainDb) noexcept
{
    const auto a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const auto omega = juce::MathConstants<double>::twoPi * frequency / sr;
    const auto alpha = std::sin(omega) / (2.0 * q);
    const auto cosine = std::cos(omega);
    setNormalized(1.0 + alpha * a, -2.0 * cosine, 1.0 - alpha * a,
                  1.0 + alpha / a, -2.0 * cosine, 1.0 - alpha / a);
}

void AmpProcessor::Biquad::setLowShelf(double sr, double frequency, double q, float gainDb) noexcept
{
    const auto a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const auto omega = juce::MathConstants<double>::twoPi * frequency / sr;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0 * q);
    const auto rootAAlpha2 = 2.0 * std::sqrt(a) * alpha;
    setNormalized(a * ((a + 1.0) - (a - 1.0) * cosine + rootAAlpha2),
                  2.0 * a * ((a - 1.0) - (a + 1.0) * cosine),
                  a * ((a + 1.0) - (a - 1.0) * cosine - rootAAlpha2),
                  (a + 1.0) + (a - 1.0) * cosine + rootAAlpha2,
                  -2.0 * ((a - 1.0) + (a + 1.0) * cosine),
                  (a + 1.0) + (a - 1.0) * cosine - rootAAlpha2);
}

void AmpProcessor::Biquad::setHighShelf(double sr, double frequency, double q, float gainDb) noexcept
{
    const auto a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const auto omega = juce::MathConstants<double>::twoPi * frequency / sr;
    const auto cosine = std::cos(omega);
    const auto alpha = std::sin(omega) / (2.0 * q);
    const auto rootAAlpha2 = 2.0 * std::sqrt(a) * alpha;
    setNormalized(a * ((a + 1.0) + (a - 1.0) * cosine + rootAAlpha2),
                  -2.0 * a * ((a - 1.0) + (a + 1.0) * cosine),
                  a * ((a + 1.0) + (a - 1.0) * cosine - rootAAlpha2),
                  (a + 1.0) - (a - 1.0) * cosine + rootAAlpha2,
                  2.0 * ((a - 1.0) - (a + 1.0) * cosine),
                  (a + 1.0) - (a - 1.0) * cosine - rootAAlpha2);
}

float AmpProcessor::Biquad::process(float input) noexcept
{
    const auto output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    return output;
}

void AmpProcessor::prepare(double newSampleRate, int, int numberOfChannels)
{
    sampleRate = newSampleRate;
    activeChannels = std::clamp(numberOfChannels, 1, maxChannels);

    inputGain.reset(sampleRate, 0.02);
    outputGain.reset(sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(values.gainDb.load()));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(values.outputDb.load()));
    updateFilters();
    reset();
}

void AmpProcessor::reset()
{
    for (int channel = 0; channel < maxChannels; ++channel)
    {
        bass[static_cast<size_t>(channel)].reset();
        mid[static_cast<size_t>(channel)].reset();
        treble[static_cast<size_t>(channel)].reset();
        presence[static_cast<size_t>(channel)].reset();
    }
}

float AmpProcessor::softClip(float sample) noexcept
{
    return std::tanh(sample);
}

void AmpProcessor::updateFilters()
{
    for (int channel = 0; channel < activeChannels; ++channel)
    {
        bass[static_cast<size_t>(channel)].setLowShelf(sampleRate, 120.0, 0.707, values.bassDb.load());
        mid[static_cast<size_t>(channel)].setPeak(sampleRate, 750.0, 0.8, values.midDb.load());
        treble[static_cast<size_t>(channel)].setHighShelf(sampleRate, 3200.0, 0.707, values.trebleDb.load());
        presence[static_cast<size_t>(channel)].setPeak(sampleRate, 4500.0, 1.0, values.presenceDb.load());
    }
}

void AmpProcessor::process(juce::AudioBuffer<float>& buffer) noexcept
{
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(values.gainDb.load()));
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(values.outputDb.load()));
    updateFilters();

    const auto channels = std::min(buffer.getNumChannels(), activeChannels);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto in = inputGain.getNextValue();
        const auto out = outputGain.getNextValue();

        for (int channel = 0; channel < channels; ++channel)
        {
            auto value = buffer.getSample(channel, sample) * in;
            value = softClip(value);
            value = bass[static_cast<size_t>(channel)].process(value);
            value = mid[static_cast<size_t>(channel)].process(value);
            value = treble[static_cast<size_t>(channel)].process(value);
            value = presence[static_cast<size_t>(channel)].process(value);
            buffer.setSample(channel, sample, value * out);
        }
    }
}
