#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_processors/juce_audio_processors.h>
#endif

#if __has_include("PluginProcessor.h")
 #include "PluginProcessor.h"
#elif __has_include("../PluginProcessor.h")
 #include "../PluginProcessor.h"
#endif

#include "SequencerEngine.h"
#include "ModulationMatrix.h"
#include <algorithm>
#include <cstdint>

// =============================================================================
// S-116 Flagship Factory Preset Soundbank (64 Handcrafted Presets)
// Fully categorized: Basses, Leads, Pads, Keys, Percussion, Sequences, FX
// Deeply integrated with 16-Slot Mod Matrix & Universal P-Lock Matrix
// =============================================================================
class PresetBank
{
public:
    static juce::StringArray getPresetNames()
    {
        return {
            // 01 Basses (0 - 9)
            "Init Patch",
            "01 Basses / Model D Acid 303",
            "01 Basses / Neurofunk Reese",
            "01 Basses / Analog 808 Sub Boom",
            "01 Basses / 90s M1 Organ Bass",
            "01 Basses / Cyberpunk Slap Bass",
            "01 Basses / West Coast Sub Pluck",
            "01 Basses / Solid FM House Bass",
            "01 Basses / 2000s Donk Bass",
            "01 Basses / Vintage Juno Finger Bass",

            // 02 Leads (10 - 19)
            "02 Leads / JP-8000 Supersaw Lead",
            "02 Leads / Blade Runner CS-80 Brass",
            "02 Leads / Vangelis Expressive Pitch",
            "02 Leads / Hyperpop Screamer",
            "02 Leads / Modular Sync Scream",
            "02 Leads / Neo-Tokyo Cyber Lead",
            "02 Leads / OB-Xa Detuned 2x Unison",
            "02 Leads / 90s Rave Alpha Hoover",
            "02 Leads / 2000s Euro Trance Lead",
            "02 Leads / Trance Euphoria Lead",

            // 03 Pads & Strings (20 - 29)
            "03 Pads / Juno-106 Lush Strings",
            "03 Pads / Vaporwave Dreamscape",
            "03 Pads / Ambient Shimmer Swell",
            "03 Pads / Celestial Aurora Pad",
            "03 Pads / CS-80 Majestic Ensemble",
            "03 Pads / Glass Prism Texture",
            "03 Pads / Dark Matter Choir",
            "03 Pads / Warm Analog Horizon",
            "03 Pads / Arctic Ice Crystals",
            "03 Pads / Bi-Timbral Titan Symphony",

            // 04 Keys & Plucks (30 - 38)
            "04 Keys & Plucks / Chillwave Lo-Fi Keys",
            "04 Keys & Plucks / DX FM Crystal E-Piano",
            "04 Keys & Plucks / Karplus Modular Pluck",
            "04 Keys & Plucks / 90s Dreamhouse Pizzicato",
            "04 Keys & Plucks / Glass Chimes Matrix",
            "04 Keys & Plucks / Neo-Classical Kalimba",
            "04 Keys & Plucks / 90s Eurodance Piano",
            "04 Keys & Plucks / Dub Techno Chord Stab",
            "04 Keys & Plucks / Neo-Soul Analog Keys",

            // 05 Percussion & Drums (39 - 47)
            "05 Percussion / Analog 808 Kick",
            "05 Percussion / 909 Dance Kick",
            "05 Percussion / Electro Analog Snare",
            "05 Percussion / Crisp Metallic Hi-Hat",
            "05 Percussion / Modular Tribal Tom",
            "05 Percussion / 909 Dance Clap",
            "05 Percussion / Industrial Metallic Perc",
            "05 Percussion / West Coast Buchla Bongo",
            "05 Percussion / Noise Shaker Burst",

            // 06 Sequences & Arps (48 - 55)
            "06 Sequences / Acid 303 Bass Sequence",
            "06 Sequences / 90s Eurodance Rave Bass",
            "06 Sequences / 8-Bit Chiptune Arp",
            "06 Sequences / Shimmering Crystal Arp",
            "06 Sequences / Modular Glitch Sequence",
            "06 Sequences / Techno Industrial Pulser",
            "06 Sequences / Berlin School Space Arp",
            "06 Sequences / Cyberpunk Bi-Timbral Split",

            // 07 FX, Textures & Drones (56 - 63)
            "07 FX & Drones / Cinematic Sub Drone",
            "07 FX & Drones / Laser Beam Pitch Drop",
            "07 FX & Drones / UFO Sci-Fi Theremin",
            "07 FX & Drones / Alien Transmission S&H",
            "07 FX & Drones / Dark Abyss Sub Shaker",
            "07 FX & Drones / Metallic FM Industrial Ring",
            "07 FX & Drones / Modular Self-Oscillation",
            "07 FX & Drones / Cosmic Solar Wind Pad"
        };
    }

    static void loadPreset (int index,
                            TimbrePatch& pA,
                            TimbrePatch& pB,
                            SequencerEngine& sequencer,
                            BiTimbralModMatrix& modMatrix,
                            juce::AudioProcessorValueTreeState& apvts,
                            juce::String& outPatchName)
    {
        index = std::clamp(index, 0, 63);
        auto names = getPresetNames();
        outPatchName = names[index];

        // 1. Reset baseline patches to clean defaults
        pA = TimbrePatch();
        pB = TimbrePatch();

        // 2. Reset Modulation Matrix & Sequencer Tracks
        modMatrix.reset();
        sequencer.reset();

        // Mute and clear Track B by default to prevent ghost background droning
        auto& trkB = sequencer.getTrack(TrackID::TrackB);
        trkB.setMute(true);
        for (int s = 0; s < SequencerTrack::MAX_STEPS; ++s)
            trkB.getStep(s).active = false;

        auto setParam = [&apvts](const juce::String& id, float val) {
            if (auto* p = apvts.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        };

        auto setSlotA = [&modMatrix](int slot, ModSource src, ModDestination dst, float amt,
                                     ModSource via = ModSource::None, float viaAmt = 1.0f) {
            modMatrix.parts[0].setSlot(slot, src, dst, amt, via, viaAmt, true);
        };

        auto setSlotB = [&modMatrix](int slot, ModSource src, ModDestination dst, float amt,
                                     ModSource via = ModSource::None, float viaAmt = 1.0f) {
            modMatrix.parts[1].setSlot(slot, src, dst, amt, via, viaAmt, true);
        };

        // Standard Common Defaults
        setParam("syncToHost", 1.0f);
        setParam("internalBpm", 120.0f);
        setParam("seqPlay", 0.0f);
        setParam("seqRec", 0.0f);
        setParam("seqDirection", 0.0f);
        setParam("seqDivision", 3.0f); // 1/16th
        setParam("seqSwing", 0.0f);
        setParam("seqGate", 0.75f);
        setParam("seqPages", 3.0f);    // 4 pages (64 steps)
        setParam("arpEnable", 0.0f);
        setParam("arpMode", 0.0f);
        setParam("chordEnable", 0.0f);
        setParam("chordType", 0.0f);
        setParam("chorusMode", 0.0f);
        setParam("delayMix", 0.0f);
        setParam("delayFeedback", 0.40f);
        setParam("delayDamp", 0.25f);
        setParam("delaySync", 6.0f);   // 1/4 note
        setParam("delaySyncEnable", 1.0f);
        setParam("delayPingPong", 1.0f);
        setParam("reverbMix", 0.0f);
        setParam("reverbSize", 0.65f);
        setParam("reverbDamp", 0.30f);
        setParam("fxRouting", 0.0f);
        setParam("unisonDetune", 18.0f);
        setParam("unisonSpread", 0.85f);
        setParam("playMode", 1.0f);    // Poly 16 [Single] default for rich 16-voice polyphony

        pA.playMode = 1;
        pB.playMode = 1;

        for (int i = 1; i <= 16; ++i) {
            setParam("vPan" + juce::String(i), 0.0f);
            setParam("vTune" + juce::String(i), 0.0f);
        }

        switch (index) {
            // =================================================================
            // 01 BASSES (0 - 9)
            // =================================================================
            case 0: // Init Patch
                pA.dco1Morph = 0.25f;
                pA.dco1Octave = 2;
                pA.lpfCutoff = 3000.0f;
                pB = pA;
                break;

            case 1: // Model D Acid 303
                pA.dco1Morph = 0.50f; pA.dco1Octave = 1;
                pA.subLevel = 0.35f; pA.subOctave = 0;
                pA.lpfCutoff = 650.0f; pA.lpfRes = 0.85f; pA.envMod = 0.78f;
                pA.ampAttack = 0.002f; pA.ampDecay = 0.28f; pA.ampSustain = 0.05f; pA.ampRelease = 0.08f;
                pA.filtAttack = 0.003f; pA.filtDecay = 0.25f; pA.filtSustain = 0.10f;
                pA.playMode = 3; // Mono Legato
                setParam("playMode", 3.0f);
                pA.glideTime = 0.065f; pA.glideEnable = true;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.45f);
                setSlotA(1, ModSource::ModWheel, ModDestination::Filter1Resonance, 0.30f);
                setSlotA(2, ModSource::Aftertouch, ModDestination::Filter1Cutoff, 0.35f);
                pB = pA;
                setParam("delayMix", 0.20f); setParam("delayFeedback", 0.35f);
                break;

            case 2: // Neurofunk Reese
                pA.dco1Morph = 0.50f; pA.dco1Octave = 1; pA.dco1SawMix = 0.40f; pA.dco1SawDetune = 0.65f;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.95f; pA.dco2Cents = 15.0f;
                pA.subLevel = 0.50f; pA.subOctave = 0;
                pA.f2Mode = 3; // Notch Filter
                pA.f2Cutoff = 1100.0f; pA.f2Res = 0.70f; pA.filterBalance = 0.85f;
                pA.lfo1Rate = 0.8f; pA.lfo1ToFilter = 0.35f;
                pA.ampAttack = 0.008f; pA.ampDecay = 0.45f; pA.ampSustain = 0.85f; pA.ampRelease = 0.20f;
                pA.playMode = 3; // Mono Legato
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::LFO1, ModDestination::Filter2Cutoff, 0.35f);
                setSlotA(1, ModSource::ModWheel, ModDestination::FilterBalance, 0.40f);
                setSlotA(2, ModSource::NoiseSampleHold, ModDestination::Pan, 0.25f);
                setSlotA(3, ModSource::Velocity, ModDestination::VCAVolume, 0.20f);
                pB = pA;
                break;

            case 3: // Analog 808 Sub Boom
                pA.dco1Morph = 0.00f; pA.dco1Octave = 0; // Pure deep fundamental Sine
                pA.dco1Fold = 0.0f;
                pA.subLevel = 0.0f;
                pA.dco2Level = 0.0f;
                pA.auxAttack = 0.001f; pA.auxDecay = 0.032f; pA.auxSustain = 0.0f; pA.auxToPitch = 14.0f;
                pA.lpfCutoff = 260.0f; pA.lpfRes = 0.05f;
                pA.ampAttack = 0.001f; pA.ampDecay = 1.15f; pA.ampSustain = 0.0f; pA.ampRelease = 0.35f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::VCAVolume, 0.35f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.20f);
                pB = pA;
                break;

            case 4: // 90s M1 Organ Bass (The definitive 90s House & UK Garage organ bassline)
                pA.dco1Morph = 0.00f; pA.dco1Octave = 1; // Pure 16' sine fundamental
                pA.dco2Morph = 0.00f; pA.dco2Level = 0.65f; pA.dco2Semi = 19; // Drawbar 5th/octave overtone
                pA.subLevel = 0.35f; pA.subOctave = 0;
                pA.lpfCutoff = 3200.0f; pA.lpfRes = 0.20f; pA.envMod = 0.50f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.30f; pA.ampSustain = 0.05f; pA.ampRelease = 0.12f;
                pA.filtAttack = 0.001f; pA.filtDecay = 0.22f; pA.filtSustain = 0.0f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.50f);
                setSlotA(1, ModSource::Velocity, ModDestination::VCAVolume, 0.30f);
                pB = pA;
                setParam("chorusMode", 1.0f);
                break;

            case 5: // Cyberpunk Slap Bass
                pA.dco1Morph = 0.75f; pA.dco1PWM = 0.65f; pA.dco1Octave = 1;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.50f; pA.dco2Semi = 12;
                pA.lpfCutoff = 1350.0f; pA.lpfRes = 0.38f; pA.envMod = 0.58f;
                pA.ampAttack = 0.002f; pA.ampDecay = 0.22f; pA.ampSustain = 0.15f; pA.ampRelease = 0.12f;
                pA.filtAttack = 0.002f; pA.filtDecay = 0.16f; pA.filtSustain = 0.05f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.55f);
                setSlotA(1, ModSource::Velocity, ModDestination::OscPWMDetune, 0.30f);
                setSlotA(2, ModSource::ModWheel, ModDestination::Filter1Resonance, 0.35f);
                pB = pA;
                setParam("chorusMode", 1.0f);
                break;

            case 6: // West Coast Sub Pluck
                pA.dco1Morph = 0.00f; pA.dco1Octave = 0;
                pA.dco1Fold = 0.28f;
                pA.subLevel = 0.45f; pA.subOctave = 0;
                pA.auxAttack = 0.001f; pA.auxDecay = 0.025f; pA.auxSustain = 0.0f; pA.auxToPitch = 5.0f;
                pA.auxToFold = 0.35f;
                pA.lpfCutoff = 520.0f; pA.lpfRes = 0.25f; pA.envMod = 0.60f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.38f; pA.ampSustain = 0.0f; pA.ampRelease = 0.16f;
                pA.filtAttack = 0.001f; pA.filtDecay = 0.24f; pA.filtSustain = 0.02f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.50f);
                setSlotA(1, ModSource::Velocity, ModDestination::OscWavefold, 0.30f);
                setSlotA(2, ModSource::KeyTracking, ModDestination::AmpEnvDecay, -0.20f);
                pB = pA;
                break;

            case 7: // Solid FM House Bass
                pA.dco1Morph = 0.00f; pA.dco1Octave = 1;
                pA.dco2Morph = 0.00f; pA.dco2Level = 0.75f; pA.dco2Semi = 12;
                pA.xmodAmount = 0.38f; pA.xmodMode = 0;
                pA.lpfCutoff = 1750.0f; pA.lpfRes = 0.22f; pA.envMod = 0.45f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.28f; pA.ampSustain = 0.15f; pA.ampRelease = 0.12f;
                pA.filtAttack = 0.001f; pA.filtDecay = 0.19f; pA.filtSustain = 0.08f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::FMAmount, 0.45f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.40f);
                pB = pA;
                break;

            case 8: // 2000s Donk Bass (Punchy, bouncy UK hard-house / donk bass)
                pA.dco1Morph = 0.00f; pA.dco1Octave = 1;
                pA.dco2Morph = 0.25f; pA.dco2Level = 0.80f; pA.dco2Semi = 12;
                pA.xmodAmount = 0.55f; pA.xmodMode = 0; // Linear FM rubbery bite
                pA.auxAttack = 0.001f; pA.auxDecay = 0.035f; pA.auxToPitch = 12.0f; // Bouncy pitch drop
                pA.lpfCutoff = 1900.0f; pA.lpfRes = 0.40f; pA.envMod = 0.65f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.25f; pA.ampSustain = 0.0f; pA.ampRelease = 0.10f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::FMAmount, 0.55f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.45f);
                pB = pA;
                break;

            case 9: // Vintage Juno Finger Bass
                pA.dco1Morph = 0.75f; pA.dco1PWM = 0.50f; pA.dco1Octave = 1;
                pA.subLevel = 0.60f; pA.subOctave = 0;
                pA.lpfCutoff = 1400.0f; pA.lpfRes = 0.20f; pA.envMod = 0.45f;
                pA.ampAttack = 0.005f; pA.ampDecay = 0.35f; pA.ampSustain = 0.40f; pA.ampRelease = 0.18f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.45f);
                setSlotA(1, ModSource::Velocity, ModDestination::AmpEnvDecay, 0.20f);
                setSlotA(2, ModSource::KeyTracking, ModDestination::Filter1Cutoff, 0.40f);
                pB = pA;
                setParam("chorusMode", 1.0f);
                break;

            // =================================================================
            // 02 LEADS (10 - 19)
            // =================================================================
            case 10: // JP-8000 Supersaw Lead
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.75f; pA.dco1SawDetune = 0.68f;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.55f; pA.dco2Cents = 8.0f;
                pA.lpfCutoff = 7500.0f; pA.lpfRes = 0.18f; pA.envMod = 0.30f;
                pA.ampAttack = 0.008f; pA.ampDecay = 0.45f; pA.ampSustain = 0.85f; pA.ampRelease = 0.40f;
                setSlotA(0, ModSource::ModWheel, ModDestination::LFO1Rate, 0.35f);
                setSlotA(1, ModSource::Aftertouch, ModDestination::Filter1Cutoff, 0.30f);
                setSlotA(2, ModSource::Velocity, ModDestination::VCAVolume, 0.25f);
                setSlotA(3, ModSource::LFO2, ModDestination::Pan, 0.30f);
                pB = pA;
                setParam("chorusMode", 2.0f);
                setParam("delayMix", 0.30f); setParam("delayFeedback", 0.45f);
                setParam("reverbMix", 0.28f); setParam("reverbSize", 0.75f);
                break;

            case 11: // Blade Runner CS-80 Brass
                pA.dco1Morph = 0.50f; pA.dco1PWM = 0.55f;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.70f; pA.dco2Cents = -8.0f;
                pA.lpfCutoff = 1900.0f; pA.envMod = 0.50f;
                pA.f2Cutoff = 2200.0f; pA.f2Res = 0.30f; pA.filterBalance = 0.30f;
                pA.ampAttack = 0.16f; pA.ampDecay = 1.2f; pA.ampSustain = 0.75f; pA.ampRelease = 1.5f;
                pA.filtAttack = 0.20f; pA.filtDecay = 1.4f; pA.filtSustain = 0.40f; pA.filtRelease = 1.4f;
                pA.glideTime = 0.045f; pA.glideEnable = true;
                setSlotA(0, ModSource::Aftertouch, ModDestination::Filter1Cutoff, 0.55f);
                setSlotA(1, ModSource::Velocity, ModDestination::FilterEnvAttack, -0.30f);
                setSlotA(2, ModSource::ModWheel, ModDestination::LFO1Rate, 0.40f);
                setSlotA(3, ModSource::LFO2, ModDestination::FilterBalance, 0.25f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("delayMix", 0.25f); setParam("reverbMix", 0.35f);
                break;

            case 12: // Vangelis Expressive Pitch
                pA.dco1Morph = 0.50f; pA.dco2Morph = 0.50f; pA.dco2Level = 0.65f; pA.dco2Cents = 7.0f;
                pA.lpfCutoff = 2400.0f; pA.lpfRes = 0.25f; pA.envMod = 0.40f;
                pA.ampAttack = 0.08f; pA.ampDecay = 0.80f; pA.ampSustain = 0.85f; pA.ampRelease = 0.80f;
                pA.glideTime = 0.04f; pA.glideEnable = true;
                setSlotA(0, ModSource::Aftertouch, ModDestination::Filter1Cutoff, 0.60f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.40f);
                setSlotA(2, ModSource::ModWheel, ModDestination::DCO2Pitch, 0.20f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("reverbMix", 0.35f);
                break;

            case 13: // Hyperpop Screamer (Massive, powerful, aggressive screaming lead with thick body)
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.85f; pA.dco1SawDetune = 0.62f; // Wall-of-sound supersaw
                pA.dco1Fold = 0.35f;                                                   // Thick harmonic bite
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.85f; pA.dco2Semi = 12;          // Searing octave sync
                pA.dco2Fold = 0.40f;
                pA.syncMode = true;
                pA.subLevel = 0.40f; pA.subOctave = 0;                                 // Foundation body
                pA.lpfCutoff = 4500.0f; pA.lpfRes = 0.45f; pA.envMod = 0.40f;
                pA.ampAttack = 0.002f; pA.ampDecay = 0.40f; pA.ampSustain = 0.85f; pA.ampRelease = 0.20f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::ModWheel, ModDestination::OscWavefold, 0.45f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.50f);
                setSlotA(2, ModSource::Aftertouch, ModDestination::Filter1Resonance, 0.35f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("delayMix", 0.28f); setParam("reverbMix", 0.22f);
                break;

            case 14: // Modular Sync Scream
                pA.dco1Morph = 0.50f; pA.dco2Morph = 0.50f; pA.dco2Level = 0.95f;
                pA.syncMode = true; pA.dco2Semi = 14;
                pA.auxAttack = 0.008f; pA.auxDecay = 0.42f; pA.auxSustain = 0.0f; pA.auxToPitch = 28.0f;
                pA.lpfCutoff = 4800.0f; pA.lpfRes = 0.45f;
                pA.ampAttack = 0.004f; pA.ampDecay = 0.50f; pA.ampSustain = 0.85f; pA.ampRelease = 0.30f;
                setSlotA(0, ModSource::AuxEnv, ModDestination::DCO2Pitch, 0.70f);
                setSlotA(1, ModSource::ModWheel, ModDestination::DCO2Pitch, 0.50f);
                setSlotA(2, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.40f);
                pB = pA;
                setParam("delayMix", 0.22f); setParam("reverbMix", 0.20f);
                break;

            case 15: // Neo-Tokyo Cyber Lead
                pA.dco1Morph = 0.35f; pA.dco1Octave = 2;
                pA.dco2Morph = 0.00f; pA.dco2Level = 0.65f; pA.dco2Semi = 12;
                pA.xmodAmount = 0.22f; pA.xmodMode = 0;
                pA.hpfCutoff = 180.0f;
                pA.lpfCutoff = 4200.0f; pA.lpfRes = 0.32f; pA.envMod = 0.30f;
                pA.ampAttack = 0.006f; pA.ampDecay = 0.45f; pA.ampSustain = 0.80f; pA.ampRelease = 0.40f;
                pA.glideTime = 0.035f; pA.glideEnable = true;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.50f);
                setSlotA(1, ModSource::ModWheel, ModDestination::FMAmount, 0.35f);
                setSlotA(2, ModSource::LFO2, ModDestination::Pan, 0.30f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("delayMix", 0.32f); setParam("reverbMix", 0.30f);
                break;

            case 16: // OB-Xa Detuned 2x Unison
                pA.dco1Morph = 0.50f; pA.dco2Morph = 0.50f; pA.dco2Level = 0.75f; pA.dco2Cents = -12.0f;
                pA.f2Cutoff = 2100.0f; pA.f2Res = 0.25f; pA.filterBalance = 0.80f;
                pA.ampAttack = 0.10f; pA.ampDecay = 0.90f; pA.ampSustain = 0.70f; pA.ampRelease = 0.70f;
                pA.playMode = 5;
                setParam("playMode", 5.0f);
                setSlotA(0, ModSource::ModWheel, ModDestination::Filter2Cutoff, 0.50f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter2Cutoff, 0.40f);
                setSlotA(2, ModSource::Aftertouch, ModDestination::Filter2Resonance, 0.25f);
                pB = pA;
                setParam("unisonDetune", 24.0f); setParam("unisonSpread", 0.85f);
                break;

            case 17: // 90s Rave Alpha Hoover (Legendary Alpha Juno / Joey Beltram Mentasm rave hoover)
                pA.dco1Morph = 0.75f; pA.dco1PWM = 0.60f; pA.dco1Octave = 1;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.80f; pA.dco2Cents = -16.0f;
                pA.subLevel = 0.50f; pA.subOctave = 0;
                pA.auxAttack = 0.005f; pA.auxDecay = 0.45f; pA.auxSustain = 0.0f; pA.auxToPitch = -24.0f; // Hoover dive
                pA.lpfCutoff = 3800.0f; pA.lpfRes = 0.35f; pA.envMod = 0.45f;
                pA.ampAttack = 0.005f; pA.ampDecay = 0.60f; pA.ampSustain = 0.85f; pA.ampRelease = 0.30f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::ModWheel, ModDestination::OscPWMDetune, 0.45f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.50f);
                pB = pA;
                setParam("chorusMode", 3.0f); setParam("reverbMix", 0.30f);
                break;

            case 18: // 2000s Euro Trance Lead (Ferry Corsten / Rank 1 soaring euphoric trance lead)
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.85f; pA.dco1SawDetune = 0.75f;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.70f; pA.dco2Semi = 7; pA.dco2Cents = 9.0f;
                pA.lpfCutoff = 6500.0f; pA.lpfRes = 0.25f; pA.envMod = 0.40f;
                pA.ampAttack = 0.004f; pA.ampDecay = 0.55f; pA.ampSustain = 0.85f; pA.ampRelease = 0.45f;
                pA.glideTime = 0.035f; pA.glideEnable = true;
                setSlotA(0, ModSource::ModWheel, ModDestination::Filter1Cutoff, 0.55f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.40f);
                pB = pA;
                setParam("chorusMode", 2.0f); setParam("delayMix", 0.35f); setParam("delaySync", 5.0f);
                setParam("reverbMix", 0.35f); setParam("reverbSize", 0.80f);
                break;

            case 19: // Trance Euphoria Lead
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.75f; pA.dco1SawDetune = 0.70f;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.55f; pA.dco2Semi = 12;
                pA.lpfCutoff = 8500.0f; pA.lpfRes = 0.15f;
                pA.ampAttack = 0.005f; pA.ampDecay = 0.50f; pA.ampSustain = 0.85f; pA.ampRelease = 0.40f;
                setSlotA(0, ModSource::ModWheel, ModDestination::Filter1Cutoff, 0.55f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.35f);
                setSlotA(2, ModSource::Aftertouch, ModDestination::DCO1SawMix, 0.30f);
                pB = pA;
                setParam("chorusMode", 2.0f); setParam("delayMix", 0.30f); setParam("reverbMix", 0.30f);
                break;

            // =================================================================
            // 03 PADS & STRINGS (20 - 29) (Calibrated Poly Headroom)
            // =================================================================
            case 20: // Juno-106 Lush Strings (Authentic Saw + slow LFO PWM Pulse with BBD Chorus I+II)
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.60f; pA.dco1SawDetune = 0.25f; // Pure vintage Saw
                pA.dco2Morph = 0.75f; pA.dco2PWM = 0.55f; pA.dco2Level = 0.50f; pA.dco2Cents = 6.0f; // Lush PWM
                pA.subLevel = 0.25f; pA.subOctave = 0;                                 // Controlled sub
                pA.hpfCutoff = 25.0f;                                                  // Position 1 bass boost
                pA.lpfCutoff = 2500.0f; pA.lpfRes = 0.22f; pA.envMod = 0.30f;         // Warm vintage IR3109
                pA.filtAttack = 0.45f; pA.filtDecay = 1.2f; pA.filtSustain = 0.75f; pA.filtRelease = 1.4f;
                pA.ampAttack = 0.38f; pA.ampDecay = 1.2f; pA.ampSustain = 0.80f; pA.ampRelease = 1.3f;
                setSlotA(0, ModSource::ModWheel, ModDestination::Filter1Cutoff, 0.50f);
                setSlotA(1, ModSource::VoiceAlternator, ModDestination::Pan, 0.35f);
                setSlotA(2, ModSource::KeyTracking, ModDestination::Filter1Cutoff, 0.30f);
                pB = pA;
                setParam("chorusMode", 3.0f); // Dual Chorus I+II
                setParam("reverbMix", 0.32f); setParam("reverbSize", 0.75f);
                break;

            case 21: // Vaporwave Dreamscape
                pA.dco1Morph = 0.40f; pA.dco1PWM = 0.50f;
                pA.dco2Morph = 0.30f; pA.dco2Level = 0.50f; pA.dco2Semi = 7; pA.dco2Cents = -11.0f;
                pA.hpfCutoff = 45.0f;
                pA.lpfCutoff = 1950.0f; pA.lpfRes = 0.18f;
                pA.lfo1Rate = 0.35f; pA.lfo1ToPitch = 0.035f;
                pA.ampAttack = 0.65f; pA.ampDecay = 1.4f; pA.ampSustain = 0.75f; pA.ampRelease = 1.6f;
                setSlotA(0, ModSource::LFO2, ModDestination::Filter1Cutoff, 0.30f);
                setSlotA(1, ModSource::LFO2, ModDestination::Pan, 0.40f);
                pB = pA;
                setParam("chorusMode", 2.0f); setParam("delayMix", 0.28f); setParam("reverbMix", 0.40f);
                break;

            case 22: // Ambient Shimmer Swell
                pA.dco1Morph = 0.50f; pA.dco2Morph = 0.25f; pA.dco2Level = 0.45f; pA.dco2Semi = 12;
                pA.filterRouting = true; pA.filterBalance = 0.50f;
                pA.f2Mode = 1; pA.f2Cutoff = 2400.0f; pA.f2Res = 0.50f;
                pA.ampAttack = 0.50f; pA.ampDecay = 1.2f; pA.ampSustain = 0.70f; pA.ampRelease = 1.6f;
                setSlotA(0, ModSource::LFO3, ModDestination::FilterBalance, 0.35f);
                setSlotA(1, ModSource::ModWheel, ModDestination::Filter2Cutoff, 0.50f);
                setSlotA(2, ModSource::VoiceAlternator, ModDestination::Pan, 0.45f);
                pB = pA;
                setParam("delayMix", 0.35f); setParam("delaySync", 5.0f); setParam("reverbMix", 0.42f);
                break;

            case 23: // Celestial Aurora Pad
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.55f; pA.dco1SawDetune = 0.40f;
                pA.dco2Morph = 0.20f; pA.dco2Level = 0.50f; pA.dco2Semi = 12; pA.dco2Cents = 7.0f;
                pA.filterRouting = true; pA.filterBalance = 0.45f;
                pA.lpfCutoff = 2200.0f; pA.lpfRes = 0.22f;
                pA.f2Mode = 1; pA.f2Cutoff = 3200.0f; pA.f2Res = 0.40f;
                pA.ampAttack = 0.75f; pA.ampDecay = 1.6f; pA.ampSustain = 0.80f; pA.ampRelease = 1.8f;
                setSlotA(0, ModSource::LFO1, ModDestination::Filter2Cutoff, 0.35f);
                setSlotA(1, ModSource::LFO2, ModDestination::Pan, 0.40f);
                setSlotA(2, ModSource::ModWheel, ModDestination::Filter1Cutoff, 0.50f);
                pB = pA;
                setParam("chorusMode", 2.0f); setParam("delayMix", 0.28f); setParam("reverbMix", 0.40f);
                break;

            case 24: // CS-80 Majestic Ensemble
                pA.dco1Morph = 0.50f; pA.dco2Morph = 0.50f; pA.dco2Level = 0.65f; pA.dco2Cents = -9.0f;
                pA.lpfCutoff = 2400.0f; pA.lpfRes = 0.18f; pA.envMod = 0.30f;
                pA.ampAttack = 0.55f; pA.ampDecay = 1.4f; pA.ampSustain = 0.75f; pA.ampRelease = 1.4f;
                setSlotA(0, ModSource::Aftertouch, ModDestination::Filter1Cutoff, 0.45f);
                setSlotA(1, ModSource::ModWheel, ModDestination::Filter1Cutoff, 0.40f);
                setSlotA(2, ModSource::VoiceAlternator, ModDestination::Pan, 0.35f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("reverbMix", 0.35f); setParam("reverbSize", 0.80f);
                break;

            case 25: // Glass Prism Texture
                pA.dco1Morph = 0.00f; pA.dco2Morph = 0.25f; pA.dco2Level = 0.55f; pA.dco2Semi = 24;
                pA.xmodAmount = 0.22f; pA.xmodMode = 0;
                pA.lpfCutoff = 5500.0f; pA.lpfRes = 0.15f;
                pA.ampAttack = 0.40f; pA.ampDecay = 1.2f; pA.ampSustain = 0.65f; pA.ampRelease = 1.5f;
                setSlotA(0, ModSource::LFO2, ModDestination::FMAmount, 0.30f);
                setSlotA(1, ModSource::LFO3, ModDestination::Pan, 0.35f);
                pB = pA;
                setParam("delayMix", 0.28f); setParam("reverbMix", 0.40f);
                break;

            case 26: // Dark Matter Choir
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.50f; pA.dco1SawDetune = 0.45f;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.55f; pA.dco2Semi = -12;
                pA.f2Mode = 3; pA.f2Cutoff = 1500.0f; pA.f2Res = 0.40f; pA.filterBalance = 0.70f;
                pA.ampAttack = 0.80f; pA.ampDecay = 1.8f; pA.ampSustain = 0.85f; pA.ampRelease = 1.8f;
                setSlotA(0, ModSource::LFO1, ModDestination::Filter2Cutoff, 0.30f);
                setSlotA(1, ModSource::ModWheel, ModDestination::Filter2Resonance, 0.40f);
                pB = pA;
                setParam("reverbMix", 0.65f); setParam("reverbDamp", 0.18f); setParam("reverbSize", 0.90f);
                break;

            case 27: // Warm Analog Horizon
                pA.dco1Morph = 0.50f; pA.dco1PWM = 0.50f; pA.subLevel = 0.30f;
                pA.lpfCutoff = 1800.0f; pA.lpfRes = 0.10f;
                pA.ampAttack = 0.45f; pA.ampDecay = 1.0f; pA.ampSustain = 0.80f; pA.ampRelease = 1.3f;
                setSlotA(0, ModSource::VoiceAlternator, ModDestination::Pan, 0.35f);
                setSlotA(1, ModSource::ModWheel, ModDestination::Filter1Cutoff, 0.45f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("reverbMix", 0.28f);
                break;

            case 28: // Arctic Ice Crystals
                pA.dco1Morph = 0.75f; pA.dco1PWM = 0.25f; pA.dco2Morph = 0.00f; pA.dco2Level = 0.50f; pA.dco2Semi = 19;
                pA.f2Mode = 1; pA.f2Cutoff = 3500.0f; pA.f2Res = 0.60f; pA.filterBalance = 0.75f;
                pA.ampAttack = 0.35f; pA.ampDecay = 1.2f; pA.ampSustain = 0.60f; pA.ampRelease = 1.4f;
                setSlotA(0, ModSource::LFO2, ModDestination::Filter2Cutoff, 0.35f);
                setSlotA(1, ModSource::NoiseSampleHold, ModDestination::Pan, 0.30f);
                pB = pA;
                setParam("delayMix", 0.30f); setParam("reverbMix", 0.35f);
                break;

            case 29: { // Bi-Timbral Titan Symphony (Track B DCO1 set to 4' for clear symphonic counterpoint)
                // Part A: Upper strings & horn ensemble
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.65f; pA.dco1SawDetune = 0.35f;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.55f; pA.dco2Semi = 12; pA.dco2Cents = -7.0f;
                pA.lpfCutoff = 2800.0f; pA.lpfRes = 0.18f; pA.envMod = 0.30f;
                pA.ampAttack = 0.40f; pA.ampDecay = 1.5f; pA.ampSustain = 0.80f; pA.ampRelease = 1.6f;
                pA.playMode = 0;

                // Part B: Orchestral brass & cello (DCO1 set to 4' octave)
                pB.dco1Morph = 0.50f; pB.dco1Octave = 3; // 4' Octave for audible musical definition
                pB.subLevel = 0.45f; pB.subOctave = 0;
                pB.dco2Morph = 0.50f; pB.dco2Level = 0.55f; pB.dco2Semi = 0;
                pB.lpfCutoff = 1600.0f; pB.lpfRes = 0.25f; pB.envMod = 0.40f;
                pB.ampAttack = 0.08f; pB.ampDecay = 0.80f; pB.ampSustain = 0.70f; pB.ampRelease = 1.1f;
                pB.playMode = 0;

                setParam("playMode", 0.0f);
                setSlotA(0, ModSource::ModWheel, ModDestination::Filter1Cutoff, 0.45f);
                setSlotB(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.40f);
                setParam("fxRouting", 1.0f); setParam("chorusMode", 2.0f); setParam("reverbMix", 0.40f);
                setParam("internalBpm", 108.0f);

                auto& trkA = sequencer.getTrack(TrackID::TrackA);
                trkA.setLength(16);
                const uint8_t nA[16] = { 62, 65, 69, 65,  58, 62, 65, 62,  60, 64, 67, 64,  57, 60, 64, 60 };
                for (int s = 0; s < 16; ++s) {
                    auto& st = trkA.getStep(s);
                    st.active = true; st.note = nA[s]; st.velocity = 0.78f;
                    st.locks.setLock(Lock_LPFCutoff, (s % 4 == 0) ? 3200.0f : 2400.0f);
                }

                trkB.setMute(false);
                trkB.setLength(16);
                const uint8_t nB[16] = { 50, 50, 62, 50,  46, 46, 58, 46,  48, 48, 60, 48,  45, 45, 57, 45 };
                for (int s = 0; s < 16; ++s) {
                    auto& st = trkB.getStep(s);
                    st.active = true; st.note = nB[s]; st.velocity = 0.85f;
                    st.locks.setLock(Lock_LPFCutoff, (s % 4 == 0) ? 1800.0f : 1300.0f);
                }
                break;
            }

            // =================================================================
            // 04 KEYS & PLUCKS (30 - 38)
            // =================================================================
            case 30: // Chillwave Lo-Fi Keys
                pA.dco1Morph = 0.75f; pA.dco1PWM = 0.45f; pA.lfo1Rate = 4.2f; pA.lfo1ToPitch = 0.030f;
                pA.lpfCutoff = 1800.0f; pA.lpfRes = 0.05f;
                pA.ampAttack = 0.04f; pA.ampDecay = 0.80f; pA.ampSustain = 0.55f; pA.ampRelease = 0.45f;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.45f);
                setSlotA(1, ModSource::KeyTracking, ModDestination::Pan, 0.35f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("reverbMix", 0.32f); setParam("reverbDamp", 0.60f);
                break;

            case 31: // DX FM Crystal E-Piano
                pA.dco1Morph = 0.00f; pA.dco2Morph = 0.00f; pA.dco2Level = 0.70f; pA.dco2Semi = 12;
                pA.xmodAmount = 0.45f; pA.xmodMode = 0;
                pA.lpfCutoff = 6500.0f; pA.lpfRes = 0.12f;
                pA.ampAttack = 0.005f; pA.ampDecay = 0.95f; pA.ampSustain = 0.30f; pA.ampRelease = 0.60f;
                setSlotA(0, ModSource::Velocity, ModDestination::FMAmount, 0.55f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.40f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("reverbMix", 0.22f);
                break;

            case 32: // Karplus Modular Pluck
                pA.dco1Morph = 0.00f; pA.dco1Fold = 0.55f; pA.noiseLevel = 0.20f;
                pA.f2Mode = 0; pA.f2Cutoff = 4500.0f; pA.f2Res = 0.70f; pA.filterBalance = 0.85f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.22f; pA.ampSustain = 0.0f; pA.ampRelease = 0.15f;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter2Cutoff, 0.55f);
                setSlotA(1, ModSource::Velocity, ModDestination::OscWavefold, 0.30f);
                pB = pA;
                setParam("delayMix", 0.25f); setParam("reverbMix", 0.20f);
                break;

            case 33: // 90s Dreamhouse Pizzicato (Robert Miles "Children" / Faithless nylon pluck)
                pA.dco1Morph = 0.25f; pA.dco1Fold = 0.25f;
                pA.dco2Morph = 0.75f; pA.dco2PWM = 0.35f; pA.dco2Level = 0.65f; pA.dco2Semi = 12;
                pA.lpfCutoff = 3600.0f; pA.lpfRes = 0.25f; pA.envMod = 0.60f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.26f; pA.ampSustain = 0.0f; pA.ampRelease = 0.15f;
                pA.filtAttack = 0.001f; pA.filtDecay = 0.20f; pA.filtSustain = 0.0f;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.60f);
                setSlotA(1, ModSource::KeyTracking, ModDestination::Filter1Cutoff, 0.40f);
                pB = pA;
                setParam("delayMix", 0.35f); setParam("delayFeedback", 0.45f); setParam("delaySync", 5.0f);
                setParam("reverbMix", 0.32f);
                break;

            case 34: // Glass Chimes Matrix
                pA.dco1Morph = 0.00f; pA.dco1Fold = 0.30f; pA.dco1Octave = 2;
                pA.dco2Morph = 0.00f; pA.dco2Level = 0.60f; pA.dco2Semi = 19;
                pA.xmodAmount = 0.25f; pA.xmodMode = 0;
                pA.hpfCutoff = 350.0f;
                pA.f2Mode = 1; pA.f2Cutoff = 5800.0f; pA.f2Res = 0.60f; pA.filterBalance = 0.75f;
                pA.ampAttack = 0.002f; pA.ampDecay = 1.3f; pA.ampSustain = 0.0f; pA.ampRelease = 0.85f;
                setSlotA(0, ModSource::Velocity, ModDestination::OscWavefold, 0.45f);
                setSlotA(1, ModSource::NoiseSampleHold, ModDestination::Pan, 0.45f);
                setSlotA(2, ModSource::LFO2, ModDestination::DCO2Pitch, 0.15f);
                setSlotA(3, ModSource::ModWheel, ModDestination::Filter2Cutoff, 0.40f);
                pB = pA;
                setParam("delayMix", 0.32f); setParam("delayFeedback", 0.50f); setParam("reverbMix", 0.40f);
                break;

            case 35: // Neo-Classical Kalimba
                pA.dco1Morph = 0.00f; pA.dco1Fold = 0.35f; pA.dco2Morph = 0.25f; pA.dco2Level = 0.45f; pA.dco2Semi = 19;
                pA.auxAttack = 0.001f; pA.auxDecay = 0.025f; pA.auxToPitch = 12.0f;
                pA.lpfCutoff = 3400.0f; pA.lpfRes = 0.22f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.65f; pA.ampSustain = 0.0f; pA.ampRelease = 0.35f;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.55f);
                setSlotA(1, ModSource::KeyTracking, ModDestination::Pan, 0.40f);
                pB = pA;
                setParam("delayMix", 0.28f); setParam("delayFeedback", 0.45f); setParam("delaySync", 5.0f); setParam("delayPingPong", 1.0f);
                setParam("reverbMix", 0.35f); setParam("reverbSize", 0.78f); setParam("reverbDamp", 0.25f);
                break;

            case 36: // 90s Eurodance Piano (Bright, punchy, compressed dance house piano/keys)
                pA.dco1Morph = 0.25f; pA.dco1Fold = 0.30f;
                pA.dco2Morph = 0.75f; pA.dco2PWM = 0.35f; pA.dco2Level = 0.60f; pA.dco2Semi = 12;
                pA.lpfCutoff = 4500.0f; pA.lpfRes = 0.15f; pA.envMod = 0.45f;
                pA.ampAttack = 0.002f; pA.ampDecay = 0.85f; pA.ampSustain = 0.35f; pA.ampRelease = 0.30f;
                pA.filtAttack = 0.001f; pA.filtDecay = 0.60f; pA.filtSustain = 0.15f;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.65f);
                setSlotA(1, ModSource::Velocity, ModDestination::VCAVolume, 0.35f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("reverbMix", 0.25f);
                break;

            case 37: // Dub Techno Chord Stab
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.70f; pA.dco1SawDetune = 0.55f;
                pA.dco2Morph = 0.65f; pA.dco2Level = 0.70f; pA.dco2Cents = -8.0f;
                pA.subLevel = 0.35f; pA.subOctave = 0;
                pA.noiseLevel = 0.10f;
                pA.filterRouting = true; pA.filterBalance = 0.70f;
                pA.lpfCutoff = 1100.0f; pA.lpfRes = 0.65f; pA.envMod = 0.75f;
                pA.f2Mode = 1; pA.f2Cutoff = 950.0f; pA.f2Res = 0.55f;
                pA.ampAttack = 0.002f; pA.ampDecay = 0.28f; pA.ampSustain = 0.0f; pA.ampRelease = 0.18f;
                pA.filtAttack = 0.002f; pA.filtDecay = 0.22f; pA.filtSustain = 0.0f;
                pA.chordEnable = true; pA.chordType = 1;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.60f);
                setSlotA(1, ModSource::ModWheel, ModDestination::Filter1Cutoff, 0.65f);
                setSlotA(2, ModSource::ModWheel, ModDestination::Filter2Cutoff, 0.40f);
                pB = pA;
                setParam("delayMix", 0.45f); setParam("delayFeedback", 0.70f); setParam("delayDamp", 0.40f);
                setParam("delaySync", 5.0f);
                setParam("reverbMix", 0.40f); setParam("reverbSize", 0.85f);
                break;

            case 38: // Neo-Soul Analog Keys
                pA.dco1Morph = 0.75f; pA.dco1PWM = 0.45f; pA.dco1Octave = 2;
                pA.dco2Morph = 0.20f; pA.dco2Level = 0.45f; pA.dco2Semi = 7; pA.dco2Cents = -6.0f;
                pA.lpfCutoff = 2200.0f; pA.lpfRes = 0.15f; pA.envMod = 0.35f;
                pA.ampAttack = 0.012f; pA.ampDecay = 1.1f; pA.ampSustain = 0.40f; pA.ampRelease = 0.40f;
                pA.lfo1Rate = 4.5f; pA.lfo1ToPitch = 0.015f;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.55f);
                setSlotA(1, ModSource::Velocity, ModDestination::VCAVolume, 0.25f);
                setSlotA(2, ModSource::KeyTracking, ModDestination::Pan, 0.35f);
                pB = pA;
                setParam("chorusMode", 1.0f); setParam("reverbMix", 0.25f); setParam("reverbSize", 0.70f);
                break;

            // =================================================================
            // 05 PERCUSSION & DRUMS (39 - 47)
            // =================================================================
            case 39: // Analog 808 Kick
                pA.dco1Morph = 0.00f; pA.dco1Octave = 0; pA.dco1Fold = 0.12f;
                pA.auxAttack = 0.001f; pA.auxDecay = 0.038f; pA.auxSustain = 0.0f; pA.auxToPitch = 24.0f;
                pA.lpfCutoff = 420.0f; pA.lpfRes = 0.0f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.55f; pA.ampSustain = 0.0f; pA.ampRelease = 0.12f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.40f);
                pB = pA;
                break;

            case 40: // 909 Dance Kick (True TR-909 bridged-T 55 Hz punch and pitch drop)
                pA.dco1Morph = 0.05f; pA.dco1Octave = 1; // 16' octave places fundamental at punchy 55 Hz
                pA.dco1Fold = 0.10f;
                pA.dco2Morph = 0.75f; pA.dco2Level = 0.30f; pA.dco2Semi = 12; // Beater attack transient
                pA.auxAttack = 0.001f; pA.auxDecay = 0.035f; pA.auxSustain = 0.0f; pA.auxToPitch = 24.0f; // 220 Hz -> 55 Hz drop
                pA.lpfCutoff = 4500.0f; pA.lpfRes = 0.08f; pA.envMod = 0.30f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.42f; pA.ampSustain = 0.0f; pA.ampRelease = 0.10f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::VCAVolume, 0.35f);
                pB = pA;
                break;

            case 41: // Electro Analog Snare
                pA.dco1Morph = 0.00f; pA.dco1Octave = 1;
                pA.dco2Morph = 0.00f; pA.dco2Level = 0.45f; pA.dco2Semi = 5;
                pA.auxToPitch = 0.0f;
                pA.noiseLevel = 0.85f;
                pA.filterRouting = true; pA.filterBalance = 0.70f;
                pA.f2Mode = 2; pA.f2Cutoff = 2800.0f; pA.f2Res = 0.25f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.19f; pA.ampSustain = 0.0f; pA.ampRelease = 0.12f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::NoiseLevel, 0.40f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter2Cutoff, 0.45f);
                pB = pA;
                setParam("reverbMix", 0.18f); setParam("reverbSize", 0.30f);
                break;

            case 42: // Crisp Metallic Hi-Hat (Unchoked open LPF, razor-sharp 8.5 kHz highpass metallic sizzle)
                pA.dco1Morph = 0.75f; pA.dco1Octave = 4; pA.dco1PWM = 0.35f;
                pA.dco2Morph = 0.75f; pA.dco2Level = 0.70f; pA.dco2Semi = 17;
                pA.noiseLevel = 0.90f;
                pA.hpfCutoff = 6500.0f;
                pA.lpfCutoff = 20000.0f; // Open LPF eliminates high-end choking
                pA.f2Mode = 2; pA.f2Cutoff = 8500.0f; pA.f2Res = 0.35f; pA.filterBalance = 0.85f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.075f; pA.ampSustain = 0.0f; pA.ampRelease = 0.04f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::AmpEnvDecay, 0.35f);
                setSlotA(1, ModSource::Velocity, ModDestination::VCAVolume, 0.30f);
                pB = pA;
                break;

            case 43: // Modular Tribal Tom
                pA.dco1Morph = 0.00f; pA.dco1Octave = 1; pA.dco1Fold = 0.40f;
                pA.auxAttack = 0.001f; pA.auxDecay = 0.065f; pA.auxSustain = 0.0f; pA.auxToPitch = 14.0f;
                pA.lpfCutoff = 850.0f; pA.lpfRes = 0.50f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.35f; pA.ampSustain = 0.0f; pA.ampRelease = 0.12f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.50f);
                pB = pA;
                setParam("reverbMix", 0.18f);
                break;

            case 44: // 909 Dance Clap (Essential 90s house/techno handclap with 1.45 kHz bandpass body)
                pA.dco1Morph = 0.75f; pA.dco1Octave = 2; pA.dco1Fold = 0.30f;
                pA.noiseLevel = 1.0f;
                pA.lpfCutoff = 8500.0f; pA.lpfRes = 0.15f;
                pA.f2Mode = 1; pA.f2Cutoff = 1450.0f; pA.f2Res = 0.65f; pA.filterBalance = 0.90f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.22f; pA.ampSustain = 0.0f; pA.ampRelease = 0.12f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::VCAVolume, 0.35f);
                pB = pA;
                setParam("reverbMix", 0.25f); setParam("reverbSize", 0.40f); setParam("reverbDamp", 0.45f);
                break;

            case 45: // Industrial Metallic Perc
                pA.dco1Morph = 0.50f; pA.dco1Fold = 0.55f;
                pA.dco2Morph = 0.50f; pA.dco2Fold = 0.65f;
                pA.dco2Level = 0.90f; pA.dco2Semi = 19; pA.dco2Cents = 38.0f;
                pA.xmodAmount = 0.75f; pA.xmodMode = 0;
                pA.lpfCutoff = 4800.0f; pA.lpfRes = 0.45f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.16f; pA.ampSustain = 0.0f; pA.ampRelease = 0.08f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::FMAmount, 0.60f);
                setSlotA(1, ModSource::NoiseSampleHold, ModDestination::Pan, 0.35f);
                pB = pA;
                setParam("delayMix", 0.25f);
                break;

            case 46: // West Coast Buchla Bongo
                pA.dco1Morph = 0.00f; pA.dco1Fold = 0.70f;
                pA.auxAttack = 0.001f; pA.auxDecay = 0.045f; pA.auxSustain = 0.0f;
                pA.auxToPitch = 7.0f; pA.auxToFold = 0.45f;
                pA.lpfCutoff = 9000.0f; pA.filterBalance = 0.65f;
                pA.f2Cutoff = 3500.0f; pA.f2Res = 0.45f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.24f; pA.ampSustain = 0.0f; pA.ampRelease = 0.15f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::OscWavefold, 0.50f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter2Cutoff, 0.40f);
                pB = pA;
                setParam("reverbMix", 0.25f); setParam("reverbSize", 0.45f);
                break;

            case 47: // Noise Shaker Burst
                pA.dco2Level = 0.0f; pA.noiseLevel = 0.95f;
                pA.f2Mode = 1; pA.f2Cutoff = 4200.0f; pA.f2Res = 0.50f; pA.filterBalance = 0.95f;
                pA.ampAttack = 0.002f; pA.ampDecay = 0.08f; pA.ampSustain = 0.0f; pA.ampRelease = 0.04f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::Filter2Cutoff, 0.45f);
                setSlotA(1, ModSource::NoiseSampleHold, ModDestination::Pan, 0.40f);
                pB = pA;
                break;

            // =================================================================
            // 06 SEQUENCES & ARPS (48 - 55)
            // =================================================================
            case 48: { // Acid 303 Bass Sequence
                pA.dco1Morph = 0.50f; pA.dco2Morph = 0.75f; pA.dco2Level = 0.60f; pA.dco2Semi = -12;
                pA.subLevel = 0.30f; pA.lpfCutoff = 650.0f; pA.lpfRes = 0.85f; pA.envMod = 0.78f;
                pA.ampAttack = 0.002f; pA.ampDecay = 0.28f; pA.ampSustain = 0.05f; pA.ampRelease = 0.08f;
                pA.filtAttack = 0.003f; pA.filtDecay = 0.25f; pA.filtSustain = 0.10f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                pA.glideTime = 0.065f; pA.glideEnable = true;
                setSlotA(0, ModSource::SeqTrack, ModDestination::Filter1Cutoff, 0.35f);
                setSlotA(1, ModSource::ModWheel, ModDestination::Filter1Resonance, 0.30f);
                pB = pA;
                setParam("internalBpm", 130.0f); setParam("delayMix", 0.28f); setParam("delayFeedback", 0.40f);

                auto& trk = sequencer.getTrack(TrackID::TrackA);
                trk.setLength(16);
                const uint8_t n[16] = { 38, 38, 50, 38, 41, 38, 43, 44, 38, 48, 38, 41, 38, 38, 36, 37 };
                const float cut[16] = { 1800.f, 650.f, 2400.f, 580.f, 1600.f, 650.f, 2100.f, 3200.f,
                                        650.f, 2200.f, 650.f, 1400.f, 2800.f, 600.f, 1100.f, 1900.f };
                const float res[16] = { 0.85f, 0.70f, 0.92f, 0.65f, 0.80f, 0.75f, 0.90f, 0.95f,
                                        0.70f, 0.88f, 0.75f, 0.80f, 0.92f, 0.70f, 0.85f, 0.90f };
                const float dec[16] = { 0.28f, 0.15f, 0.40f, 0.12f, 0.25f, 0.18f, 0.35f, 0.45f,
                                        0.15f, 0.32f, 0.15f, 0.25f, 0.42f, 0.14f, 0.20f, 0.30f };

                for (int s = 0; s < 16; ++s) {
                    auto& st = trk.getStep(s);
                    st.active = true; st.note = n[s]; st.velocity = (s % 4 == 0) ? 0.95f : 0.70f;
                    st.locks.setLock(Lock_LPFCutoff, cut[s]);
                    st.locks.setLock(Lock_LPFRes, res[s]);
                    st.locks.setLock(Lock_FiltDecay, dec[s]);
                }
                break;
            }

            case 49: { // 90s Eurodance Rave Bass (Rolling 16th 2 Unlimited / Culture Beat bassline)
                pA.dco1Morph = 0.50f; pA.dco1Octave = 1;
                pA.subLevel = 0.55f; pA.subOctave = 0;
                pA.lpfCutoff = 1350.0f; pA.lpfRes = 0.35f; pA.envMod = 0.65f;
                pA.ampAttack = 0.002f; pA.ampDecay = 0.20f; pA.ampSustain = 0.05f; pA.ampRelease = 0.08f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                pB = pA;
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.50f);
                setParam("internalBpm", 138.0f); setParam("chorusMode", 1.0f);

                auto& trk = sequencer.getTrack(TrackID::TrackA);
                trk.setLength(16);
                const uint8_t n90s[16] = { 36, 48, 36, 48,  36, 48, 39, 41,  36, 48, 36, 48,  36, 46, 48, 39 };
                for (int s = 0; s < 16; ++s) {
                    auto& st = trk.getStep(s);
                    st.active = true; st.note = n90s[s];
                    st.velocity = (s % 2 == 1) ? 0.95f : 0.72f;
                    st.locks.setLock(Lock_LPFCutoff, (s % 2 == 1) ? 2400.0f : 1200.0f);
                    st.locks.setLock(Lock_AmpDecay, (s % 4 == 0) ? 0.24f : 0.16f);
                }
                break;
            }

            case 50: { // 8-Bit Chiptune Arp
                pA.dco1Morph = 0.75f; pA.dco1PWM = 0.25f;
                pA.dco2Level = 0.0f; pA.lpfCutoff = 12000.0f; pA.lpfRes = 0.0f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.12f; pA.ampSustain = 0.0f; pA.ampRelease = 0.02f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                pB = pA;
                setParam("internalBpm", 136.0f);

                auto& trk = sequencer.getTrack(TrackID::TrackA);
                trk.setLength(16);
                const uint8_t chipNotes[16] = { 60, 64, 67, 72,  57, 60, 64, 69,  53, 57, 60, 65,  55, 59, 62, 67 };
                for (int s = 0; s < 16; ++s) {
                    auto& st = trk.getStep(s);
                    st.active = true; st.note = chipNotes[s]; st.velocity = 0.90f;
                    st.locks.setLock(Lock_AmpDecay, (s % 4 == 3) ? 0.18f : 0.09f);
                }
                break;
            }

            case 51: { // Shimmering Crystal Arp
                pA.dco1Morph = 0.25f; pA.dco2Morph = 0.00f; pA.dco2Level = 0.55f; pA.dco2Semi = 12;
                pA.f2Mode = 1; pA.f2Cutoff = 2800.0f; pA.f2Res = 0.50f; pA.filterBalance = 0.75f;
                pA.ampAttack = 0.005f; pA.ampDecay = 0.35f; pA.ampSustain = 0.15f; pA.ampRelease = 0.40f;
                setSlotA(0, ModSource::LFO2, ModDestination::Filter2Cutoff, 0.35f);
                pB = pA;
                setParam("internalBpm", 120.0f);
                setParam("delayMix", 0.35f); setParam("delaySync", 5.0f); setParam("reverbMix", 0.40f);

                auto& trk = sequencer.getTrack(TrackID::TrackA);
                trk.setLength(16);
                const uint8_t arpNotes[16] = { 62, 65, 69, 72, 76, 72, 69, 65,  60, 65, 69, 72, 77, 72, 69, 65 };
                for (int s = 0; s < 16; ++s) {
                    auto& st = trk.getStep(s);
                    st.active = true; st.note = arpNotes[s]; st.velocity = 0.85f;
                    st.locks.setLock(Lock_LPFCutoff, static_cast<float>(2000 + (s * 140)));
                }
                break;
            }

            case 52: { // Modular Glitch Sequence
                pA.dco1Morph = 0.00f; pA.dco1Fold = 0.50f; pA.noiseLevel = 0.20f;
                pA.lpfCutoff = 3200.0f; pA.lpfRes = 0.55f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.12f; pA.ampSustain = 0.0f; pA.ampRelease = 0.05f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::NoiseSampleHold, ModDestination::Filter1Cutoff, 0.40f);
                setSlotA(1, ModSource::NoiseSampleHold, ModDestination::Pan, 0.50f);
                pB = pA;
                setParam("internalBpm", 136.0f); setParam("delayMix", 0.35f); setParam("delayFeedback", 0.55f);

                auto& trk = sequencer.getTrack(TrackID::TrackA);
                trk.setLength(16);
                const float foldLocks[16] = { 0.85f, 0.10f, 0.95f, 0.30f, 0.60f, 0.05f, 0.75f, 0.90f,
                                              0.20f, 0.80f, 0.40f, 0.95f, 0.15f, 0.70f, 0.50f, 0.85f };
                const float cutLocks[16]  = { 4500.f, 1200.f, 6000.f, 2100.f, 3800.f, 900.f, 5000.f, 7500.f,
                                              1500.f, 4200.f, 2800.f, 6800.f, 1800.f, 5200.f, 3100.f, 6200.f };

                for (int s = 0; s < 16; ++s) {
                    auto& st = trk.getStep(s);
                    st.active = (s % 2 == 0) || (s == 7) || (s == 11) || (s == 15);
                    st.note = static_cast<uint8_t>(48 + (s * 5) % 24);
                    st.velocity = (s % 4 == 0) ? 0.95f : 0.72f;
                    st.locks.setLock(Lock_DCO1Fold, foldLocks[s]);
                    st.locks.setLock(Lock_LPFCutoff, cutLocks[s]);
                }
                break;
            }

            case 53: { // Techno Industrial Pulser (DCO1 Octave set to 16' for audible, driving low-end impact)
                pA.dco1Morph = 0.50f; pA.dco1Octave = 1; // 16' octave for clear punchy bass
                pA.dco1Fold = 0.45f;
                pA.dco2Morph = 0.75f; pA.dco2Level = 0.75f; pA.dco2Semi = 7;
                pA.syncMode = true; pA.xmodAmount = 0.40f; pA.xmodMode = 1;
                pA.subLevel = 0.45f; pA.subOctave = 0;
                pA.lpfCutoff = 2200.0f; pA.lpfRes = 0.55f; pA.envMod = 0.65f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.22f; pA.ampSustain = 0.0f; pA.ampRelease = 0.08f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                pB = pA;
                setParam("internalBpm", 134.0f); setParam("delayMix", 0.30f); setParam("reverbMix", 0.22f);

                auto& trk = sequencer.getTrack(TrackID::TrackA);
                trk.setLength(16);
                for (int s = 0; s < 16; ++s) {
                    auto& st = trk.getStep(s);
                    st.active = true;
                    st.note = (s % 8 == 6) ? 38 : 36;
                    st.velocity = (s % 4 == 0) ? 0.98f : ((s % 2 == 0) ? 0.82f : 0.68f);
                    st.locks.setLock(Lock_LPFCutoff, (s % 4 == 0) ? 3400.0f : ((s % 2 == 0) ? 2200.0f : 1400.0f));
                    st.locks.setLock(Lock_LPFRes, (s % 4 == 0) ? 0.70f : 0.50f);
                    st.locks.setLock(Lock_AmpDecay, (s % 4 == 0) ? 0.26f : 0.16f);
                }
                break;
            }

            case 54: { // Berlin School Space Arp
                pA.dco1Morph = 0.50f; pA.dco2Morph = 0.50f; pA.dco2Level = 0.60f; pA.dco2Cents = 6.0f;
                pA.lpfCutoff = 1800.0f; pA.lpfRes = 0.40f; pA.envMod = 0.50f;
                pA.ampAttack = 0.01f; pA.ampDecay = 0.25f; pA.ampSustain = 0.15f; pA.ampRelease = 0.20f;
                setSlotA(0, ModSource::LFO1, ModDestination::Filter1Cutoff, 0.30f);
                setSlotA(1, ModSource::ModWheel, ModDestination::Filter1Cutoff, 0.45f);
                pB = pA;
                setParam("delayMix", 0.40f); setParam("delaySync", 4.0f);
                setParam("delayFeedback", 0.55f); setParam("reverbMix", 0.35f);

                auto& trk = sequencer.getTrack(TrackID::TrackA);
                trk.setLength(16);
                const uint8_t n[16] = { 48, 55, 60, 63, 55, 60, 63, 67, 48, 55, 60, 63, 55, 60, 65, 67 };
                for (int s = 0; s < 16; ++s) {
                    auto& st = trk.getStep(s);
                    st.active = true; st.note = n[s]; st.velocity = 0.80f;
                    st.locks.setLock(Lock_LPFCutoff, static_cast<float>(1200 + (s * 90)));
                    st.locks.setLock(Lock_FiltDecay, (s % 2 == 0) ? 0.30f : 0.18f);
                }
                break;
            }

            case 55: { // Cyberpunk Bi-Timbral Split (Track B DCO1 set to 8' for driving neuro slap groove)
                // Part A: Searing supersaw lead hook
                pA.dco1Morph = 0.50f; pA.dco1SawMix = 0.75f; pA.dco1SawDetune = 0.60f; pA.dco1Fold = 0.35f;
                pA.dco2Morph = 0.50f; pA.dco2Level = 0.65f; pA.dco2Semi = 12;
                pA.lpfCutoff = 3200.0f; pA.lpfRes = 0.40f; pA.envMod = 0.45f;
                pA.ampAttack = 0.003f; pA.ampDecay = 0.28f; pA.ampSustain = 0.35f; pA.ampRelease = 0.18f;
                pA.playMode = 0;

                // Part B: Driving neuro slap bass (DCO1 set to 8' octave)
                pB.dco1Morph = 0.50f; pB.dco1Octave = 2; // 8' Octave brings bass out of the sub mud
                pB.subLevel = 0.50f; pB.subOctave = 0;
                pB.dco2Morph = 0.75f; pB.dco2Level = 0.75f; pB.dco2Cents = 12.0f; pB.dco2Fold = 0.30f;
                pB.lpfCutoff = 1200.0f; pB.lpfRes = 0.45f; pB.envMod = 0.60f;
                pB.ampAttack = 0.002f; pB.ampDecay = 0.24f; pB.ampSustain = 0.15f; pB.ampRelease = 0.12f;
                pB.playMode = 0;

                setParam("playMode", 0.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.45f);
                setSlotB(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.40f);
                setParam("fxRouting", 1.0f); setParam("chorusMode", 1.0f);
                setParam("delayMix", 0.30f); setParam("delayFeedback", 0.45f); setParam("reverbMix", 0.25f);
                setParam("internalBpm", 128.0f);

                auto& trkA = sequencer.getTrack(TrackID::TrackA);
                trkA.setLength(16);
                const uint8_t hookA[16] = { 64, 64, 76, 64,  67, 64, 71, 70,  64, 64, 76, 64,  74, 72, 71, 67 };
                for (int s = 0; s < 16; ++s) {
                    auto& st = trkA.getStep(s);
                    st.active = (s % 2 == 0) || (s == 3) || (s == 7) || (s == 11) || (s == 15);
                    st.note = hookA[s]; st.velocity = (s % 4 == 0) ? 0.95f : 0.78f;
                    st.locks.setLock(Lock_LPFCutoff, (s % 4 == 0) ? 4500.0f : 2400.0f);
                }

                trkB.setMute(false);
                trkB.setLength(16);
                const uint8_t bassB[16] = { 40, 40, 40, 40,  40, 40, 52, 40,  40, 40, 40, 40,  43, 40, 45, 46 };
                for (int s = 0; s < 16; ++s) {
                    auto& st = trkB.getStep(s);
                    st.active = true; st.note = bassB[s]; st.velocity = (s % 4 == 0) ? 0.95f : 0.75f;
                    st.locks.setLock(Lock_LPFCutoff, (s % 4 == 0) ? 1800.0f : 1200.0f);
                }
                break;
            }

            // =================================================================
            // 07 FX, TEXTURES & DRONES (56 - 63)
            // =================================================================
            case 56: // Cinematic Sub Drone
                pA.dco1Morph = 0.50f; pA.dco1Octave = 0; pA.dco2Morph = 0.25f; pA.dco2Level = 0.70f; pA.dco2Cents = 5.0f;
                pA.subLevel = 0.50f; pA.lpfCutoff = 650.0f; pA.lpfRes = 0.20f;
                pA.lfo1Rate = 0.25f; pA.lfo1ToFilter = 0.25f;
                pA.ampAttack = 1.2f; pA.ampDecay = 2.0f; pA.ampSustain = 0.90f; pA.ampRelease = 2.2f;
                setSlotA(0, ModSource::LFO1, ModDestination::Filter1Cutoff, 0.35f);
                setSlotA(1, ModSource::VoiceAlternator, ModDestination::Pan, 0.40f);
                pB = pA;
                setParam("reverbMix", 0.45f); setParam("reverbSize", 0.90f);
                break;

            case 57: // Laser Beam Pitch Drop
                pA.dco1Morph = 0.25f; pA.dco1Octave = 2; pA.dco1Fold = 0.0f;
                pA.auxAttack = 0.001f; pA.auxDecay = 0.12f; pA.auxSustain = 0.0f; pA.auxToPitch = 48.0f;
                pA.lpfCutoff = 4200.0f; pA.lpfRes = 0.75f; pA.envMod = 0.65f;
                pA.filtAttack = 0.001f; pA.filtDecay = 0.11f; pA.filtSustain = 0.0f;
                pA.ampAttack = 0.001f; pA.ampDecay = 0.22f; pA.ampSustain = 0.0f; pA.ampRelease = 0.08f;
                pA.playMode = 3;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.50f);
                pB = pA;
                setParam("delayMix", 0.35f); setParam("delayFeedback", 0.55f); setParam("delaySync", 3.0f);
                break;

            case 58: // UFO Sci-Fi Theremin
                pA.dco1Morph = 0.00f; pA.dco1Octave = 3;
                pA.lfo1Rate = 6.0f; pA.lfo1ToPitch = 0.06f;
                pA.lpfCutoff = 2500.0f; pA.lpfRes = 0.30f;
                pA.ampAttack = 0.15f; pA.ampDecay = 0.60f; pA.ampSustain = 0.90f; pA.ampRelease = 0.60f;
                pA.playMode = 3; pA.glideTime = 0.15f; pA.glideEnable = true;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::ModWheel, ModDestination::LFO1Rate, 0.50f);
                setSlotA(1, ModSource::Aftertouch, ModDestination::Filter1Cutoff, 0.40f);
                pB = pA;
                setParam("delayMix", 0.35f); setParam("reverbMix", 0.40f);
                break;

            case 59: // Alien Transmission S&H
                pA.dco1Morph = 0.50f; pA.dco2Morph = 0.75f; pA.dco2Level = 0.80f; pA.dco2Semi = 19;
                pA.xmodAmount = 0.65f; pA.xmodMode = 0;
                pA.noiseLevel = 0.20f;
                pA.lpfCutoff = 2200.0f; pA.lpfRes = 0.85f;
                pA.ampAttack = 0.01f; pA.ampDecay = 0.60f; pA.ampSustain = 0.80f; pA.ampRelease = 0.50f;
                setSlotA(0, ModSource::NoiseSampleHold, ModDestination::DCO2Pitch, 0.45f);
                setSlotA(1, ModSource::NoiseSampleHold, ModDestination::Filter1Cutoff, 0.65f);
                setSlotA(2, ModSource::NoiseSampleHold, ModDestination::Pan, 0.70f);
                setSlotA(3, ModSource::LFO1, ModDestination::FMAmount, 0.40f);
                pB = pA;
                setParam("delayMix", 0.38f); setParam("delayFeedback", 0.60f); setParam("reverbMix", 0.30f);
                break;

            case 60: // Dark Abyss Sub Shaker
                pA.dco1Morph = 0.00f; pA.dco1Octave = 0; pA.noiseLevel = 0.15f;
                pA.lpfCutoff = 320.0f; pA.lpfRes = 0.10f;
                pA.lfo1Rate = 0.15f; pA.lfo1ToFilter = 0.20f;
                pA.ampAttack = 0.8f; pA.ampDecay = 1.2f; pA.ampSustain = 0.85f; pA.ampRelease = 1.5f;
                setSlotA(0, ModSource::LFO1, ModDestination::Filter1Cutoff, 0.35f);
                setSlotA(1, ModSource::ModWheel, ModDestination::NoiseLevel, 0.40f);
                pB = pA;
                setParam("reverbMix", 0.35f); setParam("reverbSize", 0.85f);
                break;

            case 61: // Metallic FM Industrial Ring
                pA.dco1Morph = 0.00f; pA.dco2Morph = 0.50f; pA.dco2Level = 0.85f; pA.dco2Semi = 19;
                pA.xmodAmount = 0.80f; pA.xmodMode = 0;
                pA.lpfCutoff = 4800.0f; pA.lpfRes = 0.50f;
                pA.ampAttack = 0.01f; pA.ampDecay = 0.60f; pA.ampSustain = 0.70f; pA.ampRelease = 0.45f;
                setSlotA(0, ModSource::ModWheel, ModDestination::FMAmount, 0.40f);
                setSlotA(1, ModSource::Velocity, ModDestination::Filter1Cutoff, 0.45f);
                pB = pA;
                setParam("delayMix", 0.30f); setParam("delayFeedback", 0.60f); setParam("reverbMix", 0.28f);
                break;

            case 62: // Modular Self-Oscillation
                pA.dco1Morph = 0.00f; pA.dco1PWM = 0.50f;
                pA.dco2Level = 0.0f;
                pA.lpfCutoff = 1000.0f; pA.lpfRes = 0.96f;
                pA.f2KeyTrack = 1.0f;
                pA.ampAttack = 0.005f; pA.ampDecay = 0.40f; pA.ampSustain = 0.85f; pA.ampRelease = 0.35f;
                pA.playMode = 3; pA.glideTime = 0.04f; pA.glideEnable = true;
                setParam("playMode", 3.0f);
                setSlotA(0, ModSource::KeyTracking, ModDestination::Filter1Cutoff, 0.75f);
                setSlotA(1, ModSource::Aftertouch, ModDestination::Filter1Resonance, 0.25f);
                pB = pA;
                setParam("delayMix", 0.25f); setParam("reverbMix", 0.30f);
                break;

            case 63: // Cosmic Solar Wind Pad
                pA.dco2Level = 0.0f; pA.noiseLevel = 0.75f;
                pA.lpfCutoff = 1400.0f; pA.lpfRes = 0.80f;
                pA.filterRouting = true; pA.filterBalance = 0.50f;
                pA.f2Mode = 1; pA.f2Cutoff = 2100.0f; pA.f2Res = 0.75f;
                pA.lfo1Rate = 0.06f; pA.lfo1ToFilter = 0.40f;
                pA.ampAttack = 1.6f; pA.ampDecay = 2.2f; pA.ampSustain = 0.85f; pA.ampRelease = 2.6f;
                setSlotA(0, ModSource::LFO1, ModDestination::Filter1Cutoff, 0.45f);
                setSlotA(1, ModSource::LFO2, ModDestination::Filter2Cutoff, -0.40f);
                setSlotA(2, ModSource::VoiceAlternator, ModDestination::Pan, 0.45f);
                pB = pA;
                setParam("reverbMix", 0.55f); setParam("reverbSize", 0.90f); setParam("chorusMode", 2.0f);
                break;

            default:
                pA = TimbrePatch();
                pB = pA;
                break;
        }
    }
};