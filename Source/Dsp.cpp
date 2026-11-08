// SPDX-License-Identifier: AGPL-3.0-only
#include "Dsp.h"
#include <algorithm>
#include <cmath>

namespace mica
{
namespace
{
constexpr float pi = 3.14159265358979323846f, twoPi = 2.f * pi;
constexpr int controlPeriod = 32, tableSize = 1024, frameCount = 6, bandCount = 7;
constexpr std::array<int, bandCount> bandHarmonics{48, 24, 12, 6, 3, 2, 1};
float clamp01(float x)
{
    return std::clamp(x, 0.f, 1.f);
}
float mix(float a, float b, float x)
{
    return a + x * (b - a);
}
float wrap(float x)
{
    return x - std::floor(x);
}
float smooth(float x)
{
    return x * x * (3.f - 2.f * x);
}
float sat(float x)
{
    return x / (1.f + .72f * std::abs(x));
}
float random01(std::uint32_t &seed)
{
    seed ^= seed << 13u;
    seed ^= seed >> 17u;
    seed ^= seed << 5u;
    return float(seed >> 8u) * (1.f / 16777216.f);
}
struct Tables
{
    std::array<float, tableSize + 1> sine{};
    std::array<std::array<std::array<float, tableSize + 1>, frameCount>, bandCount> frames{};
    Tables()
    {
        for (int i = 0; i < tableSize; ++i)
            sine[std::size_t(i)] = std::sin(twoPi * float(i) / float(tableSize));
        sine.back() = sine.front();
        for (int band = 0; band < bandCount; ++band)
            for (int frame = 0; frame < frameCount; ++frame)
            {
                auto &table = frames[std::size_t(band)][std::size_t(frame)];
                for (int harmonic = 1; harmonic <= bandHarmonics[std::size_t(band)]; ++harmonic)
                {
                    const float h = float(harmonic);
                    const bool odd = (harmonic & 1) != 0;
                    const auto bell = [](float x, float center, float width)
                    {
                        const float distance = (x - center) / width;
                        return std::exp(-distance * distance);
                    };
                    float amplitude = 0.f;
                    switch (frame)
                    {
                    case 0:
                        amplitude = harmonic == 1 ? 1.f : 0.f;
                        break;
                    case 1:
                        amplitude = odd ? (harmonic % 4 == 1 ? 1.f : -1.f) / (h * h) : .21f / (h * h);
                        break;
                    case 2:
                        amplitude = odd ? 1.f / h : -.04f / h;
                        break;
                    case 3:
                        amplitude = .59f / h + .55f * bell(h, 4.f, 1.4f) / std::sqrt(h) +
                                    .64f * bell(h, 9.f, 2.3f) / std::sqrt(h);
                        break;
                    case 4:
                        amplitude = (harmonic % 3 == 1 ? 1.f : -.48f) / std::pow(h, .76f);
                        break;
                    case 5:
                        amplitude = (harmonic % 4 == 0 ? -1.f : .74f) / std::pow(h, .94f);
                        break;
                    }
                    for (int i = 0; i < tableSize; ++i)
                        table[std::size_t(i)] +=
                            amplitude * sine[std::size_t((i * harmonic) & (tableSize - 1))];
                }
                float peak = 1.f;
                for (auto v : table)
                    peak = std::max(peak, std::abs(v));
                for (auto &v : table)
                    v *= .91f / peak;
                table.back() = table.front();
            }
    }
};
const Tables tables;
float interpolate(const std::array<float, tableSize + 1> &table, float phase)
{
    const float x = wrap(phase) * float(tableSize);
    const int i = std::min(int(x), tableSize - 1);
    return mix(table[std::size_t(i)], table[std::size_t(i + 1)], x - float(i));
}
float sine(float phase)
{
    return interpolate(tables.sine, phase);
}
int bandFor(float delta)
{
    int band = 0;
    while (band < bandCount - 1 && std::max(0.f, delta) * float(bandHarmonics[std::size_t(band)]) > .47f)
        ++band;
    return band;
}
float blep(float phase, float delta)
{
    if (delta <= 0.f)
        return 0.f;
    if (phase < delta)
    {
        const float x = phase / delta;
        return 2.f * x - x * x - 1.f;
    }
    if (phase > 1.f - delta)
    {
        const float x = (phase - 1.f) / delta;
        return x * x + 2.f * x + 1.f;
    }
    return 0.f;
}
Stereo panned(float value, float pan)
{
    return {value * (1.f - .75f * std::max(pan, 0.f)), value * (1.f + .75f * std::min(pan, 0.f))};
}
float readClip(const SampleClip &clip, float position)
{
    if (position < 0.f || position >= float(clip.data.size() - 1))
        return 0.f;
    const auto i = std::size_t(position);
    return mix(clip.data[i], clip.data[i + 1], position - float(i));
}
} // namespace
float oscillatorTable(int frame, float phase, float phaseStep)
{
    return interpolate(
        tables.frames[std::size_t(bandFor(phaseStep))][std::size_t(std::clamp(frame, 0, frameCount - 1))],
        phase);
}
float oscillatorShape(float phase, float position, float pulseWidth, float phaseStep)
{
    const float triangle = 1.f - 4.f * std::abs(phase - .5f);
    const float saw = 2.f * phase - 1.f - blep(phase, phaseStep);
    const float width = .12f + .76f * pulseWidth;
    const float pulse =
        (phase < width ? 1.f : -1.f) + blep(phase, phaseStep) - blep(wrap(phase - width), phaseStep);
    const float select = position * 3.f;
    if (select < 1.f)
        return mix(sine(phase), triangle, select);
    if (select < 2.f)
        return mix(triangle, saw, select - 1.f);
    return mix(saw, pulse * .8f, select - 2.f);
}
float lfoWave(int type, float phase)
{
    phase = wrap(phase);
    switch (type)
    {
    case 1:
        return 1.f - 4.f * std::abs(phase - .5f);
    case 2:
        return 2.f * phase - 1.f;
    case 3:
        return phase < .5f ? 1.f : -1.f;
    default:
        return sine(phase);
    }
}
void SampleClip::buildPreview()
{
    for (std::size_t i = 0; i < preview.size(); ++i)
    {
        const auto start = i * data.size() / preview.size(), end = (i + 1) * data.size() / preview.size();
        float peak = 0.f;
        for (auto j = start; j < end; ++j)
            peak = std::max(peak, std::abs(data[j]));
        preview[i] = peak;
    }
}
const SampleClip &builtinClip()
{
    static const auto clip = []
    {
        SampleClip result;
        result.name = "Weathered Tape";
        result.data.resize(storageSampleRate * 2);
        std::uint32_t seed = 137131u;
        float noise = 0.f;
        for (std::size_t i = 0; i < result.data.size(); ++i)
        {
            const float t = float(i) / storageSampleRate;
            noise += .038f * (2.f * random01(seed) - 1.f - noise);
            const float f = 261.6256f * t + .22f * sine(t * .83f) + .06f * sine(t * 5.76f);
            const float ripple = .63f + .26f * sine(t * 1.74f) + .11f * sine(t * 7.21f);
            const float window = smooth(std::min(t * 13.f, 1.f)) * smooth(std::min((2.f - t) * 12.f, 1.f));
            result.data[i] =
                window * (ripple * (.50f * sine(f) + .22f * sine(f * 2.004f) + .12f * sine(f * 3.017f)) +
                          .46f * noise);
        }
        result.buildPreview();
        return result;
    }();
    return clip;
}
void Synth::Envelope::start(float level)
{
    stage = rise;
    position = 0.f;
    initial = value = level;
}
void Synth::Envelope::off()
{
    if (stage != idle && stage != tail)
    {
        initial = value;
        position = 0.f;
        stage = tail;
    }
}
void Synth::Envelope::tick(const std::array<float, 4> &c)
{
    switch (stage)
    {
    case idle:
        break;
    case rise:
        position = std::min(position + c[0], 1.f);
        value = mix(initial, 1.f, smooth(position));
        if (position >= 1.f)
        {
            stage = fall;
            position = 0.f;
        }
        break;
    case fall:
        position = std::min(position + c[1], 1.f);
        value = mix(1.f, c[2], position * (2.f - position));
        if (position >= 1.f)
            stage = hold;
        break;
    case hold:
        value = mix(value, c[2], .002f);
        break;
    case tail:
        position = std::min(position + c[3], 1.f);
        value = initial * (1.f - position) * (1.f - position);
        if (position >= 1.f)
            stage = idle;
        break;
    }
}
void Synth::Filter::update(float frequency, float resonance, float newDrive, float level, int mode,
                           float sampleRate)
{
    g = std::tan(pi * std::clamp(frequency, 18.f, sampleRate * .447f) / sampleRate);
    k = mix(1.87f, .14f, resonance * resonance);
    h = 1.f / (1.f + g * (g + k));
    drive = newDrive * newDrive * 4.f;
    gain = level;
    if (type == int(FilterType::bypass) && mode != type)
        state = {};
    type = mode;
}
Stereo Synth::Filter::tick(Stereo input)
{
    if (type == int(FilterType::bypass))
        return input * gain;
    if (drive > .0001f)
    {
        const float compensate = 1.f / (1.f + drive * .23f);
        input = {sat(input.left * (1.f + drive)) * compensate, sat(input.right * (1.f + drive)) * compensate};
    }
    const auto delta = input + state[1] * -1.f;
    const auto band = (state[0] + delta * g) * h;
    const auto low = state[1] + band * g;
    state[0] = band * 2.f + state[0] * -1.f;
    state[1] = low * 2.f + state[1] * -1.f;
    const auto high = input + band * -k + low * -1.f;
    switch (static_cast<FilterType>(type))
    {
    case FilterType::highPass:
        return high * gain;
    case FilterType::bandPass:
        return band * gain;
    case FilterType::notch:
        return (high + low) * gain;
    default:
        return low * gain;
    }
}
void Synth::Layer::update(const float *p, float fundamental, float rate, const SampleClip &clip)
{
    const int next = pick(p[aType], 6);
    const bool newMode = next != type, newClip = revision != clip.revision;
    type = next;
    revision = clip.revision;
    const float tune = 48.f * (p[aTune] - .5f) + (p[aFine] - .5f);
    const float hz = fundamental * std::exp2(tune / 12.f);
    freq = std::min(hz / rate, .445f);
    shape = p[aShape];
    color = p[aColor];
    motion = p[aMotion];
    width = p[aWidth];
    level = p[aLevel];
    send = p[aSend];
    if (type == int(EngineType::analog))
    {
        const float spread = .003f + color * color * .047f;
        down = freq * (1.f - spread);
        up = std::min(freq * (1.f + spread), .475f);
    }
    if (type == int(EngineType::harmonic))
    {
        partialCount = std::min(2 + int(shape * 10.999f), int(.48f / std::max(freq, .000001f)));
        float normalise = 1.f;
        for (int j = 0; j < partialCount; ++j)
        {
            const float oddEven =
                j == 0 ? 1.f : (j % 2 == 0 ? mix(1.f, .24f, motion) : mix(.28f, 1.f, motion));
            weights[std::size_t(j)] = oddEven / std::pow(float(j + 1), mix(2.15f, .50f, color));
            if (j > 0)
                normalise += weights[std::size_t(j)] * .41f;
        }
        for (int j = 0; j < partialCount; ++j)
            weights[std::size_t(j)] /= normalise;
    }
    if (type == int(EngineType::modal))
    {
        static constexpr std::array<float, 6> wood{1.f, 2.03f, 3.01f, 4.16f, 5.28f, 6.80f};
        static constexpr std::array<float, 6> metal{1.f, 2.76f, 5.40f, 8.93f, 13.34f, 18.62f};
        for (std::size_t j = 0; j < modePhase.size(); ++j)
        {
            modeStep[j] = freq * mix(wood[j], metal[j], shape);
            const float life = mix(4.7f, .12f, color) / (1.f + float(j) * (.35f + .61f * color));
            modeFall[j] = std::exp(-1.f / (life * rate));
            if (newMode)
                modeEnergy[j] = j == 0 ? .63f : mix(.33f, .95f, motion) / float(j + 1);
        }
        hitFall = std::exp(-1.f / (rate * mix(.013f, .083f, motion)));
        if (newMode)
            hit = 1.f;
    }
    if (type == int(EngineType::sample) || type == int(EngineType::granular))
    {
        pitchRate = std::clamp(hz / 261.625565f * float(clip.rate) / rate, .013f, 12.f);
        if (newMode || newClip)
        {
            playhead = shape * float(clip.data.size() - 2) * .91f;
            grainCountdown = 0.f;
            grains = {};
        }
    }
}
Stereo Synth::Layer::tick(const SampleClip &clip, float rate, std::uint32_t &seed)
{
    if (level < .0001f)
        return {};
    Stereo output;
    float mono = 0.f;
    switch (static_cast<EngineType>(type))
    {
    case EngineType::wavetable:
    {
        const float position = shape * float(frameCount - 1), blend = position - float(int(position));
        const int frame = int(position), band = bandFor(freq * (1.f + color * 1.3f));
        const auto &family = tables.frames[std::size_t(band)];
        const auto &first = family[std::size_t(frame)];
        const auto &second = family[std::size_t(std::min(frame + 1, frameCount - 1))];
        const float bent = phase + color * .115f * sine(phase + motion * .5f) + motion * .31f;
        const float right = bent + width * .10f * sine(phase * 2.f);
        output = {mix(interpolate(first, bent), interpolate(second, bent), blend),
                  mix(interpolate(first, right), interpolate(second, right), blend)};
        phase = wrap(phase + freq);
        break;
    }
    case EngineType::analog:
    {
        const float main = oscillatorShape(phase, shape, motion, freq) * .62f;
        const float lower = oscillatorShape(lowerPhase, shape, motion, down) * .22f;
        const float upper = oscillatorShape(upperPhase, shape, motion, up) * .22f;
        output = {main + lower + (1.f - width) * upper, main + upper + (1.f - width) * lower};
        phase = wrap(phase + freq);
        lowerPhase = wrap(lowerPhase + down);
        upperPhase = wrap(upperPhase + up);
        break;
    }
    case EngineType::harmonic:
        for (int j = 0; j < partialCount; ++j)
            mono += sine(phase * float(j + 1)) * weights[std::size_t(j)];
        output = {mono * (1.f - width * .08f), mono};
        phase = wrap(phase + freq);
        break;
    case EngineType::modal:
        for (std::size_t j = 0; j < modePhase.size(); ++j)
        {
            if (modeStep[j] < .48f)
            {
                output += panned(sine(modePhase[j]) * modeEnergy[j], width * (j % 2 == 0 ? -.74f : .74f));
                modePhase[j] = wrap(modePhase[j] + modeStep[j]);
            }
            modeEnergy[j] *= modeFall[j];
        }
        mono = (random01(seed) * 2.f - 1.f) * hit * motion * .12f;
        hit *= hitFall;
        output += {mono, mono};
        break;
    case EngineType::sample:
    {
        const float full = float(clip.data.size() - 2), first = shape * full * .91f;
        const float last = std::max(first + 1.f, mix(first, full, .11f + motion * .89f));
        const float fade = std::min((last - first) * .23f, float(clip.rate) * .014f);
        if (playhead < first)
            playhead = first;
        if (playhead < last)
        {
            mono = readClip(clip, playhead);
            if (color >= .5f && playhead > last - fade)
            {
                const float edge = playhead - (last - fade);
                mono = mix(mono, readClip(clip, first + edge), edge / fade);
            }
            else
            {
                mono *= std::min((playhead - first) / std::max(fade, .001f), 1.f);
                if (color < .5f)
                    mono *= std::min((last - playhead) / std::max(fade, .001f), 1.f);
            }
            playhead += pitchRate;
        }
        if (color >= .5f && playhead >= last)
            playhead = first + fade + std::fmod(playhead - last, std::max(1.f, last - first - fade));
        output = {mono * (1.f - .12f * width), mono};
        break;
    }
    case EngineType::granular:
    {
        if (--grainCountdown <= 0.f)
        {
            const float duration = rate * (.024f + color * color * .29f);
            auto grain = std::find_if(grains.begin(), grains.end(),
                                      [](const auto &item) { return item.progress >= 1.f; });
            if (grain != grains.end())
            {
                grain->position =
                    wrap(shape + (2.f * random01(seed) - 1.f) * motion * .39f) * float(clip.data.size() - 2);
                grain->increment = 1.f / duration;
                grain->pan = (2.f * random01(seed) - 1.f) * width;
                grain->progress = 0.f;
            }
            grainCountdown += std::max(12.f, duration * (.42f - width * .08f));
        }
        for (auto &grain : grains)
        {
            if (grain.progress >= 1.f)
                continue;
            const float window = .5f - .5f * sine(grain.progress + .25f);
            output += panned(readClip(clip, grain.position) * window, grain.pan);
            grain.position += pitchRate;
            if (grain.position >= float(clip.data.size() - 2))
                grain.position = std::fmod(grain.position, float(clip.data.size() - 2));
            grain.progress += grain.increment;
        }
        output = output * .76f;
        break;
    }
    default:
        break;
    }
    return output * level;
}
void Synth::Voice::off()
{
    for (auto &env : envelopes)
        env.off();
}
Stereo Synth::Voice::tick(const std::array<const SampleClip *, 2> &clips,
                          const std::array<std::array<float, 4>, 3> &env, float rate)
{
    for (std::size_t i = 0; i < envelopes.size(); ++i)
        envelopes[i].tick(env[i]);
    if (envelopes[0].stage == Envelope::idle)
    {
        note = -1;
        return {};
    }
    Stereo in1, in2;
    for (std::size_t i = 0; i < layers.size(); ++i)
    {
        const auto sound = layers[i].tick(*clips[i], rate, seed);
        in1 += sound * (1.f - layers[i].send);
        in2 += sound * layers[i].send;
    }
    if (sub > .0001f || noise > .0001f)
    {
        const float white = 2.f * random01(seed) - 1.f;
        noiseState += (white - noiseState) * mix(.021f, .76f, noiseTone);
        const float delta = std::min(frequency * .5f / rate, .45f);
        const float triangle = 1.f - 4.f * std::abs(subPhase - .5f);
        const float utility =
            mix(sine(subPhase), triangle, subShape) * sub * .69f + noiseState * noise * .66f;
        subPhase = wrap(subPhase + delta);
        in1 += Stereo{utility, utility} * (1.f - utilitySend);
        in2 += Stereo{utility, utility} * utilitySend;
    }
    const auto first = filters[0].tick(in1 + in2 * (1.f - route));
    const auto second = filters[1].tick(in2 * route + first * (1.f - route));
    const auto output =
        first * (route * (1.f - blend * .84f)) + second * (1.f - route * (1.f - blend) * .84f);
    const float gain = envelopes[0].value * velocityGain;
    return {output.left * gain * (1.f - .72f * std::max(pan, 0.f)),
            output.right * gain * (1.f + .72f * std::min(pan, 0.f))};
}
void Synth::Delay::prepare(int frames)
{
    data.assign(std::size_t(frames), 0.f);
    cursor = 0;
}
void Synth::Delay::clear()
{
    std::fill(data.begin(), data.end(), 0.f);
    cursor = 0;
}
float Synth::Delay::read(float frames) const
{
    const float pos = float(cursor) - std::clamp(frames, 1.f, float(data.size() - 2));
    const float wrapped = pos < 0.f ? pos + float(data.size()) : pos;
    const auto a = std::min(std::size_t(wrapped), data.size() - 1), b = a + 1 == data.size() ? 0 : a + 1;
    return mix(data[a], data[b], wrapped - float(a));
}
void Synth::Delay::write(float sample)
{
    data[cursor] = std::abs(sample) < 1.e-20f ? 0.f : sample;
    if (++cursor >= data.size())
        cursor = 0;
}
void Synth::Effects::prepare(float rate)
{
    for (auto &line : chorusLine)
        line.prepare(int(rate * .042f) + 16);
    for (auto &line : echoLine)
        line.prepare(int(rate * 1.72f) + 16);
    for (auto &line : octaveLine)
        line.prepare(int(rate * .075f) + 16);
    for (auto &line : spaceLine)
        line.prepare(int(rate * .091f) + 16);
    clear();
    delayFrames = rate * .39f;
}
void Synth::Effects::clear()
{
    for (auto &line : chorusLine)
        line.clear();
    for (auto &line : echoLine)
        line.clear();
    for (auto &line : octaveLine)
        line.clear();
    for (auto &line : spaceLine)
        line.clear();
    damping.fill(0.f);
    chorusPhase = octavePhase = 0.f;
}
Stereo Synth::Effects::tick(Stereo signal, const std::array<float, paramCount> &p, float rate)
{
    chorusPhase = wrap(chorusPhase + (.032f + p[chorusRate] * p[chorusRate] * 3.9f) / rate);
    const float chorusL = chorusLine[0].read(rate * (.013f + .009f * p[chorusDepth] * sine(chorusPhase)));
    const float chorusR =
        chorusLine[1].read(rate * (.017f + .009f * p[chorusDepth] * sine(chorusPhase + .44f)));
    chorusLine[0].write(signal.left);
    chorusLine[1].write(signal.right);
    signal = {mix(signal.left, chorusL, p[chorusMix] * .54f),
              mix(signal.right, chorusR, p[chorusMix] * .54f)};
    delayFrames = mix(delayFrames, delaySeconds(p[delayTime]) * rate, .0007f);
    const float echoL = echoLine[0].read(delayFrames), echoR = echoLine[1].read(delayFrames * 1.041f);
    echoLine[0].write(signal.left + echoR * p[delayFeedback] * .89f);
    echoLine[1].write(signal.right + echoL * p[delayFeedback] * .89f);
    signal += Stereo{echoL, echoR} * (p[delayMix] * .79f);
    constexpr std::array<float, 8> times{.0297f, .0369f, .0411f, .0449f, .0323f, .0391f, .0457f, .0511f};
    float reverbL = 0.f, reverbR = 0.f, pitchL = 0.f, pitchR = 0.f;
    if (p[shimmer] > .0001f)
    {
        const float window = rate * .052f;
        octavePhase = wrap(octavePhase + 1.f / window);
        const float phase2 = wrap(octavePhase + .5f), crossfade = .5f - .5f * sine(octavePhase + .25f);
        pitchL = mix(octaveLine[0].read(2.f + window * (1.f - phase2)),
                     octaveLine[0].read(2.f + window * (1.f - octavePhase)), crossfade);
        pitchR = mix(octaveLine[1].read(2.f + window * (1.f - phase2)),
                     octaveLine[1].read(2.f + window * (1.f - octavePhase)), crossfade);
    }
    const float dampingRate = mix(.76f, .069f, p[roomDamp]), feedback = mix(.51f, .952f, p[roomSize]);
    for (std::size_t i = 0; i < spaceLine.size(); ++i)
    {
        auto &line = spaceLine[i];
        const float output = line.read(times[i] * rate);
        damping[i] += dampingRate * (output - damping[i]);
        const float dry =
            i < 4 ? signal.left * .77f + signal.right * .23f : signal.right * .77f + signal.left * .23f;
        line.write(dry * .28f + feedback * damping[i] + (i < 4 ? pitchR : pitchL) * p[shimmer] * .036f);
        if (i < 4)
            reverbL += output;
        else
            reverbR += output;
    }
    octaveLine[0].write(reverbL * .24f);
    octaveLine[1].write(reverbR * .24f);
    const Stereo wet{reverbL * .31f + pitchL * p[shimmer] * .41f,
                     reverbR * .31f + pitchR * p[shimmer] * .41f};
    return signal * (1.f - p[roomMix] * .44f) + wet * p[roomMix];
}
void Synth::prepare(double sampleRate)
{
    rate = float(std::clamp(sampleRate, 8000., 384000.));
    inverseRate = 1.f / rate;
    clips.fill(&builtinClip());
    for (int note = 0; note < 128; ++note)
        frequencies[std::size_t(note)] = midiHz(note);
    effects.prepare(rate);
    patch = factory().front().patch;
    outputGain = patch.values[master] * patch.values[master];
    panic();
}
void Synth::beginBlock(const Patch &next, const std::array<const SampleClip *, 2> &buffers, bool runSequencer)
{
    patch = next;
    for (std::size_t i = 0; i < clips.size(); ++i)
        clips[i] = buffers[i] == nullptr || buffers[i]->data.size() < 3 ? &builtinClip() : buffers[i];
    if (runSequencer != sequencePlaying)
    {
        sequencePlaying = runSequencer;
        if (seqNote >= 0)
            noteOff(0, seqNote);
        seqNote = seqIndex = -1;
        seqRemaining = 0.;
        seqGateFrames = 0;
        stepValue = 0.f;
    }
    meterL = meterR = 0.f;
}
void Synth::noteOn(int channel, int note, float velocity)
{
    if (channel < 0 || channel >= int(pedals.size()) || note < 0 || note > 127)
        return;
    Voice *voice = nullptr;
    float reAttack = 0.f;
    for (int i = 0; i < voiceLimit(patch.values[polyphony]); ++i)
    {
        auto &candidate = voices[std::size_t(i)];
        if (candidate.note == note && candidate.channel == channel && candidate.keyDown)
        {
            voice = &candidate;
            reAttack = candidate.envelopes[0].value * .46f;
            break;
        }
        if (voice == nullptr || candidate.note == -1 ||
            (voice->note != -1 && candidate.envelopes[0].value + (candidate.keyDown ? .5f : 0.f) <
                                      voice->envelopes[0].value + (voice->keyDown ? .5f : 0.f)))
            voice = &candidate;
    }
    *voice = {};
    voice->note = note;
    voice->channel = channel;
    voice->keyDown = true;
    voice->age = ++serial;
    voice->velocity = clamp01(velocity);
    voice->frequency = frequencies[std::size_t(note)];
    voice->seed = std::uint32_t(voice->age * 661u + 721u);
    voice->envelopes[0].start(reAttack);
    voice->envelopes[1].start(0.f);
    voice->envelopes[2].start(0.f);
    voice->layers[1].upperPhase = .03f;
    updateVoice(*voice);
    quietFrames = controlCountdown = 0;
}
void Synth::noteOff(int channel, int note)
{
    if (channel < 0 || channel >= int(pedals.size()))
        return;
    for (auto &voice : voices)
        if (voice.note == note && voice.channel == channel && voice.keyDown)
        {
            voice.keyDown = false;
            if (!pedals[std::size_t(channel)])
                voice.off();
        }
}
void Synth::sustainPedal(int channel, bool down)
{
    if (channel < 1 || channel >= int(pedals.size()))
        return;
    pedals[std::size_t(channel)] = down;
    if (!down)
        for (auto &voice : voices)
            if (voice.note >= 0 && voice.channel == channel && !voice.keyDown)
                voice.off();
}
void Synth::pitchWheel(int channel, float bend)
{
    if (channel > 0 && channel < int(bends.size()))
        bends[std::size_t(channel)] = std::clamp(bend, -1.f, 1.f);
}
void Synth::modWheel(int channel, float value)
{
    if (channel > 0 && channel < int(wheels.size()))
        wheels[std::size_t(channel)] = clamp01(value);
}
void Synth::allNotesOff(int channel)
{
    for (auto &voice : voices)
        if (voice.note >= 0 && (channel == 0 || voice.channel == channel))
        {
            voice.keyDown = false;
            voice.off();
        }
    if (channel > 0 && channel < int(pedals.size()))
        pedals[std::size_t(channel)] = false;
    if (channel == 0)
        pedals.fill(false);
}
void Synth::panic()
{
    voices = {};
    pedals.fill(false);
    bends.fill(0.f);
    wheels.fill(0.f);
    effects.clear();
    lfoPhases.fill(0.f);
    sources.fill(0.f);
    randomPhase = randomValue = stepValue = 0.f;
    seqRemaining = 0.;
    seqGateFrames = 0;
    seqIndex = seqNote = -1;
    quietFrames = int(rate * 2.3f);
    controlCountdown = 0;
}
int Synth::voicesPlaying() const
{
    return int(std::count_if(voices.begin(), voices.end(), [](const auto &v) { return v.note >= 0; }));
}
void Synth::updateSources(const Voice *voice, std::array<float, int(Source::count)> &out) const
{
    out = sources;
    out[std::size_t(Source::velocity)] = voice != nullptr ? voice->velocity : 0.f;
    out[std::size_t(Source::keytrack)] = voice != nullptr ? float(voice->note) / 127.f : 0.f;
    out[std::size_t(Source::wheel)] = voice != nullptr ? wheels[std::size_t(voice->channel)] : wheels[1];
    for (std::size_t i = 0; i < 3; ++i)
        out[std::size_t(Source::amp) + i] = voice != nullptr ? voice->envelopes[i].value : 0.f;
}
void Synth::updateVoice(Voice &voice)
{
    std::array<float, int(Source::count)> mod;
    updateSources(&voice, mod);
    auto p = patch.values;
    for (const auto &route : patch.routes)
        if (route.source >= 0)
            p[std::size_t(route.target)] += route.amount * mod[std::size_t(route.source)];
    for (auto &value : p)
        value = clamp01(value);
    const float hz = voice.frequency * std::exp2(bends[std::size_t(voice.channel)] * (2.f / 12.f));
    for (std::size_t i = 0; i < voice.layers.size(); ++i)
        voice.layers[i].update(p.data() + i * bType, hz, rate, *clips[i]);
    voice.filters[0].update(filterFrequency(p[f1Cut]), p[f1Res], p[f1Drive], p[f1Level], pick(p[f1Type], 5),
                            rate);
    voice.filters[1].update(filterFrequency(p[f2Cut]), p[f2Res], p[f2Drive], p[f2Level], pick(p[f2Type], 5),
                            rate);
    voice.route = p[filterRoute];
    voice.blend = p[filterBlend];
    const float alternating = voice.age % 2 == 0 ? .15f : -.15f;
    voice.pan = std::clamp(2.f * p[voicePan] - 1.f + alternating * (p[aWidth] + p[bWidth]), -1.f, 1.f);
    voice.velocityGain = mix(1.f, .20f + .80f * voice.velocity, p[ampVelocity]);
    voice.sub = p[subLevel];
    voice.subShape = p[subShape];
    voice.noise = p[noiseLevel];
    voice.noiseTone = p[noiseColor];
    voice.utilitySend = p[utilitySend];
}
void Synth::controlTick()
{
    const auto &p = patch.values;
    for (std::size_t i = 0; i < 3; ++i)
    {
        const auto j = std::size_t(attack) + i * 4;
        envCoefficients[i] = {inverseRate / seconds(p[j]), inverseRate / seconds(p[j + 1]), p[j + 2],
                              inverseRate / seconds(p[j + 3])};
        const auto lfo = std::size_t(lfo1Rate) + i * 2;
        lfoPhases[i] = wrap(lfoPhases[i] + lfoFrequency(p[lfo]) * inverseRate * controlPeriod);
        sources[std::size_t(Source::lfo1) + i] = lfoWave(pick(p[lfo + 1], 4), lfoPhases[i]);
    }
    randomPhase += lfoFrequency(p[randomRate]) * inverseRate * controlPeriod;
    if (randomPhase >= 1.f)
    {
        randomPhase = wrap(randomPhase);
        randomValue = random01(randomSeed) * 2.f - 1.f;
    }
    sources[std::size_t(Source::random)] = randomValue;
    sources[std::size_t(Source::step)] = stepValue;
    for (std::size_t i = 0; i < 4; ++i)
        sources[std::size_t(Source::macro1) + i] = p[std::size_t(macro1) + i];
    Voice *newest = nullptr;
    const int limit = voiceLimit(p[polyphony]);
    for (int i = 0; i < int(voices.size()); ++i)
    {
        auto &voice = voices[std::size_t(i)];
        if (voice.note < 0)
            continue;
        if (i >= limit && voice.keyDown)
        {
            voice.keyDown = false;
            voice.off();
        }
        updateVoice(voice);
        if (newest == nullptr || voice.age > newest->age)
            newest = &voice;
    }
    updateSources(newest, sources);
    fxValues = p;
    for (const auto &route : patch.routes)
        if (route.source >= 0 && route.target >= chorusRate && route.target <= roomMix)
            fxValues[std::size_t(route.target)] += route.amount * sources[std::size_t(route.source)];
    for (int i = chorusRate; i <= roomMix; ++i)
        fxValues[std::size_t(i)] = clamp01(fxValues[std::size_t(i)]);
}
void Synth::sequenceTick()
{
    if (seqGateFrames > 0 && --seqGateFrames == 0 && seqNote >= 0)
    {
        noteOff(0, seqNote);
        seqNote = -1;
    }
    if (seqRemaining > 0.)
    {
        seqRemaining -= 1.;
        return;
    }
    const auto &p = patch.values;
    seqIndex = (seqIndex + 1) % (1 + pick(p[seqLength], 16));
    if (seqNote >= 0)
        noteOff(0, seqNote);
    seqNote = -1;
    const float subdivision = 1.f / float(1 << pick(p[seqDivision], 3));
    const float swing = (seqIndex % 2 == 0 ? 1.f : -1.f) * p[seqSwing] * .44f;
    const float interval = rate * (60.f / (40.f + 160.f * p[seqTempo])) * subdivision * (1.f + swing);
    seqRemaining += double(interval) - 1.;
    const int pitch = pick(p[std::size_t(stepParameter(seqIndex, stepPitch))], 49) - 24;
    stepValue = float(pitch) / 24.f;
    if (p[std::size_t(stepParameter(seqIndex, stepOn))] > .5f &&
        random01(sequenceSeed) < p[std::size_t(stepParameter(seqIndex, stepChance))])
    {
        seqNote = std::clamp(24 + pick(p[seqRoot], 73) + pitch, 0, 127);
        noteOn(0, seqNote, p[std::size_t(stepParameter(seqIndex, stepVelocity))]);
        seqGateFrames = std::max(1, int(interval * (.07f + .90f * p[seqGate])));
    }
}
void Synth::render(float *left, float *right, int frames)
{
    const int idle = int(rate * 2.25f);
    if (!sequencePlaying && quietFrames >= idle && voicesPlaying() == 0)
    {
        controlCountdown -= frames;
        while (controlCountdown <= 0)
        {
            controlTick();
            controlCountdown += controlPeriod;
        }
        std::fill_n(left, frames, 0.f);
        if (right != nullptr)
            std::fill_n(right, frames, 0.f);
        return;
    }
    const float volume = patch.values[master] * patch.values[master];
    for (int frame = 0; frame < frames; ++frame)
    {
        if (sequencePlaying)
            sequenceTick();
        if (controlCountdown <= 0)
        {
            controlTick();
            controlCountdown = controlPeriod;
        }
        --controlCountdown;
        Stereo result;
        bool active = false;
        for (auto &voice : voices)
            if (voice.note >= 0)
            {
                result += voice.tick(clips, envCoefficients, rate);
                active = true;
            }
        const auto wet = effects.tick(result * .41f, fxValues, rate);
        quietFrames = active || std::max(std::abs(wet.left), std::abs(wet.right)) > .000065f
                          ? 0
                          : std::min(idle, quietFrames + 1);
        outputGain = mix(outputGain, volume, .0035f);
        const float l = sat(wet.left * outputGain * 1.62f), r = sat(wet.right * outputGain * 1.62f);
        meterL = std::max(meterL, std::abs(l));
        meterR = std::max(meterR, std::abs(r));
        left[frame] = right != nullptr ? l : (l + r) * .5f;
        if (right != nullptr)
            right[frame] = r;
    }
}
} // namespace mica
