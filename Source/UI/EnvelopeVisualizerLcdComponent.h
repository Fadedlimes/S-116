#pragma once
// ============================================================================
//  EnvelopeVisualizerLcdComponent.h
//  Live ADSR curve visualizer. Re-skin only: colours/fonts from S116 theme.
//  No timer, no input handling — driven by setEnvelope() calls from the editor.
// ============================================================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UI/SkinManager.h"

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

        // Inner glass retainer outline
        g.setColour(juce::Colour(0x50000000));
        g.drawRoundedRectangle(glassRect, 2.5f, 1.0f);
        g.setColour(ledCol.withAlpha(0.20f));
        g.drawRoundedRectangle(glassRect.reduced(0.5f), 2.0f, 0.8f);

        // 3. LCD header readout
        const auto pixelCol = ledCol.brighter(0.65f);
        const auto ghostCol = ledCol.withAlpha(0.08f);

        juce::Font headerFont (S116::skin().mono (10.0f));
        headerFont.setBold (true);
        g.setFont (headerFont);

        const auto titleRect = juce::Rectangle<float>(glassRect.getX() + 8.0f, glassRect.getY() + 3.0f, 140.0f, 12.0f);
        const auto readoutsRect = juce::Rectangle<float>(glassRect.getRight() - 170.0f, glassRect.getY() + 3.0f, 162.0f, 12.0f);

        const juce::String readoutsText = "A:" + attackText + " D:" + decayText + " S:" + sustainText + " R:" + releaseText;

        g.setColour(ghostCol);
        g.drawText(envName, titleRect.translated(1.0f, 1.0f), juce::Justification::centredLeft, false);
        g.drawText(readoutsText, readoutsRect.translated(1.0f, 1.0f), juce::Justification::centredRight, false);

        g.setColour(pixelCol);
        g.drawText(envName, titleRect, juce::Justification::centredLeft, false);
        g.drawText(readoutsText, readoutsRect, juce::Justification::centredRight, false);

        const float graphX = glassRect.getX() + 10.0f;
        const float graphY = glassRect.getY() + 18.0f;
        const float graphW = glassRect.getWidth() - 20.0f;
        const float graphH = glassRect.getHeight() - 24.0f;

        // Grid lines (3 vertical, 3 horizontal)
        g.setColour (juce::Colour (0xffff3a4c).withAlpha (0.15f));
        for (int i = 1; i < 4; ++i)
        {
            float gx = graphX + graphW * (float) i / 4.0f;
            float gy = graphY + graphH * (float) i / 4.0f;
            g.drawVerticalLine ((int) gx, graphY, graphY + graphH);
            g.drawHorizontalLine ((int) gy, graphX, graphX + graphW);
        }

        // 4. Real-time vector ADSR curve
        const float yBase = graphY + graphH;
        const float yPeak = graphY + 3.0f;
        const float ySus = yBase - (sustainNorm * (yBase - yPeak));

        const float wA = std::max(6.0f, (0.08f + 0.28f * attackNorm) * graphW);
        const float wD = std::max(6.0f, (0.08f + 0.28f * decayNorm) * graphW);
        const float wR = std::max(6.0f, (0.08f + 0.28f * releaseNorm) * graphW);

        float wHold = graphW - (wA + wD + wR);
        if (wHold < 14.0f) wHold = 14.0f;

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

        juce::ColourGradient fillGrad(juce::Colour (0xffff3c50).withAlpha(0.28f), 0.0f, yPeak,
                                     juce::Colour (0xffff3c50).withAlpha(0.02f), 0.0f, yBase, false);
        g.setGradientFill(fillGrad);
        g.fillPath(fillPath);

        g.setColour(ledCol.withAlpha(0.15f));
        g.drawHorizontalLine(juce::roundToInt(yBase), graphX, graphX + graphW);

        g.setColour(juce::Colour (0xffff6b76).withAlpha(0.30f));
        g.strokePath(curvePath, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour (0xffff6b76));
        g.strokePath(curvePath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

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
    float attackNorm = 0.10f, decayNorm = 0.30f, sustainNorm = 0.70f, releaseNorm = 0.35f;
    juce::String attackText = "10ms", decayText = "200ms", sustainText = "70%", releaseText = "350ms";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeVisualizerLcdComponent)
};
