#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float minusInfinityDb = -100.0f;
    float toDecibels (float x) noexcept { return x > 1.0e-6f ? 20.0f * std::log10 (x) : minusInfinityDb; }

    void setHighPass (double sr, double f, double q, double& b0, double& b1, double& b2, double& a1, double& a2)
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const auto c = std::cos (w), alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
        b0 = (1.0 + c) * 0.5 / a0; b1 = -(1.0 + c) / a0; b2 = b0;
        a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
    }

    void setLowPass (double sr, double f, double q, double& b0, double& b1, double& b2, double& a1, double& a2)
    {
        const auto w = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const auto c = std::cos (w), alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
        b0 = (1.0 - c) * 0.5 / a0; b1 = (1.0 - c) / a0; b2 = b0;
        a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
    }

    void setHighShelf (double sr, double f, double q, double gainDb,
                       double& b0, double& b1, double& b2, double& a1, double& a2)
    {
        const auto a = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const auto c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * q), beta = 2.0 * std::sqrt (a) * alpha;
        const auto a0 = (a + 1.0) - (a - 1.0) * c + beta;
        b0 = a * ((a + 1.0) + (a - 1.0) * c + beta) / a0;
        b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * c) / a0;
        b2 = a * ((a + 1.0) + (a - 1.0) * c - beta) / a0;
        a1 = 2.0 * ((a - 1.0) - (a + 1.0) * c) / a0;
        a2 = ((a + 1.0) - (a - 1.0) * c - beta) / a0;
    }
}

float HeavenLimitsAudioProcessor::Biquad::process (float x) noexcept
{
    const auto y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
    return (float) y;
}

HeavenLimitsAudioProcessor::HeavenLimitsAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout HeavenLimitsAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat> ("input", "Input Gain", NormalisableRange<float> (-24, 24, 0.01f), 0.0f, "dB"));
    p.push_back (std::make_unique<AudioParameterFloat> ("ceiling", "Output Ceiling", NormalisableRange<float> (-12, 0, 0.01f), -1.0f, "dB"));
    p.push_back (std::make_unique<AudioParameterFloat> ("attack", "Attack", NormalisableRange<float> (0.01f, 10, 0.01f, 0.5f), 0.10f, "ms"));
    p.push_back (std::make_unique<AudioParameterFloat> ("release", "Release", NormalisableRange<float> (10, 1000, 0.1f, 0.5f), 100.0f, "ms"));
    p.push_back (std::make_unique<AudioParameterFloat> ("lookahead", "Lookahead", NormalisableRange<float> (0, 10, 0.01f), 2.0f, "ms"));
    p.push_back (std::make_unique<AudioParameterFloat> ("stereoLink", "Stereo Link", NormalisableRange<float> (0, 100, 1), 100.0f, "%"));
    p.push_back (std::make_unique<AudioParameterChoice> ("oversampling", "Oversampling", StringArray { "1x", "2x", "4x", "8x" }, 2));
    p.push_back (std::make_unique<AudioParameterBool> ("truePeak", "True Peak", true));
    p.push_back (std::make_unique<AudioParameterBool> ("bypass", "Bypass", false));
    p.push_back (std::make_unique<AudioParameterBool> ("multiband", "Multiband", false));
    p.push_back (std::make_unique<AudioParameterFloat> ("multibandAmount", "Multiband Amount", NormalisableRange<float> (0, 100, 1), 50.0f, "%"));
    p.push_back (std::make_unique<AudioParameterBool> ("monitor", "Monitor", false));
    p.push_back (std::make_unique<AudioParameterBool> ("aes17", "AES17 +3dB", false));
    p.push_back (std::make_unique<AudioParameterBool> ("isp", "ISP", false));
    p.push_back (std::make_unique<AudioParameterChoice> ("dither", "Dithering", StringArray { "Off", "16-bit", "24-bit" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> ("noiseShaping", "Noise Shaping", NormalisableRange<float> (0, 100, 1), 0.0f, "%"));
    p.push_back (std::make_unique<AudioParameterBool> ("meterReset", "Meter Reset", false));
    return { p.begin(), p.end() };
}

void HeavenLimitsAudioProcessor::configureFilters (double sr) noexcept
{
    for (size_t ch = 0; ch < 2; ++ch)
    {
        setLowPass (sr, 180.0, 0.70710678, crossoverLowPass[ch].b0, crossoverLowPass[ch].b1,
                    crossoverLowPass[ch].b2, crossoverLowPass[ch].a1, crossoverLowPass[ch].a2);
        setHighPass (sr, 38.135470876, 0.500327037, lufsHighPass[ch].b0, lufsHighPass[ch].b1,
                     lufsHighPass[ch].b2, lufsHighPass[ch].a1, lufsHighPass[ch].a2);
        setHighShelf (sr, 1681.97445, 0.707175237, 4.0, lufsHighShelf[ch].b0, lufsHighShelf[ch].b1,
                      lufsHighShelf[ch].b2, lufsHighShelf[ch].a1, lufsHighShelf[ch].a2);
        crossoverLowPass[ch].reset(); lufsHighPass[ch].reset(); lufsHighShelf[ch].reset();
    }
}

void HeavenLimitsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maximumLookaheadSamples = juce::jmax (1, (int) std::ceil (sampleRate * 0.012 * 8.0));
    meterSmoothing = std::exp (-1.0 / (0.300 * sampleRate));
    rmsEnergy = lufsEnergy = 0.0; configureFilters (sampleRate);
    for (auto& ch : channels)
    {
        ch.delayLow.assign ((size_t) maximumLookaheadSamples + 2, 0.0f);
        ch.delayHigh.assign ((size_t) maximumLookaheadSamples + 2, 0.0f);
        ch.writePosition = 0; ch.gainLow = ch.gainHigh = 1.0f;
    }
    auto makeOversampler = [samplesPerBlock] (unsigned int stages)
    {
        auto os = std::make_unique<juce::dsp::Oversampling<float>> (2, stages,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing ((size_t) samplesPerBlock); return os;
    };
    oversampling2x = makeOversampler (1); oversampling4x = makeOversampler (2); oversampling8x = makeOversampler (3);
    setLatencySamples (0);
}

void HeavenLimitsAudioProcessor::releaseResources()
{
    for (auto& ch : channels)
    {
        std::fill (ch.delayLow.begin(), ch.delayLow.end(), 0.0f);
        std::fill (ch.delayHigh.begin(), ch.delayHigh.end(), 0.0f);
        ch.writePosition = 0; ch.gainLow = ch.gainHigh = 1.0f;
    }
}

bool HeavenLimitsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet(); const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void HeavenLimitsAudioProcessor::processAtRate (juce::dsp::AudioBlock<float>& block, double rate,
    float inputGain, float ceiling, float attackMs, float releaseMs, float lookaheadMs, float stereoLink,
    float multibandAmount, bool multibandEnabled)
{
    const int count = (int) juce::jmin ((size_t) 2, block.getNumChannels());
    const int samples = (int) block.getNumSamples();
    const int delaySamples = juce::jlimit (0, maximumLookaheadSamples, (int) std::round (lookaheadMs * 0.001 * rate));
    const float atk = std::exp (-1.0f / juce::jmax (1.0f, (float) (attackMs * 0.001 * rate)));
    const float rel = std::exp (-1.0f / juce::jmax (1.0f, (float) (releaseMs * 0.001 * rate)));
    const float link = juce::jlimit (0.0f, 1.0f, stereoLink), ceilingLin = juce::Decibels::decibelsToGain (ceiling);
    const float inputLin = juce::Decibels::decibelsToGain (inputGain), amount = juce::jlimit (0.0f, 1.0f, multibandAmount * 0.01f);
    float maxReduction = 0.0f;

    for (int n = 0; n < samples; ++n)
    {
        float low[2] {}, high[2] {}, lowPeak[2] {}, highPeak[2] {}, fullPeak[2] {};
        for (int ch = 0; ch < count; ++ch)
        {
            const float x = block.getSample ((size_t) ch, (size_t) n) * inputLin;
            if (multibandEnabled)
            {
                low[ch] = crossoverLowPass[(size_t) ch].process (x);
                high[ch] = x - low[ch];
            }
            else { low[ch] = x; high[ch] = 0.0f; }
            lowPeak[ch] = std::abs (low[ch]); highPeak[ch] = std::abs (high[ch]); fullPeak[ch] = std::abs (x);
        }
        const float stereoLow = count > 1 ? juce::jmax (lowPeak[0], lowPeak[1]) : lowPeak[0];
        const float stereoHigh = count > 1 ? juce::jmax (highPeak[0], highPeak[1]) : highPeak[0];
        const float stereoFull = count > 1 ? juce::jmax (fullPeak[0], fullPeak[1]) : fullPeak[0];
        for (int ch = 0; ch < count; ++ch)
        {
            auto& state = channels[(size_t) ch];
            const float detLow = count == 1 ? lowPeak[ch] : link * stereoLow + (1.0f - link) * lowPeak[ch];
            const float detHigh = count == 1 ? highPeak[ch] : link * stereoHigh + (1.0f - link) * highPeak[ch];
            const float detFull = count == 1 ? fullPeak[ch] : link * stereoFull + (1.0f - link) * fullPeak[ch];
            const float commonGain = detFull > ceilingLin ? ceilingLin / juce::jmax (detFull, 1.0e-6f) : 1.0f;
            const float targetLow = detLow > ceilingLin ? ceilingLin / juce::jmax (detLow, 1.0e-6f) : 1.0f;
            const float targetHigh = detHigh > ceilingLin ? ceilingLin / juce::jmax (detHigh, 1.0e-6f) : 1.0f;
            const float desiredLow = commonGain + amount * (targetLow - commonGain);
            const float desiredHigh = commonGain + amount * (targetHigh - commonGain);
            state.gainLow = desiredLow + (desiredLow < state.gainLow ? atk : rel) * (state.gainLow - desiredLow);
            state.gainHigh = desiredHigh + (desiredHigh < state.gainHigh ? atk : rel) * (state.gainHigh - desiredHigh);

            const int size = (int) state.delayLow.size();
            const int read = (state.writePosition - delaySamples + size) % size;
            const float outLow = delaySamples == 0 ? low[ch] : state.delayLow[(size_t) read];
            const float outHigh = delaySamples == 0 ? high[ch] : state.delayHigh[(size_t) read];
            state.delayLow[(size_t) state.writePosition] = low[ch];
            state.delayHigh[(size_t) state.writePosition] = high[ch];
            state.writePosition = (state.writePosition + 1) % size;
            float out = outLow * state.gainLow + outHigh * state.gainHigh;
            if (std::abs (out) > ceilingLin) out = std::copysign (ceilingLin, out);
            block.setSample ((size_t) ch, (size_t) n, out);
            maxReduction = juce::jmax (maxReduction, -toDecibels (juce::jmin (state.gainLow, state.gainHigh)));
        }
    }
    gainReductionDb.store (maxReduction);
}

float HeavenLimitsAudioProcessor::nextRandom (size_t ch) noexcept
{
    auto& x = randomState[ch]; x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return (float) (x & 0x00ffffffu) / 16777216.0f;
}

void HeavenLimitsAudioProcessor::updateMeters (const juce::AudioBuffer<float>& buffer, float inPeak) noexcept
{
    const int count = juce::jmin (2, buffer.getNumChannels()), samples = buffer.getNumSamples();
    float peak = 0.0f, isp = 0.0f;
    for (int ch = 0; ch < count; ++ch)
    {
        float prev = previousOutput[(size_t) ch];
        for (int n = 0; n < samples; ++n)
        {
            const float x = buffer.getSample (ch, n);
            peak = juce::jmax (peak, std::abs (x));
            // Catmull-Rom interpolation estimates inter-sample peaks for the ISP display only.
            const float next = n + 1 < samples ? buffer.getSample (ch, n + 1) : x;
            const float before = n > 0 ? buffer.getSample (ch, n - 1) : prev;
            for (int k = 1; k < 4; ++k)
            {
                const float t = (float) k * 0.25f, t2 = t * t, t3 = t2 * t;
                const float y = 0.5f * ((2.0f * x) + (-before + next) * t
                    + (2.0f * before - 5.0f * x + 4.0f * next - (n + 2 < samples ? buffer.getSample (ch, n + 2) : next)) * t2
                    + (-before + 3.0f * x - 3.0f * next + (n + 2 < samples ? buffer.getSample (ch, n + 2) : next)) * t3);
                isp = juce::jmax (isp, std::abs (y));
            }
            const float weighted = lufsHighShelf[(size_t) ch].process (lufsHighPass[(size_t) ch].process (x));
            const double rmsSq = ((double) x * x) / juce::jmax (1, count);
            const double lufsSq = ((double) weighted * weighted) / juce::jmax (1, count);
            rmsEnergy = meterSmoothing * rmsEnergy + (1.0 - meterSmoothing) * rmsSq;
            lufsEnergy = meterSmoothing * lufsEnergy + (1.0 - meterSmoothing) * lufsSq;
            prev = x;
        }
        previousOutput[(size_t) ch] = prev;
    }
    inputPeak.store (inPeak); outputPeak.store (peak); ispPeak.store (juce::jmax (peak, isp));
    rmsLevelDb.store ((float) (10.0 * std::log10 (juce::jmax (rmsEnergy, 1.0e-12))));
    lufsLevel.store ((float) (-0.691 + 10.0 * std::log10 (juce::jmax (lufsEnergy, 1.0e-12))));
}

void HeavenLimitsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int samples = buffer.getNumSamples(), count = buffer.getNumChannels();
    if (samples <= 0 || count <= 0) return;
    float rawPeak = 0.0f;
    for (int ch = 0; ch < juce::jmin (2, count); ++ch) rawPeak = juce::jmax (rawPeak, buffer.getMagnitude (ch, 0, samples));

    const auto input = parameters.getRawParameterValue ("input")->load();
    const auto ceiling = juce::jmin (parameters.getRawParameterValue ("ceiling")->load(), 0.0f);
    const auto attack = parameters.getRawParameterValue ("attack")->load();
    const auto release = parameters.getRawParameterValue ("release")->load();
    const auto lookahead = parameters.getRawParameterValue ("lookahead")->load();
    const auto stereoLink = parameters.getRawParameterValue ("stereoLink")->load() * 0.01f;
    const int osChoice = (int) parameters.getRawParameterValue ("oversampling")->load();
    const bool truePeak = parameters.getRawParameterValue ("truePeak")->load() > 0.5f;
    const bool bypass = parameters.getRawParameterValue ("bypass")->load() > 0.5f;
    const bool monitor = parameters.getRawParameterValue ("monitor")->load() > 0.5f;
    const bool multiband = parameters.getRawParameterValue ("multiband")->load() > 0.5f;
    const float multibandAmount = parameters.getRawParameterValue ("multibandAmount")->load();
    const int ditherMode = (int) parameters.getRawParameterValue ("dither")->load();
    const float shaping = parameters.getRawParameterValue ("noiseShaping")->load() * 0.01f;
    const bool resetMeters = parameters.getRawParameterValue ("meterReset")->load() > 0.5f;

    if (resetMeters) { rmsEnergy = lufsEnergy = 0.0; inputPeak.store (0); outputPeak.store (0); ispPeak.store (0); gainReductionDb.store (0); }
    int latency = (bypass || monitor) ? 0 : (int) std::round (lookahead * 0.001 * currentSampleRate);
    if (!bypass && !monitor && truePeak && osChoice > 0)
    {
        auto* os = osChoice == 1 ? oversampling2x.get() : osChoice == 2 ? oversampling4x.get() : oversampling8x.get();
        latency += (int) std::ceil (os->getLatencyInSamples());
    }
    setLatencySamples (latency);

    if (!bypass && !monitor)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        if (truePeak && osChoice > 0)
        {
            auto* os = osChoice == 1 ? oversampling2x.get() : osChoice == 2 ? oversampling4x.get() : oversampling8x.get();
            auto up = os->processSamplesUp (block);
            processAtRate (up, currentSampleRate * (1 << osChoice), input, ceiling, attack, release, lookahead,
                           stereoLink, multibandAmount, multiband);
            os->processSamplesDown (block);
        }
        else processAtRate (block, currentSampleRate, input, ceiling, attack, release, lookahead,
                            stereoLink, multibandAmount, multiband);

        // Optional TPDF dither + quantization for integer export, followed by a hard ceiling guard.
        if (ditherMode > 0)
        {
            const int bits = ditherMode == 1 ? 16 : 24;
            const float step = 2.0f / (float) ((1u << (bits - 1)) - 1u);
            for (int ch = 0; ch < juce::jmin (2, count); ++ch)
                for (int n = 0; n < samples; ++n)
                {
                    const size_t c = (size_t) ch;
                    const float x = buffer.getSample (ch, n) + noiseShapingError[c] * shaping;
                    const float tpdf = (nextRandom (c) - nextRandom (c)) * step;
                    const float quantized = std::round ((x + tpdf) / step) * step;
                    noiseShapingError[c] = x - quantized;
                    buffer.setSample (ch, n, juce::jlimit (-juce::Decibels::decibelsToGain (ceiling),
                                                           juce::Decibels::decibelsToGain (ceiling), quantized));
                }
        }
        const float ceilingLin = juce::Decibels::decibelsToGain (ceiling);
        for (int ch = 0; ch < juce::jmin (2, count); ++ch)
            for (int n = 0; n < samples; ++n)
            {
                const float x = buffer.getSample (ch, n);
                if (std::abs (x) > ceilingLin) buffer.setSample (ch, n, std::copysign (ceilingLin, x));
            }
    }
    else gainReductionDb.store (0.0f);

    // AES17 and ISP affect only how the output meter is displayed, never the audio samples.
    updateMeters (buffer, rawPeak);
}

juce::AudioProcessorEditor* HeavenLimitsAudioProcessor::createEditor() { return new HeavenLimitsAudioProcessorEditor (*this); }
void HeavenLimitsAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = parameters.copyState(); std::unique_ptr<juce::XmlElement> xml (state.createXml()); copyXmlToBinary (*xml, dest);
}
void HeavenLimitsAudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType())) parameters.replaceState (juce::ValueTree::fromXml (*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HeavenLimitsAudioProcessor(); }
