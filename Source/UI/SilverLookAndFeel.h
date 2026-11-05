#pragma once
// ============================================================================
//  SilverLookAndFeel.h  (REPLACES your existing file; class name kept so
//  `setLookAndFeel (&laf)` call sites keep working)
//
//  Draws: rotary knobs (cached body + live pointer), ledge/LED buttons,
//  LCD-style dropdowns, LED value-readout labels, popup menus.
//
//  Per-component switches (set via component.getProperties()):
//    Slider  "morph" (bool)   -> 5-stop waveform knob with glyph ring
//    Slider  "halo"  (int)    -> halo px around body (11 default, 34 in glyph rows)
//    Label   "readout" (bool) -> draw as red LED value plate
//    Label   "big"     (bool) -> larger readout (aux knobs)
// ============================================================================
#include "SkinManager.h"
#include <set>

class SilverLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SilverLookAndFeel() { applyTheme(); }

    /** Call after SkinManager::setTheme(), then editor.sendLookAndFeelChange(). */
    void applyTheme()
    {
        using namespace S116;
        const auto& t = skin().theme();
        setColour (juce::ComboBox::textColourId,        t.selText);
        setColour (juce::ComboBox::backgroundColourId,  t.selTop);
        setColour (juce::ComboBox::outlineColourId,     t.selOutline);
        setColour (juce::ComboBox::arrowColourId,       t.selArrow);
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff0e0f12));
        setColour (juce::PopupMenu::textColourId,                  t.selText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, t.ledOff.brighter (0.2f));
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
        setColour (juce::Label::textColourId,           t.caption);
        setColour (juce::TextButton::textColourOffId,   t.btnText);
        setColour (juce::TextButton::textColourOnId,    t.btnText);
    }

    // ------------------------------------------------------------------ knobs
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace S116;
        const auto& t = skin().theme();

        const bool morph = (bool) s.getProperties().getWithDefault ("morph", false);
        const int  halo  = (int)  s.getProperties().getWithDefault ("halo", (int) kTickMargin);
        const int  T     = juce::jmin (w, h);
        const int  body  = T - 2 * halo;
        if (body < 8) return;

        const juce::Rectangle<float> sq ((float) x + ((float) w - (float) T) * 0.5f,
                                         (float) y + ((float) h - (float) T) * 0.5f, (float) T, (float) T);

        // 1) cached body, ticks, cap, glyphs
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (skin().getKnobBase (body, morph, halo), sq);

        // 2) live pointer (3px, red LED glow) - sits on the cap, which is 2px above body centre
        const float a  = startAngle + pos * (endAngle - startAngle);
        const float r  = (float) body * 0.5f;
        const float cx = sq.getCentreX(), cy = sq.getCentreY() - 2.0f;
        const float inset = std::round ((float) body * 0.095f);
        const float top = inset + 1.5f, len = (float) body * 0.28f;
        const float dx = std::sin (a), dy = -std::cos (a);

        juce::Path p;
        p.startNewSubPath (cx + dx * (r - top),        cy + dy * (r - top));
        p.lineTo          (cx + dx * (r - top - len),  cy + dy * (r - top - len));

        g.setColour (t.pointerGlow.withMultipliedAlpha (0.45f));
        g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (t.pointer);
        g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ---------------------------------------------------------------- buttons
    static juce::Rectangle<float> faceOf (const juce::Component& c)
    {
        using namespace S116;
        return c.getLocalBounds().toFloat()
                .withTrimmedLeft ((float) kBtnInsL).withTrimmedRight ((float) kBtnInsR)
                .withTrimmedTop ((float) kBtnInsT).withTrimmedBottom ((float) kBtnInsB);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool over, bool down) override
    {
        const auto& t = S116::skin().theme();
        const bool on = b.getToggleState();
        const bool pressed = on || down;
        const juce::Colour black (0xff060607);

        auto fullBounds = b.getLocalBounds().toFloat();
        int insetL = juce::jmin (3, (int) fullBounds.getWidth() / 8);
        int insetR = juce::jmin (3, (int) fullBounds.getWidth() / 8);
        int insetT = juce::jmin (3, (int) fullBounds.getHeight() / 8);
        int insetB = juce::jmin (6, (int) fullBounds.getHeight() / 6);
        auto face = fullBounds.withTrimmedLeft ((float) insetL).withTrimmedRight ((float) insetR)
                             .withTrimmedTop ((float) insetT).withTrimmedBottom ((float) insetB);

        if (face.getWidth() < 8 || face.getHeight() < 8) return;

        const float rad = juce::jmin (6.0f, face.getWidth() * 0.25f, face.getHeight() * 0.25f);
        const float ledge = pressed ? 1.0f : 3.0f;
        if (pressed) face.translate (0.0f, 2.0f);
        const auto ledgeR = face.translated (0.0f, ledge);

        // 3px ledge (outline / black ring / fill), then the face's own 2px black ring
        g.setColour (t.btnRing); g.fillRoundedRectangle (ledgeR.expanded (3.0f), rad + 3.0f);
        g.setColour (black);     g.fillRoundedRectangle (ledgeR.expanded (2.0f), rad + 2.0f);
        g.setColour (t.btnLedge);g.fillRoundedRectangle (ledgeR, rad);
        g.setColour (black);     g.fillRoundedRectangle (face.expanded (2.0f), rad + 2.0f);

        // face
        g.setColour (t.btnTop);
        g.fillRoundedRectangle (face, rad);

        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path fp; fp.addRoundedRectangle (face, rad);
            g.reduceClipRegion (fp);

            // bottom inset shade
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::transparentBlack, face.getBottom() - 8.0f,
                                                               juce::Colours::black.withAlpha (0.35f), face.getBottom()));
            g.fillRect (face);

            if (pressed)
            {
                g.setColour (juce::Colours::black.withAlpha (0.20f)); g.fillRect (face);
                g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.6f), face.getY(),
                                                                   juce::Colours::transparentBlack, face.getY() + 6.0f));
                g.fillRect (face);
            }
            else if (over)
            {
                g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRect (face);
            }
            // top edge highlight
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.fillRect (face.getX(), face.getY(), face.getWidth(), 1.0f);
        }

        // LED strip: 3px tall, inset 10px, 5px from top. Lit only for toggled-on buttons.
        if (face.getWidth() < 24 || face.getHeight() < 14) return;
        const juce::Rectangle<float> led (face.getX() + 10.0f, face.getY() + 5.0f, face.getWidth() - 20.0f, 3.0f);
        if (on)
        {
            g.setColour (t.ledOn.withAlpha (0.12f)); g.fillRoundedRectangle (led.expanded (5.0f, 4.0f), 4.0f);
            g.setColour (t.ledOn.withAlpha (0.22f)); g.fillRoundedRectangle (led.expanded (2.5f, 2.0f), 3.0f);
        }
        g.setColour (on ? t.ledOn : t.ledOff);
        g.fillRoundedRectangle (led, 1.5f);
        if (! on)
        {
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRect (led.getX() + 1.0f, led.getY(), led.getWidth() - 2.0f, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        static int saveCallCount = 0;
        static std::set<juce::String> printedThemes;
        if (b.getButtonText() == "SAVE")
        {
            ++saveCallCount;
            const auto& t = S116::skin().theme();
            auto face = faceOf (b);
            if (b.getToggleState() || down) face.translate (0.0f, 2.0f);
            const auto area = face.withTrimmedTop (9.0f);
            const bool lightFace = t.btnTop.interpolatedWith (t.btnBottom, 0.5f).getPerceivedBrightness() > 0.55f;
            const auto txt    = lightFace ? juce::Colour (0xff14171a) : t.btnText;
            const auto shadow = lightFace ? juce::Colours::white.withAlpha (0.7f) : juce::Colours::black;
            const float dy    = lightFace ? 1.0f : -1.0f;
            if (printedThemes.find (t.name) == printedThemes.end())
            {
                printedThemes.insert (t.name);
                std::cerr << "DRAWBUTTONTEXT theme=" << t.name.toStdString()
                          << " | btnText=" << t.btnText.toString().toStdString()
                          << " | btnTop=" << t.btnTop.toString().toStdString()
                          << " | btnBottom=" << t.btnBottom.toString().toStdString()
                          << " | mainTextColour=" << txt.toString().toStdString()
                          << " | shadow=" << shadow.toString().toStdString()
                          << " | dy=" << dy
                          << " | saveCallCount=" << saveCallCount
                          << " | findColourOff=" << b.findColour (juce::TextButton::textColourOffId).toString().toStdString()
                          << " | findColourOn=" << b.findColour (juce::TextButton::textColourOnId).toString().toStdString()
                          << std::endl;
            }
        }
        const auto& t = S116::skin().theme();
        auto face = faceOf (b);
        if (b.getToggleState() || down) face.translate (0.0f, 2.0f);
        const auto area = face.withTrimmedTop (9.0f);

        const bool lightFace = t.btnTop.interpolatedWith (t.btnBottom, 0.5f).getPerceivedBrightness() > 0.55f;
        const auto txt    = lightFace ? juce::Colour (0xff14171a) : t.btnText;
        const auto shadow = lightFace ? juce::Colours::white.withAlpha (0.7f) : juce::Colours::black;
        const float dy    = lightFace ? 1.0f : -1.0f;

        g.setFont (S116::skin().ui (14.0f, 0.10f));
        g.setColour (shadow); g.drawText (b.getButtonText(), area.translated (0.0f, dy), juce::Justification::centred, false);
        g.setColour (txt);    g.drawText (b.getButtonText(), area,                       juce::Justification::centred, false);
    }


    juce::Font getTextButtonFont (juce::TextButton&, int) override { return S116::skin().ui (14.0f, 0.10f); }

    // -------------------------------------------------------------- dropdowns
    void drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) override
    {
        juce::LookAndFeel_V4::drawComboBox (g, w, h, down, 0, 0, 0, 0, box);
        return;
        const auto& t = S116::skin().theme();
        const juce::Rectangle<float> r (0.0f, 0.0f, (float) w, (float) h);
        const float alpha = box.isEnabled() ? 1.0f : 0.5f;

        g.setColour (t.selOutline.withMultipliedAlpha (alpha)); g.fillRoundedRectangle (r, 5.0f);
        const auto in = r.reduced (1.0f);
        g.setGradientFill (juce::ColourGradient::vertical (t.selTop, in.getY(), t.selBottom, in.getBottom()));
        g.fillRoundedRectangle (in, 4.0f);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.85f), in.getY(),
                                                           juce::Colours::transparentBlack, in.getY() + 5.0f));
        g.fillRoundedRectangle (in, 4.0f);
        if (down) { g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (in, 4.0f); }

        juce::Path arrow;                                            // 8 x 6 triangle, 11px from right edge
        arrow.addTriangle ((float) w - 19.0f, 13.0f, (float) w - 11.0f, 13.0f, (float) w - 15.0f, 19.0f);
        g.setColour (t.selArrow.withMultipliedAlpha (alpha));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return S116::skin().mono (13.0f); }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->getProperties().set ("readout", true);
        l->setJustificationType (juce::Justification::centred);
        return l;
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 1, box.getWidth() - 10 - 28, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    juce::Font getPopupMenuFont() override { return S116::skin().mono (20.0f); }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardHeight, int& w, int& h) override
    {
        juce::LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardHeight, w, h);
        if (! isSeparator) h = juce::jmax (h, 30);
    }

    // ----------------------------------------------------------------- labels
    juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
    {
        if (! s.isRotary())
            return juce::LookAndFeel_V4::getSliderLayout (s);

        const auto& p = s.getProperties();
        const bool right = (bool) p.getWithDefault ("readoutRight", false);
        if (! right && s.getTextBoxPosition() != juce::Slider::TextBoxBelow)
            return juce::LookAndFeel_V4::getSliderLayout (s);

        const int body = (int) p.getWithDefault ("body", 52);
        const int halo = (int) p.getWithDefault ("halo", 11);
        const int rw   = (int) p.getWithDefault ("readoutW", 54);
        const int rh   = (int) p.getWithDefault ("readoutH", 20);
        const int T    = body + 2 * halo;
        const auto b   = s.getLocalBounds();

        juce::Slider::SliderLayout l;
        if (right)                                              // header master knob: readout to the right of the knob
        {
            l.sliderBounds  = { 0, (b.getHeight() - T) / 2, T, T };
            l.textBoxBounds = { T - 6, (b.getHeight() - rh) / 2, rw, rh };
        }
        else                                                    // readout plate below the knob body
        {
            const int gap = halo > 11 ? 18 : 13;
            l.sliderBounds  = { (b.getWidth() - T) / 2, 0, T, T };
            l.textBoxBounds = { (b.getWidth() - rw) / 2, T / 2 + body / 2 + gap, rw, rh };
        }
        return l;
    }

    void drawLabel (juce::Graphics& g, juce::Label& l) override
    {
        const auto& t = S116::skin().theme();
        const auto& props = l.getProperties();
        const auto r = l.getLocalBounds().toFloat();

        // ---- LED readout plate (slider text boxes, readout labels) ----
        bool big = (bool) props.getWithDefault ("big", false);
        if (auto* parentSlider = dynamic_cast<juce::Slider*> (l.getParentComponent()))
            big = big || (bool) parentSlider->getProperties().getWithDefault ("bigReadout", false);

        if ((bool) props.getWithDefault ("readout", false))
        {
            g.setGradientFill (juce::ColourGradient::vertical (t.plateTop, r.getY(), t.plateBottom, r.getBottom()));
            g.fillRoundedRectangle (r, 3.0f);
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.8f), r.getY(),
                                                               juce::Colours::transparentBlack, r.getY() + 5.0f));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (r.getX() + 2.0f, r.getBottom() - 1.0f, r.getWidth() - 4.0f, 1.0f);

            g.setFont (S116::skin().mono (big ? 16.0f : 12.0f));
            const auto txt = l.getText();
            g.setColour (t.ledReadout.withAlpha (0.18f));
            for (auto o : { juce::Point<float> (-1, 0), { 1, 0 }, { 0, -1 }, { 0, 1 } })
                g.drawText (txt, r.translated (o.x, o.y), juce::Justification::centred, false);
            g.setColour (t.ledReadout);
            g.drawText (txt, r, juce::Justification::centred, false);
            return;
        }

        // ---- styled text labels ----
        const auto style = props.getWithDefault ("style", "").toString();
        if (style.isNotEmpty())
        {
            const auto txt = l.getText();
            const auto just = l.getJustificationType();
            if (style == "caption")
            {
                const float size = (float) (double) props.getWithDefault ("captionSize", 13.0);
                g.setFont (S116::skin().ui (size, 0.085f));
                g.setColour (juce::Colours::black);   g.drawText (txt, r.translated (0, -1), just, false);
                g.setColour (t.caption);              g.drawText (txt, r, just, false);
            }
            else if (style == "field")
            {
                g.setFont (S116::skin().ui (12.0f, 0.10f));
                g.setColour (juce::Colours::black);   g.drawText (txt, r.translated (0, -1), just, false);
                g.setColour (t.heading);              g.drawText (txt, r, just, false);
            }
            else if (style == "sub")
            {
                g.setColour (t.rule);     g.fillRect (r.getX(), r.getBottom() - 2.0f, r.getWidth(), 1.0f);
                g.setColour (t.ruleDark); g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
                g.setFont (S116::skin().ui (12.0f, 0.15f));
                g.setColour (t.sub);
                g.drawText (txt, r.withTrimmedBottom (3.0f), juce::Justification::centredLeft, false);
            }
            return;
        }

        juce::LookAndFeel_V4::drawLabel (g, l);
    }

};
using StudioLookAndFeel = SilverLookAndFeel;
