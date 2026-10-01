#include "MainComponent.h"

MainComponent::MainComponent()
    : deviceSelector(deviceManager, 1, 2, 1, 2,
                     false, false, true, false)
{
    auto& parameters = processor.parameters();

    configureControl(controls[0], "Gain",
                     0.0, 36.0, 12.0, parameters.gainDb);

    configureControl(controls[1], "Bass",
                     -12.0, 12.0, 0.0, parameters.bassDb);

    configureControl(controls[2], "Mid",
                     -12.0, 12.0, 0.0, parameters.midDb);

    configureControl(controls[3], "Treble",
                     -12.0, 12.0, 0.0, parameters.trebleDb);

    configureControl(controls[4], "Presence",
                     -12.0, 12.0, 0.0, parameters.presenceDb);

    configureControl(controls[5], "Output",
                     -60.0, 6.0, -12.0, parameters.outputDb);

    addAndMakeVisible(audioSettings);
    audioSettings.addListener(this);

    addAndMakeVisible(bypassButton);
    bypassButton.addListener(this);

    deviceSelector.setVisible(false);
    addChildComponent(deviceSelector);

    setSize(820, 420);
    setAudioChannels(1, 2);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::configureControl(
    Control& control,
    const juce::String& name,
    double minimum,
    double maximum,
    double initial,
    std::atomic<float>& destination)
{
    control.slider.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag);

    control.slider.setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        72,
        22);

    control.slider.setRange(minimum, maximum, 0.1);
    control.slider.setValue(initial, juce::dontSendNotification);
    control.slider.setTextValueSuffix(" dB");

    control.slider.onValueChange = [&control, &destination]
    {
        destination.store(
            static_cast<float>(control.slider.getValue()));
    };

    addAndMakeVisible(control.slider);

    control.label.setText(name, juce::dontSendNotification);
    control.label.setJustificationType(
        juce::Justification::centred);

    addAndMakeVisible(control.label);
}

void MainComponent::prepareToPlay(
    int samplesPerBlockExpected,
    double newSampleRate)
{
    processor.prepare(newSampleRate,
                      samplesPerBlockExpected,
                      2);
}

void MainComponent::getNextAudioBlock(
    const juce::AudioSourceChannelInfo& info)
{
    auto* buffer = info.buffer;

    if (buffer == nullptr)
        return;

    // Copy the mono guitar input to the second output channel.
    if (buffer->getNumChannels() > 1)
    {
        buffer->copyFrom(1,
                         info.startSample,
                         *buffer,
                         0,
                         info.startSample,
                         info.numSamples);
    }

    // When bypass is enabled, leave the copied dry signal unchanged.
    if (!bypassEnabled.load(std::memory_order_relaxed))
        processor.process(*buffer);
}

void MainComponent::releaseResources()
{
    processor.reset();
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(
        juce::Colour::fromRGB(24, 25, 28));

    graphics.setColour(juce::Colours::whitesmoke);

    graphics.setFont(
        juce::FontOptions(24.0f, juce::Font::bold));

    graphics.drawText("Amp Sim",
                      20,
                      12,
                      getWidth() - 40,
                      34,
                      juce::Justification::centredLeft);
}

void MainComponent::resized()
{
    const auto margin = 20;
    auto area = getLocalBounds().reduced(margin);
    auto header = area.removeFromTop(48);

    audioSettings.setBounds(
        header.removeFromRight(140).reduced(0, 6));

    bypassButton.setBounds(
        header.removeFromRight(100).reduced(0, 6));

    if (deviceSelector.isVisible())
    {
        deviceSelector.setBounds(area.reduced(20));
        return;
    }

    const auto controlWidth =
        area.getWidth() / static_cast<int>(controls.size());

    for (auto& control : controls)
    {
        auto column =
            area.removeFromLeft(controlWidth).reduced(6);

        control.label.setBounds(
            column.removeFromTop(28));

        control.slider.setBounds(
            column.reduced(0, 8));
    }
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &bypassButton)
    {
        bypassEnabled.store(
            bypassButton.getToggleState(),
            std::memory_order_relaxed);

        return;
    }

    if (button != &audioSettings)
        return;

    const auto showSettings = !deviceSelector.isVisible();
    deviceSelector.setVisible(showSettings);

    for (auto& control : controls)
    {
        control.slider.setVisible(!showSettings);
        control.label.setVisible(!showSettings);
    }

    audioSettings.setButtonText(
        showSettings ? "Back to Amp" : "Audio Settings");

    resized();
}