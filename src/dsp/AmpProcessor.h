#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class AmpProcessor
{
public:
    struct Parameters
    {
        std::atomic<float> gainDb { 12.0f };
        std::atomic<float> bassDb { 0.0f };
        std::atomic<float> midDb { 0.0f };
        std::atomic<float> trebleDb { 0.0f };
        std::atomic<float> presenceDb { 0.0f };
        std::atomic<float> outputDb { -12.0f };
    };

    void prepare(double newSampleRate, int maximumBlockSize, int numberOfChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    Parameters& parameters() noexcept { return values; }
    static float softClip(float sample) noexcept;

private:
    struct Biquad
    {
        void setPeak(double sampleRate, double frequency, double q, float gainDb) noexcept;
        void setLowShelf(double sampleRate, double frequency, double q, float gainDb) noexcept;
        void setHighShelf(double sampleRate, double frequency, double q, float gainDb) noexcept;
        void setLowPass(double sampleRate, double frequency, double q) noexcept;
        float process(float input) noexcept;
        void reset() noexcept { z1 = z2 = 0.0f; }

    private:
        void setNormalized(double b0In, double b1In, double b2In,
                           double a0In, double a1In, double a2In) noexcept;
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;
    };

    void updateFilters();

    static constexpr int maxChannels = 2;
    Parameters values;
    double sampleRate = 44100.0;
    int activeChannels = 1;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGain;
    // Fixed first-pass voicing. Tune these with recorded guitar comparisons.
    static constexpr double preBassFrequencyHz = 180.0;
    static constexpr float preBassCutDb = -6.0f;
    static constexpr double outputLowPassHz = 6000.0;
    std::array<Biquad, maxChannels> preBass;
    std::array<Biquad, maxChannels> outputLowPass;
    std::array<Biquad, maxChannels> bass;
    std::array<Biquad, maxChannels> mid;
    std::array<Biquad, maxChannels> treble;
    std::array<Biquad, maxChannels> presence;
};
