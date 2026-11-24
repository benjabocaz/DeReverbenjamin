#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class AnalogKnobLAF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override;
};

class DeReverbenjaminEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DeReverbenjaminEditor(DeReverbenjaminProcessor&);
    ~DeReverbenjaminEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& suffix = "");
    void drawScrew(juce::Graphics& g, float cx, float cy) const;
    void drawVuMeter(juce::Graphics& g, juce::Rectangle<float> area) const;

    DeReverbenjaminProcessor& audioProcessor;
    AnalogKnobLAF analogLAF;

    juce::Slider roomAmountSlider, focusSlider, decaySlider, smoothSlider;
    juce::Slider tailAmountSlider, sensSlider, releaseSlider, outputSlider;

    juce::Label roomAmountLabel, focusLabel, decayLabel, smoothLabel;
    juce::Label tailAmountLabel, sensLabel, releaseLabel, outputLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> roomAmountAtt, focusAtt, decayAtt, smoothAtt;
    std::unique_ptr<Attachment> tailAmountAtt, sensAtt, releaseAtt, outputAtt;

    float smoothedGr = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeReverbenjaminEditor)
};
