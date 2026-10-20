#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/PresetBank.h"
#include <cmath>

Simple116AudioProcessor::Simple116AudioProcessor()
: AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Initialize ValueTree so DAW host successfully saves/restores parameter state
    apvts.state = juce::ValueTree("S-116Patch");

    activeEditPart = 0;
    modMatrix.activePartPtr = &activeEditPart;

    for (int p = 0; p < 2; ++p) {
        physicalKeysHeld[static_cast<size_t>(p)].fill(false);
        physicalKeyVelocities[static_cast<size_t>(p)].fill(0.0f);
        lastArpPlayingNote[static_cast<size_t>(p)] = -1;
        lastSeqPlayingNote[static_cast<size_t>(p)] = -1;
        lastArpOn[static_cast<size_t>(p)] = false;
        currentLfo2Amount[static_cast<size_t>(p)] = 0.0f;
        currentLfo3Amount[static_cast<size_t>(p)] = 0.0f;
        lastLfo2Val[static_cast<size_t>(p)] = 0.0f;
        lastLfo3Val[static_cast<size_t>(p)] = 0.0f;

        currentPLockCut[static_cast<size_t>(p)] = 3000.0f;
        currentPLockRes[static_cast<size_t>(p)] = 0.0f;
        currentPLockMorph[static_cast<size_t>(p)] = 0.25f;

        currentPLockValues[static_cast<size_t>(p)].fill(0.0f);
    }

    for (int i = 0; i < 16; ++i) {
        vPanParams[static_cast<size_t>(i)] = apvts.getRawParameterValue("vPan" + juce::String(i + 1));
        vTuneParams[static_cast<size_t>(i)] = apvts.getRawParameterValue("vTune" + juce::String(i + 1));
    }

    // Auto-discover any expansion cards in Documents/Fadedlimes/S-116/Expansions
    expansionCardManager.scanExpansionsFolder();

    // Seed the theme parameters with the colours the user last chose (they live in
    // the global settings file). A restored session overwrites these again, so the
    // VST session colour always wins once a project has been loaded.
    {
        auto& skin = SkinManager::getInstance();

        auto seed = [this] (const char* paramId, int value)
        {
            if (auto* p = apvts.getParameter (paramId))
                p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float>(value)));
        };

        seed ("guiTheme", static_cast<int>(skin.getChassisTheme()));
        seed ("ledTheme", static_cast<int>(skin.getLEDTheme()));
        seed ("crtTheme", static_cast<int>(skin.getCRTTheme()));
    }

    // Initialize with clean bi-timbral Mod Matrices and analog INIT baseline
    modMatrix.reset();
    loadFactoryPreset(0);

    basePatches[0] = partPatches[0];
    basePatches[1] = partPatches[1];
}

Simple116AudioProcessor::~Simple116AudioProcessor() {
    releaseResources();
}

const juce::String Simple116AudioProcessor::getProgramName(int index) {
    auto presets = getAvailablePresets();
    if (index >= 0 && index < presets.size())
        return presets[index];
    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout Simple116AudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // =========================================================================
    // True Exponential Logarithmic Ranges (Constant Octaves per Knob Angle)
    // =========================================================================
    auto makeLogRange = [](float start, float end) {
        return juce::NormalisableRange<float>(
            start, end,
            [](float s, float e, float norm) {
                return s * std::pow(e / s, std::clamp(norm, 0.0f, 1.0f));
            },
            [](float s, float e, float val) {
                return std::log(std::clamp(val, s, e) / s) / std::log(e / s);
            },
            [](float s, float e, float val) {
                return std::clamp(val, s, e);
            }
        );
    };

    auto cutoffRange = makeLogRange(30.0f, 20000.0f);
    auto hpfRange    = makeLogRange(10.0f, 1000.0f);

    juce::NormalisableRange<float> attackRange(0.001f, 3.0f);
    attackRange.setSkewForCentre(0.20f);

    juce::NormalisableRange<float> decayRange(0.001f, 3.0f);
    decayRange.setSkewForCentre(0.30f);

    juce::NormalisableRange<float> releaseRange(0.001f, 5.0f);
    releaseRange.setSkewForCentre(0.40f);

    juce::NormalisableRange<float> auxAttackRange(0.001f, 5.0f);
    auxAttackRange.setSkewForCentre(0.25f);

    juce::NormalisableRange<float> auxDecayRange(0.001f, 8.0f);
    auxDecayRange.setSkewForCentre(0.50f);

    juce::NormalisableRange<float> auxReleaseRange(0.001f, 10.0f);
    auxReleaseRange.setSkewForCentre(0.60f);

    // Header Unison & Master Performance
    params.push_back(std::make_unique<juce::AudioParameterFloat>("unisonDetune", "Unison Detune", 0.0f, 100.0f, 18.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("unisonSpread", "Unison Spread", 0.0f, 1.0f, 0.85f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("masterVolume", "Master Volume", 0.0f, 1.0f, 0.85f));

    // DCO 1
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco1Morph", "DCO1 Morph", 0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco1PWM", "DCO1 PWM", 0.05f, 0.95f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("dco1Octave", "DCO1 Octave",
                                                                  juce::StringArray{"32'", "16'", "8'", "4'"}, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco1Fold", "DCO1 Fold", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco1SawMix", "Saw / Supersaw Mix", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco1SawDetune", "Supersaw Detune", 0.0f, 1.0f, 0.50f));

    // DCO 2
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco2Morph", "DCO2 Morph", 0.0f, 1.0f, 0.50f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco2PWM", "DCO2 PWM", 0.05f, 0.95f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("dco2Semi", "DCO2 Semi", -24, 24, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco2Cents", "DCO2 Cents", -50.0f, 50.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco2Fold", "DCO2 Fold", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dco2Level", "DCO2 Level", 0.0f, 1.0f, 0.0f));

    // Hard Sync & Cross-Mod / FM
    params.push_back(std::make_unique<juce::AudioParameterBool>("syncMode", "Hard Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("xmodAmount", "FM Amount", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("xmodMode", "X-Mod Type",
                                                                  juce::StringArray{"Linear FM", "Exp FM"}, 0));

    // Sub & Noise
    params.push_back(std::make_unique<juce::AudioParameterChoice>("subOctave", "Sub Octave",
                                                                  juce::StringArray{"-1", "-2"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("subLevel", "Sub Level", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("subWaveform", "Sub Waveform",
                                                                  juce::StringArray{"Square", "Sine"}, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("noiseLevel", "Noise Level", 0.0f, 1.0f, 0.0f));

    // Filter 1
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hpfCutoff", "HPF Cutoff", hpfRange, 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lpfCutoff", "LPF Cutoff", cutoffRange, 3000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lpfRes", "LPF Resonance", 0.0f, 0.98f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("envMod", "Filter Env Mod", -1.0f, 1.0f, 0.0f));

    // Filter 2
    params.push_back(std::make_unique<juce::AudioParameterBool>("filterRouting", "Filter Routing", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("filterBalance", "Filter Balance", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("f2Cutoff", "F2 Cutoff", cutoffRange, 3000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("f2Res", "F2 Resonance", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("f2Mode", "F2 Mode",
                                                                  juce::StringArray{"Lowpass", "Bandpass", "Highpass", "Notch", "Peak"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("f2EnvDepth", "F2 Env Depth", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("f2KeyTrack", "F2 Key Track", 0.0f, 1.0f, 0.0f));

    // Envelopes
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ampAttack", "Amp Attack", attackRange, 0.001f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ampDecay", "Amp Decay", decayRange, 0.10f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ampSustain", "Amp Sustain", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ampRelease", "Amp Release", releaseRange, 0.10f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("filtAttack", "Filt Attack", attackRange, 0.001f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("filtDecay", "Filt Decay", decayRange, 0.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("filtSustain", "Filt Sustain", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("filtRelease", "Filt Release", releaseRange, 0.10f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("auxAttack", "Aux Attack", auxAttackRange, 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("auxDecay", "Aux Decay", auxDecayRange, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("auxSustain", "Aux Sustain", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("auxRelease", "Aux Release", auxReleaseRange, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("auxToPitch", "Aux to Pitch", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("auxToFilter1", "Aux to Filter 1", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("auxToFilter2", "Aux to Filter 2", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("auxToFold", "Aux to Fold", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("auxToPWM", "Aux to PWM", -1.0f, 1.0f, 0.0f));

    // LFOs
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfo1Rate", "LFO 1 Rate", 0.1f, 25.0f, 2.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("lfo1Shape", "LFO 1 Shape",
                                                                  juce::StringArray{"Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H"}, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfo1ToFilter", "LFO 1 to Filter", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfo1ToPitch", "LFO 1 to Pitch", 0.0f, 1.0f, 0.0f));

    const juce::StringArray modTargets = {
        "None", "LPF Cutoff", "LPF Reso", "HPF Cutoff", "Env Mod",
        "Filt Attack", "Filt Decay", "Filt Sustain", "Filt Release",
        "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release",
        "DCO1 Morph", "DCO1 PWM", "DCO2 Morph", "DCO2 PWM", "DCO2 Semi", "DCO2 Detune",
        "Sub Level", "Noise Level", "Glide Time", "Master Volume",
        "DCO1 Fold", "X-Mod Amount"
    };

    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfo2Rate", "LFO 2 Rate", 0.05f, 25.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("lfo2Shape", "LFO 2 Shape",
                                                                  juce::StringArray{"Sine", "Triangle", "S&H"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfo2Amount", "LFO 2 Amount", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("lfo2Target", "LFO 2 Target", modTargets, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfo3Rate", "LFO 3 Rate", 0.05f, 25.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("lfo3Shape", "LFO 3 Shape",
                                                                  juce::StringArray{"Sine", "Triangle", "S&H"}, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfo3Amount", "LFO 3 Amount", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("lfo3Target", "LFO 3 Target", modTargets, 0));

    // Voice Variations (1 to 16)
    params.push_back(std::make_unique<juce::AudioParameterChoice>("voiceVarMode", "Var Mode", juce::StringArray{"PANNING", "TUNING"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("cycleMode", "Voice Cycle", false));

    for (int i = 1; i <= 16; ++i) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>("vPan" + juce::String(i), "Voice " + juce::String(i) + " Pan", -1.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("vTune" + juce::String(i), "Voice " + juce::String(i) + " Tune", -50.0f, 50.0f, 0.0f));
    }

    const juce::StringArray playModeChoices = {
        "Poly 8 Bi-Timbral",
        "Poly 16 [Single]",
        "Round Robin",
        "Mono Legato",
        "Mono Standard",
        "Unison 2x",
        "Unison 4x",
        "Unison 8x",
        "Unison 16x"
    };

    params.push_back(std::make_unique<juce::AudioParameterChoice>("playMode", "Play Mode", playModeChoices, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("glideTime", "Glide Time", 0.001f, 2.0f, 0.05f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("glideEnable", "Portamento", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>("chordEnable", "Chord Mode", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("chordType", "Chord Type",
                                                                  juce::StringArray{"Major", "Minor", "Maj7", "Min7", "Dom7", "Sus4", "Dim", "Octave", "Power (5th)"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>("arpEnable", "Arp Mode", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpMode", "Arp Type",
                                                                  juce::StringArray{"Up", "Down", "Up/Down", "Down/Up", "Random", "As Played", "Chord"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>("syncToHost", "Sync to Host", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("internalBpm", "Internal BPM", 20.0f, 300.0f, 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("seqPlay", "Seq Play", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("seqRec", "Seq Rec", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("seqDirection", "Seq Direction",
                                                                  juce::StringArray{"Forward", "Reverse", "Ping-Pong", "Random"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("seqDivision", "Seq Division",
                                                                  juce::StringArray{"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"}, 3));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("seqSwing", "Sequencer Swing", 0.0f, 0.75f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("seqGate", "Sequencer Gate", 0.05f, 0.98f, 0.75f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>("seqPages", "Seq Pages",
                                                                  juce::StringArray{"1 PG [16]", "2 PG [32]", "3 PG [48]", "4 PG [64]"}, 3));

    // Themes
    // NOTE: The choice order must match the LEDTheme / CRTTheme enumerations in
    // UI/SkinManager.h exactly, so that the stored index can be cast directly.
    juce::StringArray ledThemeNames;
    for (int i = 0; i < static_cast<int>(LEDTheme::Count); ++i)
        ledThemeNames.add (SkinManager::getLEDThemeName (static_cast<LEDTheme>(i)));

    juce::StringArray crtThemeNames;
    for (int i = 0; i < static_cast<int>(CRTTheme::Count); ++i)
        crtThemeNames.add (SkinManager::getCRTThemeOptionName (static_cast<CRTTheme>(i)));

    params.push_back(std::make_unique<juce::AudioParameterChoice>("guiTheme", "Theme",
                                                                  juce::StringArray{"Classic Silver", "Midnight Blue", "Vintage Wood", "Stealth Dark"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("ledTheme", "LED Colour", ledThemeNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("crtTheme", "Scope Colour", crtThemeNames, 0));

    // Master FX Suite
    params.push_back(std::make_unique<juce::AudioParameterChoice>("chorusMode", "Chorus",
                                                                  juce::StringArray{"Off", "Chorus I", "Chorus II", "Chorus I+II"}, 0));

    const juce::StringArray syncTimings = {
        "1/16", "1/16T", "1/16D", "1/8", "1/8T", "1/8D", "1/4", "1/4T", "1/4D", "1/2", "1/2T", "1/2D", "1/1"
    };

    params.push_back(std::make_unique<juce::AudioParameterBool>("delaySyncEnable", "Delay Sync Enable", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("delaySync", "Delay Sync", syncTimings, 6));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("delayTime", "Delay Time", 0.01f, 1.95f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("delayFeedback", "Delay Feedback", 0.0f, 0.95f, 0.40f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("delayDamp", "Delay Damp", 0.0f, 0.95f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("delayMix", "Delay Mix", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("delayPingPong", "Delay Ping-Pong", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("reverbSize", "Reverb Size", 0.0f, 1.0f, 0.65f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("reverbDamp", "Reverb Damp", 0.0f, 1.0f, 0.30f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("reverbMix", "Reverb Mix", 0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>("fxRouting", "FX Routing",
                                                                  juce::StringArray{"Both", "Track A Only", "Track B Only"}, 0));

    return { params.begin(), params.end() };
}

// =============================================================================
// Bi-Timbral Part Capture and APVTS Dispatch
// =============================================================================
void Simple116AudioProcessor::captureCurrentPartFromApvts(int part) {
    if (part < 0 || part > 1) return;
    auto& p = partPatches[static_cast<size_t>(part)];

    auto getVal = [this](const juce::String& id, float def) -> float {
        auto* raw = apvts.getRawParameterValue(id);
        return raw != nullptr ? raw->load() : def;
    };

    if (!sequencer.isRecording) {
        p.dco1Morph     = getVal("dco1Morph", 0.25f);
        p.dco1PWM       = getVal("dco1PWM", 0.50f);
        p.dco1Fold      = getVal("dco1Fold", 0.0f);
        p.dco1SawMix    = getVal("dco1SawMix", 0.0f);
        p.dco1SawDetune = getVal("dco1SawDetune", 0.50f);

        p.dco2Morph   = getVal("dco2Morph", 0.50f);
        p.dco2PWM     = getVal("dco2PWM", 0.50f);
        p.dco2Semi    = static_cast<int>(getVal("dco2Semi", 0.0f));
        p.dco2Cents   = getVal("dco2Cents", 0.0f);
        p.dco2Fold    = getVal("dco2Fold", 0.0f);
        p.dco2Level   = getVal("dco2Level", 0.0f);

        p.xmodAmount  = getVal("xmodAmount", 0.0f);
        p.xmodMode    = static_cast<int>(getVal("xmodMode", 0.0f));

        p.subLevel    = getVal("subLevel", 0.0f);
        p.noiseLevel  = getVal("noiseLevel", 0.0f);

        p.hpfCutoff   = getVal("hpfCutoff", 20.0f);
        p.lpfCutoff   = getVal("lpfCutoff", 3000.0f);
        p.lpfRes      = getVal("lpfRes", 0.0f);
        p.envMod      = getVal("envMod", 0.0f);

        p.filterBalance = getVal("filterBalance", 0.50f);
        p.f2Cutoff    = getVal("f2Cutoff", 3000.0f);
        p.f2Res       = getVal("f2Res", 0.0f);
        p.f2EnvDepth  = getVal("f2EnvDepth", 0.0f);

        p.ampAttack   = getVal("ampAttack", 0.001f);
        p.ampDecay    = getVal("ampDecay", 0.10f);
        p.ampSustain  = getVal("ampSustain", 1.0f);
        p.ampRelease  = getVal("ampRelease", 0.10f);

        p.filtAttack  = getVal("filtAttack", 0.001f);
        p.filtDecay   = getVal("filtDecay", 0.20f);
        p.filtSustain = getVal("filtSustain", 1.0f);
        p.filtRelease = getVal("filtRelease", 0.10f);

        p.auxToPitch   = getVal("auxToPitch", 0.0f);
        p.auxToFilter1 = getVal("auxToFilter1", 0.0f);
        p.auxToFilter2 = getVal("auxToFilter2", 0.0f);
        p.auxToFold    = getVal("auxToFold", 0.0f);
        p.auxToPWM     = getVal("auxToPWM", 0.0f);
    }

    p.dco1Octave  = static_cast<int>(getVal("dco1Octave", 2.0f));
    p.syncMode    = getVal("syncMode", 0.0f) > 0.5f;
    p.subOctave   = static_cast<int>(getVal("subOctave", 0.0f));
    p.subWaveform = static_cast<int>(getVal("subWaveform", 1.0f));

    p.filterRouting = getVal("filterRouting", 0.0f) > 0.5f;
    p.f2Mode      = static_cast<int>(getVal("f2Mode", 0.0f));
    p.f2KeyTrack  = getVal("f2KeyTrack", 0.0f);

    p.auxAttack   = getVal("auxAttack", 0.01f);
    p.auxDecay    = getVal("auxDecay", 0.50f);
    p.auxSustain  = getVal("auxSustain", 0.0f);
    p.auxRelease  = getVal("auxRelease", 0.50f);

    p.lfo1Rate     = getVal("lfo1Rate", 2.0f);
    p.lfo1Shape    = static_cast<int>(getVal("lfo1Shape", 1.0f));
    p.lfo1ToFilter = getVal("lfo1ToFilter", 0.0f);
    p.lfo1ToPitch  = getVal("lfo1ToPitch", 0.0f);

    p.lfo2Rate     = getVal("lfo2Rate", 1.0f);
    p.lfo2Shape    = static_cast<int>(getVal("lfo2Shape", 0.0f));
    p.lfo2Amount   = getVal("lfo2Amount", 0.0f);
    p.lfo2Target   = static_cast<int>(getVal("lfo2Target", 0.0f));

    p.lfo3Rate     = getVal("lfo3Rate", 0.5f);
    p.lfo3Shape    = static_cast<int>(getVal("lfo3Shape", 1.0f));
    p.lfo3Amount   = getVal("lfo3Amount", 0.0f);
    p.lfo3Target   = static_cast<int>(getVal("lfo3Target", 0.0f));

    p.playMode     = static_cast<int>(getVal("playMode", 1.0f));
    p.glideTime    = getVal("glideTime", 0.05f);
    p.glideEnable  = getVal("glideEnable", 0.0f) > 0.5f;
    p.chordEnable  = getVal("chordEnable", 0.0f) > 0.5f;
    p.chordType    = static_cast<int>(getVal("chordType", 0.0f));
}

void Simple116AudioProcessor::applyPartToApvts(int part) {
    if (part < 0 || part > 1) return;
    const auto& p = partPatches[static_cast<size_t>(part)];

    auto setVal = [this](const juce::String& id, float val) {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(val));
    };

    setVal("dco1Morph", p.dco1Morph);
    setVal("dco1PWM", p.dco1PWM);
    setVal("dco1Octave", static_cast<float>(p.dco1Octave));
    setVal("dco1Fold", p.dco1Fold);
    setVal("dco1SawMix", p.dco1SawMix);
    setVal("dco1SawDetune", p.dco1SawDetune);

    setVal("dco2Morph", p.dco2Morph);
    setVal("dco2PWM", p.dco2PWM);
    setVal("dco2Semi", static_cast<float>(p.dco2Semi));
    setVal("dco2Cents", p.dco2Cents);
    setVal("dco2Fold", p.dco2Fold);
    setVal("dco2Level", p.dco2Level);

    setVal("syncMode", p.syncMode ? 1.0f : 0.0f);
    setVal("xmodAmount", p.xmodAmount);
    setVal("xmodMode", static_cast<float>(p.xmodMode));

    setVal("subOctave", static_cast<float>(p.subOctave));
    setVal("subLevel", p.subLevel);
    setVal("subWaveform", static_cast<float>(p.subWaveform));
    setVal("noiseLevel", p.noiseLevel);

    setVal("hpfCutoff", p.hpfCutoff);
    setVal("lpfCutoff", p.lpfCutoff);
    setVal("lpfRes", p.lpfRes);
    setVal("envMod", p.envMod);

    setVal("filterRouting", p.filterRouting ? 1.0f : 0.0f);
    setVal("filterBalance", p.filterBalance);
    setVal("f2Cutoff", p.f2Cutoff);
    setVal("f2Res", p.f2Res);
    setVal("f2Mode", static_cast<float>(p.f2Mode));
    setVal("f2EnvDepth", p.f2EnvDepth);
    setVal("f2KeyTrack", p.f2KeyTrack);

    setVal("ampAttack", p.ampAttack);
    setVal("ampDecay", p.ampDecay);
    setVal("ampSustain", p.ampSustain);
    setVal("ampRelease", p.ampRelease);

    setVal("filtAttack", p.filtAttack);
    setVal("filtDecay", p.filtDecay);
    setVal("filtSustain", p.filtSustain);
    setVal("filtRelease", p.filtRelease);

    setVal("auxAttack", p.auxAttack);
    setVal("auxDecay", p.auxDecay);
    setVal("auxSustain", p.auxSustain);
    setVal("auxRelease", p.auxRelease);

    setVal("auxToPitch", p.auxToPitch);
    setVal("auxToFilter1", p.auxToFilter1);
    setVal("auxToFilter2", p.auxToFilter2);
    setVal("auxToFold", p.auxToFold);
    setVal("auxToPWM", p.auxToPWM);

    setVal("lfo1Rate", p.lfo1Rate);
    setVal("lfo1Shape", static_cast<float>(p.lfo1Shape));
    setVal("lfo1ToFilter", p.lfo1ToFilter);
    setVal("lfo1ToPitch", p.lfo1ToPitch);

    setVal("lfo2Rate", p.lfo2Rate);
    setVal("lfo2Shape", static_cast<float>(p.lfo2Shape));
    setVal("lfo2Amount", p.lfo2Amount);
    setVal("lfo2Target", static_cast<float>(p.lfo2Target));

    setVal("lfo3Rate", p.lfo3Rate);
    setVal("lfo3Shape", static_cast<float>(p.lfo3Shape));
    setVal("lfo3Amount", p.lfo3Amount);
    setVal("lfo3Target", static_cast<float>(p.lfo3Target));

    setVal("playMode", static_cast<float>(p.playMode));
    setVal("glideTime", p.glideTime);
    setVal("glideEnable", p.glideEnable ? 1.0f : 0.0f);
    setVal("chordEnable", p.chordEnable ? 1.0f : 0.0f);
    setVal("chordType", static_cast<float>(p.chordType));
}

void Simple116AudioProcessor::setActiveEditPart(int part) {
    if (part < 0 || part > 1 || part == activeEditPart)
        return;

    captureCurrentPartFromApvts(activeEditPart);
    activeEditPart = part;
    applyPartToApvts(activeEditPart);
}

void Simple116AudioProcessor::snapshotBasePatch(int part) noexcept {
    if (part < 0 || part > 1) return;
    captureCurrentPartFromApvts(part);
    basePatches[static_cast<size_t>(part)] = partPatches[static_cast<size_t>(part)];
}

void Simple116AudioProcessor::restoreBasePatch(int part) noexcept {
    if (part < 0 || part > 1) return;
    partPatches[static_cast<size_t>(part)] = basePatches[static_cast<size_t>(part)];
    applyPartToApvts(part);
}

void Simple116AudioProcessor::auditionStep(int stepIndex, int part) {
    const int p = std::clamp(part, 0, 1);
    
    voiceManager.allNotesOff(p);

    auto& track = sequencer.getTrack(static_cast<TrackID>(p));
    track.applyStepLocks(stepIndex);
    const auto& step = track.getStep(stepIndex);
    const auto& patch = partPatches[static_cast<size_t>(p)];

    currentPLockCut[static_cast<size_t>(p)]   = step.locks.isLocked(Lock_LPFCutoff) ? step.locks.values[Lock_LPFCutoff] : patch.lpfCutoff;
    currentPLockRes[static_cast<size_t>(p)]   = step.locks.isLocked(Lock_LPFRes)    ? step.locks.values[Lock_LPFRes]    : patch.lpfRes;
    currentPLockMorph[static_cast<size_t>(p)] = step.locks.isLocked(Lock_DCO1Morph) ? step.locks.values[Lock_DCO1Morph] : patch.dco1Morph;

    for (size_t k = 0; k < ParamLocks::NUM_PARAMS; ++k) {
        const auto id = static_cast<LockParamID>(k);
        if (step.locks.isLocked(id)) {
            currentPLockValues[static_cast<size_t>(p)][k] = step.locks.values[k];
        }
    }

    bool playedChord = false;
    for (int cn : step.chordNotes) {
        if (cn >= 0) {
            voiceManager.noteOn(cn, step.velocity, p);
            playedChord = true;
        }
    }
    if (!playedChord) {
        if (patch.chordEnable) {
            auto expNotes = getChordNotes(step.note, patch.chordType);
            for (int cn : expNotes) {
                voiceManager.noteOn(cn, step.velocity, p);
            }
        } else {
            voiceManager.noteOn(step.note, step.velocity, p);
        }
    }
}

void Simple116AudioProcessor::stopAudition(int part) {
    const int p = std::clamp(part, 0, 1);
    voiceManager.allNotesOff(p);
    auto& track = sequencer.getTrack(static_cast<TrackID>(p));
    track.activeLockMask = 0;
    track.activeLockValues.fill(0.0f);
    track.activeLockCutoff = -1.0f;
    track.activeLockReso   = -1.0f;
    track.activeLockMorph  = -1.0f;
}

// =============================================================================
// Performance Controls Direct Dispatch (UI & MIDI Hardware Convergence)
// =============================================================================
void Simple116AudioProcessor::setModWheel(float val, int part) noexcept {
    currentModWheel = std::clamp(val, 0.0f, 1.0f);
    smoothedModWheel = currentModWheel;
    voiceManager.setModWheel(currentModWheel, part);
}

void Simple116AudioProcessor::setPitchBend(float val, int part) noexcept {
    currentPitchBend = std::clamp(val, -1.0f, 1.0f);
    smoothedPitchBend = currentPitchBend;
    voiceManager.setPitchBend(currentPitchBend, part);
}

juce::File Simple116AudioProcessor::getPresetsDirectory() const {
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Fadedlimes")
        .getChildFile("S-116")
        .getChildFile("Presets");
    if (!dir.exists()) dir.createDirectory();
    return dir;
}

juce::StringArray Simple116AudioProcessor::getAvailablePresets() const {
    juce::StringArray presets;

    if (expansionCardManager.isCardMounted()) {
        presets = expansionCardManager.getActiveCardPresetNames();
    } else {
        presets = PresetBank::getPresetNames();
    }

    const auto dir = getPresetsDirectory();
    const auto files = dir.findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& f : files) {
        const auto name = f.getFileNameWithoutExtension();
        if (!presets.contains(name))
            presets.add(name);
    }
    return presets;
}

bool Simple116AudioProcessor::saveUserPreset(const juce::String& presetName) {
    const auto cleanName = presetName.trim();
    if (cleanName.isEmpty())
        return false;

    const auto dir = getPresetsDirectory();
    const auto file = dir.getChildFile(cleanName + ".xml");

    const auto xmlStr = createPatchXml(cleanName, false);
    if (file.replaceWithText(xmlStr)) {
        currentPresetName = cleanName;
        return true;
    }
    return false;
}

bool Simple116AudioProcessor::loadPresetByName(const juce::String& presetName) {
    const auto cleanName = presetName.trim();

    if (expansionCardManager.isCardMounted()) {
        auto cardPresetNames = expansionCardManager.getActiveCardPresetNames();
        int cardIdx = cardPresetNames.indexOf(cleanName);
        if (cardIdx >= 0)
            return loadCardPreset(cardIdx);
    }

    auto presets = PresetBank::getPresetNames();
    const int idx = presets.indexOf(cleanName);
    if (idx >= 0) {
        loadFactoryPreset(idx);
        return true;
    }

    const auto file = getPresetsDirectory().getChildFile(cleanName + ".xml");
    if (file.existsAsFile()) {
        const auto xmlStr = file.loadFileAsString();
        loadPatchXml(xmlStr, false);
        currentPresetName = cleanName;
        currentPresetIndex = -1;
        return true;
    }
    return false;
}

// =============================================================================
// Expansion Card Virtual Cartridge Actions
// =============================================================================
bool Simple116AudioProcessor::mountExpansionCard(int cardIndex) {
    bool ok = expansionCardManager.mountCard(cardIndex);
    if (ok) {
        loadCardPreset(0);
    }
    return ok;
}

bool Simple116AudioProcessor::mountExpansionCardFromFile(const juce::File& file) {
    bool ok = expansionCardManager.mountCardFromFile(file);
    if (ok) {
        loadCardPreset(0);
    }
    return ok;
}

void Simple116AudioProcessor::ejectExpansionCard() {
    expansionCardManager.ejectCard();
    loadFactoryPreset(0);
}

bool Simple116AudioProcessor::loadCardPreset(int cardPresetIndex) {
    if (!expansionCardManager.isCardMounted())
        return false;

    auto xml = expansionCardManager.getActiveCardPresetXml(cardPresetIndex);
    if (xml.isNotEmpty()) {
        loadPatchXml(xml, false);
        auto names = expansionCardManager.getActiveCardPresetNames();
        if (cardPresetIndex >= 0 && cardPresetIndex < names.size())
            currentPresetName = names[cardPresetIndex];

        currentPresetIndex = -1;
        return true;
    }
    return false;
}

bool Simple116AudioProcessor::createNewExpansionCard(const juce::String& cardId,
                                                    const juce::String& cardName,
                                                    const juce::String& author,
                                                    const juce::String& version) {
    // Snapshot active synth state to seed the initial patch on the blank card
    const auto seedPatchXml = createPatchXml("01 INIT " + cardId.trim().toUpperCase(), false);

    bool ok = expansionCardManager.createNewBlankCard(cardId, cardName, author, seedPatchXml, version);
    if (ok) {
        loadCardPreset(0);
    }
    return ok;
}

bool Simple116AudioProcessor::savePatchToActiveCard(const juce::String& patchName) {
    if (!expansionCardManager.isCardMounted())
        return false;

    const auto cleanName = patchName.trim().isEmpty() ? currentPresetName : patchName.trim();
    const auto patchXml  = createPatchXml(cleanName, false);

    bool ok = expansionCardManager.addOrUpdatePresetInActiveCard(cleanName, patchXml);
    if (ok) {
        currentPresetName = cleanName;
        currentPresetIndex = -1;
    }
    return ok;
}

bool Simple116AudioProcessor::deletePatchFromActiveCard(int presetIndex) {
    if (!expansionCardManager.isCardMounted())
        return false;

    bool ok = expansionCardManager.deletePresetFromActiveCard(presetIndex);
    if (ok) {
        int nextIdx = std::max(0, presetIndex - 1);
        loadCardPreset(nextIdx);
    }
    return ok;
}

bool Simple116AudioProcessor::exportCurrentBankToCard(const juce::File& destinationFile,
                                                     const juce::String& cardId,
                                                     const juce::String& cardName,
                                                     const juce::String& author,
                                                     const juce::String& version) {
    std::vector<std::pair<juce::String, juce::String>> patches;

    const auto activePresetName = currentPresetName;
    const auto activePresetXml  = createPatchXml(activePresetName, false);

    if (expansionCardManager.isCardMounted()) {
        auto* card = expansionCardManager.getActiveCard();
        for (const auto& cp : card->presets)
            patches.push_back({ cp.name, cp.xmlData });
    } else {
        auto names = PresetBank::getPresetNames();
        patches.reserve(static_cast<size_t>(names.size()));

        for (int i = 0; i < names.size(); ++i) {
            const auto pName = names[i];
            const auto userFile = getPresetsDirectory().getChildFile(pName + ".xml");

            if (userFile.existsAsFile()) {
                patches.push_back({ pName, userFile.loadFileAsString() });
            } else {
                loadFactoryPreset(i);
                patches.push_back({ pName, createPatchXml(pName, false) });
            }
        }

        loadPatchXml(activePresetXml, false);
        currentPresetName = activePresetName;
    }

    const juce::String b1 = "EXP CARD: " + (cardId.isNotEmpty() ? cardId : "CUSTOM");
    const juce::String b2 = cardName.isNotEmpty() ? cardName : "EXPANSION READY";

    bool success = ExpansionCardManager::exportCardToFile(destinationFile, cardId, cardName, author, version, b1, b2, patches);
    if (success)
        expansionCardManager.scanExpansionsFolder();

    return success;
}

juce::String Simple116AudioProcessor::createPatchXml(const juce::String& patchName, bool isProjectState) {
    captureCurrentPartFromApvts(activeEditPart);

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> root(state.createXml());
    if (root == nullptr) root = std::make_unique<juce::XmlElement>("S-116Patch");
    root->setAttribute("patchName", patchName);

    for (int i = root->getNumChildElements() - 1; i >= 0; --i) {
        auto* child = root->getChildElement(i);
        if (child != nullptr) {
            juce::String tag = child->getTagName();
            if (tag == "PartA" || tag == "PartB" || tag == "Sequencer" || tag == "ModMatrix" ||
                tag == "ModMatrixPartA" || tag == "ModMatrixPartB") {
                root->removeChildElement(child, true);
            } else if (!isProjectState && tag == "PARAM") {
                auto id = child->getStringAttribute("id");
                if (id == "guiTheme" || id == "ledTheme" || id == "crtTheme")
                    root->removeChildElement(child, true);
                else if (id == "seqPlay" || id == "seqRec")
                    child->setAttribute("value", 0.0f);
            } else if (isProjectState && tag == "PARAM") {
                // Transport state is never persisted: a reopened project must come up stopped.
                auto id = child->getStringAttribute("id");
                if (id == "seqPlay" || id == "seqRec")
                    child->setAttribute("value", 0.0f);
            }
        }
    }

    for (int pIdx = 0; pIdx < 2; ++pIdx) {
        auto* partXml = root->createNewChildElement(pIdx == 0 ? "PartA" : "PartB");
        const auto& p = partPatches[static_cast<size_t>(pIdx)];
        partXml->setAttribute("dco1Morph", p.dco1Morph);
        partXml->setAttribute("dco1PWM", p.dco1PWM);
        partXml->setAttribute("dco1Octave", p.dco1Octave);
        partXml->setAttribute("dco1Fold", p.dco1Fold);
        partXml->setAttribute("dco1SawMix", p.dco1SawMix);
        partXml->setAttribute("dco1SawDetune", p.dco1SawDetune);

        partXml->setAttribute("dco2Morph", p.dco2Morph);
        partXml->setAttribute("dco2PWM", p.dco2PWM);
        partXml->setAttribute("dco2Semi", p.dco2Semi);
        partXml->setAttribute("dco2Cents", p.dco2Cents);
        partXml->setAttribute("dco2Fold", p.dco2Fold);
        partXml->setAttribute("dco2Level", p.dco2Level);
        partXml->setAttribute("syncMode", p.syncMode ? 1 : 0);
        partXml->setAttribute("xmodAmount", p.xmodAmount);
        partXml->setAttribute("xmodMode", p.xmodMode);
        partXml->setAttribute("subOctave", p.subOctave);
        partXml->setAttribute("subLevel", p.subLevel);
        partXml->setAttribute("subWaveform", p.subWaveform);
        partXml->setAttribute("noiseLevel", p.noiseLevel);
        partXml->setAttribute("hpfCutoff", p.hpfCutoff);
        partXml->setAttribute("lpfCutoff", p.lpfCutoff);
        partXml->setAttribute("lpfRes", p.lpfRes);
        partXml->setAttribute("envMod", p.envMod);
        partXml->setAttribute("filterRouting", p.filterRouting ? 1 : 0);
        partXml->setAttribute("filterBalance", p.filterBalance);
        partXml->setAttribute("f2Cutoff", p.f2Cutoff);
        partXml->setAttribute("f2Res", p.f2Res);
        partXml->setAttribute("f2Mode", p.f2Mode);
        partXml->setAttribute("f2EnvDepth", p.f2EnvDepth);
        partXml->setAttribute("f2KeyTrack", p.f2KeyTrack);
        partXml->setAttribute("ampAttack", p.ampAttack);
        partXml->setAttribute("ampDecay", p.ampDecay);
        partXml->setAttribute("ampSustain", p.ampSustain);
        partXml->setAttribute("ampRelease", p.ampRelease);
        partXml->setAttribute("filtAttack", p.filtAttack);
        partXml->setAttribute("filtDecay", p.filtDecay);
        partXml->setAttribute("filtSustain", p.filtSustain);
        partXml->setAttribute("filtRelease", p.filtRelease);
        partXml->setAttribute("auxAttack", p.auxAttack);
        partXml->setAttribute("auxDecay", p.auxDecay);
        partXml->setAttribute("auxSustain", p.auxSustain);
        partXml->setAttribute("auxRelease", p.auxRelease);
        partXml->setAttribute("auxToPitch", p.auxToPitch);
        partXml->setAttribute("auxToFilter1", p.auxToFilter1);
        partXml->setAttribute("auxToFilter2", p.auxToFilter2);
        partXml->setAttribute("auxToFold", p.auxToFold);
        partXml->setAttribute("auxToPWM", p.auxToPWM);
        partXml->setAttribute("lfo1Rate", p.lfo1Rate);
        partXml->setAttribute("lfo1Shape", p.lfo1Shape);
        partXml->setAttribute("lfo1ToFilter", p.lfo1ToFilter);
        partXml->setAttribute("lfo1ToPitch", p.lfo1ToPitch);
        partXml->setAttribute("lfo2Rate", p.lfo2Rate);
        partXml->setAttribute("lfo2Shape", p.lfo2Shape);
        partXml->setAttribute("lfo2Amount", p.lfo2Amount);
        partXml->setAttribute("lfo2Target", p.lfo2Target);
        partXml->setAttribute("lfo3Rate", p.lfo3Rate);
        partXml->setAttribute("lfo3Shape", p.lfo3Shape);
        partXml->setAttribute("lfo3Amount", p.lfo3Amount);
        partXml->setAttribute("lfo3Target", p.lfo3Target);
        partXml->setAttribute("playMode", p.playMode);
        partXml->setAttribute("glideTime", p.glideTime);
        partXml->setAttribute("glideEnable", p.glideEnable ? 1 : 0);
        partXml->setAttribute("chordEnable", p.chordEnable ? 1 : 0);
        partXml->setAttribute("chordType", p.chordType);
    }

    auto* seqXml = root->createNewChildElement("Sequencer");
    seqXml->setAttribute("numPages", sequencer.getNumPages());

    for (int t = 0; t < 2; ++t) {
        auto* trkXml = seqXml->createNewChildElement(t == 0 ? "TrackA" : "TrackB");
        const auto& track = sequencer.getTrack(static_cast<TrackID>(t));
        trkXml->setAttribute("length", track.getLength());

        for (int i = 0; i < SequencerTrack::MAX_STEPS; ++i) {
            const auto& step = track.getStep(i);
            auto* stepXml = trkXml->createNewChildElement("Step");
            stepXml->setAttribute("index", i);
            stepXml->setAttribute("note", step.note);
            stepXml->setAttribute("active", step.active ? 1 : 0);
            stepXml->setAttribute("velocity", static_cast<double>(step.velocity));
            stepXml->setAttribute("gateLength", static_cast<double>(step.gateLength));
            stepXml->setAttribute("chord1", step.chordNotes[0]);
            stepXml->setAttribute("chord2", step.chordNotes[1]);
            stepXml->setAttribute("chord3", step.chordNotes[2]);
            stepXml->setAttribute("chord4", step.chordNotes[3]);

            stepXml->setAttribute("lockCut", step.locks.cutoffVal);
            stepXml->setAttribute("lockRes", step.locks.resoVal);
            stepXml->setAttribute("lockMorph", step.locks.morphVal);
            stepXml->setAttribute("hasLockCut", step.locks.hasCutoffLock ? 1 : 0);
            stepXml->setAttribute("hasLockRes", step.locks.hasResoLock ? 1 : 0);
            stepXml->setAttribute("hasLockMorph", step.locks.hasMorphLock ? 1 : 0);

            stepXml->setAttribute("lockMask", juce::String(static_cast<juce::int64>(step.locks.mask)));
            for (size_t k = 0; k < ParamLocks::NUM_PARAMS; ++k) {
                if (step.locks.isLocked(static_cast<LockParamID>(k))) {
                    stepXml->setAttribute("lk_" + juce::String(k), step.locks.values[k]);
                }
            }
        }
    }

    for (int p = 0; p < 2; ++p) {
        auto* mmXml = root->createNewChildElement(p == 0 ? "ModMatrixPartA" : "ModMatrixPartB");
        for (int i = 0; i < ModulationMatrix::NUM_SLOTS; ++i) {
            const auto& slot = modMatrix.parts[static_cast<size_t>(p)].getSlot(i);
            auto* slotXml = mmXml->createNewChildElement("Slot");
            slotXml->setAttribute("index", i);
            slotXml->setAttribute("src", static_cast<int>(slot.source));
            slotXml->setAttribute("dst", static_cast<int>(slot.destination));
            slotXml->setAttribute("amt", static_cast<double>(slot.amount));
            slotXml->setAttribute("via", static_cast<int>(slot.viaSource));
            slotXml->setAttribute("viaAmt", static_cast<double>(slot.viaAmount));
            slotXml->setAttribute("enabled", slot.enabled ? 1 : 0);
        }
    }

    return root->toString();
}

void Simple116AudioProcessor::loadPatchXml(const juce::String& xmlString, bool isProjectState) {
    for (int p = 0; p < 2; ++p) {
        physicalKeysHeld[static_cast<size_t>(p)].fill(false);
        physicalKeyVelocities[static_cast<size_t>(p)].fill(0.0f);
        lastArpPlayingNote[static_cast<size_t>(p)] = -1;
        lastSeqPlayingNote[static_cast<size_t>(p)] = -1;
        currentPLockCut[static_cast<size_t>(p)] = 3000.0f;
        currentPLockRes[static_cast<size_t>(p)] = 0.0f;
        currentPLockMorph[static_cast<size_t>(p)] = 0.25f;
        currentPLockValues[static_cast<size_t>(p)].fill(0.0f);
    }
    sequencer.clearArp();
    sequencer.flushAllTracks(voiceManager);
    voiceManager.allNotesOff(0);
    voiceManager.allNotesOff(1);

    float savedGuiTheme = apvts.getRawParameterValue("guiTheme")->load();
    float savedLedTheme = apvts.getRawParameterValue("ledTheme")->load();
    float savedCrtTheme = apvts.getRawParameterValue("crtTheme") != nullptr
                             ? apvts.getRawParameterValue("crtTheme")->load()
                             : 0.0f;

    auto xml = juce::XmlDocument::parse(xmlString);
    if (xml != nullptr) {
        if (xml->hasAttribute("patchName"))
            currentPresetName = xml->getStringAttribute("patchName");

        std::unique_ptr<juce::XmlElement> partAClone(xml->getChildByName("PartA") ? new juce::XmlElement(*xml->getChildByName("PartA")) : nullptr);
        std::unique_ptr<juce::XmlElement> partBClone(xml->getChildByName("PartB") ? new juce::XmlElement(*xml->getChildByName("PartB")) : nullptr);
        std::unique_ptr<juce::XmlElement> seqClone(xml->getChildByName("Sequencer") ? new juce::XmlElement(*xml->getChildByName("Sequencer")) : nullptr);
        
        std::unique_ptr<juce::XmlElement> modAClone(xml->getChildByName("ModMatrixPartA") ? new juce::XmlElement(*xml->getChildByName("ModMatrixPartA")) : nullptr);
        std::unique_ptr<juce::XmlElement> modBClone(xml->getChildByName("ModMatrixPartB") ? new juce::XmlElement(*xml->getChildByName("ModMatrixPartB")) : nullptr);
        std::unique_ptr<juce::XmlElement> legacyModClone(xml->getChildByName("ModMatrix") ? new juce::XmlElement(*xml->getChildByName("ModMatrix")) : nullptr);

        xml->deleteAllChildElementsWithTagName("PartA");
        xml->deleteAllChildElementsWithTagName("PartB");
        xml->deleteAllChildElementsWithTagName("Sequencer");
        xml->deleteAllChildElementsWithTagName("ModMatrix");
        xml->deleteAllChildElementsWithTagName("ModMatrixPartA");
        xml->deleteAllChildElementsWithTagName("ModMatrixPartB");

        if (!isProjectState) {
            for (int i = xml->getNumChildElements() - 1; i >= 0; --i) {
                auto* child = xml->getChildElement(i);
                if (child != nullptr && child->hasTagName("PARAM")) {
                    auto id = child->getStringAttribute("id");
                    if (id == "guiTheme" || id == "ledTheme" || id == "crtTheme")
                        xml->removeChildElement(child, true);
                    else if (id == "seqPlay" || id == "seqRec")
                        child->setAttribute("value", 0.0f);
                }
            }
        }

        if (xml->hasTagName(apvts.state.getType()) || xml->hasTagName("Parameters") ||
            xml->hasTagName("S-116Patch") || xml->hasTagName("Simple116Patch") || xml->hasTagName("Simple106Patch")) {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
        }

        if (!isProjectState) {
            if (auto* pGui = apvts.getParameter("guiTheme"))
                pGui->setValueNotifyingHost(pGui->convertTo0to1(savedGuiTheme));
            if (auto* pLed = apvts.getParameter("ledTheme"))
                pLed->setValueNotifyingHost(pLed->convertTo0to1(savedLedTheme));
            if (auto* pCrt = apvts.getParameter("crtTheme"))
                pCrt->setValueNotifyingHost(pCrt->convertTo0to1(savedCrtTheme));
        }

        // Transport is never restored: a project/patch must always come up with the
        // sequencer stopped, otherwise the "PLAY" state survives a close/reopen cycle.
        if (auto* pPlay = apvts.getParameter("seqPlay"))
            pPlay->setValueNotifyingHost(0.0f);
        if (auto* pRec = apvts.getParameter("seqRec"))
            pRec->setValueNotifyingHost(0.0f);

        sequencer.isPlaying = false;
        sequencer.isRecording = false;

        auto deserializePart = [](juce::XmlElement* partXml, TimbrePatch& p) {
            if (partXml == nullptr) return;
            p.dco1Morph     = static_cast<float>(partXml->getDoubleAttribute("dco1Morph", p.dco1Morph));
            p.dco1PWM       = static_cast<float>(partXml->getDoubleAttribute("dco1PWM", p.dco1PWM));
            p.dco1Octave    = partXml->getIntAttribute("dco1Octave", p.dco1Octave);
            p.dco1Fold      = static_cast<float>(partXml->getDoubleAttribute("dco1Fold", p.dco1Fold));
            p.dco1SawMix    = static_cast<float>(partXml->getDoubleAttribute("dco1SawMix", p.dco1SawMix));
            p.dco1SawDetune = static_cast<float>(partXml->getDoubleAttribute("dco1SawDetune", p.dco1SawDetune));

            p.dco2Morph   = static_cast<float>(partXml->getDoubleAttribute("dco2Morph", p.dco2Morph));
            p.dco2PWM     = static_cast<float>(partXml->getDoubleAttribute("dco2PWM", p.dco2PWM));
            p.dco2Semi    = partXml->getIntAttribute("dco2Semi", p.dco2Semi);
            p.dco2Cents   = static_cast<float>(partXml->getDoubleAttribute("dco2Cents", p.dco2Cents));
            p.dco2Fold    = static_cast<float>(partXml->getDoubleAttribute("dco2Fold", p.dco2Fold));
            p.dco2Level   = static_cast<float>(partXml->getDoubleAttribute("dco2Level", p.dco2Level));
            p.syncMode    = partXml->getIntAttribute("syncMode", p.syncMode ? 1 : 0) != 0;
            p.xmodAmount  = static_cast<float>(partXml->getDoubleAttribute("xmodAmount", p.xmodAmount));
            p.xmodMode    = partXml->getIntAttribute("xmodMode", p.xmodMode);
            p.subOctave   = partXml->getIntAttribute("subOctave", p.subOctave);
            p.subLevel    = static_cast<float>(partXml->getDoubleAttribute("subLevel", p.subLevel));
            p.subWaveform = partXml->getIntAttribute("subWaveform", p.subWaveform);
            p.noiseLevel  = static_cast<float>(partXml->getDoubleAttribute("noiseLevel", p.noiseLevel));
            p.hpfCutoff   = static_cast<float>(partXml->getDoubleAttribute("hpfCutoff", p.hpfCutoff));
            p.lpfCutoff   = static_cast<float>(partXml->getDoubleAttribute("lpfCutoff", p.lpfCutoff));
            p.lpfRes      = static_cast<float>(partXml->getDoubleAttribute("lpfRes", p.lpfRes));
            p.envMod      = static_cast<float>(partXml->getDoubleAttribute("envMod", p.envMod));
            p.filterRouting = partXml->getIntAttribute("filterRouting", p.filterRouting ? 1 : 0) != 0;
            p.filterBalance = static_cast<float>(partXml->getDoubleAttribute("filterBalance", p.filterBalance));
            p.f2Cutoff    = static_cast<float>(partXml->getDoubleAttribute("f2Cutoff", p.f2Cutoff));
            p.f2Res       = static_cast<float>(partXml->getDoubleAttribute("f2Res", p.f2Res));
            p.f2Mode      = partXml->getIntAttribute("f2Mode", p.f2Mode);
            p.f2EnvDepth  = static_cast<float>(partXml->getDoubleAttribute("f2EnvDepth", p.f2EnvDepth));
            p.f2KeyTrack  = static_cast<float>(partXml->getDoubleAttribute("f2KeyTrack", p.f2KeyTrack));
            p.ampAttack   = static_cast<float>(partXml->getDoubleAttribute("ampAttack", p.ampAttack));
            p.ampDecay    = static_cast<float>(partXml->getDoubleAttribute("ampDecay", p.ampDecay));
            p.ampSustain  = static_cast<float>(partXml->getDoubleAttribute("ampSustain", p.ampSustain));
            p.ampRelease  = static_cast<float>(partXml->getDoubleAttribute("ampRelease", p.ampRelease));
            p.filtAttack  = static_cast<float>(partXml->getDoubleAttribute("filtAttack", p.filtAttack));
            p.filtDecay   = static_cast<float>(partXml->getDoubleAttribute("filtDecay", p.filtDecay));
            p.filtSustain = static_cast<float>(partXml->getDoubleAttribute("filtSustain", p.filtSustain));
            p.filtRelease = static_cast<float>(partXml->getDoubleAttribute("filtRelease", p.filtRelease));
            p.auxAttack   = static_cast<float>(partXml->getDoubleAttribute("auxAttack", p.auxAttack));
            p.auxDecay    = static_cast<float>(partXml->getDoubleAttribute("auxDecay", p.auxDecay));
            p.auxSustain  = static_cast<float>(partXml->getDoubleAttribute("auxSustain", p.auxSustain));
            p.auxRelease  = static_cast<float>(partXml->getDoubleAttribute("auxRelease", p.auxRelease));
            p.auxToPitch   = static_cast<float>(partXml->getDoubleAttribute("auxToPitch", p.auxToPitch));
            p.auxToFilter1 = static_cast<float>(partXml->getDoubleAttribute("auxToFilter1", p.auxToFilter1));
            p.auxToFilter2 = static_cast<float>(partXml->getDoubleAttribute("auxToFilter2", p.auxToFilter2));
            p.auxToFold    = static_cast<float>(partXml->getDoubleAttribute("auxToFold", p.auxToFold));
            p.auxToPWM     = static_cast<float>(partXml->getDoubleAttribute("auxToPWM", p.auxToPWM));
            p.lfo1Rate     = static_cast<float>(partXml->getDoubleAttribute("lfo1Rate", p.lfo1Rate));
            p.lfo1Shape    = partXml->getIntAttribute("lfo1Shape", p.lfo1Shape);
            p.lfo1ToFilter = static_cast<float>(partXml->getDoubleAttribute("lfo1ToFilter", p.lfo1ToFilter));
            p.lfo1ToPitch  = static_cast<float>(partXml->getDoubleAttribute("lfo1ToPitch", p.lfo1ToPitch));
            p.lfo2Rate     = static_cast<float>(partXml->getDoubleAttribute("lfo2Rate", p.lfo2Rate));
            p.lfo2Shape    = partXml->getIntAttribute("lfo2Shape", p.lfo2Shape);
            p.lfo2Amount   = static_cast<float>(partXml->getDoubleAttribute("lfo2Amount", p.lfo2Amount));
            p.lfo2Target   = partXml->getIntAttribute("lfo2Target", p.lfo2Target);
            p.lfo3Rate     = static_cast<float>(partXml->getDoubleAttribute("lfo3Rate", p.lfo3Rate));
            p.lfo3Shape    = partXml->getIntAttribute("lfo3Shape", p.lfo3Shape);
            p.lfo3Amount   = static_cast<float>(partXml->getDoubleAttribute("lfo3Amount", p.lfo3Amount));
            p.lfo3Target   = partXml->getIntAttribute("lfo3Target", p.lfo3Target);
            p.playMode     = partXml->getIntAttribute("playMode", p.playMode);
            p.glideTime    = static_cast<float>(partXml->getDoubleAttribute("glideTime", p.glideTime));
            p.glideEnable  = partXml->getIntAttribute("glideEnable", p.glideEnable ? 1 : 0) != 0;
            p.chordEnable  = partXml->getIntAttribute("chordEnable", p.chordEnable ? 1 : 0) != 0;
            p.chordType    = partXml->getIntAttribute("chordType", p.chordType);
        };

        if (partAClone != nullptr) {
            deserializePart(partAClone.get(), partPatches[0]);
        } else {
            captureCurrentPartFromApvts(0);
        }

        if (partBClone != nullptr) {
            deserializePart(partBClone.get(), partPatches[1]);
        } else {
            partPatches[1] = partPatches[0];
        }

        basePatches[0] = partPatches[0];
        basePatches[1] = partPatches[1];

        applyPartToApvts(activeEditPart);

        if (seqClone != nullptr) {
            int pages = seqClone->getIntAttribute("numPages", 4);
            sequencer.setNumPages(pages);

            auto deserializeTrack = [](juce::XmlElement* trkXml, SequencerTrack& track) {
                if (trkXml == nullptr) return;
                track.setLength(trkXml->getIntAttribute("length", 64));
                for (auto* stepXml : trkXml->getChildIterator()) {
                    if (stepXml->hasTagName("Step")) {
                        int idx = stepXml->getIntAttribute("index", 0);
                        auto& step = track.getStep(idx);
                        step.note = stepXml->getIntAttribute("note", 60);
                        step.active = stepXml->getIntAttribute("active", 1) != 0;
                        step.velocity = static_cast<float>(stepXml->getDoubleAttribute("velocity", 0.8));
                        step.gateLength = static_cast<float>(stepXml->getDoubleAttribute("gateLength", -1.0));
                        step.chordNotes[0] = stepXml->getIntAttribute("chord1", step.note);
                        step.chordNotes[1] = stepXml->getIntAttribute("chord2", -1);
                        step.chordNotes[2] = stepXml->getIntAttribute("chord3", -1);
                        step.chordNotes[3] = stepXml->getIntAttribute("chord4", -1);

                        step.locks.cutoffVal = static_cast<float>(stepXml->getDoubleAttribute("lockCut", 3000.0));
                        step.locks.resoVal   = static_cast<float>(stepXml->getDoubleAttribute("lockRes", 0.0));
                        step.locks.morphVal  = static_cast<float>(stepXml->getDoubleAttribute("lockMorph", 0.25));
                        step.locks.hasCutoffLock = stepXml->getIntAttribute("hasLockCut", 0) != 0;
                        step.locks.hasResoLock   = stepXml->getIntAttribute("hasLockRes", 0) != 0;
                        step.locks.hasMorphLock  = stepXml->getIntAttribute("hasLockMorph", 0) != 0;

                        if (stepXml->hasAttribute("lockMask")) {
                            step.locks.mask = static_cast<uint64_t>(stepXml->getStringAttribute("lockMask").getLargeIntValue());
                            for (size_t k = 0; k < ParamLocks::NUM_PARAMS; ++k) {
                                const juce::String attrName = "lk_" + juce::String(k);
                                if (stepXml->hasAttribute(attrName)) {
                                    step.locks.values[k] = static_cast<float>(stepXml->getDoubleAttribute(attrName, 0.0));
                                }
                            }
                        } else {
                            if (step.locks.hasCutoffLock) step.locks.setLock(Lock_LPFCutoff, step.locks.cutoffVal);
                            if (step.locks.hasResoLock)   step.locks.setLock(Lock_LPFRes, step.locks.resoVal);
                            if (step.locks.hasMorphLock)  step.locks.setLock(Lock_DCO1Morph, step.locks.morphVal);
                        }
                    }
                }
            };

            auto* trkA = seqClone->getChildByName("TrackA");
            auto* trkB = seqClone->getChildByName("TrackB");
            if (trkA != nullptr) deserializeTrack(trkA, sequencer.getTrack(TrackID::TrackA));
            if (trkB != nullptr) deserializeTrack(trkB, sequencer.getTrack(TrackID::TrackB));

            if (trkA == nullptr && trkB == nullptr) {
                for (auto* stepXml : seqClone->getChildIterator()) {
                    if (stepXml->hasTagName("Step")) {
                        int idx = stepXml->getIntAttribute("index", 0);
                        auto& stepA = sequencer.getTrack(TrackID::TrackA).getStep(idx);
                        stepA.note = stepXml->getIntAttribute("note", 60);
                        stepA.active = stepXml->getIntAttribute("active", 1) != 0;
                        stepA.velocity = static_cast<float>(stepXml->getDoubleAttribute("velocity", 0.8));
                        stepA.gateLength = static_cast<float>(stepXml->getDoubleAttribute("gateLength", -1.0));
                    }
                }
            }
        } else {
            sequencer.reset();
        }

        if (auto* pBpm = apvts.getRawParameterValue("internalBpm"))
            sequencer.setTempo(pBpm->load());
        if (auto* pSync = apvts.getRawParameterValue("syncToHost"))
            sequencer.setSyncToHost(pSync->load() > 0.5f);

        modMatrix.reset();

        auto deserializeMatrix = [](juce::XmlElement* matrixXml, ModulationMatrix& matrix) {
            if (matrixXml == nullptr) return;
            for (auto* slotXml : matrixXml->getChildIterator()) {
                if (slotXml->hasTagName("Slot")) {
                    int idx = slotXml->getIntAttribute("index", -1);
                    if (idx >= 0 && idx < ModulationMatrix::NUM_SLOTS) {
                        auto src = static_cast<ModSource>(slotXml->getIntAttribute("src", 0));
                        auto dst = static_cast<ModDestination>(slotXml->getIntAttribute("dst", 0));
                        float amt = static_cast<float>(slotXml->getDoubleAttribute("amt", 0.0));
                        auto via = static_cast<ModSource>(slotXml->getIntAttribute("via", 0));
                        float viaAmt = static_cast<float>(slotXml->getDoubleAttribute("viaAmt", 1.0));
                        bool en = slotXml->getIntAttribute("enabled", 0) != 0;
                        matrix.setSlot(idx, src, dst, amt, via, viaAmt, en);
                    }
                }
            }
        };

        if (modAClone != nullptr)
            deserializeMatrix(modAClone.get(), modMatrix.parts[0]);
        else if (legacyModClone != nullptr)
            deserializeMatrix(legacyModClone.get(), modMatrix.parts[0]);

        if (modBClone != nullptr)
            deserializeMatrix(modBClone.get(), modMatrix.parts[1]);
        else if (legacyModClone != nullptr)
            deserializeMatrix(legacyModClone.get(), modMatrix.parts[1]);
    }
}

void Simple116AudioProcessor::loadFactoryPreset(int presetIndex) {
    for (int p = 0; p < 2; ++p) {
        physicalKeysHeld[static_cast<size_t>(p)].fill(false);
        physicalKeyVelocities[static_cast<size_t>(p)].fill(0.0f);
        lastArpPlayingNote[static_cast<size_t>(p)] = -1;
        lastSeqPlayingNote[static_cast<size_t>(p)] = -1;
        currentPLockCut[static_cast<size_t>(p)] = 3000.0f;
        currentPLockRes[static_cast<size_t>(p)] = 0.0f;
        currentPLockMorph[static_cast<size_t>(p)] = 0.25f;
        currentPLockValues[static_cast<size_t>(p)].fill(0.0f);
    }
    sequencer.clearArp();
    sequencer.flushAllTracks(voiceManager);
    voiceManager.allNotesOff(0);
    voiceManager.allNotesOff(1);

    PresetBank::loadPreset(presetIndex, partPatches[0], partPatches[1], sequencer, modMatrix, apvts, currentPresetName);
    currentPresetIndex = presetIndex;

    basePatches[0] = partPatches[0];
    basePatches[1] = partPatches[1];

    applyPartToApvts(activeEditPart);

    if (auto* pBpm = apvts.getRawParameterValue("internalBpm"))
        sequencer.setTempo(pBpm->load());
    if (auto* pSync = apvts.getRawParameterValue("syncToHost"))
        sequencer.setSyncToHost(pSync->load() > 0.5f);
}

void Simple116AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    voiceManager.prepare(sampleRate, samplesPerBlock);

    for (int p = 0; p < 2; ++p) {
        lfo1[static_cast<size_t>(p)].setSampleRate(sampleRate);
        lfo2[static_cast<size_t>(p)].setSampleRate(sampleRate);
        lfo3[static_cast<size_t>(p)].setSampleRate(sampleRate);
        currentLfo2Amount[static_cast<size_t>(p)] = 0.0f;
        currentLfo3Amount[static_cast<size_t>(p)] = 0.0f;
        lastLfo2Val[static_cast<size_t>(p)] = 0.0f;
        lastLfo3Val[static_cast<size_t>(p)] = 0.0f;
        lastArpPlayingNote[static_cast<size_t>(p)] = -1;
        lastSeqPlayingNote[static_cast<size_t>(p)] = -1;
        lastArpOn[static_cast<size_t>(p)] = false;
        physicalKeysHeld[static_cast<size_t>(p)].fill(false);
        physicalKeyVelocities[static_cast<size_t>(p)].fill(0.0f);

        currentPLockCut[static_cast<size_t>(p)]   = partPatches[static_cast<size_t>(p)].lpfCutoff;
        currentPLockRes[static_cast<size_t>(p)]   = partPatches[static_cast<size_t>(p)].lpfRes;
        currentPLockMorph[static_cast<size_t>(p)] = partPatches[static_cast<size_t>(p)].dco1Morph;

        const auto& patch = partPatches[static_cast<size_t>(p)];
        auto& pLocks = currentPLockValues[static_cast<size_t>(p)];
        pLocks[Lock_DCO1Morph]    = patch.dco1Morph;
        pLocks[Lock_DCO1PWM]      = patch.dco1PWM;
        pLocks[Lock_DCO1Fold]     = patch.dco1Fold;
        pLocks[Lock_DCO1SawMix]   = patch.dco1SawMix;
        pLocks[Lock_DCO2Morph]    = patch.dco2Morph;
        pLocks[Lock_DCO2PWM]      = patch.dco2PWM;
        pLocks[Lock_DCO2Level]    = patch.dco2Level;
        pLocks[Lock_DCO2Semi]     = static_cast<float>(patch.dco2Semi);
        pLocks[Lock_DCO2Cents]    = patch.dco2Cents;
        pLocks[Lock_SubLevel]     = patch.subLevel;
        pLocks[Lock_NoiseLevel]   = patch.noiseLevel;
        pLocks[Lock_HPFCutoff]    = patch.hpfCutoff;
        pLocks[Lock_LPFCutoff]    = patch.lpfCutoff;
        pLocks[Lock_LPFRes]       = patch.lpfRes;
        pLocks[Lock_EnvMod]       = patch.envMod;
        pLocks[Lock_FilterBalance]= patch.filterBalance;
        pLocks[Lock_F2Cutoff]     = patch.f2Cutoff;
        pLocks[Lock_F2Res]        = patch.f2Res;
        pLocks[Lock_F2EnvDepth]   = patch.f2EnvDepth;
        pLocks[Lock_AmpAttack]    = patch.ampAttack;
        pLocks[Lock_AmpDecay]     = patch.ampDecay;
        pLocks[Lock_AmpSustain]   = patch.ampSustain;
        pLocks[Lock_AmpRelease]   = patch.ampRelease;
        pLocks[Lock_FiltAttack]   = patch.filtAttack;
        pLocks[Lock_FiltDecay]    = patch.filtDecay;
        pLocks[Lock_FiltSustain]  = patch.filtSustain;
        pLocks[Lock_FiltRelease]  = patch.filtRelease;
        pLocks[Lock_AuxToPitch]   = patch.auxToPitch;
        pLocks[Lock_AuxToFilter1] = patch.auxToFilter1;
        pLocks[Lock_AuxToFilter2] = patch.auxToFilter2;
        pLocks[Lock_AuxToFold]    = patch.auxToFold;
        pLocks[Lock_AuxToPWM]     = patch.auxToPWM;
    }

    chorus.setSampleRate(sampleRate);
    delay.setSampleRate(sampleRate);
    reverb.prepare(sampleRate, samplesPerBlock);
    sequencer.setSampleRate(sampleRate);
    keyboardState.reset();
    midiCollector.reset(sampleRate);

    for (int i = 0; i < 16; ++i) {
        vPanParams[static_cast<size_t>(i)] = apvts.getRawParameterValue("vPan" + juce::String(i + 1));
        vTuneParams[static_cast<size_t>(i)] = apvts.getRawParameterValue("vTune" + juce::String(i + 1));
    }

    lastHostPlaying = false;
    currentPitchBend = 0.0f;
    currentModWheel = 0.0f;
    currentAftertouch = 0.0f;
    currentExpression = 1.0f;
    currentBreath = 0.0f;
    lastMidiNotePlayed = 60.0f;
    lastMidiVelocity = 0.8f;

    smoothedMidiNote = 60.0f;
    smoothedVelocity = 0.8f;
    smoothedPitchBend = 0.0f;
    smoothedModWheel = 0.0f;
    smoothedAftertouch = 0.0f;
    smoothedExpression = 1.0f;
    smoothedBreath = 0.0f;

    activeMidiInputs.clear();
}

void Simple116AudioProcessor::releaseResources() {
    activeMidiInputs.clear();
    keyboardState.reset();
    for (int p = 0; p < 2; ++p) {
        physicalKeysHeld[static_cast<size_t>(p)].fill(false);
        physicalKeyVelocities[static_cast<size_t>(p)].fill(0.0f);
        lastArpPlayingNote[static_cast<size_t>(p)] = -1;
        lastSeqPlayingNote[static_cast<size_t>(p)] = -1;
        currentLfo2Amount[static_cast<size_t>(p)] = 0.0f;
        currentLfo3Amount[static_cast<size_t>(p)] = 0.0f;
        lastLfo2Val[static_cast<size_t>(p)] = 0.0f;
        lastLfo3Val[static_cast<size_t>(p)] = 0.0f;
        currentPLockCut[static_cast<size_t>(p)]   = 3000.0f;
        currentPLockRes[static_cast<size_t>(p)]   = 0.0f;
        currentPLockMorph[static_cast<size_t>(p)] = 0.25f;
        currentPLockValues[static_cast<size_t>(p)].fill(0.0f);
    }
    sequencer.clearArp();
    sequencer.flushAllTracks(voiceManager);
    voiceManager.allNotesOff(-1);
    delay.reset();
    chorus.reset();
    reverb.reset();
}

void Simple116AudioProcessor::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) {
    midiCollector.addMessageToQueue(message);
    keyboardState.processNextMidiEvent(message);
}

bool Simple116AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Simple116AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = 0; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    captureCurrentPartFromApvts(activeEditPart);

    const int activePlayMode = partPatches[static_cast<size_t>(activeEditPart)].playMode;
    voiceManager.setVoiceMode(activePlayMode);

    switch (activePlayMode)
    {
        case 0:
            voiceManager.setBiTimbral(true);
            voiceManager.setTruePoly16(false);
            break;
        case 1:
            voiceManager.setBiTimbral(false);
            voiceManager.setTruePoly16(true);
            break;
        case 3:
            voiceManager.setLegatoMode(true);
            break;
        case 4:
            voiceManager.setLegatoMode(false);
            break;
        default:
            break;
    }

    const bool syncToHost = apvts.getRawParameterValue("syncToHost")->load() > 0.5f;
    const float internalBpm = apvts.getRawParameterValue("internalBpm")->load();
    const bool seqPlay = apvts.getRawParameterValue("seqPlay")->load() > 0.5f;

    if (auto* pRec = apvts.getRawParameterValue("seqRec"))
        sequencer.isRecording = (pRec->load() > 0.5f);

    double hostBpm = 120.0;
    bool isHostPlaying = false;
    bool hasHost = false;
    double ppqPos = 0.0;

    if (auto* playHead = getPlayHead()) {
        if (auto pos = playHead->getPosition()) {
            hasHost = true;
            if (pos->getBpm()) hostBpm = *pos->getBpm();
            isHostPlaying = pos->getIsPlaying();
            if (pos->getPpqPosition()) ppqPos = *pos->getPpqPosition();
        }
    }

    const bool isStandalone = (wrapperType == wrapperType_Standalone);
    const bool effectiveSync = syncToHost && hasHost && !isStandalone;

    double effectiveBpm = 120.0;
    bool effectivePlaying = false;

    if (effectiveSync) {
        effectiveBpm = (hostBpm > 20.0 && hostBpm < 400.0) ? hostBpm : internalBpm;
        effectivePlaying = seqPlay || isHostPlaying;
    } else {
        effectiveBpm = internalBpm;
        effectivePlaying = seqPlay;
    }

    sequencer.setSyncToHost(syncToHost);
    sequencer.setTempo(static_cast<float>(effectiveBpm));

    if (auto* pPages = apvts.getRawParameterValue("seqPages")) {
        const int pagesChoice = static_cast<int>(pPages->load());
        const int numPages = std::clamp(pagesChoice + 1, 1, 4);
        if (sequencer.getNumPages() != numPages)
            sequencer.setNumPages(numPages);
    }

    const float seqSwing = apvts.getRawParameterValue("seqSwing")->load();
    const float seqGate  = apvts.getRawParameterValue("seqGate")->load();
    if (seqGateLinked) {
        sequencer.getTrack(TrackID::TrackA).setSwing(seqSwing);
        sequencer.getTrack(TrackID::TrackB).setSwing(seqSwing);
        sequencer.getTrack(TrackID::TrackA).setGateLength(seqGate);
        sequencer.getTrack(TrackID::TrackB).setGateLength(seqGate);
    } else {
        const auto editTrk = sequencer.getEditTrack();
        sequencer.getTrack(editTrk).setSwing(seqSwing);
        sequencer.getTrack(editTrk).setGateLength(seqGate);
    }

    if (effectivePlaying != lastHostPlaying || sequencer.isPlaying != effectivePlaying) {
        sequencer.isPlaying = effectivePlaying;
        if (effectivePlaying) {
            if (effectiveSync && hasHost) {
                double sixteenthSteps = ppqPos * 4.0;
                int stepIdx = static_cast<int>(std::fmod(std::floor(std::max(0.0, sixteenthSteps)), static_cast<double>(sequencer.getNumSteps())));
                sequencer.getTrack(TrackID::TrackA).setCurrentStep(stepIdx);
                sequencer.getTrack(TrackID::TrackB).setCurrentStep(stepIdx);
                sequencer.currentStep = stepIdx;
            }
        } else {
            SequencerEngine::FlushedNote flushedNotes[4];
            int numFlushed = sequencer.flushAllTracks(voiceManager, flushedNotes);
            for (int i = 0; i < numFlushed && i < 4; ++i) {
                midiMessages.addEvent(juce::MidiMessage::noteOff(flushedNotes[i].channel, flushedNotes[i].note, 0.0f), 0);
            }
            voiceManager.allNotesOff(0);
            voiceManager.allNotesOff(1);
        }
        lastHostPlaying = effectivePlaying;
    }

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    const float uniDet = apvts.getRawParameterValue("unisonDetune")->load();
    const float uniSpr = apvts.getRawParameterValue("unisonSpread")->load();
    voiceManager.setUnisonDetune(uniDet);
    voiceManager.setUnisonSpread(uniSpr);

    auto mapLfoShape = [](int choice) noexcept -> int {
        if (choice == 2) return 5;
        return choice;
    };

    for (int p = 0; p < 2; ++p) {
        const auto& patch = partPatches[static_cast<size_t>(p)];
        lfo1[static_cast<size_t>(p)].setFrequency(patch.lfo1Rate);
        lfo1[static_cast<size_t>(p)].setWaveform(patch.lfo1Shape);

        lfo2[static_cast<size_t>(p)].setFrequency(patch.lfo2Rate);
        lfo2[static_cast<size_t>(p)].setWaveform(mapLfoShape(patch.lfo2Shape));

        lfo3[static_cast<size_t>(p)].setFrequency(patch.lfo3Rate);
        lfo3[static_cast<size_t>(p)].setWaveform(mapLfoShape(patch.lfo3Shape));
    }

    const bool arpOn   = apvts.getRawParameterValue("arpEnable")->load() > 0.5f;
    const int  arpIdx  = static_cast<int>(apvts.getRawParameterValue("arpMode")->load());
    const bool chordOn = apvts.getRawParameterValue("chordEnable")->load() > 0.5f;
    const int  chordIdx = static_cast<int>(apvts.getRawParameterValue("chordType")->load());

    voiceManager.setChordMode(chordOn, chordIdx);
    sequencer.arpEnabled = arpOn;
    sequencer.setArpMode(arpIdx);

    if (arpOn != lastArpOn[static_cast<size_t>(activeEditPart)]) {
        if (arpOn) {
            voiceManager.allNotesOff(activeEditPart);
            sequencer.clearArp();
            for (int n = 0; n < 128; ++n) keyboardState.noteOff(16, n, 0.0f);

            if (lastArpPlayingNote[static_cast<size_t>(activeEditPart)] >= 0) {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f), 0);
                lastArpPlayingNote[static_cast<size_t>(activeEditPart)] = -1;
            }

            for (int n = 0; n < 128; ++n) {
                if (physicalKeysHeld[static_cast<size_t>(activeEditPart)][static_cast<size_t>(n)]) {
                    if (chordOn) {
                        auto chordNotes = getChordNotes(n, chordIdx);
                        for (int cn : chordNotes) sequencer.addArpNote(cn);
                    } else {
                        sequencer.addArpNote(n);
                    }
                }
            }
        } else {
            if (lastArpPlayingNote[static_cast<size_t>(activeEditPart)] >= 0) {
                voiceManager.noteOff(lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f, activeEditPart);
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f), 0);
                lastArpPlayingNote[static_cast<size_t>(activeEditPart)] = -1;
            }
            sequencer.clearArp();

            for (int n = 0; n < 128; ++n) {
                if (physicalKeysHeld[static_cast<size_t>(activeEditPart)][static_cast<size_t>(n)]) {
                    const float vel = physicalKeyVelocities[static_cast<size_t>(activeEditPart)][static_cast<size_t>(n)];
                    if (chordOn) {
                        auto chordNotes = getChordNotes(n, chordIdx);
                        for (int cn : chordNotes) {
                            voiceManager.noteOn(cn, (vel > 0.001f) ? vel : 0.8f, activeEditPart);
                            if (cn != n) keyboardState.noteOn(16, cn, (vel > 0.001f) ? vel : 0.8f);
                        }
                    } else {
                        voiceManager.noteOn(n, (vel > 0.001f) ? vel : 0.8f, activeEditPart);
                    }
                }
            }
        }
        lastArpOn[static_cast<size_t>(activeEditPart)] = arpOn;
    }

    const int chorusM      = static_cast<int>(apvts.getRawParameterValue("chorusMode")->load());
    const float rSize      = apvts.getRawParameterValue("reverbSize")->load();
    const float rDamp      = apvts.getRawParameterValue("reverbDamp")->load();
    const float rMix       = apvts.getRawParameterValue("reverbMix")->load();

    const bool  dSyncOn    = (apvts.getRawParameterValue("delaySyncEnable") != nullptr)
                                ? (apvts.getRawParameterValue("delaySyncEnable")->load() > 0.5f)
                                : true;
    const int   delaySyncIdx = static_cast<int>(apvts.getRawParameterValue("delaySync")->load());
    const float dFb        = apvts.getRawParameterValue("delayFeedback")->load();
    const float dDamp      = apvts.getRawParameterValue("delayDamp")->load();
    const float dMix       = apvts.getRawParameterValue("delayMix")->load();
    const bool  dPingPong  = apvts.getRawParameterValue("delayPingPong")->load() > 0.5f;

    const int fxRouting = (apvts.getRawParameterValue("fxRouting") != nullptr)
                        ? static_cast<int>(apvts.getRawParameterValue("fxRouting")->load())
                        : 0;

    const float syncMultipliers[] = {
        0.25f, 0.16667f, 0.375f, 0.5f, 0.33333f, 0.75f,
        1.0f, 0.66667f, 1.5f, 2.0f, 1.33333f, 3.0f, 4.0f
    };

    float finalDelayTimeSec = 0.35f;
    if (dSyncOn && delaySyncIdx >= 0 && delaySyncIdx < 13) {
        const float safeHostBpm = static_cast<float>((effectiveBpm > 10.0 && effectiveBpm < 400.0) ? effectiveBpm : 120.0);
        const float beatTime = 60.0f / safeHostBpm;
        finalDelayTimeSec = std::clamp(beatTime * syncMultipliers[delaySyncIdx], 0.01f, 1.95f);
    } else {
        finalDelayTimeSec = std::clamp(apvts.getRawParameterValue("delayTime")->load(), 0.01f, 1.95f);
    }

    chorus.setMode(chorusM);
    delay.setParameters(finalDelayTimeSec, dFb, dDamp, dMix, dPingPong);
    reverb.setParameters(rSize, rDamp, rMix);

    auto& voices = voiceManager.getVoices();
    for (int p = 0; p < 2; ++p) {
        const auto& patch = partPatches[static_cast<size_t>(p)];
        const auto& track = sequencer.getTrack(static_cast<TrackID>(p));
        const int startV = p * VoiceManager::VOICES_PER_PART;
        const int endV   = startV + VoiceManager::VOICES_PER_PART;

        const float curAmpA = track.isLockActive(Lock_AmpAttack)  ? track.getActiveLockValue(Lock_AmpAttack, patch.ampAttack)   : patch.ampAttack;
        const float curAmpD = track.isLockActive(Lock_AmpDecay)   ? track.getActiveLockValue(Lock_AmpDecay, patch.ampDecay)     : patch.ampDecay;
        const float curAmpS = track.isLockActive(Lock_AmpSustain) ? track.getActiveLockValue(Lock_AmpSustain, patch.ampSustain) : patch.ampSustain;
        const float curAmpR = track.isLockActive(Lock_AmpRelease) ? track.getActiveLockValue(Lock_AmpRelease, patch.ampRelease) : patch.ampRelease;

        const float curFiltA = track.isLockActive(Lock_FiltAttack)  ? track.getActiveLockValue(Lock_FiltAttack, patch.filtAttack)   : patch.filtAttack;
        const float curFiltD = track.isLockActive(Lock_FiltDecay)   ? track.getActiveLockValue(Lock_FiltDecay, patch.filtDecay)     : patch.filtDecay;
        const float curFiltS = track.isLockActive(Lock_FiltSustain) ? track.getActiveLockValue(Lock_FiltSustain, patch.filtSustain) : patch.filtSustain;
        const float curFiltR = track.isLockActive(Lock_FiltRelease) ? track.getActiveLockValue(Lock_FiltRelease, patch.filtRelease) : patch.filtRelease;

        for (int vi = startV; vi < endV; ++vi) {
            auto& voice = voices[static_cast<size_t>(vi)];
            voice.ampEnv.setParameters(curAmpA, curAmpD, curAmpS, curAmpR);
            voice.filterEnv.setParameters(curFiltA, curFiltD, curFiltS, curFiltR);
            voice.auxEnv.setParameters(patch.auxAttack, patch.auxDecay, patch.auxSustain, patch.auxRelease);

            voice.setSupersawDetune(patch.dco1SawDetune);

            const bool shouldGlide = patch.glideEnable || (activePlayMode == 3);
            voice.setPortamentoEnabled(shouldGlide);
            voice.setPortamentoTime(patch.glideTime);

            if (vPanParams[static_cast<size_t>(vi)] != nullptr)
                voice.setPan(vPanParams[static_cast<size_t>(vi)]->load());
            if (vTuneParams[static_cast<size_t>(vi)] != nullptr)
                voice.setVoiceTuneOffset(vTuneParams[static_cast<size_t>(vi)]->load());
        }
    }

    struct OutMidiEvent { int type; int note; float vel; int sample; int channel; };
    std::array<OutMidiEvent, 128> outMidiEvents;
    int outMidiCount = 0;

    const int numSamples = buffer.getNumSamples();
    const float numSamplesInv = (numSamples > 0) ? (1.0f / static_cast<float>(numSamples)) : 1.0f;

    std::array<float, 2> lfo2Delta {};
    std::array<float, 2> lfo3Delta {};
    for (int p = 0; p < 2; ++p) {
        lfo2Delta[static_cast<size_t>(p)] = (partPatches[static_cast<size_t>(p)].lfo2Amount - currentLfo2Amount[static_cast<size_t>(p)]) * numSamplesInv;
        lfo3Delta[static_cast<size_t>(p)] = (partPatches[static_cast<size_t>(p)].lfo3Amount - currentLfo3Amount[static_cast<size_t>(p)]) * numSamplesInv;
    }

    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);

    constexpr int MAX_SCRATCH_SAMPLES = 4096;
    alignas(16) float dryBusL[MAX_SCRATCH_SAMPLES] = { 0.0f };
    alignas(16) float dryBusR[MAX_SCRATCH_SAMPLES] = { 0.0f };
    const int safeSamples = std::min(numSamples, MAX_SCRATCH_SAMPLES);

    auto midiIterator = midiMessages.cbegin();
    auto midiEnd = midiMessages.cend();
    int nextMidiSample = (midiIterator != midiEnd) ? (*midiIterator).samplePosition : -1;

    for (int sample = 0; sample < numSamples; ++sample) {
        
        while (sample == nextMidiSample && midiIterator != midiEnd) {
            const auto msg = (*midiIterator).getMessage();

            if (msg.getChannel() == 16) {
                ++midiIterator;
                nextMidiSample = (midiIterator != midiEnd) ? (*midiIterator).samplePosition : -1;
                continue;
            }

            const int msgChannel = msg.getChannel();
            int targetPart = activeEditPart;
            if (msgChannel == 2) {
                targetPart = 1;
            } else if (msgChannel == 1) {
                targetPart = isStandalone ? activeEditPart : 0;
            }

            const int noteNum = msg.getNoteNumber();

            if (msg.isNoteOn() && msg.getVelocity() > 0) {
                if (noteNum >= 0 && noteNum <= 127) {
                    const float vel = msg.getFloatVelocity();
                    lastMidiNotePlayed = static_cast<float>(noteNum);
                    lastMidiVelocity = vel;

                    if (sequencer.isRecording) {
                        if (chordOn && targetPart == activeEditPart) {
                            auto chordNotesToRecord = getChordNotes(noteNum, chordIdx);
                            sequencer.recordChord(chordNotesToRecord, vel);
                        } else {
                            sequencer.recordNote(noteNum, vel);
                        }
                    }

                    if (!physicalKeysHeld[static_cast<size_t>(targetPart)][static_cast<size_t>(noteNum)]) {
                        physicalKeysHeld[static_cast<size_t>(targetPart)][static_cast<size_t>(noteNum)] = true;
                        physicalKeyVelocities[static_cast<size_t>(targetPart)][static_cast<size_t>(noteNum)] = vel;

                        if (chordOn && targetPart == activeEditPart) {
                            auto chordNotes = getChordNotes(noteNum, chordIdx);
                            if (arpOn) {
                                for (int cn : chordNotes) sequencer.addArpNote(cn);
                            } else {
                                for (int cn : chordNotes) {
                                    voiceManager.noteOn(cn, vel, targetPart);
                                    if (cn != noteNum) keyboardState.noteOn(16, cn, vel);
                                }
                            }
                        } else {
                            if (arpOn && targetPart == activeEditPart) {
                                sequencer.addArpNote(noteNum);
                            } else {
                                voiceManager.noteOn(noteNum, vel, targetPart);
                            }
                        }
                    }
                }
            } else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0)) {
                if (noteNum >= 0 && noteNum <= 127) {
                    physicalKeysHeld[static_cast<size_t>(targetPart)][static_cast<size_t>(noteNum)] = false;
                    physicalKeyVelocities[static_cast<size_t>(targetPart)][static_cast<size_t>(noteNum)] = 0.0f;

                    if (chordOn && targetPart == activeEditPart) {
                        auto chordNotes = getChordNotes(noteNum, chordIdx);
                        if (arpOn) {
                            for (int cn : chordNotes) sequencer.removeArpNote(cn);
                            if (sequencer.getNumArpHeld() == 0 && lastArpPlayingNote[static_cast<size_t>(targetPart)] >= 0) {
                                voiceManager.noteOff(lastArpPlayingNote[static_cast<size_t>(targetPart)], 0.0f, targetPart);
                                if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, lastArpPlayingNote[static_cast<size_t>(targetPart)], 0.0f, sample, targetPart + 1 };
                                lastArpPlayingNote[static_cast<size_t>(targetPart)] = -1;
                            }
                        } else {
                            for (int cn : chordNotes) {
                                voiceManager.noteOff(cn, 0.0f, targetPart);
                                if (cn != noteNum) keyboardState.noteOff(16, cn, 0.0f);
                            }
                        }
                    } else {
                        if (arpOn && targetPart == activeEditPart) {
                            sequencer.removeArpNote(noteNum);
                            if (sequencer.getNumArpHeld() == 0 && lastArpPlayingNote[static_cast<size_t>(targetPart)] >= 0) {
                                voiceManager.noteOff(lastArpPlayingNote[static_cast<size_t>(targetPart)], 0.0f, targetPart);
                                if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, lastArpPlayingNote[static_cast<size_t>(targetPart)], 0.0f, sample, targetPart + 1 };
                                lastArpPlayingNote[static_cast<size_t>(targetPart)] = -1;
                            }
                        } else {
                            voiceManager.noteOff(noteNum, 0.0f, targetPart);
                        }
                    }
                }
            } else if (msg.isPitchWheel()) {
                currentPitchBend = static_cast<float>(msg.getPitchWheelValue() - 8192) / 8192.0f;
                smoothedPitchBend = currentPitchBend;
                voiceManager.setPitchBend(currentPitchBend, targetPart);
            } else if (msg.isChannelPressure()) {
                currentAftertouch = static_cast<float>(msg.getChannelPressureValue()) / 127.0f;
                smoothedAftertouch = currentAftertouch;
                voiceManager.setAftertouch(currentAftertouch, targetPart);
            } else if (msg.isController()) {
                const int ccNum = msg.getControllerNumber();
                const float ccVal = static_cast<float>(msg.getControllerValue()) / 127.0f;
                if (ccNum == 1) {
                    currentModWheel = ccVal;
                    smoothedModWheel = currentModWheel;
                    voiceManager.setModWheel(currentModWheel, targetPart);
                } else if (ccNum == 2) {
                    currentBreath = ccVal;
                    smoothedBreath = currentBreath;
                } else if (ccNum == 11) {
                    currentExpression = ccVal;
                    smoothedExpression = currentExpression;
                }
            } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                for (int p = 0; p < 2; ++p) {
                    physicalKeysHeld[static_cast<size_t>(p)].fill(false);
                    physicalKeyVelocities[static_cast<size_t>(p)].fill(0.0f);
                    if (lastArpPlayingNote[static_cast<size_t>(p)] >= 0) {
                        voiceManager.noteOff(lastArpPlayingNote[static_cast<size_t>(p)], 0.0f, p);
                        if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, lastArpPlayingNote[static_cast<size_t>(p)], 0.0f, sample, p + 1 };
                        lastArpPlayingNote[static_cast<size_t>(p)] = -1;
                    }
                    lastSeqPlayingNote[static_cast<size_t>(p)] = -1;
                }
                for (int n = 0; n < 128; ++n) keyboardState.noteOff(16, n, 0.0f);

                sequencer.clearArp();
                SequencerEngine::FlushedNote flushedNotes[4];
                int numFlushed = sequencer.flushAllTracks(voiceManager, flushedNotes);
                for (int f = 0; f < numFlushed && f < 4; ++f) {
                    if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, flushedNotes[f].note, 0.0f, sample, flushedNotes[f].channel };
                }
                voiceManager.allNotesOff(-1);
            }

            ++midiIterator;
            nextMidiSample = (midiIterator != midiEnd) ? (*midiIterator).samplePosition : -1;
        }

        int arpNote = -1;
        float arpVel = 0.85f;
        bool arpOff = false;
        if (arpOn) {
            if (sequencer.advanceArpClock(1, arpNote, arpVel, arpOff)) {
                if (lastArpPlayingNote[static_cast<size_t>(activeEditPart)] >= 0) {
                    voiceManager.noteOff(lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f, activeEditPart);
                    if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f, sample, activeEditPart + 1 };
                }
                lastArpPlayingNote[static_cast<size_t>(activeEditPart)] = arpNote;
                lastMidiNotePlayed = static_cast<float>(arpNote);
                lastMidiVelocity = arpVel;
                voiceManager.noteOn(arpNote, arpVel, activeEditPart);
                if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x90, arpNote, arpVel, sample, activeEditPart + 1 };
            } else if (arpOff && lastArpPlayingNote[static_cast<size_t>(activeEditPart)] >= 0) {
                voiceManager.noteOff(lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f, activeEditPart);
                if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f, sample, activeEditPart + 1 };
                lastArpPlayingNote[static_cast<size_t>(activeEditPart)] = -1;
            } else if (sequencer.getNumArpHeld() == 0 && lastArpPlayingNote[static_cast<size_t>(activeEditPart)] >= 0) {
                voiceManager.noteOff(lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f, activeEditPart);
                if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f, sample, activeEditPart + 1 };
                lastArpPlayingNote[static_cast<size_t>(activeEditPart)] = -1;
            }
        } else if (lastArpPlayingNote[static_cast<size_t>(activeEditPart)] >= 0) {
            voiceManager.noteOff(lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f, activeEditPart);
            if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, lastArpPlayingNote[static_cast<size_t>(activeEditPart)], 0.0f, sample, activeEditPart + 1 };
            lastArpPlayingNote[static_cast<size_t>(activeEditPart)] = -1;
        }

        const double hostBpmSafe = std::clamp(static_cast<double>(effectiveBpm), 20.0, 300.0);
        for (int t = 0; t < 2; ++t) {
            int seqNoteOn = -1;
            float seqVelOn = 0.0f;
            int seqNoteOff = -1;
            if (sequencer.advanceStepClockSample(static_cast<TrackID>(t), hostBpmSafe, seqNoteOn, seqVelOn, seqNoteOff)) {
                auto& track = sequencer.getTrack(static_cast<TrackID>(t));

                if (seqNoteOff >= 0) {
                    voiceManager.noteOff(seqNoteOff, 0.0f, t);
                    for (int cn : track.activeSoundingChord) {
                        if (cn >= 0 && cn != seqNoteOff)
                            voiceManager.noteOff(cn, 0.0f, t);
                    }
                    track.activeSoundingChord.fill(-1);
                    if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, seqNoteOff, 0.0f, sample, t + 1 };
                }

                if (seqNoteOn >= 0) {
                    lastMidiNotePlayed = static_cast<float>(seqNoteOn);
                    lastMidiVelocity = seqVelOn;

                    const auto& step = track.getStep(track.getPlayingStep());
                    bool playedRecordedChord = false;

                    for (size_t ci = 0; ci < 4; ++ci) {
                        const int cn = step.chordNotes[ci];
                        if (cn >= 0 && ci > 0) {
                            voiceManager.noteOn(cn, seqVelOn, t);
                            track.activeSoundingChord[ci] = cn;
                            if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x90, cn, seqVelOn, sample, t + 1 };
                            playedRecordedChord = true;
                        }
                    }

                    if (playedRecordedChord) {
                        voiceManager.noteOn(seqNoteOn, seqVelOn, t);
                        track.activeSoundingChord[0] = seqNoteOn;
                        if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x90, seqNoteOn, seqVelOn, sample, t + 1 };
                    } else if (partPatches[static_cast<size_t>(t)].chordEnable) {
                        auto expNotes = getChordNotes(seqNoteOn, partPatches[static_cast<size_t>(t)].chordType);
                        for (size_t ci = 0; ci < 4; ++ci) {
                            track.activeSoundingChord[ci] = (ci < expNotes.size()) ? expNotes[ci] : -1;
                        }
                        for (int cn : expNotes) {
                            voiceManager.noteOn(cn, seqVelOn, t);
                            if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x90, cn, seqVelOn, sample, t + 1 };
                        }
                    } else {
                        voiceManager.noteOn(seqNoteOn, seqVelOn, t);
                        track.activeSoundingChord[0] = seqNoteOn;
                        for (size_t ci = 1; ci < 4; ++ci) track.activeSoundingChord[ci] = -1;
                        if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x90, seqNoteOn, seqVelOn, sample, t + 1 };
                    }
                }
            } else if (seqNoteOff >= 0) {
                voiceManager.noteOff(seqNoteOff, 0.0f, t);
                auto& track = sequencer.getTrack(static_cast<TrackID>(t));
                for (int cn : track.activeSoundingChord) {
                    if (cn >= 0 && cn != seqNoteOff)
                        voiceManager.noteOff(cn, 0.0f, t);
                }
                track.activeSoundingChord.fill(-1);
                if (outMidiCount < 128) outMidiEvents[outMidiCount++] = { 0x80, seqNoteOff, 0.0f, sample, t + 1 };
            }
        }

        for (int p = 0; p < 2; ++p) {
            currentLfo2Amount[static_cast<size_t>(p)] += lfo2Delta[static_cast<size_t>(p)];
            currentLfo3Amount[static_cast<size_t>(p)] += lfo3Delta[static_cast<size_t>(p)];

            lastLfo2Val[static_cast<size_t>(p)] = lfo2[static_cast<size_t>(p)].process() * currentLfo2Amount[static_cast<size_t>(p)];
            lastLfo3Val[static_cast<size_t>(p)] = lfo3[static_cast<size_t>(p)].process() * currentLfo3Amount[static_cast<size_t>(p)];
        }

        const float lfo1Val_A = lfo1[0].process();
        const float lfo1Val_B = lfo1[1].process();

        smoothedMidiNote += (lastMidiNotePlayed - smoothedMidiNote) * 0.01f;
        smoothedVelocity += (lastMidiVelocity - smoothedVelocity) * 0.01f;
        smoothedPitchBend += (currentPitchBend - smoothedPitchBend) * 0.05f;
        smoothedModWheel += (currentModWheel - smoothedModWheel) * 0.05f;
        smoothedAftertouch += (currentAftertouch - smoothedAftertouch) * 0.05f;
        smoothedExpression += (currentExpression - smoothedExpression) * 0.05f;
        smoothedBreath += (currentBreath - smoothedBreath) * 0.05f;

        modMatrix.parts[0].setSourceValue(ModSource::LFO1, lfo1Val_A);
        modMatrix.parts[0].setSourceValue(ModSource::LFO2, lastLfo2Val[0]);
        modMatrix.parts[0].setSourceValue(ModSource::LFO3, lastLfo3Val[0]);
        modMatrix.parts[0].setSourceValue(ModSource::LFO4, lfo1Val_A);
        modMatrix.parts[0].setSourceValue(ModSource::KeyTracking, std::clamp((smoothedMidiNote - 60.0f) / 24.0f, -1.0f, 1.0f));
        modMatrix.parts[0].setSourceValue(ModSource::Velocity, smoothedVelocity);
        modMatrix.parts[0].setSourceValue(ModSource::ModWheel, smoothedModWheel);
        modMatrix.parts[0].setSourceValue(ModSource::PitchBend, smoothedPitchBend);
        modMatrix.parts[0].setSourceValue(ModSource::Aftertouch, smoothedAftertouch);
        modMatrix.parts[0].setSourceValue(ModSource::Expression, smoothedExpression);
        modMatrix.parts[0].setSourceValue(ModSource::Breath, smoothedBreath);
        modMatrix.parts[0].setSourceValue(ModSource::Constant, 1.0f);
        modMatrix.parts[0].evaluate();

        modMatrix.parts[1].setSourceValue(ModSource::LFO1, lfo1Val_B);
        modMatrix.parts[1].setSourceValue(ModSource::LFO2, lastLfo2Val[1]);
        modMatrix.parts[1].setSourceValue(ModSource::LFO3, lastLfo3Val[1]);
        modMatrix.parts[1].setSourceValue(ModSource::LFO4, lfo1Val_B);
        modMatrix.parts[1].setSourceValue(ModSource::KeyTracking, std::clamp((smoothedMidiNote - 60.0f) / 24.0f, -1.0f, 1.0f));
        modMatrix.parts[1].setSourceValue(ModSource::Velocity, smoothedVelocity);
        modMatrix.parts[1].setSourceValue(ModSource::ModWheel, smoothedModWheel);
        modMatrix.parts[1].setSourceValue(ModSource::PitchBend, smoothedPitchBend);
        modMatrix.parts[1].setSourceValue(ModSource::Aftertouch, smoothedAftertouch);
        modMatrix.parts[1].setSourceValue(ModSource::Expression, smoothedExpression);
        modMatrix.parts[1].setSourceValue(ModSource::Breath, smoothedBreath);
        modMatrix.parts[1].setSourceValue(ModSource::Constant, 1.0f);
        modMatrix.parts[1].evaluate();

        const auto& pA = partPatches[0];
        const auto& pB = partPatches[1];
        const auto& trkA = sequencer.getTrack(TrackID::TrackA);
        const auto& trkB = sequencer.getTrack(TrackID::TrackB);

        constexpr float pLockSlew = 0.08f;

        auto updatePLockParam = [pLockSlew](float& currentVal, bool hasLock, float lockedVal, float baseVal) {
            const float target = hasLock ? lockedVal : baseVal;
            currentVal += pLockSlew * (target - currentVal);
            return currentVal;
        };

        const float curD1Morph_A   = updatePLockParam(currentPLockValues[0][Lock_DCO1Morph],    trkA.isLockActive(Lock_DCO1Morph),    trkA.getActiveLockValue(Lock_DCO1Morph, pA.dco1Morph),       pA.dco1Morph);
        const float curD1PWM_A     = updatePLockParam(currentPLockValues[0][Lock_DCO1PWM],      trkA.isLockActive(Lock_DCO1PWM),      trkA.getActiveLockValue(Lock_DCO1PWM, pA.dco1PWM),           pA.dco1PWM);
        const float curD1Fold_A    = updatePLockParam(currentPLockValues[0][Lock_DCO1Fold],     trkA.isLockActive(Lock_DCO1Fold),     trkA.getActiveLockValue(Lock_DCO1Fold, pA.dco1Fold),         pA.dco1Fold);
        const float curD1SawMix_A  = updatePLockParam(currentPLockValues[0][Lock_DCO1SawMix],   trkA.isLockActive(Lock_DCO1SawMix),   trkA.getActiveLockValue(Lock_DCO1SawMix, pA.dco1SawMix),     pA.dco1SawMix);

        const float curD2Morph_A   = updatePLockParam(currentPLockValues[0][Lock_DCO2Morph],    trkA.isLockActive(Lock_DCO2Morph),    trkA.getActiveLockValue(Lock_DCO2Morph, pA.dco2Morph),       pA.dco2Morph);
        const float curD2PWM_A     = updatePLockParam(currentPLockValues[0][Lock_DCO2PWM],      trkA.isLockActive(Lock_DCO2PWM),      trkA.getActiveLockValue(Lock_DCO2PWM, pA.dco2PWM),           pA.dco2PWM);
        const float curD2Level_A   = updatePLockParam(currentPLockValues[0][Lock_DCO2Level],    trkA.isLockActive(Lock_DCO2Level),    trkA.getActiveLockValue(Lock_DCO2Level, pA.dco2Level),       pA.dco2Level);
        const float curD2Semi_A    = updatePLockParam(currentPLockValues[0][Lock_DCO2Semi],     trkA.isLockActive(Lock_DCO2Semi),     trkA.getActiveLockValue(Lock_DCO2Semi, (float)pA.dco2Semi),  (float)pA.dco2Semi);
        const float curD2Cents_A   = updatePLockParam(currentPLockValues[0][Lock_DCO2Cents],    trkA.isLockActive(Lock_DCO2Cents),    trkA.getActiveLockValue(Lock_DCO2Cents, pA.dco2Cents),       pA.dco2Cents);

        const float curSubLvl_A    = updatePLockParam(currentPLockValues[0][Lock_SubLevel],     trkA.isLockActive(Lock_SubLevel),     trkA.getActiveLockValue(Lock_SubLevel, pA.subLevel),         pA.subLevel);
        const float curNseLvl_A    = updatePLockParam(currentPLockValues[0][Lock_NoiseLevel],   trkA.isLockActive(Lock_NoiseLevel),   trkA.getActiveLockValue(Lock_NoiseLevel, pA.noiseLevel),     pA.noiseLevel);

        const float curHpfCut_A    = updatePLockParam(currentPLockValues[0][Lock_HPFCutoff],    trkA.isLockActive(Lock_HPFCutoff),    trkA.getActiveLockValue(Lock_HPFCutoff, pA.hpfCutoff),       pA.hpfCutoff);
        const float curLpfCut_A    = updatePLockParam(currentPLockValues[0][Lock_LPFCutoff],    trkA.isLockActive(Lock_LPFCutoff),    trkA.getActiveLockValue(Lock_LPFCutoff, pA.lpfCutoff),       pA.lpfCutoff);
        const float curLpfRes_A    = updatePLockParam(currentPLockValues[0][Lock_LPFRes],       trkA.isLockActive(Lock_LPFRes),       trkA.getActiveLockValue(Lock_LPFRes, pA.lpfRes),             pA.lpfRes);
        const float curEnvMod_A    = updatePLockParam(currentPLockValues[0][Lock_EnvMod],       trkA.isLockActive(Lock_EnvMod),       trkA.getActiveLockValue(Lock_EnvMod, pA.envMod),             pA.envMod);

        const float curFBal_A      = updatePLockParam(currentPLockValues[0][Lock_FilterBalance],trkA.isLockActive(Lock_FilterBalance),trkA.getActiveLockValue(Lock_FilterBalance, pA.filterBalance),pA.filterBalance);
        const float curF2Cut_A     = updatePLockParam(currentPLockValues[0][Lock_F2Cutoff],     trkA.isLockActive(Lock_F2Cutoff),     trkA.getActiveLockValue(Lock_F2Cutoff, pA.f2Cutoff),         pA.f2Cutoff);
        const float curF2Reso_A    = updatePLockParam(currentPLockValues[0][Lock_F2Res],        trkA.isLockActive(Lock_F2Res),        trkA.getActiveLockValue(Lock_F2Res, pA.f2Res),               pA.f2Res);
        const float curF2EnvDepth_A= updatePLockParam(currentPLockValues[0][Lock_F2EnvDepth],   trkA.isLockActive(Lock_F2EnvDepth),   trkA.getActiveLockValue(Lock_F2EnvDepth, pA.f2EnvDepth),     pA.f2EnvDepth);

        const float curAuxPitch_A  = updatePLockParam(currentPLockValues[0][Lock_AuxToPitch],   trkA.isLockActive(Lock_AuxToPitch),   trkA.getActiveLockValue(Lock_AuxToPitch, pA.auxToPitch),     pA.auxToPitch);
        const float curAuxF1_A     = updatePLockParam(currentPLockValues[0][Lock_AuxToFilter1], trkA.isLockActive(Lock_AuxToFilter1), trkA.getActiveLockValue(Lock_AuxToFilter1, pA.auxToFilter1), pA.auxToFilter1);
        const float curAuxF2_A     = updatePLockParam(currentPLockValues[0][Lock_AuxToFilter2], trkA.isLockActive(Lock_AuxToFilter2), trkA.getActiveLockValue(Lock_AuxToFilter2, pA.auxToFilter2), pA.auxToFilter2);
        const float curAuxFold_A   = updatePLockParam(currentPLockValues[0][Lock_AuxToFold],    trkA.isLockActive(Lock_AuxToFold),    trkA.getActiveLockValue(Lock_AuxToFold, pA.auxToFold),       pA.auxToFold);
        const float curAuxPWM_A    = updatePLockParam(currentPLockValues[0][Lock_AuxToPWM],     trkA.isLockActive(Lock_AuxToPWM),     trkA.getActiveLockValue(Lock_AuxToPWM, pA.auxToPWM),         pA.auxToPWM);

        const float curD1Morph_B   = updatePLockParam(currentPLockValues[1][Lock_DCO1Morph],    trkB.isLockActive(Lock_DCO1Morph),    trkB.getActiveLockValue(Lock_DCO1Morph, pB.dco1Morph),       pB.dco1Morph);
        const float curD1PWM_B     = updatePLockParam(currentPLockValues[1][Lock_DCO1PWM],      trkB.isLockActive(Lock_DCO1PWM),      trkB.getActiveLockValue(Lock_DCO1PWM, pB.dco1PWM),           pB.dco1PWM);
        const float curD1Fold_B    = updatePLockParam(currentPLockValues[1][Lock_DCO1Fold],     trkB.isLockActive(Lock_DCO1Fold),     trkB.getActiveLockValue(Lock_DCO1Fold, pB.dco1Fold),         pB.dco1Fold);
        const float curD1SawMix_B  = updatePLockParam(currentPLockValues[1][Lock_DCO1SawMix],   trkB.isLockActive(Lock_DCO1SawMix),   trkB.getActiveLockValue(Lock_DCO1SawMix, pB.dco1SawMix),     pB.dco1SawMix);

        const float curD2Morph_B   = updatePLockParam(currentPLockValues[1][Lock_DCO2Morph],    trkB.isLockActive(Lock_DCO2Morph),    trkB.getActiveLockValue(Lock_DCO2Morph, pB.dco2Morph),       pB.dco2Morph);
        const float curD2PWM_B     = updatePLockParam(currentPLockValues[1][Lock_DCO2PWM],      trkB.isLockActive(Lock_DCO2PWM),      trkB.getActiveLockValue(Lock_DCO2PWM, pB.dco2PWM),           pB.dco2PWM);
        const float curD2Level_B   = updatePLockParam(currentPLockValues[1][Lock_DCO2Level],    trkB.isLockActive(Lock_DCO2Level),    trkB.getActiveLockValue(Lock_DCO2Level, pB.dco2Level),       pB.dco2Level);
        const float curD2Semi_B    = updatePLockParam(currentPLockValues[1][Lock_DCO2Semi],     trkB.isLockActive(Lock_DCO2Semi),     trkB.getActiveLockValue(Lock_DCO2Semi, (float)pB.dco2Semi),  (float)pB.dco2Semi);
        const float curD2Cents_B   = updatePLockParam(currentPLockValues[1][Lock_DCO2Cents],    trkB.isLockActive(Lock_DCO2Cents),    trkB.getActiveLockValue(Lock_DCO2Cents, pB.dco2Cents),       pB.dco2Cents);

        const float curSubLvl_B    = updatePLockParam(currentPLockValues[1][Lock_SubLevel],     trkB.isLockActive(Lock_SubLevel),     trkB.getActiveLockValue(Lock_SubLevel, pB.subLevel),         pB.subLevel);
        const float curNseLvl_B    = updatePLockParam(currentPLockValues[1][Lock_NoiseLevel],   trkB.isLockActive(Lock_NoiseLevel),   trkB.getActiveLockValue(Lock_NoiseLevel, pB.noiseLevel),     pB.noiseLevel);

        const float curHpfCut_B    = updatePLockParam(currentPLockValues[1][Lock_HPFCutoff],    trkB.isLockActive(Lock_HPFCutoff),    trkB.getActiveLockValue(Lock_HPFCutoff, pB.hpfCutoff),       pB.hpfCutoff);
        const float curLpfCut_B    = updatePLockParam(currentPLockValues[1][Lock_LPFCutoff],    trkB.isLockActive(Lock_LPFCutoff),    trkB.getActiveLockValue(Lock_LPFCutoff, pB.lpfCutoff),       pB.lpfCutoff);
        const float curLpfRes_B    = updatePLockParam(currentPLockValues[1][Lock_LPFRes],       trkB.isLockActive(Lock_LPFRes),       trkB.getActiveLockValue(Lock_LPFRes, pB.lpfRes),             pB.lpfRes);
        const float curEnvMod_B    = updatePLockParam(currentPLockValues[1][Lock_EnvMod],       trkB.isLockActive(Lock_EnvMod),       trkB.getActiveLockValue(Lock_EnvMod, pB.envMod),             pB.envMod);

        const float curFBal_B      = updatePLockParam(currentPLockValues[1][Lock_FilterBalance],trkB.isLockActive(Lock_FilterBalance),trkB.getActiveLockValue(Lock_FilterBalance, pB.filterBalance),pB.filterBalance);
        const float curF2Cut_B     = updatePLockParam(currentPLockValues[1][Lock_F2Cutoff],     trkB.isLockActive(Lock_F2Cutoff),     trkB.getActiveLockValue(Lock_F2Cutoff, pB.f2Cutoff),         pB.f2Cutoff);
        const float curF2Reso_B    = updatePLockParam(currentPLockValues[1][Lock_F2Res],        trkB.isLockActive(Lock_F2Res),        trkB.getActiveLockValue(Lock_F2Res, pB.f2Res),               pB.f2Res);
        const float curF2EnvDepth_B= updatePLockParam(currentPLockValues[1][Lock_F2EnvDepth],   trkB.isLockActive(Lock_F2EnvDepth),   trkB.getActiveLockValue(Lock_F2EnvDepth, pB.f2EnvDepth),     pB.f2EnvDepth);

        const float curAuxPitch_B  = updatePLockParam(currentPLockValues[1][Lock_AuxToPitch],   trkB.isLockActive(Lock_AuxToPitch),   trkB.getActiveLockValue(Lock_AuxToPitch, pB.auxToPitch),     pB.auxToPitch);
        const float curAuxF1_B     = updatePLockParam(currentPLockValues[1][Lock_AuxToFilter1], trkB.isLockActive(Lock_AuxToFilter1), trkB.getActiveLockValue(Lock_AuxToFilter1, pB.auxToFilter1), pB.auxToFilter1);
        const float curAuxF2_B     = updatePLockParam(currentPLockValues[1][Lock_AuxToFilter2], trkB.isLockActive(Lock_AuxToFilter2), trkB.getActiveLockValue(Lock_AuxToFilter2, pB.auxToFilter2), pB.auxToFilter2);
        const float curAuxFold_B   = updatePLockParam(currentPLockValues[1][Lock_AuxToFold],    trkB.isLockActive(Lock_AuxToFold),    trkB.getActiveLockValue(Lock_AuxToFold, pB.auxToFold),       pB.auxToFold);
        const float curAuxPWM_B    = updatePLockParam(currentPLockValues[1][Lock_AuxToPWM],     trkB.isLockActive(Lock_AuxToPWM),     trkB.getActiveLockValue(Lock_AuxToPWM, pB.auxToPWM),         pB.auxToPWM);

        const float mmMasterPitch_A   = modMatrix.parts[0].getDestinationOffset(ModDestination::MasterPitch);
        const float mmOscPitch_A      = modMatrix.parts[0].getDestinationOffset(ModDestination::OscPitch);
        const float mmD2Pitch_A       = modMatrix.parts[0].getDestinationOffset(ModDestination::DCO2Pitch);
        const float mmMorph_A         = modMatrix.parts[0].getDestinationOffset(ModDestination::OscWaveformMorph);
        const float mmD2Morph_A       = modMatrix.parts[0].getDestinationOffset(ModDestination::DCO2Morph);
        const float mmPWM_A           = modMatrix.parts[0].getDestinationOffset(ModDestination::OscPWMDetune);
        const float mmD2PWM_A         = modMatrix.parts[0].getDestinationOffset(ModDestination::DCO2PWM);
        const float mmD1SawMix_A      = modMatrix.parts[0].getDestinationOffset(ModDestination::DCO1SawMix);
        const float mmD1SawDetune_A   = modMatrix.parts[0].getDestinationOffset(ModDestination::DCO1SawDetune);
        const float mmFold_A          = modMatrix.parts[0].getDestinationOffset(ModDestination::OscWavefold);
        const float mmD2Fold_A        = modMatrix.parts[0].getDestinationOffset(ModDestination::DCO2Fold);
        const float mmD2Level_A       = modMatrix.parts[0].getDestinationOffset(ModDestination::DCO2Level);
        const float mmFM_A            = modMatrix.parts[0].getDestinationOffset(ModDestination::FMAmount);
        const float mmSub_A           = modMatrix.parts[0].getDestinationOffset(ModDestination::SubOscLevel);
        const float mmNoise_A         = modMatrix.parts[0].getDestinationOffset(ModDestination::NoiseLevel);
        const float mmF1Cut_A         = modMatrix.parts[0].getDestinationOffset(ModDestination::Filter1Cutoff);
        const float mmF1Res_A         = modMatrix.parts[0].getDestinationOffset(ModDestination::Filter1Resonance);
        const float mmF1HPF_A         = modMatrix.parts[0].getDestinationOffset(ModDestination::Filter1HPF);
        const float mmF2Cut_A         = modMatrix.parts[0].getDestinationOffset(ModDestination::Filter2Cutoff);
        const float mmF2Res_A         = modMatrix.parts[0].getDestinationOffset(ModDestination::Filter2Resonance);
        const float mmFBal_A          = modMatrix.parts[0].getDestinationOffset(ModDestination::FilterBalance);

        const float mmMasterPitch_B   = modMatrix.parts[1].getDestinationOffset(ModDestination::MasterPitch);
        const float mmOscPitch_B      = modMatrix.parts[1].getDestinationOffset(ModDestination::OscPitch);
        const float mmD2Pitch_B       = modMatrix.parts[1].getDestinationOffset(ModDestination::DCO2Pitch);
        const float mmMorph_B         = modMatrix.parts[1].getDestinationOffset(ModDestination::OscWaveformMorph);
        const float mmD2Morph_B       = modMatrix.parts[1].getDestinationOffset(ModDestination::DCO2Morph);
        const float mmPWM_B           = modMatrix.parts[1].getDestinationOffset(ModDestination::OscPWMDetune);
        const float mmD2PWM_B         = modMatrix.parts[1].getDestinationOffset(ModDestination::DCO2PWM);
        const float mmD1SawMix_B      = modMatrix.parts[1].getDestinationOffset(ModDestination::DCO1SawMix);
        const float mmD1SawDetune_B   = modMatrix.parts[1].getDestinationOffset(ModDestination::DCO1SawDetune);
        const float mmFold_B          = modMatrix.parts[1].getDestinationOffset(ModDestination::OscWavefold);
        const float mmD2Fold_B        = modMatrix.parts[1].getDestinationOffset(ModDestination::DCO2Fold);
        const float mmD2Level_B       = modMatrix.parts[1].getDestinationOffset(ModDestination::DCO2Level);
        const float mmFM_B            = modMatrix.parts[1].getDestinationOffset(ModDestination::FMAmount);
        const float mmSub_B           = modMatrix.parts[1].getDestinationOffset(ModDestination::SubOscLevel);
        const float mmNoise_B         = modMatrix.parts[1].getDestinationOffset(ModDestination::NoiseLevel);
        const float mmF1Cut_B         = modMatrix.parts[1].getDestinationOffset(ModDestination::Filter1Cutoff);
        const float mmF1Res_B         = modMatrix.parts[1].getDestinationOffset(ModDestination::Filter1Resonance);
        const float mmF1HPF_B         = modMatrix.parts[1].getDestinationOffset(ModDestination::Filter1HPF);
        const float mmF2Cut_B         = modMatrix.parts[1].getDestinationOffset(ModDestination::Filter2Cutoff);
        const float mmF2Res_B         = modMatrix.parts[1].getDestinationOffset(ModDestination::Filter2Resonance);
        const float mmFBal_B          = modMatrix.parts[1].getDestinationOffset(ModDestination::FilterBalance);

        const float finalLpfCut_A      = std::clamp(curLpfCut_A * std::exp2(mmF1Cut_A * 4.0f), 20.0f, 20000.0f);
        const float finalLpfRes_A      = std::clamp(curLpfRes_A + (mmF1Res_A * 0.8f), 0.0f, 0.98f);
        const float finalHpfCut_A      = std::clamp(curHpfCut_A + (mmF1HPF_A * 500.0f), 10.0f, 1000.0f);
        const float finalD1Morph_A     = std::clamp(curD1Morph_A + mmMorph_A, 0.0f, 1.0f);
        const float finalD1PWM_A       = std::clamp(curD1PWM_A + (mmPWM_A * 0.45f), 0.05f, 0.95f);
        const float finalD1SawMix_A    = std::clamp(curD1SawMix_A + mmD1SawMix_A, 0.0f, 1.0f);
        const float finalD1SawDetune_A = std::clamp(pA.dco1SawDetune + mmD1SawDetune_A, 0.0f, 1.0f);
        const float finalD1Fold_A      = std::clamp(curD1Fold_A + mmFold_A, 0.0f, 1.0f);

        const float finalD2Morph_A     = std::clamp(curD2Morph_A + mmD2Morph_A + mmMorph_A * 0.5f, 0.0f, 1.0f);
        const float finalD2PWM_A       = std::clamp(curD2PWM_A + (mmD2PWM_A * 0.45f) + (mmPWM_A * 0.25f), 0.05f, 0.95f);
        const float finalD2Semi_A      = curD2Semi_A + (mmD2Pitch_A * 24.0f) + (mmOscPitch_A * 24.0f) + (mmMasterPitch_A * 12.0f);
        const float finalD2Cents_A     = std::clamp(curD2Cents_A, -50.0f, 50.0f);
        const float finalD2Level_A     = std::clamp(curD2Level_A + mmD2Level_A, 0.0f, 1.0f);
        const float finalD2Fold_A      = std::clamp(pA.dco2Fold + curAuxFold_A + mmD2Fold_A, 0.0f, 1.0f);
        const float finalXmod_A        = std::clamp(pA.xmodAmount + mmFM_A, 0.0f, 1.0f);

        const float finalSubLvl_A      = std::clamp(curSubLvl_A + mmSub_A, 0.0f, 1.0f);
        const float finalNseLvl_A      = std::clamp(curNseLvl_A + mmNoise_A, 0.0f, 1.0f);
        const float finalF2Cut_A       = (mmF2Cut_A != 0.0f) ? std::clamp(curF2Cut_A * std::exp2(mmF2Cut_A * 4.0f), 20.0f, 20000.0f) : curF2Cut_A;
        const float finalF2Reso_A      = std::clamp(curF2Reso_A + mmF2Res_A * 0.8f, 0.0f, 1.0f);
        const float finalFBal_A        = std::clamp(curFBal_A + mmFBal_A, 0.0f, 1.0f);

        const float finalLpfCut_B      = std::clamp(curLpfCut_B * std::exp2(mmF1Cut_B * 4.0f), 20.0f, 20000.0f);
        const float finalLpfRes_B      = std::clamp(curLpfRes_B + (mmF1Res_B * 0.8f), 0.0f, 0.98f);
        const float finalHpfCut_B      = std::clamp(curHpfCut_B + (mmF1HPF_B * 500.0f), 10.0f, 1000.0f);
        const float finalD1Morph_B     = std::clamp(curD1Morph_B + mmMorph_B, 0.0f, 1.0f);
        const float finalD1PWM_B       = std::clamp(curD1PWM_B + (mmPWM_B * 0.45f), 0.05f, 0.95f);
        const float finalD1SawMix_B    = std::clamp(curD1SawMix_B + mmD1SawMix_B, 0.0f, 1.0f);
        const float finalD1SawDetune_B = std::clamp(pB.dco1SawDetune + mmD1SawDetune_B, 0.0f, 1.0f);
        const float finalD1Fold_B      = std::clamp(curD1Fold_B + mmFold_B, 0.0f, 1.0f);

        const float finalD2Morph_B     = std::clamp(curD2Morph_B + mmD2Morph_B + mmMorph_B * 0.5f, 0.0f, 1.0f);
        const float finalD2PWM_B       = std::clamp(curD2PWM_B + (mmD2PWM_B * 0.45f) + (mmPWM_B * 0.25f), 0.05f, 0.95f);
        const float finalD2Semi_B      = curD2Semi_B + (mmD2Pitch_B * 24.0f) + (mmOscPitch_B * 24.0f) + (mmMasterPitch_B * 12.0f);
        const float finalD2Cents_B     = std::clamp(curD2Cents_B, -50.0f, 50.0f);
        const float finalD2Level_B     = std::clamp(curD2Level_B + mmD2Level_B, 0.0f, 1.0f);
        const float finalD2Fold_B      = std::clamp(pB.dco2Fold + curAuxFold_B + mmD2Fold_B, 0.0f, 1.0f);
        const float finalXmod_B        = std::clamp(pB.xmodAmount + mmFM_B, 0.0f, 1.0f);

        const float finalSubLvl_B      = std::clamp(curSubLvl_B + mmSub_B, 0.0f, 1.0f);
        const float finalNseLvl_B      = std::clamp(curNseLvl_B + mmNoise_B, 0.0f, 1.0f);
        const float finalF2Cut_B       = (mmF2Cut_B != 0.0f) ? std::clamp(curF2Cut_B * std::exp2(mmF2Cut_B * 4.0f), 20.0f, 20000.0f) : curF2Cut_B;
        const float finalF2Reso_B      = std::clamp(curF2Reso_B + mmF2Res_B * 0.8f, 0.0f, 1.0f);
        const float finalFBal_B        = std::clamp(curFBal_B + mmFBal_B, 0.0f, 1.0f);

        float sumA_L = 0.0f, sumA_R = 0.0f;
        float sumB_L = 0.0f, sumB_R = 0.0f;

        for (int vi = 0; vi < VoiceManager::NUM_VOICES; ++vi) {
            auto& voice = voices[static_cast<size_t>(vi)];
            if (!voice.isActive())
                continue;

            const int vPart = voiceManager.getVoicePart(vi);
            float vL = 0.0f, vR = 0.0f;

            if (vPart == 0) {
                voice.process(
                    finalD1Morph_A, finalD1PWM_A, pA.dco1Octave, finalD1Fold_A, finalD1SawMix_A,
                    finalD2Morph_A, finalD2PWM_A, finalD2Level_A, finalD2Semi_A, finalD2Cents_A,
                    pA.syncMode, finalXmod_A, pA.xmodMode,
                    pA.subOctave, finalSubLvl_A, finalNseLvl_A,
                    finalHpfCut_A, finalLpfCut_A, finalLpfRes_A, curEnvMod_A,
                    lfo1Val_A, pA.lfo1ToFilter, pA.lfo1ToPitch,
                    vL, vR,
                    finalF2Cut_A, finalF2Reso_A, pA.f2Mode, curF2EnvDepth_A,
                    pA.filterRouting, finalFBal_A,
                    curAuxPitch_A, curAuxF1_A, curAuxF2_A, curAuxFold_A, curAuxPWM_A,
                    pA.f2KeyTrack,
                    finalD1SawDetune_A, finalXmod_A, finalD2Fold_A,
                    pA.subWaveform
                );
                sumA_L += vL;
                sumA_R += vR;
            } else {
                voice.process(
                    finalD1Morph_B, finalD1PWM_B, pB.dco1Octave, finalD1Fold_B, finalD1SawMix_B,
                    finalD2Morph_B, finalD2PWM_B, finalD2Level_B, finalD2Semi_B, finalD2Cents_B,
                    pB.syncMode, finalXmod_B, pB.xmodMode,
                    pB.subOctave, finalSubLvl_B, finalNseLvl_B,
                    finalHpfCut_B, finalLpfCut_B, finalLpfRes_B, curEnvMod_B,
                    lfo1Val_B, pB.lfo1ToFilter, pB.lfo1ToPitch,
                    vL, vR,
                    finalF2Cut_B, finalF2Reso_B, pB.f2Mode, curF2EnvDepth_B,
                    pB.filterRouting, finalFBal_B,
                    curAuxPitch_B, curAuxF1_B, curAuxF2_B, curAuxFold_B, curAuxPWM_B,
                    pB.f2KeyTrack,
                    finalD1SawDetune_B, finalXmod_B, finalD2Fold_B,
                    pB.subWaveform
                );
                sumB_L += vL;
                sumB_R += vR;
            }
        }

        const float gainComp = voiceManager.getGainCompensation();

        if (fxRouting == 1) {
            outL[sample] = sumA_L * gainComp;
            outR[sample] = sumA_R * gainComp;
            if (sample < safeSamples) {
                dryBusL[sample] = sumB_L * gainComp;
                dryBusR[sample] = sumB_R * gainComp;
            }
        } else if (fxRouting == 2) {
            outL[sample] = sumB_L * gainComp;
            outR[sample] = sumB_R * gainComp;
            if (sample < safeSamples) {
                dryBusL[sample] = sumA_L * gainComp;
                dryBusR[sample] = sumA_R * gainComp;
            }
        } else {
            outL[sample] = (sumA_L + sumB_L) * gainComp;
            outR[sample] = (sumA_R + sumB_R) * gainComp;
        }
    }

    for (int i = 0; i < outMidiCount; ++i) {
        auto& ev = outMidiEvents[static_cast<size_t>(i)];
        if (ev.type == 0x90) midiMessages.addEvent(juce::MidiMessage::noteOn(ev.channel, ev.note, ev.vel), ev.sample);
        else midiMessages.addEvent(juce::MidiMessage::noteOff(ev.channel, ev.note, ev.vel), ev.sample);
    }

    if (chorusM > 0) chorus.processBlock(outL, outR, numSamples);
    if (dMix > 0.001f) delay.processBlock(outL, outR, numSamples);
    if (rMix > 0.001f) reverb.processBlock(outL, outR, numSamples);

    if (fxRouting == 1 || fxRouting == 2) {
        for (int s = 0; s < safeSamples; ++s) {
            outL[s] += dryBusL[s];
            outR[s] += dryBusR[s];
        }
    }

    const float masterVol = apvts.getRawParameterValue("masterVolume")->load();
    for (int sample = 0; sample < numSamples; ++sample) {
        outL[sample] = std::tanh(outL[sample] * masterVol);
        outR[sample] = std::tanh(outR[sample] * masterVol);
    }

    oscilloscopeQueue.pushBuffer(buffer, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* Simple116AudioProcessor::createEditor() {
    return new Simple116AudioProcessorEditor(*this);
}

void Simple116AudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto patchStr = createPatchXml(currentPresetName, true);
    destData.append(patchStr.toRawUTF8(), patchStr.getNumBytesAsUTF8());
}

void Simple116AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    juce::String patchStr(static_cast<const char*>(data), static_cast<size_t>(sizeInBytes));
    loadPatchXml(patchStr, true);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new Simple116AudioProcessor();
}