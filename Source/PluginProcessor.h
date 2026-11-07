#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class HeavenLimitsAudioProcessor final : public juce::AudioProcessor
{
public:
    HeavenLimitsAudioProcessor();
    ~HeavenLimitsAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Heaven Limits"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

    float getInputPeak() const noexcept { return inputPeak.load(); }
    float getOutputPeak() const noexcept { return outputPeak.load(); }
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }
    float getRmsLevelDb() const noexcept { return rmsLevelDb.load(); }
    float getLufsLevel() const noexcept { return lufsLevel.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct ChannelState
    {
        std::vector<float> delay;
        int writePosition = 0;
        float gain = 1.0f;
    };

    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;
        float process (float input) noexcept;
        void reset() noexcept { z1 = z2 = 0.0; }
    };

    std::array<ChannelState, 2> channels;
    std::array<Biquad, 2> lufsHighPass, lufsHighShelf;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling2x, oversampling4x, oversampling8x;
    double currentSampleRate = 44100.0;
    int maximumLookaheadSamples = 1;
    double rmsEnergy = 0.0;
    double lufsEnergy = 0.0;
    double meterSmoothing = 0.0;

    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gainReductionDb { 0.0f };
    std::atomic<float> rmsLevelDb { -120.0f }, lufsLevel { -120.0f };

    void processAtRate (juce::dsp::AudioBlock<float>& block, double rate,
                        float inputGain, float ceiling, float attackMs,
                        float releaseMs, float lookaheadMs, float stereoLink);
    void updateMeters (const juce::AudioBuffer<float>& buffer, float inputPeakLinear) noexcept;
    void configureLufsFilters (double sampleRate) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeavenLimitsAudioProcessor)
};
