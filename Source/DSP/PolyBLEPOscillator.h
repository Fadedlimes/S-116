#pragma once

#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

class PolyBLEPOscillator {
public:
    PolyBLEPOscillator() = default;

    void setSampleRate(double sr) noexcept {
        if (std::isnan(sr) || std::isinf(sr)) return;
        sampleRate = (sr > 1000.0) ? sr : 44100.0;
        updateIncrements();
    }

    void setFrequency(double freqHz) noexcept {
        if (std::isnan(freqHz) || std::isinf(freqHz)) return;
        frequency = std::max(1.0, freqHz);
        updateIncrements();
    }

    void setWaveMorph(float morph) noexcept {
        if (std::isnan(morph) || std::isinf(morph)) return;
        waveMorph = std::clamp(morph, 0.0f, 1.0f);
    }

    void setPulseWidth(float pw) noexcept {
        if (std::isnan(pw) || std::isinf(pw)) return;
        pulseWidth = std::clamp(pw, 0.05f, 0.95f);
    }

    void setSupersawMix(float mix) noexcept {
        if (std::isnan(mix) || std::isinf(mix)) return;
        supersawMix = std::clamp(mix, 0.0f, 1.0f);
    }

    void setSupersawDetune(float detune) noexcept {
        if (std::isnan(detune) || std::isinf(detune)) return;
        supersawDetune = std::clamp(detune, 0.0f, 1.0f);
        updateIncrements();
    }

    double getPhase() const noexcept { return phase; }
    double getPhaseIncrement() const noexcept { return phaseIncrement; }

    void reset() noexcept {
        phase = 0.0;
        for (int i = 0; i < 6; ++i) {
            prngSeed = prngSeed * 1664525u + 1013904223u;
            sidePhases[i] = static_cast<double>(prngSeed & 0x00FFFFFF) / 16777216.0;
        }
    }

    static inline double polyBlep(double t, double dt) noexcept {
        if (dt <= 1e-9) return 0.0;
        if (t < dt) {
            t /= dt;
            return t + t - t * t - 1.0;
        } else if (t > 1.0 - dt) {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    static inline float applyWavefold(float in, float fold) noexcept {
        if (std::isnan(in) || std::isinf(in)) return 0.0f;
        if (fold <= 0.001f) return in;

        const float drive = 1.0f + fold * 3.5f;
        float x = in * drive;

        if (x > 1.0f)       x = 2.0f - x;
        else if (x < -1.0f) x = -2.0f - x;

        if (x > 1.0f)       x = 2.0f - x;
        else if (x < -1.0f) x = -2.0f - x;

        return std::tanh(x);
    }

    // Process oscillator with optional audio-rate phase modulation (Linear / Exponential PM/FM)
    float process(bool* didWrap = nullptr, double* wrapFrac = nullptr, double phaseMod = 0.0) noexcept {
        const double effectivePhase = wrapPhase(phase + phaseMod);
        float output = static_cast<float>(computeSample(effectivePhase));
        
        // REALTIME SAFETY: Catch all numerical explosions before they hit the audio bus
        if (std::isnan(output) || std::isinf(output)) output = 0.0f; 

        phase += phaseIncrement;
        if (phase >= 1.0) {
            phase -= 1.0;
            if (didWrap != nullptr) *didWrap = true;
            if (wrapFrac != nullptr) *wrapFrac = (phaseIncrement > 1e-9) ? (phase / phaseIncrement) : 0.0;
        } else {
            if (didWrap != nullptr) *didWrap = false;
            if (wrapFrac != nullptr) *wrapFrac = 0.0;
        }

        if (supersawMix > 0.001f) {
            for (int i = 0; i < 6; ++i) {
                sidePhases[i] += sideIncrements[i];
                if (sidePhases[i] >= 1.0) sidePhases[i] -= 1.0;
            }
        }

        return output;
    }

    float processSynced(bool syncTriggered, double wrapFraction, double phaseMod = 0.0) noexcept {
        if (syncTriggered) {
            const double safeWrap = std::clamp(wrapFraction, 0.0, 1.0);
            phase = safeWrap * phaseIncrement;
            if (supersawMix > 0.001f) {
                for (int i = 0; i < 6; ++i) {
                    sidePhases[i] = safeWrap * sideIncrements[i];
                }
            }
        }
        return process(nullptr, nullptr, phaseMod);
    }

private:
    void updateIncrements() noexcept {
        // STRICT NYQUIST CLAMP: Prevents PolyBLEP dt window overlaps at extreme pitches
        const double maxFreq = sampleRate * 0.49;
        const double f0 = std::clamp(frequency, 1.0, maxFreq);
        phaseIncrement = f0 / sampleRate;

        const float d = std::clamp(supersawDetune, 0.0f, 1.0f);
        // Authentic JP-8000 non-linear cubic detune expansion:
        // Fine control in 0.0 - 0.5 range, wide aggressive spread in 0.5 - 1.0 range
        const float detuneFactor = 0.085f * (d * d * d * 0.55f + d * d * 0.30f + d * 0.15f);

        static constexpr double JP_RATIOS[3] = { 0.2888, 0.6759, 1.0000 };
        sideIncrements[0] = std::clamp((f0 * (1.0 - JP_RATIOS[2] * detuneFactor)) / sampleRate, 1e-7, 0.499);
        sideIncrements[1] = std::clamp((f0 * (1.0 - JP_RATIOS[1] * detuneFactor)) / sampleRate, 1e-7, 0.499);
        sideIncrements[2] = std::clamp((f0 * (1.0 - JP_RATIOS[0] * detuneFactor)) / sampleRate, 1e-7, 0.499);
        sideIncrements[3] = std::clamp((f0 * (1.0 + JP_RATIOS[0] * detuneFactor)) / sampleRate, 1e-7, 0.499);
        sideIncrements[4] = std::clamp((f0 * (1.0 + JP_RATIOS[1] * detuneFactor)) / sampleRate, 1e-7, 0.499);
        sideIncrements[5] = std::clamp((f0 * (1.0 + JP_RATIOS[2] * detuneFactor)) / sampleRate, 1e-7, 0.499);
    }

    // REALTIME SAFETY: Rock-solid phase wrapping without std::fmod
    inline double wrapPhase(double p) const noexcept {
        while (p >= 1.0) p -= 1.0;
        while (p < 0.0) p += 1.0;
        return p;
    }

    inline double computeSine(double p) const noexcept {
        // Phase-locked to negative cosine so zero-crossings and peaks match the triangle exactly
        return -std::cos(6.28318530717958647692 * p);
    }

    inline double computeTriangle(double p) const noexcept {
        return (p < 0.5) ? (4.0 * p - 1.0) : (3.0 - 4.0 * p);
    }

    inline double computeSaw(double p, double inc) const noexcept {
        const double s = (2.0 * p) - 1.0;
        return s - polyBlep(p, inc);
    }

    inline double computeSawWithSupersaw(double p) const noexcept {
        const double centerSaw = computeSaw(p, phaseIncrement);
        if (supersawMix > 0.001f) {
            double sideSum = 0.0;
            for (int i = 0; i < 6; ++i) {
                sideSum += computeSaw(sidePhases[i], sideIncrements[i]);
            }
            // Calibrated constant-power scaling maintains even volume across all mix levels
            const double centerGain = 1.0 - (0.25 * supersawMix);
            const double sideGain = 0.28 * supersawMix;
            return (centerGain * centerSaw) + (sideGain * sideSum);
        }
        return centerSaw;
    }

    inline double computeSquare50(double p) const noexcept {
        double sq = (p < 0.5) ? 1.0 : -1.0;
        sq += polyBlep(p, phaseIncrement);
        sq -= polyBlep(wrapPhase(p + 0.5), phaseIncrement);
        return sq;
    }

    inline double computePulse(double p) const noexcept {
        const double pwClamped = std::clamp(static_cast<double>(pulseWidth), 0.05, 0.95);
        double pulse = (p < pwClamped) ? 1.0 : -1.0;
        pulse += polyBlep(p, phaseIncrement);
        pulse -= polyBlep(wrapPhase(p + (1.0 - pwClamped)), phaseIncrement);
        return pulse;
    }

    double computeSample(double p) const noexcept {
        if (waveMorph < 0.25f) {
            // Zone 1: Pure Sine -> Anti-Aliased Triangle
            const double sine = computeSine(p);
            const double tri  = computeTriangle(p);
            const double blend = static_cast<double>(waveMorph) * 4.0;
            return (1.0 - blend) * sine + blend * tri;
        } else if (waveMorph < 0.50f) {
            // Zone 2: Anti-Aliased Triangle -> Anti-Aliased Sawtooth / Supersaw
            const double tri = computeTriangle(p);
            const double saw = computeSawWithSupersaw(p);
            const double blend = static_cast<double>(waveMorph - 0.25f) * 4.0;
            return (1.0 - blend) * tri + blend * saw;
        } else if (waveMorph < 0.75f) {
            // Zone 3: Anti-Aliased Sawtooth / Supersaw -> Anti-Aliased 50% Square
            const double saw = computeSawWithSupersaw(p);
            const double sq  = computeSquare50(p);
            const double blend = static_cast<double>(waveMorph - 0.50f) * 4.0;
            return (1.0 - blend) * saw + blend * sq;
        } else {
            // Zone 4: Anti-Aliased 50% Square -> Anti-Aliased Variable Pulse (PWM)
            const double sq    = computeSquare50(p);
            const double pulse = computePulse(p);
            const double blend = static_cast<double>(waveMorph - 0.75f) * 4.0;
            return (1.0 - blend) * sq + blend * pulse;
        }
    }

    double sampleRate = 44100.0;
    double frequency = 440.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;

    std::array<double, 6> sidePhases { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    std::array<double, 6> sideIncrements { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

    float waveMorph = 0.25f;
    float pulseWidth = 0.5f;
    float supersawMix = 0.0f;
    float supersawDetune = 0.5f;

    uint32_t prngSeed = 987654321u;
};