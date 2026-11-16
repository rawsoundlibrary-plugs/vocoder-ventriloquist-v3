#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#if VQ_HAS_FONTS
 #include "BinaryData.h"
#endif

namespace vqui
{
// palette — vocoder ventriloquist_ (orange, a wink to the old orange vocoder boxes)
inline const juce::Colour orangeTop    { 0xfff39433 };
inline const juce::Colour orangeMid    { 0xffe57b21 };
inline const juce::Colour orangeBottom { 0xffd06815 };
inline const juce::Colour brown        { 0xff2a1a0e };   // text, accents
inline const juce::Colour cream        { 0xfff7e7d0 };
inline const juce::Colour panel        { 0x66fff0dc };   // translucent cream
inline const juce::Colour line         { 0x4d2a1a0e };
inline const juce::Colour lcdTop       { 0xffb4bda8 };
inline const juce::Colour lcdMid       { 0xffc1c9b4 };
inline const juce::Colour lcdBottom    { 0xffaeb7a2 };
inline const juce::Colour lcdInk       { 0xff1b1f1a };
inline const juce::Colour lcdLabel     { 0xff3f4638 };
inline const juce::Colour bezel        { 0xff23252a };

/** Jost + Share Tech Mono if they were embedded (Resources/fonts), otherwise system fonts. */
class Fonts
{
public:
    static Fonts& get() { static Fonts f; return f; }

    juce::Font sans (float h) const
    {
        return sansFace != nullptr ? juce::Font (juce::FontOptions (sansFace).withHeight (h))
                                   : juce::Font (juce::FontOptions().withHeight (h));
    }
    juce::Font mono (float h) const
    {
        return monoFace != nullptr ? juce::Font (juce::FontOptions (monoFace).withHeight (h))
                                   : juce::Font (juce::FontOptions().withName (juce::Font::getDefaultMonospacedFontName()).withHeight (h));
    }
    juce::Font spaced (float h, float kerning) const { return sans (h).withExtraKerningFactor (kerning); }

private:
    Fonts()
    {
       #if VQ_HAS_FONTS
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String name (BinaryData::namedResourceList[i]);
            int size = 0;
            if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
            {
                if (name.containsIgnoreCase ("jost"))      sansFace = juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
                if (name.containsIgnoreCase ("sharetech")) monoFace = juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
            }
        }
       #endif
    }
    juce::Typeface::Ptr sansFace, monoFace;
};

/** The insider himself, drawn from the same path data as the previews (36 x 44 box, feet at 18,44). */
inline void strokeSvg (juce::Graphics& g, const juce::String& svg, float width, float scale, const juce::AffineTransform& t)
{
    auto p = juce::Drawable::parseSVGPath (svg);
    p.applyTransform (t);
    g.strokePath (p, juce::PathStrokeType (width * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline void drawInsider (juce::Graphics& g, float x, float y, float s, const juce::String& limbs, juce::Colour c, float flip = 1.0f)
{
    const auto t = juce::AffineTransform::scale (s * flip, s).translated (x - 18.0f * s * flip, y - 44.0f * s);
    g.setColour (c);
    juce::Path head; head.addEllipse (12.0f, 1.0f, 12.0f, 12.0f); head.applyTransform (t);
    g.fillPath (head);
    strokeSvg (g, "M12 3.5 h13", 3.0f, s, t);
    strokeSvg (g, "M18 14 V27", 7.0f, s, t);
    strokeSvg (g, limbs, 5.0f, s, t);
}

/** Fine plastic grain over the orange body, generated once. */
inline juce::Image makeGrain (int w, int h)
{
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Random r (7);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const float v = r.nextFloat();
            if (v > 0.93f)      bd.setPixelColour (x, y, juce::Colours::white.withAlpha (0.10f + 0.12f * r.nextFloat()));
            else if (v < 0.06f) bd.setPixelColour (x, y, juce::Colour (0xff3a2010).withAlpha (0.08f + 0.10f * r.nextFloat()));
        }
    return img;
}
} // namespace vqui
