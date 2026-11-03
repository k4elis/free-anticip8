#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include "DSP/AudioRenderer.h"
#include "Export/WavExporter.h"

class AnticipAudioProcessor : public juce::AudioProcessor,
                              public juce::ChangeBroadcaster,
                              private juce::Timer,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct InputInfo
    {
        bool valid = false;
        juce::String fileName;
        double sampleRate = 0.0;
        double seconds = 0.0;
        int channels = 0;
        bool fromCapture = false;
    };

    AnticipAudioProcessor();
    ~AnticipAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "FREE ANTICIP8"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void loadFile (const juce::File& file);
    InputInfo getInputInfo() const;
    std::shared_ptr<const juce::AudioBuffer<float>> getInputBuffer() const { return inputBuffer; }
    std::shared_ptr<const anticip8::RenderedAudio> getRenderedAudio() const;
    bool isRendering() const { return rendering.load(); }
    juce::String getLastError() const { return lastError; }
    void reportError (const juce::String& e) { lastError = e; sendChangeMessage(); }
    juce::String getWildcard() const { return formatManager.getWildcardForAllFormats(); }

    void startPreview();
    void stopPreview() { previewPlaying.store (false); }
    bool isPreviewPlaying() const { return previewPlaying.load(); }
    float getPreviewFraction() const;

    bool isInputOpen() const { return inputOpen.load(); }
    void setInputOpen (bool open);
    void toggleInputOpen() { setInputOpen (! isInputOpen()); }

    float getTrimStart() const { return trimStart.load(); }
    float getTrimEnd() const { return trimEnd.load(); }
    void setTrimRange (float start01, float end01);

    double getHostBpm() const { return hostBpm.load(); }

    juce::Result exportRendered (juce::File& outFile);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    anticip8::RenderSettings readSettings() const;
    void startRender();
    void renderFinished (uint32_t gen, std::shared_ptr<const anticip8::RenderedAudio> r, juce::String err);
    void inputLoaded (uint32_t token, std::shared_ptr<const juce::AudioBuffer<float>> buf, double sr,
                      juce::File file, juce::String err, bool fromCapture);
    void purgeOldExports();
    void commitCaptureToInput();
    void updateHostBpm();

    static constexpr double kMaxSeconds = 180.0;
    static constexpr double kMaxCaptureSeconds = 10.0;

    juce::AudioFormatManager formatManager;
    juce::File exportDir;

    std::shared_ptr<const juce::AudioBuffer<float>> inputBuffer;
    double inputSampleRate = 44100.0;
    juce::File inputFile;
    juce::String lastError;
    uint32_t loadToken = 0;
    bool inputFromCapture = false;

    std::shared_ptr<std::atomic<uint32_t>> generation = std::make_shared<std::atomic<uint32_t>> (0);
    std::atomic<bool> dirty { false }, rendering { false };
    std::atomic<uint32_t> lastChangeMs { 0 };

    mutable juce::SpinLock resultLock;
    std::shared_ptr<const anticip8::RenderedAudio> result;
    std::atomic<bool> previewPlaying { false };
    std::atomic<double> previewPos { 0.0 };
    std::atomic<double> hostSampleRate { 44100.0 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<int> resultLength { 0 };
    std::atomic<bool> previewRaw { false };

    // live capture (audio thread)
    std::atomic<bool> inputOpen { false };
    juce::SpinLock captureLock;
    juce::AudioBuffer<float> captureBuffer;
    int captureWrite = 0, captureValid = 0;
    int captureCapacity = 0;
    std::atomic<bool> captureDirty { false };
    uint32_t lastCaptureCommitMs = 0;

    std::atomic<float> trimStart { 0.0f }, trimEnd { 1.0f };

    juce::ThreadPool pool;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AnticipAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnticipAudioProcessor)
};
