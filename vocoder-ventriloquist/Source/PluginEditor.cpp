#include "PluginEditor.h"
#include "Presets.h"
#include "gui/Style.h"

using namespace vqui;

//==============================================================================
void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float cx = b.getCentreX(), cy = b.getCentreY(), radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 1.0f;
    const float scale = radius / 22.0f;
    const bool dark = (bool) s.getProperties().getWithDefault ("darkArc", false);

    // recess
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x55000000), cx, cy + 2.0f, juce::Colours::transparentBlack, cx, cy + radius * 0.9f, true));
    g.fillEllipse (cx - radius * 0.9f, cy - radius * 0.9f, radius * 1.8f, radius * 1.8f);

    // track and value arcs
    const float arcR = radius * 0.78f;
    juce::Path track; track.addCentredArc (cx, cy, arcR, arcR, 0.0f, start, end, true);
    g.setColour (brown.withAlpha (0.22f));
    g.strokePath (track, juce::PathStrokeType (2.4f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float angle = start + pos * (end - start);
    juce::Path val; val.addCentredArc (cx, cy, arcR, arcR, 0.0f, start, juce::jmax (start + 0.01f, angle), true);
    g.setColour (dark ? juce::Colour (0xff120a04) : brown);
    g.strokePath (val, juce::PathStrokeType (2.4f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // cap with drop shadow and soft relief
    const float capR = radius * 0.6f;
    g.setColour (juce::Colour (0x66000000));
    g.fillEllipse (cx - capR, cy - capR + 3.0f * scale, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colour (0x26000000));
    g.fillEllipse (cx - capR * 1.05f, cy - capR + 6.0f * scale, capR * 2.1f, capR * 2.1f);
    juce::ColourGradient cap (juce::Colours::white, cx - capR * 0.25f, cy - capR * 0.45f, juce::Colour (0xffb89c79), cx + capR * 0.5f, cy + capR, true);
    cap.addColour (0.45, juce::Colour (0xfff7ecdf));
    cap.addColour (0.8, juce::Colour (0xffd9c3a6));
    g.setGradientFill (cap);
    g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

    // indicator
    const float ir = radius * 0.45f;
    g.setColour (juce::Colour (0xff26282b));
    g.fillEllipse (cx + std::sin (angle) * ir - 2.0f * scale, cy - std::cos (angle) * ir - 2.0f * scale, 4.0f * scale, 4.0f * scale);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& label, bool darkArc) : name (label)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (200);
    slider.setLookAndFeel (&lnf);
    slider.getProperties().set ("darkArc", darkArc);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    if (auto* p = s.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
    slider.setTitle (label);
}

void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (16).withHeight (56).withSizeKeepingCentre (56, 56)); }

void Knob::paint (juce::Graphics& g)
{
    g.setColour (brown);
    g.setFont (Fonts::get().spaced (12.0f, 0.05f));
    g.drawText (name, getLocalBounds().withHeight (15), juce::Justification::centred);
    g.setFont (Fonts::get().sans (12.5f));
    g.drawText (slider.getTextFromValue (slider.getValue()), getLocalBounds().withTrimmedTop (74).withHeight (16), juce::Justification::centred);
}

//==============================================================================
SegmentedControl::SegmentedControl (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& l, juce::StringArray opts)
    : label (l), options (std::move (opts))
{
    if (auto* p = s.getParameter (paramId))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { selected = juce::roundToInt (v); repaint(); });
        attachment->sendInitialUpdate();
    }
    setTitle (l);
}

int SegmentedControl::preferredWidth() const
{
    int w = 0;
    for (auto& o : options) w += 22 + (int) (7.2f * (float) o.length());
    return w + 2;
}

juce::Rectangle<float> SegmentedControl::segmentBounds (int i) const
{
    float x = 1.0f;
    for (int k = 0; k < i; ++k) x += 22.0f + 7.2f * (float) options[k].length();
    return { x, 19.0f, 22.0f + 7.2f * (float) options[i].length(), 30.0f };
}

void SegmentedControl::paint (juce::Graphics& g)
{
    g.setColour (brown);
    g.setFont (Fonts::get().spaced (12.0f, 0.05f));
    g.drawText (label, juce::Rectangle<float> (0.0f, 0.0f, (float) getWidth(), 15.0f), juce::Justification::centredLeft);

    const auto pill = juce::Rectangle<float> (1.0f, 19.0f, (float) preferredWidth() - 2.0f, 30.0f);
    g.setColour (juce::Colour (0x24000000));
    g.fillRoundedRectangle (pill.translated (0.0f, 2.0f), 15.0f);
    juce::Path clip; clip.addRoundedRectangle (pill, 15.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);
        for (int i = 0; i < options.size(); ++i)
        {
            const auto r = segmentBounds (i);
            g.setColour (i == selected ? brown : panel);
            g.fillRect (r);
            g.setColour (i == selected ? cream : brown);
            g.setFont (Fonts::get().sans (13.0f));
            g.drawText (options[i], r, juce::Justification::centred);
        }
    }
    g.setColour (line);
    g.drawRoundedRectangle (pill, 15.0f, 1.0f);
}

void SegmentedControl::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < options.size(); ++i)
        if (segmentBounds (i).contains (e.position) && attachment != nullptr)
            attachment->setValueAsCompleteGesture ((float) i);
}

//==============================================================================
void PresetBar::paint (juce::Graphics& g)
{
    const float h = (float) getHeight(), w = (float) getWidth();
    auto roundButton = [&] (float x, const juce::String& glyph)
    {
        g.setColour (juce::Colour (0x40000000)); g.fillEllipse (x, 3.0f, h, h);
        juce::ColourGradient grad (juce::Colour (0xff6a4a32), x + h * 0.38f, h * 0.3f, juce::Colours::black, x + h, h, true);
        grad.addColour (0.55, brown);
        g.setGradientFill (grad); g.fillEllipse (x, 0.0f, h, h);
        g.setColour (orangeTop); g.setFont (Fonts::get().sans (18.0f));
        g.drawText (glyph, juce::Rectangle<float> (x, -1.0f, h, h), juce::Justification::centred);
    };
    roundButton (0.0f, juce::String::fromUTF8 (u8"‹"));
    roundButton (w - h, juce::String::fromUTF8 (u8"›"));

    const auto pill = juce::Rectangle<float> (h + 6.0f, 0.0f, w - 2.0f * h - 12.0f, h);
    g.setColour (juce::Colour (0x26000000)); g.fillRoundedRectangle (pill.translated (0.0f, 2.0f), h * 0.5f);
    g.setColour (panel); g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (line); g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);
    g.setColour (brown.withAlpha (0.7f)); g.setFont (Fonts::get().sans (12.5f));
    g.drawText ("Puppets", pill.reduced (14.0f, 0.0f), juce::Justification::centredLeft);
    g.setColour (brown); g.setFont (Fonts::get().sans (15.5f));
    g.drawText (proc.getProgramName (proc.getCurrentProgram()), pill.reduced (60.0f, 0.0f).translated (14.0f, 0.0f), juce::Justification::centred);
    g.setFont (Fonts::get().sans (12.0f));
    g.drawText (juce::String::fromUTF8 (u8"▾"), pill.reduced (14.0f, 0.0f), juce::Justification::centredRight);
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    const int n = proc.getNumPrograms(), cur = proc.getCurrentProgram();
    if (e.position.x < (float) getHeight())               { proc.setCurrentProgram ((cur + n - 1) % n); repaint(); return; }
    if (e.position.x > (float) (getWidth() - getHeight())) { proc.setCurrentProgram ((cur + 1) % n); repaint(); return; }

    juce::PopupMenu menu;
    for (int i = 0; i < n; ++i) menu.addItem (i + 1, proc.getProgramName (i), true, i == cur);
    juce::Component::SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
    {
        if (safe != nullptr && result > 0) { safe->proc.setCurrentProgram (result - 1); safe->repaint(); }
    });
}

//==============================================================================
VentriloquistEditor::VentriloquistEditor (VentriloquistProcessor& p)
    : AudioProcessorEditor (&p), proc (p), theatre (p), presets (p),
      carrier (p.apvts, ParamIDs::carrier, "carrier", { "synth", "noise", "sidechain" }),
      pitch   (p.apvts, ParamIDs::pitch,   "pitch",   { "free", "major", "minor" }),
      linger  (p.apvts, ParamIDs::linger,  "linger",  false),
      whisper (p.apvts, ParamIDs::whisper, "whisper", false),
      chorus  (p.apvts, ParamIDs::chorus,  "chorus",  false),
      gate    (p.apvts, ParamIDs::gate,    "gate",    false),
      mix     (p.apvts, ParamIDs::mix,     "mix",     false),
      output  (p.apvts, ParamIDs::output,  "output",  true)
{
    grain = makeGrain (720, 720);
    for (auto* c : std::initializer_list<juce::Component*> { &theatre, &presets, &carrier, &pitch, &linger, &whisper, &chorus, &gate, &mix, &output })
        addAndMakeVisible (c);
    setSize (720, 720);
    startTimerHz (30);
}

VentriloquistEditor::~VentriloquistEditor() { stopTimer(); }

void VentriloquistEditor::resized()
{
    presets.setBounds (434, 54, 256, 34);
    theatre.setBounds (30, 104, 660, 452);
    carrier.setBounds (30, 580, carrier.preferredWidth(), 50);
    pitch.setBounds (30, 638, pitch.preferredWidth(), 50);
    int x = 236;
    for (auto* k : { &linger, &whisper, &chorus, &gate, &mix, &output }) { k->setBounds (x, 590, 66, 92); x += 66; }
}

void VentriloquistEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float level, const juce::String& label)
{
    auto bar = r.withTrimmedBottom (16.0f);
    g.setColour (juce::Colour (0x1a000000)); g.fillRoundedRectangle (bar, 6.0f);
    g.setColour (line); g.drawRoundedRectangle (bar, 6.0f, 1.0f);
    const float hgt = bar.getHeight() * (0.1f + 0.85f * juce::jlimit (0.0f, 1.0f, level));
    g.setColour (brown); g.fillRoundedRectangle (bar.reduced (1.0f).removeFromBottom (hgt), 4.0f);
    g.setFont (Fonts::get().spaced (11.0f, 0.05f));
    g.drawText (label, r.removeFromBottom (14.0f).expanded (8.0f, 0.0f), juce::Justification::centred);
}

void VentriloquistEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (orangeTop, 0.0f, 0.0f, orangeBottom, 0.0f, 720.0f, false);
    bg.addColour (0.55, orangeMid);
    g.setGradientFill (bg);
    g.fillAll();
    g.drawImageAt (grain, 0, 0);

    // brand
    g.setColour (brown);
    g.fillEllipse (31.0f, 11.0f, 14.4f, 14.4f);
    drawInsider (g, 38.2f, 22.6f, 0.21f, "M18 17 L9 23 M18 17 L28 12 M18 27 L10 41 H5 M18 27 L26 39 H31", orangeMid);
    g.setColour (brown);
    g.setFont (Fonts::get().spaced (10.5f, 0.23f));
    g.drawText ("THE INSIDER", juce::Rectangle<float> (52.0f, 10.0f, 200.0f, 16.0f), juce::Justification::centredLeft);

    // title
    g.setFont (Fonts::get().sans (20.0f));
    g.drawText ("vocoder", juce::Rectangle<float> (84.0f, 34.0f, 200.0f, 24.0f), juce::Justification::centredLeft);
    const auto big = Fonts::get().sans (48.0f);
    g.setFont (big);
    g.drawText ("ventriloquist_", juce::Rectangle<float> (30.0f, 50.0f, 400.0f, 52.0f), juce::Justification::centredLeft);
    juce::GlyphArrangement ga; ga.addLineOfText (big, "ventriloquist_", 0.0f, 0.0f);
    const float titleW = ga.getBoundingBox (0, -1, true).getWidth();

    // logo: the insider with his puppet
    const float lx = juce::jmin (30.0f + titleW + 6.0f, 370.0f), ly = 42.0f, ls = 1.1f;
    const auto lt = juce::AffineTransform::scale (ls).translated (lx + 2.0f * ls, ly + 4.0f * ls);
    drawInsider (g, lx + 20.0f * ls, ly + 48.0f * ls, ls, "M18 17 L30 22 L33 19 M18 17 L9 24 M18 27 L12 41 H7 M18 27 L24 41 H29", brown);
    g.setColour (brown);
    strokeSvg (g, "M35 8 h10 v7 h-10 Z M35 17 h10 v4 h-10 Z M40 8 v-4", 2.4f, ls, lt);
    strokeSvg (g, "M40 21 V30 M36 25 h8", 3.0f, ls, lt);

    drawMeter (g, { 648.0f, 598.0f, 10.0f, 100.0f }, meterIn, "IN");
    drawMeter (g, { 672.0f, 598.0f, 10.0f, 100.0f }, meterOut, "OUT");
}

void VentriloquistEditor::timerCallback()
{
    theatre.tick();
    meterIn  = juce::jmax (proc.engine.inLevel.load(),  meterIn * 0.85f);
    meterOut = juce::jmax (proc.engine.outLevel.load(), meterOut * 0.85f);
    repaint (640, 590, 60, 115);
    presets.repaint();
}
