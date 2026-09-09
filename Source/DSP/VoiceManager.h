#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_core/juce_core.h>
#endif

#include "SynthVoice.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace VoiceTraits
{
    template <typename T, typename = void> struct has_isRelease : std::false_type {};
    template <typename T> struct has_isRelease<T, std::void_t<decltype(std::declval<T>().isRelease())>> : std::true_type {};

    template <typename T, typename = void> struct has_isReleasing : std::false_type {};
    template <typename T> struct has_isReleasing<T, std::void_t<decltype(std::declval<T>().isReleasing())>> : std::true_type {};

    template <typename Voice>
    inline bool checkRelease(const Voice& v) noexcept
    {
        if constexpr (has_isRelease<Voice>::value)
            return v.isRelease();
        else if constexpr (has_isReleasing<Voice>::value)
            return v.isReleasing();
        else
            return false;
    }

    template <typename T, typename = void> struct has_getEnvelopeLevel : std::false_type {};
    template <typename T> struct has_getEnvelopeLevel<T, std::void_t<decltype(std::declval<T>().getEnvelopeLevel())>> : std::true_type {};

    template <typename T, typename = void> struct has_getEnvLevel : std::false_type {};
    template <typename T> struct has_getEnvLevel<T, std::void_t<decltype(std::declval<T>().getEnvLevel())>> : std::true_type {};

    template <typename Voice>
    inline float checkEnvLevel(const Voice& v) noexcept
    {
        if constexpr (has_getEnvelopeLevel<Voice>::value)
            return v.getEnvelopeLevel();
        else if constexpr (has_getEnvLevel<Voice>::value)
            return v.getEnvLevel();
        else
            return 0.0f;
    }

    template <typename T, typename = void> struct has_updatePitchLegato : std::false_type {};
    template <typename T> struct has_updatePitchLegato<T, std::void_t<decltype(std::declval<T>().updatePitchLegato(0, 0.0f))>> : std::true_type {};

    template <typename T, typename = void> struct has_setGlideTarget : std::false_type {};
    template <typename T> struct has_setGlideTarget<T, std::void_t<decltype(std::declval<T>().setGlideTarget(0))>> : std::true_type {};

    template <typename Voice>
    inline void applyPitchLegato(Voice& v, int note, float vel) noexcept
    {
        if constexpr (has_updatePitchLegato<Voice>::value)
            v.updatePitchLegato(note, vel);
        else if constexpr (has_setGlideTarget<Voice>::value)
            v.setGlideTarget(note);
    }

    template <typename Voice>
    inline void setGlideTarget(Voice& v, int note) noexcept
    {
        applyPitchLegato(v, note, -1.0f);
    }

    template <typename T, typename = void> struct has_noteOn3 : std::false_type {};
    template <typename T> struct has_noteOn3<T, std::void_t<decltype(std::declval<T>().noteOn(0, 0.0f, true))>> : std::true_type {};

    template <typename Voice>
    inline void noteOnVoice(Voice& v, int note, float vel, bool isLegato) noexcept
    {
        if constexpr (has_noteOn3<Voice>::value)
            v.noteOn(note, vel, isLegato);
        else
            v.noteOn(note, vel);
    }

    template <typename T, typename = void> struct has_setPortamentoTime : std::false_type {};
    template <typename T> struct has_setPortamentoTime<T, std::void_t<decltype(std::declval<T>().setPortamentoTime(0.0f))>> : std::true_type {};

    template <typename T, typename = void> struct has_setGlideTime : std::false_type {};
    template <typename T> struct has_setGlideTime<T, std::void_t<decltype(std::declval<T>().setGlideTime(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void applyPortamentoTime(Voice& v, float timeSec) noexcept
    {
        if constexpr (has_setPortamentoTime<Voice>::value)
            v.setPortamentoTime(timeSec);
        else if constexpr (has_setGlideTime<Voice>::value)
            v.setGlideTime(timeSec);
    }

    template <typename T, typename = void> struct has_setPortamentoEnabled : std::false_type {};
    template <typename T> struct has_setPortamentoEnabled<T, std::void_t<decltype(std::declval<T>().setPortamentoEnabled(true))>> : std::true_type {};

    template <typename Voice>
    inline void applyPortamentoEnabled(Voice& v, bool enabled) noexcept
    {
        if constexpr (has_setPortamentoEnabled<Voice>::value)
            v.setPortamentoEnabled(enabled);
    }

    template <typename T, typename = void> struct has_setDetune : std::false_type {};
    template <typename T> struct has_setDetune<T, std::void_t<decltype(std::declval<T>().setDetune(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void applyDetune(Voice& v, float detuneCents) noexcept
    {
        if constexpr (has_setDetune<Voice>::value)
            v.setDetune(detuneCents);
    }

    template <typename T, typename = void> struct has_setPan : std::false_type {};
    template <typename T> struct has_setPan<T, std::void_t<decltype(std::declval<T>().setPan(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void applyPan(Voice& v, float pan) noexcept
    {
        if constexpr (has_setPan<Voice>::value)
            v.setPan(pan);
    }

    template <typename T, typename = void> struct has_setPitchBend : std::false_type {};
    template <typename T> struct has_setPitchBend<T, std::void_t<decltype(std::declval<T>().setPitchBend(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void applyPitchBend(Voice& v, float bend) noexcept
    {
        if constexpr (has_setPitchBend<Voice>::value)
            v.setPitchBend(bend);
    }

    template <typename T, typename = void> struct has_setModWheel : std::false_type {};
    template <typename T> struct has_setModWheel<T, std::void_t<decltype(std::declval<T>().setModWheel(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void applyModWheel(Voice& v, float mw) noexcept
    {
        if constexpr (has_setModWheel<Voice>::value)
            v.setModWheel(mw);
    }

    template <typename T, typename = void> struct has_setAftertouch : std::false_type {};
    template <typename T> struct has_setAftertouch<T, std::void_t<decltype(std::declval<T>().setAftertouch(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void applyAftertouch(Voice& v, float at) noexcept
    {
        if constexpr (has_setAftertouch<Voice>::value)
            v.setAftertouch(at);
    }

    template <typename T, typename = void> struct has_setF2KeyTrack : std::false_type {};
    template <typename T> struct has_setF2KeyTrack<T, std::void_t<decltype(std::declval<T>().setF2KeyTrack(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void applyF2KeyTrack(Voice& v, float keyTrack) noexcept
    {
        if constexpr (has_setF2KeyTrack<Voice>::value)
            v.setF2KeyTrack(keyTrack);
    }

    template <typename T, typename = void> struct has_setCutoff : std::false_type {};
    template <typename T> struct has_setCutoff<T, std::void_t<decltype(std::declval<T>().setCutoff(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void applyCutoff(Voice& v, float hz) noexcept
    {
        if constexpr (has_setCutoff<Voice>::value)
            v.setCutoff(hz);
    }

    template <typename T, typename = void> struct has_setResonance : std::false_type {};
    template <typename T> struct has_setResonance<T, std::void_t<decltype(std::declval<T>().setResonance(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void applyResonance(Voice& v, float res) noexcept
    {
        if constexpr (has_setResonance<Voice>::value)
            v.setResonance(res);
    }

    template <typename T, typename = void> struct has_setVoiceIndex : std::false_type {};
    template <typename T> struct has_setVoiceIndex<T, std::void_t<decltype(std::declval<T>().setVoiceIndex(0))>> : std::true_type {};

    template <typename Voice>
    inline void applyVoiceIndex(Voice& v, int index) noexcept
    {
        if constexpr (has_setVoiceIndex<Voice>::value)
            v.setVoiceIndex(index);
    }

    template <typename T, typename = void> struct has_prepare2 : std::false_type {};
    template <typename T> struct has_prepare2<T, std::void_t<decltype(std::declval<T>().prepare(0.0, 0))>> : std::true_type {};

    template <typename T, typename = void> struct has_prepare1 : std::false_type {};
    template <typename T> struct has_prepare1<T, std::void_t<decltype(std::declval<T>().prepare(0.0))>> : std::true_type {};

    template <typename T, typename = void> struct has_setSampleRate : std::false_type {};
    template <typename T> struct has_setSampleRate<T, std::void_t<decltype(std::declval<T>().setSampleRate(0.0))>> : std::true_type {};

    template <typename Voice>
    inline void prepareVoice(Voice& v, double sampleRate, int blockSize) noexcept
    {
        if constexpr (has_prepare2<Voice>::value)
            v.prepare(sampleRate, blockSize);
        else if constexpr (has_prepare1<Voice>::value)
            v.prepare(sampleRate);
        else if constexpr (has_setSampleRate<Voice>::value)
            v.setSampleRate(sampleRate);
    }

    template <typename T, typename = void> struct has_noteOff2 : std::false_type {};
    template <typename T> struct has_noteOff2<T, std::void_t<decltype(std::declval<T>().noteOff(0.0f, true))>> : std::true_type {};

    template <typename T, typename = void> struct has_noteOff1 : std::false_type {};
    template <typename T> struct has_noteOff1<T, std::void_t<decltype(std::declval<T>().noteOff(0.0f))>> : std::true_type {};

    template <typename Voice>
    inline void noteOffVoice(Voice& v, float velocity, bool allowTail) noexcept
    {
        if constexpr (has_noteOff2<Voice>::value)
            v.noteOff(velocity, allowTail);
        else if constexpr (has_noteOff1<Voice>::value)
            v.noteOff(velocity);
    }

    template <typename T, typename = void> struct has_prepareForSteal : std::false_type {};
    template <typename T> struct has_prepareForSteal<T, std::void_t<decltype(std::declval<T>().prepareForSteal())>> : std::true_type {};

    template <typename Voice>
    inline void prepareForSteal(Voice& v) noexcept
    {
        if constexpr (has_prepareForSteal<Voice>::value)
            v.prepareForSteal();
    }
}

// =============================================================================
// Comprehensive Voice Modes: Bi-Timbral Split, True Poly 16 & Mono Modes
// =============================================================================
enum class VoiceMode
{
    PolyBiTimbral = 0,  // Choice 0: "Poly 8 Bi-Timbral" (Default, 8 Part A + 8 Part B)
    Poly16,             // Choice 1: "Poly 16 [Single]" (Unified 16-voice pool for Track A)
    RoundRobin,         // Choice 2: "Round Robin"
    MonoLegato,         // Choice 3: "Mono Legato" (Single voice, glides without retrigger)
    MonoStandard,       // Choice 4: "Mono Standard" (Single voice, retriggers attack)
    Unison2,            // Choice 5: "Unison 2x"
    Unison4,            // Choice 6: "Unison 4x"
    Unison8,            // Choice 7: "Unison 8x"
    Unison16,           // Choice 8: "Unison 16x"
    Count
};
using PlayMode = VoiceMode;

class VoiceManager
{
public:
    static constexpr int NUM_VOICES = 16;
    static constexpr int MAX_VOICES = 16;
    static constexpr int NUM_PARTS  = 2;
    static constexpr int VOICES_PER_PART = 8; // 8 Lower (Part A) + 8 Upper (Part B)

    // Calibrated Gain Compensation (Preserves full output punch across modes)
    static constexpr float GAIN_COMP_POLY         = 0.72f;
    static constexpr float GAIN_COMP_MONO         = 0.95f;
    static constexpr float GAIN_COMP_UNISON2      = 0.707f;
    static constexpr float GAIN_COMP_UNISON4      = 0.500f;
    static constexpr float GAIN_COMP_UNISON8      = 0.400f;
    static constexpr float GAIN_COMP_UNISON16     = 0.320f;

    static constexpr std::array<float, 2> UNISON2_DETUNE = { -1.000f, 1.000f };
    static constexpr std::array<float, 2> UNISON2_PAN    = { -1.000f, 1.000f };

    static constexpr std::array<float, 4> UNISON4_DETUNE = { -1.000f, -0.350f, 0.350f, 1.000f };
    static constexpr std::array<float, 4> UNISON4_PAN    = { -1.000f, -0.450f, 0.450f, 1.000f };

    static constexpr std::array<float, 8> UNISON8_DETUNE = {
        -1.000f, -0.680f, -0.420f, -0.150f, 0.150f, 0.420f, 0.680f, 1.000f
    };
    static constexpr std::array<float, 8> UNISON8_PAN = {
        -1.000f, -0.714f, -0.429f, -0.143f, 0.143f, 0.429f, 0.714f, 1.000f
    };

    static constexpr std::array<float, 16> UNISON16_DETUNE = {
        -1.000f, -0.850f, -0.710f, -0.580f, -0.440f, -0.310f, -0.180f, -0.060f,
         0.060f,  0.180f,  0.310f,  0.440f,  0.580f,  0.710f,  0.850f,  1.000f
    };
    static constexpr std::array<float, 16> UNISON16_PAN = {
        -1.000f, -0.867f, -0.733f, -0.600f, -0.467f, -0.333f, -0.200f, -0.067f,
         0.067f,  0.200f,  0.333f,  0.467f,  0.600f,  0.733f,  0.867f,  1.000f
    };

    struct NoteEntry
    {
        int noteNumber = -1;
        float velocity = 0.0f;
    };
    static constexpr int MAX_NOTE_STACK = 32;

    VoiceManager() noexcept
    {
        voiceNote.fill(-1);
        voiceVelocity.fill(0.0f);
        voiceAge.fill(0);
        voiceSustained.fill(false);
        voicePan.fill(0.0f);
        voiceDetune.fill(0.0f);
        voicePart.fill(0);
        partF2KeyTrack.fill(0.0f);

        for (int i = 0; i < NUM_VOICES; ++i)
        {
            voicePart[static_cast<size_t>(i)] = (i < VOICES_PER_PART) ? 0 : 1;
            VoiceTraits::applyVoiceIndex(voices[static_cast<size_t>(i)], i);
        }

        for (int p = 0; p < NUM_PARTS; ++p)
        {
            physicalKeysDown[static_cast<size_t>(p)].fill(false);
            monoNoteStack[static_cast<size_t>(p)].fill(NoteEntry{ -1, 0.0f });
            monoNoteCount[p] = 0;
            roundRobinIndex[p] = p * VOICES_PER_PART;
        }

        applyModeConfiguration();
    }

    ~VoiceManager() = default;

    void prepare(double sampleRate, int maxSamplesPerBlock) noexcept
    {
        currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        currentBlockSize = maxSamplesPerBlock;

        for (int i = 0; i < NUM_VOICES; ++i)
        {
            VoiceTraits::applyVoiceIndex(voices[static_cast<size_t>(i)], i);
            VoiceTraits::prepareVoice(voices[static_cast<size_t>(i)], currentSampleRate, maxSamplesPerBlock);
        }

        reset();
    }

    void init(double sampleRate) noexcept
    {
        prepare(sampleRate, 512);
    }

    void reset() noexcept
    {
        for (int i = 0; i < NUM_VOICES; ++i)
        {
            voices[static_cast<size_t>(i)].reset();
            VoiceTraits::applyVoiceIndex(voices[static_cast<size_t>(i)], i);
            voiceNote[static_cast<size_t>(i)] = -1;
            voiceVelocity[static_cast<size_t>(i)] = 0.0f;
            voiceAge[static_cast<size_t>(i)] = 0;
            voiceSustained[static_cast<size_t>(i)] = false;
            voicePan[static_cast<size_t>(i)] = 0.0f;
            voiceDetune[static_cast<size_t>(i)] = 0.0f;
            voicePart[static_cast<size_t>(i)] = (i < VOICES_PER_PART) ? 0 : 1;
        }

        for (int p = 0; p < NUM_PARTS; ++p)
        {
            physicalKeysDown[static_cast<size_t>(p)].fill(false);
            monoNoteStack[static_cast<size_t>(p)].fill(NoteEntry{ -1, 0.0f });
            monoNoteCount[p] = 0;
            roundRobinIndex[p] = p * VOICES_PER_PART;
        }

        globalAgeCounter = 0;
        sustainPedalDown = false;
        applyModeConfiguration();
    }

    std::array<SynthVoice, NUM_VOICES>& getVoices() noexcept { return voices; }
    const std::array<SynthVoice, NUM_VOICES>& getVoices() const noexcept { return voices; }

    void setVoiceMode(VoiceMode mode) noexcept
    {
        currentMode = mode;
        applyModeConfiguration();
    }

    void setVoiceMode(int modeIndex) noexcept
    {
        currentMode = static_cast<VoiceMode>(std::clamp(modeIndex, 0, static_cast<int>(VoiceMode::Count) - 1));
        applyModeConfiguration();
    }

    void setPlayMode(PlayMode mode) noexcept { setVoiceMode(mode); }
    void setPlayMode(int modeIndex) noexcept { setVoiceMode(modeIndex); }

    void applyModeConfiguration() noexcept
    {
        switch (currentMode)
        {
            case VoiceMode::PolyBiTimbral:
                biTimbralSplit = true;
                truePoly16 = false;
                break;
            case VoiceMode::Poly16:
                biTimbralSplit = false;
                truePoly16 = true;
                break;
            case VoiceMode::MonoLegato:
                legatoMode = true;
                break;
            case VoiceMode::MonoStandard:
                legatoMode = false;
                break;
            default:
                break;
        }

        // Clean up any extra sustained/sounding voices when entering monophonic modes
        if (isMonoMode())
        {
            for (int p = 0; p < NUM_PARTS; ++p)
            {
                const int startOther = p * VOICES_PER_PART + 1;
                const int endOther   = (p + 1) * VOICES_PER_PART;
                for (int v = startOther; v < endOther; ++v)
                {
                    VoiceTraits::noteOffVoice(voices[static_cast<size_t>(v)], 0.0f, false);
                    voiceSustained[static_cast<size_t>(v)] = false;
                    voiceNote[static_cast<size_t>(v)] = -1;
                }
            }
        }
    }

    void setCycleMode(bool cycleOn) noexcept { cycleMode = cycleOn; }

    void setChordMode(bool enabled, int chordIndex = 0) noexcept
    {
        chordMode = enabled;
        currentChordIndex = chordIndex;
    }

    std::vector<int> getChordIntervals(int rootNote) const
    {
        if (!chordMode) return { rootNote };

        switch (currentChordIndex)
        {
            case 0: return { rootNote, rootNote + 4, rootNote + 7 };                   // Major
            case 1: return { rootNote, rootNote + 3, rootNote + 7 };                   // Minor
            case 2: return { rootNote, rootNote + 4, rootNote + 7, rootNote + 11 };    // Maj7
            case 3: return { rootNote, rootNote + 3, rootNote + 7, rootNote + 10 };    // Min7
            case 4: return { rootNote, rootNote + 4, rootNote + 7, rootNote + 10 };    // Dom7
            case 5: return { rootNote, rootNote + 5, rootNote + 7 };                   // Sus4
            case 6: return { rootNote, rootNote + 3, rootNote + 6 };                   // Dim
            case 7: return { rootNote, rootNote + 12 };                                // Octave
            case 8: return { rootNote, rootNote + 7 };                                 // Power (5th)
            default: return { rootNote };
        }
    }

    VoiceMode getVoiceMode() const noexcept { return currentMode; }
    PlayMode getPlayMode() const noexcept   { return currentMode; }

    inline bool isMonoMode() const noexcept
    {
        return (currentMode == VoiceMode::MonoLegato ||
                currentMode == VoiceMode::MonoStandard ||
                (getUnisonStackSize() > 1 && unisonMono));
    }

    inline bool isLegatoActive() const noexcept
    {
        if (currentMode == VoiceMode::MonoStandard)
            return false;
        if (currentMode == VoiceMode::MonoLegato)
            return true;
        return legatoMode;
    }

    void setTruePoly16(bool enabled) noexcept
    {
        truePoly16 = enabled;
        if (enabled)
            biTimbralSplit = false;
    }
    bool isTruePoly16() const noexcept { return (currentMode == VoiceMode::Poly16 && truePoly16 && !biTimbralSplit); }

    void setBiTimbral(bool enabled) noexcept
    {
        biTimbralSplit = enabled;
        if (enabled)
            truePoly16 = false;
    }
    bool isBiTimbral() const noexcept { return biTimbralSplit; }

    void setBiTimbralSplit(bool enabled) noexcept { setBiTimbral(enabled); }
    bool isBiTimbralSplit() const noexcept { return isBiTimbral(); }

    inline bool isTruePoly16Active(int part) const noexcept
    {
        return (part == 0 && currentMode == VoiceMode::Poly16 && truePoly16 && !biTimbralSplit);
    }

    void setUnisonDetune(float detuneCents) noexcept
    {
        unisonDetuneCents = std::max(0.0f, detuneCents);
        updateActiveVoiceUnisonParameters();
    }
    float getUnisonDetune() const noexcept { return unisonDetuneCents; }

    void setUnisonSpread(float spread) noexcept
    {
        unisonSpread = std::clamp(spread, 0.0f, 1.0f);
        updateActiveVoiceUnisonParameters();
    }
    float getUnisonSpread() const noexcept { return unisonSpread; }

    void setUnisonMono(bool isMono) noexcept { unisonMono = isMono; }
    bool isUnisonMono() const noexcept       { return unisonMono; }

    void setGlideTime(float seconds, int part = -1) noexcept
    {
        glideTimeSeconds = std::max(0.0005f, seconds);
        const int startV = (part == 1) ? VOICES_PER_PART : 0;
        const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

        for (int v = startV; v < endV; ++v)
            VoiceTraits::applyPortamentoTime(voices[static_cast<size_t>(v)], glideTimeSeconds);
    }
    void setPortamentoTime(float seconds, int part = -1) noexcept { setGlideTime(seconds, part); }
    float getGlideTime() const noexcept            { return glideTimeSeconds; }
    float getPortamentoTime() const noexcept       { return glideTimeSeconds; }

    void setGlideEnabled(bool enabled, int part = -1) noexcept
    {
        glideEnabled = enabled;
        const int startV = (part == 1) ? VOICES_PER_PART : 0;
        const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

        for (int v = startV; v < endV; ++v)
            VoiceTraits::applyPortamentoEnabled(voices[static_cast<size_t>(v)], enabled);
    }
    void setPortamentoEnabled(bool enabled, int part = -1) noexcept { setGlideEnabled(enabled, part); }
    bool isGlideEnabled() const noexcept             { return glideEnabled; }
    bool isPortamentoEnabled() const noexcept        { return glideEnabled; }

    void setLegatoMode(bool legato) noexcept         { legatoMode = legato; }
    bool isLegatoMode() const noexcept               { return legatoMode; }

    void setPitchBendRange(int semitones) noexcept   { pitchBendRange = std::clamp(semitones, 1, 48); }
    int getPitchBendRange() const noexcept           { return pitchBendRange; }

    void setPitchBend(float normalizedBend, int part = -1) noexcept
    {
        currentPitchBend = std::clamp(normalizedBend, -1.0f, 1.0f);
        const int startV = (part == 1) ? VOICES_PER_PART : 0;
        const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

        for (int v = startV; v < endV; ++v)
            VoiceTraits::applyPitchBend(voices[static_cast<size_t>(v)], currentPitchBend);
    }
    float getPitchBend() const noexcept { return currentPitchBend; }

    void setModWheel(float normalizedWheel, int part = -1) noexcept
    {
        currentModWheel = std::clamp(normalizedWheel, 0.0f, 1.0f);
        const int startV = (part == 1) ? VOICES_PER_PART : 0;
        const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

        for (int v = startV; v < endV; ++v)
            VoiceTraits::applyModWheel(voices[static_cast<size_t>(v)], currentModWheel);
    }
    float getModWheel() const noexcept { return currentModWheel; }

    void setAftertouch(float normalizedAftertouch, int part = -1) noexcept
    {
        currentAftertouch = std::clamp(normalizedAftertouch, 0.0f, 1.0f);
        const int startV = (part == 1) ? VOICES_PER_PART : 0;
        const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

        for (int v = startV; v < endV; ++v)
            VoiceTraits::applyAftertouch(voices[static_cast<size_t>(v)], currentAftertouch);
    }
    float getAftertouch() const noexcept { return currentAftertouch; }

    void setF2KeyTrack(float keyTrack, int part = -1) noexcept
    {
        if (part >= 0 && part < NUM_PARTS)
            partF2KeyTrack[static_cast<size_t>(part)] = keyTrack;
        else
            partF2KeyTrack.fill(keyTrack);

        const int startV = (part == 1) ? VOICES_PER_PART : 0;
        const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

        for (int v = startV; v < endV; ++v)
            VoiceTraits::applyF2KeyTrack(voices[static_cast<size_t>(v)], keyTrack);
    }
    void setFilter2KeyTrack(float keyTrack, int part = -1) noexcept { setF2KeyTrack(keyTrack, part); }
    float getF2KeyTrack(int part = 0) const noexcept
    {
        const size_t p = static_cast<size_t>(std::clamp(part, 0, NUM_PARTS - 1));
        return partF2KeyTrack[p];
    }

    void setCutoff(float hz, int part = -1) noexcept
    {
        const int startV = (part == 1) ? VOICES_PER_PART : 0;
        const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

        for (int v = startV; v < endV; ++v)
            VoiceTraits::applyCutoff(voices[static_cast<size_t>(v)], hz);
    }

    void setResonance(float res, int part = -1) noexcept
    {
        const int startV = (part == 1) ? VOICES_PER_PART : 0;
        const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

        for (int v = startV; v < endV; ++v)
            VoiceTraits::applyResonance(voices[static_cast<size_t>(v)], res);
    }

    void setSustainPedal(bool pedalDown, int part = -1) noexcept
    {
        if (sustainPedalDown == pedalDown)
            return;

        sustainPedalDown = pedalDown;

        if (!sustainPedalDown)
        {
            const int startV = (part == 1) ? VOICES_PER_PART : 0;
            const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

            for (int i = startV; i < endV; ++i)
            {
                const size_t idx = static_cast<size_t>(i);
                if (voiceSustained[idx])
                {
                    voiceSustained[idx] = false;
                    const int note = voiceNote[idx];
                    const int p = voicePart[idx];
                    if (note < 0 || note >= 128 || !physicalKeysDown[static_cast<size_t>(p)][static_cast<size_t>(note)])
                    {
                        VoiceTraits::noteOffVoice(voices[idx], 0.0f, true);
                    }
                }
            }
        }
    }

    bool isSustainPedalDown() const noexcept { return sustainPedalDown; }

    float getGainCompensation() const noexcept
    {
        switch (currentMode)
        {
            case VoiceMode::PolyBiTimbral: return GAIN_COMP_POLY;
            case VoiceMode::Poly16:        return GAIN_COMP_POLY;
            case VoiceMode::RoundRobin:    return GAIN_COMP_POLY;
            case VoiceMode::MonoStandard:  return GAIN_COMP_MONO;
            case VoiceMode::MonoLegato:    return GAIN_COMP_MONO;
            case VoiceMode::Unison2:       return GAIN_COMP_UNISON2;
            case VoiceMode::Unison4:       return GAIN_COMP_UNISON4;
            case VoiceMode::Unison8:       return GAIN_COMP_UNISON8;
            case VoiceMode::Unison16:      return GAIN_COMP_UNISON16;
            default:                       return GAIN_COMP_POLY;
        }
    }

    void setApplyGainCompensation(bool shouldApply) noexcept { applyGainCompensation = shouldApply; }
    bool isApplyingGainCompensation() const noexcept         { return applyGainCompensation; }

    // =========================================================================
    // Bi-Timbral / True Poly 16 Note Triggering
    // Part 0 = Lower / Track A (Voices 0..7, or 0..15 in True Poly 16)
    // Part 1 = Upper / Track B (Voices 8..15 in bi-timbral split)
    // =========================================================================
    void noteOn(int noteNumber, float velocity, int part = 0) noexcept
    {
        if (noteNumber < 0 || noteNumber > 127) return;
        const int p = std::clamp(part, 0, NUM_PARTS - 1);

        if (physicalKeysDown[static_cast<size_t>(p)][static_cast<size_t>(noteNumber)])
            return;

        const float safeVelocity = (velocity > 0.001f) ? std::clamp(velocity, 0.0f, 1.0f) : 0.8f;
        physicalKeysDown[static_cast<size_t>(p)][static_cast<size_t>(noteNumber)] = true;

        if (isMonoMode())
        {
            const int stackSize = getUnisonStackSize();
            if (stackSize > 1)
                handleMonoUnisonNoteOn(noteNumber, safeVelocity, stackSize, p);
            else
                handleMonoNoteOn(noteNumber, safeVelocity, p);
            return;
        }

        if (currentMode == VoiceMode::RoundRobin || cycleMode)
        {
            handleRoundRobinNoteOn(noteNumber, safeVelocity, p);
            return;
        }

        const int stackSize = getUnisonStackSize();
        handleGroupNoteOn(noteNumber, safeVelocity, stackSize, p);
    }

    void handleNoteOn(int noteNumber, float velocity, bool isPoly = true, bool glide = false) noexcept
    {
        juce::ignoreUnused(isPoly, glide);
        noteOn(noteNumber, velocity, 0);
    }

    void noteOff(int noteNumber, float velocity = 0.0f, int part = -1) noexcept
    {
        if (noteNumber < 0 || noteNumber > 127) return;

        const int startP = (part == 1) ? 1 : 0;
        const int endP   = (part == 0) ? 1 : NUM_PARTS;

        for (int p = startP; p < endP; ++p)
        {
            physicalKeysDown[static_cast<size_t>(p)][static_cast<size_t>(noteNumber)] = false;

            if (isMonoMode())
            {
                const int stackSize = getUnisonStackSize();
                if (stackSize > 1)
                    handleMonoUnisonNoteOff(noteNumber, velocity, stackSize, p);
                else
                    handleMonoNoteOff(noteNumber, velocity, p);
                continue;
            }

            const int stackSize = getUnisonStackSize();
            const int numGroups = isTruePoly16Active(p) ? (NUM_VOICES / stackSize) : (VOICES_PER_PART / stackSize);

            for (int g = 0; g < numGroups; ++g)
            {
                const int baseVoice = getVoiceGroupBase(g, stackSize, p);
                if (voiceNote[static_cast<size_t>(baseVoice)] == noteNumber && isGroupActive(g, stackSize, p))
                {
                    if (sustainPedalDown)
                    {
                        for (int v = 0; v < stackSize; ++v)
                            voiceSustained[static_cast<size_t>(baseVoice + v)] = true;
                    }
                    else
                    {
                        for (int v = 0; v < stackSize; ++v)
                        {
                            const size_t vIdx = static_cast<size_t>(baseVoice + v);
                            VoiceTraits::noteOffVoice(voices[vIdx], velocity, true);
                            voiceSustained[vIdx] = false;
                        }
                    }
                }
            }
        }
    }

    void handleNoteOff(int noteNumber) noexcept
    {
        noteOff(noteNumber, 0.0f, -1);
    }

    void allNotesOff(int part = -1) noexcept
    {
        const int startV = (part == 1) ? VOICES_PER_PART : 0;
        const int endV   = (part == 0) ? VOICES_PER_PART : NUM_VOICES;

        for (int i = startV; i < endV; ++i)
        {
            const size_t idx = static_cast<size_t>(i);
            VoiceTraits::noteOffVoice(voices[idx], 0.0f, true);
            voiceSustained[idx] = false;
            voiceNote[idx] = -1;
        }

        const int startP = (part == 1) ? 1 : 0;
        const int endP   = (part == 0) ? 1 : NUM_PARTS;

        for (int p = startP; p < endP; ++p)
        {
            physicalKeysDown[static_cast<size_t>(p)].fill(false);
            monoNoteStack[static_cast<size_t>(p)].fill(NoteEntry{ -1, 0.0f });
            monoNoteCount[p] = 0;
        }
    }

    int getNumVoices() const noexcept { return NUM_VOICES; }

    int getActiveVoiceCount() const noexcept
    {
        int count = 0;
        for (int i = 0; i < NUM_VOICES; ++i)
        {
            if (voices[static_cast<size_t>(i)].isActive())
                ++count;
        }
        return count;
    }

    bool isVoiceActive(int voiceIndex) const noexcept
    {
        if (voiceIndex >= 0 && voiceIndex < NUM_VOICES)
            return voices[static_cast<size_t>(voiceIndex)].isActive();
        return false;
    }

    int getVoiceNote(int voiceIndex) const noexcept
    {
        if (voiceIndex >= 0 && voiceIndex < NUM_VOICES)
            return voiceNote[static_cast<size_t>(voiceIndex)];
        return -1;
    }

    int getVoicePart(int voiceIndex) const noexcept
    {
        if (voiceIndex >= 0 && voiceIndex < NUM_VOICES)
        {
            if (isTruePoly16Active(0))
                return 0; // True Poly 16 assigns entire 16-voice pool to Track A
            return voicePart[static_cast<size_t>(voiceIndex)];
        }
        return 0;
    }

    float getVoiceEnvLevel(int voiceIndex) const noexcept
    {
        if (voiceIndex >= 0 && voiceIndex < NUM_VOICES)
            return VoiceTraits::checkEnvLevel(voices[static_cast<size_t>(voiceIndex)]);
        return 0.0f;
    }

    SynthVoice& getVoice(int index) noexcept
    {
        jassert(index >= 0 && index < NUM_VOICES);
        return voices[static_cast<size_t>(index)];
    }

    const SynthVoice& getVoice(int index) const noexcept
    {
        jassert(index >= 0 && index < NUM_VOICES);
        return voices[static_cast<size_t>(index)];
    }

    void handleMonoLegatoNoteOn(int noteNumber, float velocity, int part) noexcept
    {
        handleMonoNoteOn(noteNumber, velocity, part);
    }

    void handleMonoLegatoNoteOff(int noteNumber, float velocity, int part) noexcept
    {
        handleMonoNoteOff(noteNumber, velocity, part);
    }

private:
    inline int getVoiceGroupBase(int groupIndex, int stackSize, int part) const noexcept
    {
        if (isTruePoly16Active(part))
            return groupIndex * stackSize;
        return (part * VOICES_PER_PART) + (groupIndex * stackSize);
    }

    int getUnisonStackSize() const noexcept
    {
        switch (currentMode)
        {
            case VoiceMode::Unison2:  return 2;
            case VoiceMode::Unison4:  return 4;
            case VoiceMode::Unison8:  return 8;
            case VoiceMode::Unison16: return (truePoly16 && !biTimbralSplit) ? 16 : 8;
            default:                  return 1;
        }
    }

    float getDetuneFactor(int stackSize, int indexInGroup) const noexcept
    {
        const size_t idx = static_cast<size_t>(indexInGroup);
        switch (stackSize)
        {
            case 2:  return (idx < UNISON2_DETUNE.size())  ? UNISON2_DETUNE[idx]  : 0.0f;
            case 4:  return (idx < UNISON4_DETUNE.size())  ? UNISON4_DETUNE[idx]  : 0.0f;
            case 8:  return (idx < UNISON8_DETUNE.size())  ? UNISON8_DETUNE[idx]  : 0.0f;
            case 16: return (idx < UNISON16_DETUNE.size()) ? UNISON16_DETUNE[idx] : 0.0f;
            default: return 0.0f;
        }
    }

    float getPanFactor(int stackSize, int indexInGroup) const noexcept
    {
        const size_t idx = static_cast<size_t>(indexInGroup);
        switch (stackSize)
        {
            case 2:  return (idx < UNISON2_PAN.size())  ? UNISON2_PAN[idx]  : 0.0f;
            case 4:  return (idx < UNISON4_PAN.size())  ? UNISON4_PAN[idx]  : 0.0f;
            case 8:  return (idx < UNISON8_PAN.size())  ? UNISON8_PAN[idx]  : 0.0f;
            case 16: return (idx < UNISON16_PAN.size()) ? UNISON16_PAN[idx] : 0.0f;
            default: return 0.0f;
        }
    }

    void updateActiveVoiceUnisonParameters() noexcept
    {
        const int stackSize = getUnisonStackSize();
        if (stackSize <= 1) return;

        for (int p = 0; p < NUM_PARTS; ++p)
        {
            const int numGroups = isTruePoly16Active(p) ? (NUM_VOICES / stackSize) : (VOICES_PER_PART / stackSize);

            for (int g = 0; g < numGroups; ++g)
            {
                const int baseVoice = getVoiceGroupBase(g, stackSize, p);
                if (isGroupActive(g, stackSize, p))
                {
                    for (int v = 0; v < stackSize; ++v)
                    {
                        const size_t vIdx = static_cast<size_t>(baseVoice + v);
                        const float det = getDetuneFactor(stackSize, v) * unisonDetuneCents;
                        const float pn  = getPanFactor(stackSize, v) * unisonSpread;
                        voiceDetune[vIdx] = det;
                        voicePan[vIdx]    = pn;
                        VoiceTraits::applyDetune(voices[vIdx], det);
                        VoiceTraits::applyPan(voices[vIdx], pn);
                    }
                }
            }
            if (isTruePoly16Active(p)) break;
        }
    }

    bool isGroupActive(int groupIndex, int stackSize, int part) const noexcept
    {
        const int baseVoice = getVoiceGroupBase(groupIndex, stackSize, part);
        for (int v = 0; v < stackSize; ++v)
        {
            if (voices[static_cast<size_t>(baseVoice + v)].isActive())
                return true;
        }
        return false;
    }

    bool isGroupRelease(int groupIndex, int stackSize, int part) const noexcept
    {
        const int baseVoice = getVoiceGroupBase(groupIndex, stackSize, part);
        bool anyActive = false;
        for (int v = 0; v < stackSize; ++v)
        {
            const size_t idx = static_cast<size_t>(baseVoice + v);
            if (voices[idx].isActive())
            {
                anyActive = true;
                if (!VoiceTraits::checkRelease(voices[idx]))
                    return false;
            }
        }
        return anyActive;
    }

    float getGroupEnvelopeLevel(int groupIndex, int stackSize, int part) const noexcept
    {
        const int baseVoice = getVoiceGroupBase(groupIndex, stackSize, part);
        float maxEnv = 0.0f;
        for (int v = 0; v < stackSize; ++v)
        {
            const float env = VoiceTraits::checkEnvLevel(voices[static_cast<size_t>(baseVoice + v)]);
            if (env > maxEnv)
                maxEnv = env;
        }
        return maxEnv;
    }

    uint32_t getGroupAge(int groupIndex, int stackSize, int part) const noexcept
    {
        return voiceAge[static_cast<size_t>(getVoiceGroupBase(groupIndex, stackSize, part))];
    }

    int getGroupNote(int groupIndex, int stackSize, int part) const noexcept
    {
        return voiceNote[static_cast<size_t>(getVoiceGroupBase(groupIndex, stackSize, part))];
    }

    int findBestGroupToAllocate(int noteNumber, int stackSize, int part) noexcept
    {
        juce::ignoreUnused(noteNumber);
        const int p = std::clamp(part, 0, NUM_PARTS - 1);
        const int numGroups = isTruePoly16Active(p) ? (NUM_VOICES / stackSize) : (VOICES_PER_PART / stackSize);

        // 1. Free / Idle group priority
        int freeGroup = -1;
        uint32_t oldestFreeAge = 0xFFFFFFFF;
        for (int g = 0; g < numGroups; ++g)
        {
            if (!isGroupActive(g, stackSize, p))
            {
                const uint32_t age = getGroupAge(g, stackSize, p);
                if (age < oldestFreeAge)
                {
                    oldestFreeAge = age;
                    freeGroup = g;
                }
            }
        }
        if (freeGroup >= 0)
            return freeGroup;

        // 2. Release group priority (MUST NOT steal physically held keys)
        int releaseGroup = -1;
        float lowestReleaseEnv = 100.0f;
        for (int g = 0; g < numGroups; ++g)
        {
            const int note = getGroupNote(g, stackSize, p);
            const bool isKeyPhysicallyHeld = (note >= 0 && note < 128 && physicalKeysDown[static_cast<size_t>(p)][static_cast<size_t>(note)]);

            if (!isKeyPhysicallyHeld && isGroupRelease(g, stackSize, p))
            {
                const float env = getGroupEnvelopeLevel(g, stackSize, p);
                if (env < lowestReleaseEnv)
                {
                    lowestReleaseEnv = env;
                    releaseGroup = g;
                }
            }
        }
        if (releaseGroup >= 0)
            return releaseGroup;

        // 3. Steal among NON-HELD groups (LRU + lowest envelope)
        int stealGroup = -1;
        float lowestScore = 1e9f;
        for (int g = 0; g < numGroups; ++g)
        {
            const int note = getGroupNote(g, stackSize, p);
            const bool isKeyPhysicallyHeld = (note >= 0 && note < 128 && physicalKeysDown[static_cast<size_t>(p)][static_cast<size_t>(note)]);

            if (!isKeyPhysicallyHeld)
            {
                const float env = getGroupEnvelopeLevel(g, stackSize, p);
                const int baseV = getVoiceGroupBase(g, stackSize, p);
                const float vel = voiceVelocity[static_cast<size_t>(baseV)];
                const uint32_t ageDelta = globalAgeCounter - getGroupAge(g, stackSize, p);
                const float ageWeight = std::min(0.5f, static_cast<float>(ageDelta) * 0.001f);
                const float score = (env * 0.7f) + ((vel / 127.0f) * 0.3f) - ageWeight;

                if (score < lowestScore)
                {
                    lowestScore = score;
                    stealGroup = g;
                }
            }
        }
        if (stealGroup >= 0)
            return stealGroup;

        // 4. Emergency fallback: steal oldest held note within boundary
        stealGroup = 0;
        uint32_t oldestHeldAge = 0xFFFFFFFF;
        for (int g = 0; g < numGroups; ++g)
        {
            const uint32_t age = getGroupAge(g, stackSize, p);
            if (age < oldestHeldAge)
            {
                oldestHeldAge = age;
                stealGroup = g;
            }
        }
        return stealGroup;
    }

    void handleGroupNoteOn(int noteNumber, float velocity, int stackSize, int part) noexcept
    {
        const int p = std::clamp(part, 0, NUM_PARTS - 1);
        const int chosenGroup = findBestGroupToAllocate(noteNumber, stackSize, p);
        const int baseVoice = getVoiceGroupBase(chosenGroup, stackSize, p);
        const uint32_t currentAge = ++globalAgeCounter;

        for (int v = 0; v < stackSize; ++v)
        {
            const size_t vIdx = static_cast<size_t>(baseVoice + v);
            const float det = (stackSize > 1) ? (getDetuneFactor(stackSize, v) * unisonDetuneCents) : 0.0f;
            const float pn  = (stackSize > 1) ? (getPanFactor(stackSize, v) * unisonSpread) : 0.0f;

            voiceDetune[vIdx]    = det;
            voicePan[vIdx]       = pn;
            voiceNote[vIdx]      = noteNumber;
            voiceVelocity[vIdx]  = velocity;
            voiceAge[vIdx]       = currentAge;
            voiceSustained[vIdx] = false;
            voicePart[vIdx]      = p;

            VoiceTraits::applyDetune(voices[vIdx], det);
            VoiceTraits::applyPan(voices[vIdx], pn);
            VoiceTraits::applyF2KeyTrack(voices[vIdx], partF2KeyTrack[static_cast<size_t>(p)]);
            VoiceTraits::prepareForSteal(voices[vIdx]);
            VoiceTraits::noteOnVoice(voices[vIdx], noteNumber, velocity, false);
        }
    }

    void handleRoundRobinNoteOn(int noteNumber, float velocity, int part) noexcept
    {
        const int p = std::clamp(part, 0, NUM_PARTS - 1);
        const bool isT16 = isTruePoly16Active(p);
        const size_t chosenVoice = static_cast<size_t>(roundRobinIndex[static_cast<size_t>(p)]);

        if (isT16)
        {
            roundRobinIndex[static_cast<size_t>(p)] = (roundRobinIndex[static_cast<size_t>(p)] + 1) % NUM_VOICES;
        }
        else
        {
            roundRobinIndex[static_cast<size_t>(p)] = (p * VOICES_PER_PART) + ((roundRobinIndex[static_cast<size_t>(p)] + 1 - (p * VOICES_PER_PART)) % VOICES_PER_PART);
        }

        voiceDetune[chosenVoice]    = 0.0f;
        voicePan[chosenVoice]       = 0.0f;
        voiceNote[chosenVoice]      = noteNumber;
        voiceVelocity[chosenVoice]  = velocity;
        voiceAge[chosenVoice]       = ++globalAgeCounter;
        voiceSustained[chosenVoice] = false;
        voicePart[chosenVoice]      = p;

        VoiceTraits::applyDetune(voices[chosenVoice], 0.0f);
        VoiceTraits::applyPan(voices[chosenVoice], 0.0f);
        VoiceTraits::applyF2KeyTrack(voices[chosenVoice], partF2KeyTrack[static_cast<size_t>(p)]);
        VoiceTraits::prepareForSteal(voices[chosenVoice]);
        VoiceTraits::noteOnVoice(voices[chosenVoice], noteNumber, velocity, false);
    }

    // =========================================================================
    // Authentic Last-Note Priority Monophonic Note Stack
    // Strictly enforces a single active voice (voice index 0 for Part 0)
    // =========================================================================
    void pushMonoNote(int noteNumber, float velocity, int part) noexcept
    {
        const size_t p = static_cast<size_t>(std::clamp(part, 0, NUM_PARTS - 1));
        removeMonoNote(noteNumber, part);
        if (monoNoteCount[p] < MAX_NOTE_STACK)
        {
            monoNoteStack[p][static_cast<size_t>(monoNoteCount[p])] = { noteNumber, velocity };
            ++monoNoteCount[p];
        }
    }

    void removeMonoNote(int noteNumber, int part) noexcept
    {
        const size_t p = static_cast<size_t>(std::clamp(part, 0, NUM_PARTS - 1));
        for (int i = 0; i < monoNoteCount[p]; ++i)
        {
            const size_t idx = static_cast<size_t>(i);
            if (monoNoteStack[p][idx].noteNumber == noteNumber)
            {
                for (int j = i; j < monoNoteCount[p] - 1; ++j)
                {
                    const size_t jIdx = static_cast<size_t>(j);
                    monoNoteStack[p][jIdx] = monoNoteStack[p][jIdx + 1];
                }
                --monoNoteCount[p];
                monoNoteStack[p][static_cast<size_t>(monoNoteCount[p])] = { -1, 0.0f };
                break;
            }
        }
    }

    void handleMonoNoteOn(int noteNumber, float velocity, int part) noexcept
    {
        const int p = std::clamp(part, 0, NUM_PARTS - 1);
        const size_t vLead = static_cast<size_t>(p * VOICES_PER_PART);
        const bool wasActive = voices[vLead].isActive();

        pushMonoNote(noteNumber, velocity, p);

        voiceAge[vLead] = ++globalAgeCounter;
        voiceVelocity[vLead] = velocity;
        voiceSustained[vLead] = false;
        voiceNote[vLead] = noteNumber;
        voicePart[vLead] = p;

        VoiceTraits::applyF2KeyTrack(voices[vLead], partF2KeyTrack[static_cast<size_t>(p)]);

        // Silence any other non-lead voices in this part to guarantee strict monophony
        const int startOther = static_cast<int>(vLead) + 1;
        const int endOther   = (p + 1) * VOICES_PER_PART;
        for (int v = startOther; v < endOther; ++v)
        {
            if (voices[static_cast<size_t>(v)].isActive())
            {
                VoiceTraits::noteOffVoice(voices[static_cast<size_t>(v)], 0.0f, false);
                voiceSustained[static_cast<size_t>(v)] = false;
                voiceNote[static_cast<size_t>(v)] = -1;
            }
        }

        const bool isLegato = isLegatoActive();
        if (wasActive && monoNoteCount[static_cast<size_t>(p)] > 1 && isLegato)
        {
            VoiceTraits::applyPitchLegato(voices[vLead], noteNumber, velocity);
        }
        else
        {
            VoiceTraits::applyDetune(voices[vLead], 0.0f);
            VoiceTraits::applyPan(voices[vLead], 0.0f);
            VoiceTraits::prepareForSteal(voices[vLead]);
            VoiceTraits::noteOnVoice(voices[vLead], noteNumber, velocity, false);
        }
    }

    void handleMonoNoteOff(int noteNumber, float velocity, int part) noexcept
    {
        const int p = std::clamp(part, 0, NUM_PARTS - 1);
        removeMonoNote(noteNumber, p);

        const size_t vLead = static_cast<size_t>(p * VOICES_PER_PART);
        if (monoNoteCount[static_cast<size_t>(p)] == 0)
        {
            if (sustainPedalDown)
            {
                voiceSustained[vLead] = true;
            }
            else
            {
                VoiceTraits::noteOffVoice(voices[vLead], velocity, true);
                voiceSustained[vLead] = false;
            }
        }
        else
        {
            const NoteEntry topNote = monoNoteStack[static_cast<size_t>(p)][static_cast<size_t>(monoNoteCount[static_cast<size_t>(p)] - 1)];
            voiceNote[vLead] = topNote.noteNumber;
            voiceVelocity[vLead] = topNote.velocity;

            const bool isLegato = isLegatoActive();
            if (isLegato)
            {
                VoiceTraits::applyPitchLegato(voices[vLead], topNote.noteNumber, topNote.velocity);
            }
            else
            {
                VoiceTraits::noteOnVoice(voices[vLead], topNote.noteNumber, topNote.velocity, false);
            }
        }
    }

    void handleMonoUnisonNoteOn(int noteNumber, float velocity, int stackSize, int part) noexcept
    {
        const int p = std::clamp(part, 0, NUM_PARTS - 1);
        const bool wasActive = isGroupActive(0, stackSize, p);
        pushMonoNote(noteNumber, velocity, p);
        const uint32_t currentAge = ++globalAgeCounter;
        const int baseVoice = getVoiceGroupBase(0, stackSize, p);
        const bool isLegato = isLegatoActive();

        for (int v = 0; v < stackSize; ++v)
        {
            const size_t vIdx = static_cast<size_t>(baseVoice + v);
            const float det = getDetuneFactor(stackSize, v) * unisonDetuneCents;
            const float pn  = getPanFactor(stackSize, v) * unisonSpread;

            voiceDetune[vIdx]    = det;
            voicePan[vIdx]       = pn;
            voiceNote[vIdx]      = noteNumber;
            voiceVelocity[vIdx]  = velocity;
            voiceAge[vIdx]       = currentAge;
            voiceSustained[vIdx] = false;
            voicePart[vIdx]      = p;

            VoiceTraits::applyDetune(voices[vIdx], det);
            VoiceTraits::applyPan(voices[vIdx], pn);
            VoiceTraits::applyF2KeyTrack(voices[vIdx], partF2KeyTrack[static_cast<size_t>(p)]);

            if (wasActive && monoNoteCount[static_cast<size_t>(p)] > 1 && isLegato)
            {
                VoiceTraits::applyPitchLegato(voices[vIdx], noteNumber, velocity);
            }
            else
            {
                VoiceTraits::prepareForSteal(voices[vIdx]);
                VoiceTraits::noteOnVoice(voices[vIdx], noteNumber, velocity, false);
            }
        }
    }

    void handleMonoUnisonNoteOff(int noteNumber, float velocity, int stackSize, int part) noexcept
    {
        const int p = std::clamp(part, 0, NUM_PARTS - 1);
        removeMonoNote(noteNumber, p);
        const int baseVoice = getVoiceGroupBase(0, stackSize, p);
        const bool isLegato = isLegatoActive();

        if (monoNoteCount[static_cast<size_t>(p)] == 0)
        {
            if (sustainPedalDown)
            {
                for (int v = 0; v < stackSize; ++v)
                    voiceSustained[static_cast<size_t>(baseVoice + v)] = true;
            }
            else
            {
                for (int v = 0; v < stackSize; ++v)
                {
                    const size_t vIdx = static_cast<size_t>(baseVoice + v);
                    VoiceTraits::noteOffVoice(voices[vIdx], velocity, true);
                    voiceSustained[vIdx] = false;
                }
            }
        }
        else
        {
            const NoteEntry topNote = monoNoteStack[static_cast<size_t>(p)][static_cast<size_t>(monoNoteCount[static_cast<size_t>(p)] - 1)];
            for (int v = 0; v < stackSize; ++v)
            {
                const size_t vIdx = static_cast<size_t>(baseVoice + v);
                voiceNote[vIdx]     = topNote.noteNumber;
                voiceVelocity[vIdx] = topNote.velocity;
                if (isLegato)
                {
                    VoiceTraits::applyPitchLegato(voices[vIdx], topNote.noteNumber, topNote.velocity);
                }
                else
                {
                    VoiceTraits::noteOnVoice(voices[vIdx], topNote.noteNumber, topNote.velocity, false);
                }
            }
        }
    }

    alignas(16) std::array<SynthVoice, NUM_VOICES> voices {};

    std::array<int, NUM_VOICES>      voiceNote {};
    std::array<float, NUM_VOICES>    voiceVelocity {};
    std::array<uint32_t, NUM_VOICES> voiceAge {};
    std::array<bool, NUM_VOICES>     voiceSustained {};
    std::array<float, NUM_VOICES>    voicePan {};
    std::array<float, NUM_VOICES>    voiceDetune {};
    std::array<int, NUM_VOICES>      voicePart {};

    std::array<std::array<bool, 128>, NUM_PARTS> physicalKeysDown {};
    std::array<std::array<NoteEntry, MAX_NOTE_STACK>, NUM_PARTS> monoNoteStack {};
    std::array<int, NUM_PARTS> monoNoteCount { 0, 0 };
    std::array<int, NUM_PARTS> roundRobinIndex { 0, VOICES_PER_PART };
    std::array<float, NUM_PARTS> partF2KeyTrack { 0.0f, 0.0f };

    VoiceMode currentMode = VoiceMode::PolyBiTimbral;
    bool cycleMode = false;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    uint32_t globalAgeCounter = 0;

    bool truePoly16         = false;
    bool biTimbralSplit     = true;

    float unisonDetuneCents = 18.0f;
    float unisonSpread      = 0.85f;
    bool unisonMono         = false;

    float glideTimeSeconds  = 0.05f;
    bool glideEnabled       = false;
    bool legatoMode         = true;

    int pitchBendRange      = 2;
    float currentPitchBend  = 0.0f;
    float currentModWheel   = 0.0f;
    float currentAftertouch = 0.0f;
    bool sustainPedalDown   = false;

    bool applyGainCompensation = true;

    bool chordMode = false;
    int currentChordIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceManager)
};