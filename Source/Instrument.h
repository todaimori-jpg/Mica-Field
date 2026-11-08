// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Dsp.h"
#include <atomic>
#include <juce_audio_utils/juce_audio_utils.h>
#include <mutex>

namespace mica
{
class Instrument final : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener
{
  public:
    Instrument();
    ~Instrument() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    using AudioProcessor::processBlock;
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Mica Field"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 90.; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    float value(int) const;
    void setValue(int, float);
    void beginGesture(int);
    void endGesture(int);
    void change(int, float);
    Patch readPatch() const;
    void selectFactory(int);
    int factoryIndex() const { return presetIndex.load(); }
    juce::String patchName() const;
    bool isEdited() const { return dirty.load(); }
    Route route(int) const;
    void setRoute(int, Route);
    int addRoute(int, int);
    juce::Result readSample(int, const juce::File &);
    void clearSample(int);
    std::shared_ptr<const SampleClip> sample(int) const;
    juce::Result savePatch(const juce::File &);
    juce::Result openPatch(const juce::File &);
    void toggleSequencer();
    bool sequencerOn() const { return runSequence.load(); }
    void randomiseSteps();
    void requestPanic();
    juce::Result startRecording(const juce::File &);
    juce::Result stopRecording();
    bool isRecording() const { return recordingActive.load(); }
    double recordedSeconds() const;
    void useAudioSettings();
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }
    juce::MidiKeyboardState keyboard;
    std::atomic<float> peakL{0.f}, peakR{0.f}, cpuLoad{0.f};
    std::array<std::atomic<float>, int(Source::count)> sources{};
    std::array<std::atomic<float>, 96> scope{};
    std::atomic<int> playingVoices{0}, step{-1};

  private:
    struct SampleSlot
    {
        const SampleClip *pin();
        void unpin();
        void replace(std::shared_ptr<const SampleClip>);
        std::shared_ptr<const SampleClip> copy() const;
        mutable std::mutex mutex;
        std::shared_ptr<const SampleClip> current;
        std::vector<std::shared_ptr<const SampleClip>> retired;
        std::atomic<const SampleClip *> published{nullptr}, reading{nullptr};
    };
    struct Recording
    {
        explicit Recording(const juce::File &file) : temporary(file) {}
        juce::TemporaryFile temporary;
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void readMidi(const juce::MidiMessage &);
    void apply(const Patch &, const juce::String &, int);
    bool decodeState(const void *, int);
    void encodeState(juce::MemoryBlock &, const juce::String &);
    void publish(int, std::shared_ptr<SampleClip>);
    void parameterChanged(const juce::String &, float) override;
    juce::AudioProcessorValueTreeState state;
    std::array<juce::RangedAudioParameter *, paramCount> automatable{};
    std::array<std::atomic<float> *, paramCount> raw{};
    std::array<std::atomic<std::uint32_t>, maxRoutes> routeSlots{};
    std::array<SampleSlot, 2> sampleSlots;
    std::atomic<std::uint64_t> revision{0};
    mutable std::mutex nameMutex;
    juce::String name;
    std::atomic<int> presetIndex{0};
    std::atomic<bool> dirty{false}, runSequence{false}, panicPending{false};
    Synth synth;
    double deviceRate = 48000.;
    std::atomic<double> captureRate{0.};
    std::atomic<bool> recordingActive{false};
    std::atomic<std::uint64_t> captureFrames{0};
    std::atomic<std::uint32_t> overruns{0};
    juce::TimeSliceThread diskThread{"Mica Field WAV Writer"};
    juce::SpinLock writerLock;
    std::unique_ptr<Recording> recording;
    juce::AudioFormatManager audioFormats;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Instrument)
};
} // namespace mica
