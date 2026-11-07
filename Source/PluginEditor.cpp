#include "PluginEditor.h"
#include <cmath>

namespace
{
    const juce::Colour ink (0xff101820), red (0xffff3838), metalTop (0xffd8e3ee), metalDark (0xff718ba5);
    void setupLabel (juce::Label& l, const juce::String& text, float size = 10.0f)
    {
        l.setText (text, juce::dontSendNotification); l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, ink); l.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
        l.setInterceptsMouseClicks (false, false);
    }
    void drawScrew (juce::Graphics& g, juce::Point<float> c)
    {
        g.setColour (juce::Colour (0xff263441)); g.fillEllipse (c.x - 6, c.y - 6, 12, 12);
        g.setColour (juce::Colour (0xffdfe7ef)); g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
        g.setColour (juce::Colour (0xff273744)); g.drawLine (c.x - 2, c.y + 2, c.x + 2, c.y - 2, 1.0f);
    }
}

HeavenLimitsAudioProcessorEditor::VintageKnob::VintageKnob()
{
    setSliderStyle (RotaryHorizontalVerticalDrag); setTextBoxStyle (NoTextBox, false, 0, 0);
}
void HeavenLimitsAudioProcessorEditor::VintageKnob::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat(); const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.43f;
    g.setColour (juce::Colour (0xff151a20)); g.fillEllipse (c.x-r-4, c.y-r-4, 2*r+8, 2*r+8);
    for (int i=0; i<48; ++i)
    {
        const float a = juce::MathConstants<float>::pi * (-0.75f + 1.5f * i / 47.0f);
        const float ri = r + 7.0f, ro = r + (i % 4 == 0 ? 14.0f : 10.0f);
        g.setColour (i % 4 == 0 ? juce::Colour (0xff222a33) : juce::Colour (0xff58616b));
        g.drawLine (c.x + std::sin (a)*ri, c.y - std::cos (a)*ri,
                    c.x + std::sin (a)*ro, c.y - std::cos (a)*ro, i % 4 == 0 ? 1.7f : 1.0f);
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff59616a), c.x-r, c.y-r,
                                             juce::Colour (0xff090b0e), c.x+r, c.y+r, true));
    g.fillEllipse (c.x-r, c.y-r, 2*r, 2*r);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f3f5), c.x-r*0.6f, c.y-r*0.6f,
                                             juce::Colour (0xff4a535e), c.x+r*0.65f, c.y+r*0.65f, false));
    g.fillEllipse (c.x-r*0.82f, c.y-r*0.82f, r*1.64f, r*1.64f);
    g.setColour (juce::Colour (0x66778088));
    for (int i=0;i<5;++i) g.drawLine (c.x-r*0.65f, c.y-r*0.5f+i*2.0f, c.x+r*0.5f, c.y+r*0.6f+i*2.0f, 0.5f);
    const auto norm = (getValue()-getMinimum()) / juce::jmax (1.0e-9, getMaximum()-getMinimum());
    const float angle = (float) juce::jmap (norm, 0.0, 1.0, -juce::MathConstants<double>::pi*0.75, juce::MathConstants<double>::pi*0.75);
    g.setColour (juce::Colours::white);
    g.drawLine (c.x, c.y, c.x + std::sin (angle)*r*0.72f, c.y - std::cos (angle)*r*0.72f, juce::jmax (2.0f, r*0.06f));
    g.setColour (juce::Colour (0xff3a424c)); g.fillEllipse (c.x-3, c.y-3, 6, 6);
}

HeavenLimitsAudioProcessorEditor::HeavenLimitsAudioProcessorEditor (HeavenLimitsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (900, 500);
    styleKnob (inputGain,inputLabel,"INPUT"," dB"); styleKnob (ceiling,ceilingLabel,"OUTPUT CEILING"," dB");
    styleKnob (attack,attackLabel,"ATTACK"," ms"); styleKnob (release,releaseLabel,"RELEASE"," ms");
    styleKnob (lookahead,lookaheadLabel,"LOOKAHEAD"," ms"); styleKnob (stereoLink,stereoLinkLabel,"STEREO LINK","%");
    styleKnob (multibandAmount,multibandLabel,"MULTIBAND AMOUNT","%");
    styleKnob (noiseShaping,noiseLabel,"NOISE SHAPING","%");

    for (auto* box : { &oversampling, &dither }) { box->setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff19232b));
        box->setColour (juce::ComboBox::textColourId, juce::Colour (0xffe6edf2)); box->setJustificationType (juce::Justification::centred); addAndMakeVisible (*box); }
    setupLabel (oversamplingLabel,"OVERSAMPLING"); setupLabel (ditherLabel,"DITHER (24-BIT OUTPUT)");
    for (auto* l : { &oversamplingLabel,&ditherLabel,&rmsLabel,&lufsLabel }) addAndMakeVisible (*l);
    setupLabel (rmsLabel,"RMS"); setupLabel (lufsLabel,"LUFS (K-WEIGHTED)");
    for (auto* b : { &truePeak,&bypass,&multiband,&monitor,&aes17,&isp,&meterReset }) styleLedButton (*b);
    for (auto* l : { &inputValue,&ceilingValue,&attackValue,&releaseValue,&lookaheadValue,&stereoLinkValue,
                     &rmsValue,&lufsValue,&multibandValue,&ditherValue,&noiseValue })
    { l->setJustificationType (juce::Justification::centred); l->setColour (juce::Label::textColourId, ink);
      l->setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold))); addAndMakeVisible (*l); }
    addAndMakeVisible (inputMeter); addAndMakeVisible (outputMeter); addAndMakeVisible (grMeter);

    inputAttachment = std::make_unique<SliderAttachment> (processor.parameters,"input",inputGain);
    ceilingAttachment = std::make_unique<SliderAttachment> (processor.parameters,"ceiling",ceiling);
    attackAttachment = std::make_unique<SliderAttachment> (processor.parameters,"attack",attack);
    releaseAttachment = std::make_unique<SliderAttachment> (processor.parameters,"release",release);
    lookaheadAttachment = std::make_unique<SliderAttachment> (processor.parameters,"lookahead",lookahead);
    stereoLinkAttachment = std::make_unique<SliderAttachment> (processor.parameters,"stereoLink",stereoLink);
    multibandAmountAttachment = std::make_unique<SliderAttachment> (processor.parameters,"multibandAmount",multibandAmount);
    noiseShapingAttachment = std::make_unique<SliderAttachment> (processor.parameters,"noiseShaping",noiseShaping);
    oversamplingAttachment = std::make_unique<ComboAttachment> (processor.parameters,"oversampling",oversampling);
    ditherAttachment = std::make_unique<ComboAttachment> (processor.parameters,"dither",dither);
    for (auto* b : { &truePeak,&bypass,&multiband,&monitor,&aes17,&isp,&meterReset })
    {
        const juce::String id = b == &truePeak ? "truePeak" : b == &bypass ? "bypass" : b == &multiband ? "multiband" :
            b == &monitor ? "monitor" : b == &aes17 ? "aes17" : b == &isp ? "isp" : "meterReset";
        auto attachment = std::make_unique<ButtonAttachment> (processor.parameters,id,*b);
        if (b == &truePeak) truePeakAttachment = std::move (attachment);
        else if (b == &bypass) bypassAttachment = std::move (attachment);
        else if (b == &multiband) multibandAttachment = std::move (attachment);
        else if (b == &monitor) monitorAttachment = std::move (attachment);
        else if (b == &aes17) aes17Attachment = std::move (attachment);
        else if (b == &isp) ispAttachment = std::move (attachment);
        else meterResetAttachment = std::move (attachment);
    }
    meterReset.onClick = [this] { processor.parameters.getParameter ("meterReset")->setValueNotifyingHost (1.0f);
                                   juce::Timer::callAfterDelay (80, [this] { processor.parameters.getParameter ("meterReset")->setValueNotifyingHost (0.0f); }); };
    startTimerHz (20);
}

void HeavenLimitsAudioProcessorEditor::styleKnob (juce::Slider& s, juce::Label& l,const juce::String& name,const juce::String& suffix)
{
    setupLabel (l,name,10.0f); s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow,false,64,16); s.setTextValueSuffix (suffix);
    s.setColour (juce::Slider::textBoxTextColourId,ink); s.setColour (juce::Slider::textBoxBackgroundColourId,juce::Colour (0x778da1b3));
    addAndMakeVisible (s); addAndMakeVisible (l);
}
void HeavenLimitsAudioProcessorEditor::styleLedButton (LedToggleButton& b)
{ b.setClickingTogglesState (true); addAndMakeVisible (b); }
void HeavenLimitsAudioProcessorEditor::LedToggleButton::paintButton (juce::Graphics& g,bool over,bool down)
{
    auto r=getLocalBounds().toFloat(); g.setColour (down?juce::Colour(0xff263946):over?juce::Colour(0xff344b5c):juce::Colour(0xff1a2934));
    g.fillRoundedRectangle(r,5); auto led=juce::Rectangle<float>(r.getX()+6,r.getCentreY()-4,8,8);
    auto c=getToggleState()?juce::Colour(0xff56f28d):juce::Colour(0xff48545d); g.setColour(c.withAlpha(0.35f)); g.fillEllipse(led.expanded(2));
    g.setColour(c); g.fillEllipse(led); g.setColour(juce::Colour(0xffe7eef4)); g.setFont(juce::Font(juce::FontOptions(8.5f,juce::Font::bold)));
    g.drawText(getButtonText(),getLocalBounds().withTrimmedLeft(17),juce::Justification::centred);
}
void HeavenLimitsAudioProcessorEditor::timerCallback()
{
    const bool ispOn=processor.parameters.getRawParameterValue("isp")->load()>0.5f;
    const bool aes=processor.parameters.getRawParameterValue("aes17")->load()>0.5f;
    const float peak=ispOn?processor.getIspPeak():processor.getOutputPeak();
    outputMeter.setLevelDb(juce::Decibels::gainToDecibels(peak,-60.0f)+(aes?3.0f:0.0f));
    inputMeter.setLevelDb(juce::Decibels::gainToDecibels(processor.getInputPeak(),-60.0f)); grMeter.setReduction(processor.getGainReductionDb());
    auto set=[](juce::Label& l,float v,int d,const juce::String& s){l.setText(juce::String(v,d)+s,juce::dontSendNotification);};
    set(inputValue,(float)inputGain.getValue(),1," dB"); set(ceilingValue,(float)ceiling.getValue(),1," dB");
    set(attackValue,(float)attack.getValue(),2," ms"); set(releaseValue,(float)release.getValue(),2," ms");
    set(lookaheadValue,(float)lookahead.getValue(),2," ms"); set(stereoLinkValue,(float)stereoLink.getValue(),0,"%");
    set(rmsValue,processor.getRmsLevelDb()+(aes?3.0f:0.0f),1," dBFS"); set(lufsValue,processor.getLufsLevel()+(aes?3.0f:0.0f),1," LUFS");
    set(multibandValue,(float)multibandAmount.getValue(),0,"%");
    ditherValue.setText(dither.getText(),juce::dontSendNotification); set(noiseValue,(float)noiseShaping.getValue(),0,"%");
}
void HeavenLimitsAudioProcessorEditor::LedMeter::paint(juce::Graphics& g)
{
    auto b=getLocalBounds().toFloat(); g.setColour(juce::Colour(0xff111820)); g.fillRoundedRectangle(b,6);
    g.setColour(juce::Colour(0xff526679)); g.drawRoundedRectangle(b.reduced(0.7f),6,1);
    g.setColour(juce::Colours::white); g.setFont(juce::Font(juce::FontOptions(10,juce::Font::bold)));
    g.drawText(name,getLocalBounds().removeFromTop(20),juce::Justification::centred);
    auto a=getLocalBounds().reduced(8,27); const int n=20; const float gap=1.5f,h=(a.getHeight()-gap*(n-1))/(float)n;
    int lit=juce::jlimit(0,n,(int)std::round((levelDb+60)/60*n));
    for(int i=0;i<n;++i){int from=n-1-i; auto c=from>15?juce::Colour(0xff22d65b):from>7?juce::Colour(0xffd8e62d):juce::Colour(0xffff493c);
        auto seg=juce::Rectangle<float>((float)a.getX(),a.getY()+i*(h+gap),(float)a.getWidth(),h); g.setColour(i>=n-lit?c:juce::Colour(0xff28343e));g.fillRoundedRectangle(seg,1);}
}
void HeavenLimitsAudioProcessorEditor::GainReductionMeter::paint(juce::Graphics& g)
{
    auto b=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff111820));g.fillRoundedRectangle(b,6);g.setColour(juce::Colour(0xff536b80));g.drawRoundedRectangle(b,6,1);
    g.setColour(juce::Colours::white);g.setFont(juce::Font(juce::FontOptions(10,juce::Font::bold)));g.drawText("GAIN REDUCTION",getLocalBounds().removeFromTop(20),juce::Justification::centred);
    g.setColour(red);g.setFont(juce::Font(juce::FontOptions(22,juce::Font::bold)));g.drawText(juce::String(-reduction,1)+" dB",getLocalBounds().withTrimmedTop(18).withTrimmedBottom(12),juce::Justification::centred);
}
void HeavenLimitsAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto b=getLocalBounds().toFloat();g.fillAll(juce::Colour(0xff101820));g.setGradientFill(juce::ColourGradient(metalTop,0,0,metalDark,0,b.getBottom(),false));g.fillRoundedRectangle(b.reduced(6),10);
    g.setColour(ink);g.setFont(juce::Font(juce::FontOptions(25,juce::Font::bold)));g.drawText("HEAVEN LIMITS",215,5,470,30,juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(9,juce::Font::bold)));g.drawText("STEREO BRICKWALL MAXIMIZER  •  Powered By TILADCO",200,36,500,18,juce::Justification::centred);
    g.setColour(juce::Colour(0x8843586d));g.drawLine(18,62,882,62,1);g.drawLine(18,480,882,480,1);
    drawScrew(g,{17,17});drawScrew(g,{883,17});drawScrew(g,{17,483});drawScrew(g,{883,483});
}
void HeavenLimitsAudioProcessorEditor::resized()
{
    inputMeter.setBounds(20,78,70,335);outputMeter.setBounds(810,78,70,335);
    inputLabel.setBounds(112,75,130,16);inputGain.setBounds(118,90,120,120);inputValue.setBounds(120,210,116,16);
    ceilingLabel.setBounds(650,75,130,16);ceiling.setBounds(655,90,120,120);ceilingValue.setBounds(658,210,116,16);
    grMeter.setBounds(365,95,170,75);
    attackLabel.setBounds(100,250,85,16);attack.setBounds(100,264,85,90);attackValue.setBounds(100,354,85,16);
    releaseLabel.setBounds(188,250,85,16);release.setBounds(188,264,85,90);releaseValue.setBounds(188,354,85,16);
    lookaheadLabel.setBounds(276,250,85,16);lookahead.setBounds(276,264,85,90);lookaheadValue.setBounds(276,354,85,16);
    stereoLinkLabel.setBounds(364,250,85,16);stereoLink.setBounds(364,264,85,90);stereoLinkValue.setBounds(364,354,85,16);
    multibandLabel.setBounds(452,250,110,16);multibandAmount.setBounds(462,264,90,90);multibandValue.setBounds(462,354,90,16);
    oversamplingLabel.setBounds(560,250,100,16);oversampling.setBounds(560,268,100,24);
    ditherLabel.setBounds(560,300,100,16);dither.setBounds(560,318,100,24);noiseLabel.setBounds(560,344,100,16);noiseShaping.setBounds(565,358,90,75);noiseValue.setBounds(565,430,90,16);
    rmsLabel.setBounds(665,265,90,14);rmsValue.setBounds(665,279,90,18);lufsLabel.setBounds(665,300,90,14);lufsValue.setBounds(665,314,90,18);
    multiband.setBounds(665,345,105,24);truePeak.setBounds(665,373,105,24);bypass.setBounds(665,401,105,24);
    monitor.setBounds(665,429,105,24);aes17.setBounds(775,345,105,24);isp.setBounds(775,373,105,24);meterReset.setBounds(775,401,105,24);
}
