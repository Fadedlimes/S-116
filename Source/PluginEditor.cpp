#include "PluginEditor.h"
#include <cstdlib>

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
    S116::skin().addChangeListener(this);
    configureFocus(*this);

#if S116_HAS_FONTS
    S116::skin().setTypefaces (
        juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize));
    jassert (S116::skin().theme().name.isNotEmpty());
#else
    DBG ("S116: Font files not found in Source/Resources/Fonts/ - using system fonts");
#endif

    // The S-116 design canvas is 1920 x 1500. Everything is parented to content.
    content.setBounds(0, 0, 1920, 1500);
    content.setWantsKeyboardFocus(false);
    content.setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(content);

    // Add chassis background and panels as first children (behind everything)
    content.addAndMakeVisible(chassisBg);
    chassisBg.setBounds(0, 0, 1920, 1500);
    content.addAndMakeVisible(headerBar);
    headerBar.setBounds(36, 14, 1848, 72);
    content.addAndMakeVisible(oscPanel);
    oscPanel.setBounds(36, 100, 830, 592);
    content.addAndMakeVisible(filtPanel);
    filtPanel.setBounds(878, 100, 560, 592);
    content.addAndMakeVisible(scopePanel);
    scopePanel.setBounds(1450, 100, 434, 592);
    content.addAndMakeVisible(envPanel);
    envPanel.setBounds(36, 706, 700, 570);
    content.addAndMakeVisible(lfoPanel);
    lfoPanel.setBounds(748, 706, 480, 570);
    content.addAndMakeVisible(seqPanel);
    seqPanel.setBounds(1240, 706, 644, 570);
    content.addAndMakeVisible(fxPanel);
    fxPanel.setBounds(36, 1290, 1848, 190);

    // ==========================================
    // 1. Header Bar Controls
    // ==========================================
    titleBadgeLabel.setText("S-116", juce::dontSendNotification);
    titleBadgeLabel.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
    titleBadgeLabel.setJustificationType(juce::Justification::centred);
    configureFocus(titleBadgeLabel);
    content.addAndMakeVisible(titleBadgeLabel);

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
        S116::skin().refreshLed();
        if (auto* p = audioProcessor.apvts.getParameter("guiTheme"))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(skin.getChassisTheme())));
        repaint();
    };

    setupButton(ledThemeButton, "LED COLOR", false);
    ledThemeButton.onClick = [this]() {
        auto& skin = SkinManager::getInstance();
        skin.cycleLEDTheme();
        S116::skin().refreshLed();
        if (auto* p = audioProcessor.apvts.getParameter("ledTheme"))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(skin.getLEDTheme())));
        repaint();
    };

    // The scope colour is a first class parameter so that it travels with the
    // VST session instead of only living in the global settings file.
    setupButton(crtThemeButton, "SCOPE COLOR", false);
    crtThemeButton.onClick = [this]() {
        auto& skin = SkinManager::getInstance();
        skin.cycleCRTTheme();
        if (auto* p = audioProcessor.apvts.getParameter("crtTheme"))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(skin.getCRTTheme())));
        oscilloscope.setTheme (static_cast<OscilloscopeComponent::Theme>(skin.getCRTTheme()));
        scopeFrame.setPhosphor (oscilloscope.getThemeColor());
        scopeFrame.setLabels ("S-116 CRT // " + skin.getCRTThemeName(), "ZOOM 1.00x");

        const auto scopeText = juce::String ("SCOPE: ") + skin.getCRTThemeName();
        if (crtThemeButton.getButtonText() != scopeText)
            crtThemeButton.setButtonText (scopeText);

        repaint();
    };

    configureFocus(lcdScreen);
    content.addAndMakeVisible(lcdScreen);
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
    oscMorphSlider.getProperties().set ("morph", true);
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
    dco2MorphSlider.getProperties().set ("morph", true);
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
    octaveSlider.getProperties().set ("halo", 34);
    oscMorphSlider.getProperties().set ("halo", 34);
    oscPWMSlider.getProperties().set ("halo", 34);
    supersawMixSlider.getProperties().set ("halo", 34);
    supersawDetuneSlider.getProperties().set ("halo", 34);
    wavefoldSlider.getProperties().set ("halo", 34);
    dco2MorphSlider.getProperties().set ("halo", 34);
    dco2PWMSlider.getProperties().set ("halo", 34);
    semiTuneSlider.getProperties().set ("halo", 34);
    fineTuneSlider.getProperties().set ("halo", 34);
    fmAmountSlider.getProperties().set ("halo", 34);
    dco2FoldSlider.getProperties().set ("halo", 34);;
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

    // Sub oscillator waveform: Sine (default) or the classic Roland square.
    setupButton(subOscWaveButton, "SUB WAVE", true);
    subOscWaveButton.setClickingTogglesState(true);
    subOscWaveButton.onClick = [this]() {
        const bool sine = subOscWaveButton.getToggleState();

        if (auto* p = audioProcessor.apvts.getParameter("subWaveform"))
            p->setValueNotifyingHost(p->convertTo0to1(sine ? 1.0f : 0.0f));

        updateSubWaveButton();
        lcdScreen.setParameterEdit("SUB WAVE", sine ? "SINE" : "SQUARE", sine ? 1.0f : 0.0f);
    };
    updateSubWaveButton();

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

    setupRotaryKnob(f1CutoffSlider, f1CutoffLabel, "F1 CUTOFF", 20.0, 20000.0, 3000.0);
    f1CutoffSlider.setSkewFactorFromMidPoint(1000.0);
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
    content.addAndMakeVisible(oscilloscope);

    oscilloscope.onThemeChanged = [this](OscilloscopeComponent::Theme newTheme) {
        SkinManager::getInstance().setCRTTheme(static_cast<CRTTheme>(newTheme));
        if (auto* p = audioProcessor.apvts.getParameter("crtTheme"))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(newTheme)));
        lcdScreen.setParameterEdit("CRT THEME", oscilloscope.getThemeName(), 1.0f);
    };

    // ==========================================
    // 6. Dynamic Vector ADSR LCD & Unified 4-Knob Deck
    // ==========================================
    configureFocus(envLcd);
    content.addAndMakeVisible(envLcd);

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
    content.addAndMakeVisible(auxModSectionLabel);

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

    content.addAndMakeVisible(seqLcd);

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

        if (stepIdx == selectedStepIndex)
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
    content.addAndMakeVisible(currentStepLabel);

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
    content.addAndMakeVisible(keyboardComponent);

    // ==========================================
    // 12. Deep White-Box Eradication Sweep
    // ==========================================
    for (auto* comp : content.getChildren())
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

    setResizable (true, true);
    setResizeLimits (960, 750, 1920, 1500);
    getConstrainer()->setFixedAspectRatio (1920.0 / 1500.0);
    setSize (1440, 1125);

    if (std::getenv ("S116_SELFTEST") != nullptr)
        juce::Timer::callAfterDelay (1500, [safe = juce::Component::SafePointer<Simple116AudioProcessorEditor> (this)]
                                     { if (safe != nullptr) safe->runSelfTest(); });

    DBG ("CONSTRUCTOR DONE");
    scopeFrame.setPhosphor (oscilloscope.getThemeColor());
    scopeFrame.setLabels ("S-116 CRT // " + SkinManager::getInstance().getCRTThemeName(), "ZOOM 1.00x");

#if S116_SNAPSHOTS
    // Snapshot generation (debug mode)
    juce::File snapDir (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("s116-snaps"));
    snapDir.createDirectory();
    setSize (1920, 1500);
    for (int ti = 0; ti < 4; ++ti)
    {
        SkinManager::getInstance().setChassisTheme (static_cast<ChassisTheme> (ti));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        juce::FileOutputStream os (snapDir.getChildFile ("chassis_" + juce::String (ti) + "_" + S116::skin().theme().name + "_full.png"));
        if (os.openedOk()) png.writeImageToStream (img, os);
    }
    // Silver button crop (enlarged)
    {
        SkinManager::getInstance().setChassisTheme (ChassisTheme::VintageCream);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        // find a button to crop: use chassisThemeButton
        auto bnds = chassisThemeButton.getBounds();
        auto img = chassisThemeButton.createComponentSnapshot (chassisThemeButton.getLocalBounds(), true, 4.0f);
        juce::PNGImageFormat png;
        juce::FileOutputStream os (snapDir.getChildFile ("silver_button_crop.png"));
        if (os.openedOk()) png.writeImageToStream (img, os);
    }
    // Graphite with two different LED colours
    {
        SkinManager::getInstance().setChassisTheme (ChassisTheme::DarkGraphite);
        for (int li : { 0, 2 }) // Cyan, Amber
        {
            SkinManager::getInstance().setLEDTheme (static_cast<LEDTheme> (li));
            S116::skin().refreshLed();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            juce::FileOutputStream os (snapDir.getChildFile ("graphite_led" + juce::String (li) + "_" + SkinManager::getInstance().getLEDThemeName (static_cast<LEDTheme> (li)) + "_full.png"));
            if (os.openedOk()) png.writeImageToStream (img, os);
        }
    }
    DBG ("Snapshots written to " + snapDir.getFullPathName());
#endif
    S116::skin().refreshLed();
    startTimerHz(30);

    refreshPresetList();
    updateTrackStatusUI();
    updateSequencerLcd();
}

Simple116AudioProcessorEditor::~Simple116AudioProcessorEditor() {
    stopTimer();

    // Leaving the editor must never leave the sequencer running: a reopened
    // console has to come up with the transport stopped.
    if (auto* pPlay = audioProcessor.apvts.getParameter("seqPlay"))
        pPlay->setValueNotifyingHost(0.0f);
    if (auto* pRec = audioProcessor.apvts.getParameter("seqRec"))
        pRec->setValueNotifyingHost(0.0f);

    audioProcessor.sequencer.isPlaying = false;
    audioProcessor.sequencer.isRecording = false;

    S116::skin().removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void Simple116AudioProcessorEditor::applyFitScale() {
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* primary = displays.getPrimaryDisplay();

    float scale = 1.0f;

    if (primary != nullptr)
    {
        // Leave room for the window frame / title bar of the host window.
        constexpr int marginW = 48;
        constexpr int marginH = 96;

        const auto usable = primary->userArea.withTrimmedRight (marginW).withTrimmedBottom (marginH);

        if (usable.getWidth()  > 0) scale = std::min (scale, static_cast<float>(usable.getWidth())  / static_cast<float>(UI_WIDTH));
        if (usable.getHeight() > 0) scale = std::min (scale, static_cast<float>(usable.getHeight()) / static_cast<float>(UI_HEIGHT));
    }

    scale = juce::jlimit (0.35f, 1.0f, scale);

    if (approximatelyEqual (scale, uiScale))
        return;

    uiScale = scale;

    // The canvas keeps its logical 1920x1500 size; the transform does the scaling.
    content.setTransform (juce::AffineTransform::scale (uiScale));
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

    DBG ("ATTACH ENV: " + attId);
    if (audioProcessor.apvts.getParameter(attId) != nullptr)
        envAttackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, attId, envAttackSlider);
    DBG ("ATTACH ENV: " + decId);
    if (audioProcessor.apvts.getParameter(decId) != nullptr)
        envDecayAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, decId, envDecaySlider);
    DBG ("ATTACH ENV: " + susId);
    if (audioProcessor.apvts.getParameter(susId) != nullptr)
        envSustainAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, susId, envSustainSlider);
    DBG ("ATTACH ENV: " + relId);
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

void Simple116AudioProcessorEditor::updateSubWaveButton() noexcept {
    int wave = 1; // 0 = Square, 1 = Sine

    if (auto* p = audioProcessor.apvts.getRawParameterValue("subWaveform"))
        wave = static_cast<int>(p->load());

    const bool sine = (wave != 0);

    subOscWaveButton.setToggleState(sine, juce::dontSendNotification);

    const auto text = sine ? juce::String ("SUB WAVE: SINE") : juce::String ("SUB WAVE: SQ");
    if (subOscWaveButton.getButtonText() != text)
        subOscWaveButton.setButtonText (text);
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

    int guiT = 0, ledT = 0, crtT = 0;
    if (auto* pGui = audioProcessor.apvts.getRawParameterValue("guiTheme")) guiT = static_cast<int>(pGui->load());
    if (auto* pLed = audioProcessor.apvts.getRawParameterValue("ledTheme")) ledT = static_cast<int>(pLed->load());
    if (auto* pCrt = audioProcessor.apvts.getRawParameterValue("crtTheme")) crtT = static_cast<int>(pCrt->load());

    if (static_cast<int>(skin.getChassisTheme()) != guiT || static_cast<int>(skin.getLEDTheme()) != ledT) {
        skin.setChassisTheme(static_cast<ChassisTheme>(guiT));
        skin.setLEDTheme(static_cast<LEDTheme>(ledT));
        S116::skin().refreshLed();
        repaint();
    }

    // Keep oscilloscope theme synchronized with the (session persistent) parameter
    if (static_cast<int>(oscilloscope.getTheme()) != crtT) {
        skin.setCRTTheme(static_cast<CRTTheme>(crtT));
        oscilloscope.setTheme(static_cast<OscilloscopeComponent::Theme>(crtT));
        scopeFrame.setPhosphor (oscilloscope.getThemeColor());
        scopeFrame.setLabels ("S-116 CRT // " + SkinManager::getInstance().getCRTThemeName(), "ZOOM 1.00x");
    }

    const auto scopeText = juce::String ("SCOPE: ") + skin.getCRTThemeName();
    if (crtThemeButton.getButtonText() != scopeText)
        crtThemeButton.setButtonText (scopeText);

    // The sub waveform belongs to the edited part, so it has to follow a
    // part switch as well.
    updateSubWaveButton();

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
    DBG ("ATTACH SLIDER: " + paramId);
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
            // The knob label underneath already says "BPM": repeating it here used
            // to overflow the 44px text box and JUCE elided it to "128.0...".
            slider.textFromValueFunction = [](double val) { return juce::String (val, 1); };
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
    DBG ("ATTACH COMBO: " + paramId);
    if (audioProcessor.apvts.getParameter(paramId) != nullptr)
        comboBoxAttachments.push_back(std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, paramId, box));
}

void Simple116AudioProcessorEditor::attachButton(juce::Button& button, const juce::String& paramId) {
    DBG ("ATTACH BUTTON: " + paramId);
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
    content.addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setOpaque(false);
    configureFocus(label);
    content.addAndMakeVisible(label);
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
    content.addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setOpaque(false);
    configureFocus(label);
    content.addAndMakeVisible(label);
}

void Simple116AudioProcessorEditor::setupButton(juce::Button& button, const juce::String& text, bool isToggleable) {
    button.setButtonText(text);
    button.setClickingTogglesState(isToggleable);
    configureFocus(button);
    content.addAndMakeVisible(button);
}

void Simple116AudioProcessorEditor::setupComboBox(juce::ComboBox& box, juce::Label& label,
                                                 const juce::String& text, const juce::StringArray& items, int defaultId) {
    box.clear();
    box.addItemList(items, 1);
    box.setSelectedId(defaultId, juce::dontSendNotification);
    configureFocus(box);
    content.addAndMakeVisible(box);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setOpaque(false);
    configureFocus(label);
    content.addAndMakeVisible(label);
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
    juce::ignoreUnused (g);
}

void Simple116AudioProcessorEditor::paintChassis(juce::Graphics& g) {
    auto& skin = SkinManager::getInstance();
    const auto ledCol = skin.getLEDColour();
    const auto bounds = content.getLocalBounds().toFloat();

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
    // The canvas keeps its logical 1920x1500 size; the transform scales it.
    content.setBounds (0, 0, 1920, 1500);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / 1920.0f));
    layoutS116();

}

static juce::uint64 snapHash (juce::Component& c)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), false, 1.0f);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
    juce::uint64 h = 1469598103934665603ULL;
    for (int y = 0; y < bd.height; ++y)
    { auto* pp = bd.getLinePointer (y); for (int i = 0; i < bd.width * bd.pixelStride; ++i) h = (h ^ pp[i]) * 1099511628211ULL; }
    return h;
}

void Simple116AudioProcessorEditor::runSelfTest()
{
    auto pump = [] (int ms) { juce::Timer::callAfterDelay (ms, [] { juce::MessageManager::getInstance()->stopDispatchLoop(); }); juce::MessageManager::getInstance()->runDispatchLoop(); };
    auto state = [this]() -> juce::String
    {
        auto& seq = audioProcessor.sequencer;
        const int sel = seq.getSelectedStep();
        const auto& step = seq.getActiveTrack().getStep (sel);
        return "selectedStep=" + juce::String (sel) +
               " note=" + juce::String (step.note) +
               " gate=" + juce::String (step.gateLength) +
               " velocity=" + juce::String (step.velocity) +
               " active=" + juce::String ((int) step.active) +
               " page=" + juce::String (seq.getActivePage()) +
               " track=" + juce::String ((int) seq.getEditTrack());
    };
    auto report = [&] (const juce::String& what, const juce::String& before, const juce::String& after, bool handled)
    { std::cerr << "SELFTEST " << what << " handled=" << (int) handled << " before=[" << before << "] after=[" << after << "]" << std::endl; };

    seqLcd.grabKeyboardFocus(); pump (100);
    std::cerr << "SELFTEST focus: seqLcd.hasKeyboardFocus=" << (int) seqLcd.hasKeyboardFocus (false)
              << " wantsFocus=" << (int) seqLcd.getWantsKeyboardFocus()
              << " focused=" << (juce::Component::getCurrentlyFocusedComponent() != nullptr ? juce::Component::getCurrentlyFocusedComponent()->getName() : "none")
              << " seqLcdVisible=" << (int) seqLcd.isShowing() << " bounds=" << seqLcd.getBoundsInParent().toString() << std::endl;

    struct K { const char* name; juce::KeyPress key; };
    for (auto k : { K { "right", juce::KeyPress (juce::KeyPress::rightKey) }, K { "left", juce::KeyPress (juce::KeyPress::leftKey) },
                    K { "up", juce::KeyPress (juce::KeyPress::upKey) },       K { "down", juce::KeyPress (juce::KeyPress::downKey) },
                    K { "space", juce::KeyPress (juce::KeyPress::spaceKey) },
                    K { "d", juce::KeyPress ('d') }, K { "a", juce::KeyPress ('a') }, K { "w", juce::KeyPress ('w') }, K { "s", juce::KeyPress ('s') } })
    {
        const auto b = state();
        auto hb = snapHash (seqLcd);
        const bool h = seqLcd.keyPressed (k.key);
        pump (60);
        auto ha = snapHash (seqLcd);
        report (k.name, b, state(), h);
        std::cerr << "SELFTEST " << k.name << " hashBefore=" << hb << " hashAfter=" << ha << " changed=" << (int) (hb != ha) << std::endl;
    }

    {
        const auto b = state();
        const juce::Point<float> p ((float) seqLcd.getWidth() * 0.30f, (float) seqLcd.getHeight() * 0.60f);
        juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys::leftButtonModifier,
                            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &seqLcd, &seqLcd, juce::Time::getCurrentTime(),
                            p, juce::Time::getCurrentTime(), 1, false);
        seqLcd.mouseDown (e); seqLcd.mouseUp (e); pump (60);
        report ("mouseClick", b, state(), true);
    }
    // PLAY animation test
    {
        seqPlayButton.triggerClick(); pump (100);
        juce::uint64 hashes[4];
        int steps[4];
        for (int i = 0; i < 4; ++i)
        {
            pump (250);
            hashes[i] = snapHash (seqLcd);
            steps[i] = audioProcessor.sequencer.isPlaying ? audioProcessor.sequencer.getActiveTrack().getCurrentStep() : -1;
            std::cerr << "SELFTEST play " << i << " hash=" << hashes[i] << " playingStep=" << steps[i] << std::endl;
        }
        seqPlayButton.triggerClick(); pump (100);
    }

    // Envelope table
    {
        auto envState = [this]() -> juce::String
        {
            return "A=" + juce::String (envAttackSlider.getValue()) +
                   " D=" + juce::String (envDecaySlider.getValue()) +
                   " S=" + juce::String (envSustainSlider.getValue()) +
                   " R=" + juce::String (envReleaseSlider.getValue()) +
                   " LCD_A=" + juce::String (envAttackSlider.valueToProportionOfLength (envAttackSlider.getValue())) +
                   " LCD_D=" + juce::String (envDecaySlider.valueToProportionOfLength (envDecaySlider.getValue())) +
                   " LCD_S=" + juce::String (envSustainSlider.valueToProportionOfLength (envSustainSlider.getValue())) +
                   " LCD_R=" + juce::String (envReleaseSlider.valueToProportionOfLength (envReleaseSlider.getValue()));
        };
        std::cerr << "SELFTEST env INIT: " << envState() << std::endl;
    }

    // Control bounds table for LFO/MOD and Sequencer
    {
        auto cb = [&] (const juce::String& name, juce::Component& c)
        { std::cerr << "BOUNDS " << name << " " << c.getBoundsInParent().toString() << " visible=" << (int) c.isShowing() << std::endl; };
        cb ("lfoSelectComboBox", lfoSelectComboBox);
        cb ("lfoShapeComboBox", lfoShapeComboBox);
        cb ("lfoRateSlider", lfoRateSlider);
        cb ("lfoDepthSlider", lfoDepthSlider);
        cb ("modSlotSelectComboBox", modSlotSelectComboBox);
        cb ("modSlotEnableButton", modSlotEnableButton);
        cb ("modSourceComboBox", modSourceComboBox);
        cb ("modDestComboBox", modDestComboBox);
        cb ("modViaSourceComboBox", modViaSourceComboBox);
        cb ("modAmountSlider", modAmountSlider);
        cb ("modViaAmountSlider", modViaAmountSlider);
        cb ("seqTrackLayerButton", seqTrackLayerButton);
        cb ("seqPlayButton", seqPlayButton);
        cb ("seqLcd", seqLcd);
        cb ("prevStepButton", prevStepButton);
        cb ("seqTempoSlider", seqTempoSlider);
        cb ("stepGateSlider", stepGateSlider);
        cb ("seqDivisionComboBox", seqDivisionComboBox);
    }

    // Fonts
    {
        juce::File f1 ("Source/Resources/Fonts/BarlowCondensed-Bold.ttf");
        juce::File f2 ("Source/Resources/Fonts/ShareTechMono-Regular.ttf");
        std::cerr << "SELFTEST fonts: Barlow=" << (int) f1.existsAsFile() << " ShareTech=" << (int) f2.existsAsFile() << " S116_HAS_FONTS=" 
#ifdef S116_HAS_FONTS
                  << 1
#else
                  << 0
#endif
                  << std::endl;
    }

    // Header controls
    {
        std::cerr << "SELFTEST header: lcdScreen=" << lcdScreen.getBounds().toString()
                  << " chassisThemeButton=" << chassisThemeButton.getBounds().toString()
                  << " ledThemeButton=" << ledThemeButton.getBounds().toString()
                  << " masterVolumeSlider=" << masterVolumeSlider.getBounds().toString() << std::endl;
    }

    // Header LCD test
    {
        masterVolumeSlider.setValue (0.5, juce::sendNotificationSync);
        std::cerr << "SELFTEST lcd after knob: " << lcdScreen.getCurrentText() << std::endl;
        voiceModeComboBox.setSelectedId (2, juce::sendNotificationSync);
        std::cerr << "SELFTEST lcd after combo: " << lcdScreen.getCurrentText() << std::endl;
        chassisThemeButton.triggerClick();
        std::cerr << "SELFTEST lcd after button: " << lcdScreen.getCurrentText() << std::endl;
    }

    // Mod matrix test
    {
        modSlotSelectComboBox.setSelectedId (4, juce::sendNotificationSync); // slot 3 (1-indexed)
        modSourceComboBox.setSelectedId (2, juce::sendNotificationSync);
        modDestComboBox.setSelectedId (3, juce::sendNotificationSync);
        modAmountSlider.setValue (0.5, juce::sendNotificationSync);
        modSlotEnableButton.setToggleState (true, juce::sendNotificationSync);
        pump (100);
        auto& slot = audioProcessor.modMatrix[1].getSlot (2);
        std::cerr << "SELFTEST modmatrix: source=" << (int) slot.source << " dest=" << (int) slot.destination << " amount=" << slot.amount << " enabled=" << (int) slot.enabled << std::endl;
        // Switch to slot 1 and back
        modSlotSelectComboBox.setSelectedId (2, juce::sendNotificationSync);
        pump (50);
        modSlotSelectComboBox.setSelectedId (4, juce::sendNotificationSync);
        pump (50);
        std::cerr << "SELFTEST modmatrix after switch: source=" << (int) modSourceComboBox.getSelectedId() << " dest=" << (int) modDestComboBox.getSelectedId() << " amount=" << modAmountSlider.getValue() << " enabled=" << (int) modSlotEnableButton.getToggleState() << std::endl;
    }

    // LFO select test
    {
        std::cerr << "SELFTEST lfo select before: lfoRateSlider paramID=" << lfoRateSlider.getProperties().getWithDefault ("paramID", juce::var ("-")).toString() << std::endl;
        lfoSelectComboBox.setSelectedId (2, juce::sendNotificationSync);
        pump (50);
        std::cerr << "SELFTEST lfo select after: lfoRateSlider paramID=" << lfoRateSlider.getProperties().getWithDefault ("paramID", juce::var ("-")).toString() << std::endl;
    }

    // WCAG contrast table for each chassis theme
    {
        auto relLum = [] (juce::Colour c) {
            auto lin = [] (float x) { return x <= 0.03928f ? x / 12.92f : std::pow ((x + 0.055f) / 1.055f, 2.4f); };
            return 0.2126f * lin (c.getFloatRed()) + 0.7152f * lin (c.getFloatGreen()) + 0.0722f * lin (c.getFloatBlue());
        };
        auto contrast = [&] (juce::Colour a, juce::Colour b) {
            float la = relLum (a), lb = relLum (b);
            float mx = std::max (la, lb), mn = std::min (la, lb);
            return (mx + 0.05f) / (mn + 0.05f);
        };
        auto derivedBtnText = [&] (const S116::Theme& t) {
            const bool lightFace = t.btnTop.interpolatedWith (t.btnBottom, 0.5f).getPerceivedBrightness() > 0.55f;
            return lightFace ? juce::Colour (0xff14171a) : t.btnText;
        };
        const char* themeNames[] = { "Graphite", "JP-8000 Blue", "Classic Silver", "SH-101 Red" };
        for (int ti = 0; ti < (int) S116::ThemeId::numThemes; ++ti)
        {
            S116::skin().setTheme (static_cast<S116::ThemeId> (ti));
            const auto& t = S116::skin().theme();
            auto row = [&] (const juce::String& item, juce::Colour txt, juce::Colour bg) {
                std::cerr << "CONTRAST " << themeNames[ti] << " | " << item.toStdString() << " | " << contrast (txt, bg) << std::endl;
            };
            const auto bt = derivedBtnText (t);
            row ("button text on btn face (top)",    bt, t.btnTop);
            row ("button text on btn face (bottom)", bt, t.btnBottom);
            row ("caption text on panel (top)",     t.caption, t.panelTop);
            row ("caption text on panel (bottom)",  t.caption, t.panelBottom);
            row ("field label on panel",            t.heading, t.panelTop);
            row ("sub-heading on panel",            t.sub, t.panelTop);
            row ("readout text on plate (top)",     t.ledReadout, t.plateTop);
            row ("readout text on plate (bottom)",  t.ledReadout, t.plateBottom);
            row ("combo text on sel background (top)",    t.selText, t.selTop);
            row ("combo text on sel background (bottom)", t.selText, t.selBottom);
            row ("section title on panel",          t.heading, t.panelTop);
            row ("header logo on header (top)",     t.logo, t.hdrTop);
            row ("header logo on header (bottom)",  t.logo, t.hdrBottom);
            row ("step numbers on lcd",             t.ledReadout.darker (0.3f), t.lcdBottom);
            row ("popup text on popup bg",          t.selText, juce::Colour (0xff0e0f12));
        }
    }
    // Scope colour test
    {
        auto& skin = SkinManager::getInstance();
        for (int i = 0; i < (int) CRTTheme::Count; ++i)
        {
            skin.setCRTTheme (static_cast<CRTTheme> (i));
            if (auto* p = audioProcessor.apvts.getParameter ("crtTheme"))
                p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (i)));
            oscilloscope.setTheme (static_cast<OscilloscopeComponent::Theme> (i));
            scopeFrame.setPhosphor (oscilloscope.getThemeColor());
            scopeFrame.setLabels ("S-116 CRT // " + skin.getCRTThemeName(), "ZOOM 1.00x");
            crtThemeButton.setButtonText ("SCOPE: " + skin.getCRTThemeName());
            std::cerr << "SELFTEST scope " << i << " | " << skin.getCRTThemeName()
                      << " | osc=" << oscilloscope.getThemeColor().toString()
                      << " | phosphor=" << scopeFrame.getPhosphor().toString()
                      << " | button=" << crtThemeButton.getButtonText()
                      << " | label=" << scopeFrame.getLeftLabel() << std::endl;
        }
    }

    // LED colour test (Graphite chassis: pointer follows LED)
    {
        S116::skin().setTheme (S116::ThemeId::GraphiteRed);
        auto& skin = SkinManager::getInstance();
        seqSyncButton.setToggleState (true, juce::dontSendNotification);
        for (int i = 0; i < (int) LEDTheme::Count; ++i)
        {
            skin.setLEDTheme (static_cast<LEDTheme> (i));
            if (auto* p = audioProcessor.apvts.getParameter ("ledTheme"))
                p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (i)));
            S116::skin().refreshLed();
            juce::Colour phosphorBefore = scopeFrame.getPhosphor();
            std::cerr << "SELFTEST led " << i << " | " << skin.getLEDThemeName (static_cast<LEDTheme> (i))
                      << " | base=" << skin.getLEDColour().toString()
                      << " | ledOn=" << S116::skin().theme().ledOn.toString()
                      << " | ledText=" << S116::skin().theme().ledText.toString()
                      << " | ledReadout=" << S116::skin().theme().ledReadout.toString()
                      << " | pointer=" << S116::skin().theme().pointer.toString()
                      << " | phosphor=" << phosphorBefore.toString()
                      << " (unchanged=" << (phosphorBefore == scopeFrame.getPhosphor() ? "YES" : "NO") << ")"
                      << " | lcdHash=" << snapHash (lcdScreen)
                      << " | knobHash=" << snapHash (masterVolumeSlider)
                      << " | btnHash=" << snapHash (seqSyncButton)
                      << std::endl;
        }
        // Silver and SH-101: pointer must NOT follow LED
        for (auto tid : { S116::ThemeId::ClassicSilver, S116::ThemeId::SH101Red })
        {
            S116::skin().setTheme (tid);
            skin.setLEDTheme (LEDTheme::Amber);
            S116::skin().refreshLed();
            std::cerr << "SELFTEST led pointer-nofollow " << S116::skin().theme().name.toStdString()
                      << " | pointer=" << S116::skin().theme().pointer.toString().toStdString() << std::endl;
        }
    }

    std::cerr << "SELFTEST done" << std::endl;
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

#include "S116EditorLayout.inc"
