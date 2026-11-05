#pragma once
// ============================================================================
//  VectorSequencerLcdComponent.h
//  Live 16-step sequencer visualizer + QWERTY step editor.
//  Functional component: has keyboard focus, key handling, mouse handling,
//  and is driven by updateDisplayState() callbacks from the editor.
//  Re-skin only: colours/fonts from S116 theme.
// ============================================================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "UI/SkinManager.h"
#include "DSP/SequencerEngine.h"

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

    void setStepData(int stepIndex, bool active, float velocity, float gate,
                     const juce::String& note, bool hasLock, int ratchet) noexcept
    {
        const size_t idx = static_cast<size_t>(stepIndex % 16);
        stepActiveCache[idx]   = active;
        stepVelocityCache[idx] = velocity;
        stepGateCache[idx]     = gate;
        stepNoteCache[idx]     = note;
        stepLockCache[idx]     = hasLock;
        stepRatchetCache[idx]  = ratchet;
    }

    void setGlobalGate(float g) noexcept { globalGate = g; }

    std::function<void(int stepIndex)> onStepClicked;
    std::function<void(int stepIndex, float newVelocity)> onStepVelocityChanged;
    std::function<void(int stepIndex, float newGate)> onStepGateChanged;
    std::function<void(int stepIndex)> onStepToggleTrigger;
    std::function<void(int stepIndex, int semitones)> onStepTranspose;
    std::function<void(int stepOffset)> onStepNavigate;
    std::function<void(int stepIndex)> onStepAudition;

    void focusGained (juce::Component::FocusChangeType) override { isQwertyFocused = true; repaint(); }
    void focusLost (juce::Component::FocusChangeType) override   { isQwertyFocused = false; repaint(); }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.isKeyCode(juce::KeyPress::leftKey))
        {
            if (onStepNavigate) onStepNavigate(-1);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::rightKey))
        {
            if (onStepNavigate) onStepNavigate(1);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::spaceKey))
        {
            if (onStepToggleTrigger) onStepToggleTrigger(selectedStep);
            if (onStepAudition) onStepAudition(selectedStep);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::upKey))
        {
            const int delta = key.getModifiers().isShiftDown() ? 12 : 1;
            if (onStepTranspose) onStepTranspose(selectedStep, delta);
            if (onStepAudition) onStepAudition(selectedStep);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::downKey))
        {
            const int delta = key.getModifiers().isShiftDown() ? -12 : -1;
            if (onStepTranspose) onStepTranspose(selectedStep, delta);
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

    void mouseDown(const juce::MouseEvent&) override
    {
        grabKeyboardFocus();
    }

    void paint(juce::Graphics& g) override
    {
        auto& skin = S116::skin();
        const auto& t = skin.theme();
        const auto ledCol = t.ledReadout;
        const auto bounds = getLocalBounds().toFloat();

        // 1. Recessed bezel socket
        g.setColour(juce::Colour(0xff060709));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(t.knobRing.darker(0.4f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

        // 2. Backlit phosphor glass window
        const auto glassRect = bounds.reduced(2.5f);
        const auto bgBacklight = ledCol.withMultipliedSaturation(0.70f).withMultipliedBrightness(0.08f);
        g.setColour(bgBacklight);
        g.fillRoundedRectangle(glassRect, 2.5f);

        // CRT scanline grid texture
        g.setColour(juce::Colour(0x0a000000));
        for (float y = glassRect.getY(); y < glassRect.getBottom(); y += 3.0f)
            g.drawHorizontalLine(juce::roundToInt(y), glassRect.getX(), glassRect.getRight());

        // Inner glass shadow & retainer
        g.setColour(juce::Colour(0x50000000));
        g.drawRoundedRectangle(glassRect, 2.5f, 1.0f);
        g.setColour(ledCol.withAlpha(0.20f));
        g.drawRoundedRectangle(glassRect.reduced(0.5f), 2.0f, 0.8f);

        // 3. Status HUD header line
        const auto pixelCol = ledCol.brighter(0.65f);
        const auto ghostCol = ledCol.withAlpha(0.08f);

        juce::Font hudFont (S116::skin().mono (9.5f));
        hudFont.setBold (true);
        g.setFont (hudFont);

        const float hudY = glassRect.getY() + 3.0f;
        const juce::String trkStr = (activeTrack == TrackID::TrackA) ? "TRK A [CH 1]" : "TRK B [CH 2]";
        const juce::String bpmStr = "BPM:" + juce::String(bpm, 1) + (syncToHost ? " [HOST]" : " [INT]");
        const juce::String qwertStr = isQwertyFocused ? "QWERTY: [<-> NAV | SPC: TOGGLE | UP/DN: NOTE]"
                                                      : "CLICK TO FOCUS QWERTY";

        g.setColour(ghostCol);
        g.drawText(trkStr, glassRect.getX() + 6.0f + 1.0f, hudY + 1.0f, 110.0f, 12.0f, juce::Justification::centredLeft, false);
        g.drawText(bpmStr, glassRect.getX() + 120.0f + 1.0f, hudY + 1.0f, 120.0f, 12.0f, juce::Justification::centredLeft, false);
        g.drawText(qwertStr, glassRect.getRight() - 200.0f + 1.0f, hudY + 1.0f, 194.0f, 12.0f, juce::Justification::centredRight, false);

        g.setColour(pixelCol);
        g.drawText(trkStr, glassRect.getX() + 6.0f, hudY, 110.0f, 12.0f, juce::Justification::centredLeft, false);
        g.drawText(bpmStr, glassRect.getX() + 120.0f, hudY, 120.0f, 12.0f, juce::Justification::centredLeft, false);
        g.setColour(isQwertyFocused ? pixelCol : ledCol.withAlpha(0.40f));
        g.drawText(qwertStr, glassRect.getRight() - 200.0f, hudY, 194.0f, 12.0f, juce::Justification::centredRight, false);

        // 4. 16-step grid drawing
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

            g.setColour(isStepSelected ? ledCol.withAlpha(0.18f) : juce::Colour(0x12000000));
            g.fillRoundedRectangle(colRect, 1.5f);

            juce::Font stepFont (S116::skin().mono (8.5f));
            stepFont.setBold (true);
            g.setFont (stepFont);
            g.setColour(isStepSelected ? pixelCol : ledCol.withAlpha(0.50f));
            const juce::String numStr = juce::String(stepIdx + 1).paddedLeft('0', 2);
            g.drawText(numStr, colRect.getX(), colRect.getY() + 1.0f, colRect.getWidth(), 10.0f, juce::Justification::centred, false);

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

            juce::Font noteFont (S116::skin().mono (8.0f));
            g.setFont (noteFont);
            g.setColour(isStepActive ? pixelCol : ledCol.withAlpha(0.25f));
            g.drawText(isStepActive ? noteStr : "--", colRect.getX(), colRect.getY() + 18.0f, colRect.getWidth(), 10.0f, juce::Justification::centred, false);

            const float velAreaY = colRect.getY() + 30.0f;
            const float velAreaH = 34.0f;
            const float barW = 8.0f;
            const float barX = colRect.getCentreX() - (barW * 0.5f);

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

            const float gateY = velAreaY + velAreaH + 2.0f;
            const float gateH = 4.0f;
            g.setColour(ledCol.withAlpha(0.10f));
            g.fillRect(colRect.getX() + 2.0f, gateY, colRect.getWidth() - 4.0f, gateH);

            if (isStepActive)
            {
                const float effectiveGate = (stepGate > 0.0f) ? stepGate : globalGate;
                const float gateNorm = std::clamp(effectiveGate / 2.0f, 0.0f, 1.0f);
                const float gateBarW = (colRect.getWidth() - 4.0f) * gateNorm;
                g.setColour(stepGate > 0.0f ? ledCol.brighter(0.4f) : ledCol.withAlpha(0.50f));
                g.fillRect(colRect.getX() + 2.0f, gateY, gateBarW, gateH);
            }

            const float badgeY = gateY + gateH + 2.0f;
            if (hasLock)
            {
                g.setColour(juce::Colour(0xffffcc00));
                const float diamondSize = 5.0f;
                const float diamondCx = colRect.getCentreX();
                const float diamondCy = badgeY + 4.0f;
                juce::Path diamond;
                diamond.startNewSubPath(diamondCx, diamondCy - diamondSize * 0.5f);
                diamond.lineTo(diamondCx + diamondSize * 0.5f, diamondCy);
                diamond.lineTo(diamondCx, diamondCy + diamondSize * 0.5f);
                diamond.lineTo(diamondCx - diamondSize * 0.5f, diamondCy);
                diamond.closeSubPath();
                g.fillPath(diamond);
            }
            else if (ratchet > 1)
            {
                g.setColour (pixelCol);
                juce::Font ratchetFont (7.0f);
                ratchetFont.setBold (true);
                g.setFont (ratchetFont);
                g.drawText (juce::String (ratchet) + "x", colRect.getX(), badgeY, colRect.getWidth(), 8.0f, juce::Justification::centred, false);
            }
        }
    }

private:
    int playheadStep = 0, selectedStep = 0, activePage = 0;
    TrackID activeTrack = TrackID::TrackA;
    bool isPlaying = false;
    double bpm = 120.0;
    bool syncToHost = true;
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
