#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
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
    float getIspPeak() const noexcept { return ispPeak.load(); }
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }
    float getRmsLevelDb() const noexcept { return rmsLevelDb.load(); }
    float getLufsLevel() const noexcept { return lufsLevel.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;
        float process (float x) noexcept;
        void reset() noexcept { z1 = z2 = 0.0; }
    };

    struct ChannelState
    {
        std::vector<float> delayLow, delayHigh;
        int writePosition = 0;
        float gainLow = 1.0f, gainHigh = 1.0f;
    };

    std::array<ChannelState, 2> channels;
    std::array<Biquad, 2> crossoverLowPass, lufsHighPass, lufsHighShelf;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling2x, oversampling4x, oversampling8x;
    double currentSampleRate = 44100.0;
    int maximumLookaheadSamples = 1;
    double rmsEnergy = 0.0, lufsEnergy = 0.0, meterSmoothing = 0.0;
    std::array<uint32_t, 2> randomState { 0x1234567u, 0x89abcdefu };
    std::array<float, 2> noiseShapingError { 0.0f, 0.0f };
    std::array<float, 2> previousOutput { 0.0f, 0.0f };

    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, ispPeak { 0.0f }, gainReductionDb { 0.0f };
    std::atomic<float> rmsLevelDb { -120.0f }, lufsLevel { -120.0f };

    void processAtRate (juce::dsp::AudioBlock<float>&, double rate, float inputGain, float ceiling,
                        float attackMs, float releaseMs, float lookaheadMs, float stereoLink,
                        float multibandAmount, bool multibandEnabled);
    void updateMeters (const juce::AudioBuffer<float>&, float inputPeakLinear) noexcept;
    void configureFilters (double sampleRate) noexcept;
    float nextRandom (size_t channel) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeavenLimitsAudioProcessor)
};
