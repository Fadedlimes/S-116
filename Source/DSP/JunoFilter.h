#pragma once
#include <cmath>
#include <algorithm>

class JunoFilter {
public:
    void setSampleRate(double sr) {
        sampleRate = (sr > 1000.0) ? sr : 44100.0;
        updateHPFCoeff();
        updateLPFCoeff();
    }

    void setHPFCutoff(float cutoffHz) {
        hpfCutoff = std::clamp(cutoffHz, 10.0f, 2000.0f);
        updateHPFCoeff();
    }

    void setLPFParams(float cutoffHz, float resonance) {
        float maxSafeCutoff = static_cast<float>(sampleRate * 0.45);
        lpfCutoff = std::clamp(cutoffHz, 20.0f, maxSafeCutoff);
        lpfResonance = std::clamp(resonance, 0.0f, 1.0f);
        updateLPFCoeff();
    }

    float process(float input) {
        // --- 1. High-Pass Filter Stage (1-pole) ---
        float hpfOut = hpfCoeff * (hpfPrevOut + input - hpfPrevIn);
        hpfPrevIn = input;
        hpfPrevOut = hpfOut;

        // Anti-denormal flush for HPF
        if (std::abs(hpfPrevOut) < 1.0e-15f) hpfPrevOut = 0.0f;
        if (std::abs(hpfPrevIn) < 1.0e-15f) hpfPrevIn = 0.0f;

        // --- 2. Low-Pass Filter Stage (4-pole cascade with 2x oversampling) ---
        for (int i = 0; i < 2; ++i) {
            float feedback = s4 - (hpfOut * 0.5f);
            float saturatedInput = fastTanh(hpfOut - k * feedback);

            s1 += g * (saturatedInput - s1);
            s2 += g * (s1 - s2);
            s3 += g * (s2 - s3);
            s4 += g * (s3 - s4);
        }

        // Anti-denormal flush on integrator states
        if (std::abs(s1) < 1.0e-15f) s1 = 0.0f;
        if (std::abs(s2) < 1.0e-15f) s2 = 0.0f;
        if (std::abs(s3) < 1.0e-15f) s3 = 0.0f;
        if (std::abs(s4) < 1.0e-15f) s4 = 0.0f;

        return s4;
    }

    void reset() {
        s1 = s2 = s3 = s4 = 0.0f;
        hpfPrevIn = hpfPrevOut = 0.0f;
    }

private:
    static inline float fastTanh(float x) {
        if (x <= -3.0f) return -1.0f;
        if (x >= 3.0f) return 1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    void updateHPFCoeff() {
        float omega = 2.0f * 3.141592653589793f * (hpfCutoff / static_cast<float>(sampleRate));
        hpfCoeff = 1.0f / (1.0f + omega);
    }

    void updateLPFCoeff() {
        g = std::tan(3.141592653589793f * (lpfCutoff / static_cast<float>(2.0 * sampleRate)));
        g = std::clamp(g, 0.0001f, 0.70f);
        k = 3.95f * lpfResonance;
    }

    double sampleRate = 44100.0;
    float hpfCutoff = 20.0f;
    float hpfCoeff = 0.0f;
    float hpfPrevIn = 0.0f;
    float hpfPrevOut = 0.0f;

    float lpfCutoff = 1000.0f;
    float lpfResonance = 0.0f;
    float g = 0.1f;
    float k = 0.0f;
    float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
};