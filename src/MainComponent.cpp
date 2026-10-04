#include "MainComponent.h"

#include <algorithm>

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
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    stopTimer();
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

void MainComponent::updatePeak(
    std::atomic<float>& destination,
    float newPeak) noexcept
{
    auto currentPeak = destination.load(std::memory_order_relaxed);

    while (newPeak > currentPeak
           && !destination.compare_exchange_weak(
               currentPeak,
               newPeak,
               std::memory_order_relaxed))
    {
    }
}

void MainComponent::getNextAudioBlock(
    const juce::AudioSourceChannelInfo& info)
{
    auto* buffer = info.buffer;

    if (buffer == nullptr)
        return;

    const auto inputLevel = buffer->getMagnitude(
        0,
        info.startSample,
        info.numSamples);

    updatePeak(inputPeak, inputLevel);

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

    float outputLevel = 0.0f;

    for (int channel = 0;
         channel < buffer->getNumChannels();
         ++channel)
    {
        outputLevel = std::max(
            outputLevel,
            buffer->getMagnitude(
                channel,
                info.startSample,
                info.numSamples));
    }

    updatePeak(outputPeak, outputLevel);
}

void MainComponent::releaseResources()
{
    processor.reset();
}

void MainComponent::timerCallback()
{
    const auto newInputDb = juce::Decibels::gainToDecibels(
        inputPeak.exchange(0.0f, std::memory_order_relaxed),
        -100.0f);

    const auto newOutputDb = juce::Decibels::gainToDecibels(
        outputPeak.exchange(0.0f, std::memory_order_relaxed),
        -100.0f);

    constexpr auto decayPerFrameDb = 2.0f;

    displayedInputDb = std::max(
        newInputDb,
        displayedInputDb - decayPerFrameDb);

    displayedOutputDb = std::max(
        newOutputDb,
        displayedOutputDb - decayPerFrameDb);

    displayedInputDb = std::max(displayedInputDb, -100.0f);
    displayedOutputDb = std::max(displayedOutputDb, -100.0f);

    repaint();
}

void MainComponent::drawLevelMeter(
    juce::Graphics& graphics,
    juce::Rectangle<int> bounds,
    const juce::String& name,
    float levelDb) const
{
    if (bounds.isEmpty())
        return;

    graphics.setFont(juce::FontOptions(13.0f));

    auto labelArea = bounds.removeFromLeft(58);
    graphics.setColour(juce::Colours::whitesmoke);
    graphics.drawText(name,
                      labelArea,
                      juce::Justification::centredLeft);

    auto valueArea = bounds.removeFromRight(82);
    graphics.drawText(juce::String(levelDb, 1) + " dBFS",
                      valueArea,
                      juce::Justification::centredRight);

    auto barArea = bounds.reduced(5, 10);
    graphics.setColour(juce::Colour::fromRGB(45, 47, 52));
    graphics.fillRoundedRectangle(barArea.toFloat(), 3.0f);

    const auto normalizedLevel = juce::jlimit(
        0.0f,
        1.0f,
        (levelDb + 60.0f) / 60.0f);

    auto fillArea = barArea;
    fillArea.setWidth(
        juce::roundToInt(
            static_cast<float>(barArea.getWidth())
            * normalizedLevel));

    const auto meterColour =
        levelDb >= -3.0f
            ? juce::Colours::red
            : levelDb >= -12.0f
                ? juce::Colours::orange
                : juce::Colours::limegreen;

    graphics.setColour(meterColour);
    graphics.fillRoundedRectangle(fillArea.toFloat(), 3.0f);

    graphics.setColour(
        juce::Colours::whitesmoke.withAlpha(0.5f));

    graphics.drawRoundedRectangle(
        barArea.toFloat(),
        3.0f,
        1.0f);
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

    drawLevelMeter(
        graphics,
        inputMeterBounds,
        "Input",
        displayedInputDb);

    drawLevelMeter(
        graphics,
        outputMeterBounds,
        "Output",
        displayedOutputDb);
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

    inputMeterBounds = {};
    outputMeterBounds = {};

    if (deviceSelector.isVisible())
    {
        deviceSelector.setBounds(area.reduced(20));
        return;
    }

    auto meterArea = area.removeFromBottom(56);
    const auto meterWidth = meterArea.getWidth() / 2;

    inputMeterBounds = meterArea
        .removeFromLeft(meterWidth)
        .reduced(6);

    outputMeterBounds = meterArea.reduced(6);

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