#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

/** The LCD: the ventriloquist show. Everything it draws comes from the engine's real state. */
class TheatreView : public juce::Component
{
public:
    explicit TheatreView (VentriloquistProcessor& p) : proc (p) { setOpaque (false); }

    void tick();   // ~30 Hz from the editor

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct Bubble { juce::String text; double born; };
    struct StaffNote { int midi; double born; };

    float param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    juce::Point<float> inner (const juce::MouseEvent& e) const { return e.position - juce::Point<float> (7.0f, 7.0f); }

    // geometry (inner LCD coordinates, 646 x 438)
    static constexpr float floorY = 300.0f, insiderX = 168.0f, insiderS = 2.4f;
    static constexpr float shelfX = 440.0f, shelfY = 58.0f;
    juce::Point<float> handPos() const { return { insiderX + (34.0f - 18.0f) * insiderS, floorY - (44.0f - 19.0f) * insiderS }; }
    float puppetScale() const { return 0.6f + param (ParamIDs::size) / 100.0f; }
    juce::Rectangle<float> puppetArea() const;
    juce::Rectangle<float> shelfCell (int m) const;
    juce::Rectangle<float> freezeButton() const { return { shelfX, 228.0f, 186.0f, 28.0f }; }
    juce::Rectangle<float> shakeButton() const  { return { shelfX, 264.0f, 186.0f, 28.0f }; }

    void drawStage (juce::Graphics&);
    void drawPuppet (juce::Graphics&, juce::Point<float>& mouthOut);
    void drawHead (juce::Graphics&, int mode, float hx, float hy, float k, float open, juce::Colour c);
    void drawShelf (juce::Graphics&);
    void drawBubbles (juce::Graphics&, juce::Point<float> mouth);
    void drawStaff (juce::Graphics&);
    void drawReadouts (juce::Graphics&);

    VentriloquistProcessor& proc;
    double now = 0.0, lastMs = 0.0, lastUvBubble = 0.0;
    std::vector<Bubble> bubbles;
    std::vector<StaffNote> staff;
    int lastNote = -1;
    bool draggingPuppet = false; float dragStartY = 0.0f, sizeAtDragStart = 50.0f;
    juce::RangedAudioParameter* sizeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TheatreView)
};
