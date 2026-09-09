#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>

class StereoReverb {
public:
    StereoReverb() = default;

    void prepare(double sampleRate, int samplesPerBlock) {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock > 0 ? samplesPerBlock : 512);
        spec.numChannels = 2;
        reverb.prepare(spec);
    }

    void setParameters(float roomSize, float damping, float mixVal) noexcept {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = std::clamp(roomSize, 0.0f, 1.0f);
        p.damping = std::clamp(damping, 0.0f, 1.0f);

        // JUCE's internal Freeverb implementation multiplies dryLevel by 2.0f (+6 dB)
        // and wetLevel by 3.0f (+9.5 dB). We normalize both scaling factors so that:
        // 1) At 0% mix, dryLevel * 2.0f = 1.000f (exact 0 dB unity gain matching bypass).
        // 2) As mix increases, the dry signal gently eases back while the wet tail blooms in seamlessly.
        const float m = std::clamp(mixVal, 0.0f, 1.0f);
        const float desiredDry = 1.0f - (m * 0.35f);
        const float desiredWet = m * 0.50f;

        p.dryLevel = desiredDry * 0.5f;          // 0.5f * 2.0f = desiredDry (1.00f at 0% mix)
        p.wetLevel = desiredWet * (1.0f / 3.0f); // (1/3) * 3.0f = desiredWet
        p.width = 1.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters(p);
        mix = mixVal;
    }

    // High-performance block-level processing
    void processBlock(float* outL, float* outR, int numSamples) noexcept {
        if (mix <= 0.0001f || numSamples <= 0) return;

        float* channelPointers[2] = { outL, outR };
        juce::dsp::AudioBlock<float> block(channelPointers, 2, static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);
    }

    void reset() noexcept {
        reverb.reset();
    }

private:
    juce::dsp::Reverb reverb;
    float mix = 0.0f;
};