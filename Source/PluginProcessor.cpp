#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float minusInfinityDb = -100.0f;

    float toDecibels (float value)
    {
        return value > 0.000001f ? 20.0f * std::log10 (value) : minusInfinityDb;
    }
}

HeavenLimitsAudioProcessor::HeavenLimitsAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout HeavenLimitsAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> ("input", "Input Gain",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, "dB"));
    p.push_back (std::make_unique<AudioParameterFloat> ("ceiling", "Output Ceiling",
        NormalisableRange<float> (-12.0f, 0.0f, 0.01f), -1.0f, "dB"));
    p.push_back (std::make_unique<AudioParameterFloat> ("attack", "Attack",
        NormalisableRange<float> (0.01f, 10.0f, 0.01f, 0.5f), 0.10f, "ms"));
    p.push_back (std::make_unique<AudioParameterFloat> ("release", "Release",
        NormalisableRange<float> (10.0f, 1000.0f, 0.1f, 0.5f), 100.0f, "ms"));
    p.push_back (std::make_unique<AudioParameterFloat> ("lookahead", "Lookahead",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 2.0f, "ms"));
    p.push_back (std::make_unique<AudioParameterFloat> ("stereoLink", "Stereo Link",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, "%"));
    p.push_back (std::make_unique<AudioParameterChoice> ("oversampling", "Oversampling",
        StringArray { "1x", "2x", "4x", "8x" }, 2));
    p.push_back (std::make_unique<AudioParameterBool> ("truePeak", "True Peak", true));
    p.push_back (std::make_unique<AudioParameterBool> ("bypass", "Bypass", false));

    return { p.begin(), p.end() };
}

void HeavenLimitsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maximumLookaheadSamples = juce::jmax (1, (int) std::ceil (sampleRate * 0.012 * 8.0));

    for (auto& ch : channels)
    {
        ch.delay.assign ((size_t) maximumLookaheadSamples + 2, 0.0f);
        ch.writePosition = 0;
        ch.gain = 1.0f;
    }

    auto makeOversampler = [samplesPerBlock] (unsigned int stages)
    {
        auto os = std::make_unique<juce::dsp::Oversampling<float>>(
            2, stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing ((size_t) samplesPerBlock);
        return os;
    };

    oversampling2x = makeOversampler (1);
    oversampling4x = makeOversampler (2);
    oversampling8x = makeOversampler (3);
    setLatencySamples (0);
}

void HeavenLimitsAudioProcessor::releaseResources()
{
    for (auto& ch : channels)
    {
        std::fill (ch.delay.begin(), ch.delay.end(), 0.0f);
        ch.writePosition = 0;
        ch.gain = 1.0f;
    }
}

bool HeavenLimitsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainIn == mainOut
        && (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo());
}

void HeavenLimitsAudioProcessor::processAtRate (juce::dsp::AudioBlock<float>& block, double rate,
                                                 float inputGain, float ceiling, float attackMs,
                                                 float releaseMs, float lookaheadMs, float stereoLink)
{
    const int numChannels = (int) juce::jmin ((size_t) 2, block.getNumChannels());
    const int numSamples = (int) block.getNumSamples();
    const int delaySamples = juce::jlimit (0, maximumLookaheadSamples,
        (int) std::round (lookaheadMs * 0.001 * rate));

    const float attackCoeff = std::exp (-1.0f / juce::jmax (1.0f, (float) (attackMs * 0.001 * rate)));
    const float releaseCoeff = std::exp (-1.0f / juce::jmax (1.0f, (float) (releaseMs * 0.001 * rate)));
    const float link = juce::jlimit (0.0f, 1.0f, stereoLink);
    const float ceilingLinear = juce::Decibels::decibelsToGain (ceiling);
    const float inputLinear = juce::Decibels::decibelsToGain (inputGain);

    float blockInputPeak = 0.0f;
    float blockOutputPeak = 0.0f;
    float blockMaxReduction = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        float sample[2] { 0.0f, 0.0f };
        float peak[2] { 0.0f, 0.0f };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            sample[ch] = block.getSample ((size_t) ch, (size_t) n) * inputLinear;
            peak[ch] = std::abs (sample[ch]);
            blockInputPeak = juce::jmax (blockInputPeak, peak[ch]);
        }

        float linkedPeak = peak[0];
        if (numChannels > 1)
            linkedPeak = juce::jmax (peak[0], peak[1]);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channels[(size_t) ch];
            const float detectorPeak = numChannels == 1 ? peak[ch]
                : (link * linkedPeak + (1.0f - link) * peak[ch]);

            const float targetGain = detectorPeak > ceilingLinear
                ? ceilingLinear / juce::jmax (detectorPeak, 0.000001f) : 1.0f;

            // Attack controls how quickly gain reduction is applied; release controls recovery.
            const float coeff = targetGain < state.gain ? attackCoeff : releaseCoeff;
            state.gain = targetGain + coeff * (state.gain - targetGain);

            const int bufferSize = (int) state.delay.size();
            const int readPosition = (state.writePosition - delaySamples + bufferSize) % bufferSize;
            const float delayed = state.delay[(size_t) readPosition];
            state.delay[(size_t) state.writePosition] = sample[ch];
            state.writePosition = (state.writePosition + 1) % bufferSize;

            // Zero lookahead uses the current sample. Positive lookahead outputs the delayed sample.
            const float source = delaySamples == 0 ? sample[ch] : delayed;
            const float out = source * state.gain;
            block.setSample ((size_t) ch, (size_t) n, out);
            blockOutputPeak = juce::jmax (blockOutputPeak, std::abs (out));
            blockMaxReduction = juce::jmax (blockMaxReduction, -toDecibels (state.gain));
        }
    }

    inputPeak.store (blockInputPeak);
    outputPeak.store (blockOutputPeak);
    gainReductionDb.store (blockMaxReduction);
}

void HeavenLimitsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0)
        return;

    const auto input = parameters.getRawParameterValue ("input")->load();
    const auto ceiling = parameters.getRawParameterValue ("ceiling")->load();
    const auto attack = parameters.getRawParameterValue ("attack")->load();
    const auto release = parameters.getRawParameterValue ("release")->load();
    const auto lookahead = parameters.getRawParameterValue ("lookahead")->load();
    const auto stereoLink = parameters.getRawParameterValue ("stereoLink")->load() / 100.0f;
    const auto osChoice = (int) parameters.getRawParameterValue ("oversampling")->load();
    const bool truePeak = parameters.getRawParameterValue ("truePeak")->load() > 0.5f;
    const bool bypassed = parameters.getRawParameterValue ("bypass")->load() > 0.5f;

    // Report fixed processing latency to the host: lookahead plus oversampling filter latency.
    int reportedLatency = (int) std::round (lookahead * 0.001 * currentSampleRate);
    if (truePeak && osChoice > 0)
    {
        auto* latencyOS = osChoice == 1 ? oversampling2x.get()
                       : osChoice == 2 ? oversampling4x.get() : oversampling8x.get();
        reportedLatency += (int) std::ceil (latencyOS->getLatencyInSamples());
    }
    setLatencySamples (reportedLatency);

    if (bypassed)
    {
        inputPeak.store (buffer.getMagnitude (0, numSamples));
        outputPeak.store (buffer.getMagnitude (0, numSamples));
        gainReductionDb.store (0.0f);
        return;
    }

    juce::dsp::AudioBlock<float> block (buffer);
    const float safeCeiling = juce::jmin (ceiling, 0.0f);

    if (truePeak && osChoice > 0)
    {
        auto* os = osChoice == 1 ? oversampling2x.get()
                : osChoice == 2 ? oversampling4x.get() : oversampling8x.get();

        auto upBlock = os->processSamplesUp (block);
        processAtRate (upBlock, currentSampleRate * (1 << osChoice), input, safeCeiling,
                       attack, release, lookahead, stereoLink);
        os->processSamplesDown (block);
    }
    else
    {
        processAtRate (block, currentSampleRate, input, safeCeiling,
                       attack, release, lookahead, stereoLink);
    }
}

juce::AudioProcessorEditor* HeavenLimitsAudioProcessor::createEditor()
{
    return new HeavenLimitsAudioProcessorEditor (*this);
}

void HeavenLimitsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void HeavenLimitsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeavenLimitsAudioProcessor();
}
