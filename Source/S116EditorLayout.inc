// ============================================================================
//  S116EditorLayout.inc  (NEW FILE)
//  Include at the END of PluginEditor.cpp:   #include "S116EditorLayout.inc"
//  Defines Simple116AudioProcessorEditor::layoutS116(). Declare it in the class:
//        void layoutS116();
//        S116::layout::DecorSet decor;
//        bool zOrderDone = false;
//  Call it from resized() AFTER content.setBounds / content.setTransform, and
//  delete every old setBounds() call in resized() (keep non-bounds statements).
//  All coordinates are CONTENT (1920 x 1500) coordinates.
//  Only bounds / properties are touched: no handlers, attachments or timers.
// ============================================================================
#include "S116Layout.h"

void Simple116AudioProcessorEditor::layoutS116()
{
    using namespace S116;
    using namespace S116::layout;

    // ---------------------------------------------------------------- panels
    // FX needs 208 px (sub-heading + a 108 px knob cell + padding), so rows 1 and 2 give back 9 px each.
    chassisBg .setBounds (0, 0, 1920, 1500);
    headerBar .setBounds (  36,   14, 1848,  72);
    oscPanel  .setBounds (  36,  100,  830, 583);
    filtPanel .setBounds ( 878,  100,  560, 583);
    scopePanel.setBounds (1450,  100,  434, 583);
    envPanel  .setBounds (  36,  697,  700, 561);
    lfoPanel  .setBounds ( 748,  697,  480, 561);
    seqPanel  .setBounds (1240,  697,  644, 561);
    fxPanel   .setBounds (  36, 1272, 1848, 208);

    // padding: 46 top, 22 sides, 20 bottom
    auto area = [] (const juce::Component& p)
    {
        const auto b = p.getBounds();
        return juce::Rectangle<int> (b.getX() + 22, b.getY() + 46, b.getWidth() - 44, b.getHeight() - 66);
    };
    auto btn = [] (juce::Component& b, int x, int y, int faceW) { placeButton (b, x, y, faceW); };

    // ---------------------------------------------------------------- header
    {
        const int cy = 14 + 36;
        int x = 226;                                              // after the logo
        btn (prevPresetButton, x, cy - 17, 38);                   x += 38 + 8;
        presetComboBox.setBounds (x, cy - 16, 300, kSelectH);     x += 300 + 8;
        btn (nextPresetButton, x, cy - 17, 38);                   x += 38 + 14;
        btn (savePresetButton, x, cy - 17, 78);                   x += 78 + 8;
        btn (initPresetButton, x, cy - 17, 78);                   x += 78 + 8;
        btn (cardButton,       x, cy - 17, 78);                   x += 78 + 6;
        voiceModeComboBox.setBounds (x, cy - 16, 190, kSelectH);  x += 190 + 14;

        auto& mp = masterVolumeSlider.getProperties();            // knob with the readout to its RIGHT
        mp.set ("body", 42); mp.set ("halo", (int) kTickMargin); mp.set ("readoutRight", true);
        mp.set ("readoutW", 54); mp.set ("readoutH", 20);
        masterVolumeSlider.setBounds (x, 14, 112, 72);

        int rx = 36 + 1848 - 24;
        lcdScreen.setBounds (rx - 260, cy - 23, 260, 46);         rx -= 260 + 14;
        btn (ledThemeButton,     rx - 120, cy - 17, 120);         rx -= 120 + 8;
        btn (chassisThemeButton, rx - 120, cy - 17, 120);
    }

    // ----------------------------------------------------------- oscillators
    {
        const auto a = area (oscPanel);                           // 786 wide
        const int colW = a.getWidth() / 6;
        const int sub = 19 + 6;
        const int hGlyph = knobCellHeight (52, true);             // 145
        const int hMix   = juce::jmax (knobCellHeight (52, false), 130);
        const auto ys = distribute (a.getHeight(), { sub + hGlyph, sub + hGlyph, sub + hMix });

        struct Cell { juce::Slider* s; juce::Label* l; bool morph; };
        auto row = [&] (int top, std::initializer_list<Cell> cells)
        {
            int i = 0;
            for (const auto& c : cells)
                placeKnob (*c.s, *c.l, a.getX() + i++ * colW, top + sub, colW, 52, true, c.morph);
        };

        int y = a.getY() + ys[0];
        decor.sub (content, "dco1", "DCO 1").setBounds (a.getX(), y, a.getWidth(), 19);
        row (y, { { &octaveSlider,        &octaveLabel,        false },
                  { &oscMorphSlider,      &oscMorphLabel,      true  },
                  { &oscPWMSlider,        &oscPWMLabel,        false },
                  { &supersawMixSlider,   &supersawMixLabel,   false },
                  { &supersawDetuneSlider,&supersawDetuneLabel,false },
                  { &wavefoldSlider,      &wavefoldLabel,      false } });

        y = a.getY() + ys[1];
        decor.sub (content, "dco2", "DCO 2").setBounds (a.getX(), y, a.getWidth(), 19);
        row (y, { { &dco2MorphSlider,     &dco2MorphLabel,     true  },
                  { &dco2PWMSlider,       &dco2PWMLabel,       false },
                  { &semiTuneSlider,      &semiTuneLabel,      false },
                  { &fineTuneSlider,      &fineTuneLabel,      false },
                  { &fmAmountSlider,      &fmAmountLabel,      false },
                  { &dco2FoldSlider,      &dco2FoldLabel,      false } });

        y = a.getY() + ys[2];
        decor.sub (content, "mix", "MIXER / PERFORMANCE").setBounds (a.getX(), y, a.getWidth(), 19);
        const int top = y + sub;
        placeKnob (dco2LevelSlider,   dco2LevelLabel,   a.getX() + 0 * colW, top, colW, 52, false);
        placeKnob (subOscLevelSlider, subOscLevelLabel, a.getX() + 1 * colW, top, colW, 52, false);
        placeKnob (noiseLevelSlider,  noiseLevelLabel,  a.getX() + 2 * colW, top, colW, 52, false);
        placeKnob (glideTimeSlider,   glideTimeLabel,   a.getX() + 3 * colW, top, colW, 52, false);

        const int gx = a.getX() + 4 * colW + 8, gw = 2 * colW - 8, fw = (gw - 12) / 2, gy = top + 2;
        btn (subOscOctaveButton,  gx,            gy,      fw);       // SUB -1/-2
        btn (supersawModeButton,  gx + fw + 12,  gy,      fw);       // HARD SYNC
        btn (glideEnableButton,   gx,            gy + 44, fw);       // PORTA
        btn (legatoModeButton,    gx + fw + 12,  gy + 44, fw);       // LEGATO
        btn (subOscWaveButton,    gx,            gy + 88, gw);       // SUB WAVE: SQ
    }

    // ---------------------------------------------------------------- filter
    {
        const auto a = area (filtPanel);                          // 516 wide
        const int hA = knobCellHeight (42, false), hB = knobCellHeight (76, false), hC = hA;
        const auto ys = distribute (a.getHeight(), { hA, hB, hC }, 10);

        int y = a.getY() + ys[0];                                 // row A
        btn (filterRoutingButton, a.getX(), y + 15, 110);         // PARALLEL
        placeKnob (filterBalanceSlider, filterBalanceLabel, a.getX() + 140, y, 86, 42, false);
        placeField (f2ModeComboBox, f2ModeLabel, a.getX() + 262, y + 6, 180);

        y = a.getY() + ys[1];                                     // row B (big knobs; normal ones sit 12 px lower so bodies align)
        const int w4 = a.getWidth() / 4;
        placeKnob (f1CutoffSlider,    f1CutoffLabel,    a.getX() + 0 * w4, y,      w4, 76, false);
        placeKnob (f1ResonanceSlider, f1ResonanceLabel, a.getX() + 1 * w4, y + 12, w4, 52, false);
        placeKnob (f2CutoffSlider,    f2CutoffLabel,    a.getX() + 2 * w4, y,      w4, 76, false);
        placeKnob (f2ResonanceSlider, f2ResonanceLabel, a.getX() + 3 * w4, y + 12, w4, 52, false);

        y = a.getY() + ys[2];                                     // row C
        const int w6 = a.getWidth() / 6;
        placeKnob (f1EnvDepthSlider,  f1EnvDepthLabel,  a.getX() + 0 * w6, y, w6, 42, false);
        placeKnob (f1KeyTrackSlider,  f1KeyTrackLabel,  a.getX() + 1 * w6, y, w6, 42, false);
        placeKnob (f1LfoDepthSlider,  f1LfoDepthLabel,  a.getX() + 2 * w6, y, w6, 42, false);
        placeKnob (f1HPFSlider,       f1HPFLabel,       a.getX() + 3 * w6, y, w6, 42, false);   // HPF CUTOFF
        placeKnob (f2EnvDepthSlider,  f2EnvDepthLabel,  a.getX() + 4 * w6, y, w6, 42, false);
        placeKnob (f2KeyTrackSlider,  f2KeyTrackLabel,  a.getX() + 5 * w6, y, w6, 42, false);
    }

    // ----------------------------------------------------------------- scope
    {
        const auto a = area (scopePanel);                         // 390 wide
        if (scopeFrame.getParentComponent() == nullptr)
            content.addAndMakeVisible (scopeFrame);
        scopeFrame.setBounds (a.getX(), a.getY(), 390, 390);      // SQUARE
        oscilloscope.setTraceOnly (true);
        oscilloscope.setBounds (scopeFrame.getScreenBounds().translated (scopeFrame.getX(), scopeFrame.getY()));
        btn (crtThemeButton, a.getX(), a.getY() + 390 + 18, 170); // SCOPE: <colour>
    }

    // -------------------------------------------------------------- envelope
    {
        const auto a = area (envPanel);                           // 656 wide
        const int hTop = 43 + 8 + 150, hAux = 25 + knobCellHeight (60, false, true);
        const auto ys = distribute (a.getHeight(), { hTop, hAux }, 10);

        int y = a.getY() + ys[0];
        int x = a.getX();
        btn (envPrevButton,   x, y + 3, 40);  x += 40 + 6;
        btn (envVcfTabButton, x, y + 3, 64);  x += 64 + 6;
        btn (envVcaTabButton, x, y + 3, 64);  x += 64 + 6;
        btn (envAuxTabButton, x, y + 3, 64);  x += 64 + 6;
        btn (envNextButton,   x, y + 3, 40);

        const int lcdY = y + 43 + 8;
        envLcd.setBounds (a.getX(), lcdY, 330, 150);
        const int kx = a.getX() + 330 + 16, kw = 77, ky = lcdY + (150 - knobCellHeight (42, false)) / 2;
        placeKnob (envAttackSlider,  envAttackLabel,  kx + 0 * kw, ky, kw, 42, false);
        placeKnob (envDecaySlider,   envDecayLabel,   kx + 1 * kw, ky, kw, 42, false);
        placeKnob (envSustainSlider, envSustainLabel, kx + 2 * kw, ky, kw, 42, false);
        placeKnob (envReleaseSlider, envReleaseLabel, kx + 3 * kw, ky, kw, 42, false);

        y = a.getY() + ys[1];
        styleSub (auxModSectionLabel);
        auxModSectionLabel.setBounds (a.getX(), y, a.getWidth(), 19);
        const int aw = a.getWidth() / 5, ay = y + 25;
        placeKnob (auxToPitchSlider,    auxToPitchLabel,    a.getX() + 0 * aw, ay, aw, 60, false, false, true);
        placeKnob (auxToFilter1Slider,  auxToFilter1Label,  a.getX() + 1 * aw, ay, aw, 60, false, false, true);
        placeKnob (auxToFilter2Slider,  auxToFilter2Label,  a.getX() + 2 * aw, ay, aw, 60, false, false, true);
        placeKnob (auxToWavefoldSlider, auxToWavefoldLabel, a.getX() + 3 * aw, ay, aw, 60, false, false, true);
        placeKnob (auxToPWMSlider,      auxToPWMLabel,      a.getX() + 4 * aw, ay, aw, 60, false, false, true);
    }

    // --------------------------------------------------------------- LFO / MOD
    {
        const auto a = area (lfoPanel);                           // 436 wide
        const int hL = 25 + knobCellHeight (42, false);           // 132
        const int hM = 25 + 52 + 12 + 52 + 12 + knobCellHeight (42, false);
        const auto ys = distribute (a.getHeight(), { hL, hM }, 10);

        int y = a.getY() + ys[0];
        decor.sub (content, "lfo", "LFO").setBounds (a.getX(), y, a.getWidth(), 19);
        int cy = y + 25;
        placeField (lfoSelectComboBox, lfoSelectLabel, a.getX(),       cy + 6, 100);
        placeField (lfoShapeComboBox,  lfoShapeLabel,  a.getX() + 116, cy + 6, 120);
        placeKnob  (lfoRateSlider,  lfoRateLabel,  a.getX() + 252, cy, 86, 42, false);
        placeKnob  (lfoDepthSlider, lfoDepthLabel, a.getX() + 338, cy, 86, 42, false);       // TO FILT

        y = a.getY() + ys[1];
        decor.sub (content, "mod", "MOD MATRIX").setBounds (a.getX(), y, a.getWidth(), 19);
        const int r1 = y + 25, r2 = r1 + 52 + 12, r3 = r2 + 52 + 12;
        placeField (modSlotSelectComboBox, modSlotSelectLabel, a.getX(), r1, 110);
        btn (modSlotEnableButton, a.getX() + 130, r1 + 19, 100);
        placeField (modSourceComboBox, modSourceLabel, a.getX(),       r2, 200);
        placeField (modDestComboBox,   modDestLabel,   a.getX() + 216, r2, 220);
        placeField (modViaSourceComboBox, modViaSourceLabel, a.getX(), r3 + 12, 200);
        placeKnob  (modAmountSlider,    modAmountLabel,    a.getX() + 216, r3, 86, 42, false);
        placeKnob  (modViaAmountSlider, modViaAmountLabel, a.getX() + 318, r3, 86, 42, false);
    }

    // ------------------------------------------------------------- sequencer
    {
        const auto a = area (seqPanel);                           // 600 wide
        const int hGroup = 43 + 8 + 43, hLcd = 130, hEdit = 43, hTiming = knobCellHeight (42, false), hArp = 52;
        const auto ys = distribute (a.getHeight(), { hGroup, hLcd, hEdit, hTiming, hArp }, 8);

        int y = a.getY() + ys[0];                                 // track row
        int x = a.getX();
        btn (seqTrackLayerButton, x, y + 3, 110); x += 110 + 8;
        btn (seqTrackMuteButton,  x, y + 3, 74);  x += 74 + 8;
        btn (seqTrackSoloButton,  x, y + 3, 74);  x += 74 + 16;
        for (auto* pg : { &seqPage1Button, &seqPage2Button, &seqPage3Button, &seqPage4Button })
        { btn (*pg, x, y + 3, 46); x += 46 + 8; }

        const int y2 = y + 43 + 8;                                // transport row
        x = a.getX();
        btn (seqPlayButton,  x, y2 + 3, 84); x += 84 + 8;
        btn (seqRecButton,   x, y2 + 3, 76); x += 76 + 8;
        btn (arpEnableButton,x, y2 + 3, 76); x += 76 + 8;
        btn (seqSyncButton,  x, y2 + 3, 76); x += 76 + 8;         // HOST
        btn (chordEnableButton, x, y2 + 3, 84); x += 84 + 8;
        chordTypeComboBox.setBounds (x, y2 + 4, 130, kSelectH);

        y = a.getY() + ys[1];
        seqLcd.setBounds (a.getX(), y, 600, hLcd);

        y = a.getY() + ys[2];                                     // step edit row
        x = a.getX();
        btn (prevStepButton, x, y + 3, 40);  x += 40 + 8;
        styleCaption (currentStepLabel);
        currentStepLabel.setBounds (x, y + 3, 90, 34);            x += 90 + 8;
        btn (nextStepButton, x, y + 3, 40);  x += 40 + 8;
        btn (auditionStepButton, x, y + 3, 110); x += 110 + 8;
        btn (clearLocksButton,   x, y + 3, 100);

        y = a.getY() + ys[3];                                     // timing knobs
        const int tw = a.getWidth() / 4;
        placeKnob (seqTempoSlider,      seqTempoLabel,      a.getX() + 0 * tw, y, tw, 42, false);   // BPM
        placeKnob (seqSwingSlider,      seqSwingLabel,      a.getX() + 1 * tw, y, tw, 42, false);
        placeKnob (seqGateLengthSlider, seqGateLengthLabel, a.getX() + 2 * tw, y, tw, 42, false);   // GATE
        placeKnob (stepGateSlider,      stepGateLabel,      a.getX() + 3 * tw, y, tw, 42, false);   // STEP GATE

        y = a.getY() + ys[4];                                     // ARP row (captions below)
        const int cw = a.getWidth() / 4;
        placeCaptionedCombo (seqDivisionComboBox,  seqDivisionLabel,  a.getX() + 0 * cw, y, cw - 16);
        placeCaptionedCombo (seqDirectionComboBox, seqDirectionLabel, a.getX() + 1 * cw, y, cw - 16);
        placeCaptionedCombo (seqLengthComboBox,    seqLengthLabel,    a.getX() + 2 * cw, y, cw - 16);
        placeCaptionedCombo (seqModeComboBox,      seqModeLabel,      a.getX() + 3 * cw, y, cw - 16);
    }

    // ---------------------------------------------------------- master effects
    {
        // >>> FILL IN with the REAL member names (they were not in members.txt). Find them by caption in the
        // >>> constructor: CHO I, CHO II, ROUTING combo ("Both Tracks"), SYNC, PING-PONG, TIME, FEEDBACK,
        // >>> DAMP (delay), DLY MIX, SIZE, DAMP (reverb), REV MIX, each with its caption label.
        S116::layout::FxRefs fx;
        fx.cho1 = &chorus1Button;   fx.cho2 = &chorus2Button;   fx.sync = &delaySyncButton;   fx.pingPong = &delayPingPongButton;
        fx.routing = &fxRoutingComboBox; fx.routingLabel = &fxRoutingLabel;
        fx.time = &delayTimeSlider; fx.timeLabel = &delayTimeLabel;
        fx.feedback = &delayFeedbackSlider; fx.feedbackLabel = &delayFeedbackLabel;
        fx.delayDamp = &delayDampSlider; fx.delayDampLabel = &delayDampLabel;
        fx.delayMix = &delayMixSlider; fx.delayMixLabel = &delayMixLabel;
        fx.revSize = &reverbSizeSlider; fx.revSizeLabel = &reverbSizeLabel;
        fx.revDamp = &reverbDampSlider; fx.revDampLabel = &reverbDampLabel;
        fx.revMix = &reverbMixSlider;  fx.revMixLabel = &reverbMixLabel;

        const auto a = area (fxPanel);                            // 1804 x 142
        const int w = a.getWidth();
        const int x0 = a.getX(), x1 = x0 + (int) (w * 0.27f), x2 = x0 + (int) (w * 0.73f);
        decor.sub (content, "cho", "CHORUS & ROUTE").setBounds (x0, a.getY(), x1 - x0 - 24, 19);
        decor.sub (content, "dly", "STEREO DELAY").setBounds   (x1, a.getY(), x2 - x1 - 24, 19);
        decor.sub (content, "rev", "REVERB").setBounds         (x2, a.getY(), a.getRight() - x2, 19);
        const int cy = a.getY() + 25;

        btn (*fx.cho1, x0,       cy + 20, 90);
        btn (*fx.cho2, x0 + 102, cy + 20, 90);
        placeField (*fx.routing, *fx.routingLabel, x0 + 210, cy + 6, 160);

        btn (*fx.sync,     x1,      cy + 20, 80);
        btn (*fx.pingPong, x1 + 92, cy + 20, 120);
        const int dx = x1 + 232, dw = (x2 - 24 - dx) / 4;
        placeKnob (*fx.time,      *fx.timeLabel,      dx + 0 * dw, cy, dw, 42, false);
        placeKnob (*fx.feedback,  *fx.feedbackLabel,  dx + 1 * dw, cy, dw, 42, false);
        placeKnob (*fx.delayDamp, *fx.delayDampLabel, dx + 2 * dw, cy, dw, 42, false);
        placeKnob (*fx.delayMix,  *fx.delayMixLabel,  dx + 3 * dw, cy, dw, 42, false);

        const int rw = (a.getRight() - x2) / 3;
        placeKnob (*fx.revSize, *fx.revSizeLabel, x2 + 0 * rw, cy, rw, 42, false);
        placeKnob (*fx.revDamp, *fx.revDampLabel, x2 + 1 * rw, cy, rw, 42, false);
        placeKnob (*fx.revMix,  *fx.revMixLabel,  x2 + 2 * rw, cy, rw, 42, false);
    }

    // -------------------------------------------------- z-order (once): backdrops behind controls
    if (! zOrderDone)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &scopeFrame, &fxPanel, &seqPanel, &lfoPanel, &envPanel,
                                                                 &scopePanel, &filtPanel, &oscPanel, &headerBar, &chassisBg })
            c->toBack();
        oscilloscope.toFront (false);
        zOrderDone = true;
    }
}
