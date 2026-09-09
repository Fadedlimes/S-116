#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// ================================================================
// UI THEME DATA
// ================================================================
struct ChassisTheme {
    const char* name;

    // Panel / Faceplate
    juce::Colour faceplateTop;
    juce::Colour faceplateBottom;
    juce::Colour panelOutline;

    // Header
    juce::Colour headerBg;
    juce::Colour headerAccent;
    juce::Colour headerText;

    // Section labels / readable text colour
    juce::Colour sectionTitle;

    // Knob / Dial
    juce::Colour knobOuterBezel;
    juce::Colour knobDialTop;
    juce::Colour knobDialBottom;
    juce::Colour knobCapTop;
    juce::Colour knobCapBottom;
    juce::Colour knobBevel;

    // Dropdowns / ComboBoxes
    juce::Colour comboBg;
    juce::Colour comboOutline;
    juce::Colour comboArrow;
    juce::Colour comboText;
    juce::Colour popupBg;
    juce::Colour popupText;
    juce::Colour popupHighlight;
    juce::Colour popupHighlightedText;
};

// ------------------------------------------------------------------
// Four built-in chassis themes
// ------------------------------------------------------------------
static const ChassisTheme chassisThemes[] = {
    // -------- Classic Silver (Default) --------
    {
        "Classic Silver",
        juce::Colour(0xffe0e3e8), juce::Colour(0xffcbcfd6),
        juce::Colour(0xff23272e),
        juce::Colour(0xff1f242b), juce::Colour(0xffe74c3c), juce::Colour(0xfff0f2f5),
        juce::Colour(0xff14171a),               // sectionTitle (dark)
        juce::Colour(0xff3a404a),
        juce::Colour(0xfff8f9fb), juce::Colour(0xffb0b5be),
        juce::Colour(0xffdcdfe5), juce::Colour(0xff8c93a0),
        juce::Colour(0xff68707d),
        juce::Colour(0xff181b20), juce::Colour(0xff4a5260),
        juce::Colour(0xffa5b1c2), juce::Colour(0xfff0f2f5),
        juce::Colour(0xff1c2026), juce::Colour(0xfff0f2f5),
        juce::Colour(0xff333a46), juce::Colour(0xffffffff)
    },

    // -------- Midnight Blue (80s American Poly) --------
    {
        "Midnight Blue",
        juce::Colour(0xff1b1e2a), juce::Colour(0xff0f1118),
        juce::Colour(0xff05070c),
        juce::Colour(0xff0a0c12), juce::Colour(0xff00bfff), juce::Colour(0xffffffff),
        juce::Colour(0xffcfd8e8),               // sectionTitle (light)
        juce::Colour(0xff05070c),
        juce::Colour(0xff2c3545), juce::Colour(0xff141a24),
        juce::Colour(0xffdce4ef), juce::Colour(0xff8fa3b8),
        juce::Colour(0xff62788e),
        juce::Colour(0xffe8ecf2), juce::Colour(0xff2c3545),
        juce::Colour(0xff101418), juce::Colour(0xff101418),
        juce::Colour(0xfff0f4f8), juce::Colour(0xff101418),
        juce::Colour(0xff0077c2), juce::Colour(0xffffffff)
    },

    // -------- Vintage Wood (Analog Classic) --------
    {
        "Vintage Wood",
        juce::Colour(0xff2e2a24), juce::Colour(0xff181512),
        juce::Colour(0xff0f0d0a),
        juce::Colour(0xff0f0d0a), juce::Colour(0xffd9a441), juce::Colour(0xfff5e6c8),
        juce::Colour(0xffe6dcc5),               // sectionTitle (cream)
        juce::Colour(0xff2b2b2b),
        juce::Colour(0xff3b3a38), juce::Colour(0xff141312),
        juce::Colour(0xffe8e8e8), juce::Colour(0xff9e9e9e),
        juce::Colour(0xffb99a5b),
        juce::Colour(0xff2b2b2b), juce::Colour(0xffd9a441),
        juce::Colour(0xfff1e7d8), juce::Colour(0xfff1e7d8),
        juce::Colour(0xff2b2b2b), juce::Colour(0xfff1e7d8),
        juce::Colour(0xffd9a441), juce::Colour(0xff1f1306)
    },

    // -------- Stealth Dark (Modern Dark Mode) --------
    {
        "Stealth Dark",
        juce::Colour(0xff1a1c20), juce::Colour(0xff0d0f12),
        juce::Colour(0xff05070a),
        juce::Colour(0xff0a0b0e), juce::Colour(0xff8ab4f8), juce::Colour(0xffffffff),
        juce::Colour(0xffd0d0d5),               // sectionTitle (light)
        juce::Colour(0xff252a31),
        juce::Colour(0xff3f444d), juce::Colour(0xff22262c),
        juce::Colour(0xff3a414a), juce::Colour(0xff1d2127),
        juce::Colour(0xff78828f),
        juce::Colour(0xff14171b), juce::Colour(0xff505763),
        juce::Colour(0xff90a0b0), juce::Colour(0xfff0f2f5),
        juce::Colour(0xff1a1d22), juce::Colour(0xfff0f2f5),
        juce::Colour(0xff39414b), juce::Colour(0xffffffff)
    }
};

static constexpr int NUM_CHASSIS_THEMES = 4;

// ================================================================
// SILVER LOOK AND FEEL
// ================================================================
class SilverLookAndFeel : public juce::LookAndFeel_V4 {
public:
    SilverLookAndFeel() {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff2d55));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xfff0f2f5));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdcdfe4));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffcbd0d8));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xff14171a));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff14171a));

        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff14171a));
        setColour(juce::Label::textColourId, juce::Colour(0xff14171a));

        setChassisTheme(chassisThemes[0]);
    }

    void setAccentColour(juce::Colour primary, juce::Colour glow, juce::Colour highlight) {
        accentPrimary = primary;
        accentGlow = glow;
        accentHighlight = highlight;
        updateComboTextColour();
    }

    void setChassisTheme(const ChassisTheme& t) {
        theme = t;
        isStealthDark = (juce::String(theme.name) == "Stealth Dark");

        setColour(juce::ComboBox::backgroundColourId, t.comboBg);
        setColour(juce::ComboBox::textColourId, t.comboText);
        setColour(juce::ComboBox::outlineColourId, t.comboOutline);
        setColour(juce::ComboBox::arrowColourId, t.comboArrow);
        setColour(juce::PopupMenu::backgroundColourId, t.popupBg);
        setColour(juce::PopupMenu::textColourId, t.popupText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, t.popupHighlight);
        setColour(juce::PopupMenu::highlightedTextColourId, t.popupHighlightedText);

        setColour(juce::Label::textColourId, t.sectionTitle);
        setColour(juce::Slider::textBoxTextColourId, t.sectionTitle);

        const juce::String themeName(theme.name);

        if (themeName == "Midnight Blue")
        {
            setColour(juce::ComboBox::textColourId,  juce::Colour(0xff101418));
            setColour(juce::PopupMenu::textColourId, juce::Colour(0xff101418));
        }
        else if (themeName == "Classic Silver" || themeName == "Vintage Wood")
        {
            setColour(juce::ComboBox::textColourId,  juce::Colour(0xfff0f2f5));
            setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff0f2f5));
        }

        if (isStealthDark)
            updateComboTextColour();
    }

    const ChassisTheme& getChassisTheme() const noexcept { return theme; }

    // ================================================================
    // Rotary Slider
    // ================================================================
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override {
        juce::ignoreUnused(slider);
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));
        float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 6.0f;
        if (diameter <= 0.0f) return;

        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float radius = diameter * 0.5f;

        g.setColour(juce::Colour(0x35000000));
        g.fillEllipse(cx - radius + 1.0f, cy - radius + 2.0f, radius * 2.0f, radius * 2.0f);

        g.setColour(theme.knobOuterBezel);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(theme.knobBevel);
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        float dialRadius = radius - 2.5f;
        juce::ColourGradient dialGrad(theme.knobDialTop, cx - dialRadius, cy - dialRadius,
                                      theme.knobDialBottom, cx + dialRadius, cy + dialRadius, true);
        dialGrad.addColour(0.3, theme.knobDialTop.interpolatedWith(theme.knobDialBottom, 0.4f));
        dialGrad.addColour(0.7, theme.knobDialBottom.interpolatedWith(theme.knobDialTop, 0.55f));

        g.setGradientFill(dialGrad);
        g.fillEllipse(cx - dialRadius, cy - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

        g.setColour(juce::Colour(0x28000000));
        for (int i = 0; i < 16; ++i) {
            float a = static_cast<float>(i) * (juce::MathConstants<float>::twoPi / 16.0f);
            float innerX = cx + std::cos(a) * (dialRadius - 3.5f);
            float innerY = cy + std::sin(a) * (dialRadius - 3.5f);
            float outerX = cx + std::cos(a) * dialRadius;
            float outerY = cy + std::sin(a) * dialRadius;
            g.drawLine(innerX, innerY, outerX, outerY, 0.8f);
        }

        float capRadius = dialRadius * 0.58f;
        juce::ColourGradient capGrad(theme.knobCapTop, cx - capRadius, cy - capRadius,
                                     theme.knobCapBottom, cx + capRadius, cy + capRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(cx - capRadius, cy - capRadius, capRadius * 2.0f, capRadius * 2.0f);
        g.setColour(theme.knobBevel);
        g.drawEllipse(cx - capRadius, cy - capRadius, capRadius * 2.0f, capRadius * 2.0f, 0.8f);

        float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        float startPointerR = capRadius - 1.0f;
        float endPointerR = dialRadius - 1.0f;
        float px1 = cx + std::sin(angle) * startPointerR;
        float py1 = cy - std::cos(angle) * startPointerR;
        float px2 = cx + std::sin(angle) * endPointerR;
        float py2 = cy - std::cos(angle) * endPointerR;

        g.setColour(accentGlow);
        g.drawLine(px1, py1, px2, py2, 3.8f);
        g.setColour(accentPrimary);
        g.drawLine(px1, py1, px2, py2, 2.2f);
        g.setColour(accentHighlight);
        g.drawLine(px1, py1, px2, py2, 1.0f);
    }

    // ================================================================
    // Buttons
    // ================================================================
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        juce::ignoreUnused(backgroundColour);
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        float corner = 3.5f;

        bool isDown = shouldDrawButtonAsDown || button.getToggleState();

        g.setColour(theme.panelOutline);
        g.fillRoundedRectangle(bounds, corner);

        auto inner = bounds.reduced(1.0f);
        if (isDown) {
            juce::ColourGradient downGrad(theme.knobDialBottom, inner.getX(), inner.getY(),
                                          theme.knobDialTop, inner.getX(), inner.getBottom(), false);
            g.setGradientFill(downGrad);
            g.fillRoundedRectangle(inner, corner - 0.5f);
            g.setColour(theme.knobBevel);
            g.drawRoundedRectangle(inner, corner - 0.5f, 1.0f);
        } else {
            juce::Colour base = shouldDrawButtonAsHighlighted
                ? theme.knobDialTop.interpolatedWith(juce::Colour(0xffffffff), 0.4f)
                : theme.knobDialTop;
            juce::ColourGradient upGrad(base, inner.getX(), inner.getY(),
                                        theme.knobDialBottom, inner.getX(), inner.getBottom(), false);
            g.setGradientFill(upGrad);
            g.fillRoundedRectangle(inner, corner - 0.5f);

            g.setColour(juce::Colour(0x90ffffff));
            g.drawLine(inner.getX() + 2.0f, inner.getY() + 1.0f,
                       inner.getRight() - 2.0f, inner.getY() + 1.0f, 1.0f);
            g.setColour(theme.knobBevel);
            g.drawRoundedRectangle(inner, corner - 0.5f, 1.0f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        g.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
        g.setColour(button.getToggleState() ? theme.sectionTitle : theme.sectionTitle.interpolatedWith(juce::Colour(0xff000000), 0.2f));

        auto bounds = button.getLocalBounds();
        if (button.isToggleable())
            bounds.removeFromLeft(5);

        g.drawFittedText(button.getButtonText(), bounds, juce::Justification::centred, 1);
    }

    // ================================================================
    // ComboBox / Dropdown
    // ================================================================
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override {
        juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH, box);

        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

        g.setColour(theme.comboBg);
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(theme.comboOutline);
        g.drawRoundedRectangle(bounds, 3.0f, 1.2f);

        auto arrowX = static_cast<float>(width) - 15.0f;
        auto arrowY = bounds.getCentreY();
        juce::Path arrow;
        arrow.startNewSubPath(arrowX - 3.0f, arrowY - 2.0f);
        arrow.lineTo(arrowX + 3.0f, arrowY - 2.0f);
        arrow.lineTo(arrowX, arrowY + 2.0f);
        arrow.closeSubPath();

        g.setColour(theme.comboArrow);
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setBounds(4, 1, box.getWidth() - 17, box.getHeight() - 2);
        label.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
        label.setJustificationType(juce::Justification::centredLeft);
    }

private:
    void updateComboTextColour() {
        if (isStealthDark) {
            setColour(juce::ComboBox::textColourId, accentPrimary);
            setColour(juce::PopupMenu::textColourId, accentPrimary);
        }
    }

    ChassisTheme theme;
    bool isStealthDark = false;
    juce::Colour accentPrimary { 0xffff2d55 };
    juce::Colour accentGlow { 0x60ff2d55 };
    juce::Colour accentHighlight { 0xffffa2b0 };
};