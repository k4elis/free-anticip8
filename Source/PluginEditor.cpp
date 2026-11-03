#include "PluginEditor.h"

using namespace anticip8;

AnticipAudioProcessorEditor::AnticipAudioProcessorEditor (AnticipAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      reverseKnob (p.apvts, "length"),
      modeControl (*p.apvts.getParameter ("fx"), { "FX", "BRUT" }, true),
      characterRow (*p.apvts.getParameter ("character"),
                    *p.apvts.getParameter ("tremolo"),
                    *p.apvts.getParameter ("fx")),
      divisionRow (*p.apvts.getParameter ("division"),
                   *p.apvts.getParameter ("fx"),
                   *p.apvts.getParameter ("tremolo"))
{
    setLookAndFeel (&laf);

    addAndMakeVisible (content);
    content.addAndMakeVisible (chrome);
    content.addAndMakeVisible (reverseKnob);
    content.addAndMakeVisible (listenButton);
    content.addAndMakeVisible (modeControl);
    content.addAndMakeVisible (inputButton);
    content.addAndMakeVisible (waveform);
    content.addAndMakeVisible (characterRow);
    content.addAndMakeVisible (divisionRow);
    content.addAndMakeVisible (helpButton);
    content.addAndMakeVisible (settingsButton);
    settingsButton.setEnabled (false); // decoratif (comme le vrai ANTICIP8)

    listenButton.onClick = [this] { toggleListen(); };
    inputButton.onClick  = [this]
    {
        if (juce::ModifierKeys::getCurrentModifiers().isAnyModifierKeyDown())
            openFile();
        else
            proc.toggleInputOpen();
        updateInputLabel();
    };
    waveform.onDragStart = [this] { startExternalDrag(); };
    waveform.onTrimChanged = [this] (float a, float b) { proc.setTrimRange (a, b); };
    helpButton.onClick = [this] { showHelp(); };

    setSize (kDesignW, kDesignH);
    setResizable (true, true);
    setResizeLimits (kDesignW / 2, kDesignH / 2, kDesignW * 2, kDesignH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);

    layoutDesign();
    proc.addChangeListener (this);
    changeListenerCallback (nullptr);
    updateInputLabel();
    updateListenState();
    refreshWaveformMode();
    startTimerHz (30);
}

AnticipAudioProcessorEditor::~AnticipAudioProcessorEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void AnticipAudioProcessorEditor::layoutDesign()
{
    auto r = juce::Rectangle<int> (0, 0, kDesignW, kDesignH).reduced (22, 18);

    headerArea = r.removeFromTop (52);
    auto icons = headerArea;
    auto iconStrip = icons.removeFromRight (96);
    helpButton.setBounds (iconStrip.removeFromLeft (28).withSizeKeepingCentre (26, 26));
    iconStrip.removeFromLeft (8);
    settingsButton.setBounds (iconStrip.removeFromLeft (28).withSizeKeepingCentre (26, 26));

    r.removeFromTop (8);
    reverseKnob.setBounds (r.removeFromTop (210).withSizeKeepingCentre (210, 210));

    r.removeFromTop (10);
    auto row = r.removeFromTop (34);
    listenButton.setBounds (row.removeFromLeft (92));
    row.removeFromLeft (10);
    modeControl.setBounds (row.removeFromLeft (110));
    inputButton.setBounds (row.removeFromRight (128));

    r.removeFromTop (12);
    waveform.setBounds (r.removeFromTop (160));

    r.removeFromTop (12);
    characterRow.setBounds (r.removeFromTop (40));

    r.removeFromTop (6);
    auto statusRow = r.removeFromTop (22);
    statusArea = statusRow.removeFromLeft (statusRow.getWidth() - 200);
    divisionRow.setBounds (statusRow.reduced (0, 1));

    r.removeFromTop (8);
    footerArea = r.removeFromTop (28);

    chrome.setBounds (0, 0, kDesignW, kDesignH);
    chrome.toBack();
}

void AnticipAudioProcessorEditor::resized()
{
    // Uniform scale from top-left of a centred design-size panel.
    // Position via setBounds (ox,oy) - pas de translation dans le transform
    // (sinon hit-test / clipping foireux dans plusieurs hosts).
    const float sx = (float) getWidth()  / (float) kDesignW;
    const float sy = (float) getHeight() / (float) kDesignH;
    const float scale = juce::jmin (sx, sy);
    const int w = juce::roundToInt ((float) kDesignW * scale);
    const int h = juce::roundToInt ((float) kDesignH * scale);
    const int ox = (getWidth()  - w) / 2;
    const int oy = (getHeight() - h) / 2;

    content.setTransform (juce::AffineTransform());
    content.setBounds (ox, oy, kDesignW, kDesignH);
    content.setTransform (juce::AffineTransform::scale (scale));
}

void AnticipAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace Palette;
    g.setGradientFill (juce::ColourGradient (bg, 0.0f, 0.0f, bgLow, 0.0f, (float) getHeight(), false));
    g.fillAll();
}

void AnticipAudioProcessorEditor::paintChrome (juce::Graphics& g)
{
    using namespace Palette;

    g.setColour (violet.withAlpha (0.04f));
    g.fillEllipse (-80.0f, -60.0f, 260.0f, 220.0f);
    g.setColour (pink.withAlpha (0.03f));
    g.fillEllipse ((float) kDesignW - 160.0f, (float) kDesignH - 200.0f, 240.0f, 240.0f);

    // Header: ANTICIP8 + coloured first letters
    {
        auto title = headerArea.toFloat();
        g.setColour (text);
        g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
        g.drawText ("ANTICIP8", title.removeFromTop (28.0f), juce::Justification::centredLeft, false);

        const juce::Font sub (juce::FontOptions (11.0f, juce::Font::bold));
        g.setFont (sub);
        float x = title.getX();
        const float y = title.getY() + 11.0f;
        auto drawWord = [&] (const juce::String& word, bool last)
        {
            g.setColour (pink);
            g.drawSingleLineText (word.substring (0, 1), (int) x, (int) y, juce::Justification::left);
            x += sub.getStringWidthFloat (word.substring (0, 1));
            g.setColour (dim);
            g.drawSingleLineText (word.substring (1), (int) x, (int) y, juce::Justification::left);
            x += sub.getStringWidthFloat (word.substring (1));
            if (! last) x += sub.getStringWidthFloat ("  ");
        };
        drawWord ("INVERSE", false);
        drawWord ("REVERB", false);
        drawWord ("RISER", true);

        g.setColour (text.withAlpha (0.9f));
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText ("TEMPL8", headerArea.withTrimmedLeft (headerArea.getWidth() - 70).withTrimmedTop (4).withHeight (24),
                    juce::Justification::centredRight, false);
    }

    // LISTEN — FX/RAW connector
    {
        const auto a = listenButton.getBounds().toFloat();
        const auto b = modeControl.getBounds().toFloat();
        g.setColour (muted.withAlpha (0.85f));
        g.drawLine (a.getRight() + 2.0f, a.getCentreY(), b.getX() - 2.0f, b.getCentreY(), 1.2f);
    }

    if (hover)
    {
        g.setColour (violet.withAlpha (0.25f));
        g.drawRoundedRectangle (waveform.getBounds().toFloat().expanded (3.0f), 16.0f, 2.0f);
    }

    // Status
    {
        const auto err = proc.getLastError();
        const auto info = proc.getInputInfo();
        const bool fx = proc.apvts.getRawParameterValue ("fx")->load() >= 0.5f;
        const auto sr = info.valid ? juce::String (info.sampleRate / 1000.0, 0) + " kHz"
                                   : juce::String (48) + " kHz";

        juce::String msg;
        auto col = dim;

        if (err.isNotEmpty())                         { msg = err; col = error; }
        else if (listening)
        {
            const float len = reverseKnob.getSeconds();
            msg = fx ? ("ÉCOUTER: riser  -  " + juce::String (len, 1) + " s")
                     : "ÉCOUTER: enregistrement coupé";
            col = pinkLight;
        }
        else if (proc.isInputOpen())                  { msg = "ENTRÉE OUVERTE - joue ton entrée une fois"; col = pinkLight; }
        else if (proc.isRendering())                  { msg = "Rendu..."; col = violetLight; }
        else if (! info.valid)                        { msg = "Joue ton entrée une fois"; }
        else if (hasResult && fx)
        {
            juce::AttributedString a;
            a.append ("Prêt:  ", juce::Font (juce::FontOptions (11.5f)), dim);
            a.append ("glisse dans ta DAW", juce::Font (juce::FontOptions (11.5f, juce::Font::bold)), text);
            a.append ("  -  " + sr, juce::Font (juce::FontOptions (11.5f)), dim);
            a.setJustification (juce::Justification::centredLeft);
            a.draw (g, statusArea.toFloat());
        }
        else if (! fx)                                { msg = "BRUT: ton enregistrement. Déplace les poignées pour couper."; }
        else                                          { msg = "Prêt  -  " + sr; }

        if (msg.isNotEmpty())
        {
            g.setColour (col);
            g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::plain)));
            g.drawFittedText (msg, statusArea, juce::Justification::centredLeft, 2);
        }
    }

    // Footer
    g.setColour (muted);
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::plain)));
    g.drawText ("v" + juce::String (ANTICIP8_VERSION), footerArea, juce::Justification::centredLeft, false);
    g.setColour (dim);
    g.drawText ("FREE AUDIO", footerArea, juce::Justification::centred, false);
    g.setColour (muted);
    g.drawText ("freeanticip8", footerArea, juce::Justification::centredRight, false);
}

void AnticipAudioProcessorEditor::refreshWaveformMode()
{
    const bool fx = proc.apvts.getRawParameterValue ("fx")->load() >= 0.5f;
    const bool trem = proc.apvts.getRawParameterValue ("tremolo")->load() >= 0.5f;
    waveform.setFxMode (fx);
    waveform.setInput (proc.getInputBuffer());
    waveform.setTrim (proc.getTrimStart(), proc.getTrimEnd());
    waveform.setCanDrag (fx && hasResult && ! proc.isRendering());
    waveform.setLengthSeconds (reverseKnob.getSeconds());
    if (trem)
        waveform.setStatusNote ("Grille tremolo sync  -  " + juce::String (proc.getHostBpm(), 0) + " BPM");
    else
        waveform.setStatusNote ({});
    divisionRow.setEnabled (trem);
    divisionRow.setAlpha (trem ? 1.0f : 0.45f);
}

void AnticipAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    auto r = proc.getRenderedAudio();
    hasResult = (r != nullptr);
    waveform.setResult (r);
    waveform.setBusy (proc.isRendering());
    refreshWaveformMode();
    updateInputLabel();
    updateListenState();
    chrome.repaint();
}

void AnticipAudioProcessorEditor::timerCallback()
{
    waveform.setLengthSeconds (reverseKnob.getSeconds());
    waveform.setBusy (proc.isRendering());
    waveform.setPlayhead (proc.isPreviewPlaying() ? proc.getPreviewFraction() : -1.0f);
    refreshWaveformMode();
    updateListenState();
    updateInputLabel();
    chrome.repaint (statusArea);
    chrome.repaint (headerArea);
}

void AnticipAudioProcessorEditor::updateListenState()
{
    listening = proc.isPreviewPlaying();
    listenButton.setSelected (listening);
    listenButton.setLabel (listening ? "STOP" : "ÉCOUTER");
    const bool fx = proc.apvts.getRawParameterValue ("fx")->load() >= 0.5f;
    const bool can = fx ? (hasResult && ! proc.isRendering())
                        : (proc.getInputInfo().valid && ! proc.isRendering());
    listenButton.setEnabled (can);
}

void AnticipAudioProcessorEditor::updateInputLabel()
{
    if (proc.isInputOpen())
    {
        inputButton.setLabel ("ENTRÉE OUVERTE");
        inputButton.setSelected (true);
    }
    else
    {
        inputButton.setLabel ("ENTRÉE FERMÉE");
        inputButton.setSelected (false);
    }
}

void AnticipAudioProcessorEditor::toggleListen()
{
    if (proc.isPreviewPlaying())
        proc.stopPreview();
    else
        proc.startPreview();
    updateListenState();
}

void AnticipAudioProcessorEditor::showHelp()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "ANTICIP8",
        "ENTRÉE OUVERTE enregistre jusqu'à 10 s (Alt-clic = fichier).\n"
        "FX = riser (glisse dans la DAW). BRUT = poignées de coupe.\n"
        "TREMOLO se superpose à tout caractère ; 1/8 - 1/64 = grille tempo.\n"
        "Tire un coin de la fenêtre pour redimensionner - l'UI suit.");
}

void AnticipAudioProcessorEditor::openFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Ouvrir un fichier audio", juce::File(), proc.getWildcard());
    juce::Component::SafePointer<AnticipAudioProcessorEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile())
                                  safe->proc.loadFile (f);
                          });
}

void AnticipAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    hover = false;
    chrome.repaint();
    if (files.isEmpty()) return;
    juce::File f (files[0]);
    if (f.existsAsFile())
        proc.loadFile (f);
    else
        proc.reportError ("Impossible d'accéder au fichier déposé.");
}

void AnticipAudioProcessorEditor::startExternalDrag()
{
    juce::File f;
    const auto res = proc.exportRendered (f);
    if (res.failed())
    {
        proc.reportError (res.getErrorMessage());
        return;
    }
    juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, &waveform);
}
