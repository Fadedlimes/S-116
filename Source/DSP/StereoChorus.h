#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class StereoChorus {
public:
    enum Mode { Off = 0, Chorus1, Chorus2, Chorus1And2 };

    StereoChorus() {
        setSampleRate(44100.0);
        setMode(Chorus1);
    }

    void setSampleRate(double sr) {
        sampleRate = (sr > 1000.0) ? sr : 44100.0;
        // Nearest power-of-two buffer size >= 100ms
        bufferSize = 8192;
        bufferMask = bufferSize - 1;

        delayBufferL.assign(static_cast<size_t>(bufferSize), 0.0f);
        delayBufferR.assign(static_cast<size_t>(bufferSize), 0.0f);
        writeIndex = 0;
    }

    void setMode(int m) {
        mode = static_cast<Mode>(std::clamp(m, 0, 3));
        switch (mode) {
            case Chorus1:
                lfoRate1 = 0.513f; // Juno Chorus I rate (~0.51 Hz)
                depth1 = 0.0018f;  // 1.8 ms depth
                baseDelay = 0.0035f;
                break;
            case Chorus2:
                lfoRate1 = 0.863f; // Juno Chorus II rate (~0.86 Hz)
                depth1 = 0.0028f;  // 2.8 ms depth
                baseDelay = 0.0035f;
                break;
            case Chorus1And2:
                lfoRate1 = 1.000f; // Roland secret dual-vibrato rate
                depth1 = 0.0035f;  // 3.5 ms depth
                baseDelay = 0.0035f;
                break;
            default:
                break;
        }
    }

    void process(float inL, float inR, float& outL, float& outR) noexcept {
        if (mode == Off || delayBufferL.empty()) {
            outL = inL;
            outR = inR;
            return;
        }

        delayBufferL[static_cast<size_t>(writeIndex)] = inL;
        delayBufferR[static_cast<size_t>(writeIndex)] = inR;

        // Anti-phase stereo LFO
        const float lfo = std::sin(2.0f * 3.14159265f * static_cast<float>(lfoPhase));
        lfoPhase += static_cast<double>(lfoRate1) / sampleRate;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;

        const float delaySecL = baseDelay + depth1 * lfo;
        const float delaySecR = baseDelay + depth1 * (-lfo);

        const float delayedL = readInterpolated(delayBufferL, delaySecL);
        const float delayedR = readInterpolated(delayBufferR, delaySecR);

        outL = 0.707f * (inL + delayedL);
        outR = 0.707f * (inR + delayedR);

        writeIndex = (writeIndex + 1) & bufferMask;
    }

    void processBlock(float* outL, float* outR, int numSamples) noexcept {
        if (mode == Off || delayBufferL.empty() || numSamples <= 0) return;

        for (int i = 0; i < numSamples; ++i) {
            float cL = outL[i];
            float cR = outR[i];
            process(outL[i], outR[i], cL, cR);
            outL[i] = cL;
            outR[i] = cR;
        }
    }

    void reset() noexcept {
        std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0f);
        std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0f);
        writeIndex = 0;
        lfoPhase = 0.0;
    }

private:
    float readInterpolated(const std::vector<float>& buffer, float delaySeconds) const noexcept {
        const float delaySamples = delaySeconds * static_cast<float>(sampleRate);
        float readPos = static_cast<float>(writeIndex) - delaySamples;

        if (readPos < 0.0f) readPos += static_cast<float>(bufferSize);

        const int i0 = static_cast<int>(readPos) & bufferMask;
        const int i1 = (i0 + 1) & bufferMask;
        const float frac = readPos - static_cast<float>(static_cast<int>(readPos));

        return buffer[static_cast<size_t>(i0)] + frac * (buffer[static_cast<size_t>(i1)] - buffer[static_cast<size_t>(i0)]);
    }

    double sampleRate = 44100.0;
    int bufferSize = 8192;
    int bufferMask = 8191;
    int writeIndex = 0;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;

    Mode mode = Chorus1;
    double lfoPhase = 0.0;
    float lfoRate1 = 0.513f;
    float depth1 = 0.0018f;
    float baseDelay = 0.0035f;
};