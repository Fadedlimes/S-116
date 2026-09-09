#include "PluginEditor.h"

namespace
{
    // Realtime-safe audition preview timer tracking (prevents hanging notes)
    int auditionCountdown = 0;
    int auditionPart = 0;
    std::array<int, 4> auditionChordNotes { -1, -1, -1, -1 };

    // =========================================================================
    // Ergonomic Compact Parameter Value Formatters (No Ellipses, Zero Truncation)
    // =========================================================================
    juce::String formatFrequency(double val)
    {
        if (val < 999.5)
            return juce::String(juce::roundToInt(val)) + "Hz";
        if (val < 9995.0)
            return juce::String(val / 1000.0, 2) + "kHz";
        return juce::String(val / 1000.0, 1) + "kHz";
    }

    double parseFrequency(const juce::String& text)
    {
        auto t = text.trim().toLowerCase();
        if (t.endsWith("khz"))
            return t.dropLastCharacters(3).trim().getDoubleValue() * 1000.0;
        if (t.endsWith("k"))
            return t.dropLastCharacters(1).trim().getDoubleValue() * 1000.0;
        if (t.endsWith("hz"))
            return t.dropLastCharacters(2).trim().getDoubleValue();
        return t.getDoubleValue();
    }

    juce::String formatEnvTime(double val)
    {
        if (val < 0.9995)
        {
            const int ms = juce::roundToInt(val * 1000.0);
            return juce::String(ms) + "ms";
        }
        return juce::String(val, 2) + "s";
    }

    double parseEnvTime(const juce::String& text)
    {
        auto t = text.trim().toLowerCase();
        if (t.endsWith("ms"))
            return t.dropLastCharacters(2).trim().getDoubleValue() * 0.001;
        if (t.endsWith("s"))
            return t.dropLastCharacters(1).trim().getDoubleValue();
        return t.getDoubleValue();
    }

    juce::String formatPercent(double val)
    {
        return juce::String(juce::roundToInt(val * 100.0)) + "%";
    }

    double parsePercent(const juce::String& text)
    {
        auto t = text.trim().toLowerCase();
        if (t.endsWith("%"))
            return t.dropLastCharacters(1).trim().getDoubleValue() * 0.01;
        return t.getDoubleValue();
    }

    juce::String formatMidiNoteName(int noteNum)
    {
        if (noteNum < 0 || noteNum > 127) return "--";
        static const char* const noteNames[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        const int noteIndex = noteNum % 12;
        const int octave = (noteNum / 12) - 2; // MIDI 60 = C3
        return juce::String(noteNames[noteIndex]) + juce::String(octave);
    }
}

Simple116AudioProcessorEditor::Simple116AudioProcessorEditor(Simple106AudioProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
      audioProcessor(processor),
      keyboardComponent(processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&studioLookAndFeel);
    configureFocus(*this);

    // ==========================================
    // 1. Header Bar Controls
    // ==========================================
    titleBadgeLabel.setText("S-116", juce::dontSendNotification);
    titleBadgeLabel.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
    titleBadgeLabel.setJustificationType(juce::Justification::centred);
    configureFocus(titleBadgeLabel);
    addAndMakeVisible(titleBadgeLabel);

    setupComboBox(presetComboBox, presetLabel, "PRESET", { "Init Patch" }, 1);
    setupButton(prevPresetButton, "<", false);
    setupButton(nextPresetButton, ">", false);
    setupButton(savePresetButton, "SAVE", false);
    setupButton(initPresetButton, "INIT", false);
    setupButton(cardButton, "CARD", false);

    presetComboBox.onChange = [this]() {
        const auto selectedName = presetComboBox.getText();
        if (selectedName.isNotEmpty()) {
            audioProcessor.loadPresetByName(selectedName);
            updateSequencerLcd();
            refreshSelectedStepUI();
            refreshMatrixUI();
            updateEnvelopeLcd();
            lcdScreen.setPresetInfo(selectedName, "PRESET LOADED");
        }
    };

    prevPresetButton.onClick = [this]() {
        const int cur = presetComboBox.getSelectedItemIndex();
        if (cur > 0)
            presetComboBox.setSelectedItemIndex(cur - 1);
        else if (presetComboBox.getNumItems() > 0)
            presetComboBox.setSelectedItemIndex(presetComboBox.getNumItems() - 1);
    };

    nextPresetButton.onClick = [this]() {
        const int cur = presetComboBox.getSelectedItemIndex();
        if (cur < presetComboBox.getNumItems() - 1)
            presetComboBox.setSelectedItemIndex(cur + 1);
        else if (presetComboBox.getNumItems() > 0)
            presetComboBox.setSelectedItemIndex(0);
    };

    savePresetButton.onClick = [this]() {
        savePresetDialog();
    };

    initPresetButton.onClick = [this]() {
        audioProcessor.loadFactoryPreset(0);
        refreshPresetList();
        updateSequencerLcd();
        refreshSelectedStepUI();
        refreshMatrixUI();
        setActiveEnvelope(activeEnvIndex);
        lcdScreen.setParameterEdit("INITIALIZE", "INIT PATCH RESTORED", 0.0f);
    };

    cardButton.onClick = [this]() {
        showExpansionCardMenu();
    };

    // Full 9-Mode Voice Allocation
    setupComboBox(voiceModeComboBox, voiceModeLabel, "VOICE MODE",
                  { "Poly 8 Bi-Timbral",
                    "Poly 16 [Single]",
                    "Round Robin",
                    "Mono Legato",
                    "Mono Standard",
                    "Unison 2x",
                    "Unison 4x",
                    "Unison 8x",
                    "Unison 16x" }, 2);
    attachComboBox(voiceModeComboBox, "playMode");

    setupRotaryKnob(masterVolumeSlider, masterVolumeLabel, "MASTER VOL", 0.0, 1.0, 0.85);
    attachSlider(masterVolumeSlider, "masterVolume");

    setupButton(chassisThemeButton, "CHASSIS", false);
    chassisThemeButton.onClick = [this]() {
        auto& skin = SkinManager::getInstance();
        skin.cycleChassisTheme();
        if (auto* p = audioProcessor.apvts.getParameter("guiTheme"))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(skin.getChassisTheme())));
        repaint();
    };

    setupButton(ledThemeButton, "LED COLOR", false);
    ledThemeButton.onClick = [this]() {
        auto& skin = SkinManager::getInstance();
        skin.cycleLEDTheme();
        if (auto* p = audioProcessor.apvts.getParameter("ledTheme"))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(skin.getLEDTheme())));
        repaint();
    };

    configureFocus(lcdScreen);
    addAndMakeVisible(lcdScreen);
    lcdScreen.triggerBoot();

    // ==========================================
    // 2. Universal P-Lock Tweak Dispatcher
    // ==========================================
    auto bindLock = [this](juce::Slider& slider, LockParamID id, const juce::String& paramName) {
        slider.onValueChange = [this, &slider, id, paramName]() {
            if (isUpdatingKnobsFromStep)
                return;

            const float val = static_cast<float>(slider.getValue());
            const float norm = static_cast<float>(slider.valueToProportionOfLength(val));

            if (audioProcessor.sequencer.isRecording) {
                auto& track = audioProcessor.sequencer.getActiveTrack();
                const int targetStep = audioProcessor.sequencer.isPlaying
                                     ? track.getCurrentStep()
                                     : selectedStepIndex;
                track.getStep(targetStep).locks.setLock(id, val);
                updateSequencerLcd();

                const int sIdx = (audioProcessor.sequencer.isPlaying ? track.getCurrentStep() : selectedStepIndex);
                lcdScreen.setParameterEdit("LOCK S" + juce::String(sIdx + 1) + " " + paramName, slider.getTextFromValue(val), norm);
            } else {
                lcdScreen.setParameterEdit(paramName, slider.getTextFromValue(val), norm);
            }
        };
    };

    // ==========================================
    // 3. Oscillator Section Controls (Symmetrical 6x2 Grid)
    // ==========================================
    // Row 1: DCO 1 Controls
    setupRotaryKnob(octaveSlider, octaveLabel, "D1 OCT", 0.0, 3.0, 2.0);
    attachSlider(octaveSlider, "dco1Octave");

    setupRotaryKnob(oscMorphSlider, oscMorphLabel, "D1 MORPH", 0.0, 1.0, 0.25);
    bindLock(oscMorphSlider, Lock_DCO1Morph, "D1 MORPH");
    attachSlider(oscMorphSlider, "dco1Morph");

    setupRotaryKnob(oscPWMSlider, oscPWMLabel, "D1 PWM", 0.05, 0.95, 0.50);
    bindLock(oscPWMSlider, Lock_DCO1PWM, "D1 PWM");
    attachSlider(oscPWMSlider, "dco1PWM");

    setupRotaryKnob(supersawMixSlider, supersawMixLabel, "SAW MIX", 0.0, 1.0, 0.0);
    bindLock(supersawMixSlider, Lock_DCO1SawMix, "SAW MIX");
    attachSlider(supersawMixSlider, "dco1SawMix");

    setupRotaryKnob(supersawDetuneSlider, supersawDetuneLabel, "SAW DETUNE", 0.0, 1.0, 0.50);
    attachSlider(supersawDetuneSlider, "dco1SawDetune");

    setupRotaryKnob(wavefoldSlider, wavefoldLabel, "D1 FOLD", 0.0, 1.0, 0.0);
    bindLock(wavefoldSlider, Lock_DCO1Fold, "D1 FOLD");
    attachSlider(wavefoldSlider, "dco1Fold");

    // Row 2: DCO 2 Controls
    setupRotaryKnob(dco2MorphSlider, dco2MorphLabel, "D2 MORPH", 0.0, 1.0, 0.50);
    bindLock(dco2MorphSlider, Lock_DCO2Morph, "D2 MORPH");
    attachSlider(dco2MorphSlider, "dco2Morph");

    setupRotaryKnob(dco2PWMSlider, dco2PWMLabel, "D2 PWM", 0.05, 0.95, 0.50);
    bindLock(dco2PWMSlider, Lock_DCO2PWM, "D2 PWM");
    attachSlider(dco2PWMSlider, "dco2PWM");

    setupRotaryKnob(semiTuneSlider, semiTuneLabel, "D2 SEMI", -24.0, 24.0, 0.0, "", true);
    bindLock(semiTuneSlider, Lock_DCO2Semi, "D2 SEMI");
    attachSlider(semiTuneSlider, "dco2Semi");

    setupRotaryKnob(fineTuneSlider, fineTuneLabel, "D2 FINE", -50.0, 50.0, 0.0, "", true);
    bindLock(fineTuneSlider, Lock_DCO2Cents, "D2 FINE");
    attachSlider(fineTuneSlider, "dco2Cents");

    setupRotaryKnob(fmAmountSlider, fmAmountLabel, "FM AMT", 0.0, 1.0, 0.0);
    attachSlider(fmAmountSlider, "xmodAmount");

    setupRotaryKnob(dco2FoldSlider, dco2FoldLabel, "D2 FOLD", 0.0, 1.0, 0.0);
    attachSlider(dco2FoldSlider, "dco2Fold");

    // Row 3: Mixer, Sub, Noise & Glide Deck
    dco2LevelSlider.setBounds(20, 256, 52, 50);
    setupRotaryKnob(dco2LevelSlider, dco2LevelLabel, "D2 LEVEL", 0.0, 1.0, 0.0);
    bindLock(dco2LevelSlider, Lock_DCO2Level, "D2 LEVEL");
    attachSlider(dco2LevelSlider, "dco2Level");

    setupRotaryKnob(subOscLevelSlider, subOscLevelLabel, "SUB LEVEL", 0.0, 1.0, 0.0);
    bindLock(subOscLevelSlider, Lock_SubLevel, "SUB LEVEL");
    attachSlider(subOscLevelSlider, "subLevel");

    setupButton(subOscOctaveButton, "SUB -1/-2", true);
    attachButton(subOscOctaveButton, "subOctave");

    setupRotaryKnob(noiseLevelSlider, noiseLevelLabel, "NOISE", 0.0, 1.0, 0.0);
    bindLock(noiseLevelSlider, Lock_NoiseLevel, "NOISE");
    attachSlider(noiseLevelSlider, "noiseLevel");

    setupButton(supersawModeButton, "HARD SYNC", true);
    attachButton(supersawModeButton, "syncMode");

    setupRotaryKnob(glideTimeSlider, glideTimeLabel, "GLIDE", 0.0, 2.0, 0.05);
    attachSlider(glideTimeSlider, "glideTime");

    setupButton(glideEnableButton, "PORTA", true);
    attachButton(glideEnableButton, "glideEnable");

    setupButton(legatoModeButton, "LEGATO", true);
    legatoModeButton.setToggleState(audioProcessor.voiceManager.isLegatoMode(), juce::dontSendNotification);
    legatoModeButton.onClick = [this]() {
        audioProcessor.voiceManager.setLegatoMode(legatoModeButton.getToggleState());
    };

    // ==========================================
    // 4. Dual Filter Section Controls
    // ==========================================
    setupButton(filterRoutingButton, "PARALLEL", true);
    attachButton(filterRoutingButton, "filterRouting");

    setupRotaryKnob(filterBalanceSlider, filterBalanceLabel, "FILT BAL", 0.0, 1.0, 0.50, "", true);
    bindLock(filterBalanceSlider, Lock_FilterBalance, "FILT BAL");
    attachSlider(filterBalanceSlider, "filterBalance");

    f1CutoffSlider.setSkewFactorFromMidPoint(1000.0);
    setupRotaryKnob(f1CutoffSlider, f1CutoffLabel, "F1 CUTOFF", 20.0, 20000.0, 3000.0);
    bindLock(f1CutoffSlider, Lock_LPFCutoff, "F1 CUTOFF");
    attachSlider(f1CutoffSlider, "lpfCutoff");

    setupRotaryKnob(f1ResonanceSlider, f1ResonanceLabel, "F1 RESO", 0.0, 0.98, 0.0);
    bindLock(f1ResonanceSlider, Lock_LPFRes, "F1 RESO");
    attachSlider(f1ResonanceSlider, "lpfRes");

    setupRotaryKnob(f1HPFSlider, f1HPFLabel, "HPF CUTOFF", 10.0, 1000.0, 20.0);
    f1HPFSlider.setSkewFactorFromMidPoint(100.0);
    bindLock(f1HPFSlider, Lock_HPFCutoff, "HPF CUTOFF");
    attachSlider(f1HPFSlider, "hpfCutoff");

    setupRotaryKnob(f1EnvDepthSlider, f1EnvDepthLabel, "F1 ENV", -1.0, 1.0, 0.0, "", true);
    bindLock(f1EnvDepthSlider, Lock_EnvMod, "F1 ENV");
    attachSlider(f1EnvDepthSlider, "envMod");

    setupRotaryKnob(f1KeyTrackSlider, f1KeyTrackLabel, "F1 KEY", 0.0, 1.0, 0.50);
    attachSlider(f1KeyTrackSlider, "f1KeyTrack");

    setupRotaryKnob(f1LfoDepthSlider, f1LfoDepthLabel, "F1 LFO", 0.0, 1.0, 0.0);
    attachSlider(f1LfoDepthSlider, "lfo1ToFilter");

    setupRotaryKnob(f2CutoffSlider, f2CutoffLabel, "F2 CUTOFF", 20.0, 20000.0, 3000.0);
    f2CutoffSlider.setSkewFactorFromMidPoint(1000.0);
    bindLock(f2CutoffSlider, Lock_F2Cutoff, "F2 CUTOFF");
    attachSlider(f2CutoffSlider, "f2Cutoff");

    setupRotaryKnob(f2ResonanceSlider, f2ResonanceLabel, "F2 RESO", 0.0, 1.0, 0.0);
    bindLock(f2ResonanceSlider, Lock_F2Res, "F2 RESO");
    attachSlider(f2ResonanceSlider, "f2Res");

    setupComboBox(f2ModeComboBox, f2ModeLabel, "F2 MODE", { "Lowpass", "Bandpass", "Highpass", "Notch", "Peak" }, 1);
    attachComboBox(f2ModeComboBox, "f2Mode");

    setupRotaryKnob(f2EnvDepthSlider, f2EnvDepthLabel, "F2 ENV", -1.0, 1.0, 0.0, "", true);
    bindLock(f2EnvDepthSlider, Lock_F2EnvDepth, "F2 ENV");
    attachSlider(f2EnvDepthSlider, "f2EnvDepth");

    setupRotaryKnob(f2KeyTrackSlider, f2KeyTrackLabel, "F2 KEY", 0.0, 1.0, 0.0);
    attachSlider(f2KeyTrackSlider, "f2KeyTrack");

    // ==========================================
    // 5. Oscilloscope Screen & Reboot Theme Sync
    // ==========================================
    oscilloscope.setQueue(&audioProcessor.oscilloscopeQueue);
    configureFocus(oscilloscope);
    addAndMakeVisible(oscilloscope);

    oscilloscope.onThemeChanged = [this](OscilloscopeComponent::Theme newTheme) {
        SkinManager::getInstance().setCRTTheme(static_cast<CRTTheme>(newTheme));
        lcdScreen.setParameterEdit("CRT THEME", oscilloscope.getThemeName(), 1.0f);
    };

    // ==========================================
    // 6. Dynamic Vector ADSR LCD & Unified 4-Knob Deck
    // ==========================================
    configureFocus(envLcd);
    addAndMakeVisible(envLcd);

    setupButton(envPrevButton, "<", false);
    setupButton(envVcfTabButton, "VCF", true);
    setupButton(envVcaTabButton, "VCA", true);
    setupButton(envAuxTabButton, "AUX", true);
    setupButton(envNextButton, ">", false);

    envVcfTabButton.setRadioGroupId(2001);
    envVcaTabButton.setRadioGroupId(2001);
    envAuxTabButton.setRadioGroupId(2001);
    envVcfTabButton.setToggleState(true, juce::dontSendNotification);

    envPrevButton.onClick = [this]() {
        setActiveEnvelope((activeEnvIndex + 2) % 3);
    };

    envNextButton.onClick = [this]() {
        setActiveEnvelope((activeEnvIndex + 1) % 3);
    };

    envVcfTabButton.onClick = [this]() { setActiveEnvelope(0); };
    envVcaTabButton.onClick = [this]() { setActiveEnvelope(1); };
    envAuxTabButton.onClick = [this]() { setActiveEnvelope(2); };

    setupRotaryKnob(envAttackSlider, envAttackLabel, "ATTACK", 0.001, 3.0, 0.01);
    setupRotaryKnob(envDecaySlider, envDecayLabel, "DECAY", 0.001, 3.0, 0.10);
    setupRotaryKnob(envSustainSlider, envSustainLabel, "SUSTAIN", 0.0, 1.0, 1.0);
    setupRotaryKnob(envReleaseSlider, envReleaseLabel, "RELEASE", 0.001, 5.0, 0.10);

    auto bindEnvLock = [this](juce::Slider& slider, int stage) {
        slider.onValueChange = [this, &slider, stage]() {
            updateEnvelopeLcd();
            if (isUpdatingKnobsFromStep) return;

            const float val = static_cast<float>(slider.getValue());
            const float norm = static_cast<float>(slider.valueToProportionOfLength(val));

            if (audioProcessor.sequencer.isRecording) {
                auto& track = audioProcessor.sequencer.getActiveTrack();
                const int targetStep = audioProcessor.sequencer.isPlaying ? track.getCurrentStep() : selectedStepIndex;

                LockParamID lockId = Lock_FiltAttack;
                if (activeEnvIndex == 0) {
                    if (stage == 0) lockId = Lock_FiltAttack;
                    else if (stage == 1) lockId = Lock_FiltDecay;
                    else if (stage == 2) lockId = Lock_FiltSustain;
                    else if (stage == 3) lockId = Lock_FiltRelease;
                } else if (activeEnvIndex == 1) {
                    if (stage == 0) lockId = Lock_AmpAttack;
                    else if (stage == 1) lockId = Lock_AmpDecay;
                    else if (stage == 2) lockId = Lock_AmpSustain;
                    else if (stage == 3) lockId = Lock_AmpRelease;
                }

                if (activeEnvIndex != 2) {
                    track.getStep(targetStep).locks.setLock(lockId, val);
                    updateSequencerLcd();
                }

                lcdScreen.setParameterEdit("LOCK S" + juce::String(targetStep + 1) + " ENV", slider.getTextFromValue(val), norm);
            } else {
                lcdScreen.setParameterEdit("ENV EDIT", slider.getTextFromValue(val), norm);
            }
        };
    };

    bindEnvLock(envAttackSlider, 0);
    bindEnvLock(envDecaySlider, 1);
    bindEnvLock(envSustainSlider, 2);
    bindEnvLock(envReleaseSlider, 3);

    auxModSectionLabel.setText("AUX MOD ROUTING", juce::dontSendNotification);
    auxModSectionLabel.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
    auxModSectionLabel.setJustificationType(juce::Justification::centred);
    configureFocus(auxModSectionLabel);
    addAndMakeVisible(auxModSectionLabel);

    setupRotaryKnob(auxToPitchSlider, auxToPitchLabel, "> PITCH", -24.0, 24.0, 0.0, "", true);
    bindLock(auxToPitchSlider, Lock_AuxToPitch, "AUX > PITCH");
    attachSlider(auxToPitchSlider, "auxToPitch");

    setupRotaryKnob(auxToFilter1Slider, auxToFilter1Label, "> FILT 1", -1.0, 1.0, 0.0, "", true);
    bindLock(auxToFilter1Slider, Lock_AuxToFilter1, "AUX > FILT 1");
    attachSlider(auxToFilter1Slider, "auxToFilter1");

    setupRotaryKnob(auxToFilter2Slider, auxToFilter2Label, "> FILT 2", -1.0, 1.0, 0.0, "", true);
    bindLock(auxToFilter2Slider, Lock_AuxToFilter2, "AUX > FILT 2");
    attachSlider(auxToFilter2Slider, "auxToFilter2");

    setupRotaryKnob(auxToWavefoldSlider, auxToWavefoldLabel, "> FOLD", 0.0, 1.0, 0.0);
    bindLock(auxToWavefoldSlider, Lock_AuxToFold, "AUX > FOLD");
    attachSlider(auxToWavefoldSlider, "auxToFold");

    setupRotaryKnob(auxToPWMSlider, auxToPWMLabel, "> PWM", -1.0, 1.0, 0.0, "", true);
    bindLock(auxToPWMSlider, Lock_AuxToPWM, "AUX > PWM");
    attachSlider(auxToPWMSlider, "auxToPWM");

    setActiveEnvelope(0);

    // ==========================================
    // 7. LFO Section Controls
    // ==========================================
    setupComboBox(lfoSelectComboBox, lfoSelectLabel, "LFO SELECT", { "LFO 1", "LFO 2", "LFO 3" }, 1);
    setupComboBox(lfoShapeComboBox, lfoShapeLabel, "SHAPE", { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" }, 2);
    attachComboBox(lfoShapeComboBox, "lfo1Shape");

    setupRotaryKnob(lfoRateSlider, lfoRateLabel, "RATE", 0.1, 25.0, 2.0);
    lfoRateSlider.setSkewFactorFromMidPoint(3.0);
    attachSlider(lfoRateSlider, "lfo1Rate");

    setupRotaryKnob(lfoDepthSlider, lfoDepthLabel, "TO FILT", 0.0, 1.0, 0.0);
    attachSlider(lfoDepthSlider, "lfo1ToFilter");

    // ==========================================
    // 8. Modulation Matrix Controls
    // ==========================================
    juce::StringArray slotNames;
    for (int i = 1; i <= 16; ++i) slotNames.add("Slot " + juce::String(i));

    setupComboBox(modSlotSelectComboBox, modSlotSelectLabel, "SLOT", slotNames, 1);
    setupComboBox(modSourceComboBox, modSourceLabel, "SOURCE", ModulationMatrix::getSourceChoices(), 1);
    setupComboBox(modDestComboBox, modDestLabel, "DESTINATION", ModulationMatrix::getDestinationChoices(), 1);
    setupRotaryKnob(modAmountSlider, modAmountLabel, "AMOUNT", -1.0, 1.0, 0.0, "", true);

    setupComboBox(modViaSourceComboBox, modViaSourceLabel, "VIA CONTROLLER", ModulationMatrix::getSourceChoices(), 1);
    setupRotaryKnob(modViaAmountSlider, modViaAmountLabel, "VIA DEPTH", -1.0, 1.0, 1.0, "", true);

    setupButton(modSlotEnableButton, "ENABLE", true);

    modSlotSelectComboBox.onChange = [this]() {
        refreshMatrixUI();
    };

    auto updateCurrentSlot = [this]() {
        const int slotIdx = modSlotSelectComboBox.getSelectedItemIndex();
        if (slotIdx < 0 || slotIdx >= ModulationMatrix::NUM_SLOTS) return;
        const auto src = static_cast<ModSource>(modSourceComboBox.getSelectedItemIndex());
        const auto dst = static_cast<ModDestination>(modDestComboBox.getSelectedItemIndex());
        const float amt = static_cast<float>(modAmountSlider.getValue());
        const auto via = static_cast<ModSource>(modViaSourceComboBox.getSelectedItemIndex());
        const float viaAmt = static_cast<float>(modViaAmountSlider.getValue());
        const bool en = modSlotEnableButton.getToggleState();
        audioProcessor.modMatrix.setSlot(slotIdx, src, dst, amt, via, viaAmt, en);
    };

    modSourceComboBox.onChange       = updateCurrentSlot;
    modDestComboBox.onChange         = updateCurrentSlot;
    modAmountSlider.onValueChange    = updateCurrentSlot;
    modViaSourceComboBox.onChange    = updateCurrentSlot;
    modViaAmountSlider.onValueChange = updateCurrentSlot;
    modSlotEnableButton.onClick      = updateCurrentSlot;

    refreshMatrixUI();

    // ==========================================
    // 9. Bi-Timbral Sequencer Section & Vector LCD
    // ==========================================
    setupButton(seqTrackLayerButton, "TRACK A", false);
    setupButton(seqTrackMuteButton, "MUTE", true);
    setupButton(seqTrackSoloButton, "SOLO", true);

    seqTrackLayerButton.onClick = [this]() {
        audioProcessor.sequencer.toggleEditTrack();
        updateTrackStatusUI();
    };

    seqTrackMuteButton.onClick = [this]() {
        audioProcessor.sequencer.getActiveTrack().setMute(seqTrackMuteButton.getToggleState());
    };

    seqTrackSoloButton.onClick = [this]() {
        audioProcessor.sequencer.getActiveTrack().setSolo(seqTrackSoloButton.getToggleState());
    };

    setupButton(seqPage1Button, "1", true);
    setupButton(seqPage2Button, "2", true);
    setupButton(seqPage3Button, "3", true);
    setupButton(seqPage4Button, "4", true);

    seqPage1Button.setRadioGroupId(1001);
    seqPage2Button.setRadioGroupId(1001);
    seqPage3Button.setRadioGroupId(1001);
    seqPage4Button.setRadioGroupId(1001);
    seqPage1Button.setToggleState(true, juce::dontSendNotification);

    auto setSeqPage = [this](int page) {
        const int neededPages = page + 1;
        if (audioProcessor.sequencer.getNumPages() < neededPages) {
            audioProcessor.sequencer.setNumPages(neededPages);
            if (auto* p = audioProcessor.apvts.getParameter("seqPages"))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(neededPages - 1)));
        }

        audioProcessor.sequencer.setActivePage(page);
        updateSequencerLcd();
        refreshSelectedStepUI();
    };

    seqPage1Button.onClick = [setSeqPage]() { setSeqPage(0); };
    seqPage2Button.onClick = [setSeqPage]() { setSeqPage(1); };
    seqPage3Button.onClick = [setSeqPage]() { setSeqPage(2); };
    seqPage4Button.onClick = [setSeqPage]() { setSeqPage(3); };

    setupButton(seqPlayButton, "PLAY", true);
    attachButton(seqPlayButton, "seqPlay");

    setupButton(seqRecButton, "REC", true);
    attachButton(seqRecButton, "seqRec");
    seqRecButton.onClick = [this]() {
        const bool isRec = seqRecButton.getToggleState();
        audioProcessor.sequencer.isRecording = isRec;
        if (isRec) {
            audioProcessor.snapshotBasePatch(audioProcessor.getActiveEditPart());
            syncKnobsToStepLocksOrBase();
            lcdScreen.setParameterEdit("SEQUENCER REC", "P-LOCK ARMED: TWEAK KNOBS", 1.0f);
        } else {
            audioProcessor.restoreBasePatch(audioProcessor.getActiveEditPart());
            syncKnobsToStepLocksOrBase();
            lcdScreen.setParameterEdit("SEQUENCER REC", "RECORDING OFF - PATCH RESTORED", 0.0f);
        }
    };

    setupButton(arpEnableButton, "ARP", true);
    attachButton(arpEnableButton, "arpEnable");

    setupButton(chordEnableButton, "CHORD", true);
    attachButton(chordEnableButton, "chordEnable");

    addAndMakeVisible(seqLcd);

    seqLcd.onStepClicked = [this](int stepIdx) {
        selectedStepIndex = stepIdx;
        audioProcessor.sequencer.setSelectedStep(selectedStepIndex);
        refreshSelectedStepUI();
        updateSequencerLcd();
    };

    seqLcd.onStepVelocityChanged = [this](int stepIdx, float newVel) {
        auto& track = audioProcessor.sequencer.getActiveTrack();
        track.getStep(stepIdx).velocity = std::clamp(newVel, 0.05f, 1.0f);
        updateSequencerLcd();
    };

    seqLcd.onStepGateChanged = [this](int stepIdx, float newGate) {
        auto& track = audioProcessor.sequencer.getActiveTrack();
        track.setStepGateLength(stepIdx, newGate);
        stepGateSlider.setValue(newGate <= 0.0f ? 0.0 : newGate, juce::dontSendNotification);
        updateSequencerLcd();
    };

    seqLcd.onStepToggleTrigger = [this](int stepIdx) {
        auto& track = audioProcessor.sequencer.getActiveTrack();
        track.toggleStepActive(stepIdx);
        updateSequencerLcd();
    };

    seqLcd.onStepTranspose = [this](int stepIdx, int semitones) {
        auto& track = audioProcessor.sequencer.getActiveTrack();
        track.transposeStep(stepIdx, semitones);
        updateSequencerLcd();
    };

    seqLcd.onStepNavigate = [this](int stepOffset) {
        const int totalSteps = audioProcessor.sequencer.getNumSteps();
        selectedStepIndex = (selectedStepIndex + stepOffset + totalSteps) % totalSteps;
        audioProcessor.sequencer.setSelectedStep(selectedStepIndex);

        const int pageIdx = selectedStepIndex / 16;
        audioProcessor.sequencer.setActivePage(pageIdx);
        if (pageIdx == 0) seqPage1Button.setToggleState(true, juce::dontSendNotification);
        else if (pageIdx == 1) seqPage2Button.setToggleState(true, juce::dontSendNotification);
        else if (pageIdx == 2) seqPage3Button.setToggleState(true, juce::dontSendNotification);
        else if (pageIdx == 3) seqPage4Button.setToggleState(true, juce::dontSendNotification);

        refreshSelectedStepUI();
        updateSequencerLcd();
    };

    seqLcd.onStepAudition = [this](int) {
        auditionSelectedStep();
    };

    // Step Navigation Strip
    setupButton(prevStepButton, "<", false);
    prevStepButton.onClick = [this]() {
        seqLcd.grabKeyboardFocus();
        if (seqLcd.onStepNavigate) seqLcd.onStepNavigate(-1);
    };

    currentStepLabel.setText("STEP 1", juce::dontSendNotification);
    currentStepLabel.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    currentStepLabel.setJustificationType(juce::Justification::centred);
    configureFocus(currentStepLabel);
    addAndMakeVisible(currentStepLabel);

    setupButton(nextStepButton, ">", false);
    nextStepButton.onClick = [this]() {
        seqLcd.grabKeyboardFocus();
        if (seqLcd.onStepNavigate) seqLcd.onStepNavigate(1);
    };

    setupButton(auditionStepButton, "AUDITION", false);
    auditionStepButton.onClick = [this]() {
        auditionSelectedStep();
    };

    setupButton(clearLocksButton, "CLR S1", false);
    clearLocksButton.onClick = [this]() {
        auto& step = audioProcessor.sequencer.getActiveTrack().getStep(selectedStepIndex);
        step.locks.clear();
        syncKnobsToStepLocksOrBase();
        refreshSelectedStepUI();
        updateSequencerLcd();
        lcdScreen.setParameterEdit("CLEAR LOCKS", "STEP " + juce::String(selectedStepIndex + 1) + " RESTORED", 0.0f);
    };

    // Unified Hardware Rotary Dial for Step Gate Length (TRK = follow global, 5% to 200%)
    setupRotaryKnob(stepGateSlider, stepGateLabel, "STEP GATE", 0.0, 2.0, 0.0);
    stepGateSlider.textFromValueFunction = [](double val) {
        if (val <= 0.001) return juce::String("TRK");
        return juce::String(juce::roundToInt(val * 100.0)) + "%";
    };
    stepGateSlider.onValueChange = [this]() {
        if (isUpdatingKnobsFromStep) return;
        const float val = static_cast<float>(stepGateSlider.getValue());
        auto& track = audioProcessor.sequencer.getActiveTrack();
        track.setStepGateLength(selectedStepIndex, (val <= 0.001f) ? -1.0f : val);
        updateSequencerLcd();
        lcdScreen.setParameterEdit("STEP GATE", stepGateSlider.getTextFromValue(val), (val <= 0.001f) ? 0.0f : (val / 2.0f));
    };

    setupComboBox(chordTypeComboBox, chordTypeLabel, "CHORD",
                  { "Major", "Minor", "Maj7", "Min7", "Dom7", "Sus4", "Dim", "Octave", "5th" }, 1);
    attachComboBox(chordTypeComboBox, "chordType");

    // Bottom Unified Hardware Controls
    setupRotaryKnob(seqTempoSlider, seqTempoLabel, "BPM", 20.0, 300.0, 120.0);
    seqTempoSlider.setNumDecimalPlacesToDisplay(1);
    attachSlider(seqTempoSlider, "internalBpm");

    setupButton(seqSyncButton, "HOST", true);
    attachButton(seqSyncButton, "syncToHost");

    setupRotaryKnob(seqSwingSlider, seqSwingLabel, "SWING", 0.0, 75.0, 0.0);
    attachSlider(seqSwingSlider, "seqSwing");

    setupRotaryKnob(seqGateLengthSlider, seqGateLengthLabel, "GATE", 5.0, 95.0, 75.0);
    attachSlider(seqGateLengthSlider, "seqGate");

    setupComboBox(seqDivisionComboBox, seqDivisionLabel, "RATE", { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 4);
    attachComboBox(seqDivisionComboBox, "seqDivision");

    setupComboBox(seqDirectionComboBox, seqDirectionLabel, "DIR", { "Forward", "Reverse", "Ping-Pong", "Random" }, 1);
    attachComboBox(seqDirectionComboBox, "seqDirection");

    setupComboBox(seqLengthComboBox, seqLengthLabel, "LEN", { "16st", "32st", "48st", "64st" }, 4);
    seqLengthComboBox.onChange = [this]() {
        const int numPages = seqLengthComboBox.getSelectedItemIndex() + 1;
        audioProcessor.sequencer.setNumPages(numPages);
        updateSequencerLcd();
    };
    attachComboBox(seqLengthComboBox, "seqPages");

    setupComboBox(seqModeComboBox, seqModeLabel, "ARP MODE", { "Up", "Down", "Up/Down", "Down/Up", "Random", "As Played", "Chord" }, 1);
    attachComboBox(seqModeComboBox, "arpMode");

    // ==========================================
    // 10. Master FX Section Controls
    // ==========================================
    setupButton(chorus1Button, "CHO I", true);
    setupButton(chorus2Button, "CHO II", true);

    auto updateChorusModeFromButtons = [this]() {
        const int mode = (chorus1Button.getToggleState() ? 1 : 0) + (chorus2Button.getToggleState() ? 2 : 0);
        if (auto* p = audioProcessor.apvts.getParameter("chorusMode"))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(mode)));
    };
    chorus1Button.onClick = updateChorusModeFromButtons;
    chorus2Button.onClick = updateChorusModeFromButtons;

    setupComboBox(fxRoutingComboBox, fxRoutingLabel, "ROUTING", { "Both Tracks", "Track A Only", "Track B Only" }, 1);
    attachComboBox(fxRoutingComboBox, "fxRouting");

    setupButton(delaySyncButton, "SYNC", true);
    attachButton(delaySyncButton, "delaySyncEnable");

    setupButton(delayPingPongButton, "PING-PONG", true);
    attachButton(delayPingPongButton, "delayPingPong");

    setupRotaryKnob(delayTimeSlider, delayTimeLabel, "TIME", 0.0, 12.0, 6.0);
    delayTimeSlider.setRange(0.0, 12.0, 1.0);
    attachSlider(delayTimeSlider, "delaySync");

    setupRotaryKnob(delayFeedbackSlider, delayFeedbackLabel, "FEEDBACK", 0.0, 0.95, 0.40);
    attachSlider(delayFeedbackSlider, "delayFeedback");

    setupRotaryKnob(delayDampSlider, delayDampLabel, "DAMP", 0.0, 0.95, 0.25);
    attachSlider(delayDampSlider, "delayDamp");

    setupRotaryKnob(delayMixSlider, delayMixLabel, "DLY MIX", 0.0, 1.0, 0.0);
    attachSlider(delayMixSlider, "delayMix");

    setupRotaryKnob(reverbSizeSlider, reverbSizeLabel, "SIZE", 0.0, 1.0, 0.65);
    attachSlider(reverbSizeSlider, "reverbSize");

    setupRotaryKnob(reverbDampSlider, reverbDampLabel, "DAMP", 0.0, 1.0, 0.30);
    attachSlider(reverbDampSlider, "reverbDamp");

    setupRotaryKnob(reverbMixSlider, reverbMixLabel, "REV MIX", 0.0, 1.0, 0.0);
    attachSlider(reverbMixSlider, "reverbMix");

    // ==========================================
    // 11. Bottom Performance Deck (Vertical Pitch & Mod Wheels with Double-Click Reset)
    // ==========================================
    setupLinearFader(pitchWheelSlider, pitchWheelLabel, "PITCH", -1.0, 1.0, 0.0);
    pitchWheelSlider.setDoubleClickReturnValue(true, 0.0);
    pitchWheelSlider.textFromValueFunction = [](double val) {
        return juce::String(val, 2);
    };
    pitchWheelSlider.onValueChange = [this]() {
        const float val = static_cast<float>(pitchWheelSlider.getValue());
        audioProcessor.setPitchBend(val, audioProcessor.getActiveEditPart());
        const float norm = (val + 1.0f) * 0.5f;
        lcdScreen.setParameterEdit("PITCH BEND", juce::String(val, 2), norm);
    };

    setupLinearFader(modWheelSlider, modWheelLabel, "MOD", 0.0, 1.0, 0.0);
    modWheelSlider.setDoubleClickReturnValue(true, 0.0);
    modWheelSlider.onValueChange = [this]() {
        const float val = static_cast<float>(modWheelSlider.getValue());
        audioProcessor.setModWheel(val, audioProcessor.getActiveEditPart());
        lcdScreen.setParameterEdit("MOD WHEEL", juce::String(juce::roundToInt(val * 100.0f)) + " %", val);
    };

    keyboardComponent.setAvailableRange(36, 96);
    keyboardComponent.setLowestVisibleKey(36);
    keyboardComponent.setKeyWidth(1240.0f / 36.0f);
    keyboardComponent.setScrollButtonsVisible(false);
    keyboardComponent.setWantsKeyboardFocus(true);
    keyboardComponent.setMouseClickGrabsKeyboardFocus(true);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colours::white);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff121418));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff2d313a));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, juce::Colour(0xff0b0d10));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0x6000f0ff));
    addAndMakeVisible(keyboardComponent);

    // ==========================================
    // 12. Deep White-Box Eradication Sweep
    // ==========================================
    for (auto* comp : getChildren())
    {
        if (auto* slider = dynamic_cast<juce::Slider*>(comp))
        {
            slider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            slider->setColour(juce::Slider::textBoxHighlightColourId, juce::Colours::transparentBlack);
            slider->setOpaque(false);

            for (auto* child : slider->getChildren())
            {
                child->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
                child->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
                child->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff121418));
                child->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
                child->setOpaque(false);
            }
        }
        else if (auto* label = dynamic_cast<juce::Label*>(comp))
        {
            label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
            label->setOpaque(false);
        }
    }

    setSize(UI_WIDTH, UI_HEIGHT);
    startTimerHz(30);

    refreshPresetList();
    updateTrackStatusUI();
    updateSequencerLcd();
}

Simple116AudioProcessorEditor::~Simple116AudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

// =============================================================================
// Unified Dynamic Envelope APVTS Re-Binding & LCD Visualization
// =============================================================================
void Simple116AudioProcessorEditor::setActiveEnvelope(int envIndex)
{
    activeEnvIndex = std::clamp(envIndex, 0, 2);

    envVcfTabButton.setToggleState(activeEnvIndex == 0, juce::dontSendNotification);
    envVcaTabButton.setToggleState(activeEnvIndex == 1, juce::dontSendNotification);
    envAuxTabButton.setToggleState(activeEnvIndex == 2, juce::dontSendNotification);

    juce::String attId, decId, susId, relId;
    if (activeEnvIndex == 0) {
        attId = "filtAttack"; decId = "filtDecay"; susId = "filtSustain"; relId = "filtRelease";
    } else if (activeEnvIndex == 1) {
        attId = "ampAttack";  decId = "ampDecay";  susId = "ampSustain";  relId = "ampRelease";
    } else {
        attId = "auxAttack";  decId = "auxDecay";  susId = "auxSustain";  relId = "auxRelease";
    }

    envAttackAttachment.reset();
    envDecayAttachment.reset();
    envSustainAttachment.reset();
    envReleaseAttachment.reset();

    if (audioProcessor.apvts.getParameter(attId) != nullptr)
        envAttackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, attId, envAttackSlider);
    if (audioProcessor.apvts.getParameter(decId) != nullptr)
        envDecayAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, decId, envDecaySlider);
    if (audioProcessor.apvts.getParameter(susId) != nullptr)
        envSustainAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, susId, envSustainSlider);
    if (audioProcessor.apvts.getParameter(relId) != nullptr)
        envReleaseAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, relId, envReleaseSlider);

    envAttackSlider.textFromValueFunction = formatEnvTime;
    envAttackSlider.valueFromTextFunction = parseEnvTime;
    envDecaySlider.textFromValueFunction  = formatEnvTime;
    envDecaySlider.valueFromTextFunction  = parseEnvTime;
    envSustainSlider.textFromValueFunction = formatPercent;
    envSustainSlider.valueFromTextFunction = parsePercent;
    envReleaseSlider.textFromValueFunction = formatEnvTime;
    envReleaseSlider.valueFromTextFunction = parseEnvTime;

    envAttackSlider.updateText();
    envDecaySlider.updateText();
    envSustainSlider.updateText();
    envReleaseSlider.updateText();

    updateEnvelopeLcd();
}

void Simple116AudioProcessorEditor::updateEnvelopeLcd() noexcept
{
    juce::String name;
    if (activeEnvIndex == 0)      name = "VCF (FILTER) ENV";
    else if (activeEnvIndex == 1) name = "VCA (AMP) ENV";
    else                          name = "AUX / MOD ENV";

    const float aNorm = static_cast<float>(envAttackSlider.valueToProportionOfLength(envAttackSlider.getValue()));
    const float dNorm = static_cast<float>(envDecaySlider.valueToProportionOfLength(envDecaySlider.getValue()));
    const float sNorm = static_cast<float>(envSustainSlider.valueToProportionOfLength(envSustainSlider.getValue()));
    const float rNorm = static_cast<float>(envReleaseSlider.valueToProportionOfLength(envReleaseSlider.getValue()));

    envLcd.setEnvelope(name,
                       aNorm, dNorm, sNorm, rNorm,
                       envAttackSlider.getTextFromValue(envAttackSlider.getValue()),
                       envDecaySlider.getTextFromValue(envDecaySlider.getValue()),
                       envSustainSlider.getTextFromValue(envSustainSlider.getValue()),
                       envReleaseSlider.getTextFromValue(envReleaseSlider.getValue()));
}

void Simple116AudioProcessorEditor::updateCardStatusUI() {
    const bool isMounted = audioProcessor.isExpansionCardMounted();
    const auto* card = audioProcessor.getActiveExpansionCard();

    if (isMounted && card != nullptr) {
        cardButton.setButtonText(card->cardId);
        cardButton.setToggleState(true, juce::dontSendNotification);
        lcdScreen.setCardInfo(true, card->cardId, card->cardName, card->lcdBanner1, card->lcdBanner2);
    } else {
        cardButton.setButtonText("CARD");
        cardButton.setToggleState(false, juce::dontSendNotification);
        lcdScreen.setCardInfo(false);
    }
}

void Simple116AudioProcessorEditor::refreshPresetList() {
    updateCardStatusUI();

    const auto presets = audioProcessor.getAvailablePresets();
    const auto currentName = audioProcessor.getCurrentPresetName();

    presetComboBox.clear(juce::dontSendNotification);
    presetComboBox.addItemList(presets, 1);

    const int idx = presets.indexOf(currentName);
    if (idx >= 0)
        presetComboBox.setSelectedId(idx + 1, juce::dontSendNotification);
    else if (presetComboBox.getNumItems() > 0)
        presetComboBox.setSelectedId(1, juce::dontSendNotification);

    lcdScreen.setPresetInfo(presetComboBox.getText(), "READY");
}

void Simple116AudioProcessorEditor::showExpansionCardMenu() {
    juce::PopupMenu menu;
    const bool isMounted = audioProcessor.isExpansionCardMounted();
    const auto* activeCard = audioProcessor.getActiveExpansionCard();

    if (isMounted && activeCard != nullptr) {
        menu.addItem(1, "Eject Card [" + activeCard->cardId + "] (Return to Internal ROM)", true);
        menu.addItem(4, "Delete Current Patch from [" + activeCard->cardId + "]...", true);
    } else {
        menu.addItem(1, "Internal ROM Bank Active (No Card Inserted)", false);
    }

    menu.addSeparator();
    menu.addItem(10, "Create New Blank Cartridge... (Live Authoring)");
    menu.addItem(2, "Insert Card from File... (.s116card)");

    audioProcessor.expansionCardManager.scanExpansionsFolder();
    const auto& cards = audioProcessor.expansionCardManager.getAvailableCards();

    juce::PopupMenu cardsSubMenu;
    if (cards.empty()) {
        cardsSubMenu.addItem(999, "No cards in Documents/Fadedlimes/S-116/Expansions", false);
    } else {
        for (size_t i = 0; i < cards.size(); ++i) {
            const bool isActive = (isMounted && activeCard != nullptr && activeCard->cardId == cards[i].cardId);
            cardsSubMenu.addItem(100 + static_cast<int>(i),
                                 "[" + cards[i].cardId + "] " + cards[i].cardName,
                                 true, isActive);
        }
    }
    menu.addSubMenu("Available Cards in Expansions Folder", cardsSubMenu);

    menu.addSeparator();
    menu.addItem(3, "Export Active Bank to .s116card...");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&cardButton),
        [this, cards, activeCard](int result) {
            if (result == 1) {
                audioProcessor.ejectExpansionCard();
                refreshPresetList();
                updateCardStatusUI();
                lcdScreen.triggerBoot();
            } else if (result == 10) {
                auto* alert = new juce::AlertWindow("CREATE BLANK EXPANSION CARTRIDGE",
                                                    "Format a new writable .s116card virtual cartridge on disk:",
                                                    juce::AlertWindow::QuestionIcon);
                alert->addTextEditor("cardId", "EXP-01", "Cartridge ID (e.g. EXP-01):");
                alert->addTextEditor("cardName", "Night City Leads", "Soundbank Title:");
                alert->addTextEditor("author", "Fadedlimes", "Sound Designer / Author:");

                alert->addButton("Format & Mount", 1, juce::KeyPress(juce::KeyPress::returnKey));
                alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

                alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int res) {
                    if (res == 1) {
                        const auto id   = alert->getTextEditorContents("cardId").trim();
                        const auto name = alert->getTextEditorContents("cardName").trim();
                        const auto auth = alert->getTextEditorContents("author").trim();

                        if (audioProcessor.createNewExpansionCard(id, name, auth)) {
                            refreshPresetList();
                            updateCardStatusUI();
                            updateSequencerLcd();
                            refreshSelectedStepUI();
                            lcdScreen.triggerBoot();
                        }
                    }
                    delete alert;
                }), true);
            } else if (result == 4 && activeCard != nullptr) {
                const int curIdx = presetComboBox.getSelectedItemIndex();
                if (curIdx >= 0) {
                    auto* alert = new juce::AlertWindow("DELETE PATCH FROM CARTRIDGE",
                                                        "Are you sure you want to delete '" + audioProcessor.getCurrentPresetName() + "' from " + activeCard->cardId + "?",
                                                        juce::AlertWindow::WarningIcon);
                    alert->addButton("Delete", 1);
                    alert->addButton("Cancel", 0);
                    alert->enterModalState(true, juce::ModalCallbackFunction::create([this, curIdx, alert](int res) {
                        if (res == 1) {
                            if (audioProcessor.deletePatchFromActiveCard(curIdx)) {
                                refreshPresetList();
                                updateCardStatusUI();
                                updateSequencerLcd();
                                refreshSelectedStepUI();
                            }
                        }
                        delete alert;
                    }), true);
                }
            } else if (result == 2) {
                fileChooser = std::make_unique<juce::FileChooser>(
                    "Insert S-116 Voice Expansion Card",
                    ExpansionCardManager::getExpansionsDirectory(),
                    "*.s116card");

                fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this](const juce::FileChooser& fc) {
                        auto file = fc.getResult();
                        if (file.existsAsFile()) {
                            if (audioProcessor.mountExpansionCardFromFile(file)) {
                                refreshPresetList();
                                updateCardStatusUI();
                                lcdScreen.triggerBoot();
                            }
                        }
                    });
            } else if (result == 3) {
                exportCardDialog();
            } else if (result >= 100 && result < 100 + static_cast<int>(cards.size())) {
                const int cardIdx = result - 100;
                if (audioProcessor.mountExpansionCard(cardIdx)) {
                    refreshPresetList();
                    updateCardStatusUI();
                    lcdScreen.triggerBoot();
                }
            }
        });
}

void Simple116AudioProcessorEditor::exportCardDialog() {
    auto* alert = new juce::AlertWindow("EXPORT EXPANSION CARD",
                                        "Package current soundbank into a single .s116card file:",
                                        juce::AlertWindow::QuestionIcon);
    alert->addTextEditor("cardId", "EXP-01", "Card ID (e.g. EXP-01):");
    alert->addTextEditor("cardName", "90s Dance Classics", "Card Title:");
    alert->addTextEditor("author", "Fadedlimes", "Author:");

    alert->addButton("Save Card...", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int result) {
        if (result == 1) {
            const auto id   = alert->getTextEditorContents("cardId").trim();
            const auto name = alert->getTextEditorContents("cardName").trim();
            const auto auth = alert->getTextEditorContents("author").trim();

            const auto defaultFileName = (id.isNotEmpty() ? (id + " - ") : "") + (name.isNotEmpty() ? name : "Expansion") + ".s116card";
            const auto defaultDest = ExpansionCardManager::getExpansionsDirectory().getChildFile(defaultFileName);

            fileChooser = std::make_unique<juce::FileChooser>(
                "Export S-116 Voice Expansion Card",
                defaultDest,
                "*.s116card");

            fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                [this, id, name, auth](const juce::FileChooser& fc) {
                    auto dest = fc.getResult();
                    if (dest != juce::File()) {
                        if (dest.getFileExtension().toLowerCase() != ".s116card")
                            dest = dest.withFileExtension(".s116card");

                        if (audioProcessor.exportCurrentBankToCard(dest, id, name, auth)) {
                            refreshPresetList();
                            updateCardStatusUI();
                            lcdScreen.setParameterEdit("CARD EXPORTED", dest.getFileName(), 1.0f);
                        }
                    }
                });
        }
        delete alert;
    }), true);
}

void Simple116AudioProcessorEditor::savePresetDialog() {
    const bool isCardMounted = audioProcessor.isExpansionCardMounted();
    const auto* activeCard = audioProcessor.getActiveExpansionCard();

    auto* alert = new juce::AlertWindow("SAVE S-116 PRESET",
                                        isCardMounted ? ("Cartridge [" + activeCard->cardId + "] is active. Choose destination:")
                                                      : "Enter a name for the new patch file:",
                                        juce::AlertWindow::QuestionIcon);
    alert->addTextEditor("presetName", audioProcessor.getCurrentPresetName());

    if (isCardMounted && activeCard != nullptr) {
        alert->addButton("Burn to Cartridge [" + activeCard->cardId + "]", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Save to User Presets", 2);
    } else {
        alert->addButton("Save", 2, juce::KeyPress(juce::KeyPress::returnKey));
    }
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int result) {
        const auto name = alert->getTextEditorContents("presetName").trim();
        if (name.isNotEmpty()) {
            if (result == 1) {
                if (audioProcessor.savePatchToActiveCard(name)) {
                    refreshPresetList();
                    updateCardStatusUI();
                    lcdScreen.setParameterEdit("CARD UPDATED", name + " BURNED", 1.0f);
                }
            } else if (result == 2) {
                if (audioProcessor.saveUserPreset(name)) {
                    refreshPresetList();
                    lcdScreen.setParameterEdit("PRESET SAVED", name, 1.0f);
                }
            }
        }
        delete alert;
    }), true);
}

void Simple116AudioProcessorEditor::refreshMatrixUI() {
    const int slotIdx = modSlotSelectComboBox.getSelectedItemIndex();
    if (slotIdx < 0 || slotIdx >= ModulationMatrix::NUM_SLOTS) return;
    const auto& slot = audioProcessor.modMatrix.getSlot(slotIdx);
    modSourceComboBox.setSelectedId(static_cast<int>(slot.source) + 1, juce::dontSendNotification);
    modDestComboBox.setSelectedId(static_cast<int>(slot.destination) + 1, juce::dontSendNotification);
    modAmountSlider.setValue(slot.amount, juce::dontSendNotification);
    modViaSourceComboBox.setSelectedId(static_cast<int>(slot.viaSource) + 1, juce::dontSendNotification);
    modViaAmountSlider.setValue(slot.viaAmount, juce::dontSendNotification);
    modSlotEnableButton.setToggleState(slot.enabled, juce::dontSendNotification);
}

void Simple116AudioProcessorEditor::updateTrackStatusUI() {
    const bool isTrackB = (audioProcessor.sequencer.getEditTrack() == TrackID::TrackB);
    seqTrackLayerButton.setButtonText(isTrackB ? "TRACK B" : "TRACK A");

    audioProcessor.setActiveEditPart(isTrackB ? 1 : 0);
    keyboardComponent.setMidiChannel(isTrackB ? 2 : 1);

    auto& track = audioProcessor.sequencer.getActiveTrack();
    seqTrackMuteButton.setToggleState(track.isMute(), juce::dontSendNotification);
    seqTrackSoloButton.setToggleState(track.isSolo(), juce::dontSendNotification);

    if (!audioProcessor.isSeqGateLinked()) {
        if (auto* pGate = audioProcessor.apvts.getParameter("seqGate"))
            pGate->setValueNotifyingHost(pGate->convertTo0to1(track.getGateLength()));
        if (auto* pSwing = audioProcessor.apvts.getParameter("seqSwing"))
            pSwing->setValueNotifyingHost(pSwing->convertTo0to1(track.getSwing()));
    }

    refreshMatrixUI();
    updateSequencerLcd();
    refreshSelectedStepUI();
    if (audioProcessor.sequencer.isRecording)
        syncKnobsToStepLocksOrBase();
}

void Simple116AudioProcessorEditor::syncKnobsToStepLocksOrBase() noexcept {
    isUpdatingKnobsFromStep = true;
    const int activePart = audioProcessor.getActiveEditPart();
    const auto& track = audioProcessor.sequencer.getActiveTrack();
    const auto& step = track.getStep(selectedStepIndex);
    const auto& base = audioProcessor.getBasePatch(activePart);

    auto syncSlider = [&](juce::Slider& s, LockParamID id, float baseVal) {
        if (audioProcessor.sequencer.isRecording && step.locks.isLocked(id))
            s.setValue(step.locks.values[static_cast<size_t>(id)], juce::dontSendNotification);
        else
            s.setValue(baseVal, juce::dontSendNotification);
    };

    syncSlider(oscMorphSlider, Lock_DCO1Morph, base.dco1Morph);
    syncSlider(oscPWMSlider, Lock_DCO1PWM, base.dco1PWM);
    syncSlider(wavefoldSlider, Lock_DCO1Fold, base.dco1Fold);
    syncSlider(supersawMixSlider, Lock_DCO1SawMix, base.dco1SawMix);
    supersawDetuneSlider.setValue(base.dco1SawDetune, juce::dontSendNotification);

    syncSlider(dco2MorphSlider, Lock_DCO2Morph, base.dco2Morph);
    syncSlider(dco2PWMSlider, Lock_DCO2PWM, base.dco2PWM);
    syncSlider(semiTuneSlider, Lock_DCO2Semi, static_cast<float>(base.dco2Semi));
    syncSlider(fineTuneSlider, Lock_DCO2Cents, base.dco2Cents);
    fmAmountSlider.setValue(base.xmodAmount, juce::dontSendNotification);
    dco2FoldSlider.setValue(base.dco2Fold, juce::dontSendNotification);

    syncSlider(dco2LevelSlider, Lock_DCO2Level, base.dco2Level);
    syncSlider(subOscLevelSlider, Lock_SubLevel, base.subLevel);
    syncSlider(noiseLevelSlider, Lock_NoiseLevel, base.noiseLevel);

    syncSlider(f1HPFSlider, Lock_HPFCutoff, base.hpfCutoff);
    syncSlider(f1CutoffSlider, Lock_LPFCutoff, base.lpfCutoff);
    syncSlider(f1ResonanceSlider, Lock_LPFRes, base.lpfRes);
    syncSlider(f1EnvDepthSlider, Lock_EnvMod, base.envMod);

    syncSlider(filterBalanceSlider, Lock_FilterBalance, base.filterBalance);
    syncSlider(f2CutoffSlider, Lock_F2Cutoff, base.f2Cutoff);
    syncSlider(f2ResonanceSlider, Lock_F2Res, base.f2Res);
    syncSlider(f2EnvDepthSlider, Lock_F2EnvDepth, base.f2EnvDepth);

    if (activeEnvIndex == 0) {
        syncSlider(envAttackSlider, Lock_FiltAttack, base.filtAttack);
        syncSlider(envDecaySlider, Lock_FiltDecay, base.filtDecay);
        syncSlider(envSustainSlider, Lock_FiltSustain, base.filtSustain);
        syncSlider(envReleaseSlider, Lock_FiltRelease, base.filtRelease);
    } else if (activeEnvIndex == 1) {
        syncSlider(envAttackSlider, Lock_AmpAttack, base.ampAttack);
        syncSlider(envDecaySlider, Lock_AmpDecay, base.ampDecay);
        syncSlider(envSustainSlider, Lock_AmpSustain, base.ampSustain);
        syncSlider(envReleaseSlider, Lock_AmpRelease, base.ampRelease);
    } else {
        envAttackSlider.setValue(base.auxAttack, juce::dontSendNotification);
        envDecaySlider.setValue(base.auxDecay, juce::dontSendNotification);
        envSustainSlider.setValue(base.auxSustain, juce::dontSendNotification);
        envReleaseSlider.setValue(base.auxRelease, juce::dontSendNotification);
    }

    syncSlider(auxToPitchSlider, Lock_AuxToPitch, base.auxToPitch);
    syncSlider(auxToFilter1Slider, Lock_AuxToFilter1, base.auxToFilter1);
    syncSlider(auxToFilter2Slider, Lock_AuxToFilter2, base.auxToFilter2);
    syncSlider(auxToWavefoldSlider, Lock_AuxToFold, base.auxToFold);
    syncSlider(auxToPWMSlider, Lock_AuxToPWM, base.auxToPWM);

    updateEnvelopeLcd();
    isUpdatingKnobsFromStep = false;
}

void Simple116AudioProcessorEditor::updateSequencerLcd() noexcept {
    const auto& track = audioProcessor.sequencer.getActiveTrack();
    const int page = audioProcessor.sequencer.getActivePage();
    const int pageOffset = page * 16;
    const auto trackId = audioProcessor.sequencer.getEditTrack();

    for (int i = 0; i < 16; ++i) {
        const int stepIdx = pageOffset + i;
        const auto& step = track.getStep(stepIdx);

        seqLcd.cacheStep(i,
                         step.active,
                         step.velocity,
                         step.gateLength,
                         formatMidiNoteName(step.note),
                         step.locks.hasAnyLock(),
                         step.ratchet);
    }

    seqLcd.setGlobalGate(track.getGateLength());

    const bool isPlaying = audioProcessor.sequencer.getIsPlaying() || audioProcessor.sequencer.arpEnabled;
    const int playhead = isPlaying ? track.getCurrentStep() : -1;
    const double bpmVal = static_cast<double>(audioProcessor.sequencer.getTempo());
    const bool syncState = audioProcessor.sequencer.isSyncToHost();

    seqLcd.updateDisplayState(playhead, selectedStepIndex, page, trackId, isPlaying, bpmVal, syncState);
}

void Simple116AudioProcessorEditor::refreshSelectedStepUI() noexcept {
    const auto& track = audioProcessor.sequencer.getActiveTrack();
    const auto& step = track.getStep(selectedStepIndex);
    const bool hasLock = step.locks.hasAnyLock();

    currentStepLabel.setText("STEP " + juce::String(selectedStepIndex + 1), juce::dontSendNotification);
    clearLocksButton.setButtonText("CLR S" + juce::String((selectedStepIndex % 16) + 1));

    isUpdatingKnobsFromStep = true;
    stepGateSlider.setValue(step.gateLength <= 0.0f ? 0.0 : static_cast<double>(step.gateLength), juce::dontSendNotification);
    isUpdatingKnobsFromStep = false;

    if (audioProcessor.sequencer.isRecording) {
        syncKnobsToStepLocksOrBase();
        if (hasLock) {
            lcdScreen.setParameterEdit("STEP " + juce::String(selectedStepIndex + 1) + " [P-LOCKED]",
                                       "TWEAK KNOB TO EDIT LOCK", 1.0f);
        } else {
            lcdScreen.setParameterEdit("STEP " + juce::String(selectedStepIndex + 1) + " [UNLOCKED]",
                                       "TWEAK KNOB TO ADD LOCK", 0.0f);
        }
    } else {
        if (hasLock) {
            lcdScreen.setParameterEdit("STEP " + juce::String(selectedStepIndex + 1) + " [LOCKED]",
                                       "ARM REC TO EDIT LOCKS", 0.8f);
        }
    }

    updateSequencerLcd();
}

void Simple116AudioProcessorEditor::auditionSelectedStep() noexcept {
    const int currentPart = audioProcessor.getActiveEditPart();

    if (auditionCountdown > 0) {
        audioProcessor.stopAudition(auditionPart);
        for (int& n : auditionChordNotes) n = -1;
    }

    const auto& track = audioProcessor.sequencer.getActiveTrack();
    const auto& step = track.getStep(selectedStepIndex);
    auditionPart = currentPart;
    auditionCountdown = 8;

    int count = 0;
    for (int cn : step.chordNotes) {
        if (cn >= 0 && count < 4) {
            auditionChordNotes[static_cast<size_t>(count++)] = cn;
        }
    }
    if (count == 0) {
        if (audioProcessor.getPartPatch(currentPart).chordEnable) {
            auto exp = audioProcessor.getChordNotes(step.note, audioProcessor.getPartPatch(currentPart).chordType);
            for (size_t ci = 0; ci < exp.size() && ci < 4; ++ci) {
                auditionChordNotes[ci] = exp[ci];
            }
        } else {
            auditionChordNotes[0] = step.note;
        }
    }

    audioProcessor.auditionStep(selectedStepIndex, currentPart);
    
    if (step.locks.hasAnyLock()) {
        lcdScreen.setParameterEdit("AUDITION (P-LOCKED)", "STEP " + juce::String(selectedStepIndex + 1), 1.0f);
    } else {
        lcdScreen.setParameterEdit("AUDITION STEP", "STEP " + juce::String(selectedStepIndex + 1), 0.5f);
    }
}

void Simple116AudioProcessorEditor::timerCallback() {
    auto& skin = SkinManager::getInstance();

    lcdScreen.advanceTimer();

    if (lcdScreen.isBootComplete()) {
        const auto currentName = audioProcessor.getCurrentPresetName();
        const bool isTrackB = (audioProcessor.sequencer.getEditTrack() == TrackID::TrackB);
        const int activeVoices = audioProcessor.voiceManager.getActiveVoiceCount();
        const juce::String subInfo = juce::String(isTrackB ? "TRK B [CH 2]" : "TRK A [CH 1]")
                                   + " | " + juce::String(activeVoices) + "V ACTIVE"
                                   + " | BPM: " + juce::String(audioProcessor.sequencer.getTempo(), 1);
        lcdScreen.setPresetInfo(currentName, subInfo);
    }

    updateEnvelopeLcd();

    int guiT = 0, ledT = 0;
    if (auto* pGui = audioProcessor.apvts.getRawParameterValue("guiTheme")) guiT = static_cast<int>(pGui->load());
    if (auto* pLed = audioProcessor.apvts.getRawParameterValue("ledTheme")) ledT = static_cast<int>(pLed->load());

    if (static_cast<int>(skin.getChassisTheme()) != guiT || static_cast<int>(skin.getLEDTheme()) != ledT) {
        skin.setChassisTheme(static_cast<ChassisTheme>(guiT));
        skin.setLEDTheme(static_cast<LEDTheme>(ledT));
        repaint();
    }

    // Keep oscilloscope theme synchronized with persistent settings
    if (static_cast<int>(oscilloscope.getTheme()) != static_cast<int>(skin.getCRTTheme())) {
        oscilloscope.setTheme(static_cast<OscilloscopeComponent::Theme>(skin.getCRTTheme()));
    }

    const int numActivePages = audioProcessor.sequencer.getNumPages();

    if (auditionCountdown > 0) {
        if (--auditionCountdown == 0) {
            audioProcessor.stopAudition(auditionPart);
            for (int& n : auditionChordNotes) n = -1;
        }
    }

    seqPage1Button.setAlpha(1.0f);
    seqPage2Button.setAlpha(numActivePages >= 2 ? 1.0f : 0.40f);
    seqPage3Button.setAlpha(numActivePages >= 3 ? 1.0f : 0.40f);
    seqPage4Button.setAlpha(numActivePages >= 4 ? 1.0f : 0.40f);

    updateSequencerLcd();

    if (auto* p = audioProcessor.apvts.getRawParameterValue("chorusMode")) {
        const int mode = static_cast<int>(p->load());
        chorus1Button.setToggleState((mode == 1 || mode == 3), juce::dontSendNotification);
        chorus2Button.setToggleState((mode == 2 || mode == 3), juce::dontSendNotification);
    }
}

void Simple116AudioProcessorEditor::attachSlider(juce::Slider& slider, const juce::String& paramId) {
    if (audioProcessor.apvts.getParameter(paramId) != nullptr)
    {
        sliderAttachments.push_back(std::make_unique<SliderAttachment>(audioProcessor.apvts, paramId, slider));

        if (paramId.containsIgnoreCase("cutoff") || paramId.containsIgnoreCase("hpf"))
        {
            slider.textFromValueFunction = formatFrequency;
            slider.valueFromTextFunction = parseFrequency;
        }
        else if (paramId.containsIgnoreCase("attack") || paramId.containsIgnoreCase("decay") ||
                 paramId.containsIgnoreCase("release") || paramId.containsIgnoreCase("glide"))
        {
            slider.textFromValueFunction = formatEnvTime;
            slider.valueFromTextFunction = parseEnvTime;
        }
        else if (paramId.containsIgnoreCase("internalBpm"))
        {
            slider.textFromValueFunction = [](double val) { return juce::String(val, 1) + " BPM"; };
        }
        else if (paramId.containsIgnoreCase("sustain") || paramId.containsIgnoreCase("mix") ||
                 paramId.containsIgnoreCase("depth") || paramId.containsIgnoreCase("level") ||
                 paramId.containsIgnoreCase("pwm") || paramId.containsIgnoreCase("morph") ||
                 paramId.containsIgnoreCase("fold") || paramId.containsIgnoreCase("reso") ||
                 paramId.containsIgnoreCase("res") || paramId.containsIgnoreCase("swing") ||
                 paramId.containsIgnoreCase("gate") || paramId.containsIgnoreCase("volume") ||
                 paramId.containsIgnoreCase("balance") || paramId.containsIgnoreCase("damp") ||
                 paramId.containsIgnoreCase("feedback") || paramId.containsIgnoreCase("size") ||
                 paramId.containsIgnoreCase("xmod") || paramId.containsIgnoreCase("detune"))
        {
            if (paramId.containsIgnoreCase("balance"))
            {
                slider.textFromValueFunction = [](double val) {
                    const int pct = juce::roundToInt(val * 100.0);
                    if (pct == 50) return juce::String("MID");
                    if (pct < 50)  return "F1 " + juce::String(100 - pct * 2) + "%";
                    return "F2 " + juce::String((pct - 50) * 2) + "%";
                };
            }
            else
            {
                slider.textFromValueFunction = formatPercent;
                slider.valueFromTextFunction = parsePercent;
            }
        }
        else if (paramId.containsIgnoreCase("semi") || paramId.containsIgnoreCase("pitch"))
        {
            slider.textFromValueFunction = [](double val) {
                const int st = juce::roundToInt(val);
                return (st > 0 ? "+" : "") + juce::String(st) + "st";
            };
        }
        else if (paramId.containsIgnoreCase("cents") || paramId.containsIgnoreCase("fine"))
        {
            slider.textFromValueFunction = [](double val) {
                const int c = juce::roundToInt(val);
                return (c > 0 ? "+" : "") + juce::String(c) + "c";
            };
        }
        else if (paramId.containsIgnoreCase("octave") || paramId.containsIgnoreCase("range"))
        {
            slider.textFromValueFunction = [](double val) {
                const int o = juce::roundToInt(val);
                if (o == 0) return juce::String("32'");
                if (o == 1) return juce::String("16'");
                if (o == 2) return juce::String("8'");
                return juce::String("4'");
            };
        }

        slider.updateText();
    }
}

void Simple116AudioProcessorEditor::attachComboBox(juce::ComboBox& box, const juce::String& paramId) {
    if (audioProcessor.apvts.getParameter(paramId) != nullptr)
        comboBoxAttachments.push_back(std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, paramId, box));
}

void Simple116AudioProcessorEditor::attachButton(juce::Button& button, const juce::String& paramId) {
    if (audioProcessor.apvts.getParameter(paramId) != nullptr)
        buttonAttachments.push_back(std::make_unique<ButtonAttachment>(audioProcessor.apvts, paramId, button));
}

void Simple116AudioProcessorEditor::setupRotaryKnob(juce::Slider& slider, juce::Label& label,
                                                   const juce::String& text, double minVal, double maxVal,
                                                   double defaultVal, const juce::String& suffix, bool isBipolar) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 15);
    slider.setRange(minVal, maxVal, (maxVal - minVal > 50.0) ? 1.0 : 0.01);
    slider.setValue(defaultVal, juce::dontSendNotification);
    slider.setDoubleClickReturnValue(true, defaultVal);
    if (suffix.isNotEmpty()) slider.setTextValueSuffix(suffix);
    if (isBipolar) slider.getProperties().set("bipolar", true);

    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff94a3b8));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxHighlightColourId, juce::Colours::transparentBlack);
    slider.setOpaque(false);

    slider.onValueChange = [this, &slider, text]() {
        if (isUpdatingKnobsFromStep) return;
        const float norm = static_cast<float>(slider.valueToProportionOfLength(slider.getValue()));
        lcdScreen.setParameterEdit(text, slider.getTextFromValue(slider.getValue()), norm);
    };

    configureFocus(slider);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setOpaque(false);
    configureFocus(label);
    addAndMakeVisible(label);
}

void Simple116AudioProcessorEditor::setupLinearFader(juce::Slider& slider, juce::Label& label,
                                                    const juce::String& text, double minVal, double maxVal, double defaultVal) {
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 36, 14);
    slider.setRange(minVal, maxVal, (maxVal - minVal > 20.0) ? 0.1 : 0.01);
    slider.setValue(defaultVal, juce::dontSendNotification);
    slider.setDoubleClickReturnValue(true, defaultVal);

    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff94a3b8));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxHighlightColourId, juce::Colours::transparentBlack);
    slider.setOpaque(false);

    slider.onValueChange = [this, &slider, text]() {
        if (isUpdatingKnobsFromStep) return;
        const float norm = static_cast<float>(slider.valueToProportionOfLength(slider.getValue()));
        lcdScreen.setParameterEdit(text, slider.getTextFromValue(slider.getValue()), norm);
    };

    configureFocus(slider);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setOpaque(false);
    configureFocus(label);
    addAndMakeVisible(label);
}

void Simple116AudioProcessorEditor::setupButton(juce::Button& button, const juce::String& text, bool isToggleable) {
    button.setButtonText(text);
    button.setClickingTogglesState(isToggleable);
    configureFocus(button);
    addAndMakeVisible(button);
}

void Simple116AudioProcessorEditor::setupComboBox(juce::ComboBox& box, juce::Label& label,
                                                 const juce::String& text, const juce::StringArray& items, int defaultId) {
    box.clear();
    box.addItemList(items, 1);
    box.setSelectedId(defaultId, juce::dontSendNotification);
    configureFocus(box);
    addAndMakeVisible(box);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setOpaque(false);
    configureFocus(label);
    addAndMakeVisible(label);
}

void Simple116AudioProcessorEditor::drawSectionPanel(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                                                     const juce::String& title, const juce::Colour& ledCol) {
    auto& skin = SkinManager::getInstance();

    g.setColour(skin.getSectionBackgroundColour());
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(skin.getBorderColour());
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    const auto headerRect = juce::Rectangle<float>(bounds.getX(), bounds.getY(), bounds.getWidth(), 22.0f);
    g.setColour(skin.getSectionHeaderColour());
    g.fillRoundedRectangle(headerRect, 4.0f);

    g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    g.setColour(skin.getTextColour());
    g.drawText(title, headerRect.reduced(8.0f, 0.0f), juce::Justification::centredLeft, false);

    g.setColour(ledCol.withAlpha(0.75f));
    g.drawHorizontalLine(juce::roundToInt(headerRect.getBottom()), bounds.getX(), bounds.getRight());
}

void Simple116AudioProcessorEditor::paint(juce::Graphics& g) {
    auto& skin = SkinManager::getInstance();
    const auto ledCol = skin.getLEDColour();
    const auto bounds = getLocalBounds().toFloat();

    if (skin.hasCustomBackground()) {
        g.drawImage(skin.getCustomBackground(), bounds);
    } else {
        g.setColour(skin.getChassisBackgroundColour());
        g.fillRect(bounds);

        juce::ColourGradient chassisGrad(
            juce::Colour(0x18ffffff), bounds.getCentreX(), 0.0f,
            juce::Colour(0x00000000), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(chassisGrad);
        g.fillRect(bounds);
    }

    g.setColour(juce::Colour(0xff090a0d));
    g.fillRect(0.0f, 0.0f, static_cast<float>(UI_WIDTH), 54.0f);
    g.setColour(skin.getBorderColour());
    g.drawHorizontalLine(54, 0.0f, static_cast<float>(UI_WIDTH));

    drawSectionPanel(g, juce::Rectangle<float>(10.0f, 60.0f, 530.0f, 290.0f), "OSCILLATOR & WAVEFORM ENGINE", ledCol);

    g.setColour(skin.getBorderColour().withAlpha(0.35f));
    g.drawHorizontalLine(162, 14.0f, 536.0f);
    g.drawHorizontalLine(248, 14.0f, 536.0f);

    g.setFont(juce::Font(juce::FontOptions(8.5f).withStyle("Bold")));
    g.setColour(skin.getTextColour().withAlpha(0.55f));
    g.drawText("DCO 1", 16, 84, 50, 10, juce::Justification::left);
    g.drawText("DCO 2 (WITH AUDIO-RATE FM & FOLD)", 16, 164, 220, 10, juce::Justification::left);
    g.drawText("MIXER / HARD SYNC / GLIDE", 16, 250, 160, 10, juce::Justification::left);

    drawSectionPanel(g, juce::Rectangle<float>(548.0f, 60.0f, 516.0f, 290.0f), "DUAL FILTER MATRIX (JUNO 24dB + SEM SVF 12dB)", ledCol);
    drawSectionPanel(g, juce::Rectangle<float>(1072.0f, 60.0f, 278.0f, 290.0f), "REAL-TIME VECTOR CRT OSCILLOSCOPE", ledCol);

    drawSectionPanel(g, juce::Rectangle<float>(10.0f, 356.0f, 480.0f, 236.0f), "TRIPLE ENVELOPE ENGINE (DYNAMIC VECTOR LCD)", ledCol);

    g.setColour(skin.getBorderColour().withAlpha(0.35f));
    g.drawVerticalLine(296, 382.0f, 582.0f);

    drawSectionPanel(g, juce::Rectangle<float>(498.0f, 356.0f, 386.0f, 236.0f), "4x LFOs & 16-SLOT MODULATION MATRIX", ledCol);
    drawSectionPanel(g, juce::Rectangle<float>(892.0f, 356.0f, 458.0f, 236.0f), "BI-TIMBRAL DUAL-TRACK 64-STEP SEQUENCER & ARP", ledCol);

    drawSectionPanel(g, juce::Rectangle<float>(10.0f, 598.0f, 1340.0f, 92.0f), "MASTER EFFECTS (STEREO CHORUS, DELAY & REVERB)", ledCol);

    g.setColour(skin.getBorderColour().withAlpha(0.35f));
    g.drawVerticalLine(246, 621.0f, 688.0f);
    g.drawVerticalLine(860, 621.0f, 688.0f);

    g.setFont(juce::Font(juce::FontOptions(8.5f).withStyle("Bold")));
    g.setColour(skin.getTextColour().withAlpha(0.55f));
    g.drawText("CHORUS & ROUTE", 20, 622, 110, 10, juce::Justification::left);
    g.drawText("STEREO DELAY", 256, 622, 100, 10, juce::Justification::left);
    g.drawText("STEREO REVERB", 870, 622, 100, 10, juce::Justification::left);

    g.setColour(juce::Colour(0xff0b0d10));
    g.fillRect(0.0f, 696.0f, static_cast<float>(UI_WIDTH), 124.0f);
    g.setColour(skin.getBorderColour());
    g.drawHorizontalLine(696, 0.0f, static_cast<float>(UI_WIDTH));
}

void Simple116AudioProcessorEditor::resized() {
    titleBadgeLabel.setBounds(12, 10, 100, 34);

    prevPresetButton.setBounds(116, 14, 26, 26);
    presetComboBox.setBounds(144, 14, 154, 26);
    nextPresetButton.setBounds(300, 14, 26, 26);
    savePresetButton.setBounds(328, 14, 46, 26);
    initPresetButton.setBounds(376, 14, 40, 26);
    cardButton.setBounds(418, 14, 56, 26);

    voiceModeComboBox.setBounds(478, 14, 134, 26);
    masterVolumeSlider.setBounds(618, 6, 60, 44);

    chassisThemeButton.setBounds(684, 14, 78, 26);
    ledThemeButton.setBounds(764, 14, 84, 26);

    lcdScreen.setBounds(854, 8, 494, 38);

    // Oscillators Layout
    octaveSlider.setBounds(30, 92, 54, 50);
    octaveLabel.setBounds(30, 142, 54, 14);

    oscMorphSlider.setBounds(118, 92, 54, 50);
    oscMorphLabel.setBounds(118, 142, 54, 14);

    oscPWMSlider.setBounds(206, 92, 54, 50);
    oscPWMLabel.setBounds(206, 142, 54, 14);

    supersawMixSlider.setBounds(294, 92, 54, 50);
    supersawMixLabel.setBounds(294, 142, 54, 14);

    supersawDetuneSlider.setBounds(382, 92, 54, 50);
    supersawDetuneLabel.setBounds(382, 142, 54, 14);

    wavefoldSlider.setBounds(470, 92, 54, 50);
    wavefoldLabel.setBounds(470, 142, 54, 14);

    dco2MorphSlider.setBounds(30, 172, 54, 50);
    dco2MorphLabel.setBounds(30, 222, 54, 14);

    dco2PWMSlider.setBounds(118, 172, 54, 50);
    dco2PWMLabel.setBounds(118, 222, 54, 14);

    semiTuneSlider.setBounds(206, 172, 54, 50);
    semiTuneLabel.setBounds(206, 222, 54, 14);

    fineTuneSlider.setBounds(294, 172, 54, 50);
    fineTuneLabel.setBounds(294, 222, 54, 14);

    fmAmountSlider.setBounds(382, 172, 54, 50);
    fmAmountLabel.setBounds(382, 222, 54, 14);

    dco2FoldSlider.setBounds(470, 172, 54, 50);
    dco2FoldLabel.setBounds(470, 222, 54, 14);

    dco2LevelSlider.setBounds(20, 256, 52, 50);
    dco2LevelLabel.setBounds(20, 306, 52, 14);

    subOscLevelSlider.setBounds(76, 256, 52, 50);
    subOscLevelLabel.setBounds(76, 306, 52, 14);

    subOscOctaveButton.setBounds(132, 269, 58, 24);

    noiseLevelSlider.setBounds(194, 256, 52, 50);
    noiseLevelLabel.setBounds(194, 306, 52, 14);

    supersawModeButton.setBounds(250, 269, 68, 24);

    glideTimeSlider.setBounds(322, 256, 52, 50);
    glideTimeLabel.setBounds(322, 306, 52, 14);

    glideEnableButton.setBounds(378, 269, 66, 24);
    legatoModeButton.setBounds(448, 269, 66, 24);

    // Filters Layout
    filterRoutingButton.setBounds(562, 92, 86, 24);
    filterBalanceSlider.setBounds(656, 84, 58, 48);
    filterBalanceLabel.setBounds(656, 132, 58, 14);

    f1CutoffSlider.setBounds(562, 154, 58, 56);
    f1CutoffLabel.setBounds(562, 210, 58, 14);

    f1ResonanceSlider.setBounds(628, 154, 56, 56);
    f1ResonanceLabel.setBounds(628, 210, 56, 14);

    f1HPFSlider.setBounds(692, 154, 56, 56);
    f1HPFLabel.setBounds(692, 210, 56, 14);

    f1EnvDepthSlider.setBounds(562, 230, 56, 48);
    f1EnvDepthLabel.setBounds(562, 278, 56, 14);

    f1KeyTrackSlider.setBounds(624, 230, 58, 48);
    f1KeyTrackLabel.setBounds(624, 278, 58, 14);

    f1LfoDepthSlider.setBounds(686, 230, 58, 48);
    f1LfoDepthLabel.setBounds(686, 278, 58, 14);

    f2CutoffSlider.setBounds(766, 154, 58, 56);
    f2CutoffLabel.setBounds(766, 210, 58, 14);

    f2ResonanceSlider.setBounds(832, 154, 56, 56);
    f2ResonanceLabel.setBounds(832, 210, 56, 14);

    f2ModeComboBox.setBounds(900, 168, 90, 24);
    f2ModeLabel.setBounds(900, 194, 90, 14);

    f2EnvDepthSlider.setBounds(766, 230, 58, 48);
    f2EnvDepthLabel.setBounds(766, 278, 58, 14);

    f2KeyTrackSlider.setBounds(828, 230, 58, 48);
    f2KeyTrackLabel.setBounds(828, 278, 58, 14);

    oscilloscope.setBounds(1082, 88, 258, 252);

    // Vector ADSR Layout
    envPrevButton.setBounds(18, 384, 22, 22);
    envVcfTabButton.setBounds(44, 384, 60, 22);
    envVcaTabButton.setBounds(108, 384, 60, 22);
    envAuxTabButton.setBounds(172, 384, 60, 22);
    envNextButton.setBounds(236, 384, 22, 22);

    envLcd.setBounds(18, 412, 268, 86);

    envAttackSlider.setBounds(18, 506, 56, 50);
    envAttackLabel.setBounds(18, 556, 56, 14);

    envDecaySlider.setBounds(88, 506, 56, 50);
    envDecayLabel.setBounds(88, 556, 56, 14);

    envSustainSlider.setBounds(158, 506, 56, 50);
    envSustainLabel.setBounds(158, 556, 56, 14);

    envReleaseSlider.setBounds(228, 506, 56, 50);
    envReleaseLabel.setBounds(228, 556, 56, 14);

    auxModSectionLabel.setBounds(302, 384, 180, 20);

    auxToPitchSlider.setBounds(318, 410, 56, 44);
    auxToPitchLabel.setBounds(318, 452, 56, 14);

    auxToFilter1Slider.setBounds(398, 410, 56, 44);
    auxToFilter1Label.setBounds(398, 452, 56, 14);

    auxToFilter2Slider.setBounds(318, 468, 56, 44);
    auxToFilter2Label.setBounds(318, 510, 56, 14);

    auxToWavefoldSlider.setBounds(398, 468, 56, 44);
    auxToWavefoldLabel.setBounds(398, 510, 56, 14);

    auxToPWMSlider.setBounds(358, 526, 56, 44);
    auxToPWMLabel.setBounds(358, 568, 56, 14);

    // LFOs & Mod Matrix Layout
    lfoSelectComboBox.setBounds(512, 390, 84, 22);
    lfoShapeComboBox.setBounds(604, 390, 88, 22);
    lfoRateSlider.setBounds(700, 386, 52, 46);
    lfoRateLabel.setBounds(700, 432, 52, 14);
    lfoDepthSlider.setBounds(754, 386, 52, 46);
    lfoDepthLabel.setBounds(754, 432, 52, 14);

    modSlotSelectComboBox.setBounds(512, 458, 84, 22);
    modSlotEnableButton.setBounds(604, 458, 68, 22);

    modSourceLabel.setBounds(512, 482, 110, 14);
    modSourceComboBox.setBounds(512, 496, 110, 22);

    modDestLabel.setBounds(630, 482, 130, 14);
    modDestComboBox.setBounds(630, 496, 130, 22);

    modAmountSlider.setBounds(770, 486, 54, 44);
    modAmountLabel.setBounds(770, 530, 54, 14);

    modViaSourceLabel.setBounds(512, 524, 120, 14);
    modViaSourceComboBox.setBounds(512, 538, 120, 22);

    modViaAmountSlider.setBounds(642, 528, 54, 44);
    modViaAmountLabel.setBounds(642, 572, 54, 14);

    // =========================================================================
    // Redesigned Vector Sequencer Panel Layout (458 x 236 px at x=892, y=356)
    // =========================================================================
    // Row 1: Header / Track / Page / Transport (y = 384, h = 22)
    seqTrackLayerButton.setBounds(902, 384, 68, 22);
    seqTrackMuteButton.setBounds(972, 384, 34, 22);
    seqTrackSoloButton.setBounds(1008, 384, 34, 22);

    seqPage1Button.setBounds(1048, 384, 20, 22);
    seqPage2Button.setBounds(1070, 384, 20, 22);
    seqPage3Button.setBounds(1092, 384, 20, 22);
    seqPage4Button.setBounds(1114, 384, 20, 22);

    seqPlayButton.setBounds(1142, 384, 44, 22);
    seqRecButton.setBounds(1188, 384, 40, 22);
    arpEnableButton.setBounds(1230, 384, 52, 22);
    chordEnableButton.setBounds(1284, 384, 56, 22);

    // Row 2: Vector Sequencer LCD (y = 410, h = 96, w = 438)
    seqLcd.setBounds(902, 410, 438, 96);

    // Row 3: QWERTY Focus & Step Edit Strip (y = 510, h = 22)
    prevStepButton.setBounds(902, 510, 22, 22);
    currentStepLabel.setBounds(926, 510, 54, 22);
    nextStepButton.setBounds(982, 510, 22, 22);

    auditionStepButton.setBounds(1008, 510, 56, 22);
    clearLocksButton.setBounds(1068, 510, 52, 22);

    stepGateLabel.setBounds(1124, 510, 56, 20);
    stepGateSlider.setBounds(1182, 502, 42, 38);

    chordTypeComboBox.setBounds(1238, 510, 102, 22);

    // Row 4: Unified Hardware Controls Deck (y = 536 to 588)
    seqTempoSlider.setBounds(902, 536, 44, 42);
    seqTempoLabel.setBounds(902, 576, 44, 12);

    seqSyncButton.setBounds(950, 546, 38, 22);

    seqSwingSlider.setBounds(992, 536, 44, 42);
    seqSwingLabel.setBounds(992, 576, 44, 12);

    seqGateLengthSlider.setBounds(1040, 536, 44, 42);
    seqGateLengthLabel.setBounds(1040, 576, 44, 12);

    seqDivisionComboBox.setBounds(1088, 546, 58, 22);
    seqDivisionLabel.setBounds(1088, 572, 58, 12);

    seqDirectionComboBox.setBounds(1150, 546, 62, 22);
    seqDirectionLabel.setBounds(1150, 572, 62, 12);

    seqLengthComboBox.setBounds(1216, 546, 60, 22);
    seqLengthLabel.setBounds(1216, 572, 60, 12);

    seqModeComboBox.setBounds(1280, 546, 60, 22);
    seqModeLabel.setBounds(1280, 572, 60, 12);

    // Master FX Layout
    chorus1Button.setBounds(24, 638, 56, 26);
    chorus2Button.setBounds(84, 638, 56, 26);

    fxRoutingComboBox.setBounds(148, 638, 92, 24);
    fxRoutingLabel.setBounds(148, 620, 92, 14);

    delaySyncButton.setBounds(262, 638, 58, 26);
    delayPingPongButton.setBounds(328, 638, 86, 26);

    delayTimeSlider.setBounds(434, 624, 60, 46);
    delayTimeLabel.setBounds(434, 672, 60, 14);

    delayFeedbackSlider.setBounds(542, 624, 60, 46);
    delayFeedbackLabel.setBounds(542, 672, 60, 14);

    delayDampSlider.setBounds(650, 624, 60, 46);
    delayDampLabel.setBounds(650, 672, 60, 14);

    delayMixSlider.setBounds(758, 624, 60, 46);
    delayMixLabel.setBounds(758, 672, 60, 14);

    reverbSizeSlider.setBounds(920, 624, 60, 46);
    reverbSizeLabel.setBounds(920, 672, 60, 14);

    reverbDampSlider.setBounds(1050, 624, 60, 46);
    reverbDampLabel.setBounds(1050, 672, 60, 14);

    reverbMixSlider.setBounds(1180, 624, 60, 46);
    reverbMixLabel.setBounds(1180, 672, 60, 14);

    // Keybed & Wheels Layout (Vertical Linear Wheels with Double-Click Reset)
    pitchWheelSlider.setBounds(14, 704, 38, 96);
    pitchWheelLabel.setBounds(14, 800, 38, 14);

    modWheelSlider.setBounds(58, 704, 38, 96);
    modWheelLabel.setBounds(58, 800, 38, 14);

    keyboardComponent.setBounds(108, 702, 1240, 112);
    keyboardComponent.setAvailableRange(36, 96);
    keyboardComponent.setLowestVisibleKey(36);
    keyboardComponent.setKeyWidth(1240.0f / 36.0f);
    keyboardComponent.setScrollButtonsVisible(false);
}