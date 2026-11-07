#include "PluginEditor.h"
#include <cmath>

namespace
{
    const juce::Colour ink (0xff101820), red (0xffff3838), metalTop (0xffd8e3ee), metalDark (0xff718ba5);
    const juce::Colour meterOrange (0xffff9f1c);
    const juce::Colour meterMint (0xff35e0c1);
    const juce::Colour meterViolet (0xffc33ce6);

    void setupLabel (juce::Label& label, const juce::String& text, float size = 10.0f)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, ink);
        label.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
        label.setInterceptsMouseClicks (false, false);
    }

    void drawScrew (juce::Graphics& g, juce::Point<float> c)
    {
        g.setColour (juce::Colour (0xff263441));
        g.fillEllipse (c.x - 6, c.y - 6, 12, 12);
        g.setColour (juce::Colour (0xffdfe7ef));
        g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
        g.setColour (juce::Colour (0xff273744));
        g.drawLine (c.x - 2, c.y + 2, c.x + 2, c.y - 2, 1.0f);
    }
}

HeavenLimitsAudioProcessorEditor::VintageKnob::VintageKnob()
{
    setSliderStyle (RotaryHorizontalVerticalDrag);
    setTextBoxStyle (NoTextBox, false, 0, 0);
}

void HeavenLimitsAudioProcessorEditor::VintageKnob::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.43f;
    g.setColour (juce::Colour (0xff151a20));
    g.fillEllipse (centre.x - radius - 4, centre.y - radius - 4, 2 * radius + 8, 2 * radius + 8);

    for (int i = 0; i < 48; ++i)
    {
        const float angle = juce::MathConstants<float>::pi * (-0.75f + 1.5f * i / 47.0f);
        const float inner = radius + 7.0f;
        const float outer = radius + (i % 4 == 0 ? 14.0f : 10.0f);
        g.setColour (i % 4 == 0 ? juce::Colour (0xff222a33) : juce::Colour (0xff58616b));
        g.drawLine (centre.x + std::sin (angle) * inner, centre.y - std::cos (angle) * inner,
                    centre.x + std::sin (angle) * outer, centre.y - std::cos (angle) * outer,
                    i % 4 == 0 ? 1.7f : 1.0f);
    }

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff59616a), centre.x - radius, centre.y - radius,
                                              juce::Colour (0xff090b0e), centre.x + radius, centre.y + radius, true));
    g.fillEllipse (centre.x - radius, centre.y - radius, 2 * radius, 2 * radius);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f3f5), centre.x - radius * 0.6f, centre.y - radius * 0.6f,
                                              juce::Colour (0xff4a535e), centre.x + radius * 0.65f, centre.y + radius * 0.65f, false));
    g.fillEllipse (centre.x - radius * 0.82f, centre.y - radius * 0.82f, radius * 1.64f, radius * 1.64f);

    const double range = juce::jmax (1.0e-9, getMaximum() - getMinimum());
    const float normalised = (float) ((getValue() - getMinimum()) / range);
    const float angle = juce::jmap (normalised, 0.0f, 1.0f,
                                    -juce::MathConstants<float>::pi * 0.75f,
                                     juce::MathConstants<float>::pi * 0.75f);
    g.setColour (juce::Colours::white);
    g.drawLine (centre.x, centre.y, centre.x + std::sin (angle) * radius * 0.72f,
                centre.y - std::cos (angle) * radius * 0.72f, juce::jmax (2.0f, radius * 0.06f));
    g.setColour (juce::Colour (0xff3a424c));
    g.fillEllipse (centre.x - 3, centre.y - 3, 6, 6);
}

HeavenLimitsAudioProcessorEditor::HeavenLimitsAudioProcessorEditor (HeavenLimitsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (900, 500);
    styleKnob (inputGain, inputLabel, "INPUT", " dB");
    styleKnob (ceiling, ceilingLabel, "OUTPUT CEILING", " dB");
    styleKnob (attack, attackLabel, "ATTACK", " ms");
    styleKnob (release, releaseLabel, "RELEASE", " ms");
    styleKnob (lookahead, lookaheadLabel, "LOOKAHEAD", " ms");
    styleKnob (stereoLink, stereoLinkLabel, "STEREO LINK", "%");
    styleKnob (multibandAmount, multibandLabel, "MULTIBAND AMOUNT", "%");
    styleKnob (noiseShaping, noiseLabel, "NOISE SHAPING", "%");

    oversampling.addItem ("1x", 1);
    oversampling.addItem ("2x", 2);
    oversampling.addItem ("4x", 3);
    oversampling.addItem ("8x", 4);
    dither.addItem ("Off", 1);
    dither.addItem ("16-bit", 2);
    dither.addItem ("24-bit", 3);
    for (auto* box : { &oversampling, &dither })
    {
        box->setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff19232b));
        box->setColour (juce::ComboBox::textColourId, juce::Colour (0xffe6edf2));
        box->setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff657887));
        box->setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffe6edf2));
        box->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*box);
    }
    setupLabel (oversamplingLabel, "OVERSAMPLING");
    setupLabel (ditherLabel, "DITHER");
    setupLabel (rmsLabel, "RMS");
    setupLabel (lufsLabel, "LUFS (K-WEIGHTED)");
    for (auto* label : { &oversamplingLabel, &ditherLabel, &rmsLabel, &lufsLabel })
        addAndMakeVisible (*label);

    for (auto* button : { &truePeak, &bypass, &multiband, &aes17, &isp })
        styleLedButton (*button);

    for (auto* label : { &inputValue, &ceilingValue, &attackValue, &releaseValue, &lookaheadValue,
                         &stereoLinkValue, &rmsValue, &lufsValue, &multibandValue, &ditherValue, &noiseValue })
    {
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, ink);
        label->setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        addAndMakeVisible (*label);
    }
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (grMeter);

    inputAttachment = std::make_unique<SliderAttachment> (processor.parameters, "input", inputGain);
    ceilingAttachment = std::make_unique<SliderAttachment> (processor.parameters, "ceiling", ceiling);
    attackAttachment = std::make_unique<SliderAttachment> (processor.parameters, "attack", attack);
    releaseAttachment = std::make_unique<SliderAttachment> (processor.parameters, "release", release);
    lookaheadAttachment = std::make_unique<SliderAttachment> (processor.parameters, "lookahead", lookahead);
    stereoLinkAttachment = std::make_unique<SliderAttachment> (processor.parameters, "stereoLink", stereoLink);
    multibandAmountAttachment = std::make_unique<SliderAttachment> (processor.parameters, "multibandAmount", multibandAmount);
    noiseShapingAttachment = std::make_unique<SliderAttachment> (processor.parameters, "noiseShaping", noiseShaping);
    oversamplingAttachment = std::make_unique<ComboAttachment> (processor.parameters, "oversampling", oversampling);
    ditherAttachment = std::make_unique<ComboAttachment> (processor.parameters, "dither", dither);

    auto attachButton = [this] (LedToggleButton& button, const juce::String& id)
    {
        return std::make_unique<ButtonAttachment> (processor.parameters, id, button);
    };
    truePeakAttachment = attachButton (truePeak, "truePeak");
    bypassAttachment = attachButton (bypass, "bypass");
    multibandAttachment = attachButton (multiband, "multiband");
    aes17Attachment = attachButton (aes17, "aes17");
    ispAttachment = attachButton (isp, "isp");
    startTimerHz (20);
}

void HeavenLimitsAudioProcessorEditor::styleKnob (juce::Slider& slider, juce::Label& label,
                                                    const juce::String& name, const juce::String& suffix)
{
    setupLabel (label, name, 10.0f);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setTextValueSuffix (suffix);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void HeavenLimitsAudioProcessorEditor::styleLedButton (LedToggleButton& button)
{
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
}

void HeavenLimitsAudioProcessorEditor::LedToggleButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (down ? juce::Colour (0xff263946) : over ? juce::Colour (0xff344b5c) : juce::Colour (0xff1a2934));
    g.fillRoundedRectangle (bounds, 5.0f);
    auto led = juce::Rectangle<float> (bounds.getX() + 6, bounds.getCentreY() - 4, 8, 8);
    const bool bypassButton = getButtonText() == "BYPASS";
    auto colour = getToggleState() ? (bypassButton ? juce::Colour (0xffff3838) : juce::Colour (0xff56f28d))
                                   : juce::Colour (0xff48545d);
    g.setColour (colour.withAlpha (0.35f));
    g.fillEllipse (led.expanded (2.0f));
    g.setColour (colour);
    g.fillEllipse (led);
    g.setColour (juce::Colour (0xffe7eef4));
    g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
    g.drawText (getButtonText(), getLocalBounds().withTrimmedLeft (17), juce::Justification::centred);
}

void HeavenLimitsAudioProcessorEditor::timerCallback()
{
    const bool ispOn = processor.parameters.getRawParameterValue ("isp")->load() > 0.5f;
    const bool aes = processor.parameters.getRawParameterValue ("aes17")->load() > 0.5f;
    const float peak = ispOn ? processor.getIspPeak() : processor.getOutputPeak();
    outputMeter.setLevelDb (juce::Decibels::gainToDecibels (peak, -60.0f) + (aes ? 3.0f : 0.0f));
    inputMeter.setLevelDb (juce::Decibels::gainToDecibels (processor.getInputPeak(), -60.0f));
    grMeter.setReduction (processor.getGainReductionDb());
    auto set = [] (juce::Label& label, float value, int decimals, const juce::String& suffix)
    { label.setText (juce::String (value, decimals) + suffix, juce::dontSendNotification); };
    set (inputValue, (float) inputGain.getValue(), 1, " dB");
    set (ceilingValue, (float) ceiling.getValue(), 1, " dB");
    set (attackValue, (float) attack.getValue(), 2, " ms");
    set (releaseValue, (float) release.getValue(), 2, " ms");
    set (lookaheadValue, (float) lookahead.getValue(), 2, " ms");
    set (stereoLinkValue, (float) stereoLink.getValue(), 0, "%");
    set (rmsValue, processor.getRmsLevelDb() + (aes ? 3.0f : 0.0f), 1, " dBFS");
    set (lufsValue, processor.getLufsLevel() + (aes ? 3.0f : 0.0f), 1, " LUFS");
    set (multibandValue, (float) multibandAmount.getValue(), 0, "%");
    ditherValue.setText (dither.getText(), juce::dontSendNotification);
    set (noiseValue, (float) noiseShaping.getValue(), 0, "%");
}

void HeavenLimitsAudioProcessorEditor::LedMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff111820)); g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff526679)); g.drawRoundedRectangle (bounds.reduced (0.7f), 6.0f, 1.0f);
    g.setColour (juce::Colours::white); g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText (name, getLocalBounds().removeFromTop (20), juce::Justification::centred);
    auto area = getLocalBounds().reduced (8, 27);
    const int count = 20; const float gap = 1.5f;
    const float h = (area.getHeight() - gap * (count - 1)) / (float) count;
    const int lit = juce::jlimit (0, count, (int) std::round ((levelDb + 60.0f) / 60.0f * count));
    for (int i = 0; i < count; ++i)
    {
        const int fromBottom = count - 1 - i;
        const auto colour = fromBottom > 15 ? meterOrange : fromBottom > 7 ? meterMint : meterViolet;
        auto segment = juce::Rectangle<float> ((float) area.getX(), area.getY() + i * (h + gap), (float) area.getWidth(), h);
        g.setColour (i >= count - lit ? colour : juce::Colour (0xff28343e));
        g.fillRoundedRectangle (segment, 1.0f);
    }
}

void HeavenLimitsAudioProcessorEditor::GainReductionMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff111820)); g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff536b80)); g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    g.setColour (juce::Colours::white); g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("GAIN REDUCTION", getLocalBounds().removeFromTop (20), juce::Justification::centred);
    g.setColour (red); g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    g.drawText (juce::String (-reduction, 1) + " dB",
                getLocalBounds().withTrimmedTop (18).withTrimmedBottom (22), juce::Justification::centred);

    // 12-segment scale, 0-12 dB. Fill starts at the right and progresses to the left.
    auto bar = bounds.reduced (9.0f, 0.0f).withHeight (10.0f).withY (bounds.getBottom() - 15.0f);
    constexpr int segments = 12;
    constexpr float segmentGap = 2.0f;
    const float segmentWidth = (bar.getWidth() - segmentGap * (segments - 1)) / (float) segments;
    const int active = juce::jlimit (0, segments, (int) std::round (reduction));
    for (int i = 0; i < segments; ++i)
    {
        auto segment = juce::Rectangle<float> (bar.getX() + i * (segmentWidth + segmentGap), bar.getY(),
                                                segmentWidth, bar.getHeight());
        const int distanceFromRight = segments - 1 - i;
        const auto colour = distanceFromRight < 4 ? meterViolet
                          : distanceFromRight < 8 ? meterMint : meterOrange;
        g.setColour (i >= segments - active ? colour : juce::Colour (0xff34323b));
        g.fillRoundedRectangle (segment, 1.0f);
        g.setColour (juce::Colour (0xff625a68));
        g.drawRoundedRectangle (segment, 1.0f, 0.5f);
    }
}

void HeavenLimitsAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff101820));
    g.setGradientFill (juce::ColourGradient (metalTop, 0, 0, metalDark, 0, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds.reduced (6), 10);
    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions (25, juce::Font::bold)));
    g.drawText ("HEAVEN LIMITS", 215, 4, 470, 29, juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
    g.drawText ("STEREO BRICKWALL MAXIMIZER", 200, 34, 500, 13, juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
    g.drawText ("Powered By TILADCO", 200, 47, 500, 12, juce::Justification::centred);
    g.setColour (juce::Colour (0x8843586d));
    g.drawLine (18, 65, 882, 65, 1);
    g.drawLine (18, 480, 882, 480, 1);
    drawScrew (g, { 17, 17 }); drawScrew (g, { 883, 17 });
    drawScrew (g, { 17, 483 }); drawScrew (g, { 883, 483 });
}

void HeavenLimitsAudioProcessorEditor::resized()
{
    inputMeter.setBounds (20, 78, 70, 335);
    outputMeter.setBounds (810, 78, 70, 335);
    inputLabel.setBounds (112, 75, 130, 16); inputGain.setBounds (118, 90, 120, 120); inputValue.setBounds (120, 210, 116, 16);
    ceilingLabel.setBounds (650, 75, 130, 16); ceiling.setBounds (655, 90, 120, 120); ceilingValue.setBounds (658, 210, 116, 16);
    grMeter.setBounds (365, 95, 170, 75);

    attackLabel.setBounds (100, 245, 85, 16); attack.setBounds (100, 259, 85, 75); attackValue.setBounds (100, 334, 85, 16);
    releaseLabel.setBounds (188, 245, 85, 16); release.setBounds (188, 259, 85, 75); releaseValue.setBounds (188, 334, 85, 16);
    lookaheadLabel.setBounds (276, 245, 85, 16); lookahead.setBounds (276, 259, 85, 75); lookaheadValue.setBounds (276, 334, 85, 16);
    stereoLinkLabel.setBounds (364, 245, 85, 16); stereoLink.setBounds (364, 259, 85, 75); stereoLinkValue.setBounds (364, 334, 85, 16);
    multibandLabel.setBounds (452, 245, 110, 16); multibandAmount.setBounds (462, 259, 90, 75); multibandValue.setBounds (462, 334, 90, 16);

    oversamplingLabel.setBounds (560, 245, 100, 16); oversampling.setBounds (560, 263, 100, 24);
    ditherLabel.setBounds (560, 291, 100, 16); dither.setBounds (560, 309, 100, 24);
    noiseLabel.setBounds (560, 337, 100, 16); noiseShaping.setBounds (565, 350, 90, 60); noiseValue.setBounds (565, 410, 90, 15);
    rmsLabel.setBounds (665, 250, 130, 14); rmsValue.setBounds (665, 265, 130, 18);
    lufsLabel.setBounds (665, 291, 130, 14); lufsValue.setBounds (665, 306, 130, 18);

    constexpr int buttonHeight = 20;
    multiband.setBounds (130, 438, 160, buttonHeight);
    truePeak.setBounds (370, 438, 160, buttonHeight);
    bypass.setBounds (610, 438, 160, buttonHeight);
    aes17.setBounds (250, 463, 160, buttonHeight);
    isp.setBounds (490, 463, 160, buttonHeight);
}
