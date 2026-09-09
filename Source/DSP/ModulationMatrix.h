#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_core/juce_core.h>
#endif

#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>

enum class ModSource : uint8_t
{
    None = 0,
    LFO1,
    LFO2,
    LFO3,
    LFO4,
    FilterEnv,
    AmpEnv,
    AuxEnv,
    KeyTracking,
    Velocity,
    ReleaseVelocity,
    ModWheel,
    PitchBend,
    Aftertouch,
    Expression,
    Breath,
    NoiseSampleHold,
    VoiceAlternator,
    SeqTrack,
    Constant,
    Count
};

enum class ModDestination : uint8_t
{
    None = 0,
    MasterPitch,
    OscPitch,           // DCO 1 Pitch
    DCO2Pitch,          // DCO 2 Pitch Offset
    OscWaveformMorph,   // DCO 1 Wave Morph
    DCO2Morph,          // DCO 2 Wave Morph
    OscPWMDetune,       // DCO 1 Pulse Width
    DCO2PWM,            // DCO 2 Pulse Width
    DCO1SawMix,         // DCO 1 Supersaw Mix
    DCO1SawDetune,      // DCO 1 Supersaw Detune
    OscWavefold,        // DCO 1 Wavefold Drive
    DCO2Fold,           // DCO 2 Wavefold Drive
    DCO2Level,          // DCO 2 Mixer Level
    FMAmount,           // Cross-Mod / Audio-Rate FM Depth
    SubOscLevel,        // Phase-Locked Sub-Osc Level
    NoiseLevel,         // White Noise Level
    Filter1Cutoff,      // Roland IR3109 ZDF Cutoff
    Filter1Resonance,   // Roland IR3109 ZDF Resonance
    Filter1HPF,         // Roland High-Pass Filter Slider
    Filter2Cutoff,      // Oberheim SEM SVF Cutoff
    Filter2Resonance,   // Oberheim SEM SVF Resonance
    Filter2Morph,       // Oberheim SEM SVF Mode
    FilterBalance,      // Serial / Parallel Filter Mix Balance
    VCAVolume,          // Voice VCA Gain Level
    Pan,                // Voice Stereo Panning
    LFO1Rate,           // LFO 1 Frequency
    LFO2Rate,           // LFO 2 Frequency
    LFO3Rate,           // LFO 3 Frequency
    LFO4Rate,           // LFO 4 Frequency
    FilterEnvAttack,    // Filter Envelope Attack Time
    FilterEnvDecay,     // Filter Envelope Decay Time
    FilterEnvSustain,   // Filter Envelope Sustain Level
    FilterEnvRelease,   // Filter Envelope Release Time
    AmpEnvAttack,       // Amp Envelope Attack Time
    AmpEnvDecay,        // Amp Envelope Decay Time
    AmpEnvSustain,      // Amp Envelope Sustain Level
    AmpEnvRelease,      // Amp Envelope Release Time
    AuxEnvAttack,       // Aux Envelope Attack Time
    AuxEnvDecay,        // Aux Envelope Decay Time
    AuxEnvSustain,      // Aux Envelope Sustain Level
    AuxEnvRelease,      // Aux Envelope Release Time
    Count
};

struct ModMatrixSlot
{
    ModSource source = ModSource::None;
    ModDestination destination = ModDestination::None;
    float amount = 0.0f;
    ModSource viaSource = ModSource::None;
    float viaAmount = 1.0f;
    bool enabled = false;

    void reset() noexcept
    {
        source = ModSource::None;
        destination = ModDestination::None;
        amount = 0.0f;
        viaSource = ModSource::None;
        viaAmount = 1.0f;
        enabled = false;
    }
};

class ModulationMatrix
{
public:
    static constexpr int NUM_SLOTS = 16;
    static constexpr int NUM_SOURCES = static_cast<int>(ModSource::Count);
    static constexpr int NUM_DESTINATIONS = static_cast<int>(ModDestination::Count);

    using SourceArray = std::array<float, NUM_SOURCES>;
    using DestinationArray = std::array<float, NUM_DESTINATIONS>;

    // Ergonomic compatibility aliases
    static constexpr ModDestination DCO1Pitch     = ModDestination::OscPitch;
    static constexpr ModDestination DCO1Morph     = ModDestination::OscWaveformMorph;
    static constexpr ModDestination DCO1PWM       = ModDestination::OscPWMDetune;
    static constexpr ModDestination DCO1Fold      = ModDestination::OscWavefold;
    static constexpr ModDestination FilterBal     = ModDestination::FilterBalance;
    static constexpr ModDestination CrossMod      = ModDestination::FMAmount;

    ModulationMatrix() noexcept
    {
        reset();
    }

    ~ModulationMatrix() = default;

    void reset() noexcept
    {
        for (auto& slot : slots)
            slot.reset();

        sources.fill(0.0f);
        sources[static_cast<size_t>(ModSource::Constant)] = 1.0f;
        destinationOffsets.fill(0.0f);
    }

    void setSlot(int slotIndex,
                 ModSource src,
                 ModDestination dst,
                 float amt,
                 ModSource via = ModSource::None,
                 float viaAmt = 1.0f,
                 bool isEnabled = false) noexcept
    {
        if (slotIndex >= 0 && slotIndex < NUM_SLOTS)
        {
            slots[static_cast<size_t>(slotIndex)].source = src;
            slots[static_cast<size_t>(slotIndex)].destination = dst;
            slots[static_cast<size_t>(slotIndex)].amount = std::clamp(amt, -1.0f, 1.0f);
            slots[static_cast<size_t>(slotIndex)].viaSource = via;
            slots[static_cast<size_t>(slotIndex)].viaAmount = std::clamp(viaAmt, -1.0f, 1.0f);
            slots[static_cast<size_t>(slotIndex)].enabled = isEnabled;
        }
    }

    void setSlotSource(int slotIndex, ModSource src) noexcept
    {
        if (slotIndex >= 0 && slotIndex < NUM_SLOTS)
            slots[static_cast<size_t>(slotIndex)].source = src;
    }

    void setSlotDestination(int slotIndex, ModDestination dst) noexcept
    {
        if (slotIndex >= 0 && slotIndex < NUM_SLOTS)
            slots[static_cast<size_t>(slotIndex)].destination = dst;
    }

    void setSlotAmount(int slotIndex, float amt) noexcept
    {
        if (slotIndex >= 0 && slotIndex < NUM_SLOTS)
            slots[static_cast<size_t>(slotIndex)].amount = std::clamp(amt, -1.0f, 1.0f);
    }

    void setSlotViaSource(int slotIndex, ModSource via) noexcept
    {
        if (slotIndex >= 0 && slotIndex < NUM_SLOTS)
            slots[static_cast<size_t>(slotIndex)].viaSource = via;
    }

    void setSlotViaAmount(int slotIndex, float viaAmt) noexcept
    {
        if (slotIndex >= 0 && slotIndex < NUM_SLOTS)
            slots[static_cast<size_t>(slotIndex)].viaAmount = std::clamp(viaAmt, -1.0f, 1.0f);
    }

    void setSlotEnabled(int slotIndex, bool isEnabled) noexcept
    {
        if (slotIndex >= 0 && slotIndex < NUM_SLOTS)
            slots[static_cast<size_t>(slotIndex)].enabled = isEnabled;
    }

    const ModMatrixSlot& getSlot(int slotIndex) const noexcept
    {
        jassert(slotIndex >= 0 && slotIndex < NUM_SLOTS);
        return slots[static_cast<size_t>(std::clamp(slotIndex, 0, NUM_SLOTS - 1))];
    }

    ModMatrixSlot& getSlot(int slotIndex) noexcept
    {
        jassert(slotIndex >= 0 && slotIndex < NUM_SLOTS);
        return slots[static_cast<size_t>(std::clamp(slotIndex, 0, NUM_SLOTS - 1))];
    }

    void setSourceValue(ModSource src, float value) noexcept
    {
        const size_t idx = static_cast<size_t>(src);
        if (idx < NUM_SOURCES)
        {
            sources[idx] = value;
        }
        sources[static_cast<size_t>(ModSource::Constant)] = 1.0f;
    }

    float getSourceValue(ModSource src) const noexcept
    {
        const size_t idx = static_cast<size_t>(src);
        if (idx < NUM_SOURCES)
            return sources[idx];
        return 0.0f;
    }

    inline void evaluate() noexcept
    {
        destinationOffsets.fill(0.0f);

        for (int i = 0; i < NUM_SLOTS; ++i)
        {
            const auto& slot = slots[static_cast<size_t>(i)];

            if (!slot.enabled || slot.source == ModSource::None || slot.destination == ModDestination::None)
                continue;

            const float srcVal = sources[static_cast<size_t>(slot.source)];
            float scale = 1.0f;

            if (slot.viaSource != ModSource::None)
            {
                const float viaVal = sources[static_cast<size_t>(slot.viaSource)];
                scale = viaVal * slot.viaAmount;
            }

            const float modOutput = srcVal * slot.amount * scale;
            destinationOffsets[static_cast<size_t>(slot.destination)] += modOutput;
        }
    }

    inline void evaluateVoice(const SourceArray& voiceSources, DestinationArray& voiceDestinations) const noexcept
    {
        voiceDestinations.fill(0.0f);

        for (int i = 0; i < NUM_SLOTS; ++i)
        {
            const auto& slot = slots[static_cast<size_t>(i)];

            if (!slot.enabled || slot.source == ModSource::None || slot.destination == ModDestination::None)
                continue;

            const float srcVal = voiceSources[static_cast<size_t>(slot.source)];
            float scale = 1.0f;

            if (slot.viaSource != ModSource::None)
            {
                const float viaVal = voiceSources[static_cast<size_t>(slot.viaSource)];
                scale = viaVal * slot.viaAmount;
            }

            const float modOutput = srcVal * slot.amount * scale;
            voiceDestinations[static_cast<size_t>(slot.destination)] += modOutput;
        }
    }

    inline float getDestinationOffset(ModDestination dst) const noexcept
    {
        const size_t idx = static_cast<size_t>(dst);
        if (idx < NUM_DESTINATIONS)
            return destinationOffsets[idx];
        return 0.0f;
    }

    static inline float getDestinationOffset(const DestinationArray& dests, ModDestination dst) noexcept
    {
        const size_t idx = static_cast<size_t>(dst);
        if (idx < NUM_DESTINATIONS)
            return dests[idx];
        return 0.0f;
    }

    const DestinationArray& getAllDestinations() const noexcept
    {
        return destinationOffsets;
    }

    const SourceArray& getAllSources() const noexcept
    {
        return sources;
    }

    static juce::String getSourceName(ModSource src) noexcept
    {
        switch (src)
        {
            case ModSource::None:            return "Off";
            case ModSource::LFO1:            return "LFO 1";
            case ModSource::LFO2:            return "LFO 2";
            case ModSource::LFO3:            return "LFO 3";
            case ModSource::LFO4:            return "LFO 4";
            case ModSource::FilterEnv:       return "Filter EG";
            case ModSource::AmpEnv:          return "Amp EG";
            case ModSource::AuxEnv:          return "Aux EG";
            case ModSource::KeyTracking:     return "Key Tracking";
            case ModSource::Velocity:        return "Velocity";
            case ModSource::ReleaseVelocity: return "Rel Velocity";
            case ModSource::ModWheel:        return "Mod Wheel (CC 1)";
            case ModSource::PitchBend:       return "Pitch Bend";
            case ModSource::Aftertouch:      return "Aftertouch";
            case ModSource::Expression:      return "Expression (CC 11)";
            case ModSource::Breath:          return "Breath (CC 2)";
            case ModSource::NoiseSampleHold: return "Noise / S&H";
            case ModSource::VoiceAlternator: return "Voice Alternator";
            case ModSource::SeqTrack:        return "Seq Track";
            case ModSource::Constant:        return "Constant (+1.0)";
            default:                         return "Unknown";
        }
    }

    static juce::String getDestinationName(ModDestination dst) noexcept
    {
        switch (dst)
        {
            case ModDestination::None:             return "Off";
            case ModDestination::MasterPitch:      return "Master Pitch";
            case ModDestination::OscPitch:         return "DCO 1 Pitch";
            case ModDestination::DCO2Pitch:        return "DCO 2 Pitch";
            case ModDestination::OscWaveformMorph: return "DCO 1 Morph";
            case ModDestination::DCO2Morph:        return "DCO 2 Morph";
            case ModDestination::OscPWMDetune:     return "DCO 1 PWM";
            case ModDestination::DCO2PWM:          return "DCO 2 PWM";
            case ModDestination::DCO1SawMix:       return "DCO 1 Saw Mix";
            case ModDestination::DCO1SawDetune:    return "DCO 1 Saw Detune";
            case ModDestination::OscWavefold:      return "DCO 1 Wavefolder";
            case ModDestination::DCO2Fold:         return "DCO 2 Wavefolder";
            case ModDestination::DCO2Level:        return "DCO 2 Level";
            case ModDestination::FMAmount:         return "FM Amount";
            case ModDestination::SubOscLevel:      return "Sub Osc Level";
            case ModDestination::NoiseLevel:       return "Noise Level";
            case ModDestination::Filter1Cutoff:    return "Filter 1 Cutoff";
            case ModDestination::Filter1Resonance: return "Filter 1 Resonance";
            case ModDestination::Filter1HPF:       return "Filter 1 HPF";
            case ModDestination::Filter2Cutoff:    return "Filter 2 Cutoff";
            case ModDestination::Filter2Resonance: return "Filter 2 Resonance";
            case ModDestination::Filter2Morph:     return "Filter 2 Morph";
            case ModDestination::FilterBalance:    return "Filter Balance";
            case ModDestination::VCAVolume:        return "VCA Volume";
            case ModDestination::Pan:              return "Stereo Pan";
            case ModDestination::LFO1Rate:         return "LFO 1 Rate";
            case ModDestination::LFO2Rate:         return "LFO 2 Rate";
            case ModDestination::LFO3Rate:         return "LFO 3 Rate";
            case ModDestination::LFO4Rate:         return "LFO 4 Rate";
            case ModDestination::FilterEnvAttack:  return "Filter EG Attack";
            case ModDestination::FilterEnvDecay:   return "Filter EG Decay";
            case ModDestination::FilterEnvSustain: return "Filter EG Sustain";
            case ModDestination::FilterEnvRelease: return "Filter EG Release";
            case ModDestination::AmpEnvAttack:     return "Amp EG Attack";
            case ModDestination::AmpEnvDecay:      return "Amp EG Decay";
            case ModDestination::AmpEnvSustain:    return "Amp EG Sustain";
            case ModDestination::AmpEnvRelease:    return "Amp EG Release";
            case ModDestination::AuxEnvAttack:     return "Aux EG Attack";
            case ModDestination::AuxEnvDecay:      return "Aux EG Decay";
            case ModDestination::AuxEnvSustain:    return "Aux EG Sustain";
            case ModDestination::AuxEnvRelease:    return "Aux EG Release";
            case ModDestination::Count:
            default:                               return "Unknown";
        }
    }

    static juce::StringArray getSourceChoices() noexcept
    {
        juce::StringArray choices;
        for (int i = 0; i < NUM_SOURCES; ++i)
        {
            choices.add(getSourceName(static_cast<ModSource>(i)));
        }
        return choices;
    }

    static juce::StringArray getDestinationChoices() noexcept
    {
        juce::StringArray choices;
        for (int i = 0; i < NUM_DESTINATIONS; ++i)
        {
            choices.add(getDestinationName(static_cast<ModDestination>(i)));
        }
        return choices;
    }

    static juce::String getSlotParamId(int slotIndex, const juce::String& paramSuffix) noexcept
    {
        return "mod_slot_" + juce::String(slotIndex + 1) + "_" + paramSuffix;
    }

    static void addSlotParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        const auto sourceChoices = getSourceChoices();
        const auto destChoices = getDestinationChoices();

        for (int i = 0; i < NUM_SLOTS; ++i)
        {
            const juce::String slotPrefix = "Slot " + juce::String(i + 1) + " ";

            layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(getSlotParamId(i, "src"), 1),
                slotPrefix + "Source",
                sourceChoices,
                0));

            layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(getSlotParamId(i, "dst"), 1),
                slotPrefix + "Destination",
                destChoices,
                0));

            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(getSlotParamId(i, "amt"), 1),
                slotPrefix + "Amount",
                juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
                0.0f));

            layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(getSlotParamId(i, "via"), 1),
                slotPrefix + "Via Source",
                sourceChoices,
                0));

            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(getSlotParamId(i, "via_amt"), 1),
                slotPrefix + "Via Amount",
                juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
                1.0f));

            layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(getSlotParamId(i, "enable"), 1),
                slotPrefix + "Enable",
                false));
        }
    }

private:
    alignas(16) std::array<ModMatrixSlot, NUM_SLOTS> slots {};
    alignas(16) SourceArray sources {};
    alignas(16) DestinationArray destinationOffsets {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationMatrix)
};