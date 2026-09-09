#pragma once
#include <cmath>
#include <algorithm>

class ADSREnvelope {
public:
    enum class State { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sr) {
        sampleRate = (sr > 1000.0) ? sr : 44100.0;
        updateCoefficients();
    }

    void setParameters(float aSec, float dSec, float sLevel, float rSec) {
        attackTime = std::max(0.001f, aSec);
        decayTime = std::max(0.001f, dSec);
        sustainLevel = std::clamp(sLevel, 0.0f, 1.0f);
        releaseTime = std::max(0.001f, rSec);
        updateCoefficients();
    }

    void noteOn() {
        if (state == State::Idle) {
            currentLevel = 0.0f;
        }
        state = State::Attack;
    }

    void noteOff() {
        if (state != State::Idle) {
            state = State::Release;
        }
    }

    void reset() {
        state = State::Idle;
        currentLevel = 0.0f;
    }

    bool isActive() const {
        return state != State::Idle;
    }

    float getLevel() const {
        return currentLevel;
    }

    float process() {
        switch (state) {
            case State::Idle:
                currentLevel = 0.0f;
                break;

            case State::Attack:
                currentLevel += attackRate;
                if (currentLevel >= 1.0f) {
                    currentLevel = 1.0f;
                    state = State::Decay;
                }
                break;

            case State::Decay:
                currentLevel = sustainLevel + (currentLevel - sustainLevel) * decayCoeff;
                if (std::abs(currentLevel - sustainLevel) < 0.0005f) {
                    currentLevel = sustainLevel;
                    state = State::Sustain;
                }
                break;

            case State::Sustain:
                currentLevel = sustainLevel;
                break;

            case State::Release:
                currentLevel *= releaseCoeff;
                // Below 16-bit noise floor (-86 dB): seamlessly transitions to Idle without any audible click
                if (currentLevel < 0.00005f) {
                    currentLevel = 0.0f;
                    state = State::Idle;
                }
                break;
        }

        // Anti-denormal protection
        if (std::abs(currentLevel) < 1.0e-7f) {
            currentLevel = 0.0f;
            if (state == State::Release) {
                state = State::Idle;
            }
        }

        return currentLevel;
    }

private:
    void updateCoefficients() {
        float srFloat = static_cast<float>(sampleRate);
        attackRate = static_cast<float>(1.0 / (static_cast<double>(attackTime) * sampleRate));

        // True T60 exponential decay time-constants: reaches silence at exactly the specified time
        decayCoeff = std::exp(-5.0f / (decayTime * srFloat));
        releaseCoeff = std::exp(-6.0f / (releaseTime * srFloat));
    }

    double sampleRate = 44100.0;
    State state = State::Idle;
    float attackTime = 0.01f;
    float decayTime = 0.3f;
    float sustainLevel = 0.7f;
    float releaseTime = 0.5f;

    float attackRate = 0.0f;
    float decayCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float currentLevel = 0.0f;
};