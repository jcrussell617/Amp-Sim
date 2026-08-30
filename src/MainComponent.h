#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/AmpProcessor.h"
#include <array>

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Button::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
    };

    void configureControl(Control& control, const juce::String& name,
                          double minimum, double maximum, double initial,
                          std::atomic<float>& destination);
    void buttonClicked(juce::Button* button) override;

    AmpProcessor processor;
    std::array<Control, 6> controls;
    juce::TextButton audioSettings { "Audio Settings" };
    juce::AudioDeviceSelectorComponent deviceSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

