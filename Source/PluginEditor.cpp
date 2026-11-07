#include "PluginEditor.h"

namespace
{
    const juce::Colour metalTop   (0xffd8e3ee);
    const juce::Colour metalMid   (0xffaabed1);
    const juce::Colour metalDark  (0xff718ba5);
    const juce::Colour ink        (0xff101820);
    const juce::Colour lime       (0xffb9f15a);
    const juce::Colour red        (0xffff3838);

    void drawScrew (juce::Graphics& g, juce::Point<float> c)
    {
        g.setColour (juce::Colour (0xff263441));
        g.fillEllipse (c.x - 11.0f, c.y - 11.0f, 22.0f, 22.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f7fa), c.x - 5, c.y - 7,
                                                  juce::Colour (0xff536575), c.x + 7, c.y + 8, false));
        g.fillEllipse (c.x - 8.5f, c.y - 8.5f, 17.0f, 17.0f);
        g.setColour (juce::Colour (0xff273744));
        g.drawLine (c.x - 5.0f, c.y + 4.0f, c.x + 5.0f, c.y - 4.0f, 2.0f);
    }

    void setupLabel (juce::Label& label, const juce::String& text, float size = 13.0f)
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
    setSize (1080, 600);

    styleKnob (inputGain, inputLabel, "INPUT", " dB");
    styleKnob (ceiling, ceilingLabel, "OUTPUT CEILING", " dB");
    styleKnob (attack, attackLabel, "ATTACK", " ms");
    styleKnob (release, releaseLabel, "RELEASE", " ms");
    styleKnob (lookahead, lookaheadLabel, "LOOKAHEAD", " ms");
    styleKnob (stereoLink, stereoLinkLabel, "STEREO LINK", "%");

    oversampling.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    oversampling.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 20);
    oversampling.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff507da7));
    oversampling.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff344b60));
    oversampling.setColour (juce::Slider::thumbColourId, juce::Colour (0xffe7f0f7));
    oversampling.setColour (juce::Slider::textBoxTextColourId, ink);
    oversampling.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x557d9bb5));
    oversampling.setNumDecimalPlacesToDisplay (0);
    setupLabel (oversamplingLabel, "OVERSAMPLING", 12.0f);
    addAndMakeVisible (oversampling);
    addAndMakeVisible (oversamplingLabel);

    styleToggle (truePeak, "TRUE PEAK");
    styleToggle (bypass, "BYPASS");

    auto addValue = [this] (juce::Label& l)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, juce::Colour (0xff263b4d));
        l.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        addAndMakeVisible (l);
    };
    for (auto* l : { &inputValue, &ceilingValue, &attackValue, &releaseValue, &lookaheadValue, &stereoLinkValue })
        addValue (*l);

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

    startTimerHz (30);
}

void HeavenLimitsAudioProcessorEditor::styleKnob (juce::Slider& slider, juce::Label& label,
                                                    const juce::String& name, const juce::String& suffix)
{
    setupLabel (label, name, name == "INPUT" || name == "OUTPUT CEILING" ? 15.0f : 12.0f);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 82, 20);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff537da3));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff31495f));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffeaf2f8));
    slider.setColour (juce::Slider::textBoxTextColourId, ink);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x667f9eb9));
    slider.setTextValueSuffix (suffix);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void HeavenLimitsAudioProcessorEditor::styleToggle (juce::ToggleButton& button, const juce::String& name)
{
    button.setButtonText (name);
    button.setColour (juce::ToggleButton::textColourId, ink);
    button.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4c8d65));
    button.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff71808c));
    addAndMakeVisible (button);
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
}

void HeavenLimitsAudioProcessorEditor::LedMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff111820));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (juce::Colour (0xff526679));
    g.drawRoundedRectangle (r.reduced (1.0f), 9.0f, 1.5f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (name, getLocalBounds().removeFromTop (25), juce::Justification::centred);
    auto area = getLocalBounds().reduced (13, 34);
    const int count = 24;
    const float gap = 2.0f;
    const float h = (area.getHeight() - gap * (count - 1)) / (float) count;
    const int lit = juce::jlimit (0, count, (int) std::round ((levelDb + 60.0f) / 60.0f * count));
    for (int i = 0; i < count; ++i)
    {
        const int fromBottom = count - 1 - i;
        juce::Colour c = fromBottom > 18 ? juce::Colour (0xff22d65b)
                         : fromBottom > 7 ? juce::Colour (0xffd8e62d)
                         : juce::Colour (0xffff493c);
        auto seg = juce::Rectangle<float> ((float) area.getX(), area.getY() + i * (h + gap),
                                           (float) area.getWidth(), h);
        g.setColour (i >= count - lit ? c : juce::Colour (0xff28343e));
        g.fillRoundedRectangle (seg, 1.8f);
    }
    g.setColour (juce::Colour (0xffc8d5df));
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("L     R", getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

void HeavenLimitsAudioProcessorEditor::GainReductionMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff111820));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (juce::Colour (0xff536b80));
    g.drawRoundedRectangle (r.reduced (1.0f), 9.0f, 1.5f);
    g.setColour (juce::Colour (0xffe1eaf2));
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawText ("GAIN REDUCTION", getLocalBounds().removeFromTop (27), juce::Justification::centred);
    g.setColour (red);
    g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)));
    g.drawText (juce::String (-reduction, 1) + " dB", getLocalBounds().withTrimmedTop (25).withTrimmedBottom (18),
                juce::Justification::centred);
    auto bar = getLocalBounds().toFloat().reduced (18.0f, 0.0f).withHeight (12.0f).withY (r.getBottom() - 22.0f);
    g.setColour (juce::Colour (0xff303b45));
    g.fillRoundedRectangle (bar, 3.0f);
    auto active = bar.withWidth (bar.getWidth() * reduction / 24.0f);
    g.setColour (red);
    g.fillRoundedRectangle (active, 3.0f);
}

void HeavenLimitsAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff101820));
    auto panel = bounds.reduced (8.0f);
    g.setGradientFill (juce::ColourGradient (metalTop, 0.0f, 0.0f, metalDark, 0.0f, panel.getBottom(), false));
    g.fillRoundedRectangle (panel, 13.0f);
    g.setColour (juce::Colour (0xffedf5fc));
    g.drawRoundedRectangle (panel.reduced (2.0f), 12.0f, 2.0f);
    g.setColour (juce::Colour (0x55728da5));
    for (int y = 16; y < getHeight() - 12; y += 4)
        g.drawLine (14.0f, (float) y, (float) getWidth() - 14.0f, (float) y, 0.35f);

    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions (31.0f, juce::Font::bold)));
    g.drawText ("HEAVEN LIMITS", 250, 17, 580, 42, juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("S T E R E O   B R I C K W A L L   L I M I T E R", 300, 57, 480, 20,
                juce::Justification::centred);

    g.setColour (juce::Colour (0x8843586d));
    g.drawLine (30.0f, 84.0f, 1050.0f, 84.0f, 1.2f);
    g.drawLine (30.0f, 560.0f, 1050.0f, 560.0f, 1.2f);
    drawScrew (g, { 29.0f, 29.0f }); drawScrew (g, { 1051.0f, 29.0f });
    drawScrew (g, { 29.0f, 571.0f }); drawScrew (g, { 1051.0f, 571.0f });

    g.setColour (juce::Colour (0x66354d63));
    g.drawLine (225.0f, 112.0f, 225.0f, 535.0f, 1.0f);
    g.drawLine (855.0f, 112.0f, 855.0f, 535.0f, 1.0f);
    g.drawLine (30.0f, 330.0f, 1050.0f, 330.0f, 1.0f);
}

void HeavenLimitsAudioProcessorEditor::resized()
{
    inputMeter.setBounds (34, 112, 112, 420);
    outputMeter.setBounds (934, 112, 112, 420);

    inputLabel.setBounds (245, 105, 190, 28);
    inputGain.setBounds (260, 132, 160, 160);
    inputValue.setBounds (270, 292, 140, 22);

    ceilingLabel.setBounds (650, 105, 210, 28);
    ceiling.setBounds (675, 132, 160, 160);
    ceilingValue.setBounds (685, 292, 140, 22);

    grMeter.setBounds (447, 151, 186, 150);

    attackLabel.setBounds (235, 363, 125, 25);
    attack.setBounds (235, 388, 125, 125);
    attackValue.setBounds (235, 510, 125, 22);

    releaseLabel.setBounds (365, 363, 125, 25);
    release.setBounds (365, 388, 125, 125);
    releaseValue.setBounds (365, 510, 125, 22);

    lookaheadLabel.setBounds (495, 363, 125, 25);
    lookahead.setBounds (495, 388, 125, 125);
    lookaheadValue.setBounds (495, 510, 125, 22);

    stereoLinkLabel.setBounds (630, 365, 120, 25);
    stereoLink.setBounds (638, 390, 104, 104);
    stereoLinkValue.setBounds (635, 500, 110, 22);

    oversamplingLabel.setBounds (755, 365, 125, 25);
    oversampling.setBounds (765, 389, 105, 120);

    truePeak.setBounds (865, 400, 75, 70);
    bypass.setBounds (865, 477, 75, 45);
}
