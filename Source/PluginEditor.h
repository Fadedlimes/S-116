#pragma once
#include "S116Layout.h"

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_core/juce_core.h>
#endif

#include "PluginProcessor.h"
#include "UI/SkinManager.h"
#include "UI/SilverLookAndFeel.h"
#include "UI/S116Theme.h"
#include "UI/S116Widgets.h"
#include "UI/OscilloscopeComponent.h"
#include "DSP/SequencerEngine.h"
#include "DSP/ModulationMatrix.h"
#include "DSP/ExpansionCardManager.h"

#include <array>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

// =============================================================================
// Retro 90s/2000s Workstation Dot-Matrix Backlit LCD Screen
// Features authentic hardware POST boot sequence & ROM Cartridge Status Glyph
// =============================================================================
class WorkstationLcdComponent : public juce::Component
{
public:
    WorkstationLcdComponent()
    {
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    ~WorkstationLcdComponent() override = default;

    void triggerBoot() noexcept
    {
        bootFrame = 0;
        isBooting = true;
        repaint();
    }

    void setCardInfo(bool mounted,
                     const juce::String& id = {},
                     const juce::String& name = {},
                     const juce::String& banner1 = {},
                     const juce::String& banner2 = {}) noexcept
    {
        isCardMounted = mounted;
        cardId        = id.toUpperCase();
        cardName      = name.toUpperCase();
        cardBanner1   = banner1.toUpperCase();
        cardBanner2   = banner2.toUpperCase();
        repaint();
    }

    void setParameterEdit(const juce::String& paramName, const juce::String& valueText, float normalizedVal = 0.5f) noexcept
    {
        activeParamName = paramName.toUpperCase();
        activeParamVal  = valueText;
        activeNormVal   = std::clamp(normalizedVal, 0.0f, 1.0f);
        paramHoldFrames = 45; // ~1.5 seconds at 30 Hz
        repaint();
    }

    juce::String getCurrentText() const noexcept { return activeParamName + " / " + activeParamVal; }
    void setPresetInfo(const juce::String& name, const juce::String& subInfo) noexcept
    {
        currentPatchName  = name.toUpperCase();
        currentStatusInfo = subInfo.toUpperCase();
        if (paramHoldFrames == 0 && !isBooting)
            repaint();
    }

    void advanceTimer() noexcept
    {
        if (isBooting)
        {
            ++bootFrame;
            if (bootFrame > 48) // ~1.6 seconds total boot
                isBooting = false;
            repaint();
            return;
        }

        if (paramHoldFrames > 0)
        {
            --paramHoldFrames;
            if (paramHoldFrames == 0)
                repaint();
        }
    }

    bool isBootComplete() const noexcept { return !isBooting; }

    void paint(juce::Graphics& g) override
    {
        auto& skin = SkinManager::getInstance();
        const auto ledCol = skin.getLEDColour();
        const auto bounds = getLocalBounds().toFloat();

        // 1. Recessed Bezel Socket
        g.setColour(juce::Colour(0xff060709));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(skin.getBorderColour().darker(0.4f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

        // 2. Backlit LCD Glass Window
        const auto glassRect = bounds.reduced(2.5f);
        const auto bgBacklight = ledCol.withMultipliedSaturation(0.70f).withMultipliedBrightness(0.10f);
        
        g.setColour(bgBacklight);
        g.fillRoundedRectangle(glassRect, 2.5f);

        // Subtle LCD Scanline / Dot Grid Texture
        g.setColour(juce::Colour(0x0a000000));
        for (float y = glassRect.getY(); y < glassRect.getBottom(); y += 3.0f)
            g.drawHorizontalLine(juce::roundToInt(y), glassRect.getX(), glassRect.getRight());

        // Inner Glass Shadow & Perimeter Retainer
        g.setColour(juce::Colour(0x60000000));
        g.drawRoundedRectangle(glassRect, 2.5f, 1.0f);

        g.setColour(ledCol.withAlpha(0.25f));
        g.drawRoundedRectangle(glassRect.reduced(0.5f), 2.0f, 0.8f);

        // 3. Phosphor Pixel Glow & Text Rendering
        const auto pixelCol = ledCol.brighter(0.65f);
        const auto ghostCol = ledCol.withAlpha(0.08f);

        juce::Font lcdFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::bold));
        g.setFont(lcdFont);

        float textStartX = glassRect.getX() + 8.0f;
        const float textWidth = glassRect.getWidth() - 16.0f;

        // Draw Dot-Matrix ROM Cartridge Glyph if Card is Mounted
        if (isCardMounted && !isBooting && paramHoldFrames == 0)
        {
            drawCartridgeGlyph(g, textStartX, glassRect.getY() + 4.0f, ledCol, pixelCol);
            textStartX += 16.0f;
        }

        const auto line1Rect = juce::Rectangle<float>(textStartX, glassRect.getY() + 3.0f,
                                                      textWidth - (isCardMounted ? 16.0f : 0.0f), 14.0f);
        const auto line2Rect = juce::Rectangle<float>(glassRect.getX() + 8.0f, glassRect.getY() + 18.0f,
                                                      textWidth, 14.0f);

        juce::String textLine1, textLine2;

        if (isBooting)
        {
            if (bootFrame <= 18)
            {
                textLine1 = "S-116 // WORKSTATION OS v2.0";
                textLine2 = "CHECKING VOICES [8+8].. OK";
            }
            else if (bootFrame <= 36)
            {
                if (isCardMounted)
                {
                    textLine1 = "SLOT 1: [" + cardId + "] " + (cardBanner1.isNotEmpty() ? cardBanner1 : "CARD DETECTED");
                    textLine2 = cardBanner2.isNotEmpty() ? cardBanner2 : (cardName + " READY");
                }
                else
                {
                    textLine1 = "SLOT 1: [INTERNAL ROM BANK]";
                    textLine2 = "64 FACTORY PRESETS READY";
                }
            }
            else
            {
                textLine1 = isCardMounted ? ("[" + cardId + "] " + cardName) : "S-116 DUAL TRACK ENGINE";
                textLine2 = "EXPANSION BUS ONLINE";
            }
        }
        else if (paramHoldFrames > 0)
        {
            textLine1 = "EDIT: " + activeParamName;

            const int totalBars = 10;
            const int activeBars = std::clamp(juce::roundToInt(activeNormVal * static_cast<float>(totalBars)), 0, totalBars);
            juce::String meterBar;
            for (int b = 0; b < totalBars; ++b)
                meterBar += (b < activeBars) ? "#" : "-";

            textLine2 = activeParamVal + "  [" + meterBar + "]";
        }
        else
        {
            if (isCardMounted)
                textLine1 = "[" + cardId + "] " + (currentPatchName.isEmpty() ? "INIT PATCH" : currentPatchName);
            else
                textLine1 = "P: " + (currentPatchName.isEmpty() ? "INIT PATCH" : currentPatchName);

            textLine2 = currentStatusInfo.isEmpty() ? "TRK A [CH 1] | POLY 8 BI-TIMBRAL" : currentStatusInfo;
        }

        // Ghosted text background
        g.setColour(ghostCol);
        g.drawText(textLine1, line1Rect.translated(1.0f, 1.0f), juce::Justification::centredLeft, false);
        g.drawText(textLine2, line2Rect.translated(1.0f, 1.0f), juce::Justification::centredLeft, false);

        // Luminous active pixels with ambient glow
        g.setColour(ledCol.withAlpha(0.35f));
        g.drawText(textLine1, line1Rect, juce::Justification::centredLeft, false);
        g.drawText(textLine2, line2Rect, juce::Justification::centredLeft, false);

        g.setColour(pixelCol);
        g.drawText(textLine1, line1Rect, juce::Justification::centredLeft, false);
        g.drawText(textLine2, line2Rect, juce::Justification::centredLeft, false);
    }

private:
    void drawCartridgeGlyph(juce::Graphics& g, float x, float y, const juce::Colour& glowCol, const juce::Colour& pixCol)
    {
        const auto cardBody = juce::Rectangle<float>(x, y + 1.0f, 11.0f, 9.0f);
        
        g.setColour(glowCol.withAlpha(0.30f));
        g.fillRoundedRectangle(cardBody.expanded(0.8f), 1.5f);

        g.setColour(pixCol);
        g.drawRoundedRectangle(cardBody, 1.2f, 1.0f);

        g.fillRect(x + 2.5f, y + 3.0f, 6.0f, 3.0f);

        g.fillRect(x + 2.0f, y + 10.0f, 1.5f, 2.0f);
        g.fillRect(x + 4.8f, y + 10.0f, 1.5f, 2.0f);
        g.fillRect(x + 7.5f, y + 10.0f, 1.5f, 2.0f);
    }

    bool isBooting = true;
    int bootFrame = 0;
    int paramHoldFrames = 0;

    bool isCardMounted = false;
    juce::String cardId;
    juce::String cardName;
    juce::String cardBanner1;
    juce::String cardBanner2;

    juce::String currentPatchName = "INIT PATCH";
    juce::String currentStatusInfo = "TRK A [CH 1] | POLY 8 BI-TIMBRAL";

    juce::String activeParamName;
    juce::String activeParamVal;
    float activeNormVal = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorkstationLcdComponent)
};

// =============================================================================
// Dedicated Dynamic Vector ADSR CRT/LCD Screen Component
// Renders live anti-aliased logarithmic attack, decay, sustain & release curves
// =============================================================================
class EnvelopeVisualizerLcdComponent : public juce::Component
{
public:
    EnvelopeVisualizerLcdComponent()
    {
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    ~EnvelopeVisualizerLcdComponent() override = default;

    void setEnvelope(const juce::String& name,
                     float a, float d, float s, float r,
                     const juce::String& aStr,
                     const juce::String& dStr,
                     const juce::String& sStr,
                     const juce::String& rStr) noexcept
    {
        envName     = name.toUpperCase();
        attackNorm  = std::clamp(a, 0.0f, 1.0f);
        decayNorm   = std::clamp(d, 0.0f, 1.0f);
        sustainNorm = std::clamp(s, 0.0f, 1.0f);
        releaseNorm = std::clamp(r, 0.0f, 1.0f);

        attackText  = aStr;
        decayText   = dStr;
        sustainText = sStr;
        releaseText = rStr;

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto& skin = SkinManager::getInstance();
        const auto ledCol = skin.getLEDColour();
        const auto bounds = getLocalBounds().toFloat();

        // 1. Recessed Bezel Socket
        g.setColour(juce::Colour(0xff060709));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(skin.getBorderColour().darker(0.4f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

        // 2. Backlit Phosphor Glass Window
        const auto glassRect = bounds.reduced(2.5f);
        const auto bgBacklight = ledCol.withMultipliedSaturation(0.70f).withMultipliedBrightness(0.08f);

        g.setColour(bgBacklight);
        g.fillRoundedRectangle(glassRect, 2.5f);

        // CRT Scanline Grid Texture
        g.setColour(juce::Colour(0x0a000000));
        for (float y = glassRect.getY(); y < glassRect.getBottom(); y += 3.0f)
            g.drawHorizontalLine(juce::roundToInt(y), glassRect.getX(), glassRect.getRight());

        // Inner Glass Retainer Outline
        g.setColour(juce::Colour(0x50000000));
        g.drawRoundedRectangle(glassRect, 2.5f, 1.0f);

        g.setColour(ledCol.withAlpha(0.20f));
        g.drawRoundedRectangle(glassRect.reduced(0.5f), 2.0f, 0.8f);

        // 3. LCD Header Readout
        const auto pixelCol = ledCol.brighter(0.65f);
        const auto ghostCol = ledCol.withAlpha(0.08f);

        juce::Font headerFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold));
        g.setFont(headerFont);

        const auto titleRect = juce::Rectangle<float>(glassRect.getX() + 8.0f, glassRect.getY() + 3.0f, 140.0f, 12.0f);
        const auto readoutsRect = juce::Rectangle<float>(glassRect.getRight() - 170.0f, glassRect.getY() + 3.0f, 162.0f, 12.0f);

        const juce::String readoutsText = "A:" + attackText + " D:" + decayText + " S:" + sustainText + " R:" + releaseText;

        g.setColour(ghostCol);
        g.drawText(envName, titleRect.translated(1.0f, 1.0f), juce::Justification::centredLeft, false);
        g.drawText(readoutsText, readoutsRect.translated(1.0f, 1.0f), juce::Justification::centredRight, false);

        g.setColour(pixelCol);
        g.drawText(envName, titleRect, juce::Justification::centredLeft, false);
        g.drawText(readoutsText, readoutsRect, juce::Justification::centredRight, false);

        // 4. Real-Time Vector ADSR Curve Calculation & Rendering
        const float graphX = glassRect.getX() + 10.0f;
        const float graphY = glassRect.getY() + 18.0f;
        const float graphW = glassRect.getWidth() - 20.0f;
        const float graphH = glassRect.getHeight() - 24.0f;

        const float yBase = graphY + graphH;
        const float yPeak = graphY + 3.0f;
        const float ySus  = yBase - (sustainNorm * (yBase - yPeak));

        // Proportional horizontal spacing for stages
        const float wA = std::max(6.0f, (0.08f + 0.28f * attackNorm) * graphW);
        const float wD = std::max(6.0f, (0.08f + 0.28f * decayNorm) * graphW);
        const float wR = std::max(6.0f, (0.08f + 0.28f * releaseNorm) * graphW);
        
        float wHold = graphW - (wA + wD + wR);
        if (wHold < 14.0f)
            wHold = 14.0f;

        const float totalW = wA + wD + wHold + wR;
        const float scale = graphW / totalW;

        const float p0x = graphX;
        const float p0y = yBase;

        const float p1x = p0x + (wA * scale);
        const float p1y = yPeak;

        const float p2x = p1x + (wD * scale);
        const float p2y = ySus;

        const float p3x = p2x + (wHold * scale);
        const float p3y = ySus;

        const float p4x = graphX + graphW;
        const float p4y = yBase;

        // Build Smooth ADSR Curve Path
        juce::Path curvePath;
        curvePath.startNewSubPath(p0x, p0y);

        curvePath.quadraticTo((p0x + p1x) * 0.5f, (p0y + p1y) * 0.5f - 4.0f, p1x, p1y);
        curvePath.quadraticTo((p1x + p2x) * 0.5f, p2y - 2.0f, p2x, p2y);
        curvePath.lineTo(p3x, p3y);
        curvePath.quadraticTo((p3x + p4x) * 0.5f, (p3y + p4y) * 0.5f + 4.0f, p4x, p4y);

        juce::Path fillPath = curvePath;
        fillPath.lineTo(p4x, yBase);
        fillPath.lineTo(p0x, yBase);
        fillPath.closeSubPath();

        juce::ColourGradient fillGrad(ledCol.withAlpha(0.22f), 0.0f, yPeak,
                                     ledCol.withAlpha(0.02f), 0.0f, yBase, false);
        g.setGradientFill(fillGrad);
        g.fillPath(fillPath);

        g.setColour(ledCol.withAlpha(0.15f));
        g.drawHorizontalLine(juce::roundToInt(yBase), graphX, graphX + graphW);

        g.setColour(ledCol.withAlpha(0.35f));
        g.strokePath(curvePath, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(pixelCol);
        g.strokePath(curvePath, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto drawNode = [&](float nx, float ny) {
            g.setColour(ledCol.withAlpha(0.50f));
            g.fillEllipse(nx - 2.5f, ny - 2.5f, 5.0f, 5.0f);
            g.setColour(pixelCol);
            g.fillEllipse(nx - 1.5f, ny - 1.5f, 3.0f, 3.0f);
        };

        drawNode(p0x, p0y);
        drawNode(p1x, p1y);
        drawNode(p2x, p2y);
        drawNode(p3x, p3y);
        drawNode(p4x, p4y);
    }

private:
    juce::String envName = "VCF (FILTER) ENVELOPE";
    float attackNorm  = 0.10f;
    float decayNorm   = 0.30f;
    float sustainNorm = 0.70f;
    float releaseNorm = 0.35f;

    juce::String attackText  = "10ms";
    juce::String decayText   = "200ms";
    juce::String sustainText = "70%";
    juce::String releaseText = "350ms";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeVisualizerLcdComponent)
};

// =============================================================================
// Dedicated Vector Sequencer LCD Component
// Visualizes 16-step grid, active playhead beam, velocity bars, per-step gate bars,
// P-lock indicators, ratchet badges, and handles interactive QWERTY step programming.
// =============================================================================
class VectorSequencerLcdComponent : public juce::Component
{
public:
    VectorSequencerLcdComponent()
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(true);
    }

    ~VectorSequencerLcdComponent() override = default;

    void updateDisplayState(int currentPlayhead,
                            int selectedStepIndex,
                            int pageIndex,
                            TrackID trackId,
                            bool isPlayingState,
                            double tempoBpm,
                            bool syncToHostState) noexcept
    {
        playheadStep = currentPlayhead;
        selectedStep = selectedStepIndex;
        activePage   = pageIndex;
        activeTrack  = trackId;
        isPlaying    = isPlayingState;
        bpm          = tempoBpm;
        syncToHost   = syncToHostState;
        repaint();
    }

    // Callbacks for step edits triggered via mouse or QWERTY keys
    std::function<void(int stepIndex)> onStepClicked;
    std::function<void(int stepIndex, float newVelocity)> onStepVelocityChanged;
    std::function<void(int stepIndex, float newGate)> onStepGateChanged;
    std::function<void(int stepIndex)> onStepToggleTrigger;
    std::function<void(int stepIndex, int semitones)> onStepTranspose;
    std::function<void(int stepOffset)> onStepNavigate;
    std::function<void(int stepIndex)> onStepAudition;

    void focusGained(FocusChangeType) override { isQwertyFocused = true; repaint(); }
    void focusLost(FocusChangeType) override   { isQwertyFocused = false; repaint(); }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.isKeyCode(juce::KeyPress::leftKey) || key.getKeyCode() == 'a' || key.getKeyCode() == 'A')
        {
            if (onStepNavigate) onStepNavigate(-1);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::rightKey) || key.getKeyCode() == 'd' || key.getKeyCode() == 'D')
        {
            if (onStepNavigate) onStepNavigate(1);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::upKey) || key.getKeyCode() == 'w' || key.getKeyCode() == 'W')
        {
            const int delta = key.getModifiers().isShiftDown() ? 12 : 1;
            if (onStepTranspose) onStepTranspose(selectedStep, delta);
            if (onStepAudition) onStepAudition(selectedStep);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::downKey) || key.getKeyCode() == 's' || key.getKeyCode() == 'S')
        {
            const int delta = key.getModifiers().isShiftDown() ? -12 : -1;
            if (onStepTranspose) onStepTranspose(selectedStep, delta);
            if (onStepAudition) onStepAudition(selectedStep);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::spaceKey))
        {
            if (onStepToggleTrigger) onStepToggleTrigger(selectedStep);
            if (onStepAudition) onStepAudition(selectedStep);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::escapeKey))
        {
            giveAwayKeyboardFocus();
            return true;
        }
        return false;
    }

    void paint(juce::Graphics& g) override
    {
        auto& skin = SkinManager::getInstance();
        const auto ledCol = skin.getLEDColour();
        const auto bounds = getLocalBounds().toFloat();

        // 1. Recessed Bezel Socket
        g.setColour(juce::Colour(0xff060709));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(skin.getBorderColour().darker(0.4f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

        // 2. Backlit Phosphor Glass Window
        const auto glassRect = bounds.reduced(2.5f);
        const auto bgBacklight = ledCol.withMultipliedSaturation(0.70f).withMultipliedBrightness(0.08f);

        g.setColour(bgBacklight);
        g.fillRoundedRectangle(glassRect, 2.5f);

        // CRT Scanline Grid Texture
        g.setColour(juce::Colour(0x0a000000));
        for (float y = glassRect.getY(); y < glassRect.getBottom(); y += 3.0f)
            g.drawHorizontalLine(juce::roundToInt(y), glassRect.getX(), glassRect.getRight());

        // Inner Glass Shadow & Retainer
        g.setColour(juce::Colour(0x50000000));
        g.drawRoundedRectangle(glassRect, 2.5f, 1.0f);

        g.setColour(ledCol.withAlpha(0.20f));
        g.drawRoundedRectangle(glassRect.reduced(0.5f), 2.0f, 0.8f);

        // 3. Status HUD Header Line
        const auto pixelCol = ledCol.brighter(0.65f);
        const auto ghostCol = ledCol.withAlpha(0.08f);

        juce::Font hudFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.5f, juce::Font::bold));
        g.setFont(hudFont);

        const float hudY = glassRect.getY() + 3.0f;
        const juce::String trkStr = (activeTrack == TrackID::TrackA) ? "TRK A [CH 1]" : "TRK B [CH 2]";
        const juce::String bpmStr = "BPM:" + juce::String(bpm, 1) + (syncToHost ? " [HOST]" : " [INT]");
        const juce::String qwertStr = isQwertyFocused ? "QWERTY: [<-> NAV | SPC: TOGGLE | UP/DN: NOTE]" : "CLICK TO FOCUS QWERTY";

        g.setColour(ghostCol);
        g.drawText(trkStr, glassRect.getX() + 6.0f + 1.0f, hudY + 1.0f, 110.0f, 12.0f, juce::Justification::centredLeft, false);
        g.drawText(bpmStr, glassRect.getX() + 120.0f + 1.0f, hudY + 1.0f, 120.0f, 12.0f, juce::Justification::centredLeft, false);
        g.drawText(qwertStr, glassRect.getRight() - 200.0f + 1.0f, hudY + 1.0f, 194.0f, 12.0f, juce::Justification::centredRight, false);

        g.setColour(pixelCol);
        g.drawText(trkStr, glassRect.getX() + 6.0f, hudY, 110.0f, 12.0f, juce::Justification::centredLeft, false);
        g.drawText(bpmStr, glassRect.getX() + 120.0f, hudY, 120.0f, 12.0f, juce::Justification::centredLeft, false);

        g.setColour(isQwertyFocused ? pixelCol : ledCol.withAlpha(0.40f));
        g.drawText(qwertStr, glassRect.getRight() - 200.0f, hudY, 194.0f, 12.0f, juce::Justification::centredRight, false);

        // 4. 16-Step Grid Drawing
        const float gridX = glassRect.getX() + 6.0f;
        const float gridY = glassRect.getY() + 17.0f;
        const float gridW = glassRect.getWidth() - 12.0f;
        const float gridH = glassRect.getHeight() - 20.0f;
        const float stepW = gridW / 16.0f;

        const int pageOffset = activePage * 16;

        for (int i = 0; i < 16; ++i)
        {
            const int stepIdx = pageOffset + i;
            const float sx = gridX + (static_cast<float>(i) * stepW);
            const auto colRect = juce::Rectangle<float>(sx, gridY, stepW - 2.0f, gridH);

            const bool isStepSelected  = (selectedStep == stepIdx);
            const bool isStepPlayhead  = isPlaying && (playheadStep == stepIdx);
            const bool isStepActive    = stepActiveCache[static_cast<size_t>(i)];
            const float stepVel        = stepVelocityCache[static_cast<size_t>(i)];
            const float stepGate       = stepGateCache[static_cast<size_t>(i)];
            const juce::String noteStr = stepNoteCache[static_cast<size_t>(i)];
            const bool hasLock         = stepLockCache[static_cast<size_t>(i)];
            const int ratchet          = stepRatchetCache[static_cast<size_t>(i)];

            // Subtle step column background
            g.setColour(isStepSelected ? ledCol.withAlpha(0.18f) : juce::Colour(0x12000000));
            g.fillRoundedRectangle(colRect, 1.5f);

            // Step Number Header (e.g. 01..16)
            juce::Font stepFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.5f, juce::Font::bold));
            g.setFont(stepFont);
            g.setColour(isStepSelected ? pixelCol : ledCol.withAlpha(0.50f));
            const juce::String numStr = juce::String(stepIdx + 1).paddedLeft('0', 2);
            g.drawText(numStr, colRect.getX(), colRect.getY() + 1.0f, colRect.getWidth(), 10.0f, juce::Justification::centred, false);

            // Trigger Dot / LED
            const float dotSize = 4.5f;
            const float dotX = colRect.getCentreX() - (dotSize * 0.5f);
            const float dotY = colRect.getY() + 12.0f;
            if (isStepActive)
            {
                g.setColour(pixelCol);
                g.fillEllipse(dotX, dotY, dotSize, dotSize);
            }
            else
            {
                g.setColour(ledCol.withAlpha(0.20f));
                g.drawEllipse(dotX, dotY, dotSize, dotSize, 0.8f);
            }

            // Note Pitch Readout
            juce::Font noteFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.0f, juce::Font::plain));
            g.setFont(noteFont);
            g.setColour(isStepActive ? pixelCol : ledCol.withAlpha(0.25f));
            g.drawText(isStepActive ? noteStr : "--", colRect.getX(), colRect.getY() + 18.0f, colRect.getWidth(), 10.0f, juce::Justification::centred, false);

            // Vertical Velocity Bar
            const float velAreaY = colRect.getY() + 30.0f;
            const float velAreaH = 34.0f;
            const float barW = 8.0f;
            const float barX = colRect.getCentreX() - (barW * 0.5f);

            // Bar background slot
            g.setColour(ledCol.withAlpha(0.08f));
            g.fillRect(barX, velAreaY, barW, velAreaH);

            if (isStepActive && stepVel > 0.001f)
            {
                const float barH = std::clamp(stepVel * velAreaH, 1.0f, velAreaH);
                const float barY = velAreaY + (velAreaH - barH);

                juce::ColourGradient barGrad(pixelCol, 0.0f, barY, ledCol.withAlpha(0.40f), 0.0f, velAreaY + velAreaH, false);
                g.setGradientFill(barGrad);
                g.fillRect(barX, barY, barW, barH);
            }

            // Per-Step Gate Length Stripe
            const float gateY = velAreaY + velAreaH + 2.0f;
            const float gateH = 4.0f;
            g.setColour(ledCol.withAlpha(0.10f));
            g.fillRect(colRect.getX() + 2.0f, gateY, colRect.getWidth() - 4.0f, gateH);

            if (isStepActive)
            {
                // The stripe spans 0%..200% of the step, which is exactly the
                // range of the STEP GATE dial, so the bar position and the
                // position the user can drag to always mean the same thing.
                const float effectiveGate = (stepGate > 0.0f) ? stepGate : globalGate;
                const float gateNorm = std::clamp(effectiveGate / 2.0f, 0.0f, 1.0f);
                const float gateBarW = (colRect.getWidth() - 4.0f) * gateNorm;
                g.setColour(stepGate > 0.0f ? ledCol.brighter(0.4f) : ledCol.withAlpha(0.50f));
                g.fillRect(colRect.getX() + 2.0f, gateY, gateBarW, gateH);
            }

            // P-Lock & Ratchet Badges
            const float badgeY = gateY + gateH + 2.0f;
            if (hasLock)
            {
                // Vector diamond: the U+25C6 glyph is missing from many Linux fonts
                // and renders as a "tofu" box, so the badge is drawn as a path.
                g.setColour(juce::Colour(0xffffcc00)); // Gold diamond for P-Locks
                const float diamondSize = 5.0f;
                const float diamondCx = colRect.getCentreX();
                const float diamondCy = badgeY + 4.0f;

                juce::Path diamond;
                diamond.startNewSubPath (diamondCx, diamondCy - diamondSize * 0.5f);
                diamond.lineTo (diamondCx + diamondSize * 0.5f, diamondCy);
                diamond.lineTo (diamondCx, diamondCy + diamondSize * 0.5f);
                diamond.lineTo (diamondCx - diamondSize * 0.5f, diamondCy);
                diamond.closeSubPath();

                g.fillPath (diamond);
            }
            else if (ratchet > 1)
            {
                g.setColour(pixelCol);
                g.setFont(juce::Font(juce::FontOptions(7.0f).withStyle("Bold")));
                g.drawText(juce::String(ratchet) + "x", colRect.getX(), badgeY, colRect.getWidth(), 8.0f, juce::Justification::centred, false);
            }

            // QWERTY Selection Cursor Bracket
            if (isStepSelected)
            {
                const auto cursorRect = colRect.expanded(1.0f, 1.0f);
                g.setColour(isQwertyFocused ? pixelCol : ledCol.withAlpha(0.60f));
                g.drawRoundedRectangle(cursorRect, 2.0f, isQwertyFocused ? 1.5f : 1.0f);

                // Distinct Corner Accent Marks
                const float cL = 3.0f;
                g.drawLine(cursorRect.getX(), cursorRect.getY(), cursorRect.getX() + cL, cursorRect.getY(), 2.0f);
                g.drawLine(cursorRect.getX(), cursorRect.getY(), cursorRect.getX(), cursorRect.getY() + cL, 2.0f);
                g.drawLine(cursorRect.getRight() - cL, cursorRect.getY(), cursorRect.getRight(), cursorRect.getY(), 2.0f);
                g.drawLine(cursorRect.getRight(), cursorRect.getY(), cursorRect.getRight(), cursorRect.getY() + cL, 2.0f);
            }

            // Sweeping Real-Time Playhead Beam
            if (isStepPlayhead)
            {
                g.setColour(pixelCol.withAlpha(0.35f));
                g.fillRect(colRect);

                g.setColour(pixelCol);
                g.drawVerticalLine(juce::roundToInt(colRect.getCentreX()), colRect.getY(), colRect.getBottom());
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();

        const auto glassRect = getLocalBounds().toFloat().reduced(2.5f);
        const float gridX = glassRect.getX() + 6.0f;
        const float gridW = glassRect.getWidth() - 12.0f;
        const float stepW = gridW / 16.0f;

        if (e.x >= gridX && e.x <= gridX + gridW)
        {
            const int col = std::clamp(static_cast<int>((e.x - gridX) / stepW), 0, 15);
            const int targetStep = activePage * 16 + col;

            if (selectedStep != targetStep)
            {
                selectedStep = targetStep;
                if (onStepClicked) onStepClicked(selectedStep);
            }
            else
            {
                // Clicking already selected step toggles trigger active
                if (onStepToggleTrigger) onStepToggleTrigger(selectedStep);
                if (onStepAudition) onStepAudition(selectedStep);
            }
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const auto glassRect = getLocalBounds().toFloat().reduced(2.5f);
        const float gridX = glassRect.getX() + 6.0f;
        const float gridW = glassRect.getWidth() - 12.0f;
        const float stepW = gridW / 16.0f;

        if (e.x >= gridX && e.x <= gridX + gridW)
        {
            const int col = std::clamp(static_cast<int>((e.x - gridX) / stepW), 0, 15);
            const int targetStep = activePage * 16 + col;

            const float velAreaY = glassRect.getY() + 17.0f + 30.0f;
            const float velAreaH = 34.0f;

            // Per-step gate stripe sits directly underneath the velocity column.
            const float gateY = velAreaY + velAreaH + 2.0f;
            const float gateH = 4.0f;

            if (e.y >= velAreaY && e.y <= velAreaY + velAreaH)
            {
                const float normVel = std::clamp(1.0f - ((e.y - velAreaY) / velAreaH), 0.05f, 1.0f);
                if (onStepVelocityChanged) onStepVelocityChanged(targetStep, normVel);
                repaint();
            }
            else if (e.y >= gateY - 1.0f && e.y <= gateY + gateH + 1.0f)
            {
                // Horizontal drag across the stripe sets the note length of that step.
                const auto colRect = juce::Rectangle<float>(gridX + (static_cast<float>(col) * stepW),
                                                             glassRect.getY() + 17.0f,
                                                             stepW - 2.0f,
                                                             glassRect.getHeight() - 20.0f);
                const float usable = std::max(1.0f, colRect.getWidth() - 4.0f);
                const float norm = std::clamp((e.x - (colRect.getX() + 2.0f)) / usable, 0.0f, 1.0f);

                // The stripe spans 0%..200% of the step: dragging to the far left
                // hands the step back to the track gate (TRK).
                const float newGate = norm * 2.0f;

                if (onStepGateChanged) onStepGateChanged(targetStep, newGate);
                repaint();
            }
        }
    }

    void cacheStep(int indexInPage, bool active, float velocity, float gate, const juce::String& noteName, bool hasLock, int ratchet) noexcept
    {
        if (indexInPage >= 0 && indexInPage < 16)
        {
            stepActiveCache[static_cast<size_t>(indexInPage)]   = active;
            stepVelocityCache[static_cast<size_t>(indexInPage)] = velocity;
            stepGateCache[static_cast<size_t>(indexInPage)]     = gate;
            stepNoteCache[static_cast<size_t>(indexInPage)]     = noteName;
            stepLockCache[static_cast<size_t>(indexInPage)]     = hasLock;
            stepRatchetCache[static_cast<size_t>(indexInPage)]  = ratchet;
        }
    }

    void setGlobalGate(float g) noexcept { globalGate = std::clamp(g, 0.05f, 1.5f); }

private:
    int playheadStep = -1;
    int selectedStep = 0;
    int activePage   = 0;
    TrackID activeTrack = TrackID::TrackA;
    bool isPlaying   = false;
    double bpm       = 120.0;
    bool syncToHost  = true;
    bool isQwertyFocused = false;
    float globalGate = 0.75f;

    std::array<bool, 16> stepActiveCache {};
    std::array<float, 16> stepVelocityCache {};
    std::array<float, 16> stepGateCache {};
    std::array<juce::String, 16> stepNoteCache {};
    std::array<bool, 16> stepLockCache {};
    std::array<int, 16> stepRatchetCache {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VectorSequencerLcdComponent)
};

// =============================================================================
// Main Audio Processor Editor
// =============================================================================
class EditorCanvas : public juce::Component
{
public:
    // Note: a default constructor has to be declared explicitly, because the
    // JUCE_DECLARE_NON_COPYABLE macro below counts as a user declared constructor.
    EditorCanvas() = default;

    std::function<void(juce::Graphics&)> onPaint;

    void paint(juce::Graphics& g) override
    {
        if (onPaint != nullptr)
            onPaint(g);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorCanvas)
};

class Simple116AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::ChangeListener,
                                      private juce::Timer
{
public:
    static constexpr int UI_WIDTH = 1360;
    static constexpr int UI_HEIGHT = 820;

    explicit Simple116AudioProcessorEditor(Simple106AudioProcessor& processor);
    ~Simple116AudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Screen fitting: the console has a fixed 1360x820 canvas. On displays that
    // cannot show it at 1:1 the whole console is scaled down uniformly.
    void changeListenerCallback (juce::ChangeBroadcaster*) override { studioLookAndFeel.applyTheme(); sendLookAndFeelChange(); }

    void applyFitScale();
    void layoutS116();
    void runSelfTest();
    S116::layout::DecorSet decor;
    bool zOrderDone = false;
    void paintChassis(juce::Graphics& g);

    // Preset & Expansion Card Disk Management
    void refreshPresetList();
    void savePresetDialog();
    void showExpansionCardMenu();
    void exportCardDialog();
    void updateCardStatusUI();

    // Unified Envelope Engine Methods
    void setActiveEnvelope(int envIndex);
    void updateEnvelopeLcd() noexcept;

    void refreshMatrixUI();
    void updateTrackStatusUI();

    // Elektron-Style P-Lock Knob Synchronization
    void syncKnobsToStepLocksOrBase() noexcept;

    // Sequencer LCD & Step Management
    void updateSequencerLcd() noexcept;
    void refreshSelectedStepUI() noexcept;
    void auditionSelectedStep() noexcept;

    // Sub Oscillator Waveform (Sine / Square)
    void updateSubWaveButton() noexcept;

private:
    void timerCallback() override;

    void setupRotaryKnob(juce::Slider& slider,
                         juce::Label& label,
                         const juce::String& text,
                         double minVal,
                         double maxVal,
                         double defaultVal,
                         const juce::String& suffix = "",
                         bool isBipolar = false);

    void setupLinearFader(juce::Slider& slider,
                          juce::Label& label,
                          const juce::String& text,
                          double minVal,
                          double maxVal,
                          double defaultVal);

    void setupButton(juce::Button& button,
                     const juce::String& text,
                     bool isToggleable = true);

    void setupComboBox(juce::ComboBox& box,
                       juce::Label& label,
                       const juce::String& text,
                       const juce::StringArray& items,
                       int defaultId = 1);

    static void configureFocus(juce::Component& c) noexcept
    {
        c.setWantsKeyboardFocus(false);
        c.setMouseClickGrabsKeyboardFocus(false);
    }

    void drawSectionPanel(juce::Graphics& g,
                          const juce::Rectangle<float>& bounds,
                          const juce::String& title,
                          const juce::Colour& ledCol);

    void attachSlider(juce::Slider& slider, const juce::String& paramId);
    void attachComboBox(juce::ComboBox& box, const juce::String& paramId);
    void attachButton(juce::Button& button, const juce::String& paramId);

    Simple106AudioProcessor& audioProcessor;
    StudioLookAndFeel studioLookAndFeel;

    // All console widgets live on this canvas so that the entire UI can be
    // scaled uniformly (see applyFitScale) without touching the layout code.
    juce::Component content;
    S116::ChassisBackground chassisBg;
    S116::HeaderBar headerBar;
    S116::SectionPanel oscPanel { "OSCILLATORS" };
    S116::SectionPanel filtPanel { "FILTER" };
    S116::SectionPanel scopePanel { "SCOPE" };
    S116::SectionPanel envPanel { "ENVELOPE" };
    S116::SectionPanel lfoPanel { "LFO / MOD" };
    S116::SectionPanel seqPanel { "SEQUENCER / ARP" };
    S116::SectionPanel fxPanel { "MASTER EFFECTS" };
    S116::CrtScopeFrame scopeFrame;
    float uiScale = 1.0f;
    bool isApplyingFitScale = false;

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboBoxAttachment>> comboBoxAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    int selectedStepIndex = 0;
    bool isUpdatingKnobsFromStep = false;

    // File Chooser for native Preset Save & Card Loading/Exporting
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Header Controls & Retro Workstation LCD Screen
    juce::Label titleBadgeLabel;
    juce::ComboBox presetComboBox;
    juce::Label presetLabel;
    juce::TextButton prevPresetButton;
    juce::TextButton nextPresetButton;
    juce::TextButton savePresetButton;
    juce::TextButton initPresetButton;

    // Hardware Expansion Card Cartridge Slot Button
    juce::TextButton cardButton;

    juce::ComboBox voiceModeComboBox;
    juce::Label voiceModeLabel;
    juce::Slider masterVolumeSlider;
    juce::Label masterVolumeLabel;

    juce::TextButton chassisThemeButton;
    juce::TextButton ledThemeButton;
    juce::TextButton crtThemeButton;

    WorkstationLcdComponent lcdScreen;

    // =========================================================================
    // Oscillator Section Controls (Symmetrical 6x2 DCO Grid + Dedicated Controls)
    // =========================================================================
    juce::Slider octaveSlider;
    juce::Label octaveLabel;
    juce::Slider oscMorphSlider;
    juce::Label oscMorphLabel;
    juce::Slider oscPWMSlider;
    juce::Label oscPWMLabel;
    juce::Slider supersawMixSlider;
    juce::Label supersawMixLabel;
    juce::Slider supersawDetuneSlider;
    juce::Label supersawDetuneLabel;
    juce::Slider wavefoldSlider;
    juce::Label wavefoldLabel;

    juce::Slider dco2MorphSlider;
    juce::Label dco2MorphLabel;
    juce::Slider dco2PWMSlider;
    juce::Label dco2PWMLabel;
    juce::Slider semiTuneSlider;
    juce::Label semiTuneLabel;
    juce::Slider fineTuneSlider;
    juce::Label fineTuneLabel;
    juce::Slider fmAmountSlider;
    juce::Label fmAmountLabel;
    juce::Slider dco2FoldSlider;
    juce::Label dco2FoldLabel;

    juce::Slider dco2LevelSlider;
    juce::Label dco2LevelLabel;
    juce::Slider subOscLevelSlider;
    juce::Label subOscLevelLabel;
    juce::TextButton subOscOctaveButton;
    juce::TextButton subOscWaveButton;
    juce::Slider noiseLevelSlider;
    juce::Label noiseLevelLabel;
    juce::TextButton supersawModeButton;
    juce::Slider glideTimeSlider;
    juce::Label glideTimeLabel;
    juce::TextButton glideEnableButton;
    juce::TextButton legatoModeButton;

    // Dual Filter Section Controls
    juce::TextButton filterRoutingButton;
    juce::Slider filterBalanceSlider;
    juce::Label filterBalanceLabel;

    juce::Slider f1CutoffSlider;
    juce::Label f1CutoffLabel;
    juce::Slider f1ResonanceSlider;
    juce::Label f1ResonanceLabel;
    juce::Slider f1HPFSlider;
    juce::Label f1HPFLabel;
    juce::Slider f1EnvDepthSlider;
    juce::Label f1EnvDepthLabel;
    juce::Slider f1KeyTrackSlider;
    juce::Label f1KeyTrackLabel;
    juce::Slider f1LfoDepthSlider;
    juce::Label f1LfoDepthLabel;

    juce::Slider f2CutoffSlider;
    juce::Label f2CutoffLabel;
    juce::Slider f2ResonanceSlider;
    juce::Label f2ResonanceLabel;
    juce::ComboBox f2ModeComboBox;
    juce::Label f2ModeLabel;
    juce::Slider f2EnvDepthSlider;
    juce::Label f2EnvDepthLabel;
    juce::Slider f2KeyTrackSlider;
    juce::Label f2KeyTrackLabel;

    // Real-Time Oscilloscope Screen
    OscilloscopeComponent oscilloscope;

    // =========================================================================
    // Dedicated Dynamic Vector ADSR CRT/LCD Screen & Unified Knob Deck
    // =========================================================================
    EnvelopeVisualizerLcdComponent envLcd;

    int activeEnvIndex = 0; // 0 = VCF (Filter), 1 = VCA (Amp), 2 = AUX / MOD

    juce::TextButton envPrevButton;
    juce::TextButton envVcfTabButton;
    juce::TextButton envVcaTabButton;
    juce::TextButton envAuxTabButton;
    juce::TextButton envNextButton;

    juce::Slider envAttackSlider;
    juce::Label  envAttackLabel;
    juce::Slider envDecaySlider;
    juce::Label  envDecayLabel;
    juce::Slider envSustainSlider;
    juce::Label  envSustainLabel;
    juce::Slider envReleaseSlider;
    juce::Label  envReleaseLabel;

    std::unique_ptr<SliderAttachment> envAttackAttachment;
    std::unique_ptr<SliderAttachment> envDecayAttachment;
    std::unique_ptr<SliderAttachment> envSustainAttachment;
    std::unique_ptr<SliderAttachment> envReleaseAttachment;

    // Dedicated Aux Modulation Destination Deck
    juce::Label  auxModSectionLabel;
    juce::Slider auxToPitchSlider, auxToFilter1Slider, auxToFilter2Slider, auxToWavefoldSlider, auxToPWMSlider;
    juce::Label  auxToPitchLabel, auxToFilter1Label, auxToFilter2Label, auxToWavefoldLabel, auxToPWMLabel;

    // LFO Section
    juce::ComboBox lfoSelectComboBox;
    juce::Label lfoSelectLabel;
    juce::ComboBox lfoShapeComboBox;
    juce::Label lfoShapeLabel;
    juce::Slider lfoRateSlider;
    juce::Label lfoRateLabel;
    juce::Slider lfoDepthSlider;
    juce::Label lfoDepthLabel;

    // Modulation Matrix (16 Slots per Part)
    juce::ComboBox modSlotSelectComboBox;
    juce::Label modSlotSelectLabel;
    juce::ComboBox modSourceComboBox;
    juce::Label modSourceLabel;
    juce::ComboBox modDestComboBox;
    juce::Label modDestLabel;
    juce::Slider modAmountSlider;
    juce::Label modAmountLabel;
    juce::ComboBox modViaSourceComboBox;
    juce::Label modViaSourceLabel;
    juce::Slider modViaAmountSlider;
    juce::Label modViaAmountLabel;
    juce::TextButton modSlotEnableButton;

    // =========================================================================
    // Redesigned Vector Sequencer Section (Dynamic LCD + Unified Deck)
    // =========================================================================
    VectorSequencerLcdComponent seqLcd;

    juce::TextButton seqTrackLayerButton;
    juce::TextButton seqTrackMuteButton;
    juce::TextButton seqTrackSoloButton;

    juce::TextButton seqPage1Button;
    juce::TextButton seqPage2Button;
    juce::TextButton seqPage3Button;
    juce::TextButton seqPage4Button;

    juce::TextButton seqPlayButton;
    juce::TextButton seqRecButton;
    juce::TextButton arpEnableButton;

    // QWERTY Step-Focus & Step-Edit Strip
    juce::TextButton prevStepButton;
    juce::Label      currentStepLabel;
    juce::TextButton nextStepButton;
    juce::TextButton auditionStepButton;
    juce::TextButton clearLocksButton;
    juce::Slider     stepGateSlider;
    juce::Label      stepGateLabel;

    // Unified Parameter Deck (Tempo, Sync, Swing, Gate, Rate, Direction, Chords)
    juce::Slider     seqTempoSlider;
    juce::Label      seqTempoLabel;
    juce::TextButton seqSyncButton;

    juce::Slider     seqSwingSlider;
    juce::Label      seqSwingLabel;
    juce::Slider     seqGateLengthSlider;
    juce::Label      seqGateLengthLabel;

    juce::ComboBox   seqDivisionComboBox;
    juce::Label      seqDivisionLabel;
    juce::ComboBox   seqDirectionComboBox;
    juce::Label      seqDirectionLabel;
    juce::ComboBox   seqLengthComboBox;
    juce::Label      seqLengthLabel;
    juce::ComboBox   seqModeComboBox;
    juce::Label      seqModeLabel;

    juce::TextButton chordEnableButton;
    juce::ComboBox   chordTypeComboBox;
    juce::Label      chordTypeLabel;

    // Master FX Section & Bi-Timbral FX Routing
    juce::TextButton chorus1Button;
    juce::TextButton chorus2Button;

    juce::TextButton delaySyncButton;
    juce::TextButton delayPingPongButton;
    juce::Slider delayTimeSlider;
    juce::Label delayTimeLabel;
    juce::Slider delayFeedbackSlider;
    juce::Label delayFeedbackLabel;
    juce::Slider delayDampSlider;
    juce::Label delayDampLabel;
    juce::Slider delayMixSlider;
    juce::Label delayMixLabel;

    juce::Slider reverbSizeSlider;
    juce::Label reverbSizeLabel;
    juce::Slider reverbDampSlider;
    juce::Label reverbDampLabel;
    juce::Slider reverbMixSlider;
    juce::Label reverbMixLabel;

    juce::ComboBox fxRoutingComboBox;
    juce::Label    fxRoutingLabel;

    // Bottom Performance Deck
    juce::Slider pitchWheelSlider;
    juce::Label pitchWheelLabel;
    juce::Slider modWheelSlider;
    juce::Label modWheelLabel;
    juce::MidiKeyboardComponent keyboardComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Simple116AudioProcessorEditor)
};

using Simple106AudioProcessorEditor = Simple116AudioProcessorEditor;
using S116AudioProcessorEditor = Simple116AudioProcessorEditor;