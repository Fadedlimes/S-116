#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_core/juce_core.h>
#endif

#include "SkinManager.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <functional>

class OscilloscopeQueue
{
public:
    static constexpr int FIFO_SIZE = 8192;

    OscilloscopeQueue() noexcept
        : fifo(FIFO_SIZE)
    {
        buffer.fill(0.0f);
    }

    ~OscilloscopeQueue() = default;

    void reset() noexcept
    {
        fifo.reset();
        buffer.fill(0.0f);
    }

    void pushSamples(const float* inputData, int numSamples) noexcept
    {
        if (inputData == nullptr || numSamples <= 0)
            return;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

        if (size1 > 0)
        {
            std::copy(inputData, inputData + size1, buffer.data() + start1);
        }
        if (size2 > 0)
        {
            std::copy(inputData + size1, inputData + size1 + size2, buffer.data() + start2);
        }

        fifo.finishedWrite(size1 + size2);
    }

    void pushBuffer(const juce::AudioBuffer<float>& inputBuffer, int startSample, int numSamples) noexcept
    {
        const int totalBufferSamples = inputBuffer.getNumSamples();
        if (numSamples <= 0 || startSample >= totalBufferSamples)
            return;

        const int available = std::min(numSamples, totalBufferSamples - startSample);
        const int numChannels = inputBuffer.getNumChannels();

        if (numChannels == 1)
        {
            pushSamples(inputBuffer.getReadPointer(0, startSample), available);
        }
        else if (numChannels >= 2)
        {
            static constexpr int TEMP_BLOCK_SIZE = 512;
            std::array<float, TEMP_BLOCK_SIZE> tempMono {};

            int samplesRemaining = available;
            int currentOffset = startSample;

            while (samplesRemaining > 0)
            {
                const int chunkSize = std::min(samplesRemaining, TEMP_BLOCK_SIZE);
                const float* l = inputBuffer.getReadPointer(0, currentOffset);
                const float* r = inputBuffer.getReadPointer(1, currentOffset);

                for (int i = 0; i < chunkSize; ++i)
                {
                    tempMono[static_cast<size_t>(i)] = 0.5f * (l[i] + r[i]);
                }

                pushSamples(tempMono.data(), chunkSize);

                currentOffset += chunkSize;
                samplesRemaining -= chunkSize;
            }
        }
    }

    int readSamples(float* dest, int maxSamples) noexcept
    {
        if (dest == nullptr || maxSamples <= 0)
            return 0;

        const int numReady = fifo.getNumReady();
        const int numToRead = std::min(maxSamples, numReady);

        if (numToRead <= 0)
            return 0;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(numToRead, start1, size1, start2, size2);

        if (size1 > 0)
        {
            std::copy(buffer.data() + start1, buffer.data() + start1 + size1, dest);
        }
        if (size2 > 0)
        {
            std::copy(buffer.data() + start2, buffer.data() + start2 + size2, dest + size1);
        }

        fifo.finishedRead(size1 + size2);
        return size1 + size2;
    }

    int getNumReady() const noexcept
    {
        return fifo.getNumReady();
    }

private:
    juce::AbstractFifo fifo;
    std::array<float, FIFO_SIZE> buffer {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeQueue)
};

class OscilloscopeComponent : public juce::Component,
                              private juce::Timer
{
public:
    static constexpr int ACCUMULATION_SIZE = 4096;
    static constexpr int RENDER_WINDOW_SIZE = 1024;

    enum class Theme
    {
        Cyan = 0,
        PhosphorGreen,
        Amber,
        RubyRed,
        UltraViolet,
        IceWhite,
        Count
    };

    OscilloscopeComponent() noexcept
    {
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);

        accumulationBuffer.fill(0.0f);
        displayBuffer.fill(0.0f);

        activeQueue = &internalQueue;

        currentTheme = static_cast<Theme>(SkinManager::getInstance().getCRTTheme());

        startTimerHz(60);
    }

    ~OscilloscopeComponent() override
    {
        stopTimer();
    }

    void setQueue(OscilloscopeQueue* queueToUse) noexcept
    {
        activeQueue = (queueToUse != nullptr) ? queueToUse : &internalQueue;
    }

    void pushBuffer(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        internalQueue.pushBuffer(buffer, startSample, numSamples);
    }

    void setTheme(Theme newTheme) noexcept
    {
        currentTheme = newTheme;
        SkinManager::getInstance().setCRTTheme(static_cast<CRTTheme>(newTheme));
        if (onThemeChanged)
            onThemeChanged(currentTheme);
        repaint();
    }

    Theme getTheme() const noexcept { return currentTheme; }

    void cycleTheme() noexcept
    {
        int next = (static_cast<int>(currentTheme) + 1) % static_cast<int>(Theme::Count);
        setTheme(static_cast<Theme>(next));
    }

    juce::Colour getThemeColor() const noexcept
    {
        switch (currentTheme)
        {
            case Theme::Cyan:          return juce::Colour(0xff00f0ff);
            case Theme::PhosphorGreen: return juce::Colour(0xff39ff14);
            case Theme::Amber:         return juce::Colour(0xffffa200);
            case Theme::RubyRed:       return juce::Colour(0xffff2d3b);
            case Theme::UltraViolet:   return juce::Colour(0xffc800ff);
            case Theme::IceWhite:      return juce::Colour(0xffffffff);
            default:                   return juce::Colour(0xff00f0ff);
        }
    }

    juce::String getThemeName() const noexcept
    {
        switch (currentTheme)
        {
            case Theme::Cyan:          return "CYAN";
            case Theme::PhosphorGreen: return "PHOSPHOR";
            case Theme::Amber:         return "AMBER";
            case Theme::RubyRed:       return "RED";
            case Theme::UltraViolet:   return "VIOLET";
            case Theme::IceWhite:      return "WHITE";
            default:                   return "SCOPE";
        }
    }

    void setTimebaseScale(float scale) noexcept
    {
        timebaseScale = std::clamp(scale, 0.25f, 4.0f);
        repaint();
    }

    float getTimebaseScale() const noexcept { return timebaseScale; }

    void setVerticalGain(float gain) noexcept
    {
        verticalGain = std::clamp(gain, 0.2f, 5.0f);
        repaint();
    }

    float getVerticalGain() const noexcept { return verticalGain; }

    std::function<void(Theme)> onThemeChanged;

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        if (w <= 10.0f || h <= 10.0f)
            return;

        drawChassisAndBezel(g, bounds);

        const auto screenBounds = bounds.reduced(6.0f);
        g.saveState();
        g.reduceClipRegion(screenBounds.toNearestInt());

        drawScreenBackground(g, screenBounds);
        drawReticleGrid(g, screenBounds);
        drawWaveformTrace(g, screenBounds);
        drawVignetteAndGloss(g, screenBounds);
        drawReadoutHUD(g, screenBounds);

        g.restoreState();
    }

    void resized() override
    {
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            timebaseScale = 1.0f;
            verticalGain = 1.0f;
            repaint();
        }
        else
        {
            cycleTheme();
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (std::abs(wheel.deltaY) > 0.001f)
        {
            const float factor = (wheel.deltaY > 0.0f) ? 1.15f : 0.87f;
            setTimebaseScale(timebaseScale * factor);
        }
    }

private:
    void timerCallback() override
    {
        if (activeQueue == nullptr)
            return;

        static constexpr int CHUNK_SIZE = 512;
        std::array<float, CHUNK_SIZE> incomingChunk {};

        int totalRead = 0;
        while (activeQueue->getNumReady() > 0 && totalRead < ACCUMULATION_SIZE)
        {
            const int read = activeQueue->readSamples(incomingChunk.data(), CHUNK_SIZE);
            if (read <= 0)
                break;

            const int shiftAmount = read;
            if (shiftAmount < ACCUMULATION_SIZE)
            {
                std::copy(accumulationBuffer.data() + shiftAmount,
                          accumulationBuffer.data() + ACCUMULATION_SIZE,
                          accumulationBuffer.data());

                std::copy(incomingChunk.data(),
                          incomingChunk.data() + read,
                          accumulationBuffer.data() + (ACCUMULATION_SIZE - shiftAmount));
            }
            else
            {
                std::copy(incomingChunk.data() + (read - ACCUMULATION_SIZE),
                          incomingChunk.data() + read,
                          accumulationBuffer.data());
            }

            totalRead += read;
        }

        if (totalRead > 0)
        {
            extractTriggeredWindow();
            repaint();
        }
    }

    void extractTriggeredWindow() noexcept
    {
        const int searchRange = ACCUMULATION_SIZE - (RENDER_WINDOW_SIZE + 8);
        if (searchRange <= 0)
            return;

        float maxAmplitude = 0.0f;
        const int testStart = std::max(0, searchRange - 1000);
        for (int i = testStart; i < searchRange; ++i)
        {
            const float a = std::abs(accumulationBuffer[static_cast<size_t>(i)]);
            if (a > maxAmplitude)
                maxAmplitude = a;
        }

        if (maxAmplitude < 0.005f)
        {
            subSampleOffset = 0.0f;
            const int fallbackStart = searchRange / 2;
            for (int i = 0; i < RENDER_WINDOW_SIZE + 8; ++i)
                displayBuffer[static_cast<size_t>(i)] = accumulationBuffer[static_cast<size_t>(fallbackStart + i)];
            return;
        }

        const float hysteresis = std::clamp(maxAmplitude * 0.15f, 0.015f, 0.12f) / verticalGain;

        int triggerIndex = -1;
        float frac = 0.0f;
        bool armed = false;

        const int startSearch = std::max(1, searchRange - 2000);

        for (int i = startSearch; i < searchRange; ++i)
        {
            const float sample = accumulationBuffer[static_cast<size_t>(i)];

            if (!armed)
            {
                if (sample < -hysteresis)
                    armed = true;
            }
            else
            {
                const float prevSample = accumulationBuffer[static_cast<size_t>(i - 1)];
                if (prevSample <= 0.0f && sample > 0.0f)
                {
                    const float diff = sample - prevSample;
                    if (diff > 1e-5f)
                    {
                        frac = -prevSample / diff;
                        frac = std::clamp(frac, 0.0f, 1.0f);
                    }
                    triggerIndex = i - 1;
                    break;
                }
            }
        }

        if (triggerIndex < 0)
        {
            triggerIndex = searchRange / 2;
            frac = 0.0f;
        }

        subSampleOffset = frac;

        for (int i = 0; i < RENDER_WINDOW_SIZE + 8; ++i)
        {
            const size_t srcIdx = static_cast<size_t>(triggerIndex + i);
            displayBuffer[static_cast<size_t>(i)] = accumulationBuffer[srcIdx];
        }
    }

    void drawChassisAndBezel(juce::Graphics& g, const juce::Rectangle<float>& bounds) noexcept
    {
        juce::ColourGradient chassisGrad(
            juce::Colour(0xff181a1f), bounds.getX(), bounds.getY(),
            juce::Colour(0xff0a0b0d), bounds.getX(), bounds.getBottom(), false);

        g.setGradientFill(chassisGrad);
        g.fillRoundedRectangle(bounds, 5.0f);

        g.setColour(juce::Colour(0xff2d3139));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

        g.setColour(juce::Colour(0xff050507));
        g.drawRoundedRectangle(bounds.reduced(5.0f), 3.0f, 1.5f);
    }

    void drawScreenBackground(juce::Graphics& g, const juce::Rectangle<float>& screenBounds) noexcept
    {
        const auto themeCol = getThemeColor();

        juce::ColourGradient bgGrad(
            themeCol.withMultipliedBrightness(0.08f), screenBounds.getCentreX(), screenBounds.getCentreY(),
            juce::Colour(0xff030507), screenBounds.getX(), screenBounds.getBottom(), true);

        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(screenBounds, 3.0f);
    }

    void drawReticleGrid(juce::Graphics& g, const juce::Rectangle<float>& screenBounds) noexcept
    {
        const auto themeCol = getThemeColor();
        const auto gridCol = themeCol.withAlpha(0.12f);
        const auto subGridCol = themeCol.withAlpha(0.05f);

        const float left   = screenBounds.getX();
        const float top    = screenBounds.getY();
        const float right  = screenBounds.getRight();
        const float bottom = screenBounds.getBottom();
        const float w      = screenBounds.getWidth();
        const float h      = screenBounds.getHeight();
        const float midX   = left + w * 0.5f;
        const float midY   = top + h * 0.5f;

        static constexpr int NUM_DIVS_X = 8;
        static constexpr int NUM_DIVS_Y = 6;

        g.setColour(subGridCol);
        for (int i = 1; i < NUM_DIVS_X * 2; ++i)
        {
            const float x = left + (w * static_cast<float>(i)) / static_cast<float>(NUM_DIVS_X * 2);
            g.drawVerticalLine(juce::roundToInt(x), top, bottom);
        }
        for (int j = 1; j < NUM_DIVS_Y * 2; ++j)
        {
            const float y = top + (h * static_cast<float>(j)) / static_cast<float>(NUM_DIVS_Y * 2);
            g.drawHorizontalLine(juce::roundToInt(y), left, right);
        }

        g.setColour(gridCol);
        for (int i = 1; i < NUM_DIVS_X; ++i)
        {
            const float x = left + (w * static_cast<float>(i)) / static_cast<float>(NUM_DIVS_X);
            g.drawVerticalLine(juce::roundToInt(x), top, bottom);
        }
        for (int j = 1; j < NUM_DIVS_Y; ++j)
        {
            const float y = top + (h * static_cast<float>(j)) / static_cast<float>(NUM_DIVS_Y);
            g.drawHorizontalLine(juce::roundToInt(y), left, right);
        }

        g.setColour(themeCol.withAlpha(0.24f));
        g.drawVerticalLine(juce::roundToInt(midX), top, bottom);
        g.drawHorizontalLine(juce::roundToInt(midY), left, right);

        for (int i = -NUM_DIVS_X * 2; i <= NUM_DIVS_X * 2; ++i)
        {
            const float x = midX + (i * w) / (NUM_DIVS_X * 4);
            g.drawVerticalLine(juce::roundToInt(x), midY - 2.5f, midY + 2.5f);
        }
        for (int j = -NUM_DIVS_Y * 2; j <= NUM_DIVS_Y * 2; ++j)
        {
            const float y = midY + (j * h) / (NUM_DIVS_Y * 4);
            g.drawHorizontalLine(juce::roundToInt(y), midX - 2.5f, midX + 2.5f);
        }
    }

    void drawWaveformTrace(juce::Graphics& g, const juce::Rectangle<float>& screenBounds) noexcept
    {
        const float left = screenBounds.getX();
        const float w    = screenBounds.getWidth();
        const float midY = screenBounds.getCentreY();
        const float ampY = (screenBounds.getHeight() * 0.46f) * verticalGain;

        const auto themeCol = getThemeColor();

        waveformPath.clear();
        waveformPath.preallocateSpace((RENDER_WINDOW_SIZE + 8) * 3);

        const float windowSamples = static_cast<float>(RENDER_WINDOW_SIZE) / timebaseScale;
        const int numPointsToRender = std::min(RENDER_WINDOW_SIZE, static_cast<int>(windowSamples));

        if (numPointsToRender <= 1)
            return;

        const float xStep = w / static_cast<float>(numPointsToRender - 1);
        const int totalPoints = std::min(numPointsToRender + 2, RENDER_WINDOW_SIZE + 6);

        for (int i = 0; i < totalPoints; ++i)
        {
            const float rawSample = displayBuffer[static_cast<size_t>(i)];
            const float clampedSample = std::clamp(rawSample, -1.3f, 1.3f);

            const float x = left + (static_cast<float>(i) - subSampleOffset) * xStep;
            const float y = midY - (clampedSample * ampY);

            if (i == 0)
                waveformPath.startNewSubPath(x, y);
            else
                waveformPath.lineTo(x, y);
        }

        g.setColour(themeCol.withAlpha(0.10f));
        g.strokePath(waveformPath, juce::PathStrokeType(6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(themeCol.withAlpha(0.35f));
        g.strokePath(waveformPath, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto coreCol = juce::Colours::white.interpolatedWith(themeCol, 0.25f).withAlpha(0.95f);
        g.setColour(coreCol);
        g.strokePath(waveformPath, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawVignetteAndGloss(juce::Graphics& g, const juce::Rectangle<float>& screenBounds) noexcept
    {
        juce::ColourGradient vignette(
            juce::Colours::transparentBlack, screenBounds.getCentreX(), screenBounds.getCentreY(),
            juce::Colour(0xa0000000), screenBounds.getX(), screenBounds.getY(), true);

        g.setGradientFill(vignette);
        g.fillRoundedRectangle(screenBounds, 3.0f);

        juce::Path glossPath;
        glossPath.startNewSubPath(screenBounds.getX(), screenBounds.getY());
        glossPath.lineTo(screenBounds.getRight(), screenBounds.getY());
        glossPath.lineTo(screenBounds.getRight(), screenBounds.getY() + screenBounds.getHeight() * 0.40f);
        glossPath.lineTo(screenBounds.getX(), screenBounds.getY() + screenBounds.getHeight() * 0.20f);
        glossPath.closeSubPath();

        juce::ColourGradient glossGrad(
            juce::Colours::white.withAlpha(0.045f), screenBounds.getX(), screenBounds.getY(),
            juce::Colours::transparentWhite, screenBounds.getX(), screenBounds.getY() + screenBounds.getHeight() * 0.40f, false);

        g.setGradientFill(glossGrad);
        g.fillPath(glossPath);
    }

    void drawReadoutHUD(juce::Graphics& g, const juce::Rectangle<float>& screenBounds) noexcept
    {
        const auto themeCol = getThemeColor();

        g.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
        g.setColour(themeCol.withAlpha(0.65f));

        const auto hudLeft = screenBounds.reduced(6.0f, 4.0f);
        g.drawText("S-116 CRT // " + getThemeName(), hudLeft, juce::Justification::topLeft, false);

        juce::String scaleStr = "ZOOM " + juce::String(timebaseScale, 2) + "x";
        g.drawText(scaleStr, hudLeft, juce::Justification::topRight, false);
    }

    OscilloscopeQueue internalQueue;
    OscilloscopeQueue* activeQueue = nullptr;

    alignas(16) std::array<float, ACCUMULATION_SIZE> accumulationBuffer {};
    alignas(16) std::array<float, RENDER_WINDOW_SIZE + 8> displayBuffer {};

    juce::Path waveformPath;

    Theme currentTheme = Theme::Cyan;
    float timebaseScale = 1.0f;
    float verticalGain = 1.0f;
    float subSampleOffset = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeComponent)
};