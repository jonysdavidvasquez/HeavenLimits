#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float minusInfinityDb = -100.0f;

    float toDecibels (float value) noexcept
    {
        return value > 0.000001f ? 20.0f * std::log10 (value) : minusInfinityDb;
    }

    void setHighPass (double sampleRate, double frequency, double q,
                      double& b0, double& b1, double& b2, double& a1, double& a2)
    {
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
        const auto cosine = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * q);
        const auto a0 = 1.0 + alpha;
        b0 = ((1.0 + cosine) * 0.5) / a0;
        b1 = -(1.0 + cosine) / a0;
        b2 = b0;
        a1 = (-2.0 * cosine) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setHighShelf (double sampleRate, double frequency, double q, double gainDb,
                       double& b0, double& b1, double& b2, double& a1, double& a2)
    {
        const auto a = std::pow (10.0, gainDb / 40.0);
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
        const auto cosine = std::cos (w0);
        const auto sine = std::sin (w0);
        const auto alpha = sine / (2.0 * q);
        const auto beta = 2.0 * std::sqrt (a) * alpha;
        const auto a0 = (a + 1.0) - (a - 1.0) * cosine + beta;
        b0 = (a * ((a + 1.0) + (a - 1.0) * cosine + beta)) / a0;
        b1 = (-2.0 * a * ((a - 1.0) + (a + 1.0) * cosine)) / a0;
        b2 = (a * ((a + 1.0) + (a - 1.0) * cosine - beta)) / a0;
        a1 = (2.0 * ((a - 1.0) - (a + 1.0) * cosine)) / a0;
        a2 = ((a + 1.0) - (a - 1.0) * cosine - beta) / a0;
    }
}

float HeavenLimitsAudioProcessor::Biquad::process (float input) noexcept
{
    const auto output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    return (float) output;
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

void HeavenLimitsAudioProcessor::configureLufsFilters (double sampleRate) noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        setHighPass (sampleRate, 38.13547087602444, 0.5003270373238773,
                     lufsHighPass[(size_t) ch].b0, lufsHighPass[(size_t) ch].b1,
                     lufsHighPass[(size_t) ch].b2, lufsHighPass[(size_t) ch].a1,
                     lufsHighPass[(size_t) ch].a2);
        setHighShelf (sampleRate, 1681.974450955533, 0.7071752369554196, 4.0,
                      lufsHighShelf[(size_t) ch].b0, lufsHighShelf[(size_t) ch].b1,
                      lufsHighShelf[(size_t) ch].b2, lufsHighShelf[(size_t) ch].a1,
                      lufsHighShelf[(size_t) ch].a2);
        lufsHighPass[(size_t) ch].reset();
        lufsHighShelf[(size_t) ch].reset();
    }
}

void HeavenLimitsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maximumLookaheadSamples = juce::jmax (1, (int) std::ceil (sampleRate * 0.012 * 8.0));
    meterSmoothing = std::exp (-1.0 / (0.300 * sampleRate));
    rmsEnergy = lufsEnergy = 0.0;
    configureLufsFilters (sampleRate);

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
    float blockMaxReduction = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        float sample[2] { 0.0f, 0.0f };
        float peak[2] { 0.0f, 0.0f };
        for (int ch = 0; ch < numChannels; ++ch)
        {
            sample[ch] = block.getSample ((size_t) ch, (size_t) n) * inputLinear;
            peak[ch] = std::abs (sample[ch]);
        }

        const float linkedPeak = numChannels > 1 ? juce::jmax (peak[0], peak[1]) : peak[0];
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = channels[(size_t) ch];
            const float detectorPeak = numChannels == 1 ? peak[ch]
                : link * linkedPeak + (1.0f - link) * peak[ch];
            const float targetGain = detectorPeak > ceilingLinear
                ? ceilingLinear / juce::jmax (detectorPeak, 0.000001f) : 1.0f;
            const float coeff = targetGain < state.gain ? attackCoeff : releaseCoeff;
            state.gain = targetGain + coeff * (state.gain - targetGain);

            const int bufferSize = (int) state.delay.size();
            const int readPosition = (state.writePosition - delaySamples + bufferSize) % bufferSize;
            const float delayed = state.delay[(size_t) readPosition];
            state.delay[(size_t) state.writePosition] = sample[ch];
            state.writePosition = (state.writePosition + 1) % bufferSize;

            const float source = delaySamples == 0 ? sample[ch] : delayed;
            float out = source * state.gain;
            if (std::abs (out) > ceilingLinear)
                out = std::copysign (ceilingLinear, out);
            block.setSample ((size_t) ch, (size_t) n, out);
            blockMaxReduction = juce::jmax (blockMaxReduction, -toDecibels (state.gain));
        }
    }

    gainReductionDb.store (blockMaxReduction);
}

void HeavenLimitsAudioProcessor::updateMeters (const juce::AudioBuffer<float>& buffer,
                                               float inputPeakLinear) noexcept
{
    const int numChannels = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    double rmsAccumulator = 0.0;
    double lufsAccumulator = 0.0;
    float peak = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        double sampleSquares = 0.0;
        double weightedSquares = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float sample = buffer.getSample (ch, n);
            peak = juce::jmax (peak, std::abs (sample));
            sampleSquares += (double) sample * sample;
            const float weighted = lufsHighShelf[(size_t) ch].process (
                lufsHighPass[(size_t) ch].process (sample));
            weightedSquares += (double) weighted * weighted;
        }

        const double channelCount = (double) juce::jmax (1, numChannels);
        const double sampleMeanSquare = sampleSquares / channelCount;
        rmsAccumulator = meterSmoothing * rmsAccumulator + (1.0 - meterSmoothing) * sampleMeanSquare;
        lufsAccumulator = meterSmoothing * lufsAccumulator + (1.0 - meterSmoothing) * weightedSquares;
    }

    inputPeak.store (inputPeakLinear);
    outputPeak.store (peak);
    if (numSamples > 0)
    {
        rmsLevelDb.store ((float) (10.0 * std::log10 (juce::jmax (rmsAccumulator, 1.0e-12))));
        lufsLevel.store ((float) (-0.691 + 10.0 * std::log10 (juce::jmax (lufsAccumulator, 1.0e-12))));
    }
}

void HeavenLimitsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0)
        return;

    float rawInputPeak = 0.0f;
    for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
        rawInputPeak = juce::jmax (rawInputPeak, buffer.getMagnitude (ch, 0, numSamples));

    const auto input = parameters.getRawParameterValue ("input")->load();
    const auto ceiling = parameters.getRawParameterValue ("ceiling")->load();
    const auto attack = parameters.getRawParameterValue ("attack")->load();
    const auto release = parameters.getRawParameterValue ("release")->load();
    const auto lookahead = parameters.getRawParameterValue ("lookahead")->load();
    const auto stereoLink = parameters.getRawParameterValue ("stereoLink")->load() / 100.0f;
    const auto osChoice = (int) parameters.getRawParameterValue ("oversampling")->load();
    const bool truePeak = parameters.getRawParameterValue ("truePeak")->load() > 0.5f;
    const bool bypassed = parameters.getRawParameterValue ("bypass")->load() > 0.5f;

    int reportedLatency = bypassed ? 0 : (int) std::round (lookahead * 0.001 * currentSampleRate);
    if (! bypassed && truePeak && osChoice > 0)
    {
        auto* latencyOS = osChoice == 1 ? oversampling2x.get()
                       : osChoice == 2 ? oversampling4x.get() : oversampling8x.get();
        reportedLatency += (int) std::ceil (latencyOS->getLatencyInSamples());
    }
    setLatencySamples (reportedLatency);

    if (! bypassed)
    {
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

        const float ceilingLinear = juce::Decibels::decibelsToGain (safeCeiling);
        for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
            for (int n = 0; n < numSamples; ++n)
            {
                const float sample = buffer.getSample (ch, n);
                if (std::abs (sample) > ceilingLinear)
                    buffer.setSample (ch, n, std::copysign (ceilingLinear, sample));
            }
    }
    else
    {
        gainReductionDb.store (0.0f);
    }

    updateMeters (buffer, rawInputPeak);
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
