#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace anticip8;

static const char* const kParamIDs[] = {
    "length", "fx", "character", "tremolo", "division", "rate", "depth", "damping"
};

juce::AudioProcessorValueTreeState::ParameterLayout AnticipAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    // 0.5 .. 5.0 s  - affiche en % dans l'UI (50% ~ 2.3 s)
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "length", 1 }, "Durée",
            NormalisableRange<float> (0.5f, 5.0f, 0.01f, 0.6f), 2.3f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " s"; })
                .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); })));

    l.add (std::make_unique<AudioParameterBool> (ParameterID { "fx", 1 }, "FX / BRUT", true));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "character", 1 }, "Caractère",
            StringArray { "CLASSIQUE", "TÉLÉPHONE", "CHORUS" }, 0));

    l.add (std::make_unique<AudioParameterBool> (ParameterID { "tremolo", 1 }, "Tremolo", false));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Grille tremolo",
            StringArray { "1/8", "1/16", "1/32", "1/64" }, 0));

    // Params host decoratifs (toujours a fond dans le rendu).
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "rate", 1 }, "Vitesse mod",
            NormalisableRange<float> (0.1f, 12.0f, 0.01f, 0.5f), 12.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "depth", 1 }, "Profondeur",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "damping", 1 }, "Amortissement",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    return l;
}

AnticipAudioProcessor::AnticipAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Entrée",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Sortie", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()),
      pool (1)
{
    formatManager.registerBasicFormats();
    exportDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("FreeAnticip8");
    purgeOldExports();
    for (auto* id : kParamIDs)
        apvts.addParameterListener (id, this);
    startTimerHz (15);
}

AnticipAudioProcessor::~AnticipAudioProcessor()
{
    stopTimer();
    ++(*generation);
    pool.removeAllJobs (true, 5000);
    for (auto* id : kParamIDs)
        apvts.removeParameterListener (id, this);
}

void AnticipAudioProcessor::purgeOldExports()
{
    if (! exportDir.isDirectory()) return;
    const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::days (7);
    for (auto& f : exportDir.findChildFiles (juce::File::findFiles, false, "*.wav"))
        if (f.getLastModificationTime() < cutoff)
            f.deleteFile();
}

bool AnticipAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void AnticipAudioProcessor::prepareToPlay (double sampleRate, int)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    hostSampleRate.store (sr);
    captureCapacity = (int) std::lround (kMaxCaptureSeconds * sr);
    const juce::SpinLock::ScopedLockType lock (captureLock);
    captureBuffer.setSize (2, captureCapacity, false, true, true);
    captureBuffer.clear();
    captureWrite = captureValid = 0;
}

void AnticipAudioProcessor::updateHostBpm()
{
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 1.0) bpm = *b;
    hostBpm.store (bpm);
}

static inline float cubicAt (const float* s, int i, float f, int len)
{
    auto at = [&] (int k) { return s[juce::jlimit (0, len - 1, k)]; };
    const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

void AnticipAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateHostBpm();

    // live capture while INPUT OPEN (pass-through still happens)
    if (inputOpen.load (std::memory_order_relaxed) && ! previewPlaying.load (std::memory_order_relaxed))
    {
        const juce::SpinLock::ScopedTryLockType lock (captureLock);
        if (lock.isLocked() && captureCapacity > 0)
        {
            const int n = buffer.getNumSamples();
            const int ch = juce::jmin (2, buffer.getNumChannels());
            for (int i = 0; i < n; ++i)
            {
                for (int c = 0; c < ch; ++c)
                    captureBuffer.setSample (c, captureWrite, buffer.getSample (c, i));
                if (ch == 1)
                    captureBuffer.setSample (1, captureWrite, buffer.getSample (0, i));
                captureWrite = (captureWrite + 1) % captureCapacity;
                captureValid = juce::jmin (captureCapacity, captureValid + 1);
            }
            captureDirty.store (true, std::memory_order_relaxed);
            if (captureValid >= captureCapacity)
                inputOpen.store (false, std::memory_order_relaxed); // auto-close at 10 s
        }
    }

    if (! previewPlaying.load (std::memory_order_relaxed))
        return;

    const bool raw = previewRaw.load (std::memory_order_relaxed)
                     || apvts.getRawParameterValue ("fx")->load() < 0.5f;

    if (raw)
    {
        auto in = inputBuffer;
        if (in == nullptr) return;
        const int srcLen = in->getNumSamples();
        const int srcCh = in->getNumChannels();
        const float t0 = trimStart.load(), t1 = trimEnd.load();
        const int s0 = juce::jlimit (0, srcLen, (int) std::lround (juce::jmin (t0, t1) * srcLen));
        const int s1 = juce::jlimit (0, srcLen, (int) std::lround (juce::jmax (t0, t1) * srcLen));
        const int seg = juce::jmax (1, s1 - s0);
        const double ratio = inputSampleRate / hostSampleRate.load (std::memory_order_relaxed);
        const int n = buffer.getNumSamples();
        const int outCh = juce::jmin (2, buffer.getNumChannels());
        const float* srcp[2] = { in->getReadPointer (0), in->getReadPointer (srcCh > 1 ? 1 : 0) };
        float* out[2] = { buffer.getWritePointer (0), outCh > 1 ? buffer.getWritePointer (1) : nullptr };

        double pos = previewPos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            if (pos >= (double) seg) { previewPlaying.store (false); break; }
            const int idx = s0 + (int) pos;
            const float fr = (float) (pos - (int) pos);
            for (int c = 0; c < outCh; ++c)
                out[c][i] += cubicAt (srcp[c], idx, fr, srcLen);
            pos += ratio;
        }
        previewPos.store (pos, std::memory_order_relaxed);
        resultLength.store (seg);
        return;
    }

    const juce::SpinLock::ScopedTryLockType lock (resultLock);
    if (! lock.isLocked() || result == nullptr)
        return;

    const auto& src = result->audio;
    const int srcLen = src.getNumSamples();
    const int srcCh = src.getNumChannels();
    const double ratio = result->sampleRate / hostSampleRate.load (std::memory_order_relaxed);
    const int n = buffer.getNumSamples();
    const int outCh = juce::jmin (2, buffer.getNumChannels());

    const float* in[2] = { src.getReadPointer (0), src.getReadPointer (srcCh > 1 ? 1 : 0) };
    float* out[2] = { buffer.getWritePointer (0), outCh > 1 ? buffer.getWritePointer (1) : nullptr };

    double pos = previewPos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        if (pos >= (double) srcLen)
        {
            previewPlaying.store (false);
            break;
        }
        const int idx = (int) pos;
        const float fr = (float) (pos - idx);
        for (int c = 0; c < outCh; ++c)
            out[c][i] += cubicAt (in[c], idx, fr, srcLen);
        pos += ratio;
    }
    previewPos.store (pos, std::memory_order_relaxed);
}

void AnticipAudioProcessor::parameterChanged (const juce::String&, float)
{
    lastChangeMs.store (juce::Time::getMillisecondCounter());
    dirty.store (true);
}

void AnticipAudioProcessor::timerCallback()
{
    if (captureDirty.load() && inputOpen.load())
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastCaptureCommitMs > 280u)
        {
            lastCaptureCommitMs = now;
            commitCaptureToInput();
        }
    }
    else if (captureDirty.exchange (false) && ! inputOpen.load())
    {
        commitCaptureToInput();
    }

    if (dirty.load() && (juce::Time::getMillisecondCounter() - lastChangeMs.load()) > 150u)
    {
        dirty.store (false);
        startRender();
    }
}

void AnticipAudioProcessor::commitCaptureToInput()
{
    juce::AudioBuffer<float> copy;
    int valid = 0;
    {
        const juce::SpinLock::ScopedLockType lock (captureLock);
        valid = captureValid;
        if (valid < (int) (0.05 * hostSampleRate.load()))
            return;
        copy.setSize (2, valid);
        // unwrap ring: oldest = write - valid
        int start = captureWrite - valid;
        if (start < 0) start += captureCapacity;
        for (int i = 0; i < valid; ++i)
        {
            const int idx = (start + i) % captureCapacity;
            copy.setSample (0, i, captureBuffer.getSample (0, idx));
            copy.setSample (1, i, captureBuffer.getSample (1, idx));
        }
        captureDirty.store (false);
    }

    auto buf = std::make_shared<juce::AudioBuffer<float>> (std::move (copy));
    inputLoaded (++loadToken, buf, hostSampleRate.load(), juce::File(), {}, true);
}

void AnticipAudioProcessor::setInputOpen (bool open)
{
    if (open)
    {
        const juce::SpinLock::ScopedLockType lock (captureLock);
        captureBuffer.clear();
        captureWrite = captureValid = 0;
        captureDirty.store (false);
        inputOpen.store (true);
        lastError.clear();
        sendChangeMessage();
    }
    else
    {
        inputOpen.store (false);
        if (captureDirty.load() || captureValid > 0)
            commitCaptureToInput();
        sendChangeMessage();
    }
}

void AnticipAudioProcessor::setTrimRange (float start01, float end01)
{
    float a = juce::jlimit (0.0f, 1.0f, start01);
    float b = juce::jlimit (0.0f, 1.0f, end01);
    if (b - a < 0.02f) b = juce::jmin (1.0f, a + 0.02f);
    trimStart.store (a);
    trimEnd.store (b);
    lastChangeMs.store (juce::Time::getMillisecondCounter());
    dirty.store (true);
    sendChangeMessage();
}

RenderSettings AnticipAudioProcessor::readSettings() const
{
    auto choiceIndex = [this] (const char* id, int maxIndex) -> int
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            return juce::jlimit (0, maxIndex, p->getIndex());
        // raw is normalised 0..1 — convert properly (never roundToInt the raw value)
        if (auto* r = apvts.getParameter (id))
            return juce::jlimit (0, maxIndex, juce::roundToInt (r->convertFrom0to1 (r->getValue())));
        return 0;
    };

    RenderSettings s;
    s.lengthSeconds = apvts.getRawParameterValue ("length")->load();
    s.fx            = apvts.getRawParameterValue ("fx")->load() >= 0.5f;
    s.character     = choiceIndex ("character", 2);
    s.tremolo       = apvts.getRawParameterValue ("tremolo")->load() >= 0.5f;
    s.tremoloHz     = CharacterProcessor::divisionToHz (choiceIndex ("division", 3), hostBpm.load());
    s.trimStart     = trimStart.load();
    s.trimEnd        = trimEnd.load();
    return s;
}

void AnticipAudioProcessor::startRender()
{
    if (inputBuffer == nullptr)
        return;

    const uint32_t gen = ++(*generation);
    const auto settings = readSettings();
    const auto in = inputBuffer;
    const double sr = inputSampleRate;
    const auto name = inputFile.existsAsFile() ? inputFile.getFileNameWithoutExtension()
                                               : juce::String ("capture");
    auto genCounter = generation;
    juce::WeakReference<AnticipAudioProcessor> weak (this);

    rendering.store (true);
    sendChangeMessage();

    pool.addJob ([=]
    {
        juce::ScopedNoDenormals nd;
        const auto cancel = [genCounter, gen] { return genCounter->load() != gen; };
        juce::String err;
        auto res = AudioRenderer::render (*in, sr, settings, cancel, err);
        if (res == nullptr && err.isEmpty())
            return;
        if (res != nullptr)
            res->sourceName = name;
        std::shared_ptr<const RenderedAudio> constRes = res;
        juce::MessageManager::callAsync ([weak, gen, constRes, err]
        {
            if (auto* p = weak.get())
                p->renderFinished (gen, constRes, err);
        });
    });
}

void AnticipAudioProcessor::renderFinished (uint32_t gen, std::shared_ptr<const RenderedAudio> r, juce::String err)
{
    if (gen != generation->load())
        return;

    if (r != nullptr)
    {
        previewPlaying.store (false);
        previewPos.store (0.0);
        resultLength.store (r->audio.getNumSamples());
        {
            const juce::SpinLock::ScopedLockType l (resultLock);
            result.swap (r);
        }
        lastError.clear();
    }
    else
    {
        lastError = err;
    }
    rendering.store (false);
    sendChangeMessage();
}

void AnticipAudioProcessor::loadFile (const juce::File& file)
{
    inputOpen.store (false);
    {
        const juce::SpinLock::ScopedLockType lock (captureLock);
        captureBuffer.clear();
        captureWrite = captureValid = 0;
        captureDirty.store (false);
    }
    const uint32_t token = ++loadToken;
    juce::WeakReference<AnticipAudioProcessor> weak (this);
    lastError.clear();
    rendering.store (true);
    sendChangeMessage();

    pool.addJob ([=]
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        juce::String err;
        std::shared_ptr<const juce::AudioBuffer<float>> data;
        double sr = 0.0;

        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr)
            err = "Fichier audio illisible ou non supporte: " + file.getFileName();
        else if (reader->lengthInSamples <= 0 || reader->numChannels == 0)
            err = "Le fichier audio est vide: " + file.getFileName();
        else if ((double) reader->lengthInSamples > reader->sampleRate * kMaxSeconds)
            err = "Fichier trop long (max " + juce::String ((int) kMaxSeconds) + " s): " + file.getFileName();
        else
        {
            const int nch = juce::jmin (2, (int) reader->numChannels);
            auto buf = std::make_shared<juce::AudioBuffer<float>> (nch, (int) reader->lengthInSamples);
            if (reader->read (buf.get(), 0, (int) reader->lengthInSamples, 0, true, nch > 1))
            {
                data = buf;
                sr = reader->sampleRate;
            }
            else
                err = "Impossible de lire les donnees audio: " + file.getFileName();
        }

        juce::MessageManager::callAsync ([weak, token, data, sr, file, err]
        {
            if (auto* p = weak.get())
                p->inputLoaded (token, data, sr, file, err, false);
        });
    });
}

void AnticipAudioProcessor::inputLoaded (uint32_t token, std::shared_ptr<const juce::AudioBuffer<float>> buf,
                                         double sr, juce::File file, juce::String err, bool fromCapture)
{
    if (token != loadToken)
        return;

    if (buf == nullptr)
    {
        lastError = err;
        rendering.store (false);
        sendChangeMessage();
        return;
    }

    inputBuffer = buf;
    inputSampleRate = sr;
    inputFile = file;
    inputFromCapture = fromCapture;
    if (file.existsAsFile())
        apvts.state.setProperty ("inputFile", file.getFullPathName(), nullptr);
    trimStart.store (0.0f);
    trimEnd.store (1.0f);
    previewPlaying.store (false);
    startRender();
    sendChangeMessage();
}

AnticipAudioProcessor::InputInfo AnticipAudioProcessor::getInputInfo() const
{
    InputInfo i;
    if (inputBuffer != nullptr)
    {
        i.valid = true;
        i.fileName = inputFromCapture ? juce::String ("capture")
                                      : inputFile.getFileName();
        i.sampleRate = inputSampleRate;
        i.seconds = (double) inputBuffer->getNumSamples() / inputSampleRate;
        i.channels = inputBuffer->getNumChannels();
        i.fromCapture = inputFromCapture;
    }
    return i;
}

std::shared_ptr<const RenderedAudio> AnticipAudioProcessor::getRenderedAudio() const
{
    const juce::SpinLock::ScopedLockType l (resultLock);
    return result;
}

void AnticipAudioProcessor::startPreview()
{
    const bool fx = apvts.getRawParameterValue ("fx")->load() >= 0.5f;
    previewRaw.store (! fx);
    if (fx)
    {
        if (getRenderedAudio() == nullptr || rendering.load())
            return;
        if (auto r = getRenderedAudio())
            resultLength.store (r->audio.getNumSamples());
    }
    else
    {
        if (inputBuffer == nullptr) return;
        const float t0 = trimStart.load(), t1 = trimEnd.load();
        const int n = inputBuffer->getNumSamples();
        const int s0 = (int) std::lround (juce::jmin (t0, t1) * n);
        const int s1 = (int) std::lround (juce::jmax (t0, t1) * n);
        resultLength.store (juce::jmax (1, s1 - s0));
    }
    previewPos.store (0.0);
    previewPlaying.store (true);
}

float AnticipAudioProcessor::getPreviewFraction() const
{
    const int len = resultLength.load();
    return len > 0 ? (float) (previewPos.load() / (double) len) : 0.0f;
}

juce::Result AnticipAudioProcessor::exportRendered (juce::File& outFile)
{
    auto r = getRenderedAudio();
    if (r == nullptr)
        return juce::Result::fail ("Rien n'a encore ete rendu.");
    if (! r->settings.fx)
        return juce::Result::fail ("Passe en FX pour glisser le riser dans ta DAW.");

    static const char* const names[] = { "CLASSIQUE", "TELEPHONE", "CHORUS" }; // noms fichier ASCII
    auto base = juce::File::createLegalFileName (r->sourceName.isEmpty() ? juce::String ("render") : r->sourceName)
                + "_ANTICIP8_" + juce::String (names[juce::jlimit (0, 2, r->settings.character)]);
    if (r->settings.tremolo) base += "_TREMOLO";

    exportDir.createDirectory();
    outFile = exportDir.getNonexistentChildFile (base, ".wav", false);
    return WavExporter::writeWav24 (r->audio, r->sampleRate, outFile);
}

void AnticipAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("trimStart", (double) trimStart.load(), nullptr);
    state.setProperty ("trimEnd", (double) trimEnd.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void AnticipAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            // migrate old 4-character layout (TREMOLO as index 3)
            if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("character")))
            {
                // if legacy value somehow stored > 2, clamp handled by choice size
                juce::ignoreUnused (ch);
            }
            trimStart.store ((float) (double) apvts.state.getProperty ("trimStart", 0.0));
            trimEnd.store ((float) (double) apvts.state.getProperty ("trimEnd", 1.0));
            const auto path = apvts.state.getProperty ("inputFile").toString();
            if (path.isNotEmpty())
            {
                juce::File f (path);
                if (f.existsAsFile())
                    loadFile (f);
                else
                    reportError ("Fichier d'entree introuvable: " + path);
            }
        }
    }
}

juce::AudioProcessorEditor* AnticipAudioProcessor::createEditor() { return new AnticipAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AnticipAudioProcessor(); }
