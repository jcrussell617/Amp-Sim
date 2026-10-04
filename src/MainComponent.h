#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/AmpProcessor.h"

#include <array>
#include <atomic>

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Button::Listener,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(
        const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
    };

    void configureControl(Control& control,
                          const juce::String& name,
                          double minimum,
                          double maximum,
                          double initial,
                          std::atomic<float>& destination);

    void buttonClicked(juce::Button* button) override;
    void timerCallback() override;

    static void updatePeak(std::atomic<float>& destination,
                           float newPeak) noexcept;

    void drawLevelMeter(juce::Graphics& graphics,
                        juce::Rectangle<int> bounds,
                        const juce::String& name,
                        float levelDb) const;

    AmpProcessor processor;
    std::array<Control, 6> controls;
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

    float displayedInputDb = -100.0f;
    float displayedOutputDb = -100.0f;

    juce::Rectangle<int> inputMeterBounds;
    juce::Rectangle<int> outputMeterBounds;

    juce::TextButton audioSettings { "Audio Settings" };
    juce::ToggleButton bypassButton { "Bypass" };
    juce::AudioDeviceSelectorComponent deviceSelector;

    std::atomic<bool> bypassEnabled { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};