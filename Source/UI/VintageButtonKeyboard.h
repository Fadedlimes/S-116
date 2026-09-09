#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

class VintageButtonKeyboard : public juce::Component {
public:
    explicit VintageButtonKeyboard(juce::MidiKeyboardState& state)
    : keyboardState(state) {
        setWantsKeyboardFocus(false);
    }

    void setLEDTheme(juce::Colour primary, juce::Colour glow, juce::Colour highlight, juce::Colour unlit) {
        ledPrimary = primary;
        ledGlow = glow;
        ledHighlight = highlight;
        ledUnlit = unlit;
        repaint();
    }

    void setSequencerState(bool isPlaying, bool isRecording, int currentStep, int recordStep, int currentPage) {
        seqPlaying = isPlaying;
        seqRecording = isRecording;
        seqCurrentStep = currentStep;
        seqRecordStep = recordStep;
        seqCurrentPage = currentPage;
        repaint();
    }

    void setActiveNotes(const std::array<bool, 25>& active) {
        activeNotes = active;
        repaint();
    }

    int getOctaveOffset() const { return octaveOffset; }

    void shiftOctave(int delta) {
        octaveSetting = juce::jlimit(-2, 2, octaveSetting + delta);
        octaveOffset = octaveSetting * 12;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Background Tray
        g.setColour(juce::Colour(0xff181b21));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(0xff424854));
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

        // --- OCTAVE CONTROLS (Left Section: 0..80px) ---
        auto octArea = juce::Rectangle<float>(6.0f, 6.0f, 74.0f, bounds.getHeight() - 12.0f);
        g.setColour(juce::Colour(0xff222730));
        g.fillRoundedRectangle(octArea, 3.0f);

        g.setColour(juce::Colour(0xffa5b1c2));
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawText("OCTAVE", octArea.getX(), octArea.getY() + 4.0f, octArea.getWidth(), 12.0f, juce::Justification::centred);

        // Octave 5-dot LED ladder
        for (int i = -2; i <= 2; ++i) {
            float dotX = octArea.getCentreX() + static_cast<float>(i) * 11.0f;
            float dotY = octArea.getY() + 24.0f;
            float r = 2.4f;
            if (i == octaveSetting) {
                g.setColour(ledGlow);
                g.fillEllipse(dotX - r - 2.0f, dotY - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f);
                g.setColour(ledPrimary);
                g.fillEllipse(dotX - r, dotY - r, r * 2.0f, r * 2.0f);
                g.setColour(ledHighlight);
                g.fillEllipse(dotX - r * 0.4f, dotY - r * 0.4f, r * 0.8f, r * 0.8f);
            } else {
                g.setColour(ledUnlit);
                g.fillEllipse(dotX - r, dotY - r, r * 2.0f, r * 2.0f);
            }
        }

        // Octave Buttons: [-] and [+]
        auto drawOctBtn = [&](juce::Rectangle<float> b, const juce::String& text, bool isDown) {
            g.setColour(juce::Colour(0xff2d333f));
            g.fillRoundedRectangle(b, 3.0f);
            g.setColour(isDown ? juce::Colour(0xff1e222a) : juce::Colour(0xff3d4554));
            g.fillRoundedRectangle(b.reduced(1.0f), 2.5f);
            g.setColour(juce::Colour(0xfff0f2f5));
            g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
            g.drawText(text, b, juce::Justification::centred);
        };
        drawOctBtn(octDownBounds, "OCT -", mouseDownOctDown);
        drawOctBtn(octUpBounds,   "OCT +", mouseDownOctUp);

        // --- 25-KEY TACTILE PUSH BUTTON KEYBOARD (86px..Right) ---
        static const int whiteKeyIndices[] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24 };
        static const int blackKeyIndices[] = { 1, 3, -1, 6, 8, 10, -1, 13, 15, -1, 18, 20, 22, -1 };
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };

            float kbX = 86.0f;
            float kbY = 6.0f;
            float kbW = bounds.getWidth() - 92.0f;
            float kbH = bounds.getHeight() - 12.0f;
            float whiteKeyW = (kbW - 14.0f * 3.0f) / 15.0f;

            // 1. Draw 15 White Keys
            for (int i = 0; i < 15; ++i) {
                int keyIdx = whiteKeyIndices[i];
                float kx = kbX + static_cast<float>(i) * (whiteKeyW + 3.0f);
                auto keyRect = juce::Rectangle<float>(kx, kbY, whiteKeyW, kbH);
                whiteKeyBounds[i] = keyRect;

                bool isDown = activeNotes[static_cast<size_t>(keyIdx)] || (lastMouseDownKey == keyIdx);

                g.setColour(juce::Colour(0xff252a33));
                g.fillRoundedRectangle(keyRect, 4.0f);

                auto innerKey = keyRect.reduced(1.2f);
                juce::ColourGradient wGrad(isDown ? juce::Colour(0xffcbd0d8) : juce::Colour(0xfff8f9fb),
                                           innerKey.getX(), innerKey.getY(),
                                           isDown ? juce::Colour(0xffa5acb8) : juce::Colour(0xffd4d9e2),
                                           innerKey.getX(), innerKey.getBottom(), false);
                g.setGradientFill(wGrad);
                g.fillRoundedRectangle(innerKey, 3.0f);

                // LED Indicator on White Key
                float ledX = keyRect.getCentreX();
                float ledY = keyRect.getY() + 10.0f;
                drawKeyLED(g, ledX, ledY, keyIdx);

                // Note label
                g.setColour(juce::Colour(0xff333a46));
                g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
                g.drawText(noteNames[keyIdx], keyRect.getX(), keyRect.getBottom() - 18.0f, keyRect.getWidth(), 14.0f, juce::Justification::centred);
            }

            // 2. Draw 10 Black Keys
            float blackKeyW = whiteKeyW * 0.72f;
            float blackKeyH = kbH * 0.58f;

            for (int i = 0; i < 14; ++i) {
                int keyIdx = blackKeyIndices[i];
                if (keyIdx < 0) continue;

                float kx = kbX + static_cast<float>(i) * (whiteKeyW + 3.0f) + whiteKeyW - (blackKeyW * 0.5f) + 1.5f;
                auto keyRect = juce::Rectangle<float>(kx, kbY, blackKeyW, blackKeyH);
                blackKeyBounds[keyIdx] = keyRect;

                bool isDown = activeNotes[static_cast<size_t>(keyIdx)] || (lastMouseDownKey == keyIdx);

                g.setColour(juce::Colour(0xff101317));
                g.fillRoundedRectangle(keyRect, 3.0f);

                auto innerKey = keyRect.reduced(1.0f);
                juce::ColourGradient bGrad(isDown ? juce::Colour(0xff4a5260) : juce::Colour(0xff2c323c),
                                           innerKey.getX(), innerKey.getY(),
                                           isDown ? juce::Colour(0xff222730) : juce::Colour(0xff181b21),
                                           innerKey.getX(), innerKey.getBottom(), false);
                g.setGradientFill(bGrad);
                g.fillRoundedRectangle(innerKey, 2.5f);

                // LED Indicator on Black Key
                float ledX = keyRect.getCentreX();
                float ledY = keyRect.getY() + 9.0f;
                drawKeyLED(g, ledX, ledY, keyIdx);

                // Note label
                g.setColour(juce::Colour(0xffa5b1c2));
                g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
                g.drawText(noteNames[keyIdx], keyRect.getX(), keyRect.getBottom() - 15.0f, keyRect.getWidth(), 12.0f, juce::Justification::centred);
            }
    }

    void resized() override {
        octDownBounds = juce::Rectangle<float>(12.0f, getHeight() - 66.0f, 62.0f, 26.0f);
        octUpBounds   = juce::Rectangle<float>(12.0f, getHeight() - 34.0f, 62.0f, 26.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        auto pos = e.position;
        if (octDownBounds.contains(pos)) {
            mouseDownOctDown = true;
            shiftOctave(-1);
            repaint();
            return;
        }
        if (octUpBounds.contains(pos)) {
            mouseDownOctUp = true;
            shiftOctave(1);
            repaint();
            return;
        }

        int key = getKeyForPosition(e.getPosition());
        if (key >= 0 && key < 25) {
            lastMouseDownKey = key;
            int midiNote = 48 + octaveOffset + key;
            keyboardState.noteOn(1, midiNote, 1.0f);
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (mouseDownOctDown || mouseDownOctUp) return;

        int key = getKeyForPosition(e.getPosition());
        if (key != lastMouseDownKey) {
            if (lastMouseDownKey >= 0) {
                int oldMidi = 48 + octaveOffset + lastMouseDownKey;
                keyboardState.noteOff(1, oldMidi, 0.0f);
            }
            lastMouseDownKey = key;
            if (key >= 0 && key < 25) {
                int newMidi = 48 + octaveOffset + key;
                keyboardState.noteOn(1, newMidi, 1.0f);
            }
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override {
        juce::ignoreUnused(e);
        mouseDownOctDown = false;
        mouseDownOctUp = false;
        if (lastMouseDownKey >= 0) {
            int midiNote = 48 + octaveOffset + lastMouseDownKey;
            keyboardState.noteOff(1, midiNote, 0.0f);
            lastMouseDownKey = -1;
            repaint();
        }
    }

private:
    juce::MidiKeyboardState& keyboardState;
    int octaveSetting = 0;
    int octaveOffset = 0;

    std::array<bool, 25> activeNotes { false };
    bool seqPlaying = false;
    bool seqRecording = false;
    int seqCurrentStep = 0;
    int seqRecordStep = 0;
    int seqCurrentPage = 0;

    juce::Colour ledPrimary { 0xffff2d55 };
    juce::Colour ledGlow { 0x60ff2d55 };
    juce::Colour ledHighlight { 0xffffa2b0 };
    juce::Colour ledUnlit { 0xff420e14 };

    int lastMouseDownKey = -1;
    bool mouseDownOctDown = false;
    bool mouseDownOctUp = false;

    juce::Rectangle<float> octDownBounds;
    juce::Rectangle<float> octUpBounds;
    std::array<juce::Rectangle<float>, 15> whiteKeyBounds;
    std::array<juce::Rectangle<float>, 25> blackKeyBounds;

    int getKeyForPosition(juce::Point<int> pos) const {
        auto pf = pos.toFloat();
        // Check black keys first (top z-order)
        for (int k : { 1, 3, 6, 8, 10, 13, 15, 18, 20, 22 }) {
            if (blackKeyBounds[static_cast<size_t>(k)].contains(pf))
                return k;
        }
        static const int whiteKeyIndices[] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24 };
        for (int i = 0; i < 15; ++i) {
            if (whiteKeyBounds[static_cast<size_t>(i)].contains(pf))
                return whiteKeyIndices[i];
        }
        return -1;
    }

    void drawKeyLED(juce::Graphics& g, float cx, float cy, int keyIdx) {
        float r = 2.4f;
        bool isLit = activeNotes[static_cast<size_t>(keyIdx)] || (lastMouseDownKey == keyIdx);

        // Check if sequencer step matches this key in step view
        if (seqPlaying && (seqCurrentStep % 16 == keyIdx % 16) && (seqCurrentStep / 16 == seqCurrentPage)) {
            isLit = true;
        }
        if (seqRecording && (seqRecordStep % 16 == keyIdx % 16) && (seqRecordStep / 16 == seqCurrentPage)) {
            isLit = true;
        }

        if (isLit) {
            g.setColour(ledGlow);
            g.fillEllipse(cx - r - 2.5f, cy - r - 2.5f, (r + 2.5f) * 2.0f, (r + 2.5f) * 2.0f);
            g.setColour(ledPrimary);
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour(ledHighlight);
            g.fillEllipse(cx - r * 0.45f, cy - r * 0.45f, r * 0.9f, r * 0.9f);
        } else {
            g.setColour(ledUnlit);
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
    }
};
