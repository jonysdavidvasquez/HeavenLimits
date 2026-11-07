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
    class LedMeter : public juce::Component
    {
    public:
        LedMeter (juce::String title, bool redAtTop = true) : name (std::move (title)), redTop (redAtTop) {}
        void setLevelDb (float db) { levelDb = juce::jlimit (-60.0f, 0.0f, db); repaint(); }
        void paint (juce::Graphics&) override;
    private:
        juce::String name;
        bool redTop = true;
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

    void timerCallback() override;
    void styleKnob (juce::Slider&, juce::Label&, const juce::String&, const juce::String& suffix = {});
    void styleToggle (juce::ToggleButton&, const juce::String&);

    HeavenLimitsAudioProcessor& processor;
    juce::Slider inputGain, ceiling, attack, release, lookahead, stereoLink, oversampling;
    juce::ToggleButton truePeak, bypass;
    juce::Label inputLabel, ceilingLabel, attackLabel, releaseLabel, lookaheadLabel,
                  stereoLinkLabel, oversamplingLabel, truePeakLabel, bypassLabel;
    juce::Label inputValue, ceilingValue, attackValue, releaseValue, lookaheadValue, stereoLinkValue;
    LedMeter inputMeter { "INPUT" }, outputMeter { "OUTPUT" };
    GainReductionMeter grMeter;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> inputAttachment, ceilingAttachment, attackAttachment,
        releaseAttachment, lookaheadAttachment, stereoLinkAttachment, oversamplingAttachment;
    std::unique_ptr<ButtonAttachment> truePeakAttachment, bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeavenLimitsAudioProcessorEditor)
};
