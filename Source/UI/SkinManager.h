#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_graphics/juce_graphics.h>
 #include <juce_core/juce_core.h>
#endif

#include <cmath>
#include <algorithm>
#include <memory>

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
    SkinManager() noexcept
    {
        scanAndLoadCustomAssets();
        loadSettings();
    }

    ~SkinManager() = default;

    static SkinManager& getInstance() noexcept
    {
        static SkinManager instance;
        return instance;
    }

    // =========================================================================
    // Chassis Theming
    // =========================================================================
    void setChassisTheme(ChassisTheme theme) noexcept
    {
        currentChassisTheme = theme;
        saveSettings();
    }

    ChassisTheme getChassisTheme() const noexcept { return currentChassisTheme; }

    void cycleChassisTheme() noexcept
    {
        int next = (static_cast<int>(currentChassisTheme) + 1) % static_cast<int>(ChassisTheme::Count);
        setChassisTheme(static_cast<ChassisTheme>(next));
    }

    // =========================================================================
    // LED Console Theming
    // =========================================================================
    void setLEDTheme(LEDTheme theme) noexcept
    {
        currentLEDTheme = theme;
        saveSettings();
    }

    LEDTheme getLEDTheme() const noexcept { return currentLEDTheme; }

    /** The parameter name of an LED theme. The order has to match the LEDTheme
        enumeration, because the stored parameter index is cast straight to the enum. */
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

    /** The parameter name of a CRT (oscilloscope) theme. Same ordering rule as above. */
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

    // =========================================================================
    // CRT Oscilloscope Theming & Reboot Memory
    // =========================================================================
    void setCRTTheme(CRTTheme theme) noexcept
    {
        currentCRTTheme = theme;
        saveSettings();
    }

    CRTTheme getCRTTheme() const noexcept { return currentCRTTheme; }

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

    // =========================================================================
    // Hardware Palette Queries
    // =========================================================================
    juce::Colour getChassisBackgroundColour() const noexcept
    {
        switch (currentChassisTheme)
        {
            case ChassisTheme::DarkGraphite:      return juce::Colour(0xff121417);
            case ChassisTheme::RolandBlue:         return juce::Colour(0xff0b1728);
            case ChassisTheme::VintageCream:       return juce::Colour(0xff221d18);
            case ChassisTheme::CyberpunkObsidian:  return juce::Colour(0xff060709);
            default:                               return juce::Colour(0xff121417);
        }
    }

    juce::Colour getSectionBackgroundColour() const noexcept
    {
        switch (currentChassisTheme)
        {
            case ChassisTheme::DarkGraphite:      return juce::Colour(0xff1a1d22);
            case ChassisTheme::RolandBlue:         return juce::Colour(0xff13233c);
            case ChassisTheme::VintageCream:       return juce::Colour(0xffded7c8);
            case ChassisTheme::CyberpunkObsidian:  return juce::Colour(0xff0d0f14);
            default:                               return juce::Colour(0xff1a1d22);
        }
    }

    juce::Colour getSectionHeaderColour() const noexcept
    {
        switch (currentChassisTheme)
        {
            case ChassisTheme::DarkGraphite:      return juce::Colour(0xff242831);
            case ChassisTheme::RolandBlue:         return juce::Colour(0xff1c3153);
            case ChassisTheme::VintageCream:       return juce::Colour(0xff352e26);
            case ChassisTheme::CyberpunkObsidian:  return juce::Colour(0xff151821);
            default:                               return juce::Colour(0xff242831);
        }
    }

    juce::Colour getTextColour() const noexcept
    {
        if (currentChassisTheme == ChassisTheme::VintageCream)
            return juce::Colour(0xff1e1a16);

        return juce::Colour(0xfff1f5f9);
    }

    juce::Colour getSubTextColour() const noexcept
    {
        if (currentChassisTheme == ChassisTheme::VintageCream)
            return juce::Colour(0xff5c5246);

        return juce::Colour(0xff94a3b8);
    }

    juce::Colour getBorderColour() const noexcept
    {
        switch (currentChassisTheme)
        {
            case ChassisTheme::DarkGraphite:      return juce::Colour(0xff2e3440);
            case ChassisTheme::RolandBlue:         return juce::Colour(0xff26416d);
            case ChassisTheme::VintageCream:       return juce::Colour(0xffb5ab9a);
            case ChassisTheme::CyberpunkObsidian:  return juce::Colour(0xff1e232f);
            default:                               return juce::Colour(0xff2e3440);
        }
    }

    // =========================================================================
    // Retro LCD Dot-Matrix Phosphor Styling
    // =========================================================================
    juce::Colour getLCDBackgroundColour() const noexcept
    {
        switch (currentChassisTheme)
        {
            case ChassisTheme::DarkGraphite:      return juce::Colour(0xff090d12);
            case ChassisTheme::RolandBlue:         return juce::Colour(0xff06111f);
            case ChassisTheme::VintageCream:       return juce::Colour(0xff181a13);
            case ChassisTheme::CyberpunkObsidian:  return juce::Colour(0xff030406);
            default:                               return juce::Colour(0xff090d12);
        }
    }

    juce::Colour getLCDTextColour() const noexcept
    {
        switch (currentChassisTheme)
        {
            case ChassisTheme::DarkGraphite:      return getLEDColour();
            case ChassisTheme::RolandBlue:         return juce::Colour(0xff70cbff);
            case ChassisTheme::VintageCream:       return juce::Colour(0xff88e53a);
            case ChassisTheme::CyberpunkObsidian:  return juce::Colour(0xffff0084);
            default:                               return getLEDColour();
        }
    }

    juce::Colour getLCDBorderColour() const noexcept
    {
        switch (currentChassisTheme)
        {
            case ChassisTheme::DarkGraphite:      return juce::Colour(0xff1c2430);
            case ChassisTheme::RolandBlue:         return juce::Colour(0xff193054);
            case ChassisTheme::VintageCream:       return juce::Colour(0xff2d3324);
            case ChassisTheme::CyberpunkObsidian:  return juce::Colour(0xff251230);
            default:                               return juce::Colour(0xff1c2430);
        }
    }

    juce::Colour getOscilloscopeBeamColour() const noexcept
    {
        return getCRTColour();
    }

    juce::Colour getOscilloscopeGridColour() const noexcept
    {
        return getCRTColour().withAlpha(0.15f);
    }

    bool hasCustomBackground() const noexcept
    {
        return customBackground.isValid();
    }

    const juce::Image& getCustomBackground() const noexcept
    {
        return customBackground;
    }

    void scanAndLoadCustomAssets() noexcept
    {
        customBackground = juce::Image();

        juce::Array<juce::File> searchDirs;

        const auto homeDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        searchDirs.add(homeDir.getChildFile(".config/Simple116/Skins"));

        const auto execDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        searchDirs.add(execDir.getChildFile("Skins"));
        searchDirs.add(execDir.getChildFile("../Skins"));

        for (const auto& dir : searchDirs)
        {
            if (dir.isDirectory())
            {
                const auto bgFile = dir.getChildFile("background.png");
                if (bgFile.existsAsFile())
                {
                    customBackground = juce::ImageFileFormat::loadFrom(bgFile);
                    if (customBackground.isValid())
                        break;
                }
            }
        }
    }

    // =========================================================================
    // Settings Persistence Across Synth Reboots
    // =========================================================================
    static juce::File getSettingsFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Fadedlimes")
            .getChildFile("S-116");

        if (!dir.exists())
            dir.createDirectory();

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
        if (!file.existsAsFile())
            return;

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
    }

private:
    ChassisTheme currentChassisTheme = ChassisTheme::DarkGraphite;
    LEDTheme currentLEDTheme = LEDTheme::Cyan;
    CRTTheme currentCRTTheme = CRTTheme::Cyan;
    juce::Image customBackground;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkinManager)
};

class StudioLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StudioLookAndFeel() noexcept
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff121316));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff00f0ff));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff2a2d34));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff0f1013));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff20242b));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00f0ff));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd1d5db));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff000000));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff121418));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff333842));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff121418));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe2e8f0));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff2a3242));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xffffffff));

        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff94a3b8));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0x4000f0ff));
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff2d313a));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff111317));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xffe2e8f0));
    }

    ~StudioLookAndFeel() override = default;

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox(slider);
        l->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour(juce::Label::textColourId, SkinManager::getInstance().getSubTextColour());
        l->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        l->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        l->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        l->setColour(juce::TextEditor::focusedOutlineColourId, SkinManager::getInstance().getBorderColour());
        l->setColour(juce::TextEditor::backgroundColourId, SkinManager::getInstance().getSectionBackgroundColour());
        l->setColour(juce::TextEditor::textColourId, SkinManager::getInstance().getTextColour());
        l->setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
        l->setBorderSize(juce::BorderSize<int>(0));
        // Shrink long values instead of eliding them to "128.0..."
        l->setMinimumHorizontalScale (0.5f);
        l->setOpaque(false);
        return l;
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        const auto bg = label.findColour(juce::Label::backgroundColourId);
        if (bg.getAlpha() > 0)
            g.fillAll(bg);

        if (! label.isBeingEdited())
        {
            const auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            const juce::Font font(getLabelFont(label));

            g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
            g.setFont(font);

            const auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());

            g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                             juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / font.getHeight())),
                             label.getMinimumHorizontalScale());

            const auto outline = label.findColour(juce::Label::outlineColourId);
            if (outline.getAlpha() > 0)
            {
                g.setColour(outline);
                g.drawRect(label.getLocalBounds());
            }
        }
    }

    void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        juce::ignoreUnused(width, height);
        const auto bg = textEditor.findColour(juce::TextEditor::backgroundColourId);
        if (bg.getAlpha() > 0)
        {
            g.setColour(bg);
            g.fillRoundedRectangle(textEditor.getLocalBounds().toFloat(), 2.0f);
        }
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        juce::ignoreUnused(width, height);
        const auto outline = textEditor.findColour(textEditor.hasKeyboardFocus(true)
                                                    ? juce::TextEditor::focusedOutlineColourId
                                                    : juce::TextEditor::outlineColourId);
        if (outline.getAlpha() > 0)
        {
            g.setColour(outline);
            g.drawRoundedRectangle(textEditor.getLocalBounds().toFloat().reduced(0.5f), 2.0f, 1.0f);
        }
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto& skin = SkinManager::getInstance();
        const auto ledCol = skin.getLEDColour();
        const auto theme = skin.getChassisTheme();

        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                   static_cast<float>(width), static_cast<float>(height)).reduced(2.0f);

        const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float centreX = bounds.getCentreX();
        const float centreY = bounds.getCentreY();
        const float dialRadius = radius - 6.0f;

        if (dialRadius <= 4.0f)
            return;

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        juce::Path trackPath;
        trackPath.addCentredArc(centreX, centreY, radius - 3.0f, radius - 3.0f,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);

        const juce::Colour grooveColour = (theme == ChassisTheme::VintageCream)
                                            ? juce::Colour(0xffc5bcac)
                                            : juce::Colour(0xff0c0e11);
        g.setColour(grooveColour);
        g.strokePath(trackPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPosProportional > 0.001f)
        {
            juce::Path activeTrack;
            const bool isBipolar = slider.getProperties().contains("bipolar");

            if (isBipolar)
            {
                const float midAngle = (rotaryStartAngle + rotaryEndAngle) * 0.5f;
                if (angle >= midAngle)
                {
                    activeTrack.addCentredArc(centreX, centreY, radius - 3.0f, radius - 3.0f,
                                              0.0f, midAngle, angle, true);
                }
                else
                {
                    activeTrack.addCentredArc(centreX, centreY, radius - 3.0f, radius - 3.0f,
                                              0.0f, angle, midAngle, true);
                }
            }
            else
            {
                activeTrack.addCentredArc(centreX, centreY, radius - 3.0f, radius - 3.0f,
                                          0.0f, rotaryStartAngle, angle, true);
            }

            g.setColour(ledCol.withAlpha(0.25f));
            g.strokePath(activeTrack, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour(ledCol);
            g.strokePath(activeTrack, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(juce::Colour(0x65000000));
        g.fillEllipse(centreX - dialRadius + 1.0f, centreY - dialRadius + 2.5f, dialRadius * 2.0f, dialRadius * 2.0f);

        juce::Colour dialTop = juce::Colour(0xff3a3f4a);
        juce::Colour dialBot = juce::Colour(0xff181a1f);
        juce::Colour dialRim = juce::Colour(0xff4a515e);

        if (theme == ChassisTheme::RolandBlue)
        {
            dialTop = juce::Colour(0xff2d3b50);
            dialBot = juce::Colour(0xff131a26);
            dialRim = juce::Colour(0xff3f5270);
        }
        else if (theme == ChassisTheme::VintageCream)
        {
            dialTop = juce::Colour(0xff453d36);
            dialBot = juce::Colour(0xff201b16);
            dialRim = juce::Colour(0xff8c7f70);
        }
        else if (theme == ChassisTheme::CyberpunkObsidian)
        {
            dialTop = juce::Colour(0xff262933);
            dialBot = juce::Colour(0xff0d0e12);
            dialRim = juce::Colour(0xff333845);
        }

        juce::ColourGradient dialGrad(dialTop, centreX - dialRadius, centreY - dialRadius,
                                    dialBot, centreX + dialRadius, centreY + dialRadius, false);

        g.setGradientFill(dialGrad);
        g.fillEllipse(centreX - dialRadius, centreY - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

        g.setColour(dialRim);
        g.drawEllipse(centreX - dialRadius, centreY - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f, 1.2f);

        const float innerRadius = dialRadius * 0.72f;
        juce::Colour hubTop = (theme == ChassisTheme::VintageCream) ? juce::Colour(0xff2a241f) : juce::Colour(0xff202329);
        juce::Colour hubBot = (theme == ChassisTheme::VintageCream) ? juce::Colour(0xff15120f) : juce::Colour(0xff111316);

        juce::ColourGradient innerGrad(hubTop, centreX, centreY - innerRadius,
                                      hubBot, centreX, centreY + innerRadius, false);

        g.setGradientFill(innerGrad);
        g.fillEllipse(centreX - innerRadius, centreY - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

        const float pointerLength = dialRadius - 2.0f;
        const float pointerStartX = centreX + (innerRadius * 0.35f) * std::cos(angle - 1.57079632f);
        const float pointerStartY = centreY + (innerRadius * 0.35f) * std::sin(angle - 1.57079632f);
        const float pointerEndX   = centreX + pointerLength * std::cos(angle - 1.57079632f);
        const float pointerEndY   = centreY + pointerLength * std::sin(angle - 1.57079632f);

        juce::Path pointerPath;
        pointerPath.startNewSubPath(pointerStartX, pointerStartY);
        pointerPath.lineTo(pointerEndX, pointerEndY);

        g.setColour(ledCol.withAlpha(0.50f));
        g.strokePath(pointerPath, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(juce::Colours::white);
        g.strokePath(pointerPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        juce::ignoreUnused(minSliderPos, maxSliderPos, slider);

        auto& skin = SkinManager::getInstance();
        const auto ledCol = skin.getLEDColour();
        const auto theme = skin.getChassisTheme();

        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                   static_cast<float>(width), static_cast<float>(height));

        const bool isVertical = (style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical);

        if (isVertical)
        {
            const float trackWidth = 5.0f;
            const float trackX = bounds.getCentreX() - (trackWidth * 0.5f);
            const auto trackRect = juce::Rectangle<float>(trackX, bounds.getY() + 8.0f, trackWidth, bounds.getHeight() - 16.0f);

            const juce::Colour trackBg = (theme == ChassisTheme::VintageCream) ? juce::Colour(0xffbcb3a3) : juce::Colour(0xff090a0c);
            const juce::Colour trackRim = (theme == ChassisTheme::VintageCream) ? juce::Colour(0xffa59c8c) : juce::Colour(0xff2a2d34);

            g.setColour(trackBg);
            g.fillRoundedRectangle(trackRect, 2.5f);
            g.setColour(trackRim);
            g.drawRoundedRectangle(trackRect, 2.5f, 1.0f);

            const float activeH = std::max(0.0f, trackRect.getBottom() - sliderPos);
            if (activeH > 1.0f)
            {
                const auto activeRect = juce::Rectangle<float>(trackX + 1.0f, sliderPos, trackWidth - 2.0f, activeH);
                g.setColour(ledCol.withAlpha(0.70f));
                g.fillRoundedRectangle(activeRect, 1.5f);
            }

            const float thumbWidth = std::min(bounds.getWidth() - 4.0f, 28.0f);
            const float thumbHeight = 14.0f;
            const auto thumbRect = juce::Rectangle<float>(
                bounds.getCentreX() - (thumbWidth * 0.5f),
                sliderPos - (thumbHeight * 0.5f),
                thumbWidth, thumbHeight);

            g.setColour(juce::Colour(0x60000000));
            g.fillRoundedRectangle(thumbRect.translated(0.0f, 2.0f), 2.5f);

            juce::Colour thumbTop = juce::Colour(0xff3f444f);
            juce::Colour thumbBot = juce::Colour(0xff181a1f);
            juce::Colour thumbRim = juce::Colour(0xff555d6c);

            if (theme == ChassisTheme::RolandBlue)
            {
                thumbTop = juce::Colour(0xff33445c);
                thumbBot = juce::Colour(0xff162030);
                thumbRim = juce::Colour(0xff4b6182);
            }
            else if (theme == ChassisTheme::VintageCream)
            {
                thumbTop = juce::Colour(0xff4a423a);
                thumbBot = juce::Colour(0xff221d18);
                thumbRim = juce::Colour(0xff776a5b);
            }

            juce::ColourGradient thumbGrad(thumbTop, thumbRect.getX(), thumbRect.getY(),
                                          thumbBot, thumbRect.getX(), thumbRect.getBottom(), false);

            g.setGradientFill(thumbGrad);
            g.fillRoundedRectangle(thumbRect, 2.5f);

            g.setColour(thumbRim);
            g.drawRoundedRectangle(thumbRect, 2.5f, 1.0f);

            g.setColour(ledCol);
            g.drawHorizontalLine(juce::roundToInt(thumbRect.getCentreY()), thumbRect.getX() + 3.0f, thumbRect.getRight() - 3.0f);
        }
        else
        {
            const float trackHeight = 5.0f;
            const float trackY = bounds.getCentreY() - (trackHeight * 0.5f);
            const auto trackRect = juce::Rectangle<float>(bounds.getX() + 8.0f, trackY, bounds.getWidth() - 16.0f, trackHeight);

            const juce::Colour trackBg = (theme == ChassisTheme::VintageCream) ? juce::Colour(0xffbcb3a3) : juce::Colour(0xff090a0c);
            const juce::Colour trackRim = (theme == ChassisTheme::VintageCream) ? juce::Colour(0xffa59c8c) : juce::Colour(0xff2a2d34);

            g.setColour(trackBg);
            g.fillRoundedRectangle(trackRect, 2.5f);
            g.setColour(trackRim);
            g.drawRoundedRectangle(trackRect, 2.5f, 1.0f);

            const float activeW = std::max(0.0f, sliderPos - trackRect.getX());
            if (activeW > 1.0f)
            {
                const auto activeRect = juce::Rectangle<float>(trackRect.getX(), trackY + 1.0f, activeW, trackHeight - 2.0f);
                g.setColour(ledCol.withAlpha(0.70f));
                g.fillRoundedRectangle(activeRect, 1.5f);
            }

            const float thumbWidth = 14.0f;
            const float thumbHeight = std::min(bounds.getHeight() - 4.0f, 26.0f);
            const auto thumbRect = juce::Rectangle<float>(
                sliderPos - (thumbWidth * 0.5f),
                bounds.getCentreY() - (thumbHeight * 0.5f),
                thumbWidth, thumbHeight);

            g.setColour(juce::Colour(0x60000000));
            g.fillRoundedRectangle(thumbRect.translated(1.5f, 1.5f), 2.5f);

            juce::Colour thumbTop = juce::Colour(0xff3f444f);
            juce::Colour thumbBot = juce::Colour(0xff181a1f);
            juce::Colour thumbRim = juce::Colour(0xff555d6c);

            if (theme == ChassisTheme::RolandBlue)
            {
                thumbTop = juce::Colour(0xff33445c);
                thumbBot = juce::Colour(0xff162030);
                thumbRim = juce::Colour(0xff4b6182);
            }
            else if (theme == ChassisTheme::VintageCream)
            {
                thumbTop = juce::Colour(0xff4a423a);
                thumbBot = juce::Colour(0xff221d18);
                thumbRim = juce::Colour(0xff776a5b);
            }

            juce::ColourGradient thumbGrad(thumbTop, thumbRect.getX(), thumbRect.getY(),
                                          thumbBot, thumbRect.getRight(), thumbRect.getY(), false);

            g.setGradientFill(thumbGrad);
            g.fillRoundedRectangle(thumbRect, 2.5f);

            g.setColour(thumbRim);
            g.drawRoundedRectangle(thumbRect, 2.5f, 1.0f);

            g.setColour(ledCol);
            g.drawVerticalLine(juce::roundToInt(thumbRect.getCentreX()), thumbRect.getY() + 3.0f, thumbRect.getBottom() - 3.0f);
        }
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(backgroundColour);

        auto& skin = SkinManager::getInstance();
        const auto ledCol = skin.getLEDColour();

        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const bool isOn = button.getToggleState();

        const bool isStepPad = button.getProperties().contains("isPlayhead")
            || button.getProperties().contains("isSelected")
            || (button.getWidth() <= 32 && button.getHeight() <= 30
                && (button.getButtonText().isEmpty() || button.getButtonText() == "*"));

        if (isStepPad)
        {
            const bool isPlayhead = button.getProperties().getWithDefault("isPlayhead", false);
            const bool isSelected = button.getProperties().getWithDefault("isSelected", false);

            g.setColour(juce::Colour(0x80000000));
            g.fillRoundedRectangle(bounds, 3.0f);

            const auto bezelRect = bounds.reduced(1.0f);
            g.setColour(juce::Colour(0xff111317));
            g.fillRoundedRectangle(bezelRect, 2.5f);

            g.setColour(juce::Colour(0xff2d333f));
            g.drawRoundedRectangle(bezelRect, 2.5f, 1.0f);

            const auto lensRect = bezelRect.reduced(1.5f);

            if (isOn)
            {
                juce::ColourGradient ledGrad(
                    ledCol.withAlpha(0.90f), lensRect.getCentreX(), lensRect.getY(),
                    ledCol.withAlpha(0.45f), lensRect.getCentreX(), lensRect.getBottom(), false);
                g.setGradientFill(ledGrad);
                g.fillRoundedRectangle(lensRect, 1.8f);

                g.setColour(juce::Colours::white.withAlpha(0.65f));
                g.drawHorizontalLine(juce::roundToInt(lensRect.getY() + 1.0f),
                                     lensRect.getX() + 2.0f, lensRect.getRight() - 2.0f);

                g.setColour(ledCol);
                g.drawRoundedRectangle(lensRect, 1.8f, 1.2f);
            }
            else
            {
                juce::Colour unlitTop = shouldDrawButtonAsHighlighted ? juce::Colour(0xff333842) : juce::Colour(0xff22262d);
                juce::Colour unlitBot = shouldDrawButtonAsHighlighted ? juce::Colour(0xff1f2229) : juce::Colour(0xff131518);

                juce::ColourGradient smokedGrad(unlitTop, lensRect.getX(), lensRect.getY(),
                                               unlitBot, lensRect.getX(), lensRect.getBottom(), false);
                g.setGradientFill(smokedGrad);
                g.fillRoundedRectangle(lensRect, 1.8f);

                g.setColour(juce::Colour(0x28ffffff));
                g.drawHorizontalLine(juce::roundToInt(lensRect.getY() + 1.0f),
                                     lensRect.getX() + 2.0f, lensRect.getRight() - 2.0f);

                g.setColour(shouldDrawButtonAsHighlighted ? ledCol.withAlpha(0.50f) : juce::Colour(0xff383f4d));
                g.drawRoundedRectangle(lensRect, 1.8f, 1.0f);

                g.setColour(juce::Colour(0x12ffffff));
                g.fillRoundedRectangle(lensRect.reduced(2.0f), 1.0f);
            }

            if (isSelected)
            {
                g.setColour(ledCol.withAlpha(0.40f));
                g.drawRoundedRectangle(bounds.expanded(1.0f), 3.5f, 2.0f);

                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(bounds, 3.0f, 1.6f);

                g.setColour(ledCol);
                g.drawRoundedRectangle(bezelRect, 2.5f, 1.0f);

                if (!isOn)
                {
                    g.setColour(ledCol.withAlpha(0.18f));
                    g.fillRoundedRectangle(lensRect, 1.8f);
                }

                const float barW = lensRect.getWidth() * 0.70f;
                const auto selBar = juce::Rectangle<float>(
                    lensRect.getCentreX() - (barW * 0.5f),
                    lensRect.getBottom() - 2.5f,
                    barW, 2.0f);

                g.setColour(juce::Colours::white);
                g.fillRoundedRectangle(selBar, 1.0f);
            }

            if (isPlayhead)
            {
                const auto beaconRect = juce::Rectangle<float>(
                    lensRect.getX() + 1.0f, lensRect.getY() + 1.0f,
                    lensRect.getWidth() - 2.0f, 3.5f);

                g.setColour(ledCol.brighter(0.8f).withAlpha(0.65f));
                g.fillRoundedRectangle(beaconRect.expanded(1.0f), 1.5f);

                g.setColour(juce::Colours::white);
                g.fillRoundedRectangle(beaconRect, 1.0f);

                g.setColour(juce::Colours::white.withAlpha(0.90f));
                g.drawRoundedRectangle(lensRect, 1.8f, 1.5f);
            }
        }
        else
        {
            g.setColour(juce::Colour(0x50000000));
            g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f), 3.5f);

            juce::Colour baseTop = isOn ? juce::Colour(0xff1a3545) : juce::Colour(0xff262a32);
            juce::Colour baseBot = isOn ? juce::Colour(0xff0d1e29) : juce::Colour(0xff14161a);

            if (shouldDrawButtonAsDown)
            {
                std::swap(baseTop, baseBot);
            }
            else if (shouldDrawButtonAsHighlighted)
            {
                baseTop = baseTop.brighter(0.15f);
                baseBot = baseBot.brighter(0.15f);
            }

            juce::ColourGradient btnGrad(baseTop, bounds.getX(), bounds.getY(),
                                        baseBot, bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(btnGrad);
            g.fillRoundedRectangle(bounds, 3.5f);

            if (isOn)
            {
                g.setColour(ledCol.withAlpha(0.20f));
                g.fillRoundedRectangle(bounds, 3.5f);

                g.setColour(ledCol.withAlpha(0.85f));
                g.drawRoundedRectangle(bounds, 3.5f, 1.5f);

                const auto glowBar = juce::Rectangle<float>(bounds.getX() + 4.0f, bounds.getY() + 2.0f, bounds.getWidth() - 8.0f, 2.5f);
                g.setColour(ledCol);
                g.fillRoundedRectangle(glowBar, 1.25f);
            }
            else
            {
                g.setColour(juce::Colour(0xff3c424e));
                g.drawRoundedRectangle(bounds, 3.5f, 1.0f);
            }
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        const auto text = button.getButtonText();
        if (text.isEmpty())
            return;

        const auto bounds = button.getLocalBounds();
        auto& skin = SkinManager::getInstance();
        const auto ledCol = skin.getLEDColour();

        if (text == "*")
        {
            g.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
            g.setColour(button.getToggleState() ? juce::Colours::white : ledCol);
            g.drawFittedText("*", bounds, juce::Justification::centred, 1);
            return;
        }

        g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));

        if (button.getToggleState())
            g.setColour(juce::Colours::white);
        else
            g.setColour(skin.getTextColour().withAlpha(0.85f));

        g.drawFittedText(text, bounds, juce::Justification::centred, 1);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override
    {
        juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH, box);

        auto& skin = SkinManager::getInstance();
        const auto ledCol = skin.getLEDColour();

        const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);

        g.setColour(juce::Colour(0xff090b0e));
        g.fillRoundedRectangle(bounds, 3.0f);

        g.setColour(juce::Colour(0xff2d313a));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        const float arrowSize = 6.0f;
        const float arrowRight = bounds.getRight() - 10.0f;
        const float arrowMidY = bounds.getCentreY();

        juce::Path arrow;
        arrow.startNewSubPath(arrowRight - arrowSize, arrowMidY - (arrowSize * 0.4f));
        arrow.lineTo(arrowRight, arrowMidY - (arrowSize * 0.4f));
        arrow.lineTo(arrowRight - (arrowSize * 0.5f), arrowMidY + (arrowSize * 0.5f));
        arrow.closeSubPath();

        g.setColour(ledCol.withAlpha(0.80f));
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(4, 1, box.getWidth() - 17, box.getHeight() - 2);
        label.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
        label.setJustificationType(juce::Justification::centredLeft);
    }

    void drawCornerIndent(juce::Graphics& g, float x, float y, float size) noexcept
    {
        g.setColour(juce::Colour(0xff0a0b0d));
        g.fillEllipse(x - size * 0.5f, y - size * 0.5f, size, size);

        g.setColour(juce::Colour(0xff2a2e36));
        g.drawEllipse(x - size * 0.5f, y - size * 0.5f, size, size, 1.0f);

        g.setColour(juce::Colour(0xff181a1f));
        g.drawLine(x - size * 0.35f, y, x + size * 0.35f, y, 1.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioLookAndFeel)
};