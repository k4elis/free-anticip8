#pragma once
#include "PluginProcessor.h"
#include "UI/CustomLookAndFeel.h"
#include "UI/Controls.h"
#include "UI/WaveformComponent.h"

class AnticipAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::FileDragAndDropTarget,
                                    private juce::ChangeListener,
                                    private juce::Timer
{
public:
    explicit AnticipAudioProcessorEditor (AnticipAudioProcessor&);
    ~AnticipAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override { return files.size() > 0; }
    void fileDragEnter (const juce::StringArray&, int, int) override { hover = true;  repaint(); }
    void fileDragExit (const juce::StringArray&) override            { hover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    class Chrome : public juce::Component
    {
    public:
        explicit Chrome (AnticipAudioProcessorEditor& o) : owner (o) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { owner.paintChrome (g); }
        AnticipAudioProcessorEditor& owner;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void openFile();
    void startExternalDrag();
    void toggleListen();
    void showHelp();
    void updateListenState();
    void updateInputLabel();
    void refreshWaveformMode();
    void layoutDesign();
    void paintChrome (juce::Graphics&);

    static constexpr int kDesignW = 440;
    static constexpr int kDesignH = 720;

    AnticipAudioProcessor& proc;
    anticip8::CustomLookAndFeel laf;

    juce::Component content;
    Chrome chrome { *this };

    anticip8::ReverseKnob reverseKnob;
    anticip8::PillButton listenButton { "ÉCOUTER", anticip8::PillButton::Outline };
    anticip8::SegmentedControl modeControl;
    anticip8::PillButton inputButton { "ENTRÉE OUVERTE", anticip8::PillButton::SoftPink };
    anticip8::WaveformComponent waveform;
    anticip8::CharacterRow characterRow;
    anticip8::DivisionRow divisionRow;
    anticip8::IconButton helpButton { anticip8::IconButton::Help };
    anticip8::IconButton settingsButton { anticip8::IconButton::Settings }; // décoratif

    juce::Rectangle<int> headerArea, footerArea, statusArea;

    std::unique_ptr<juce::FileChooser> chooser;

    bool hover = false, hasResult = false, listening = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnticipAudioProcessorEditor)
};
