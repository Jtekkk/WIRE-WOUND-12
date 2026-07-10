/*
    FLUXCORE·12 — Theme.h
    Shared palette, metrics and a couple of drawing helpers. Dark and light
    skins select the same semantic colours through Theme::get().
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fluxcore::theme
{
struct Palette
{
    juce::Colour bg, panel, panelEdge, text, textDim, accent, accent2, meter, warn, grid, curve;
};

inline Palette dark()
{
    Palette p;
    p.bg        = juce::Colour (0xff14161a);
    p.panel     = juce::Colour (0xff1c2026);
    p.panelEdge = juce::Colour (0xff2c323b);
    p.text      = juce::Colour (0xffe6e9ee);
    p.textDim   = juce::Colour (0xff8a93a2);
    p.accent    = juce::Colour (0xffe0873b); // copper
    p.accent2   = juce::Colour (0xff4fb0c6); // cyan
    p.meter     = juce::Colour (0xff6fcf6f);
    p.warn      = juce::Colour (0xffe05a4a);
    p.grid      = juce::Colour (0x22ffffff);
    p.curve     = juce::Colour (0xffe0873b);
    return p;
}

inline Palette light()
{
    Palette p;
    p.bg        = juce::Colour (0xfff2f0ec);
    p.panel     = juce::Colour (0xffffffff);
    p.panelEdge = juce::Colour (0xffd6d2ca);
    p.text      = juce::Colour (0xff1c2026);
    p.textDim   = juce::Colour (0xff6a7280);
    p.accent    = juce::Colour (0xffc46a1f);
    p.accent2   = juce::Colour (0xff1f7f95);
    p.meter     = juce::Colour (0xff3fae4f);
    p.warn      = juce::Colour (0xffc23a2a);
    p.grid      = juce::Colour (0x18000000);
    p.curve     = juce::Colour (0xffc46a1f);
    return p;
}

/** Global current palette (flipped by the dark/light toggle). */
inline Palette& current()
{
    static Palette p = dark();
    return p;
}

inline void setDark (bool isDark) { current() = isDark ? dark() : light(); }

//== metrics ==================================================================
inline constexpr int   kDefaultWidth  = 1040;
inline constexpr int   kDefaultHeight = 640;
inline constexpr float kCorner        = 6.0f;

} // namespace fluxcore::theme
