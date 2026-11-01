#include "MainComponent.h"

#include <algorithm>
#include <cmath>

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

    inputSelector.addItem("Input 1", 1);
    inputSelector.addItem("Input 2", 2);
    inputSelector.setSelectedId(1, juce::dontSendNotification);
    inputSelector.onChange = [this]
    {
        selectedInputChannel.store(inputSelector.getSelectedId() - 1,
                                   std::memory_order_relaxed);
    };
    addAndMakeVisible(inputSelector);

    deviceSelector.setVisible(false);
    addChildComponent(deviceSelector);

    latencyLogFile = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("AmpSimCapstone").getChildFile("latency.csv");
    latencyLabel.setFont(juce::FontOptions(13.0f));
    latencyLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
    latencyLabel.setJustificationType(juce::Justification::centredLeft);
    latencyLabel.setTooltip("Driver-reported latency; not a measured round trip. Log: "
                            + latencyLogFile.getFullPathName());
    addAndMakeVisible(latencyLabel);
    logLatencyButton.onClick = [this] { updateLatencyInfo(true); };
    logLatencyButton.setTooltip("Append the current device settings and reported latency to "
                                + latencyLogFile.getFullPathName());
    addAndMakeVisible(logLatencyButton);

    setSize(820, 510);
    setAudioChannels(2, 2);
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

    if (buffer == nullptr || info.numSamples <= 0 || buffer->getNumChannels() == 0)
        return;

    const auto sourceChannel =
        selectedInputChannel.load(std::memory_order_relaxed);

    if (sourceChannel >= buffer->getNumChannels())
    {
        info.clearActiveBufferRegion();
        updatePeak(inputPeak, 0.0f);
        updatePeak(outputPeak, 0.0f);
        return;
    }

    const auto inputLevel = buffer->getMagnitude(
        sourceChannel,
        info.startSample,
        info.numSamples);

    updatePeak(inputPeak, inputLevel);

    // Route the selected input to output channel 0.
    if (sourceChannel != 0)
        buffer->copyFrom(0, info.startSample, *buffer,
                         sourceChannel, info.startSample, info.numSamples);

    // Copy that same signal to output channel 1.
    if (buffer->getNumChannels() > 1)
    {
        buffer->copyFrom(1,
                         info.startSample,
                         *buffer,
                         0,
                         info.startSample,
                         info.numSamples);
    }

    // This view references only the samples assigned to this callback.
    juce::AudioBuffer<float> activeBuffer(
        buffer->getArrayOfWritePointers(),
        buffer->getNumChannels(),
        info.startSample,
        info.numSamples);

    // When bypass is enabled, leave the copied dry signal unchanged.
    if (!bypassEnabled.load(std::memory_order_relaxed))
        processor.process(activeBuffer);

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
    // Query and write on the message thread, never in the audio callback.
    if (--latencyPollCountdown <= 0)
    {
        latencyPollCountdown = 15; // About twice per second.
        updateLatencyInfo();
    }

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
    auto footer = area.removeFromBottom(86);
    logLatencyButton.setBounds(footer.removeFromRight(116).reduced(2, 24));
    latencyLabel.setBounds(footer.reduced(0, 3));
    auto header = area.removeFromTop(48);

    audioSettings.setBounds(
        header.removeFromRight(140).reduced(0, 6));

    bypassButton.setBounds(
        header.removeFromRight(100).reduced(0, 6));

    inputSelector.setBounds(
        header.removeFromRight(130).reduced(4, 6));

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
    inputSelector.setVisible(!showSettings);

    for (auto& control : controls)
    {
        control.slider.setVisible(!showSettings);
        control.label.setVisible(!showSettings);
    }

    audioSettings.setButtonText(
        showSettings ? "Back to Amp" : "Audio Settings");

    resized();
}


void MainComponent::updateLatencyInfo(bool forceLog)
{
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr || !device->isOpen() || !device->isPlaying())
    {
        latencyLabel.setText("Audio device inactive - latency unavailable",
                             juce::dontSendNotification);
        logLatencyButton.setEnabled(false);
        lastLatencyConfiguration.clear();
        latencyLogStatus.clear();
        return;
    }

    const double rate = device->getCurrentSampleRate();
    const int bufferSize = device->getCurrentBufferSizeSamples();
    const int inputSamples = device->getInputLatencyInSamples();
    const int outputSamples = device->getOutputLatencyInSamples();
    if (rate <= 0.0 || !std::isfinite(rate))
    {
        latencyLabel.setText("Audio device starting - latency unavailable",
                             juce::dontSendNotification);
        logLatencyButton.setEnabled(false);
        lastLatencyConfiguration.clear();
        return;
    }
    logLatencyButton.setEnabled(true);

    const auto milliseconds = [rate](int samples)
    {
        return samples >= 0 ? juce::String(1000.0 * samples / rate, 2)
                            : juce::String();
    };
    const auto inputMs = milliseconds(inputSamples);
    const auto outputMs = milliseconds(outputSamples);
    const auto totalMs = inputSamples >= 0 && outputSamples >= 0
        ? juce::String(1000.0 * (static_cast<double>(inputSamples)
                               + outputSamples) / rate, 2)
        : juce::String();
    const auto displayMs = [](const juce::String& value)
    {
        return value.isEmpty() ? juce::String("unavailable") : value + " ms";
    };
    // Quote every text field so commas, quotes and line breaks remain valid CSV.
    const auto csvText = [](juce::String value)
    {
        return juce::String("\"") + value.replace("\"", "\"\"") + "\"";
    };
    const auto configuration = csvText(device->getName()) + ","
        + csvText(device->getTypeName()) + "," + juce::String(rate, 3) + ","
        + juce::String(bufferSize) + "," + juce::String(inputSamples) + ","
        + juce::String(outputSamples);

    if (forceLog || configuration != lastLatencyConfiguration)
    {
        const auto reason = forceLog ? "manual" : "device_configuration";
        juce::String row;
        if (!latencyLogFile.existsAsFile() || latencyLogFile.getSize() == 0)
            row = "timestamp_local,device,driver,sample_rate_hz,buffer_samples,"
                  "input_latency_samples,output_latency_samples,input_latency_ms,"
                  "output_latency_ms,combined_reported_latency_ms,reason,measurement_type\n";
        row += csvText(juce::Time::getCurrentTime().toISO8601(true)) + ","
            + configuration + "," + inputMs + "," + outputMs + "," + totalMs
            + "," + reason + ",driver_reported\n";
        const bool saved = latencyLogFile.getParentDirectory().createDirectory().wasOk()
                        && latencyLogFile.appendText(row);
        latencyLogStatus = saved ? "CSV saved " + juce::Time::getCurrentTime().formatted("%H:%M:%S")
                                : "CSV save failed - check folder permissions";
        // Avoid retrying disk writes every poll after a failure. The button retries.
        lastLatencyConfiguration = configuration;
    }

    latencyLabel.setText(device->getName() + " | " + device->getTypeName() + " | "
        + juce::String(rate, 0) + " Hz | " + juce::String(bufferSize) + " samples\n"
        + "Reported input: " + displayMs(inputMs) + " | output: " + displayMs(outputMs)
        + " | combined: " + displayMs(totalMs) + "\n"
        + "Driver-reported; not measured round trip. " + latencyLogStatus,
        juce::dontSendNotification);
}
