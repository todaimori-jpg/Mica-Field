// SPDX-License-Identifier: AGPL-3.0-only
#include "Instrument.h"
#include "Editor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace mica
{
namespace
{
constexpr int patchMagic = 0x3143494d, patchVersion = 1, maxPatchSize = 2 * 1024 * 1024;
juce::String text(std::string_view s)
{
    return juce::String::fromUTF8(s.data(), int(s.size()));
}
} // namespace
juce::AudioProcessorValueTreeState::ParameterLayout Instrument::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto &info : parameters())
    {
        const float interval = info.choices > 1 ? 1.f / float(info.choices - 1) : 0.f;
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(juce::String(info.id), 1), juce::String(info.name),
            juce::NormalisableRange<float>(0.f, 1.f, interval), info.initial));
    }
    return layout;
}
Instrument::Instrument()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "MicaField", makeParameters())
{
    for (int i = 0; i < paramCount; ++i)
    {
        const juce::String id(parameters()[std::size_t(i)].id);
        raw[std::size_t(i)] = state.getRawParameterValue(id);
        automatable[std::size_t(i)] = state.getParameter(id);
        state.addParameterListener(id, this);
    }
    for (auto &s : sources)
        s.store(0.f);
    for (auto &s : scope)
        s.store(0.f);
    audioFormats.registerBasicFormats();
    selectFactory(0);
}
Instrument::~Instrument()
{
    stopRecording();
    diskThread.stopThread(3000);
    for (const auto &info : parameters())
        state.removeParameterListener(juce::String(info.id), this);
}
bool Instrument::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    const auto channels = layouts.getMainOutputChannelSet();
    return channels == juce::AudioChannelSet::stereo() || channels == juce::AudioChannelSet::mono();
}
void Instrument::prepareToPlay(double rate, int)
{
    deviceRate = rate;
    keyboard.reset();
    synth.prepare(rate);
    captureRate.store(rate);
}
void Instrument::releaseResources()
{
    captureRate.store(0.);
    stopRecording();
    runSequence.store(false);
    keyboard.reset();
    peakL.store(0.f);
    peakR.store(0.f);
    cpuLoad.store(0.f);
    playingVoices.store(0);
}
void Instrument::processBlock(juce::AudioBuffer<float> &audio, juce::MidiBuffer &midi)
{
    const auto start = std::chrono::steady_clock::now();
    juce::ScopedNoDenormals noDenormals;
    const int frames = audio.getNumSamples();
    if (frames == 0 || audio.getNumChannels() == 0)
        return;
    keyboard.processNextMidiBuffer(midi, 0, frames, true);
    synth.beginBlock(readPatch(), {sampleSlots[0].pin(), sampleSlots[1].pin()},
                     runSequence.load(std::memory_order_relaxed));
    if (panicPending.exchange(false, std::memory_order_relaxed))
        synth.panic();
    auto *left = audio.getWritePointer(0);
    auto *right = audio.getNumChannels() > 1 ? audio.getWritePointer(1) : nullptr;
    int offset = 0;
    for (const auto metadata : midi)
    {
        const int next = std::clamp(metadata.samplePosition, offset, frames);
        if (next > offset)
            synth.render(left + offset, right != nullptr ? right + offset : nullptr, next - offset);
        offset = next;
        readMidi(metadata.getMessage());
    }
    if (offset < frames)
        synth.render(left + offset, right != nullptr ? right + offset : nullptr, frames - offset);
    for (auto &slot : sampleSlots)
        slot.unpin();
    midi.clear();
    peakL.store(std::max(synth.peakLeft(), peakL.load() * .94f), std::memory_order_relaxed);
    peakR.store(std::max(synth.peakRight(), peakR.load() * .94f), std::memory_order_relaxed);
    playingVoices.store(synth.voicesPlaying(), std::memory_order_relaxed);
    step.store(synth.currentStep(), std::memory_order_relaxed);
    for (std::size_t i = 0; i < sources.size(); ++i)
        sources[i].store(synth.sourceLevels()[i], std::memory_order_relaxed);
    for (std::size_t i = 0; i < scope.size(); ++i)
    {
        const auto at = i * std::size_t(frames) / scope.size();
        scope[i].store(right != nullptr ? (left[at] + right[at]) * .5f : left[at], std::memory_order_relaxed);
    }
    if (recordingActive.load(std::memory_order_relaxed))
    {
        const juce::SpinLock::ScopedTryLockType lock(writerLock);
        if (lock.isLocked() && recording != nullptr)
        {
            const float *channels[]{left, right != nullptr ? right : left};
            if (recording->writer->write(channels, frames))
                captureFrames.fetch_add(std::uint64_t(frames));
            else
                overruns.fetch_add(1);
        }
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const float old = cpuLoad.load();
    cpuLoad.store(old + .045f * (float(elapsed * deviceRate / double(frames)) - old));
}
void Instrument::readMidi(const juce::MidiMessage &msg)
{
    const int channel = msg.getChannel();
    if (msg.isNoteOn())
        synth.noteOn(channel, msg.getNoteNumber(), msg.getFloatVelocity());
    else if (msg.isNoteOff())
        synth.noteOff(channel, msg.getNoteNumber());
    else if (msg.isPitchWheel())
        synth.pitchWheel(channel, float(msg.getPitchWheelValue() - 8192) / 8192.f);
    else if (msg.isSustainPedalOn())
        synth.sustainPedal(channel, true);
    else if (msg.isSustainPedalOff())
        synth.sustainPedal(channel, false);
    else if (msg.isAllSoundOff())
        synth.panic();
    else if (msg.isAllNotesOff())
        synth.allNotesOff(channel);
    else if (msg.isControllerOfType(1))
        synth.modWheel(channel, float(msg.getControllerValue()) / 127.f);
}
float Instrument::value(int p) const
{
    return raw[std::size_t(p)]->load(std::memory_order_relaxed);
}
void Instrument::setValue(int p, float v)
{
    automatable[std::size_t(p)]->setValueNotifyingHost(std::clamp(v, 0.f, 1.f));
}
void Instrument::beginGesture(int p)
{
    automatable[std::size_t(p)]->beginChangeGesture();
}
void Instrument::endGesture(int p)
{
    automatable[std::size_t(p)]->endChangeGesture();
}
void Instrument::change(int p, float v)
{
    beginGesture(p);
    setValue(p, v);
    endGesture(p);
}
void Instrument::parameterChanged(const juce::String &, float)
{
    dirty.store(true);
}
Patch Instrument::readPatch() const
{
    Patch patch;
    for (int i = 0; i < paramCount; ++i)
        patch.values[std::size_t(i)] = value(i);
    for (int i = 0; i < maxRoutes; ++i)
        patch.routes[std::size_t(i)] = route(i);
    return patch;
}
void Instrument::apply(const Patch &patch, const juce::String &label, int index)
{
    runSequence.store(false);
    panicPending.store(true);
    for (int i = 0; i < paramCount; ++i)
        setValue(i, patch.values[std::size_t(i)]);
    for (int i = 0; i < maxRoutes; ++i)
        routeSlots[std::size_t(i)].store(packRoute(patch.routes[std::size_t(i)]));
    {
        const std::scoped_lock lock(nameMutex);
        name = label;
    }
    presetIndex.store(index);
    dirty.store(false);
}
void Instrument::selectFactory(int index)
{
    const auto &bank = factory();
    index = (index % int(bank.size()) + int(bank.size())) % int(bank.size());
    for (auto &slot : sampleSlots)
        slot.replace(nullptr);
    apply(bank[std::size_t(index)].patch, text(bank[std::size_t(index)].name), index);
}
juce::String Instrument::patchName() const
{
    const std::scoped_lock lock(nameMutex);
    return name;
}
Route Instrument::route(int i) const
{
    return unpackRoute(routeSlots[std::size_t(i)].load());
}
void Instrument::setRoute(int i, Route route)
{
    routeSlots[std::size_t(i)].store(packRoute(route));
    dirty.store(true);
}
int Instrument::addRoute(int source, int target)
{
    int empty = -1;
    for (int i = 0; i < maxRoutes; ++i)
    {
        const auto r = route(i);
        if (r.source == source && r.target == target)
            return i;
        if (r.source < 0 && empty < 0)
            empty = i;
    }
    if (empty >= 0)
        setRoute(empty, {source, target, .25f});
    return empty;
}
const SampleClip *Instrument::SampleSlot::pin()
{
    const SampleClip *clip = nullptr;
    do
    {
        clip = published.load();
        reading.store(clip);
    } while (clip != published.load());
    return clip;
}
void Instrument::SampleSlot::unpin()
{
    reading.store(nullptr);
}
void Instrument::SampleSlot::replace(std::shared_ptr<const SampleClip> clip)
{
    const std::scoped_lock lock(mutex);
    auto previous = std::move(current);
    current = std::move(clip);
    published.store(current.get());
    const auto *pinned = reading.load();
    if (previous != nullptr && previous.get() == pinned)
        retired.push_back(std::move(previous));
    retired.erase(std::remove_if(retired.begin(), retired.end(),
                                 [pinned](const auto &item) { return item.get() != pinned; }),
                  retired.end());
}
std::shared_ptr<const SampleClip> Instrument::SampleSlot::copy() const
{
    const std::scoped_lock lock(mutex);
    return current;
}
void Instrument::publish(int slot, std::shared_ptr<SampleClip> clip)
{
    if (clip != nullptr)
        clip->revision = revision.fetch_add(1) + 1;
    sampleSlots[std::size_t(slot)].replace(std::move(clip));
}
std::shared_ptr<const SampleClip> Instrument::sample(int slot) const
{
    return sampleSlots[std::size_t(slot)].copy();
}
void Instrument::clearSample(int slot)
{
    publish(slot, nullptr);
    dirty.store(true);
}
juce::Result Instrument::readSample(int slot, const juce::File &file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(audioFormats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples < 4 || !std::isfinite(reader->sampleRate) ||
        reader->sampleRate < 8000 || reader->sampleRate > 384000)
        return juce::Result::fail("Choose a supported audio file with at least four frames.");
    auto clip = std::make_shared<SampleClip>();
    clip->name = file.getFileNameWithoutExtension().substring(0, 120).toStdString();
    const double step = reader->sampleRate / double(storageSampleRate);
    const int count = std::clamp(int(std::min(double(maxClipFrames), double(reader->lengthInSamples) / step)),
                                 3, maxClipFrames);
    clip->data.resize(std::size_t(count));
    constexpr int chunkSize = 8192;
    juce::AudioBuffer<float> chunk(2, chunkSize);
    juce::int64 chunkStart = -chunkSize;
    const bool stereo = reader->numChannels > 1;
    for (int i = 0; i < count; ++i)
    {
        const double position = double(i) * step;
        const auto first = juce::int64(position);
        if (first < chunkStart || first + 1 >= chunkStart + chunkSize)
        {
            chunkStart = first;
            chunk.clear();
            if (!reader->read(&chunk, 0, chunkSize, chunkStart, true, stereo))
                return juce::Result::fail("The audio file could not be read.");
        }
        const int j = int(first - chunkStart);
        const float alpha = float(position - double(first));
        const auto *left = chunk.getReadPointer(0);
        const auto *right = chunk.getReadPointer(stereo ? 1 : 0);
        const float a = (left[j] + right[j]) * .5f, b = (left[j + 1] + right[j + 1]) * .5f;
        const float v = a + (b - a) * alpha;
        clip->data[std::size_t(i)] = std::isfinite(v) ? std::clamp(v, -1.f, 1.f) : 0.f;
    }
    clip->buildPreview();
    publish(slot, std::move(clip));
    dirty.store(true);
    return juce::Result::ok();
}
void Instrument::encodeState(juce::MemoryBlock &output, const juce::String &label)
{
    output.reset();
    juce::MemoryOutputStream out(output, false);
    out.writeInt(patchMagic);
    out.writeInt(patchVersion);
    out.writeString(label.substring(0, 120));
    const auto patch = readPatch();
    for (auto v : patch.values)
        out.writeFloat(v);
    for (auto r : patch.routes)
        out.writeInt(int(packRoute(r)));
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto clip = sample(slot);
        out.writeInt(clip != nullptr ? int(clip->data.size()) : 0);
        if (clip == nullptr)
            continue;
        out.writeString(juce::String(clip->name).substring(0, 120));
        for (auto v : clip->data)
            out.writeShort(short(std::lround(std::clamp(v, -1.f, 1.f) * 32767.f)));
    }
}
bool Instrument::decodeState(const void *data, int bytes)
{
    if (data == nullptr || bytes < 16 || bytes > maxPatchSize)
        return false;
    juce::MemoryInputStream in(data, std::size_t(bytes), false);
    if (in.readInt() != patchMagic || in.readInt() != patchVersion)
        return false;
    const auto label = in.readString();
    if (label.isEmpty() || label.length() > 120 ||
        in.getNumBytesRemaining() < (paramCount + maxRoutes + 2) * 4)
        return false;
    Patch patch;
    for (auto &value : patch.values)
    {
        value = in.readFloat();
        if (!std::isfinite(value) || value < 0.f || value > 1.f)
            return false;
    }
    for (auto &route : patch.routes)
        route = unpackRoute(std::uint32_t(in.readInt()));
    std::array<std::shared_ptr<SampleClip>, 2> loaded;
    for (auto &clip : loaded)
    {
        if (in.getNumBytesRemaining() < 4)
            return false;
        const int size = in.readInt();
        if (size == 0)
            continue;
        if (size < 3 || size > maxClipFrames)
            return false;
        const auto name = in.readString();
        if (name.length() > 120 || in.getNumBytesRemaining() < size * 2)
            return false;
        clip = std::make_shared<SampleClip>();
        clip->name = name.toStdString();
        clip->data.resize(std::size_t(size));
        for (auto &v : clip->data)
            v = std::clamp(float(in.readShort()) / 32767.f, -1.f, 1.f);
        clip->buildPreview();
    }
    if (!in.isExhausted())
        return false;
    for (int i = 0; i < 2; ++i)
        publish(i, std::move(loaded[std::size_t(i)]));
    apply(patch, label, -1);
    return true;
}
void Instrument::getStateInformation(juce::MemoryBlock &output)
{
    encodeState(output, patchName());
}
void Instrument::setStateInformation(const void *data, int bytes)
{
    decodeState(data, bytes);
}
juce::Result Instrument::savePatch(const juce::File &file)
{
    const auto label = file.getFileNameWithoutExtension().substring(0, 120);
    juce::MemoryBlock bytes;
    encodeState(bytes, label);
    if (!file.replaceWithData(bytes.getData(), bytes.getSize()))
        return juce::Result::fail("The patch could not be saved.");
    {
        const std::scoped_lock lock(nameMutex);
        name = label;
    }
    presetIndex.store(-1);
    dirty.store(false);
    return juce::Result::ok();
}
juce::Result Instrument::openPatch(const juce::File &file)
{
    juce::MemoryBlock bytes;
    if (file.getSize() > maxPatchSize || !file.loadFileAsData(bytes) ||
        !decodeState(bytes.getData(), int(bytes.getSize())))
        return juce::Result::fail("This is not a compatible Mica Field patch.");
    return juce::Result::ok();
}
void Instrument::toggleSequencer()
{
    runSequence.store(!runSequence.load());
}
void Instrument::requestPanic()
{
    keyboard.allNotesOff(0);
    runSequence.store(false);
    panicPending.store(true);
}
void Instrument::randomiseSteps()
{
    auto &random = juce::Random::getSystemRandom();
    const int scale = pick(value(seqScale), 6);
    for (int i = 0; i < maxSteps; ++i)
    {
        change(stepParameter(i, stepOn), i == 0 || random.nextFloat() < .66f ? 1.f : 0.f);
        change(stepParameter(i, stepPitch),
               scalePitchValue(scaleOffset(scale, i == 0 ? 0 : random.nextInt(15) - 4)));
        change(stepParameter(i, stepVelocity), .38f + random.nextFloat() * .5f);
        change(stepParameter(i, stepChance), i == 0 ? 1.f : .48f + random.nextFloat() * .52f);
    }
}
juce::Result Instrument::startRecording(const juce::File &file)
{
    if (recordingActive.load())
        return juce::Result::fail("A recording is already running.");
    const double rate = captureRate.load();
    if (rate <= 0.)
        return juce::Result::fail("Select an audio output before recording.");
    auto next = std::make_unique<Recording>(file);
    std::unique_ptr<juce::OutputStream> stream = next->temporary.getFile().createOutputStream();
    if (stream == nullptr)
        return juce::Result::fail("The WAV file could not be created.");
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(
        stream,
        juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
    if (writer == nullptr)
        return juce::Result::fail("The WAV encoder could not be started.");
    if (!diskThread.isThreadRunning())
        diskThread.startThread();
    next->writer =
        std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer.release(), diskThread, 65536);
    {
        const juce::SpinLock::ScopedLockType lock(writerLock);
        if (captureRate.load() != rate || recording != nullptr)
            return juce::Result::fail("The audio device changed. Start the recording again.");
        captureFrames.store(0);
        overruns.store(0);
        recording = std::move(next);
        recordingActive.store(true);
    }
    return juce::Result::ok();
}
juce::Result Instrument::stopRecording()
{
    std::unique_ptr<Recording> done;
    {
        const juce::SpinLock::ScopedLockType lock(writerLock);
        recordingActive.store(false);
        done = std::move(recording);
    }
    if (done == nullptr)
        return juce::Result::ok();
    done->writer.reset();
    if (!done->temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("The WAV file could not be finalized.");
    if (overruns.load() != 0)
        return juce::Result::fail("WAV saved with gaps because the disk writer fell behind.");
    return juce::Result::ok();
}
double Instrument::recordedSeconds() const
{
    const double rate = captureRate.load();
    return rate > 0. ? double(captureFrames.load()) / rate : 0.;
}
void Instrument::useAudioSettings()
{
    if (auto *app = juce::StandalonePluginHolder::getInstance())
        app->showAudioSettingsDialog();
}
juce::AudioProcessorEditor *Instrument::createEditor()
{
    return new Editor(*this);
}
} // namespace mica
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new mica::Instrument();
}
