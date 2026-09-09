#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_core/juce_core.h>
#endif

#include <cmath>
#include <algorithm>
#include <cstdint>

class StateVariableFilter
{
public:
    enum class Mode
    {
        Lowpass = 0,
        Bandpass,
        Highpass,
        Notch,
        Peak,
        Allpass,
        Morph,
        SEMMorph
    };

    StateVariableFilter() noexcept
    {
        reset();
        updateCoefficients();
    }

    ~StateVariableFilter() = default;

    static inline float fastTanh(float x) noexcept
    {
        const float clamped = std::clamp(x, -4.0f, 4.0f);
        const float x2 = clamped * clamped;
        return clamped * (105.0f + 10.0f * x2) / (105.0f + 45.0f * x2 + x2 * x2);
    }

    static inline float fastTan(float x) noexcept
    {
        const float clamped = std::clamp(x, 0.0001f, 1.50f);
        const float x2 = clamped * clamped;
        return clamped * (1.0f - 0.07459882f * x2) / (1.0f - 0.40506313f * x2);
    }

    void prepare(double sampleRate) noexcept
    {
        currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        reset();
        updateCoefficients();
    }

    void reset() noexcept
    {
        s1 = 0.0f;
        s2 = 0.0f;
        lastLP = 0.0f;
        lastBP = 0.0f;
        lastHP = 0.0f;
        lastNotch = 0.0f;
        lastPeak = 0.0f;
        lastAllpass = 0.0f;
    }

    void setCutoff(float cutoffHz) noexcept
    {
        const float clamped = std::clamp(cutoffHz, 15.0f, static_cast<float>(currentSampleRate * 0.485));
        if (std::abs(clamped - cutoff) > 0.001f)
        {
            cutoff = clamped;
            updateCoefficients();
        }
    }

    void setResonance(float res) noexcept
    {
        const float clamped = std::clamp(res, 0.0f, 1.0f);
        if (std::abs(clamped - resonance) > 0.0005f)
        {
            resonance = clamped;
            updateCoefficients();
        }
    }

    void setDrive(float driveAmount) noexcept
    {
        drive = std::clamp(driveAmount, 1.0f, 5.0f);
    }

    void setMode(Mode newMode) noexcept
    {
        mode = newMode;
    }

    void setMode(int modeIndex) noexcept
    {
        switch (modeIndex)
        {
            case 0: mode = Mode::Lowpass;  break;
            case 1: mode = Mode::Bandpass; break;
            case 2: mode = Mode::Highpass; break;
            case 3: mode = Mode::Notch;    break;
            case 4: mode = Mode::Peak;     break;
            case 5: mode = Mode::Allpass;  break;
            case 6: mode = Mode::Morph;    break;
            case 7: mode = Mode::SEMMorph; break;
            default: mode = Mode::Lowpass; break;
        }
    }

    void setMorph(float morphVal) noexcept
    {
        morph = std::clamp(morphVal, 0.0f, 3.0f);
    }

    void setSEMMorph(float knobVal) noexcept
    {
        semMorphKnob = std::clamp(knobVal, 0.0f, 1.0f);
    }

    float getCutoff() const noexcept    { return cutoff; }
    float getResonance() const noexcept { return resonance; }
    float getDrive() const noexcept     { return drive; }
    Mode getMode() const noexcept       { return mode; }
    float getMorph() const noexcept     { return morph; }
    float getSEMMorph() const noexcept  { return semMorphKnob; }

    float getLP() const noexcept      { return lastLP; }
    float getBP() const noexcept      { return lastBP; }
    float getHP() const noexcept      { return lastHP; }
    float getNotch() const noexcept   { return lastNotch; }
    float getPeak() const noexcept    { return lastPeak; }
    float getAllpass() const noexcept { return lastAllpass; }

    inline float process(float input) noexcept
    {
        const float drivenInput = (drive > 1.001f) ? fastTanh(input * drive) : input;

        const float hp = (drivenInput - (2.0f * RCoeff + gCoeff) * s1 - s2) * invDenom;
        const float bp = gCoeff * hp + s1;
        const float satBP = (resonance > 0.65f) ? fastTanh(bp) : bp;
        s1 = gCoeff * hp + satBP;

        const float lp = gCoeff * satBP + s2;
        s2 = gCoeff * satBP + lp;

        const float notch   = hp + lp;
        const float peak    = lp - hp;
        const float allpass = hp - (4.0f * RCoeff) * bp + lp;

        lastLP      = lp;
        lastBP      = bp;
        lastHP      = hp;
        lastNotch   = notch;
        lastPeak    = peak;
        lastAllpass = allpass;

        switch (mode)
        {
            case Mode::Lowpass:  return lp;
            case Mode::Bandpass: return bp;
            case Mode::Highpass: return hp;
            case Mode::Notch:    return notch;
            case Mode::Peak:     return peak;
            case Mode::Allpass:  return allpass;

            case Mode::Morph:
            {
                if (morph <= 1.0f)
                {
                    const float t = morph;
                    return (1.0f - t) * lp + t * bp;
                }
                else if (morph <= 2.0f)
                {
                    const float t = morph - 1.0f;
                    return (1.0f - t) * bp + t * hp;
                }
                else
                {
                    const float t = morph - 2.0f;
                    return (1.0f - t) * hp + t * notch;
                }
            }

            case Mode::SEMMorph:
            {
                const float angle = semMorphKnob * 1.57079632f;
                const float gainLP = std::cos(angle);
                const float gainHP = std::sin(angle);
                return (gainLP * lp) + (gainHP * hp);
            }

            default:
                return lp;
        }
    }

    inline void processMulti(float input, float& outLP, float& outBP, float& outHP, float& outNotch) noexcept
    {
        const float drivenInput = (drive > 1.001f) ? fastTanh(input * drive) : input;

        const float hp = (drivenInput - (2.0f * RCoeff + gCoeff) * s1 - s2) * invDenom;
        const float bp = gCoeff * hp + s1;
        const float satBP = (resonance > 0.65f) ? fastTanh(bp) : bp;
        s1 = gCoeff * hp + satBP;

        const float lp = gCoeff * satBP + s2;
        s2 = gCoeff * satBP + lp;

        const float notch = hp + lp;

        lastLP    = lp;
        lastBP    = bp;
        lastHP    = hp;
        lastNotch = notch;

        outLP    = lp;
        outBP    = bp;
        outHP    = hp;
        outNotch = notch;
    }

    inline float process(float input, float cutoffHz, float res, int modeIndex) noexcept
    {
        return process(input, cutoffHz, res, modeIndex, currentSampleRate);
    }

    inline float process(float input, float cutoffHz, float res, int modeIndex, double sampleRate) noexcept
    {
        const float safeCutoff = std::clamp(cutoffHz, 15.0f, static_cast<float>(sampleRate * 0.485));
        const float halfNormFreq = static_cast<float>(3.14159265 * safeCutoff / sampleRate);
        const float g = fastTan(halfNormFreq);
        const float R = 1.0f - 0.965f * std::clamp(res, 0.0f, 0.999f);
        const float denom = 1.0f / (1.0f + 2.0f * R * g + g * g);

        const float drivenInput = (drive > 1.001f) ? fastTanh(input * drive) : input;

        const float hp = (drivenInput - (2.0f * R + g) * s1 - s2) * denom;
        const float bp = g * hp + s1;
        const float satBP = (res > 0.65f) ? fastTanh(bp) : bp;
        s1 = g * hp + satBP;

        const float lp = g * satBP + s2;
        s2 = g * satBP + lp;

        const float notch = hp + lp;

        lastLP    = lp;
        lastBP    = bp;
        lastHP    = hp;
        lastNotch = notch;

        switch (modeIndex)
        {
            case 0: return lp;
            case 1: return bp;
            case 2: return hp;
            case 3: return notch;
            case 4: return lp - hp;
            case 5: return hp - (4.0f * R) * bp + lp;
            default: return lp;
        }
    }

    void processBlock(float* data, int numSamples) noexcept
    {
        if (data == nullptr || numSamples <= 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            data[i] = process(data[i]);
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer, int channel, int startSample, int numSamples) noexcept
    {
        if (channel < 0 || channel >= buffer.getNumChannels() || numSamples <= 0)
            return;

        float* channelData = buffer.getWritePointer(channel, startSample);
        processBlock(channelData, numSamples);
    }

private:
    void updateCoefficients() noexcept
    {
        const float halfNormFreq = static_cast<float>(3.14159265 * cutoff / currentSampleRate);
        gCoeff = fastTan(halfNormFreq);
        RCoeff = 1.0f - 0.965f * resonance;
        invDenom = 1.0f / (1.0f + 2.0f * RCoeff * gCoeff + gCoeff * gCoeff);
    }

    double currentSampleRate = 44100.0;
    float cutoff = 1000.0f;
    float resonance = 0.0f;
    float drive = 1.0f;
    Mode mode = Mode::Lowpass;
    float morph = 0.0f;
    float semMorphKnob = 0.0f;

    float s1 = 0.0f;
    float s2 = 0.0f;

    float gCoeff = 0.0f;
    float RCoeff = 1.0f;
    float invDenom = 1.0f;

    float lastLP = 0.0f;
    float lastBP = 0.0f;
    float lastHP = 0.0f;
    float lastNotch = 0.0f;
    float lastPeak = 0.0f;
    float lastAllpass = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateVariableFilter)
};

class StereoStateVariableFilter
{
public:
    StereoStateVariableFilter() noexcept = default;
    ~StereoStateVariableFilter() = default;

    void prepare(double sampleRate) noexcept
    {
        leftFilter.prepare(sampleRate);
        rightFilter.prepare(sampleRate);
        updateStereoCutoffs();
    }

    void reset() noexcept
    {
        leftFilter.reset();
        rightFilter.reset();
    }

    void setCutoff(float cutoffHz) noexcept
    {
        baseCutoff = cutoffHz;
        updateStereoCutoffs();
    }

    void setStereoSpread(float spreadCents) noexcept
    {
        stereoSpreadCents = std::clamp(spreadCents, -1200.0f, 1200.0f);
        updateStereoCutoffs();
    }

    void setResonance(float res) noexcept
    {
        leftFilter.setResonance(res);
        rightFilter.setResonance(res);
    }

    void setDrive(float driveAmount) noexcept
    {
        leftFilter.setDrive(driveAmount);
        rightFilter.setDrive(driveAmount);
    }

    void setMode(StateVariableFilter::Mode mode) noexcept
    {
        leftFilter.setMode(mode);
        rightFilter.setMode(mode);
    }

    void setMode(int modeIndex) noexcept
    {
        leftFilter.setMode(modeIndex);
        rightFilter.setMode(modeIndex);
    }

    void setMorph(float morphVal) noexcept
    {
        leftFilter.setMorph(morphVal);
        rightFilter.setMorph(morphVal);
    }

    void setSEMMorph(float knobVal) noexcept
    {
        leftFilter.setSEMMorph(knobVal);
        rightFilter.setSEMMorph(knobVal);
    }

    inline void process(float& sampleL, float& sampleR) noexcept
    {
        sampleL = leftFilter.process(sampleL);
        sampleR = rightFilter.process(sampleR);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;

        const int numChannels = buffer.getNumChannels();
        if (numChannels == 1)
        {
            leftFilter.processBlock(buffer, 0, startSample, numSamples);
        }
        else if (numChannels >= 2)
        {
            leftFilter.processBlock(buffer, 0, startSample, numSamples);
            rightFilter.processBlock(buffer, 1, startSample, numSamples);
        }
    }

    StateVariableFilter& getLeftFilter() noexcept   { return leftFilter; }
    StateVariableFilter& getRightFilter() noexcept  { return rightFilter; }

private:
    void updateStereoCutoffs() noexcept
    {
        if (std::abs(stereoSpreadCents) < 0.01f)
        {
            leftFilter.setCutoff(baseCutoff);
            rightFilter.setCutoff(baseCutoff);
        }
        else
        {
            const float halfSpreadSemi = (stereoSpreadCents * 0.5f) / 100.0f;
            const float cutoffL = baseCutoff * std::pow(2.0f, -halfSpreadSemi / 12.0f);
            const float cutoffR = baseCutoff * std::pow(2.0f,  halfSpreadSemi / 12.0f);
            leftFilter.setCutoff(cutoffL);
            rightFilter.setCutoff(cutoffR);
        }
    }

    StateVariableFilter leftFilter;
    StateVariableFilter rightFilter;
    float baseCutoff = 1000.0f;
    float stereoSpreadCents = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoStateVariableFilter)
};