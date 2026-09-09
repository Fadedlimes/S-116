#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_devices/juce_audio_devices.h>
 #include <juce_core/juce_core.h>
#endif

#include "DSP/VoiceManager.h"
#include "DSP/SynthVoice.h"
#include "DSP/LFO.h"
#include "DSP/StereoChorus.h"
#include "DSP/StereoDelay.h"
#include "DSP/StereoReverb.h"
#include "DSP/SequencerEngine.h"
#include "DSP/ModulationMatrix.h"
#include "DSP/ExpansionCardManager.h"
#include "UI/OscilloscopeComponent.h"
#include <array>
#include <vector>
#include <memory>
#include <algorithm>

// =============================================================================
// Complete sound parameter state for an individual timbre part (Lower / Upper)
// =============================================================================
struct TimbrePatch
{
    // DCO 1 (Morph: 0.00 = Sine, 0.25 = Triangle, 0.50 = Saw, 0.75 = Square, 1.00 = Pulse)
    float dco1Morph     = 0.25f; // Default Triangle
    float dco1PWM       = 0.50f;
    int   dco1Octave    = 2;
    float dco1Fold      = 0.0f;
    float dco1SawMix    = 0.0f;
    float dco1SawDetune = 0.50f; // Roland JP-8000 Supersaw Detune Spread

    // DCO 2
    float dco2Morph   = 0.50f; // Default Saw
    float dco2PWM     = 0.50f;
    int   dco2Semi    = 0;
    float dco2Cents   = 0.0f;
    float dco2Level   = 0.0f;
    float dco2Fold    = 0.0f; // West Coast Wavefolder on DCO 2

    // Hard Sync & Cross-Mod / FM
    bool  syncMode    = false;
    float xmodAmount  = 0.0f; // Audio-Rate FM Amount (DCO1 -> DCO2)
    int   xmodMode    = 0;    // 0 = Linear FM, 1 = Exponential FM

    // Sub & Noise
    int   subOctave   = 0;
    float subLevel    = 0.0f;
    float noiseLevel  = 0.0f;

    // Filters
    float hpfCutoff   = 20.0f;
    float lpfCutoff   = 3000.0f;
    float lpfRes      = 0.0f;
    float envMod      = 0.0f;

    bool  filterRouting = false;
    float filterBalance = 0.50f;
    float f2Cutoff    = 3000.0f;
    float f2Res       = 0.0f;
    int   f2Mode      = 0;
    float f2EnvDepth  = 0.0f;
    float f2KeyTrack  = 0.0f;

    // Envelopes
    float ampAttack   = 0.001f;
    float ampDecay    = 0.10f;
    float ampSustain  = 1.0f;
    float ampRelease  = 0.10f;

    float filtAttack  = 0.001f;
    float filtDecay   = 0.20f;
    float filtSustain = 1.0f;
    float filtRelease = 0.10f;

    float auxAttack   = 0.01f;
    float auxDecay    = 0.50f;
    float auxSustain  = 0.0f;
    float auxRelease  = 0.50f;

    float auxToPitch   = 0.0f;
    float auxToFilter1 = 0.0f;
    float auxToFilter2 = 0.0f;
    float auxToFold    = 0.0f;
    float auxToPWM     = 0.0f;

    // LFOs
    float lfo1Rate     = 2.0f;
    int   lfo1Shape    = 1;
    float lfo1ToFilter = 0.0f;
    float lfo1ToPitch  = 0.0f;

    float lfo2Rate     = 1.0f;
    int   lfo2Shape    = 0;
    float lfo2Amount   = 0.0f;
    int   lfo2Target   = 0;

    float lfo3Rate     = 0.5f;
    int   lfo3Shape    = 1;
    float lfo3Amount   = 0.0f;
    int   lfo3Target   = 0;

    // Performance & Play Mode
    int   playMode     = 1; // 0 = Poly 8 Bi-Timbral, 1 = Poly 16 [Single]
    float glideTime    = 0.05f;
    bool  glideEnable  = false;
    bool  chordEnable  = false;
    int   chordType    = 0;
};

// =============================================================================
// True Bi-Timbral Modulation Matrix Proxy
// Decouples Track A and Track B modulations while maintaining seamless UI access
// =============================================================================
struct BiTimbralModMatrix
{
    std::array<ModulationMatrix, 2> parts;
    const int* activePartPtr = nullptr;

    ModulationMatrix& operator[](size_t index) noexcept {
        return parts[index < 2 ? index : 0];
    }
    const ModulationMatrix& operator[](size_t index) const noexcept {
        return parts[index < 2 ? index : 0];
    }

    ModulationMatrix& getActive() noexcept {
        const int p = (activePartPtr != nullptr) ? *activePartPtr : 0;
        return parts[static_cast<size_t>(std::clamp(p, 0, 1))];
    }
    const ModulationMatrix& getActive() const noexcept {
        const int p = (activePartPtr != nullptr) ? *activePartPtr : 0;
        return parts[static_cast<size_t>(std::clamp(p, 0, 1))];
    }

    decltype(auto) getSlot(int slotIndex) const noexcept {
        return getActive().getSlot(slotIndex);
    }
    decltype(auto) getSlot(int slotIndex) noexcept {
        return getActive().getSlot(slotIndex);
    }

    void setSlot(int slotIndex, ModSource src, ModDestination dst, float amt, ModSource via, float viaAmt, bool en) noexcept {
        getActive().setSlot(slotIndex, src, dst, amt, via, viaAmt, en);
    }
    void reset() noexcept {
        parts[0].reset();
        parts[1].reset();
    }
    void evaluate() noexcept {
        parts[0].evaluate();
        parts[1].evaluate();
    }
    void setSourceValue(ModSource src, float val) noexcept {
        parts[0].setSourceValue(src, val);
        parts[1].setSourceValue(src, val);
    }
    float getDestinationOffset(ModDestination dst) const noexcept {
        return getActive().getDestinationOffset(dst);
    }
};

class Simple116AudioProcessor : public juce::AudioProcessor,
                                private juce::MidiInputCallback
{
public:
    Simple116AudioProcessor();
    ~Simple116AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "S-116"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Host DAW 16-Program Presets Management
    int getNumPrograms() override { return 16; }
    int getCurrentProgram() override { return currentPresetIndex >= 0 ? currentPresetIndex : 0; }
    void setCurrentProgram (int index) override { loadFactoryPreset(index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    // Preset Disk Management
    juce::File getPresetsDirectory() const;
    juce::StringArray getAvailablePresets() const;
    bool saveUserPreset (const juce::String& presetName);
    bool loadPresetByName (const juce::String& presetName);

    const juce::String& getCurrentPresetName() const noexcept { return currentPresetName; }
    void setCurrentPresetName (const juce::String& name) noexcept { currentPresetName = name; }
    int getCurrentPresetIndex() const noexcept { return currentPresetIndex; }

    // =========================================================================
    // Voice Expansion Card System (.s116card)
    // =========================================================================
    ExpansionCardManager expansionCardManager;

    bool mountExpansionCard(int cardIndex);
    bool mountExpansionCardFromFile(const juce::File& file);
    void ejectExpansionCard();
    bool isExpansionCardMounted() const noexcept { return expansionCardManager.isCardMounted(); }
    const ExpansionCard* getActiveExpansionCard() const noexcept { return expansionCardManager.getActiveCard(); }
    bool loadCardPreset(int cardPresetIndex);

    // Live Hardware Virtual Cartridge Burning (Workflow A)
    bool createNewExpansionCard(const juce::String& cardId,
                                const juce::String& cardName,
                                const juce::String& author,
                                const juce::String& version = "1.0");

    bool savePatchToActiveCard(const juce::String& patchName);
    bool deletePatchFromActiveCard(int presetIndex);

    // Compiles active preset bank into a single .s116card container file
    bool exportCurrentBankToCard(const juce::File& destinationFile,
                                 const juce::String& cardId,
                                 const juce::String& cardName,
                                 const juce::String& author,
                                 const juce::String& version = "1.0");

    // XML Serialization with Theme Decoupling & Ghost-Busting
    juce::String createPatchXml (const juce::String& patchName, bool isProjectState = false);
    void loadPatchXml (const juce::String& xmlString, bool isProjectState = false);
    
    void loadFactoryPreset (int presetIndex);

    // Step Audition with Universal Parameter Lock Engagement
    void auditionStep (int stepIndex, int part);
    void stopAudition (int part);

    // Base Patch Preservation for Elektron-Style Parameter Locking
    void snapshotBasePatch (int part) noexcept;
    void restoreBasePatch (int part) noexcept;
    const TimbrePatch& getBasePatch (int part) const noexcept {
        return basePatches[static_cast<size_t>(std::clamp(part, 0, 1))];
    }

    bool isVoiceActive (int voiceIndex) const {
        if (voiceIndex >= 0 && voiceIndex < VoiceManager::NUM_VOICES)
            return voiceManager.getVoices()[static_cast<size_t>(voiceIndex)].isActive();
        return false;
    }

    std::vector<int> getChordNotes (int root, int chordType) const {
        std::vector<int> notes;
        notes.reserve(4);
        notes.push_back(root);
        switch (chordType) {
            case 0: notes.push_back(root + 4); notes.push_back(root + 7); break;
            case 1: notes.push_back(root + 3); notes.push_back(root + 7); break;
            case 2: notes.push_back(root + 4); notes.push_back(root + 7); notes.push_back(root + 11); break;
            case 3: notes.push_back(root + 3); notes.push_back(root + 7); notes.push_back(root + 10); break;
            case 4: notes.push_back(root + 4); notes.push_back(root + 7); notes.push_back(root + 10); break;
            case 5: notes.push_back(root + 5); notes.push_back(root + 7); break;
            case 6: notes.push_back(root + 3); notes.push_back(root + 6); break;
            case 7: notes.push_back(root + 12); break;
            case 8: notes.push_back(root + 7); break;
            default: break;
        }
        return notes;
    }

    // =========================================================================
    // Bi-Timbral Layer Controls (Part 0 = Lower / Trk A, Part 1 = Upper / Trk B)
    // =========================================================================
    int getActiveEditPart() const noexcept { return activeEditPart; }
    void setActiveEditPart (int part);

    TimbrePatch& getPartPatch (int part) noexcept {
        return partPatches[static_cast<size_t>(std::clamp(part, 0, 1))];
    }
    const TimbrePatch& getPartPatch (int part) const noexcept {
        return partPatches[static_cast<size_t>(std::clamp(part, 0, 1))];
    }

    void captureCurrentPartFromApvts (int part);
    void applyPartToApvts (int part);

    // Performance Controls Direct Dispatch (UI & MIDI Hardware Convergence)
    void setModWheel (float val, int part = -1) noexcept;
    void setPitchBend (float val, int part = -1) noexcept;
    float getModWheel() const noexcept { return currentModWheel; }
    float getPitchBend() const noexcept { return currentPitchBend; }

    // Sequencer Track Gate & Swing Link Configuration
    void setSeqGateLink (bool shouldLink) noexcept { seqGateLinked = shouldLink; }
    bool isSeqGateLinked() const noexcept { return seqGateLinked; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    VoiceManager voiceManager;
    SequencerEngine sequencer;
    OscilloscopeQueue oscilloscopeQueue;
    BiTimbralModMatrix modMatrix;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::array<TimbrePatch, 2> partPatches;
    std::array<TimbrePatch, 2> basePatches;
    int activeEditPart = 0; // 0 = Part A (Lower), 1 = Part B (Upper)
    juce::String currentPresetName { "Init Patch" };
    int currentPresetIndex = 0;

    std::array<LFO, 2> lfo1;
    std::array<LFO, 2> lfo2;
    std::array<LFO, 2> lfo3;

    StereoChorus chorus;
    StereoDelay delay;
    StereoReverb reverb;

    juce::MidiMessageCollector midiCollector;
    std::vector<std::unique_ptr<juce::MidiInput>> activeMidiInputs;

    std::array<std::array<bool, 128>, 2> physicalKeysHeld;
    std::array<std::array<float, 128>, 2> physicalKeyVelocities;

    std::array<std::atomic<float>*, 16> vPanParams { nullptr };
    std::array<std::atomic<float>*, 16> vTuneParams { nullptr };

    std::array<int, 2> lastArpPlayingNote { -1, -1 };
    std::array<int, 2> lastSeqPlayingNote { -1, -1 };
    bool lastHostPlaying = false;
    std::array<bool, 2> lastArpOn { false, false };

    std::array<float, 2> currentLfo2Amount { 0.0f, 0.0f };
    std::array<float, 2> currentLfo3Amount { 0.0f, 0.0f };
    std::array<float, 2> lastLfo2Val { 0.0f, 0.0f };
    std::array<float, 2> lastLfo3Val { 0.0f, 0.0f };

    float currentPitchBend = 0.0f;
    float currentModWheel = 0.0f;
    float currentAftertouch = 0.0f;
    float currentExpression = 1.0f;
    float currentBreath = 0.0f;
    float lastMidiNotePlayed = 60.0f;
    float lastMidiVelocity = 0.8f;

    float smoothedMidiNote = 60.0f;
    float smoothedVelocity = 0.8f;
    float smoothedPitchBend = 0.0f;
    float smoothedModWheel = 0.0f;
    float smoothedAftertouch = 0.0f;
    float smoothedExpression = 1.0f;
    float smoothedBreath = 0.0f;

    bool seqGateLinked = false;

    std::array<std::array<float, LockParamID::Lock_ParamCount>, 2> currentPLockValues {};

    std::array<float, 2> currentPLockCut { 0.0f, 0.0f };
    std::array<float, 2> currentPLockRes { 0.0f, 0.0f };
    std::array<float, 2> currentPLockMorph { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Simple116AudioProcessor)
};

using Simple106AudioProcessor = Simple116AudioProcessor;
using S116AudioProcessor = Simple116AudioProcessor;