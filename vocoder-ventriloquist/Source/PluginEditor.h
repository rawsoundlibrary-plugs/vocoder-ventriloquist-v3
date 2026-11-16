#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/TheatreView.h"

//==============================================================================
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
};

/** Classic knob: name above, cream cap, arc, value below. Vertical drag, double-click resets. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& label, bool darkArc);
    ~Knob() override { slider.setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String name;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    KnobLookAndFeel lnf;
};

/** Pill-shaped segmented selector bound to a choice parameter. */
class SegmentedControl : public juce::Component
{
public:
    SegmentedControl (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& label, juce::StringArray options);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    int preferredWidth() const;

private:
    juce::Rectangle<float> segmentBounds (int i) const;
    juce::String label;
    juce::StringArray options;
    int selected = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

/** ‹ [Puppets · preset name ▾] › */
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (VentriloquistProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

private:
    VentriloquistProcessor& proc;
};

//==============================================================================
class VentriloquistEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VentriloquistEditor (VentriloquistProcessor&);
    ~VentriloquistEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawMeter (juce::Graphics&, juce::Rectangle<float> r, float level, const juce::String& label);

    VentriloquistProcessor& proc;
    TheatreView theatre;
    PresetBar presets;
    SegmentedControl carrier, pitch;
    Knob linger, whisper, chorus, gate, mix, output;
    juce::Image grain;
    float meterIn = 0.0f, meterOut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VentriloquistEditor)
};
