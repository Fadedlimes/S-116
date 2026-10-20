#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_core/juce_core.h>
#endif

#if __has_include("PolyBLEPOscillator.h")
 #include "PolyBLEPOscillator.h"
#elif __has_include("DSP/PolyBLEPOscillator.h")
 #include "DSP/PolyBLEPOscillator.h"
#elif __has_include("../DSP/PolyBLEPOscillator.h")
 #include "../DSP/PolyBLEPOscillator.h"
#endif

#include <cmath>
#include <algorithm>
#include <cstdint>

// =============================================================================
// Authentic Analog RC Exponential ADSR Engine
// =============================================================================
class SmoothADSR
{
public:
    enum class State { Idle = 0, Attack, Decay, Sustain, Release };

    SmoothADSR() = default;

    void setSampleRate(double sampleRate) noexcept
    {
        sr = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        recalculateCoeffs();
    }

    void setParameters(float a, float d, float s, float r) noexcept
    {
        constexpr float minT = 0.0025f; // 2.5 ms minimum analog time floor prevents step pops
        attackTime  = std::max(minT, a);
        decayTime   = std::max(minT, d);
        sustainLvl  = std::clamp(s, 0.0f, 1.0f);
        releaseTime = std::max(minT, r);
        recalculateCoeffs();
    }

    void noteOn() noexcept
    {
        state = State::Attack;
    }

    void noteOff() noexcept
    {
        state = State::Release;
    }

    void reset() noexcept
    {
        state = State::Idle;
        level = 0.0f;
    }

    inline float getNextSample() noexcept
    {
        switch (state)
        {
            case State::Attack:
            {
                level += attackCoeff * (1.4f - level);
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    state = State::Decay;
                }
                break;
            }
            case State::Decay:
            {
                level += decayCoeff * (sustainLvl - level);
                if (std::abs(level - sustainLvl) < 0.001f)
                {
                    level = sustainLvl;
                    state = State::Sustain;
                }
                break;
            }
            case State::Sustain:
            {
                level = sustainLvl;
                break;
            }
            case State::Release:
            {
                level += releaseCoeff * (0.0f - level);
                if (level <= 0.0001f)
                {
                    level = 0.0f;
                    state = State::Idle;
                }
                break;
            }
            case State::Idle:
            default:
            {
                level = 0.0f;
                break;
            }
        }
        return level;
    }

    inline float process() noexcept { return getNextSample(); }
    inline bool isActive() const noexcept { return state != State::Idle; }
    inline float getCurrentLevel() const noexcept { return level; }

private:
    void recalculateCoeffs() noexcept
    {
        attackCoeff  = 1.0f - std::exp(-1.0f / (attackTime  * static_cast<float>(sr) * 0.45f));
        decayCoeff   = 1.0f - std::exp(-1.0f / (decayTime   * static_cast<float>(sr) * 0.35f));
        releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTime * static_cast<float>(sr) * 0.35f));
    }

    State state = State::Idle;
    float level = 0.0f;
    float attackTime = 0.01f;
    float decayTime = 0.3f;
    float sustainLvl = 0.7f;
    float releaseTime = 0.5f;

    float attackCoeff = 0.01f;
    float decayCoeff = 0.01f;
    float releaseCoeff = 0.01f;
    double sr = 44100.0;
};

// =============================================================================
// SynthVoice Class
// =============================================================================
class SynthVoice
{
public:
    static constexpr int ANTI_CLICK_SAMPLES = 48;

    SmoothADSR ampEnv;
    SmoothADSR filterEnv;
    SmoothADSR auxEnv;

    SmoothADSR& ampEnvelope = ampEnv;
    SmoothADSR& filterEnvelope = filterEnv;
    SmoothADSR& auxEnvelope = auxEnv;

    SynthVoice() noexcept
    {
        reset();
    }

    ~SynthVoice() = default;

    static inline float fastExp2(float x) noexcept
    {
        return std::exp2(x);
    }

    static inline float fastTan(float x) noexcept 
    {
        const float x2 = x * x;
        return x * (15.0f - x2) / (15.0f - 6.0f * x2);
    }

    static inline float fastTanh(float x) noexcept 
    {
        const float clampedX = std::clamp(x, -9.0f, 9.0f);
        const float exp2x = std::exp2(clampedX * 2.885390f);
        return (exp2x - 1.0f) / (exp2x + 1.0f);
    }

    // =========================================================================
    // Filter 1: Zero-Delay Feedback (ZDF / TPT) 4-Pole Roland IR3109 / 303 Cascade
    // Exact linear trapezoidal state solver with feedback saturation & bass compensation
    // =========================================================================
    struct JunoFilter4Pole
    {
        float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
        float hpfState = 0.0f;
        float dcX = 0.0f, dcY = 0.0f;

        void reset() noexcept
        {
            s1 = s2 = s3 = s4 = 0.0f;
            hpfState = 0.0f;
            dcX = dcY = 0.0f;
        }

        inline float process(float input, float cutoffHz, float resonance, float hpfCutoffHz, double sampleRate) noexcept
        {
            // 1. Transparent 15 Hz DC Blocker + Roland HPF slider
            const float effectiveHpf = std::max(15.0f, hpfCutoffHz);
            const float safeHpf = std::clamp(effectiveHpf, 10.0f, static_cast<float>(sampleRate * 0.40));
            const float g_hpf = fastTan(3.14159265f * safeHpf / static_cast<float>(sampleRate));
            const float hp = (input - hpfState) / (1.0f + g_hpf);
            hpfState += 2.0f * g_hpf * hp;
            input = hp;

            // 2. Safe Cutoff and Bilinear Transform Tuning
            const float safeCutoff = std::clamp(cutoffHz, 15.0f, static_cast<float>(sampleRate * 0.42));
            const float g = fastTan(3.14159265f * safeCutoff / static_cast<float>(sampleRate));
            const float G = g / (1.0f + g);
            const float oneMinusG = 1.0f / (1.0f + g);

            // 3. Roland IR3109 / 303 Resonance Calibration
            const float res = std::clamp(resonance, 0.0f, 1.0f);
            const float resCurve = res * res * (res * 0.35f + 0.65f);
            const float nyquistDamp = 1.0f - 0.22f * (G * G);
            const float k = 3.92f * resCurve * nyquistDamp;

            // Roland passband bass compensation: prevents volume drop as resonance increases
            const float comp = 1.0f + 0.45f * resCurve;

            // 4. Exact Zero-Delay Feedback (ZDF) Closed-Loop Solver
            const float S = s4 + G * (s3 + G * (s2 + G * s1));
            const float G2 = G * G;
            const float G4 = G2 * G2;

            // Soft-saturating S in the feedback path smoothly bounds resonance peaks
            // while preserving full forward gain and low-frequency punch
            const float S_sat = fastTanh(S * 0.95f);
            const float feedbackInput = (input * comp - k * oneMinusG * S_sat) / (1.0f + k * G4);
            const float u = fastTanh(feedbackInput);

            // 5. Four Ladder Integrator Stages (Uncompressed forward path retains full output volume)
            const float v1 = G * (u - s1);
            const float y1 = v1 + s1;
            s1 = y1 + v1;

            const float v2 = G * (y1 - s2);
            const float y2 = v2 + s2;
            s2 = y2 + v2;

            const float v3 = G * (y2 - s3);
            const float y3 = v3 + s3;
            s3 = y3 + v3;

            const float v4 = G * (y3 - s4);
            const float y4 = v4 + s4;
            s4 = y4 + v4;

            // 6. Post-filter DC blocker removes baseline drift without attenuating audibility
            const float R = static_cast<float>(1.0 - (6.2831853 * 15.0 / sampleRate));
            const float outVal = y4 - dcX + R * dcY;
            dcX = y4;
            dcY = outVal;

            return outVal;
        }
    };

    // =========================================================================
    // Filter 2: Oberheim SEM 12dB/oct State Variable Filter (ZDF SVF)
    // =========================================================================
    struct SEMFilter2Pole
    {
        float s1 = 0.0f, s2 = 0.0f;

        void reset() noexcept { s1 = 0.0f; s2 = 0.0f; }

        inline float process(float input, float cutoffHz, float resonance, int mode, double sampleRate) noexcept
        {
            const float safeCutoff = std::clamp(cutoffHz, 20.0f, static_cast<float>(sampleRate * 0.45));
            const float g = fastTan(3.14159265f * safeCutoff / static_cast<float>(sampleRate));
            
            const float resClamped = std::clamp(resonance, 0.0f, 0.98f);
            const float r = std::clamp(1.0f - resClamped * 0.94f, 0.06f, 1.0f);
            const float h = 1.0f / (1.0f + 2.0f * r * g + g * g);

            const float satInput = fastTanh(input * 0.95f);
            const float hp = (satInput - (2.0f * r + g) * s1 - s2) * h;
            const float bp = g * hp + s1;
            s1 = fastTanh(g * hp + bp);
            const float lp = g * bp + s2;
            s2 = fastTanh(g * bp + lp);

            const float bpNorm = 2.0f * r * bp;

            switch (mode)
            {
                case 1:  return bpNorm;                   // Band-Pass (Normalized)
                case 2:  return hp;                       // High-Pass
                case 3:  return 0.707f * (hp + lp);       // Band-Reject / Notch
                case 4:  return 0.5f * (lp - hp);         // All-Pass / Phasor
                case 0:
                default: return lp;                       // 12dB Low-Pass
            }
        }
    };

    void prepare(double sampleRate, int maxSamplesPerBlock) noexcept
    {
        currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        currentBlockSize = maxSamplesPerBlock;

        osc1.setSampleRate(currentSampleRate);
        osc2.setSampleRate(currentSampleRate);

        ampEnv.setSampleRate(currentSampleRate);
        filterEnv.setSampleRate(currentSampleRate);
        auxEnv.setSampleRate(currentSampleRate);

        updatePortamentoCoeff();
        reset();
    }

    void setSampleRate(double sampleRate) noexcept
    {
        prepare(sampleRate, currentBlockSize);
    }

    void reset() noexcept
    {
        osc1.reset();
        osc2.reset();
        osc1.setSupersawDetune(voiceSupersawDetune);

        ampEnv.reset();
        filterEnv.reset();
        auxEnv.reset();

        filter1.reset();
        filter2.reset();

        d1CycleCount = 0;
        noiseState = 0x87654321 ^ (static_cast<uint32_t>(voiceIndex + 1) * 0x9e3779b9);

        currentNoteNumber = -1;
        targetNotePitch = 69.0f;
        glideNotePitch = 69.0f;
        targetFrequency = 440.0f;
        glideCurrentFreq = 440.0f;
        currentVelocity = 0.8f;

        targetKeyTrackOctaves = 0.0f;
        glideKeyTrackOctaves = 0.0f;
        voiceF2KeyTrack = 0.0f;

        active = false;
        isKeyHeld = false;

        currentAmpEnvLevel = 0.0f;
        currentFilterEnvLevel = 0.0f;
        currentAuxEnvLevel = 0.0f;

        antiClickOffsetL = 0.0f;
        antiClickOffsetR = 0.0f;
        antiClickSamplesRemaining = 0;

        lastSampleL = 0.0f;
        lastSampleR = 0.0f;
        voiceTuneOffsetCents = 0.0f;
        voicePan = 0.0f;
        lastOsc1Output = 0.0f;
    }

    void setVoiceIndex(int index) noexcept
    {
        voiceIndex = index;
        noiseState ^= (static_cast<uint32_t>(voiceIndex + 1) * 0x85ebca6b);
    }

    int getVoiceIndex() const noexcept { return voiceIndex; }

    void prepareForSteal() noexcept
    {
        if (active)
        {
            antiClickOffsetL = lastSampleL;
            antiClickOffsetR = lastSampleR;
            antiClickSamplesRemaining = ANTI_CLICK_SAMPLES;
            
            ampEnv.reset();
            filterEnv.reset();
            auxEnv.reset();
            
            osc1.reset();
            osc2.reset();
            
            filter1.reset();
            filter2.reset();
        }
    }

    void noteOn(int midiNote, float velocity, bool isLegato = false) noexcept
    {
        if (active && isLegato)
        {
            updatePitchLegato(midiNote, velocity);
            return;
        }

        const bool wasActive = active;

        if (active && midiNote == currentNoteNumber && !isRelease())
        {
            currentVelocity = (velocity > 0.001f) ? std::clamp(velocity, 0.0f, 1.0f) : 0.8f;
            return;
        }

        if (active && !portamentoEnabled)
        {
            antiClickOffsetL = lastSampleL;
            antiClickOffsetR = lastSampleR;
            antiClickSamplesRemaining = ANTI_CLICK_SAMPLES;

            ampEnv.reset();
            filterEnv.reset();
            auxEnv.reset();
            
            osc1.reset();
            osc2.reset();
        }
        else if (!active)
        {
            antiClickSamplesRemaining = 0;
        }

        currentNoteNumber = midiNote;
        currentVelocity = (velocity > 0.001f) ? std::clamp(velocity, 0.0f, 1.0f) : 0.8f;

        targetNotePitch = calculateTargetPitch(currentNoteNumber);
        targetKeyTrackOctaves = (targetNotePitch - 60.0f) / 12.0f;

        if (!wasActive || !portamentoEnabled)
        {
            glideNotePitch = targetNotePitch;
            glideKeyTrackOctaves = targetKeyTrackOctaves;
            const float pitchWithBend = glideNotePitch + (currentPitchBend * static_cast<float>(pitchBendRange));
            glideCurrentFreq = calculateNoteFrequencyFromPitch(pitchWithBend);
            targetFrequency = glideCurrentFreq;
        }
        else
        {
            targetFrequency = calculateNoteFrequency(currentNoteNumber);
        }

        active = true;
        isKeyHeld = true;

        ampEnv.noteOn();
        filterEnv.noteOn();
        auxEnv.noteOn();
    }

    void noteOff(float velocity = 0.0f, bool allowTail = true) noexcept
    {
        juce::ignoreUnused(velocity);
        isKeyHeld = false;

        if (!allowTail)
        {
            ampEnv.reset();
            filterEnv.reset();
            auxEnv.reset();
            active = false;
            antiClickSamplesRemaining = 0;
            lastSampleL = 0.0f;
            lastSampleR = 0.0f;
            return;
        }

        ampEnv.noteOff();
        filterEnv.noteOff();
        auxEnv.noteOff();
    }

    void updatePitchLegato(int midiNote, float velocity = -1.0f) noexcept
    {
        if (!active)
        {
            noteOn(midiNote, (velocity > 0.001f) ? velocity : 0.8f, false);
            return;
        }

        currentNoteNumber = midiNote;
        if (velocity > 0.001f)
            currentVelocity = std::clamp(velocity, 0.0f, 1.0f);

        targetNotePitch = calculateTargetPitch(currentNoteNumber);
        targetKeyTrackOctaves = (targetNotePitch - 60.0f) / 12.0f;
        targetFrequency = calculateNoteFrequency(currentNoteNumber);

        if (!portamentoEnabled || portamentoTime <= 0.0005f)
        {
            glideNotePitch = targetNotePitch;
            glideKeyTrackOctaves = targetKeyTrackOctaves;
            const float pitchWithBend = glideNotePitch + (currentPitchBend * static_cast<float>(pitchBendRange));
            glideCurrentFreq = calculateNoteFrequencyFromPitch(pitchWithBend);
        }

        if (!isKeyHeld)
        {
            ampEnv.noteOn();
            filterEnv.noteOn();
            auxEnv.noteOn();
        }

        active = true;
        isKeyHeld = true;
    }

    void setGlideTarget(int targetNote, float velocity = -1.0f, bool isLegato = true) noexcept
    {
        if (isLegato && active)
            updatePitchLegato(targetNote, velocity);
        else
            noteOn(targetNote, (velocity > 0.0f) ? velocity : currentVelocity, false);
    }

    void setDetune(float detuneCents) noexcept
    {
        voiceDetuneCents = detuneCents;
        targetNotePitch = calculateTargetPitch(currentNoteNumber);
        targetKeyTrackOctaves = (targetNotePitch - 60.0f) / 12.0f;
        targetFrequency = calculateNoteFrequency(currentNoteNumber);
    }

    void setSupersawDetune(float detune) noexcept
    {
        voiceSupersawDetune = std::clamp(detune, 0.0f, 1.0f);
        osc1.setSupersawDetune(voiceSupersawDetune);
    }

    float getSupersawDetune() const noexcept               { return voiceSupersawDetune; }
    void setPan(float pan) noexcept                        { voicePan = std::clamp(pan, -1.0f, 1.0f); }
    void setVoicePan(float pan) noexcept                   { setPan(pan); }
    void setVoiceTuneOffset(float offset) noexcept         { voiceTuneOffsetCents = offset; }

    void setPortamentoTime(float seconds) noexcept
    {
        portamentoTime = std::max(0.0005f, seconds);
        updatePortamentoCoeff();
    }

    void setGlideTime(float seconds) noexcept              { setPortamentoTime(seconds); }
    void setPortamentoEnabled(bool enabled) noexcept       { portamentoEnabled = enabled; updatePortamentoCoeff(); }
    void setGlideEnabled(bool enabled) noexcept            { setPortamentoEnabled(enabled); }

    void setPitchBend(float normalizedBend) noexcept
    {
        currentPitchBend = std::clamp(normalizedBend, -1.0f, 1.0f);
        if (currentNoteNumber >= 0)
        {
            targetFrequency = calculateNoteFrequency(currentNoteNumber);
        }
    }

    void setPitchBendRange(int semitones) noexcept         { pitchBendRange = std::clamp(semitones, 1, 48); }
    void setModWheel(float normalizedWheel) noexcept       { currentModWheel = std::clamp(normalizedWheel, 0.0f, 1.0f); }
    void setAftertouch(float normalizedAftertouch) noexcept { currentAftertouch = std::clamp(normalizedAftertouch, 0.0f, 1.0f); }

    void setCutoff(float hz) noexcept                      { vcfCutoffHz = std::clamp(hz, 20.0f, 18000.0f); }
    void setResonance(float res) noexcept                  { vcfResonance = std::clamp(res, 0.0f, 1.0f); }

    void setF2KeyTrack(float keyTrack) noexcept            { voiceF2KeyTrack = std::clamp(keyTrack, -2.0f, 2.0f); }
    void setFilter2KeyTrack(float keyTrack) noexcept       { setF2KeyTrack(keyTrack); }
    float getF2KeyTrack() const noexcept                   { return voiceF2KeyTrack; }

    void setVoiceLevel(float level) noexcept               { voiceLevel = std::clamp(level, 0.0f, 1.0f); }

    bool isActive() const noexcept                         { return active; }
    bool isRelease() const noexcept                        { return !isKeyHeld && active; }
    bool isReleasing() const noexcept                      { return isRelease(); }
    float getEnvelopeLevel() const noexcept                { return currentAmpEnvLevel; }
    float getEnvLevel() const noexcept                     { return getEnvelopeLevel(); }
    int getNoteNumber() const noexcept                     { return currentNoteNumber; }

    // =========================================================================
    // Native Multi-Parameter Process Method
    // =========================================================================
    template <typename T1, typename T2, typename T3, typename T4, typename T5,
              typename T6, typename T7, typename T8, typename T9, typename T10,
              typename T11, typename T12, typename T13, typename T14, typename T15,
              typename T16, typename T17, typename T18, typename T19, typename T20,
              typename T21, typename T22, typename T23>
    inline void process(
        T1 modD1Morph, T2 modD1PWM, T3 d1Octave, T4 modD1Fold, T5 d1SawMix,
        T6 modD2Morph, T7 modD2PWM, T8 d2Level, T9 modD2Semi, T10 modD2Cents,
        T11 syncOn, T12 modXMod, T13 xmodM,
        T14 subOct, T15 modSub, T16 modNoise,
        T17 modHpf, T18 modCutoff, T19 modRes, T20 modEnvMod,
        T21 lfo1Val, T22 lfo1ToFilt, T23 lfo1ToPitch,
        float& vL, float& vR,
        float f2Cut = 3000.0f, float f2Reso = 0.1f, int f2M = 0, float f2EnvMod = 0.0f,
        bool fParallel = false, float fBal = 0.5f,
        float auxPitchMod = 0.0f, float auxF1Mod = 0.0f, float auxF2Mod = 0.0f,
        float auxFoldMod = 0.0f, float auxPWMMod = 0.0f,
        float f2KeyTrack = 0.0f,
        float d1SawDetune = -1.0f, float fmAmount = 0.0f, float modD2Fold = 0.0f,
        int subWave = 0
    ) noexcept
    {
        if (!active)
        {
            vL = 0.0f; vR = 0.0f;
            return;
        }

        // 1. Authentic Semitone Portamento Pitch Slew
        if (portamentoEnabled && portamentoCoeff < 1.0f)
        {
            glideNotePitch += portamentoCoeff * (targetNotePitch - glideNotePitch);
            glideKeyTrackOctaves += portamentoCoeff * (targetKeyTrackOctaves - glideKeyTrackOctaves);
        }
        else
        {
            glideNotePitch = targetNotePitch;
            glideKeyTrackOctaves = targetKeyTrackOctaves;
        }

        const float currentPitchWithBend = glideNotePitch + (currentPitchBend * static_cast<float>(pitchBendRange));
        glideCurrentFreq = calculateNoteFrequencyFromPitch(currentPitchWithBend);

        const float activeFilterEnv = filterEnv.getNextSample();
        const float activeAmpEnv    = ampEnv.getNextSample();
        const float activeAuxEnv    = auxEnv.getNextSample();

        currentAmpEnvLevel = activeAmpEnv;
        currentFilterEnvLevel = activeFilterEnv;
        currentAuxEnvLevel = activeAuxEnv;

        if (!isKeyHeld && activeAmpEnv <= 0.0001f && antiClickSamplesRemaining == 0)
        {
            active = false;
            vL = 0.0f; vR = 0.0f;
            lastSampleL = 0.0f; lastSampleR = 0.0f;
            return;
        }

        // 2. DCO Pitch Modulations & Octave Multipliers
        float d1OctShift = 0.0f;
        const int iOct1 = static_cast<int>(d1Octave);
        if (iOct1 >= 0 && iOct1 <= 3)       d1OctShift = static_cast<float>(iOct1 - 2);
        else if (iOct1 == 16)               d1OctShift = -1.0f;
        else if (iOct1 == 8)                d1OctShift = 0.0f;
        else if (iOct1 == 4)                d1OctShift = 1.0f;
        else if (iOct1 == 32)               d1OctShift = -2.0f;
        else if (iOct1 >= -3 && iOct1 <= 3) d1OctShift = static_cast<float>(iOct1);

        const float totalPitchMod = ((static_cast<float>(lfo1Val) * static_cast<float>(lfo1ToPitch) * 2.0f) + (activeAuxEnv * auxPitchMod)) / 12.0f;
        const float lfoPitchFactor = fastExp2(totalPitchMod);

        const float d1Freq = std::clamp(glideCurrentFreq * fastExp2(d1OctShift) * lfoPitchFactor,
                                        10.0f, static_cast<float>(currentSampleRate * 0.485));

        // 3. DCO 1 Processing
        osc1.setFrequency(d1Freq);
        osc1.setWaveMorph(std::clamp(static_cast<float>(modD1Morph), 0.0f, 1.0f));
        osc1.setPulseWidth(std::clamp(static_cast<float>(modD1PWM) + activeAuxEnv * auxPWMMod * 0.4f, 0.05f, 0.95f));
        osc1.setSupersawMix(std::clamp(static_cast<float>(d1SawMix), 0.0f, 1.0f));

        const float effectiveSawDetune = (d1SawDetune >= 0.0f) ? d1SawDetune : voiceSupersawDetune;
        osc1.setSupersawDetune(std::clamp(effectiveSawDetune, 0.0f, 1.0f));

        bool d1Wrapped = false;
        double d1WrapFrac = 0.0;
        float osc1Sample = osc1.process(&d1Wrapped, &d1WrapFrac);
        if (d1Wrapped) ++d1CycleCount;

        const float fFold1 = std::clamp(static_cast<float>(modD1Fold) + activeAuxEnv * auxFoldMod, 0.0f, 1.0f);
        if (fFold1 > 0.001f)
            osc1Sample = PolyBLEPOscillator::applyWavefold(osc1Sample, fFold1);

        lastOsc1Output = osc1Sample;

        // 4. Phase-Locked Roland Sub-Oscillator (Square or Sine, phase locked to DCO 1)
        float subSample = 0.0f;
        const float fSubLevel = std::clamp(static_cast<float>(modSub), 0.0f, 1.0f);
        if (fSubLevel > 0.001f)
        {
            const int iSubOct = static_cast<int>(subOct);
            const double subDiv = (iSubOct <= -2 || iSubOct == 1) ? 4.0 : 2.0;
            const double d1Phase = osc1.getPhase();
            const double d1Inc = osc1.getPhaseIncrement();

            const double subPhase = ((d1CycleCount % static_cast<int>(subDiv)) + d1Phase) / subDiv;
            const double subDt = d1Inc / subDiv;

            double s = 0.0;

            if (subWave == 1)
            {
                // Pure sine sub: no band-limiting steps required, it is already continuous.
                s = std::sin (subPhase * juce::MathConstants<double>::twoPi);
            }
            else
            {
                // Classic band-limited square sub (PolyBLEP corrected).
                s = (subPhase < 0.5) ? 1.0 : -1.0;
                s += PolyBLEPOscillator::polyBlep(subPhase, subDt);

                double t2 = subPhase + 0.5;
                if (t2 >= 1.0) t2 -= 1.0;
                s -= PolyBLEPOscillator::polyBlep(t2, subDt);
            }

            subSample = static_cast<float>(s);
        }

        // 5. DCO 2 Processing with Audio-Rate Linear/Exponential FM & DCO2 Wavefolder
        const float d2SemiOffset = static_cast<float>(modD2Semi) + (static_cast<float>(modD2Cents) * 0.01f);
        float d2Freq = std::clamp(glideCurrentFreq * fastExp2((d1OctShift * 12.0f + d2SemiOffset) / 12.0f) * lfoPitchFactor,
                                  10.0f, static_cast<float>(currentSampleRate * 0.485));

        const float effectiveFM = std::clamp(std::max(static_cast<float>(modXMod), fmAmount), 0.0f, 1.0f);
        if (effectiveFM > 0.001f)
        {
            const int iXModMode = static_cast<int>(xmodM);
            if (iXModMode == 1)
            {
                d2Freq = std::clamp(d2Freq * fastExp2(osc1Sample * effectiveFM * 3.0f),
                                    10.0f, static_cast<float>(currentSampleRate * 0.485));
            }
            else
            {
                const float fmDevHz = osc1Sample * effectiveFM * d2Freq * 2.2f;
                d2Freq = std::clamp(d2Freq + fmDevHz, 10.0f, static_cast<float>(currentSampleRate * 0.485));
            }
        }

        const bool isSyncOn = static_cast<bool>(syncOn);
        float osc2Sample = 0.0f;
        const float fD2Level = std::clamp(static_cast<float>(d2Level), 0.0f, 1.0f);

        if (fD2Level > 0.001f || effectiveFM > 0.001f)
        {
            osc2.setFrequency(d2Freq);
            osc2.setWaveMorph(std::clamp(static_cast<float>(modD2Morph), 0.0f, 1.0f));
            osc2.setPulseWidth(std::clamp(static_cast<float>(modD2PWM), 0.05f, 0.95f));

            if (isSyncOn)
                osc2Sample = osc2.processSynced(d1Wrapped, d1WrapFrac);
            else
                osc2Sample = osc2.process();

            const float fFold2 = std::clamp(static_cast<float>(modD2Fold) + activeAuxEnv * auxFoldMod, 0.0f, 1.0f);
            if (fFold2 > 0.001f)
                osc2Sample = PolyBLEPOscillator::applyWavefold(osc2Sample, fFold2);
        }

        // 6. White Noise Generator
        const float rawNoise = (static_cast<float>(nextNoise()) * (2.0f / 4294967295.0f)) - 1.0f;
        const float fNoiseLevel = std::clamp(static_cast<float>(modNoise), 0.0f, 1.0f);

        // 7. Calibrated Analog Mixer (0.90f scales healthy analog drive into the filter)
        const float fD1Level = 1.0f;
        const float oscMix = 0.90f * ((osc1Sample * fD1Level)
                                   + (osc2Sample * fD2Level)
                                   + (subSample * fSubLevel)
                                   + (rawNoise * fNoiseLevel));

        // 8. Filter 1 (24dB Zero-Delay Feedback Roland IR3109 / 303 Cascade)
        const float rawCutoff = static_cast<float>(modCutoff);
        const float baseCutoff1 = std::clamp(rawCutoff, 20.0f, 16000.0f);

        const float cutoffModOct1 = (activeFilterEnv * static_cast<float>(modEnvMod) * 3.4f)
                                  + (static_cast<float>(lfo1Val) * static_cast<float>(lfo1ToFilt) * 1.5f)
                                  + (activeAuxEnv * auxF1Mod * 2.5f);

        const float activeCutoff1 = std::clamp(baseCutoff1 * fastExp2(cutoffModOct1), 20.0f, static_cast<float>(currentSampleRate * 0.42));

        float hpfCutoff = static_cast<float>(modHpf);
        if (hpfCutoff <= 3.5f)
        {
            if (hpfCutoff < 0.5f)      hpfCutoff = 0.0f;
            else if (hpfCutoff < 1.5f) hpfCutoff = 60.0f;
            else if (hpfCutoff < 2.5f) hpfCutoff = 160.0f;
            else                       hpfCutoff = 380.0f;
        }

        const float outF1 = filter1.process(oscMix, activeCutoff1, static_cast<float>(modRes),
                                            hpfCutoff, currentSampleRate);

        // 9. Filter 2 (12dB SEM SVF with Calibrated Keyboard Tracking)
        const float effectiveF2KeyTrack = (std::abs(f2KeyTrack) > 0.00001f) ? f2KeyTrack : voiceF2KeyTrack;
        const float keyTrackOctaves2 = glideKeyTrackOctaves * effectiveF2KeyTrack;

        const float cutoffModOct2 = (activeFilterEnv * f2EnvMod * 3.5f)
                                  + (activeAuxEnv * auxF2Mod * 3.0f)
                                  + keyTrackOctaves2;
        const float activeCutoff2 = std::clamp(f2Cut * fastExp2(cutoffModOct2), 20.0f, static_cast<float>(currentSampleRate * 0.42));

        float filtered = outF1;
        if (fParallel)
        {
            const float outF2 = filter2.process(oscMix, activeCutoff2, f2Reso, f2M, currentSampleRate);
            filtered = (1.0f - fBal) * outF1 + fBal * outF2;
        }
        else
        {
            filtered = filter2.process(outF1, activeCutoff2, f2Reso, f2M, currentSampleRate);
            if (fBal < 0.99f) filtered = (1.0f - fBal) * outF1 + fBal * filtered;
        }

        // 10. VCA & Panning (1.414x compensation recovers standard 3dB pan law loss)
        const float vcaGain = activeAmpEnv * currentVelocity * voiceLevel * 1.414f;
        const float totalPan = std::clamp(voicePan, -1.0f, 1.0f);
        const float panL = std::sqrt(0.5f * (1.0f - totalPan));
        const float panR = std::sqrt(0.5f * (1.0f + totalPan));

        float outL = filtered * vcaGain * panL;
        float outR = filtered * vcaGain * panR;

        // 11. Anti-Click Stealing Compensation
        if (antiClickSamplesRemaining > 0)
        {
            const float ramp = static_cast<float>(antiClickSamplesRemaining) / static_cast<float>(ANTI_CLICK_SAMPLES);
            outL += antiClickOffsetL * ramp;
            outR += antiClickOffsetR * ramp;
            --antiClickSamplesRemaining;
        }

        lastSampleL = outL;
        lastSampleR = outR;

        vL = outL;
        vR = outR;
    }

    inline void process(float& l, float& r) noexcept
    {
        float outL = 0.0f, outR = 0.0f;
        process(0.25f, 0.5f, 2, 0.0f, 0.0f,
                0.25f, 0.5f, 0.0f, 0.0f, 0.0f,
                false, 0.0f, 0,
                -1, 0.0f, 0.0f,
                0.0f, 2500.0f, 0.15f, 0.5f,
                0.0f, 0.0f, 0.0f,
                outL, outR);
        l = outL; r = outR;
    }

    inline float process() noexcept
    {
        float l = 0.0f, r = 0.0f;
        process(l, r);
        return 0.5f * (l + r);
    }

private:
    inline float calculateTargetPitch(int noteNumber) const noexcept
    {
        return static_cast<float>(noteNumber)
             + (tuneFineCents + voiceDetuneCents + voiceTuneOffsetCents) * 0.01f;
    }

    inline float calculateNoteFrequencyFromPitch(float pitch) const noexcept
    {
        return 440.0f * fastExp2((pitch - 69.0f) / 12.0f);
    }

    float calculateNoteFrequency(int noteNumber) const noexcept
    {
        const float pitchTotal = calculateTargetPitch(noteNumber)
                               + (currentPitchBend * static_cast<float>(pitchBendRange));
        return calculateNoteFrequencyFromPitch(pitchTotal);
    }

    void updatePortamentoCoeff() noexcept
    {
        if (portamentoEnabled && portamentoTime > 0.0005f)
        {
            const float tauSamples = (portamentoTime / 4.6f) * static_cast<float>(currentSampleRate);
            portamentoCoeff = 1.0f - std::exp(-1.0f / std::max(1.0f, tauSamples));
        }
        else
        {
            portamentoCoeff = 1.0f;
        }
    }

    inline uint32_t nextNoise() noexcept
    {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        return noiseState;
    }

    PolyBLEPOscillator osc1;
    PolyBLEPOscillator osc2;
    JunoFilter4Pole filter1;
    SEMFilter2Pole filter2;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int voiceIndex = 0;

    int currentNoteNumber = -1;
    float currentVelocity = 0.8f;
    bool active = false;
    bool isKeyHeld = false;

    float targetNotePitch = 69.0f;
    float glideNotePitch = 69.0f;
    float targetFrequency = 440.0f;
    float glideCurrentFreq = 440.0f;
    float portamentoTime = 0.05f;
    float portamentoCoeff = 1.0f;
    bool portamentoEnabled = false;

    float voiceF2KeyTrack = 0.0f;
    float targetKeyTrackOctaves = 0.0f;
    float glideKeyTrackOctaves = 0.0f;

    float voiceDetuneCents = 0.0f;
    float voicePan = 0.0f;
    float voiceTuneOffsetCents = 0.0f;
    float tuneFineCents = 0.0f;
    float voiceSupersawDetune = 0.5f;

    int pitchBendRange = 2;
    float currentPitchBend = 0.0f;
    float currentModWheel = 0.0f;
    float currentAftertouch = 0.0f;

    uint32_t d1CycleCount = 0;
    uint32_t noiseState = 0x87654321;
    float lastOsc1Output = 0.0f;

    float vcfCutoffHz = 2500.0f;
    float vcfResonance = 0.15f;

    float voiceLevel = 1.0f;

    float currentAmpEnvLevel = 0.0f;
    float currentFilterEnvLevel = 0.0f;
    float currentAuxEnvLevel = 0.0f;

    float antiClickOffsetL = 0.0f;
    float antiClickOffsetR = 0.0f;
    int antiClickSamplesRemaining = 0;

    float lastSampleL = 0.0f;
    float lastSampleR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthVoice)
};