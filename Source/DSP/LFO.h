#pragma once
#include <cmath>
#include <cstdlib>
#include <algorithm>

class LFO {
public:
    enum Waveform { Sine = 0, Triangle, SawUp, SawDown, Square, SampleAndHold };

    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    void setFrequency(float freqHz) {
        frequency = std::clamp(freqHz, 0.01f, 50.0f);
        phaseIncrement = frequency / sampleRate;
    }

    void setWaveform(int shapeIndex) {
        waveform = static_cast<Waveform>(std::clamp(shapeIndex, 0, 5));
    }

    void reset() {
        phase = 0.0;
        lastSampleAndHold = 0.0f;
    }

    float process() {
        float out = 0.0f;

        switch (waveform) {
            case Sine:
                out = std::sin(2.0f * 3.14159265f * static_cast<float>(phase));
                break;

            case Triangle:
                out = (phase < 0.5) ? static_cast<float>(4.0 * phase - 1.0)
                : static_cast<float>(3.0 - 4.0 * phase);
                break;

            case SawUp:
                out = static_cast<float>(2.0 * phase - 1.0);
                break;

            case SawDown:
                out = static_cast<float>(1.0 - 2.0 * phase);
                break;

            case Square:
                out = (phase < 0.5) ? 1.0f : -1.0f;
                break;

            case SampleAndHold:
                if (phase < phaseIncrement) { // Trigger on phase wrap
                    lastSampleAndHold = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
                }
                out = lastSampleAndHold;
                break;
        }

        phase += phaseIncrement;
        if (phase >= 1.0) phase -= 1.0;

        return out;
    }

private:
    double sampleRate = 44100.0;
    float frequency = 1.0f;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    Waveform waveform = Triangle;
    float lastSampleAndHold = 0.0f;
};
