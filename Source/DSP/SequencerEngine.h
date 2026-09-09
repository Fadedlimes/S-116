#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_core/juce_core.h>
#endif

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

enum class SequencerMode : uint8_t
{
    Off = 0,
    Sequencer,
    Arpeggiator
};

enum class SequencerDirection : uint8_t
{
    Forward = 0,
    Backward,
    PingPong,
    Random,
    Walk
};

enum class ArpPattern : uint8_t
{
    Up = 0,
    Down,
    UpDown,
    DownUp,
    Random,
    AsPlayed,
    Chord
};

enum class ClockDivision : uint8_t
{
    Div_1_1 = 0,
    Div_1_2,
    Div_1_2D,
    Div_1_2T,
    Div_1_4,
    Div_1_4D,
    Div_1_4T,
    Div_1_8,
    Div_1_8D,
    Div_1_8T,
    Div_1_16,
    Div_1_16D,
    Div_1_16T,
    Div_1_32,
    Div_1_32T,
    Count
};

enum class TrackID : uint8_t
{
    TrackA = 0,
    TrackB
};

// =============================================================================
// Comprehensive Front-Panel Parameter Lock Identifiers (Elektron Matrix)
// =============================================================================
enum LockParamID : uint8_t
{
    Lock_DCO1Morph = 0,
    Lock_DCO1PWM,
    Lock_DCO1Fold,
    Lock_DCO1SawMix,
    Lock_DCO2Morph,
    Lock_DCO2PWM,
    Lock_DCO2Level,
    Lock_DCO2Semi,
    Lock_DCO2Cents,
    Lock_SubLevel,
    Lock_NoiseLevel,
    Lock_HPFCutoff,
    Lock_LPFCutoff,
    Lock_LPFRes,
    Lock_EnvMod,
    Lock_FilterBalance,
    Lock_F2Cutoff,
    Lock_F2Res,
    Lock_F2EnvDepth,
    Lock_AmpAttack,
    Lock_AmpDecay,
    Lock_AmpSustain,
    Lock_AmpRelease,
    Lock_FiltAttack,
    Lock_FiltDecay,
    Lock_FiltSustain,
    Lock_FiltRelease,
    Lock_AuxToPitch,
    Lock_AuxToFilter1,
    Lock_AuxToFilter2,
    Lock_AuxToFold,
    Lock_AuxToPWM,
    Lock_ParamCount
};

// =============================================================================
// Absolute Hardware-Style Parameter Overrides (Elektron P-Locks)
// =============================================================================
struct ParamLocks
{
    static constexpr size_t NUM_PARAMS = static_cast<size_t>(LockParamID::Lock_ParamCount);

    std::array<float, NUM_PARAMS> values {};
    uint64_t mask = 0;

    // Backward-compatible direct aliases
    float cutoffVal = 3000.0f; // Absolute locked cutoff in Hz [20, 20000]
    float resoVal   = 0.0f;    // Absolute locked resonance [0.0, 0.98]
    float morphVal  = 0.25f;   // Absolute locked waveform morph [0.0, 1.0]

    bool hasCutoffLock = false;
    bool hasResoLock   = false;
    bool hasMorphLock  = false;

    // Legacy offset aliases
    float cutoffOffset = 0.0f;
    float resoOffset   = 0.0f;
    float morphOffset  = 0.0f;

    void clear() noexcept
    {
        values.fill(0.0f);
        mask = 0;

        cutoffVal = 3000.0f;
        resoVal   = 0.0f;
        morphVal  = 0.25f;
        cutoffOffset = 0.0f;
        resoOffset   = 0.0f;
        morphOffset  = 0.0f;
        hasCutoffLock = false;
        hasResoLock   = false;
        hasMorphLock  = false;
    }

    bool hasAnyLock() const noexcept
    {
        return (mask != 0) || hasCutoffLock || hasResoLock || hasMorphLock;
    }

    bool isLocked(LockParamID id) const noexcept
    {
        const size_t bit = static_cast<size_t>(id);
        if (bit < 64)
            return (mask & (1ULL << bit)) != 0;
        return false;
    }

    float getLockValue(LockParamID id, float fallback) const noexcept
    {
        const size_t bit = static_cast<size_t>(id);
        if (bit < NUM_PARAMS && (mask & (1ULL << bit)) != 0)
            return values[bit];
        return fallback;
    }

    void setLock(LockParamID id, float val) noexcept
    {
        const size_t bit = static_cast<size_t>(id);
        if (bit < NUM_PARAMS)
        {
            values[bit] = val;
            mask |= (1ULL << bit);

            if (id == Lock_LPFCutoff)   { cutoffVal = val; hasCutoffLock = true; }
            else if (id == Lock_LPFRes) { resoVal = val;   hasResoLock = true; }
            else if (id == Lock_DCO1Morph) { morphVal = val; hasMorphLock = true; }
        }
    }

    void clearLock(LockParamID id) noexcept
    {
        const size_t bit = static_cast<size_t>(id);
        if (bit < NUM_PARAMS)
        {
            mask &= ~(1ULL << bit);
            values[bit] = 0.0f;

            if (id == Lock_LPFCutoff)   { hasCutoffLock = false; cutoffVal = 3000.0f; }
            else if (id == Lock_LPFRes) { hasResoLock = false; resoVal = 0.0f; }
            else if (id == Lock_DCO1Morph) { hasMorphLock = false; morphVal = 0.25f; }
        }
    }
};

struct SequencerStep
{
    static constexpr int MAX_CHORD_NOTES = 4;

    int note = 60; // Primary root note
    std::array<int, MAX_CHORD_NOTES> chordNotes { 60, -1, -1, -1 }; // Polyphonic chord storage

    float velocity = 0.8f;
    bool active = true;
    bool tie = false;
    bool accent = false;
    uint8_t ratchet = 1;
    float probability = 1.0f;

    // Per-step gate length override:
    // -1.0f = inherit track global gate length
    // > 0.0f = explicit per-step gate length (0.05f to 2.00f)
    float gateLength = -1.0f;

    ParamLocks locks;

    void reset(int defaultNote = 60) noexcept
    {
        note = defaultNote;
        chordNotes = { defaultNote, -1, -1, -1 };
        velocity = 0.8f;
        active = true;
        tie = false;
        accent = false;
        ratchet = 1;
        probability = 1.0f;
        gateLength = -1.0f;
        locks.clear();
    }

    float getEffectiveGate(float fallbackTrackGate) const noexcept
    {
        return (gateLength > 0.001f) ? gateLength : fallbackTrackGate;
    }
};

class SequencerTrack
{
public:
    static constexpr int MAX_STEPS = 64;
    static constexpr int MAX_ARP_NOTES = 16;

    SequencerTrack() noexcept
    {
        reset();
    }

    ~SequencerTrack() = default;

    void reset() noexcept
    {
        for (int i = 0; i < MAX_STEPS; ++i)
            steps[static_cast<size_t>(i)].reset(60);

        length = 64;
        currentStep = 0;
        playingStep = 0;
        pingPongForward = true;

        mode = SequencerMode::Sequencer;
        direction = SequencerDirection::Forward;
        division = ClockDivision::Div_1_16;
        arpPattern = ArpPattern::Up;
        arpOctaves = 1;

        swing = 0.0f;
        gateLength = 0.75f;
        octaveTranspose = 0;
        midiChannel = 1;

        mute = false;
        solo = false;

        activeSoundingNote = -1;
        activeSoundingChord = { -1, -1, -1, -1 };
        samplesUntilNoteOff = 0;
        samplesUntilNextStep = 0;

        currentRatchet = 0;
        totalRatchets = 1;
        ratchetIntervalSamples = 0;
        ratchetVelocity = 0.8f;

        activeLockMask = 0;
        activeLockValues.fill(0.0f);

        activeLockCutoff = -1.0f;
        activeLockReso   = -1.0f;
        activeLockMorph  = -1.0f;

        heldNotes.fill(-1);
        numHeldNotes = 0;
        arpIndex = 0;
        arpOctaveIndex = 0;
        arpDirectionUp = true;
    }

    void setLength(int newLength) noexcept
    {
        length = std::clamp(newLength, 1, MAX_STEPS);
        if (currentStep >= length)
            currentStep = 0;
    }

    int getLength() const noexcept { return length; }

    void setMode(SequencerMode newMode) noexcept { mode = newMode; }
    SequencerMode getMode() const noexcept       { return mode; }

    void setDirection(SequencerDirection newDir) noexcept { direction = newDir; }
    SequencerDirection getDirection() const noexcept      { return direction; }

    void setClockDivision(ClockDivision newDiv) noexcept { division = newDiv; }
    ClockDivision getClockDivision() const noexcept      { return division; }

    void setArpPattern(ArpPattern newPattern) noexcept { arpPattern = newPattern; }
    ArpPattern getArpPattern() const noexcept          { return arpPattern; }

    void setArpOctaves(int octaves) noexcept { arpOctaves = std::clamp(octaves, 1, 4); }
    int getArpOctaves() const noexcept       { return arpOctaves; }

    void setSwing(float newSwing) noexcept          { swing = std::clamp(newSwing, 0.0f, 0.75f); }
    float getSwing() const noexcept                 { return swing; }

    void setGateLength(float newGateLength) noexcept { gateLength = std::clamp(newGateLength, 0.05f, 0.98f); }
    float getGateLength() const noexcept             { return gateLength; }

    void setStepGateLength(int stepIndex, float perStepGate) noexcept
    {
        if (stepIndex >= 0 && stepIndex < MAX_STEPS)
        {
            if (perStepGate <= 0.0f)
                steps[static_cast<size_t>(stepIndex)].gateLength = -1.0f;
            else
                steps[static_cast<size_t>(stepIndex)].gateLength = std::clamp(perStepGate, 0.05f, 2.00f);
        }
    }

    float getStepGateLength(int stepIndex) const noexcept
    {
        if (stepIndex >= 0 && stepIndex < MAX_STEPS)
            return steps[static_cast<size_t>(stepIndex)].gateLength;
        return -1.0f;
    }

    void setOctaveTranspose(int oct) noexcept       { octaveTranspose = std::clamp(oct, -3, 3); }
    int getOctaveTranspose() const noexcept          { return octaveTranspose; }

    void setMidiChannel(int ch) noexcept            { midiChannel = std::clamp(ch, 1, 16); }
    int getMidiChannel() const noexcept             { return midiChannel; }

    void setMute(bool shouldMute) noexcept          { mute = shouldMute; }
    bool isMute() const noexcept                    { return mute; }

    void setSolo(bool shouldSolo) noexcept          { solo = shouldSolo; }
    bool isSolo() const noexcept                    { return solo; }

    int getCurrentStep() const noexcept             { return currentStep; }
    void setCurrentStep(int step) noexcept          { currentStep = std::clamp(step, 0, length - 1); }

    int getPlayingStep() const noexcept             { return playingStep; }

    SequencerStep& getStep(int index) noexcept
    {
        jassert(index >= 0 && index < MAX_STEPS);
        return steps[static_cast<size_t>(std::clamp(index, 0, MAX_STEPS - 1))];
    }

    const SequencerStep& getStep(int index) const noexcept
    {
        jassert(index >= 0 && index < MAX_STEPS);
        return steps[static_cast<size_t>(std::clamp(index, 0, MAX_STEPS - 1))];
    }

    void toggleStepActive(int stepIndex) noexcept
    {
        if (stepIndex >= 0 && stepIndex < MAX_STEPS)
            steps[static_cast<size_t>(stepIndex)].active = !steps[static_cast<size_t>(stepIndex)].active;
    }

    void transposeStep(int stepIndex, int semitones) noexcept
    {
        if (stepIndex >= 0 && stepIndex < MAX_STEPS)
        {
            auto& s = steps[static_cast<size_t>(stepIndex)];
            s.note = std::clamp(s.note + semitones, 0, 127);
            for (int& cn : s.chordNotes)
            {
                if (cn >= 0)
                    cn = std::clamp(cn + semitones, 0, 127);
            }
        }
    }

    void applyStepLocks(int stepIndex) noexcept
    {
        const auto& step = getStep(stepIndex);
        activeLockMask   = step.locks.mask;
        activeLockValues = step.locks.values;

        activeLockCutoff = (step.locks.isLocked(Lock_LPFCutoff) || step.locks.hasCutoffLock)
                         ? (step.locks.isLocked(Lock_LPFCutoff) ? step.locks.values[Lock_LPFCutoff] : step.locks.cutoffVal)
                         : -1.0f;

        activeLockReso   = (step.locks.isLocked(Lock_LPFRes) || step.locks.hasResoLock)
                         ? (step.locks.isLocked(Lock_LPFRes) ? step.locks.values[Lock_LPFRes] : step.locks.resoVal)
                         : -1.0f;

        activeLockMorph  = (step.locks.isLocked(Lock_DCO1Morph) || step.locks.hasMorphLock)
                         ? (step.locks.isLocked(Lock_DCO1Morph) ? step.locks.values[Lock_DCO1Morph] : step.locks.morphVal)
                         : -1.0f;
    }

    void clearStepLocks(int stepIndex) noexcept
    {
        getStep(stepIndex).locks.clear();
        if (stepIndex == currentStep)
        {
            activeLockMask = 0;
            activeLockValues.fill(0.0f);
            activeLockCutoff = -1.0f;
            activeLockReso   = -1.0f;
            activeLockMorph  = -1.0f;
        }
    }

    void clearAllLocks() noexcept
    {
        for (int i = 0; i < MAX_STEPS; ++i)
            steps[static_cast<size_t>(i)].locks.clear();

        activeLockMask = 0;
        activeLockValues.fill(0.0f);
        activeLockCutoff = -1.0f;
        activeLockReso   = -1.0f;
        activeLockMorph  = -1.0f;
    }

    bool isLockActive(LockParamID id) const noexcept
    {
        const size_t bit = static_cast<size_t>(id);
        if (bit < 64)
            return (activeLockMask & (1ULL << bit)) != 0;
        return false;
    }

    float getActiveLockValue(LockParamID id, float fallback) const noexcept
    {
        const size_t bit = static_cast<size_t>(id);
        if (bit < ParamLocks::NUM_PARAMS && (activeLockMask & (1ULL << bit)) != 0)
            return activeLockValues[bit];
        return fallback;
    }

    void handleNoteOn(int noteNumber) noexcept
    {
        for (int i = 0; i < numHeldNotes; ++i)
        {
            if (heldNotes[static_cast<size_t>(i)] == noteNumber)
                return;
        }

        if (numHeldNotes < MAX_ARP_NOTES)
        {
            heldNotes[static_cast<size_t>(numHeldNotes)] = noteNumber;
            ++numHeldNotes;
            sortHeldNotes();
        }
    }

    void handleNoteOff(int noteNumber) noexcept
    {
        for (int i = 0; i < numHeldNotes; ++i)
        {
            if (heldNotes[static_cast<size_t>(i)] == noteNumber)
            {
                for (int j = i; j < numHeldNotes - 1; ++j)
                    heldNotes[static_cast<size_t>(j)] = heldNotes[static_cast<size_t>(j + 1)];

                --numHeldNotes;
                heldNotes[static_cast<size_t>(numHeldNotes)] = -1;
                break;
            }
        }
    }

    void clearHeldNotes() noexcept
    {
        heldNotes.fill(-1);
        numHeldNotes = 0;
    }

    static float getDivisionQuarterNotes(ClockDivision div) noexcept
    {
        switch (div)
        {
            case ClockDivision::Div_1_1:   return 4.0f;
            case ClockDivision::Div_1_2:   return 2.0f;
            case ClockDivision::Div_1_2D:  return 3.0f;
            case ClockDivision::Div_1_2T:  return 4.0f / 3.0f;
            case ClockDivision::Div_1_4:   return 1.0f;
            case ClockDivision::Div_1_4D:  return 1.5f;
            case ClockDivision::Div_1_4T:  return 2.0f / 3.0f;
            case ClockDivision::Div_1_8:   return 0.5f;
            case ClockDivision::Div_1_8D:  return 0.75f;
            case ClockDivision::Div_1_8T:  return 1.0f / 3.0f;
            case ClockDivision::Div_1_16:  return 0.25f;
            case ClockDivision::Div_1_16D: return 0.375f;
            case ClockDivision::Div_1_16T: return 0.5f / 3.0f;
            case ClockDivision::Div_1_32:  return 0.125f;
            case ClockDivision::Div_1_32T: return 0.25f / 3.0f;
            default:                       return 0.25f;
        }
    }

    void advanceStep(uint32_t& prngState) noexcept
    {
        if (length <= 1)
        {
            currentStep = 0;
            return;
        }

        switch (direction)
        {
            case SequencerDirection::Forward:
                currentStep = (currentStep + 1) % length;
                break;

            case SequencerDirection::Backward:
                currentStep = (currentStep - 1 + length) % length;
                break;

            case SequencerDirection::PingPong:
                if (pingPongForward)
                {
                    if (currentStep >= length - 1)
                    {
                        pingPongForward = false;
                        currentStep = length - 2;
                    }
                    else
                    {
                        ++currentStep;
                    }
                }
                else
                {
                    if (currentStep <= 0)
                    {
                        pingPongForward = true;
                        currentStep = 1;
                    }
                    else
                    {
                        --currentStep;
                    }
                }
                currentStep = std::clamp(currentStep, 0, length - 1);
                break;

            case SequencerDirection::Random:
                currentStep = static_cast<int>(nextRandomUint(prngState) % static_cast<uint32_t>(length));
                break;

            case SequencerDirection::Walk:
            {
                const int move = static_cast<int>(nextRandomUint(prngState) % 3) - 1;
                currentStep = std::clamp(currentStep + move, 0, length - 1);
                break;
            }
        }
    }

    int getNextArpNote(uint32_t& prngState) noexcept
    {
        if (numHeldNotes <= 0)
            return -1;

        int arpNote = heldNotes[0];

        switch (arpPattern)
        {
            case ArpPattern::Up:
                arpNote = heldNotes[static_cast<size_t>(arpIndex % numHeldNotes)];
                advanceArpPointerUp();
                break;

            case ArpPattern::Down:
                arpNote = heldNotes[static_cast<size_t>((numHeldNotes - 1) - (arpIndex % numHeldNotes))];
                advanceArpPointerUp();
                break;

            case ArpPattern::UpDown:
                arpNote = heldNotes[static_cast<size_t>(arpIndex % numHeldNotes)];
                if (arpDirectionUp)
                {
                    if (arpIndex >= numHeldNotes - 1)
                    {
                        arpDirectionUp = false;
                        if (numHeldNotes > 1)
                            --arpIndex;
                    }
                    else
                    {
                        ++arpIndex;
                    }
                }
                else
                {
                    if (arpIndex <= 0)
                    {
                        arpDirectionUp = true;
                        advanceArpOctave();
                        if (numHeldNotes > 1)
                            ++arpIndex;
                    }
                    else
                    {
                        --arpIndex;
                    }
                }
                break;

            case ArpPattern::DownUp:
                arpNote = heldNotes[static_cast<size_t>((numHeldNotes - 1) - (arpIndex % numHeldNotes))];
                if (!arpDirectionUp)
                {
                    if (arpIndex >= numHeldNotes - 1)
                    {
                        arpDirectionUp = true;
                        if (numHeldNotes > 1)
                            --arpIndex;
                    }
                    else
                    {
                        ++arpIndex;
                    }
                }
                else
                {
                    if (arpIndex <= 0)
                    {
                        arpDirectionUp = false;
                        advanceArpOctave();
                        if (numHeldNotes > 1)
                            ++arpIndex;
                    }
                    else
                    {
                        --arpIndex;
                    }
                }
                break;

            case ArpPattern::Random:
                arpNote = heldNotes[static_cast<size_t>(nextRandomUint(prngState) % static_cast<uint32_t>(numHeldNotes))];
                arpOctaveIndex = static_cast<int>(nextRandomUint(prngState) % static_cast<uint32_t>(arpOctaves));
                break;

            case ArpPattern::AsPlayed:
            case ArpPattern::Chord:
                arpNote = heldNotes[static_cast<size_t>(arpIndex % numHeldNotes)];
                advanceArpPointerUp();
                break;
        }

        const int transposed = arpNote + (arpOctaveIndex * 12) + (octaveTranspose * 12);
        return std::clamp(transposed, 0, 127);
    }

    // Active universal parameter lock bus
    std::array<float, ParamLocks::NUM_PARAMS> activeLockValues {};
    uint64_t activeLockMask = 0;

    // Fast-path legacy overrides (-1.0f denotes no lock)
    float activeLockCutoff = -1.0f;
    float activeLockReso   = -1.0f;
    float activeLockMorph  = -1.0f;

    std::array<int, SequencerStep::MAX_CHORD_NOTES> activeSoundingChord { -1, -1, -1, -1 };

private:
    friend class SequencerEngine;

    static inline uint32_t nextRandomUint(uint32_t& state) noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    void advanceArpPointerUp() noexcept
    {
        ++arpIndex;
        if (arpIndex >= numHeldNotes)
        {
            arpIndex = 0;
            advanceArpOctave();
        }
    }

    void advanceArpOctave() noexcept
    {
        ++arpOctaveIndex;
        if (arpOctaveIndex >= arpOctaves)
            arpOctaveIndex = 0;
    }

    void sortHeldNotes() noexcept
    {
        for (int i = 0; i < numHeldNotes - 1; ++i)
        {
            for (int j = 0; j < numHeldNotes - i - 1; ++j)
            {
                if (heldNotes[static_cast<size_t>(j)] > heldNotes[static_cast<size_t>(j + 1)])
                {
                    const int temp = heldNotes[static_cast<size_t>(j)];
                    heldNotes[static_cast<size_t>(j)] = heldNotes[static_cast<size_t>(j + 1)];
                    heldNotes[static_cast<size_t>(j + 1)] = temp;
                }
            }
        }
    }

    std::array<SequencerStep, MAX_STEPS> steps {};
    int length = 64;
    int currentStep = 0;
    int playingStep = 0;
    bool pingPongForward = true;

    SequencerMode mode = SequencerMode::Sequencer;
    SequencerDirection direction = SequencerDirection::Forward;
    ClockDivision division = ClockDivision::Div_1_16;
    ArpPattern arpPattern = ArpPattern::Up;
    int arpOctaves = 1;

    float swing = 0.0f;
    float gateLength = 0.75f;
    int octaveTranspose = 0;
    int midiChannel = 1;

    bool mute = false;
    bool solo = false;

    int activeSoundingNote = -1;
    int samplesUntilNoteOff = 0;
    int samplesUntilNextStep = 0;

    int currentRatchet = 0;
    int totalRatchets = 1;
    int ratchetIntervalSamples = 0;
    float ratchetVelocity = 0.8f;

    std::array<int, MAX_ARP_NOTES> heldNotes {};
    int numHeldNotes = 0;
    int arpIndex = 0;
    int arpOctaveIndex = 0;
    bool arpDirectionUp = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SequencerTrack)
};

class SequencerEngine
{
public:
    struct FlushedNote {
        int note;
        int channel;
    };

    bool isPlaying = false;
    int currentStep = 0;
    bool arpEnabled = false;
    bool isRecording = false;

    SequencerEngine() noexcept
    {
        reset();
    }

    ~SequencerEngine() = default;

    void prepare(double sampleRate) noexcept
    {
        currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        reset();
    }

    void setSampleRate(double sampleRate) noexcept
    {
        prepare(sampleRate);
    }

    void reset() noexcept
    {
        numPages = 4;

        tracks[0].reset();
        tracks[0].setMidiChannel(1);
        tracks[0].setLength(64);

        tracks[1].reset();
        tracks[1].setMidiChannel(2);
        tracks[1].setLength(64);

        activeEditTrack = TrackID::TrackA;
        activePage = 0;
        selectedStep = 0;

        internalBpm = 120.0f;
        syncToHost = true;
        isPlaying = false;
        currentStep = 0;
        arpEnabled = false;
        isRecording = false;
        internalSampleCounter = 0;
        prngState = 0x5a5a5a5a;

        arpSampleAccumulator = 0;
    }

    void clearTrack(TrackID trackId) noexcept
    {
        auto& track = tracks[static_cast<size_t>(trackId)];
        for (int i = 0; i < SequencerTrack::MAX_STEPS; ++i)
            track.getStep(i).reset(60);
    }

    void clearAllTracks() noexcept
    {
        clearTrack(TrackID::TrackA);
        clearTrack(TrackID::TrackB);
    }

    void setEditTrack(TrackID track) noexcept { activeEditTrack = track; }
    TrackID getEditTrack() const noexcept    { return activeEditTrack; }

    void toggleEditTrack() noexcept
    {
        activeEditTrack = (activeEditTrack == TrackID::TrackA) ? TrackID::TrackB : TrackID::TrackA;
    }

    void setActivePage(int page) noexcept { activePage = std::clamp(page, 0, 3); }
    int getActivePage() const noexcept    { return activePage; }

    int getSelectedStep() const noexcept  { return selectedStep; }
    void setSelectedStep(int step) noexcept { selectedStep = std::clamp(step, 0, SequencerTrack::MAX_STEPS - 1); }

    void toggleSelectedStepActive() noexcept
    {
        getActiveTrack().toggleStepActive(selectedStep);
    }

    void transposeSelectedStep(int semitones) noexcept
    {
        getActiveTrack().transposeStep(selectedStep, semitones);
    }

    void setSelectedStepGateLength(float gate) noexcept
    {
        getActiveTrack().setStepGateLength(selectedStep, gate);
    }

    float getSelectedStepGateLength() const noexcept
    {
        return getActiveTrack().getStepGateLength(selectedStep);
    }

    int getNumPages() const noexcept { return numPages; }
    void setNumPages(int pages) noexcept
    {
        numPages = std::clamp(pages, 1, 4);
        const int totalSteps = numPages * 16;
        tracks[0].setLength(totalSteps);
        tracks[1].setLength(totalSteps);
    }

    int getNumSteps() const noexcept
    {
        return getActiveTrack().getLength();
    }

    SequencerStep& getStep(int index) noexcept
    {
        return getActiveTrack().getStep(index);
    }

    const SequencerStep& getStep(int index) const noexcept
    {
        return getActiveTrack().getStep(index);
    }

    void applyStepLocks(TrackID trackId, int stepIndex) noexcept
    {
        tracks[static_cast<size_t>(trackId)].applyStepLocks(stepIndex);
    }

    void clearStepLocks(TrackID trackId, int stepIndex) noexcept
    {
        tracks[static_cast<size_t>(trackId)].clearStepLocks(stepIndex);
    }

    void setStepParamLock(TrackID trackId, int stepIndex, LockParamID paramId, float val) noexcept
    {
        auto& step = tracks[static_cast<size_t>(trackId)].getStep(stepIndex);
        step.locks.setLock(paramId, val);
    }

    void clearStepParamLock(TrackID trackId, int stepIndex, LockParamID paramId) noexcept
    {
        auto& step = tracks[static_cast<size_t>(trackId)].getStep(stepIndex);
        step.locks.clearLock(paramId);
    }

    void setStepCutoffLock(TrackID trackId, int stepIndex, float cutoffHz) noexcept
    {
        setStepParamLock(trackId, stepIndex, Lock_LPFCutoff, std::clamp(cutoffHz, 20.0f, 20000.0f));
    }

    void setStepResoLock(TrackID trackId, int stepIndex, float reso) noexcept
    {
        setStepParamLock(trackId, stepIndex, Lock_LPFRes, std::clamp(reso, 0.0f, 0.98f));
    }

    void setStepMorphLock(TrackID trackId, int stepIndex, float morph) noexcept
    {
        setStepParamLock(trackId, stepIndex, Lock_DCO1Morph, std::clamp(morph, 0.0f, 1.0f));
    }

    void clearArp() noexcept
    {
        tracks[0].clearHeldNotes();
        tracks[1].clearHeldNotes();
    }

    void setArpMode(int mode) noexcept
    {
        getActiveTrack().setArpPattern(static_cast<ArpPattern>(std::clamp(mode, 0, 6)));
    }

    void addArpNote(int note) noexcept
    {
        getActiveTrack().handleNoteOn(note);
    }

    void removeArpNote(int note) noexcept
    {
        getActiveTrack().handleNoteOff(note);
    }

    int getNumArpHeld() const noexcept
    {
        return getActiveTrack().numHeldNotes;
    }

    void recordNote(int noteNum, float velocity = 0.8f, int targetStepIndex = -1) noexcept
    {
        auto& track = getActiveTrack();
        const int targetStep = (targetStepIndex >= 0) ? targetStepIndex : (isPlaying ? track.getCurrentStep() : selectedStep);
        auto& step = track.getStep(targetStep);

        step.note = noteNum;
        step.chordNotes[0] = noteNum;
        for (size_t i = 1; i < step.chordNotes.size(); ++i)
            step.chordNotes[i] = -1;

        step.velocity = velocity;
        step.active = true;

        if (!isPlaying && targetStepIndex < 0)
        {
            selectedStep = (selectedStep + 1) % track.getLength();
            activePage = selectedStep / 16;
        }
    }

    void recordChord(const std::vector<int>& notesToRecord, float velocity = 0.8f, int targetStepIndex = -1) noexcept
    {
        if (notesToRecord.empty()) return;

        auto& track = getActiveTrack();
        const int targetStep = (targetStepIndex >= 0) ? targetStepIndex : (isPlaying ? track.getCurrentStep() : selectedStep);
        auto& step = track.getStep(targetStep);

        step.note = notesToRecord[0];
        for (size_t i = 0; i < step.chordNotes.size(); ++i)
        {
            if (i < notesToRecord.size())
                step.chordNotes[i] = notesToRecord[i];
            else
                step.chordNotes[i] = -1;
        }

        step.velocity = velocity;
        step.active = true;

        if (!isPlaying && targetStepIndex < 0)
        {
            selectedStep = (selectedStep + 1) % track.getLength();
            activePage = selectedStep / 16;
        }
    }

    bool advanceArpClock(int numSamples, int& arpNote, float& arpVel, bool& arpOff) noexcept
    {
        if (!arpEnabled || getNumArpHeld() == 0)
        {
            arpOff = false;
            return false;
        }

        const double secondsPerStep = (60.0 / std::max(20.0f, internalBpm)) * 0.25;
        const int stepSamples = std::max(1, static_cast<int>(secondsPerStep * currentSampleRate));

        arpSampleAccumulator += numSamples;

        if (arpSampleAccumulator >= stepSamples)
        {
            arpSampleAccumulator -= stepSamples;
            arpNote = getActiveTrack().getNextArpNote(prngState);
            arpVel = 0.85f;
            arpOff = false;
            return (arpNote >= 0);
        }

        const int gateSamples = static_cast<int>(static_cast<float>(stepSamples) * getActiveTrack().getGateLength());
        if (arpSampleAccumulator >= gateSamples)
        {
            arpOff = true;
        }

        return false;
    }

    bool advanceStepClockSample(TrackID trackId, double bpm, int& noteOnOut, float& velOnOut, int& noteOffOut) noexcept
    {
        noteOnOut = -1;
        noteOffOut = -1;
        velOnOut = 0.0f;

        if (!isPlaying) return false;

        auto& track = tracks[static_cast<size_t>(trackId)];
        if (track.getMode() == SequencerMode::Off || track.isMute())
        {
            if (track.activeSoundingNote >= 0) {
                noteOffOut = track.activeSoundingNote;
                track.activeSoundingNote = -1;
                track.activeSoundingChord.fill(-1);
                track.samplesUntilNoteOff = 0;
            }
            track.activeLockMask = 0;
            track.activeLockValues.fill(0.0f);
            track.activeLockCutoff = -1.0f;
            track.activeLockReso   = -1.0f;
            track.activeLockMorph  = -1.0f;
            return false;
        }

        bool triggered = false;

        if (track.samplesUntilNoteOff > 0)
        {
            --track.samplesUntilNoteOff;
            if (track.samplesUntilNoteOff <= 0)
            {
                noteOffOut = track.activeSoundingNote;
                track.activeSoundingNote = -1;
                track.activeSoundingChord.fill(-1);
            }
        }

        if (track.currentRatchet < track.totalRatchets - 1 && track.ratchetIntervalSamples > 0)
        {
            if (track.samplesUntilNextStep % track.ratchetIntervalSamples == 0 && track.samplesUntilNextStep > 0)
            {
                ++track.currentRatchet;
                if (track.activeSoundingNote >= 0)
                {
                    noteOffOut = track.activeSoundingNote;
                    noteOnOut = track.activeSoundingNote;
                    velOnOut = track.ratchetVelocity;
                    triggered = true;
                }
            }
        }

        if (track.samplesUntilNextStep <= 0)
        {
            const float qNotes = SequencerTrack::getDivisionQuarterNotes(track.getClockDivision());
            const double stepDurationSeconds = qNotes * (60.0 / bpm);
            int stepIntervalSamples = std::max(1, static_cast<int>(stepDurationSeconds * currentSampleRate));

            if (track.getSwing() > 0.001f && (track.getCurrentStep() % 2 == 1))
            {
                stepIntervalSamples += static_cast<int>(track.getSwing() * 0.5f * static_cast<float>(stepIntervalSamples));
            }

            track.samplesUntilNextStep = stepIntervalSamples;

            if (track.getMode() == SequencerMode::Sequencer)
            {
                const int stepToPlay = track.getCurrentStep();
                track.playingStep = stepToPlay;
                const auto& step = track.getStep(stepToPlay);

                if (step.active)
                {
                    if (nextRandomFloat(prngState) <= step.probability)
                    {
                        int noteToPlay = std::clamp(step.note + (track.getOctaveTranspose() * 12), 0, 127);
                        float vel = step.velocity;
                        if (step.accent) vel = std::min(1.0f, vel * 1.35f);

                        if (track.activeSoundingNote >= 0 && !step.tie)
                        {
                            noteOffOut = track.activeSoundingNote;
                        }

                        if (track.activeSoundingNote != noteToPlay || !step.tie)
                        {
                            noteOnOut = noteToPlay;
                            velOnOut = vel;
                            track.activeSoundingNote = noteToPlay;
                            track.activeSoundingChord = step.chordNotes;
                            triggered = true;

                            track.applyStepLocks(stepToPlay);
                        }

                        track.totalRatchets = std::clamp(static_cast<int>(step.ratchet), 1, 4);
                        track.currentRatchet = 0;
                        track.ratchetVelocity = vel;
                        track.ratchetIntervalSamples = stepIntervalSamples / track.totalRatchets;

                        if (step.tie) {
                            track.samplesUntilNoteOff = stepIntervalSamples + 64;
                        } else {
                            const float effectiveGate = step.getEffectiveGate(track.getGateLength());
                            track.samplesUntilNoteOff = std::max(1, static_cast<int>(static_cast<float>(track.ratchetIntervalSamples) * effectiveGate));
                        }
                    }
                    else if (track.activeSoundingNote >= 0)
                    {
                        noteOffOut = track.activeSoundingNote;
                        track.activeSoundingNote = -1;
                        track.activeSoundingChord.fill(-1);
                        track.activeLockMask = 0;
                        track.activeLockValues.fill(0.0f);
                        track.activeLockCutoff = -1.0f;
                        track.activeLockReso   = -1.0f;
                        track.activeLockMorph  = -1.0f;
                    }
                }
                else
                {
                    if (track.activeSoundingNote >= 0)
                    {
                        noteOffOut = track.activeSoundingNote;
                        track.activeSoundingNote = -1;
                        track.activeSoundingChord.fill(-1);
                    }
                    track.activeLockMask = 0;
                    track.activeLockValues.fill(0.0f);
                    track.activeLockCutoff = -1.0f;
                    track.activeLockReso   = -1.0f;
                    track.activeLockMorph  = -1.0f;
                }

                track.advanceStep(prngState);
            }
        }
        else
        {
            --track.samplesUntilNextStep;
        }

        return triggered;
    }

    template <typename VoiceManagerType>
    int flushAllTracks(VoiceManagerType& vm, FlushedNote* outNotes = nullptr) noexcept
    {
        int count = 0;
        for (int t = 0; t < 2; ++t)
        {
            auto& track = tracks[t];
            if (track.activeSoundingNote >= 0) {
                vm.noteOff(track.activeSoundingNote, 0.0f, t);
                for (int cn : track.activeSoundingChord) {
                    if (cn >= 0 && cn != track.activeSoundingNote)
                        vm.noteOff(cn, 0.0f, t);
                }
                if (outNotes && count < 4) {
                    outNotes[count].note = track.activeSoundingNote;
                    outNotes[count].channel = track.getMidiChannel();
                    count++;
                }
                track.activeSoundingNote = -1;
                track.activeSoundingChord.fill(-1);
            }
            track.samplesUntilNoteOff = 0;
            track.currentRatchet = 0;
            track.activeLockMask = 0;
            track.activeLockValues.fill(0.0f);
            track.activeLockCutoff = -1.0f;
            track.activeLockReso   = -1.0f;
            track.activeLockMorph  = -1.0f;
        }
        return count;
    }

    SequencerTrack& getTrack(TrackID id) noexcept
    {
        return tracks[static_cast<size_t>(id)];
    }

    const SequencerTrack& getTrack(TrackID id) const noexcept
    {
        return tracks[static_cast<size_t>(id)];
    }

    SequencerTrack& getActiveTrack() noexcept
    {
        return tracks[static_cast<size_t>(activeEditTrack)];
    }

    const SequencerTrack& getActiveTrack() const noexcept
    {
        return tracks[static_cast<size_t>(activeEditTrack)];
    }

    void setTempo(float bpm) noexcept
    {
        internalBpm = std::clamp(bpm, 20.0f, 300.0f);
    }

    float getTempo() const noexcept { return internalBpm; }

    void setSyncToHost(bool shouldSync) noexcept { syncToHost = shouldSync; }
    bool isSyncToHost() const noexcept           { return syncToHost; }

    void play() noexcept
    {
        isPlaying = true;
        internalSampleCounter = 0;
        arpSampleAccumulator = 0;
    }

    template <typename VoiceManagerType>
    void stop(VoiceManagerType& vm, FlushedNote* outNotes = nullptr) noexcept
    {
        isPlaying = false;
        internalSampleCounter = 0;
        arpSampleAccumulator = 0;
        flushAllTracks(vm, outNotes);
        tracks[0].setCurrentStep(0);
        tracks[1].setCurrentStep(0);
        currentStep = 0;
    }

    bool getIsPlaying() const noexcept { return isPlaying; }

private:
    static inline float nextRandomFloat(uint32_t& state) noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state) * (1.0f / 4294967295.0f);
    }

    std::array<SequencerTrack, 2> tracks {};
    TrackID activeEditTrack = TrackID::TrackA;
    int activePage = 0;
    int selectedStep = 0;
    int numPages = 4;

    double currentSampleRate = 44100.0;
    float internalBpm = 120.0f;
    bool syncToHost = true;
    uint64_t internalSampleCounter = 0;
    uint32_t prngState = 0x5a5a5a5a;

    int arpSampleAccumulator = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SequencerEngine)
};