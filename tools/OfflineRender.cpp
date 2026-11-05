#include "dsp/AmpProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
constexpr int blockSize = 512;
constexpr double pi = 3.14159265358979323846;

struct Levels
{
    double peak = 0.0, sumSquares = 0.0;
    std::uint64_t sampleCount = 0, fullScaleSamples = 0;

    void add(const juce::AudioBuffer<float>& buffer)
    {
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const double value = buffer.getSample(c, i);
                if (!std::isfinite(value))
                    throw std::runtime_error("Audio contains NaN or infinity.");
                peak = std::max(peak, std::abs(value));
                sumSquares += value * value;
                ++sampleCount;
                if (std::abs(value) >= 1.0)
                    ++fullScaleSamples;
            }
    }

    double rms() const
    {
        return sampleCount > 0 ? std::sqrt(sumSquares / static_cast<double>(sampleCount)) : 0.0;
    }
};

juce::File absoluteFile(const char* argument)
{
    const auto path = std::filesystem::absolute(
        std::filesystem::u8path(argument)).u8string();
    return juce::File(juce::String::fromUTF8(path.c_str()));
}

std::unique_ptr<juce::AudioFormatWriter> wavWriter(
    const juce::File& file, double rate, int channels)
{
    auto stream = file.createOutputStream();
    if (!stream)
        throw std::runtime_error("Cannot create output: " + file.getFullPathName().toStdString());
    juce::WavAudioFormat format;
    // This overload is retained in JUCE 9.0.1. Ownership transfers only on success.
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(
        stream.get(), rate, static_cast<unsigned int>(channels), 24, {}, 0));
    if (!writer)
        throw std::runtime_error("Cannot create 24-bit WAV writer.");
    stream.release();
    return writer;
}

// Synthetic plucked tones/chord for functional checks, not a real guitar model.
void createSyntheticInput(const juce::File& file)
{
    constexpr double rate = 48000.0;
    constexpr int count = 8 * 48000;
    juce::AudioBuffer<float> audio(1, count);
    constexpr double notes[] = { 82.4069, 110.0, 146.8324, 0.0 };
    double peak = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const int section = i / 96000;
        const double t = static_cast<double>(i % 96000) / rate;
        const auto tone = [t](double frequency)
        {
            return std::sin(2.0 * pi * frequency * t)
                + 0.35 * std::sin(4.0 * pi * frequency * t)
                + 0.15 * std::sin(6.0 * pi * frequency * t);
        };
        const double waveform = section < 3 ? tone(notes[section])
            : (tone(110.0) + tone(164.8138) + tone(220.0)) / 3.0;
        const double attack = std::min(1.0, t / 0.008);
        const double release = std::clamp((2.0 - t) / 0.02, 0.0, 1.0);
        const double value = waveform * attack * release * std::exp(-2.0 * t);
        audio.setSample(0, i, static_cast<float>(value));
        peak = std::max(peak, std::abs(value));
    }
    const double scale = std::pow(10.0, -12.0 / 20.0) / peak;
    for (int i = 0; i < count; ++i)
        audio.setSample(0, i, static_cast<float>(audio.getSample(0, i) * scale));
    auto writer = wavWriter(file, rate, 1);
    if (!writer->writeFromAudioSampleBuffer(audio, 0, count))
        throw std::runtime_error("Could not write synthetic input.");
}

double dbfs(double amplitude)
{
    return amplitude > 0.0 ? 20.0 * std::log10(amplitude)
                           : -std::numeric_limits<double>::infinity();
}

void reportLevels(std::ostream& csv, const char* filename,
                  const std::string& gain, double rate, int channels,
                  juce::int64 frames, const Levels& levels)
{
    csv << filename << ',' << gain << ',' << rate << ',' << channels << ','
        << frames << ',' << levels.peak << ',' << dbfs(levels.peak) << ','
        << levels.rms() << ',' << dbfs(levels.rms()) << ','
        << levels.fullScaleSamples << '\n';
    std::cout << filename << ": peak " << std::fixed << std::setprecision(2)
              << dbfs(levels.peak) << " dBFS, RMS " << dbfs(levels.rms())
              << " dBFS, samples at/above full scale: " << levels.fullScaleSamples << '\n';
}
} // namespace

// Reusable entry point: reads a mono/stereo WAV and runs the SAME processor
// used by the live app. The caller must provide a new/empty output directory.
void renderOfflineWavs(const juce::File& inputFile,
                       const juce::File& outputDirectory)
{
    juce::WavAudioFormat format;
    auto stream = inputFile.createInputStream();
    if (!stream)
        throw std::runtime_error("Cannot open input WAV.");
    std::unique_ptr<juce::AudioFormatReader> reader(
        format.createReaderFor(stream.release(), true));
    if (!reader)
        throw std::runtime_error("Unsupported or corrupt WAV.");
    if (reader->numChannels < 1 || reader->numChannels > 2)
        throw std::runtime_error("Use a mono or stereo WAV (1 or 2 channels).");
    if (!std::isfinite(reader->sampleRate) || reader->sampleRate < 8000.0
        || reader->sampleRate > 192000.0 || reader->lengthInSamples <= 0)
        throw std::runtime_error("Use a nonempty WAV between 8 and 192 kHz.");

    const auto rate = reader->sampleRate;
    const auto channels = static_cast<int>(reader->numChannels);
    const auto frames = reader->lengthInSamples;
    juce::AudioBuffer<float> block(channels, blockSize);

    // Validate before rendering; do not silently turn corrupt samples into audio.
    Levels inputLevels;
    for (juce::int64 offset = 0; offset < frames; offset += blockSize)
    {
        const int length = static_cast<int>(std::min<juce::int64>(blockSize, frames - offset));
        block.setSize(channels, length, false, false, true);
        if (!reader->read(&block, 0, length, offset, true, channels == 2))
            throw std::runtime_error("WAV read failed.");
        inputLevels.add(block);
    }

    const auto csvFile = outputDirectory.getChildFile("levels.csv");
    std::ofstream csv(std::filesystem::u8path(csvFile.getFullPathName().toStdString()));
    if (!csv)
        throw std::runtime_error("Cannot create levels.csv.");
    csv << std::setprecision(10)
        << "file,gain_db,sample_rate_hz,channels,frames,peak_linear,peak_dbfs,"
           "rms_linear,rms_dbfs,samples_at_or_above_full_scale\n";
    reportLevels(csv, "input", "dry", rate, channels, frames, inputLevels);

    constexpr float gains[] = { 0.0f, 12.0f, 24.0f, 36.0f };
    for (int pass = -1; pass < 4; ++pass)
    {
        const bool dry = pass < 0;
        const auto name = dry ? std::string("dry.wav")
            : "gain_" + std::to_string(static_cast<int>(gains[pass])) + "dB.wav";
        auto writer = wavWriter(outputDirectory.getChildFile(name.c_str()), rate, channels);
        AmpProcessor processor;
        auto& parameters = processor.parameters();
        parameters.gainDb.store(dry ? 0.0f : gains[pass]);
        parameters.bassDb.store(0.0f);
        parameters.midDb.store(0.0f);
        parameters.trebleDb.store(0.0f);
        parameters.presenceDb.store(0.0f);
        parameters.outputDb.store(-12.0f);
        processor.prepare(rate, blockSize, channels);
        Levels outputLevels;
        for (juce::int64 offset = 0; offset < frames; offset += blockSize)
        {
            const int length = static_cast<int>(std::min<juce::int64>(blockSize, frames - offset));
            block.setSize(channels, length, false, false, true);
            if (!reader->read(&block, 0, length, offset, true, channels == 2))
                throw std::runtime_error("WAV read failed during render.");
            if (!dry)
                processor.process(block);
            outputLevels.add(block); // Measure FLOAT signal before PCM encoding.
            if (!writer->writeFromAudioSampleBuffer(block, 0, length))
                throw std::runtime_error("WAV write failed.");
        }
        reportLevels(csv, name.c_str(), dry ? "dry" : std::to_string(gains[pass]),
                     rate, channels, frames, outputLevels);
        if (outputLevels.fullScaleSamples > 0)
            std::cerr << "WARNING: " << name
                      << " reaches/exceeds full scale; PCM export may clip.\n";
    }
    csv.flush();
    if (!csv)
        throw std::runtime_error("Could not finish levels.csv.");
}

int main(int argc, char* argv[])
{
    if (argc == 2 && std::string(argv[1]) == "--help")
    {
        std::cout << "AmpSimOffline --synthetic NEW_OUTPUT_FOLDER\n"
                     "AmpSimOffline --input DRY_GUITAR.wav NEW_OUTPUT_FOLDER\n";
        return 0;
    }
    const bool synthetic = argc == 3 && std::string(argv[1]) == "--synthetic";
    const bool inputMode = argc == 4 && std::string(argv[1]) == "--input";
    if (!synthetic && !inputMode)
    {
        std::cerr << "Use --help for syntax.\n";
        return 2;
    }
    try
    {
        const auto output = absoluteFile(argv[synthetic ? 2 : 3]);
        const auto outputPath = std::filesystem::u8path(output.getFullPathName().toStdString());
        // Protect recordings and prior test results from accidental replacement.
        if (std::filesystem::exists(outputPath)
            && (!std::filesystem::is_directory(outputPath)
                || !std::filesystem::is_empty(outputPath)))
            throw std::runtime_error("Output folder must be new or empty. Choose another name.");
        auto input = synthetic ? output.getChildFile("synthetic_input.wav")
                               : absoluteFile(argv[2]);
        if (!synthetic && !input.existsAsFile())
            throw std::runtime_error("Input WAV does not exist.");
        if (!output.createDirectory().wasOk())
            throw std::runtime_error("Cannot create output folder.");
        if (synthetic)
            createSyntheticInput(input);
        std::cout << "Neutral EQ; processed output -12 dB; no loudness matching.\n";
        renderOfflineWavs(input, output);
        std::cout << "Saved WAVs and levels.csv to "
                  << output.getFullPathName().toStdString() << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what()
                  << "\nIf files were partly written, use a fresh output folder for the retry.\n";
        return 1;
    }
}
