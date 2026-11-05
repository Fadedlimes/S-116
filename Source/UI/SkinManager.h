#pragma once
// ============================================================================
//  SkinManager.h  (REPLACES your existing SkinManager.h)
//
//  Provides the new S116::SkinManager (theme, typefaces, cached knob art) and
//  a backward-compatible global ::SkinManager so existing callers keep working.
//  Public methods other code still calls are forwarded to the new
//  implementation where a mapping exists.
// ============================================================================
#include "S116Theme.h"
#include <map>
#include <tuple>
#include <string>

namespace S116
{
    // Version-safe string width (JUCE 7 and 8)
    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    class SkinManager : public juce::ChangeBroadcaster
    {
    public:
        static SkinManager& get() { static SkinManager inst; return inst; }

        // ---- theme -------------------------------------------------------
        const Theme& theme() const noexcept { return current; }
        ThemeId themeId() const noexcept    { return id; }

        void setTheme (ThemeId t)
        {

            id = t;
            current = Theme::make (t);
            knobCache.clear();
            refreshLed();                  // keeps the current LED colour across chassis changes
        }

        void setLedColour (juce::Colour base)
        {
            ledBaseColour = base;
            refreshLed();
        }

        void refreshLed()
        {
            current.applyLed (ledBaseColour);
            sendChangeMessage();            // editor listens -> laf.applyTheme(); sendLookAndFeelChange();
        }

        juce::Colour getLedBaseColour() const { return ledBaseColour; }

        void cycleTheme()
        {
            setTheme (static_cast<ThemeId> ((static_cast<int> (id) + 1) % static_cast<int> (ThemeId::numThemes)));
        }

        // ---- fonts -------------------------------------------------------
        void setTypefaces (juce::Typeface::Ptr uiFace, juce::Typeface::Ptr monoFace)
        {
            uiTypeface = uiFace; monoTypeface = monoFace;
        }

        /** UI font (condensed bold). kerning is a fraction of height: CSS letter-spacing 1.4px @14px = 0.1 */
        juce::Font ui (float height, float kerning = 0.0f) const
        {
            return make (uiTypeface, height, true, juce::Font::getDefaultSansSerifFontName()).withExtraKerningFactor (kerning);
        }

        juce::Font mono (float height) const
        {
            return make (monoTypeface, height, false, juce::Font::getDefaultMonospacedFontName());
        }

        // ---- cached knob art -----------------------------------------------
        /** Component square = body + 2*halo. halo = kTickMargin (11) normally, kGlyphMargin (34) for morph rows. */
        juce::Image getKnobBase (int bodyPx, bool morph, int haloPx)
        {
            const auto key = std::make_tuple (bodyPx, morph, haloPx);
            auto it = knobCache.find (key);
            if (it != knobCache.end()) return it->second;
            auto img = renderKnobBase (bodyPx, morph, haloPx);
            knobCache[key] = img;
            return img;
        }

    private:
        SkinManager() : current (Theme::make (ThemeId::GraphiteRed)) { current.applyLed (ledBaseColour); }

        static juce::Font make (const juce::Typeface::Ptr& tf, float h, bool bold, const juce::String& fallback)
        {
           #if JUCE_MAJOR_VERSION >= 8
            if (tf != nullptr) return juce::Font (juce::FontOptions (tf).withHeight (h));
            return juce::Font (juce::FontOptions (fallback, h, bold ? juce::Font::bold : juce::Font::plain));
           #else
            juce::Font f = (tf != nullptr) ? juce::Font (tf) : juce::Font (fallback, h, bold ? juce::Font::bold : juce::Font::plain);
            f.setHeight (h);
            return f;
           #endif
        }

        // Waveform glyphs for the 5-stop morph knob (SVG path data, 18 x 12 box)
        static const char* glyphPath (int i)
        {
            static const char* d[5] = {
                "M1 6C3.5 .5 6 .5 9 6C12 11.5 14.5 11.5 17 6",   // sine      (-135 deg)
                "M1 6L5 1L13 11L17 6",                            // triangle  (-67.5)
                "M1 10L9 2V10L17 2",                              // saw       (0)
                "M1 10V2H9V10H17",                                // square    (+67.5)
                "M1 10H6V2H9V10H17" };                            // pulse     (+135)
            return d[juce::jlimit (0, 4, i)];
        }

        juce::Image renderKnobBase (int body, bool morph, int halo) const
        {
            constexpr int S = 2;                                   // raster scale
            const int T = body + 2 * halo;
            juce::Image img (juce::Image::ARGB, T * S, T * S, true);
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale ((float) S));

            const auto& t = current;
            const float r = (float) body * 0.5f;
            const juce::Point<float> c ((float) T * 0.5f, (float) T * 0.5f);
            auto circ = [] (juce::Point<float> p, float rad) { return juce::Rectangle<float> (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f); };

            // 1. ground shadow
            {
                juce::Path p; p.addEllipse (circ (c, r + 3.0f));
                juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 9, { 0, 5 }).drawForPath (g, p);
            }

            // 2. rim: coloured outline (1px) over black ring (2px)
            g.setColour (t.knobRing);                  g.fillEllipse (circ (c, r + 3.0f));
            g.setColour (juce::Colour (0xff060607));   g.fillEllipse (circ (c, r + 2.0f));

            // 3. matte body disc
            g.setGradientFill (juce::ColourGradient::vertical (t.knobBodyTop, c.y - r, t.knobBodyBottom, c.y + r));
            g.fillEllipse (circ (c, r));

            // 4. tick marks: 11 ticks / 27deg (270deg sweep) or 5 ticks / 67.5deg for morph knobs
            {
                const int   n    = morph ? 5 : 11;
                const float step = morph ? 67.5f : 27.0f;
                g.setColour (t.tick.withAlpha (0.9f));
                for (int i = 0; i < n; ++i)
                {
                    const float a = juce::degreesToRadians (-135.0f + step * (float) i);
                    const float s = std::sin (a), co = std::cos (a);
                    g.drawLine (c.x + s * (r + 4.5f),  c.y - co * (r + 4.5f),
                                c.x + s * (r + 11.0f), c.y - co * (r + 11.0f), 1.4f);
                }
            }

            // 5. cap (offset 2px up): machined sweep + soft sheen
            const float inset = std::round ((float) body * 0.095f);       // 52->5, 42->4, 60->6, 76->7
            const juce::Point<float> cc (c.x, c.y - 2.0f);
            const float cr = r - inset;
            {
                juce::Path p; p.addEllipse (circ (cc, cr));
                juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 3, { 0, 2 }).drawForPath (g, p);

                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (p);
                const int segs = 72;                                      // conic-gradient(from 20deg, A, B 25%, A 50%, B 75%, A)
                for (int i = 0; i < segs; ++i)
                {
                    const float d0 = 360.0f * (float) i / (float) segs;
                    const float d1 = d0 + 360.0f / (float) segs + 0.6f;
                    const float m  = std::fmod (d0 + 2.5f - 20.0f + 360.0f, 180.0f);
                    const float w  = 1.0f - std::abs (m - 90.0f) / 90.0f;
                    g.setColour (t.capA.interpolatedWith (t.capB, w));
                    juce::Path wedge;
                    wedge.addPieSegment (circ (cc, cr + 1.0f), juce::degreesToRadians (d0), juce::degreesToRadians (d1), 0.0f);
                    g.fillPath (wedge);
                }
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (t.capHighlight),
                                                         cc.x - cr * 0.7f, cc.y - cr * 0.7f,
                                                         juce::Colours::transparentWhite,
                                                         cc.x + cr * 0.1f, cc.y + cr * 0.1f, false));
                g.fillEllipse (circ (cc, cr));
            }
            g.setColour (juce::Colour (0xff0a0b0d)); g.drawEllipse (circ (cc, cr), 1.0f);
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (0, 0, T, (int) (cc.y - cr * 0.3f));
                g.setColour (juce::Colours::white.withAlpha (0.2f));
                g.drawEllipse (circ ({ cc.x, cc.y + 1.0f }, cr - 0.5f), 1.0f);
            }

            // 6. waveform glyphs around a morph knob (radius = body/2 + 24)
            if (morph)
            {
                for (int i = 0; i < 5; ++i)
                {
                    const float a = juce::degreesToRadians (-135.0f + 67.5f * (float) i);
                    const juce::Point<float> p (c.x + std::sin (a) * (r + 24.0f), c.y - std::cos (a) * (r + 24.0f));
                    auto path = juce::Drawable::parseSVGPath (glyphPath (i));
                    path.applyTransform (juce::AffineTransform::translation (p.x - 9.0f, p.y - 6.0f));
                    g.setColour (t.caption);
                    g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            }
            return img;
        }

        Theme   current;
        ThemeId id = ThemeId::GraphiteRed;
        juce::Colour ledBaseColour { 0xffff3047 };
        juce::Typeface::Ptr uiTypeface, monoTypeface;
        std::map<std::tuple<int, bool, int>, juce::Image> knobCache;
    };

    inline SkinManager& skin() { return SkinManager::get(); }
}

// ============================================================================
//  Backward-compatible global ::SkinManager
//  Kept so existing callers (PluginProcessor.cpp, OscilloscopeComponent.h)
//  keep compiling. Chassis theme is forwarded to S116::SkinManager; LED/CRT
//  themes are stored locally (the new chassis theme system has no separate
//  LED/CRT concept).
// ============================================================================
enum class ChassisTheme : uint8_t
{
    DarkGraphite = 0,
    RolandBlue,
    VintageCream,
    CyberpunkObsidian,
    Count
};

enum class LEDTheme : uint8_t
{
    Cyan = 0,
    PhosphorGreen,
    Amber,
    RubyRed,
    UltraViolet,
    Count
};

enum class CRTTheme : uint8_t
{
    Cyan = 0,
    PhosphorGreen,
    Amber,
    RubyRed,
    UltraViolet,
    IceWhite,
    Count
};

class SkinManager
{
public:
    SkinManager() noexcept { loadSettings(); }
    ~SkinManager() = default;

    static SkinManager& getInstance() noexcept { static SkinManager instance; return instance; }

    // ---- chassis theme (forwarded to S116::SkinManager) ------------------
    static ChassisTheme themeIdToChassis (S116::ThemeId t) noexcept
    {
        switch (t)
        {
            case S116::ThemeId::JP8000Blue: return ChassisTheme::RolandBlue;
            case S116::ThemeId::ClassicSilver: return ChassisTheme::VintageCream;
            case S116::ThemeId::SH101Red: return ChassisTheme::CyberpunkObsidian;
            case S116::ThemeId::GraphiteRed:
            default: return ChassisTheme::DarkGraphite;
        }
    }
    static S116::ThemeId chassisToThemeId (ChassisTheme c) noexcept
    {
        switch (c)
        {
            case ChassisTheme::RolandBlue: return S116::ThemeId::JP8000Blue;
            case ChassisTheme::VintageCream: return S116::ThemeId::ClassicSilver;
            case ChassisTheme::CyberpunkObsidian: return S116::ThemeId::SH101Red;
            case ChassisTheme::DarkGraphite:
            default: return S116::ThemeId::GraphiteRed;
        }
    }

    void setChassisTheme(ChassisTheme theme) noexcept
    {
        currentChassisTheme = theme;
        S116::skin().setTheme (chassisToThemeId (theme));
        saveSettings();
    }

    ChassisTheme getChassisTheme() const noexcept { return currentChassisTheme; }

    void cycleChassisTheme() noexcept
    {
        S116::skin().cycleTheme();
        currentChassisTheme = themeIdToChassis (S116::skin().themeId());
        saveSettings();
    }

    // ---- LED theme (stored locally) --------------------------------------
    void setLEDTheme(LEDTheme theme) noexcept {
        currentLEDTheme = theme;
        juce::Colour base;
        switch (theme) {
            case LEDTheme::Cyan:          base = juce::Colour(0xff00f0ff); break;
            case LEDTheme::PhosphorGreen: base = juce::Colour(0xff39ff14); break;
            case LEDTheme::Amber:         base = juce::Colour(0xffffa200); break;
            case LEDTheme::RubyRed:       base = juce::Colour(0xffff2d3b); break;
            case LEDTheme::UltraViolet:   base = juce::Colour(0xffc800ff); break;
            default:                      base = juce::Colour(0xffff3047); break;
        }
        S116::skin().setLedColour (base);
        saveSettings();
    }
    LEDTheme getLEDTheme() const noexcept { return currentLEDTheme; }

    static juce::String getLEDThemeName (LEDTheme theme) noexcept
    {
        switch (theme)
        {
            case LEDTheme::Cyan:          return "Cyan";
            case LEDTheme::PhosphorGreen: return "Phosphor Green";
            case LEDTheme::Amber:         return "Amber";
            case LEDTheme::RubyRed:       return "Ruby Red";
            case LEDTheme::UltraViolet:   return "Ultra Violet";
            default:                      return "Cyan";
        }
    }

    void cycleLEDTheme() noexcept
    {
        int next = (static_cast<int>(currentLEDTheme) + 1) % static_cast<int>(LEDTheme::Count);
        setLEDTheme(static_cast<LEDTheme>(next));
    }

    juce::Colour getLEDColour() const noexcept
    {
        switch (currentLEDTheme)
        {
            case LEDTheme::Cyan:          return juce::Colour(0xff00f0ff);
            case LEDTheme::PhosphorGreen: return juce::Colour(0xff39ff14);
            case LEDTheme::Amber:         return juce::Colour(0xffffa200);
            case LEDTheme::RubyRed:       return juce::Colour(0xffff2d3b);
            case LEDTheme::UltraViolet:   return juce::Colour(0xffc800ff);
            default:                      return juce::Colour(0xff00f0ff);
        }
    }

    // ---- CRT / scope theme (stored locally) ------------------------------
    void setCRTTheme(CRTTheme theme) noexcept { currentCRTTheme = theme; saveSettings(); }
    CRTTheme getCRTTheme() const noexcept { return currentCRTTheme; }

    static juce::String getCRTThemeOptionName (CRTTheme theme) noexcept
    {
        switch (theme)
        {
            case CRTTheme::Cyan:          return "Cyan";
            case CRTTheme::PhosphorGreen: return "Phosphor Green";
            case CRTTheme::Amber:         return "Amber";
            case CRTTheme::RubyRed:       return "Ruby Red";
            case CRTTheme::UltraViolet:   return "Ultra Violet";
            case CRTTheme::IceWhite:      return "Ice White";
            default:                      return "Cyan";
        }
    }

    void cycleCRTTheme() noexcept
    {
        int next = (static_cast<int>(currentCRTTheme) + 1) % static_cast<int>(CRTTheme::Count);
        setCRTTheme(static_cast<CRTTheme>(next));
    }

    juce::Colour getCRTColour() const noexcept
    {
        switch (currentCRTTheme)
        {
            case CRTTheme::Cyan:          return juce::Colour(0xff00f0ff);
            case CRTTheme::PhosphorGreen: return juce::Colour(0xff39ff14);
            case CRTTheme::Amber:         return juce::Colour(0xffffa200);
            case CRTTheme::RubyRed:       return juce::Colour(0xffff2d3b);
            case CRTTheme::UltraViolet:   return juce::Colour(0xffc800ff);
            case CRTTheme::IceWhite:      return juce::Colour(0xffffffff);
            default:                      return juce::Colour(0xff00f0ff);
        }
    }

    juce::String getCRTThemeName() const noexcept
    {
        switch (currentCRTTheme)
        {
            case CRTTheme::Cyan:          return "CYAN";
            case CRTTheme::PhosphorGreen: return "PHOSPHOR";
            case CRTTheme::Amber:         return "AMBER";
            case CRTTheme::RubyRed:       return "RED";
            case CRTTheme::UltraViolet:   return "VIOLET";
            case CRTTheme::IceWhite:      return "WHITE";
            default:                      return "SCOPE";
        }
    }

    // ---- hardware palette queries (forwarded to S116 theme) --------------
    juce::Colour getChassisBackgroundColour() const noexcept { return S116::skin().theme().chassisBody; }
    juce::Colour getSectionBackgroundColour() const noexcept { return S116::skin().theme().panelBottom; }
    juce::Colour getSectionHeaderColour() const noexcept { return S116::skin().theme().panelTop; }
    juce::Colour getTextColour() const noexcept { return S116::skin().theme().heading; }
    juce::Colour getSubTextColour() const noexcept { return S116::skin().theme().sub; }
    juce::Colour getBorderColour() const noexcept { return S116::skin().theme().knobRing; }

    bool hasCustomBackground() const noexcept { return false; }
    juce::Image getCustomBackground() const noexcept { return {}; }

    juce::Colour getLCDBackgroundColour() const noexcept { return S116::skin().theme().lcdBottom; }
    juce::Colour getLCDTextColour() const noexcept { return S116::skin().theme().ledText; }
    juce::Colour getLCDBorderColour() const noexcept { return S116::skin().theme().ledOff; }

    juce::Colour getOscilloscopeBeamColour() const noexcept { return getCRTColour(); }
    juce::Colour getOscilloscopeGridColour() const noexcept { return getCRTColour().withAlpha (0.15f); }

    // ---- settings persistence --------------------------------------------
    static juce::File getSettingsFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Fadedlimes").getChildFile("S-116");
        if (!dir.exists()) dir.createDirectory();
        return dir.getChildFile("settings.xml");
    }

    void saveSettings() const
    {
        auto file = getSettingsFile();
        auto xml = std::make_unique<juce::XmlElement>("S116Settings");
        xml->setAttribute("chassis", static_cast<int>(currentChassisTheme));
        xml->setAttribute("led", static_cast<int>(currentLEDTheme));
        xml->setAttribute("crt", static_cast<int>(currentCRTTheme));
        xml->writeTo(file);
    }

    void loadSettings()
    {
        auto file = getSettingsFile();
        if (!file.existsAsFile()) return;
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr && xml->hasTagName("S116Settings"))
        {
            const int ch = xml->getIntAttribute("chassis", static_cast<int>(currentChassisTheme));
            const int ld = xml->getIntAttribute("led", static_cast<int>(currentLEDTheme));
            const int cr = xml->getIntAttribute("crt", static_cast<int>(currentCRTTheme));
            if (ch >= 0 && ch < static_cast<int>(ChassisTheme::Count))
                currentChassisTheme = static_cast<ChassisTheme>(ch);
            if (ld >= 0 && ld < static_cast<int>(LEDTheme::Count))
                currentLEDTheme = static_cast<LEDTheme>(ld);
            if (cr >= 0 && cr < static_cast<int>(CRTTheme::Count))
                currentCRTTheme = static_cast<CRTTheme>(cr);
        }
        // Sync the S116 chassis theme to the persisted value.
        S116::skin().setTheme (chassisToThemeId (currentChassisTheme));
        // Sync LED colour
        {
            juce::Colour base;
            switch (currentLEDTheme) {
                case LEDTheme::Cyan:          base = juce::Colour(0xff00f0ff); break;
                case LEDTheme::PhosphorGreen: base = juce::Colour(0xff39ff14); break;
                case LEDTheme::Amber:         base = juce::Colour(0xffffa200); break;
                case LEDTheme::RubyRed:       base = juce::Colour(0xffff2d3b); break;
                case LEDTheme::UltraViolet:   base = juce::Colour(0xffc800ff); break;
                default:                      base = juce::Colour(0xffff3047); break;
            }
            S116::skin().setLedColour (base);
        }
    }

private:
    ChassisTheme currentChassisTheme = ChassisTheme::DarkGraphite;
    LEDTheme currentLEDTheme = LEDTheme::Cyan;
    CRTTheme currentCRTTheme = CRTTheme::Cyan;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkinManager)
};
