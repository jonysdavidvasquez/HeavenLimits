#include "PluginEditor.h"

namespace
{
    const juce::Colour metalTop (0xffd8e3ee);
    const juce::Colour metalDark (0xff718ba5);
    const juce::Colour ink (0xff101820);
    const juce::Colour red (0xffff3838);

    void drawScrew (juce::Graphics& g, juce::Point<float> c)
    {
        g.setColour (juce::Colour (0xff263441));
        g.fillEllipse (c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f7fa), c.x - 4, c.y - 5,
                                                  juce::Colour (0xff536575), c.x + 5, c.y + 6, false));
        g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff273744));
        g.drawLine (c.x - 3.0f, c.y + 2.0f, c.x + 3.0f, c.y - 2.0f, 1.3f);
    }

    void setupLabel (juce::Label& label, const juce::String& text, float size = 12.0f)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, ink);
        label.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
        label.setInterceptsMouseClicks (false, false);
    }
}

HeavenLimitsAudioProcessorEditor::HeavenLimitsAudioProcessorEditor (HeavenLimitsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (900, 480);

    styleKnob (inputGain, inputLabel, "INPUT", " dB");
    styleKnob (ceiling, ceilingLabel, "OUTPUT CEILING", " dB");
    styleKnob (attack, attackLabel, "ATTACK", " ms");
    styleKnob (release, releaseLabel, "RELEASE", " ms");
    styleKnob (lookahead, lookaheadLabel, "LOOKAHEAD", " ms");
    styleKnob (stereoLink, stereoLinkLabel, "STEREO LINK", "%");

    oversampling.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    oversampling.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 18);
    oversampling.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff507da7));
    oversampling.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff344b60));
    oversampling.setColour (juce::Slider::thumbColourId, juce::Colour (0xffe7f0f7));
    oversampling.setColour (juce::Slider::textBoxTextColourId, ink);
    oversampling.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x557d9bb5));
    oversampling.setNumDecimalPlacesToDisplay (0);
    setupLabel (oversamplingLabel, "OVERSAMPLING", 10.0f);
    addAndMakeVisible (oversampling);
    addAndMakeVisible (oversamplingLabel);

    styleLedButton (truePeak);
    styleLedButton (bypass);

    auto addValue = [this] (juce::Label& label)
    {
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colour (0xff263b4d));
        label.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        addAndMakeVisible (label);
    };
    for (auto* label : { &inputValue, &ceilingValue, &attackValue, &releaseValue,
                         &lookaheadValue, &stereoLinkValue, &rmsValue, &lufsValue })
        addValue (*label);

    setupLabel (rmsLabel, "RMS", 9.0f);
    setupLabel (lufsLabel, "LUFS (K-weighted)", 9.0f);
    addAndMakeVisible (rmsLabel);
    addAndMakeVisible (lufsLabel);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (grMeter);

    inputAttachment = std::make_unique<SliderAttachment> (processor.parameters, "input", inputGain);
    ceilingAttachment = std::make_unique<SliderAttachment> (processor.parameters, "ceiling", ceiling);
    attackAttachment = std::make_unique<SliderAttachment> (processor.parameters, "attack", attack);
    releaseAttachment = std::make_unique<SliderAttachment> (processor.parameters, "release", release);
    lookaheadAttachment = std::make_unique<SliderAttachment> (processor.parameters, "lookahead", lookahead);
    stereoLinkAttachment = std::make_unique<SliderAttachment> (processor.parameters, "stereoLink", stereoLink);
    oversamplingAttachment = std::make_unique<SliderAttachment> (processor.parameters, "oversampling", oversampling);
    truePeakAttachment = std::make_unique<ButtonAttachment> (processor.parameters, "truePeak", truePeak);
    bypassAttachment = std::make_unique<ButtonAttachment> (processor.parameters, "bypass", bypass);
    startTimerHz (20);
}

void HeavenLimitsAudioProcessorEditor::styleKnob (juce::Slider& slider, juce::Label& label,
                                                   const juce::String& name, const juce::String& suffix)
{
    setupLabel (label, name, name == "INPUT" || name == "OUTPUT CEILING" ? 11.0f : 10.0f);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 17);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff537da3));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff31495f));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffeaf2f8));
    slider.setColour (juce::Slider::textBoxTextColourId, ink);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x667f9eb9));
    slider.setTextValueSuffix (suffix);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void HeavenLimitsAudioProcessorEditor::styleLedButton (LedToggleButton& button)
{
    button.setClickingTogglesState (true);
    button.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffe8f0f7));
    addAndMakeVisible (button);
}

void HeavenLimitsAudioProcessorEditor::LedToggleButton::paintButton (juce::Graphics& g,
                                                                      bool isMouseOver,
                                                                      bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (isButtonDown ? juce::Colour (0xff273946)
                              : (isMouseOver ? juce::Colour (0xff344b5c) : juce::Colour (0xff1a2934)));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff697e8e));
    g.drawRoundedRectangle (bounds.reduced (0.7f), 6.0f, 1.0f);

    const auto led = juce::Rectangle<float> (bounds.getX() + 8.0f, bounds.getCentreY() - 4.0f, 8.0f, 8.0f);
    const auto ledColour = getToggleState() ? juce::Colour (0xff56f28d) : juce::Colour (0xff48545d);
    g.setColour (ledColour.withAlpha (getToggleState() ? 0.35f : 0.20f));
    g.fillEllipse (led.expanded (3.0f));
    g.setColour (ledColour);
    g.fillEllipse (led);
    g.setColour (juce::Colour (0xffe7eef4));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText (getButtonText(), getLocalBounds().withTrimmedLeft (18), juce::Justification::centred);
}

void HeavenLimitsAudioProcessorEditor::timerCallback()
{
    inputMeter.setLevelDb (juce::Decibels::gainToDecibels (processor.getInputPeak(), -60.0f));
    outputMeter.setLevelDb (juce::Decibels::gainToDecibels (processor.getOutputPeak(), -60.0f));
    grMeter.setReduction (processor.getGainReductionDb());

    auto set = [] (juce::Label& label, float value, int decimals, const juce::String& suffix)
    {
        label.setText (juce::String (value, decimals) + suffix, juce::dontSendNotification);
    };
    set (inputValue, (float) inputGain.getValue(), 1, " dB");
    set (ceilingValue, (float) ceiling.getValue(), 1, " dB");
    set (attackValue, (float) attack.getValue(), 2, " ms");
    set (releaseValue, (float) release.getValue(), 0, " ms");
    set (lookaheadValue, (float) lookahead.getValue(), 2, " ms");
    set (stereoLinkValue, (float) stereoLink.getValue(), 0, "%");
    set (rmsValue, processor.getRmsLevelDb(), 1, " dBFS");
    set (lufsValue, processor.getLufsLevel(), 1, " LUFS");
}

void HeavenLimitsAudioProcessorEditor::LedMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff111820));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (juce::Colour (0xff526679));
    g.drawRoundedRectangle (bounds.reduced (0.7f), 7.0f, 1.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText (name, getLocalBounds().removeFromTop (20), juce::Justification::centred);

    auto area = getLocalBounds().reduced (9, 27);
    const int count = 20;
    const float gap = 1.5f;
    const float segmentHeight = (area.getHeight() - gap * (count - 1)) / (float) count;
    const int lit = juce::jlimit (0, count, (int) std::round ((levelDb + 60.0f) / 60.0f * count));
    for (int i = 0; i < count; ++i)
    {
        const int fromBottom = count - 1 - i;
        const auto colour = fromBottom > 15 ? juce::Colour (0xff22d65b)
                           : fromBottom > 7 ? juce::Colour (0xffd8e62d)
                           : juce::Colour (0xffff493c);
        const auto segment = juce::Rectangle<float> ((float) area.getX(), area.getY() + i * (segmentHeight + gap),
                                                     (float) area.getWidth(), segmentHeight);
        g.setColour (i >= count - lit ? colour : juce::Colour (0xff28343e));
        g.fillRoundedRectangle (segment, 1.2f);
    }
}

void HeavenLimitsAudioProcessorEditor::GainReductionMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff111820));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (juce::Colour (0xff536b80));
    g.drawRoundedRectangle (bounds.reduced (0.7f), 7.0f, 1.0f);
    g.setColour (juce::Colour (0xffe1eaf2));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("GAIN REDUCTION", getLocalBounds().removeFromTop (22), juce::Justification::centred);
    g.setColour (red);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawText (juce::String (-reduction, 1) + " dB",
                getLocalBounds().withTrimmedTop (20).withTrimmedBottom (15), juce::Justification::centred);
    auto bar = bounds.reduced (12.0f, 0.0f).withHeight (8.0f).withY (bounds.getBottom() - 15.0f);
    g.setColour (juce::Colour (0xff303b45));
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (red);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * reduction / 24.0f), 3.0f);
}

void HeavenLimitsAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff101820));
    auto panel = bounds.reduced (6.0f);
    g.setGradientFill (juce::ColourGradient (metalTop, 0.0f, 0.0f, metalDark,
                                              0.0f, panel.getBottom(), false));
    g.fillRoundedRectangle (panel, 10.0f);
    g.setColour (juce::Colour (0xffedf5fc));
    g.drawRoundedRectangle (panel.reduced (1.0f), 9.0f, 1.0f);

    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions (25.0f, juce::Font::bold)));
    g.drawText ("HEAVEN LIMITS", 215, 8, 470, 32, juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText ("S T E R E O   B R I C K W A L L   L I M I T E R", 245, 39, 410, 15,
                juce::Justification::centred);
    g.setColour (juce::Colour (0xff43586d));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText ("Powered By TILADCO", 330, 54, 240, 14, juce::Justification::centred);
    g.setColour (juce::Colour (0x8843586d));
    g.drawLine (18.0f, 77.0f, 882.0f, 77.0f, 1.0f);
    g.drawLine (18.0f, 452.0f, 882.0f, 452.0f, 1.0f);
    drawScrew (g, { 17.0f, 17.0f }); drawScrew (g, { 883.0f, 17.0f });
    drawScrew (g, { 17.0f, 463.0f }); drawScrew (g, { 883.0f, 463.0f });
}

void HeavenLimitsAudioProcessorEditor::resized()
{
    inputMeter.setBounds (22, 98, 68, 330);
    outputMeter.setBounds (810, 98, 68, 330);

    inputLabel.setBounds (125, 96, 150, 18);
    inputGain.setBounds (143, 111, 115, 115);
    inputValue.setBounds (143, 225, 115, 16);
    ceilingLabel.setBounds (615, 96, 150, 18);
    ceiling.setBounds (633, 111, 115, 115);
    ceilingValue.setBounds (633, 225, 115, 16);
    grMeter.setBounds (360, 126, 180, 90);

    attackLabel.setBounds (112, 282, 88, 16);
    attack.setBounds (110, 295, 90, 92);
    attackValue.setBounds (110, 386, 90, 16);
    releaseLabel.setBounds (204, 282, 88, 16);
    release.setBounds (202, 295, 90, 92);
    releaseValue.setBounds (202, 386, 90, 16);
    lookaheadLabel.setBounds (296, 282, 88, 16);
    lookahead.setBounds (294, 295, 90, 92);
    lookaheadValue.setBounds (294, 386, 90, 16);
    stereoLinkLabel.setBounds (388, 282, 88, 16);
    stereoLink.setBounds (386, 295, 90, 92);
    stereoLinkValue.setBounds (386, 386, 90, 16);
    oversamplingLabel.setBounds (480, 282, 100, 16);
    oversampling.setBounds (485, 295, 90, 105);

    rmsLabel.setBounds (590, 290, 100, 16);
    rmsValue.setBounds (590, 306, 100, 20);
    lufsLabel.setBounds (590, 332, 100, 16);
    lufsValue.setBounds (590, 348, 100, 20);
    truePeak.setBounds (690, 300, 100, 30);
    bypass.setBounds (690, 341, 100, 30);
}
