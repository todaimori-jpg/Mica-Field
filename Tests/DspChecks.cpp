// SPDX-License-Identifier: AGPL-3.0-only
#include "Dsp.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
constexpr int block = 256;
void check(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}
struct Measurement
{
    double power = 0., signature = 0.;
    float peak = 0.f;
    int count = 0;
    double rms() const { return std::sqrt(power / double(count)); }
};
Measurement render(mica::Synth &synth, const mica::Patch &patch, int frames, bool sequence = false,
                   const mica::SampleClip *clip = nullptr)
{
    std::array<float, block> left{}, right{};
    Measurement result;
    for (int i = 0; i < frames; i += block)
    {
        const int count = std::min(block, frames - i);
        synth.beginBlock(patch, {clip, nullptr}, sequence);
        synth.render(left.data(), right.data(), count);
        for (int j = 0; j < count; ++j)
        {
            const float l = left[std::size_t(j)], r = right[std::size_t(j)];
            check(std::isfinite(l) && std::isfinite(r), "Non-finite audio output");
            result.peak = std::max(result.peak, std::max(std::abs(l), std::abs(r)));
            result.power += double(l * l + r * r) * .5;
            result.signature += (double(l) + double(r) * .43) * std::cos(double(i + j) * .0365927);
        }
        result.count += count;
    }
    return result;
}
mica::Patch isolated(int engine)
{
    using namespace mica;
    auto patch = factory().back().patch;
    auto &p = patch.values;
    p[aType] = fromChoice(engine, 6);
    p[aShape] = .36f;
    p[aColor] = .32f;
    p[aMotion] = .55f;
    p[aWidth] = .5f;
    p[f1Type] = 1.f;
    p[attack] = envelopeControl(.015f);
    p[release] = envelopeControl(.22f);
    p[sustain] = .85f;
    return patch;
}
void engines()
{
    using namespace mica;
    for (int rate : {44100, 48000, 96000})
    {
        std::array<double, 6> signatures{};
        for (int mode = 0; mode < 6; ++mode)
        {
            Synth synth;
            synth.prepare(rate);
            const auto patch = isolated(mode);
            synth.beginBlock(patch, {}, false);
            synth.noteOn(1, 60, .8f);
            const auto output = render(synth, patch, rate / 2);
            check(output.rms() > .001 && output.peak < .95f, "Engine silent or headroom exceeded");
            signatures[std::size_t(mode)] = output.signature;
            synth.noteOff(1, 60);
            render(synth, patch, rate * 3);
            check(synth.voicesPlaying() == 0, "Released note failed to decay");
        }
        for (std::size_t i = 0; i < signatures.size(); ++i)
            for (std::size_t j = i + 1; j < signatures.size(); ++j)
                check(std::abs(signatures[i] - signatures[j]) > .001,
                      "Engine modes rendered identical signals");
    }
}
void automation()
{
    using namespace mica;
    auto patch = factory().front().patch;
    patch.values[attack] = envelopeControl(.007f);
    patch.values[f1Res] = .96f;
    patch.values[f2Res] = .87f;
    Synth synth;
    synth.prepare(44100);
    synth.beginBlock(patch, {}, false);
    synth.noteOn(1, 45, .89f);
    synth.noteOn(1, 61, .82f);
    for (int i = 0; i < 82; ++i)
    {
        patch.values[f1Cut] = i % 3 == 0 ? 1.f : .23f + float(i % 7) * .12f;
        patch.values[f2Cut] = i % 4 == 0 ? 1.f : .16f + float(i % 5) * .14f;
        patch.values[f2Type] = fromChoice(i % 5, 5);
        check(render(synth, patch, 256).peak < 1.f, "Rapid filter automation exceeded headroom");
    }
}
void sequencing()
{
    using namespace mica;
    auto patch = isolated(0);
    patch.values[seqDivision] = 1.f;
    patch.values[seqLength] = 0.f;
    patch.values[stepParameter(0, stepOn)] = 1.f;
    patch.values[stepParameter(0, stepChance)] = 0.f;
    Synth synth;
    synth.prepare(48000);
    check(render(synth, patch, 16000, true).peak == 0.f, "Zero-chance step sounded");
    patch.values[stepParameter(0, stepChance)] = 1.f;
    check(render(synth, patch, 25000, true).peak > .01f, "Guaranteed step was silent");
    render(synth, patch, 48000, false);
    check(synth.currentStep() == -1 && synth.voicesPlaying() == 0, "Sequencer stop left notes");
    check(scaleOffset(0, -1) == -2 && scaleOffset(2, 7) == 12, "Scale conversion failed");
}
void samplesAndRoutes()
{
    using namespace mica;
    auto patch = isolated(int(EngineType::sample));
    patch.values[aShape] = 0.f;
    patch.values[aColor] = 1.f;
    patch.values[f1Type] = 0.f;
    patch.values[f1Cut] = .05f;
    patch.values[macro1] = 1.f;
    SampleClip clip;
    clip.name = "fixture";
    clip.revision = 42;
    clip.data.resize(24000);
    for (std::size_t i = 0; i < clip.data.size(); ++i)
        clip.data[i] = .65f * std::sin(float(i) * .29f);
    clip.buildPreview();
    Synth synth;
    synth.prepare(48000);
    synth.beginBlock(patch, {&clip, nullptr}, false);
    synth.noteOn(1, 60, .8f);
    const auto closed = render(synth, patch, 14000, false, &clip);
    patch.routes[0] = {int(Source::macro1), f1Cut, .85f};
    const auto decoded = unpackRoute(packRoute(patch.routes[0]));
    check(decoded.valid() && decoded.target == f1Cut && std::abs(decoded.amount - .85f) < .0001f,
          "Route serialization failed");
    check(render(synth, patch, 14000, false, &clip).rms() > closed.rms() * 5.,
          "Modulation failed to open filter");
}
void sustain()
{
    using namespace mica;
    auto patch = factory()[7].patch;
    for (int param : {master, polyphony, roomSize, shimmer, delayFeedback})
        patch.values[std::size_t(param)] = 1.f;
    Synth synth;
    synth.prepare(48000);
    synth.beginBlock(patch, {}, false);
    synth.sustainPedal(1, true);
    for (int i = 0; i < 12; ++i)
        synth.noteOn(1, 48 + i, 1.f);
    for (int i = 0; i < 12; ++i)
        synth.noteOff(1, 48 + i);
    const auto output = render(synth, patch, 96000);
    check(synth.voicesPlaying() == 12 && output.rms() > .001 && output.peak < 1.f,
          "Sustain or output failed");
    synth.sustainPedal(1, false);
    render(synth, patch, 48000 * 9);
    check(synth.voicesPlaying() == 0, "Sustain pedal left voices active");
    synth.panic();
    check(render(synth, patch, 256).peak == 0.f, "Panic left audio");
}
} // namespace
int main()
{
    engines();
    automation();
    sequencing();
    samplesAndRoutes();
    sustain();
    std::cout << "All DSP checks passed.\n";
}
