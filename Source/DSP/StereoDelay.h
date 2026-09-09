#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class StereoDelay {
public:
    StereoDelay() {
        setSampleRate(44100.0);
    }

    void setSampleRate(double sr) {
        sampleRate = (sr > 1000.0) ? sr : 44100.0;
        maxDelaySamples = static_cast<int>(sampleRate * 2.2); // 2.2s safe buffer
        bufferL.assign(static_cast<size_t>(maxDelaySamples), 0.0f);
        bufferR.assign(static_cast<size_t>(maxDelaySamples), 0.0f);
        writeIndex = 0;
        filteredL = 0.0f;
        filteredR = 0.0f;
    }

    void setParameters(float timeSec, float fb, float damp, float mixVal, bool pingPong) noexcept {
        delayTimeSec = std::clamp(timeSec, 0.01f, 1.95f);
        feedback = std::clamp(fb, 0.0f, 0.95f);
        damping = std::clamp(damp, 0.0f, 0.95f);
        mix = std::clamp(mixVal, 0.0f, 1.0f);
        isPingPong = pingPong;
    }

    void process(float inL, float inR, float& outL, float& outR) noexcept {
        if (mix <= 0.001f || maxDelaySamples <= 0) {
            outL = inL;
            outR = inR;
            return;
        }

        const float delaySamples = delayTimeSec * static_cast<float>(sampleRate);
        float readPos = static_cast<float>(writeIndex) - delaySamples;
        while (readPos < 0.0f) readPos += static_cast<float>(maxDelaySamples);

        const int i0 = static_cast<int>(readPos) % maxDelaySamples;
        const int i1 = (i0 + 1) % maxDelaySamples;
        const float frac = readPos - static_cast<float>(static_cast<int>(readPos));

        const float delayedL = bufferL[static_cast<size_t>(i0)] + frac * (bufferL[static_cast<size_t>(i1)] - bufferL[static_cast<size_t>(i0)]);
        const float delayedR = bufferR[static_cast<size_t>(i0)] + frac * (bufferR[static_cast<size_t>(i1)] - bufferR[static_cast<size_t>(i0)]);

        // Musical 1-pole damping: sweeps from fully bright (alpha=1.0) down to 2.5kHz (alpha=0.15)
        const float alpha = 1.0f - (damping * 0.85f);
        filteredL += alpha * (delayedL - filteredL);
        filteredR += alpha * (delayedR - filteredR);

        // Anti-denormal flush
        if (std::abs(filteredL) < 1e-15f) filteredL = 0.0f;
        if (std::abs(filteredR) < 1e-15f) filteredR = 0.0f;

        // Feedback path with analog soft-clipping to prevent runaway wavefolding
        const float fbL = std::tanh(filteredL * feedback);
        const float fbR = std::tanh(filteredR * feedback);

        if (isPingPong) {
            // True ping-pong: mono injected to L, L repeats to R, R repeats to L
            const float monoIn = 0.5f * (inL + inR);
            bufferL[static_cast<size_t>(writeIndex)] = monoIn + fbR;
            bufferR[static_cast<size_t>(writeIndex)] = fbL;
        } else {
            bufferL[static_cast<size_t>(writeIndex)] = inL + fbL;
            bufferR[static_cast<size_t>(writeIndex)] = inR + fbR;
        }

        writeIndex = (writeIndex + 1) % maxDelaySamples;

        // Equal-power wet/dry crossfade for punchy, present repeats
        const float dryGain = 1.0f - (mix * 0.35f);
        outL = (inL * dryGain) + (delayedL * mix);
        outR = (inR * dryGain) + (delayedR * mix);
    }

    void processBlock(float* outL, float* outR, int numSamples) noexcept {
        if (mix <= 0.001f || maxDelaySamples <= 0 || numSamples <= 0) return;

        for (int i = 0; i < numSamples; ++i) {
            float dL = outL[i];
            float dR = outR[i];
            process(outL[i], outR[i], dL, dR);
            outL[i] = dL;
            outR[i] = dR;
        }
    }

    void reset() noexcept {
        std::fill(bufferL.begin(), bufferL.end(), 0.0f);
        std::fill(bufferR.begin(), bufferR.end(), 0.0f);
        filteredL = 0.0f;
        filteredR = 0.0f;
        writeIndex = 0;
    }

private:
    double sampleRate = 44100.0;
    int maxDelaySamples = 97020;
    int writeIndex = 0;
    std::vector<float> bufferL;
    std::vector<float> bufferR;

    float delayTimeSec = 0.35f;
    float feedback = 0.40f;
    float damping = 0.25f;
    float mix = 0.0f;
    bool isPingPong = true;
    float filteredL = 0.0f;
    float filteredR = 0.0f;
};