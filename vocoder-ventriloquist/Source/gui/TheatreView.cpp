#include "TheatreView.h"
#include "Style.h"

using namespace vqui;

namespace
{
const char* const kModeNames[] = { "ROBOT", "MONSTER", "GHOST", "ANGEL", "RADIO", "CREATURE" };
const char* const kKeys[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

void centredText (juce::Graphics& g, const juce::String& s, float cx, float baselineY, float width = 260.0f)
{
    g.drawText (s, juce::Rectangle<float> (cx - width * 0.5f, baselineY - 12.0f, width, 15.0f), juce::Justification::centredBottom, false);
}

juce::PathStrokeType stroke (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); }
}

//==============================================================================
void TheatreView::tick()
{
    const double ms = juce::Time::getMillisecondCounterHiRes();
    const double dt = lastMs > 0.0 ? juce::jlimit (0.0, 0.2, (ms - lastMs) / 1000.0) : 0.033;
    lastMs = ms;
    now += dt;

    const bool frozen = param (ParamIDs::freeze) > 0.5f;
    const int note = proc.engine.noteShown.load();
    if (! frozen)
    {
        if (note >= 0 && note != lastNote)
        {
            bubbles.push_back ({ juce::MidiMessage::getMidiNoteName (note, true, true, 3), now });
            staff.push_back ({ note, now });
        }
        lastNote = note;
        if (proc.engine.unvoicedShown.load() > 0.45f && now - lastUvBubble > 0.5)
        {
            bubbles.push_back ({ "sss", now });
            lastUvBubble = now;
        }
    }
    else if (! bubbles.empty())
        bubbles.back().born = now - 0.15;   // the frozen word hangs in the air

    const double life = 0.6 + param (ParamIDs::linger) * 0.03;
    bubbles.erase (std::remove_if (bubbles.begin(), bubbles.end(), [&] (const Bubble& b) { return now - b.born > life; }), bubbles.end());
    while (bubbles.size() > 6) bubbles.erase (bubbles.begin());
    staff.erase (std::remove_if (staff.begin(), staff.end(), [&] (const StaffNote& s) { return now - s.born > 3.0; }), staff.end());

    repaint();
}

//==============================================================================
juce::Rectangle<float> TheatreView::puppetArea() const
{
    const float k = puppetScale();
    const auto hand = handPos();
    const float px = hand.x + 4.0f + 6.0f * k, seatY = hand.y + 2.0f, hy0 = seatY - 22.0f * k - 14.0f * k;
    return { px - 40.0f, hy0 - 44.0f * k, 80.0f, floorY - hy0 + 44.0f * k };
}

juce::Rectangle<float> TheatreView::shelfCell (int m) const
{
    const float cx = shelfX + 31.0f + (float) (m % 3) * 62.0f, cy = shelfY + 42.0f + (float) (m / 3) * 80.0f;
    return { cx - 29.0f, cy - 34.0f, 58.0f, 74.0f };
}

//==============================================================================
void TheatreView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (bezel);
    g.fillRoundedRectangle (b, 18.0f);

    const auto in = b.reduced (7.0f);
    juce::ColourGradient grad (lcdTop, 0.0f, in.getY(), lcdBottom, 0.0f, in.getBottom(), false);
    grad.addColour (0.5, lcdMid);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (in, 11.0f);
    for (int i = 0; i < 12; ++i)
    {
        g.setColour (juce::Colour (0xff28321e).withAlpha (0.05f * (1.0f - (float) i / 12.0f)));
        g.drawRoundedRectangle (in.reduced ((float) i * 1.5f), 11.0f, 2.0f);
    }

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (in.toNearestInt());
    g.addTransform (juce::AffineTransform::translation (in.getX(), in.getY()));

    // labels
    g.setColour (lcdLabel);
    g.setFont (Fonts::get().spaced (11.0f, 0.12f));
    g.drawText ("THE VENTRILOQUIST SHOW", juce::Rectangle<float> (12.0f, 6.0f, 300.0f, 16.0f), juce::Justification::centredLeft);

    const float st = vq::formantFromSize (param (ParamIDs::size));
    const juce::String score = (st > 0.05f ? "+" : "") + juce::String (juce::roundToInt (st));
    g.setFont (Fonts::get().mono (26.0f));
    g.setColour (lcdInk.withAlpha (0.08f));
    g.drawText ("8888", juce::Rectangle<float> (480.0f, 2.0f, 154.0f, 30.0f), juce::Justification::centredRight);
    g.setColour (lcdInk);
    g.drawText (score, juce::Rectangle<float> (480.0f, 2.0f, 154.0f, 30.0f), juce::Justification::centredRight);
    g.setColour (lcdLabel);
    g.setFont (Fonts::get().spaced (11.0f, 0.12f));
    g.drawText ("FORMANT st", juce::Rectangle<float> (380.0f, 8.0f, 160.0f, 16.0f), juce::Justification::centredRight);

    drawStage (g);
    drawInsider (g, insiderX, floorY, insiderS, "M18 17 L30 22 L34 19 M18 17 L9 24 M18 27 L12 41 H7 M18 27 L24 41 H29", lcdInk);
    juce::Point<float> mouth;
    drawPuppet (g, mouth);
    drawStaff (g);
    drawBubbles (g, mouth);
    drawShelf (g);
    drawReadouts (g);
}

void TheatreView::drawStage (juce::Graphics& g)
{
    juce::Path p;
    for (float x = 0.0f; x < 646.0f; x += 46.0f) { p.startNewSubPath (x, 30.0f); p.quadraticTo (x + 23.0f, 50.0f, x + 46.0f, 30.0f); }
    g.setColour (lcdInk.withAlpha (0.6f)); g.strokePath (p, stroke (2.5f));
    g.setColour (lcdInk.withAlpha (0.85f)); g.drawLine (0.0f, 30.0f, 646.0f, 30.0f, 3.0f);

    juce::Path folds;
    for (int k = 0; k < 4; ++k)
    {
        const float o = (float) k * 9.0f;
        folds.startNewSubPath (8.0f + o, 30.0f);  folds.quadraticTo (14.0f + o, 160.0f, 6.0f + o, floorY);
        folds.startNewSubPath (638.0f - o, 30.0f); folds.quadraticTo (632.0f - o, 160.0f, 640.0f - o, floorY);
    }
    g.setColour (lcdInk.withAlpha (0.32f)); g.strokePath (folds, stroke (2.0f));

    g.setColour (lcdInk.withAlpha (0.75f)); g.drawLine (0.0f, floorY, 646.0f, floorY, 2.5f);
    g.setColour (lcdInk.withAlpha (0.25f));
    for (float x = 20.0f; x < 640.0f; x += 34.0f) g.fillRect (x, floorY + 6.0f, 22.0f, 2.0f);

    // footlights: one lamp per vocoder band, lit by that band's energy
    const int nb = proc.engine.numBandsActive.load();
    for (int i = 0; i < nb; ++i)
    {
        const float lvl = juce::jlimit (0.0f, 1.0f, proc.engine.bandLevel[(size_t) i].load() * 9.0f);
        const float x = 40.0f + (566.0f * (float) i) / (float) juce::jmax (1, nb - 1);
        g.setColour (lcdInk.withAlpha (0.10f + 0.9f * lvl));
        g.fillEllipse (x - 3.5f, floorY + 19.0f, 7.0f, 7.0f);
    }
}

void TheatreView::drawPuppet (juce::Graphics& g, juce::Point<float>& mouthOut)
{
    const float k = puppetScale();
    const auto hand = handPos();
    const float px = hand.x + 4.0f + 6.0f * k, seatY = hand.y + 2.0f, neck = seatY - 22.0f * k;

    juce::Path body;
    body.startNewSubPath (px, seatY); body.lineTo (px, neck);
    body.startNewSubPath (px, seatY - 16.0f * k); body.lineTo (px - 9.0f * k, seatY - 4.0f * k);
    body.startNewSubPath (px, seatY - 16.0f * k); body.lineTo (px + 9.0f * k, seatY - 4.0f * k);
    body.startNewSubPath (px, seatY); body.lineTo (px + 10.0f * k, seatY + 4.0f * k); body.lineTo (px + 10.0f * k, seatY + 20.0f * k);
    body.startNewSubPath (px, seatY); body.lineTo (px - 4.0f * k, seatY + 6.0f * k); body.lineTo (px, seatY + 22.0f * k);
    g.setColour (lcdInk);
    g.strokePath (body, stroke (juce::jmax (2.5f, 4.5f * k)));

    const float hy0 = neck - 14.0f * k;
    drawHead (g, (int) param (ParamIDs::mode), px, hy0, k, proc.engine.mouth.load(), lcdInk);
    mouthOut = { px + 16.0f * k, hy0 + 6.0f * k };

    if (param (ParamIDs::freeze) > 0.5f)
    {
        g.setFont (Fonts::get().mono (12.0f));
        centredText (g, "FROZEN", px, hy0 - 40.0f * k);
    }
}

void TheatreView::drawHead (juce::Graphics& g, int m, float hx, float hy, float k, float open, juce::Colour c)
{
    const float w = 15.0f * k, h = 12.0f * k, jw = open * 9.0f * k, sw = juce::jmax (1.8f, 3.0f * k);
    g.setColour (c);
    juce::Path p;
    auto dot = [&] (float x, float y, float r) { g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f); };

    switch (m)
    {
        case 0: // robot
            p.addRectangle (hx - w, hy - h, 2.0f * w, h + 2.0f);
            p.addRectangle (hx - w, hy + 4.0f + jw, 2.0f * w, 8.0f * k);
            p.startNewSubPath (hx, hy - h); p.lineTo (hx, hy - h - 8.0f * k);
            g.strokePath (p, stroke (sw));
            g.fillRect (hx - w * 0.55f, hy - h * 0.55f, 5.0f * k, 5.0f * k);
            g.fillRect (hx + w * 0.55f - 5.0f * k, hy - h * 0.55f, 5.0f * k, 5.0f * k);
            dot (hx, hy - h - 9.0f * k, 2.6f * k);
            break;
        case 1: // monster
            p.startNewSubPath (hx - w, hy + 2.0f);
            p.quadraticTo (hx - w, hy - h - 4.0f, hx, hy - h - 4.0f);
            p.quadraticTo (hx + w, hy - h - 4.0f, hx + w, hy + 2.0f);
            p.startNewSubPath (hx - w * 0.7f, hy - h); p.lineTo (hx - w * 0.7f - 6.0f * k, hy - h - 10.0f * k); p.lineTo (hx - w * 0.7f + 4.0f * k, hy - h - 5.0f * k);
            p.startNewSubPath (hx + w * 0.7f, hy - h); p.lineTo (hx + w * 0.7f + 6.0f * k, hy - h - 10.0f * k); p.lineTo (hx + w * 0.7f - 4.0f * k, hy - h - 5.0f * k);
            p.startNewSubPath (hx - w, hy + 4.0f + jw);
            p.lineTo (hx - w * 0.5f, hy + jw); p.lineTo (hx, hy + 4.0f + jw); p.lineTo (hx + w * 0.5f, hy + jw); p.lineTo (hx + w, hy + 4.0f + jw);
            p.quadraticTo (hx, hy + 16.0f * k + jw, hx - w, hy + 4.0f + jw);
            g.strokePath (p, stroke (sw));
            dot (hx - w * 0.4f, hy - h * 0.35f, 2.4f * k); dot (hx + w * 0.4f, hy - h * 0.35f, 2.4f * k);
            break;
        case 2: // ghost
        {
            p.startNewSubPath (hx - w, hy + 14.0f * k);
            p.lineTo (hx - w, hy - 2.0f);
            p.quadraticTo (hx - w, hy - h - 6.0f, hx, hy - h - 6.0f);
            p.quadraticTo (hx + w, hy - h - 6.0f, hx + w, hy - 2.0f);
            p.lineTo (hx + w, hy + 14.0f * k);
            float x = hx + w;
            for (int i = 0; i < 4; ++i) { const float cy = hy + 14.0f * k + (i % 2 ? 5.0f : -5.0f) * k; p.quadraticTo (x - w * 0.25f, cy, x - w * 0.5f, hy + 14.0f * k); x -= w * 0.5f; }
            g.strokePath (p, stroke (sw));
            juce::Path mouthP; mouthP.addEllipse (hx - 4.0f * k, hy + 2.0f - (2.0f + jw * 0.6f), 8.0f * k, 2.0f * (2.0f + jw * 0.6f));
            g.strokePath (mouthP, stroke (sw * 0.8f));
            dot (hx - w * 0.4f, hy - h * 0.4f, 3.0f * k); dot (hx + w * 0.4f, hy - h * 0.4f, 3.0f * k);
            break;
        }
        case 3: // angel
            p.addEllipse (hx - w * 0.85f, hy - 2.0f - w * 0.85f, w * 1.7f, w * 1.7f);
            g.strokePath (p, stroke (sw));
            {
                juce::Path halo; halo.addEllipse (hx - w * 0.7f, hy - h - 10.0f * k - 3.0f * k, w * 1.4f, 6.0f * k);
                g.strokePath (halo, stroke (sw * 0.8f));
                juce::Path smile; smile.startNewSubPath (hx - w * 0.6f, hy + 8.0f * k + jw); smile.quadraticTo (hx, hy + 16.0f * k + jw, hx + w * 0.6f, hy + 8.0f * k + jw);
                g.strokePath (smile, stroke (sw));
            }
            dot (hx - w * 0.35f, hy - 4.0f * k, 2.2f * k); dot (hx + w * 0.35f, hy - 4.0f * k, 2.2f * k);
            break;
        case 4: // radio
            p.addRectangle (hx - w, hy - h, 2.0f * w, 2.0f * h + 6.0f * k);
            p.startNewSubPath (hx + w * 0.4f, hy - h); p.lineTo (hx + w * 0.4f + 8.0f * k, hy - h - 14.0f * k);
            g.strokePath (p, stroke (sw));
            {
                const float r = 5.0f * k + open * 4.0f * k;
                juce::Path cone; cone.addEllipse (hx - r, hy + 2.0f - r, 2.0f * r, 2.0f * r);
                g.strokePath (cone, stroke (sw * 0.8f));
            }
            dot (hx, hy + 2.0f, 2.0f * k);
            g.fillRect (hx - w * 0.8f, hy - h + 3.0f * k, 8.0f * k, 3.0f * k);
            break;
        default: // creature
            p.startNewSubPath (hx - w, hy + 2.0f);
            p.quadraticTo (hx - w * 1.2f, hy - h - 8.0f, hx, hy - h - 2.0f);
            p.quadraticTo (hx + w * 1.2f, hy - h - 8.0f, hx + w, hy + 2.0f);
            p.startNewSubPath (hx - w, hy + 4.0f + jw);
            p.quadraticTo (hx, hy + 18.0f * k + jw, hx + w, hy + 4.0f + jw);
            g.strokePath (p, stroke (sw));
            dot (hx - w * 0.45f, hy - h * 0.3f, 2.2f * k); dot (hx, hy - h * 0.55f, 2.6f * k); dot (hx + w * 0.45f, hy - h * 0.3f, 2.2f * k);
            break;
    }
}

void TheatreView::drawBubbles (juce::Graphics& g, juce::Point<float> mouth)
{
    const double life = 0.6 + param (ParamIDs::linger) * 0.03;
    g.setFont (Fonts::get().mono (12.0f));
    for (size_t i = 0; i < bubbles.size(); ++i)
    {
        const auto& b = bubbles[i];
        const float age = (float) (now - b.born);
        const float a = juce::jlimit (0.0f, 1.0f, 1.0f - age / (float) life);
        const float bx = mouth.x + 30.0f + age * 60.0f, by = mouth.y - 40.0f - age * 70.0f;
        const float bw = 10.0f + (float) b.text.length() * 5.5f;
        g.setColour (lcdInk.withAlpha (a));
        g.drawEllipse (bx - bw, by - 13.0f, bw * 2.0f, 26.0f, 2.0f);
        centredText (g, b.text, bx, by + 5.0f, 80.0f);
        if (i + 1 == bubbles.size())
        {
            g.setColour (lcdInk.withAlpha (a * 0.7f));
            g.drawLine (bx - bw * 0.6f, by + 10.0f, mouth.x + 6.0f, mouth.y - 4.0f, 1.6f);
        }
    }
}

void TheatreView::drawStaff (juce::Graphics& g)
{
    const int pitchMode = (int) param (ParamIDs::pitch);
    if (pitchMode == 0) return;
    const float sx0 = 300.0f, sy0 = 214.0f;
    g.setColour (lcdInk.withAlpha (0.4f));
    for (int l = 0; l < 5; ++l) g.drawLine (sx0, sy0 + (float) l * 7.0f, sx0 + 120.0f, sy0 + (float) l * 7.0f, 1.2f);
    for (size_t i = 0; i < staff.size(); ++i)
    {
        const float age = (float) (now - staff[i].born);
        const float x = sx0 + 110.0f - age * 62.0f;
        if (x < sx0) continue;
        const float y = sy0 + 28.0f - (float) (staff[i].midi % 12) / 11.0f * 28.0f;
        g.setColour (lcdInk.withAlpha (i + 1 == staff.size() ? 1.0f : 0.5f));
        g.fillEllipse (x - 3.4f, y - 3.4f, 6.8f, 6.8f);
    }
    g.setColour (lcdInk.withAlpha (0.7f));
    g.setFont (Fonts::get().mono (10.0f));
    centredText (g, juce::String (kKeys[juce::jlimit (0, 11, (int) param (ParamIDs::key))]) + (pitchMode == 1 ? " MAJOR" : " MINOR"), sx0 + 60.0f, sy0 + 50.0f);
}

void TheatreView::drawShelf (juce::Graphics& g)
{
    const int mode = (int) param (ParamIDs::mode);
    g.setColour (lcdInk.withAlpha (0.7f));
    g.drawLine (shelfX, shelfY + 83.0f, shelfX + 186.0f, shelfY + 83.0f, 2.5f);
    g.drawLine (shelfX, shelfY + 163.0f, shelfX + 186.0f, shelfY + 163.0f, 2.5f);
    for (int m = 0; m < vq::kNumModes; ++m)
    {
        const auto cell = shelfCell (m);
        const float cx = cell.getCentreX(), cy = cell.getY() + 34.0f;
        const auto c = lcdInk.withAlpha (m == mode ? 1.0f : 0.45f);
        drawHead (g, m, cx, cy, 0.75f, 0.0f, c);
        g.setColour (c);
        g.setFont (Fonts::get().mono (8.5f));
        centredText (g, kModeNames[m], cx, cy + 32.0f, 60.0f);
        if (m == mode) { g.setColour (lcdInk.withAlpha (0.9f)); g.drawRect (cell, 2.0f); }
    }

    const bool frozen = param (ParamIDs::freeze) > 0.5f;
    auto button = [&] (juce::Rectangle<float> r, const juce::String& label, bool on)
    {
        if (on) { g.setColour (lcdInk.withAlpha (0.18f)); g.fillRect (r.reduced (3.0f)); }
        g.setColour (lcdInk); g.drawRect (r, 2.0f);
        g.setFont (Fonts::get().mono (11.0f));
        g.drawText (label, r, juce::Justification::centred);
    };
    button (freezeButton(), frozen ? juce::String::fromUTF8 (u8"FROZEN · TAP TO RELEASE") : juce::String ("FREEZE"), frozen);
    button (shakeButton(), "SHAKE THE BOX", false);
}

void TheatreView::drawReadouts (juce::Graphics& g)
{
    const int mode = (int) param (ParamIDs::mode);
    const float k = puppetScale(), st = vq::formantFromSize (param (ParamIDs::size));
    const juce::String txt = juce::String ("PUPPET ") + kModeNames[mode]
                           + juce::String::fromUTF8 (u8" · SIZE ×") + juce::String (k, 2)
                           + juce::String::fromUTF8 (u8" · FORMANT ") + (st > 0.05f ? "+" : "") + juce::String (juce::roundToInt (st)) + " st";
    g.setColour (lcdInk.withAlpha (0.9f));
    g.setFont (Fonts::get().mono (12.0f));
    centredText (g, txt, 323.0f, 392.0f, 600.0f);
    g.setColour (lcdInk.withAlpha (0.5f));
    g.setFont (Fonts::get().mono (11.0f));
    centredText (g, juce::String::fromUTF8 (u8"↕ drag the puppet to resize it: bigger puppet, deeper voice"), 323.0f, 412.0f, 600.0f);
}

//==============================================================================
void TheatreView::mouseMove (const juce::MouseEvent& e)
{
    const auto p = inner (e);
    bool hand = freezeButton().contains (p) || shakeButton().contains (p);
    for (int m = 0; m < vq::kNumModes; ++m) hand = hand || shelfCell (m).contains (p);
    if (puppetArea().contains (p))  setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else if (hand)                  setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else                            setMouseCursor (juce::MouseCursor::NormalCursor);
}

void TheatreView::mouseDown (const juce::MouseEvent& e)
{
    const auto p = inner (e);
    for (int m = 0; m < vq::kNumModes; ++m)
        if (shelfCell (m).contains (p)) { proc.setParam (ParamIDs::mode, (float) m); repaint(); return; }
    if (freezeButton().contains (p)) { proc.setParam (ParamIDs::freeze, param (ParamIDs::freeze) > 0.5f ? 0.0f : 1.0f); repaint(); return; }
    if (shakeButton().contains (p))  { proc.shakeTheBox(); repaint(); return; }
    if (puppetArea().contains (p))
    {
        sizeParam = proc.apvts.getParameter (ParamIDs::size);
        if (sizeParam != nullptr)
        {
            draggingPuppet = true;
            dragStartY = e.position.y;
            sizeAtDragStart = param (ParamIDs::size);
            sizeParam->beginChangeGesture();
        }
    }
}

void TheatreView::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingPuppet || sizeParam == nullptr) return;
    const float v = juce::jlimit (0.0f, 100.0f, sizeAtDragStart + (dragStartY - e.position.y) * 0.5f);
    sizeParam->setValueNotifyingHost (sizeParam->convertTo0to1 (v));
    repaint();
}

void TheatreView::mouseUp (const juce::MouseEvent&)
{
    if (draggingPuppet && sizeParam != nullptr) sizeParam->endChangeGesture();
    draggingPuppet = false;
}
