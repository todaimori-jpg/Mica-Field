// SPDX-License-Identifier: AGPL-3.0-only
#include "Instrument.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void check(bool ok, const char *message)
{
    if (ok)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}
} // namespace
int main()
{
    juce::ScopedJuceInitialiser_GUI framework;
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("MicaChecks", "", false);
    root.createDirectory();
    const auto fixture = root.getChildFile("sample.wav");
    juce::WavAudioFormat format;
    {
        std::unique_ptr<juce::OutputStream> stream = fixture.createOutputStream();
        auto writer = format.createWriterFor(
            stream,
            juce::AudioFormatWriterOptions{}.withSampleRate(48000.).withNumChannels(2).withBitsPerSample(16));
        check(writer != nullptr, "Could not write test fixture");
        juce::AudioBuffer<float> audio(2, 48000);
        for (int i = 0; i < 48000; ++i)
        {
            audio.setSample(0, i, .44f * std::sin(float(i) * .0342467f));
            audio.setSample(1, i, .38f * std::sin(float(i) * .04127f));
        }
        for (int second = 0; second < 10; ++second)
            check(writer->writeFromAudioSampleBuffer(audio, 0, 48000), "Fixture write failed");
    }
    mica::Instrument instrument;
    instrument.prepareToPlay(48000., 256);
    check(instrument.readSample(0, fixture).wasOk(), "WAV import failed");
    check(instrument.sample(0)->data.size() == mica::maxClipFrames, "Eight-second sample limit failed");
    instrument.change(mica::aType, mica::fromChoice(int(mica::EngineType::granular), 6));
    const int route = instrument.addRoute(int(mica::Source::random), mica::aShape);
    check(route >= 0, "Route allocation failed");
    instrument.setRoute(route, {int(mica::Source::random), mica::aShape, -.41f});
    const auto patch = root.getChildFile("Saved.mica");
    check(instrument.savePatch(patch).wasOk(), "Patch save failed");
    juce::MemoryBlock state;
    instrument.getStateInformation(state);
    instrument.selectFactory(10);
    check(instrument.sample(0) == nullptr, "Factory selection retained custom clip");
    check(instrument.openPatch(patch).wasOk(), "Patch reopen failed");
    check(instrument.sample(0) != nullptr && instrument.patchName() == "Saved",
          "Patch name or clip restore failed");
    check(std::abs(instrument.route(route).amount + .41f) < .0001f, "Route restore failed");
    instrument.selectFactory(10);
    instrument.setStateInformation(state.getData(), int(state.getSize()));
    check(instrument.sample(0) != nullptr, "Host state restore failed");
    instrument.setStateInformation(state.getData(), int(state.getSize() - 4));
    check(instrument.patchName() == "Saved" && instrument.sample(0) != nullptr,
          "Invalid state was partially applied");
    const auto wav = root.getChildFile("recording.wav");
    check(instrument.startRecording(wav).wasOk(), "Recording failed to start");
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    midi.ensureSize(4096);
    double level = 0.;
    for (int i = 0; i < 200; ++i)
    {
        if (i == 0)
        {
            midi.addEvent(juce::MidiMessage::noteOn(1, 48, .82f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 55, .77f), 40);
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, .83f), 120);
        }
        if (i == 150)
            midi.addEvent(juce::MidiMessage::allNotesOff(1), 11);
        instrument.processBlock(audio, midi);
        level += audio.getRMSLevel(0, 0, 256);
    }
    check(instrument.stopRecording().wasOk() && level > .08, "Recording did not contain synth audio");
    std::unique_ptr<juce::AudioFormatReader> reader(
        format.createReaderFor(wav.createInputStream().release(), true));
    check(reader != nullptr && reader->bitsPerSample == 24 && reader->numChannels == 2 &&
              reader->lengthInSamples == 200 * 256,
          "Recorded WAV format or length failed");
    instrument.selectFactory(8);
    instrument.toggleSequencer();
    for (int i = 0; i < 90; ++i)
        instrument.processBlock(audio, midi);
    check(instrument.step.load() > 0, "Sequencer failed to advance");
    instrument.requestPanic();
    instrument.processBlock(audio, midi);
    check(instrument.step.load() == -1 && audio.getMagnitude(0, 256) == 0.f, "Panic failed to clear audio");
    std::cout << "Native sample, patch, host state, WAV recording, sequence and panic checks passed.\n";
    // These files are generated fixtures in a uniquely named system temporary directory.
    reader.reset();
    root.deleteRecursively();
}
