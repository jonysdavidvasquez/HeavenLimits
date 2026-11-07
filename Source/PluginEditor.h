#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class HeavenLimitsAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit HeavenLimitsAudioProcessorEditor (HeavenLimitsAudioProcessor&);
    ~HeavenLimitsAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class VintageKnob : public juce::Slider
    {
    public:
        VintageKnob();
        void paint (juce::Graphics&) override;
    };

    class LedMeter : public juce::Component
    {
    public:
        explicit LedMeter (juce::String title) : name (std::move (title)) {}
        void setLevelDb (float db) { levelDb = juce::jlimit (-60.0f, 0.0f, db); repaint(); }
        void paint (juce::Graphics&) override;
    private:
        juce::String name;
        float levelDb = -60.0f;
    };

    class GainReductionMeter : public juce::Component
    {
    public:
        void setReduction (float db) { reduction = juce::jlimit (0.0f, 24.0f, db); repaint(); }
        void paint (juce::Graphics&) override;
    private:
        float reduction = 0.0f;
    };

    class LedToggleButton : public juce::ToggleButton
    {
    public:
        explicit LedToggleButton (const juce::String& text) : juce::ToggleButton (text) {}
        void paintButton (juce::Graphics&, bool, bool) override;
    };

    void timerCallback() override;
    void styleKnob (juce::Slider&, juce::Label&, const juce::String&, const juce::String& suffix = {});
    void styleLedButton (LedToggleButton&);

    HeavenLimitsAudioProcessor& processor;
    VintageKnob inputGain, ceiling, attack, release, lookahead, stereoLink;
    juce::Slider oversampling;
    LedToggleButton truePeak { "TRUE PEAK" }, bypass { "BYPASS" };
    LedToggleButton multiband { "MULTIBAND" }, monitor { "MONITOR" }, aes17 { "AES17" }, isp { "ISP" };
    juce::Label inputLabel, ceilingLabel, attackLabel, releaseLabel, lookaheadLabel,
                stereoLinkLabel, oversamplingLabel, rmsLabel, lufsLabel,
                multibandLabel, ditherLabel, noiseLabel;
    juce::Label inputValue, ceilingValue, attackValue, releaseValue, lookaheadValue, stereoLinkValue,
                rmsValue, lufsValue, multibandValue, ditherValue, noiseValue;
    LedMeter inputMeter { "INPUT" }, outputMeter { "OUTPUT" };
    GainReductionMeter grMeter;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> inputAttachment, ceilingAttachment, attackAttachment,
        releaseAttachment, lookaheadAttachment, stereoLinkAttachment, oversamplingAttachment,
        multibandAttachment, multibandAmountAttachment, noiseShapingAttachment;
    std::unique_ptr<ButtonAttachment> truePeakAttachment, bypassAttachment,
        multibandBoolAttachment, monitorAttachment, aes17Attachment, ispAttachment, meterResetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ChoiceAttachment> ditherAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeavenLimitsAudioProcessorEditor)
};
