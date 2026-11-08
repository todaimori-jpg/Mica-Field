// SPDX-License-Identifier: AGPL-3.0-only
#include "Patch.h"
#include <algorithm>
#include <cmath>

namespace mica
{
const std::array<std::string_view, 6> engineNames{"Wavetable", "Analog", "Harmonic",
                                                  "Modal",     "Sample", "Granular"};
const std::array<std::array<std::string_view, 3>, 6> engineControls{{{"Position", "Warp", "Phase"},
                                                                     {"Waveform", "Detune", "Pulse Width"},
                                                                     {"Partials", "Tilt", "Odd / Even"},
                                                                     {"Material", "Damping", "Strike"},
                                                                     {"Start", "Loop", "Length"},
                                                                     {"Position", "Grain Size", "Scatter"}}};
const std::array<std::string_view, 5> filterNames{"Low-pass", "High-pass", "Band-pass", "Notch", "Bypass"};
const std::array<std::string_view, 4> waveNames{"Sine", "Triangle", "Ramp", "Square"};
const std::array<std::string_view, 6> tableNames{"Sine", "Silk", "Reed", "Vowel", "Alloy", "Fracture"};
const std::array<std::string_view, 6> scaleNames{"Minor",       "Dorian",     "Major",
                                                 "Minor Penta", "Whole Tone", "Chromatic"};
const std::array<std::string_view, int(Source::count)> sourceNames{
    "VELO",  "KBD",  "WHEEL", "ENV AMP", "ENV 2", "ENV 3", "LFO 1", "LFO 2",
    "LFO 3", "RAND", "STEP",  "M 1",     "M 2",   "M 3",   "M 4"};

const std::array<ParameterInfo, paramCount> &parameters()
{
    static const auto list = []
    {
        std::array<ParameterInfo, paramCount> infos;
        auto add = [&](int p, std::string id, std::string name, float v, int choices = 0, bool mod = true)
        { infos[std::size_t(p)] = {std::move(id), std::move(name), v, choices, mod}; };
        for (int layer = 0; layer < 2; ++layer)
        {
            const int offset = layer * bType;
            const std::string id = layer == 0 ? "a_" : "b_";
            const std::string name = layer == 0 ? "Engine 1 " : "Engine 2 ";
            add(offset + aType, id + "type", name + "Type", fromChoice(layer == 0 ? 0 : 3, 6), 6, false);
            add(offset + aTune, id + "tune", name + "Coarse", .5f, 49);
            add(offset + aFine, id + "fine", name + "Fine", .5f);
            add(offset + aShape, id + "shape", name + "Shape", layer == 0 ? .28f : .49f);
            add(offset + aColor, id + "color", name + "Color", layer == 0 ? .22f : .28f);
            add(offset + aMotion, id + "motion", name + "Motion", layer == 0 ? .14f : .25f);
            add(offset + aWidth, id + "width", name + "Stereo", .38f);
            add(offset + aLevel, id + "level", name + "Level", layer == 0 ? .59f : .25f);
            add(offset + aSend, id + "send", name + "Filter Mix", layer == 0 ? .28f : .73f);
        }
        add(subLevel, "sub_level", "Utility Sub", .17f);
        add(subShape, "sub_shape", "Utility Sub Shape", .04f);
        add(noiseLevel, "noise_level", "Utility Noise", .038f);
        add(noiseColor, "noise_color", "Utility Noise Color", .21f);
        add(utilitySend, "utility_send", "Utility Filter Mix", .53f);
        add(f1Type, "f1_type", "Filter 1 Mode", 0.f, 5, false);
        add(f1Cut, "f1_cut", "Filter 1 Cutoff", .63f);
        add(f1Res, "f1_res", "Filter 1 Resonance", .25f);
        add(f1Drive, "f1_drive", "Filter 1 Drive", .15f);
        add(f1Level, "f1_level", "Filter 1 Level", .95f);
        add(f2Type, "f2_type", "Filter 2 Mode", .25f, 5, false);
        add(f2Cut, "f2_cut", "Filter 2 Cutoff", .30f);
        add(f2Res, "f2_res", "Filter 2 Resonance", .28f);
        add(f2Drive, "f2_drive", "Filter 2 Drive", .10f);
        add(f2Level, "f2_level", "Filter 2 Level", .85f);
        add(filterRoute, "filter_route", "Series to Parallel", .77f);
        add(filterBlend, "filter_blend", "Filter Balance", .45f);
        add(ampVelocity, "amp_vel", "Velocity to Amp", .55f);
        add(voicePan, "voice_pan", "Voice Pan", .5f);
        const std::array<std::array<float, 4>, 3> env{
            {{1.05f, 1.7f, .72f, 4.6f}, {.62f, 2.4f, .17f, 3.25f}, {.06f, 1.20f, .36f, 3.4f}}};
        const std::array<std::string, 4> parts{"Attack", "Decay", "Sustain", "Release"};
        for (int e = 0; e < 3; ++e)
            for (int part = 0; part < 4; ++part)
            {
                const float value = env[std::size_t(e)][std::size_t(part)];
                add(attack + e * 4 + part, "env" + std::to_string(e + 1) + "_" + std::to_string(part),
                    (e == 0 ? "Amp " : "Envelope " + std::to_string(e + 1) + " ") + parts[std::size_t(part)],
                    part == 2 ? value : envelopeControl(value), 0, false);
            }
        for (int i = 0; i < 3; ++i)
        {
            const auto n = std::to_string(i + 1);
            add(lfo1Rate + i * 2, "lfo" + n + "_rate", "LFO " + n + " Rate", .33f + float(i) * .12f, 0,
                false);
            add(lfo1Shape + i * 2, "lfo" + n + "_shape", "LFO " + n + " Shape", i == 1 ? 1.f / 3.f : 0.f, 4,
                false);
        }
        add(randomRate, "random_rate", "Random Rate", .42f, 0, false);
        const std::array<std::string, 4> macroNames{"Timbre", "Motion", "Distance", "Space"};
        for (int i = 0; i < 4; ++i)
            add(macro1 + i, "macro" + std::to_string(i + 1),
                "Macro " + std::to_string(i + 1) + " " + macroNames[std::size_t(i)], .10f + float(i) * .035f,
                0, false);
        add(chorusRate, "chorus_rate", "Orbit Rate", .24f);
        add(chorusDepth, "chorus_depth", "Orbit Depth", .48f);
        add(chorusMix, "chorus_mix", "Orbit Mix", .28f);
        add(delayTime, "delay_time", "Drift Time", .45f);
        add(delayFeedback, "delay_fb", "Drift Feedback", .48f);
        add(delayMix, "delay_mix", "Drift Mix", .15f);
        add(roomSize, "room_size", "Space Size", .79f);
        add(roomDamp, "room_damp", "Space Damping", .42f);
        add(shimmer, "shimmer", "Space Shimmer", .13f);
        add(roomMix, "room_mix", "Space Mix", .35f);
        add(master, "master", "Master Volume", .67f, 0, false);
        add(polyphony, "poly", "Voice Limit", .5f, 3, false);
        add(seqTempo, "seq_tempo", "Sequencer Tempo", .30f, 161, false);
        add(seqLength, "seq_length", "Sequencer Length", 1.f, 16, false);
        add(seqRoot, "seq_root", "Sequencer Root", fromChoice(36, 73), 73, false);
        add(seqDivision, "seq_div", "Sequencer Division", .5f, 3, false);
        add(seqScale, "seq_scale", "Sequencer Scale", 0.f, 6, false);
        add(seqSwing, "seq_swing", "Sequencer Swing", .12f, 0, false);
        add(seqGate, "seq_gate", "Sequencer Gate", .69f, 0, false);
        const std::array<int, 16> pitches{0, 0, 7, 0, 3, 12, 0, 10, 0, 0, 7, 0, 15, 12, 0, -5};
        for (int step = 0; step < 16; ++step)
        {
            const auto n = std::to_string(step + 1);
            add(stepParameter(step, stepOn), "step" + n + "_on", "Step " + n + " On",
                step % 2 == 0 || step % 4 == 3 ? 1.f : 0.f, 2, false);
            add(stepParameter(step, stepPitch), "step" + n + "_pitch", "Step " + n + " Pitch",
                scalePitchValue(pitches[std::size_t(step)]), 49, false);
            add(stepParameter(step, stepVelocity), "step" + n + "_vel", "Step " + n + " Velocity",
                step % 4 == 0 ? .79f : .55f, 0, false);
            add(stepParameter(step, stepChance), "step" + n + "_chance", "Step " + n + " Chance",
                step % 4 == 0 ? 1.f : .74f, 0, false);
        }
        return infos;
    }();
    return list;
}

Patch initialPatch()
{
    Patch patch;
    for (int i = 0; i < paramCount; ++i)
        patch.values[std::size_t(i)] = parameters()[std::size_t(i)].initial;
    return patch;
}

const std::array<FactoryPatch, 11> &factory()
{
    static const auto patches = []
    {
        const auto base = initialPatch();
        std::array<FactoryPatch, 11> bank{
            {{"Harbor After Rain", "EVOLVING PAD", "A wavetable bed beneath a resonant metallic glow.", base},
             {"Tape Prayer", "GRAIN CLOUD", "Loose tape grains around a soft analog root.", base},
             {"Glass on the River", "MODAL KEYS", "Struck glass decays into an octave of light.", base},
             {"Salt Organ", "HARMONIC DRONE", "Breathing partials and a low, steady fifth.", base},
             {"Shortwave Dust", "FIELD TEXTURE", "Looped texture under wavetable noise and echo.", base},
             {"Concrete Bloom", "INHARMONIC PAD", "A dark metallic bloom through a narrow band.", base},
             {"Slow Buoy", "ANALOG PAD", "Soft detuned oscillators across the stereo field.", base},
             {"Votive", "SHIMMER DRONE", "Long overlapping grains feeding an octave reverb.", base},
             {"Moth Steps", "SEQUENCE", "Probability and short modal strikes.", base},
             {"Pale Transmission", "SAMPLE KEYS", "A pitched tape fragment in stereo echo.", base},
             {"Init", "STARTING POINT", "A single dry wavetable with room to build.", base}}};
        for (auto &entry : bank)
        {
            auto &routes = entry.patch.routes;
            routes[0] = {int(Source::lfo1), aShape, .18f};
            routes[1] = {int(Source::lfo2), f1Cut, .11f};
            routes[2] = {int(Source::env2), bLevel, .24f};
            routes[3] = {int(Source::macro1), aColor, .37f};
            routes[4] = {int(Source::macro2), bMotion, .44f};
            routes[5] = {int(Source::macro3), f1Cut, -.34f};
            routes[6] = {int(Source::macro4), roomMix, .46f};
            routes[7] = {int(Source::macro4), shimmer, .32f};
        }
        auto set = [&](int index, std::initializer_list<std::pair<int, float>> edits)
        {
            for (const auto &[param, value] : edits)
                bank[std::size_t(index)].patch.values[std::size_t(param)] = value;
        };
        auto type = [](EngineType t) { return fromChoice(int(t), 6); };
        set(1, {{aType, type(EngineType::granular)},
                {bType, type(EngineType::analog)},
                {aShape, .42f},
                {aColor, .76f},
                {aMotion, .39f},
                {aWidth, .84f},
                {bShape, .12f},
                {bTune, .25f},
                {bColor, .18f},
                {roomMix, .48f},
                {delayMix, .25f}});
        bank[1].patch.routes[2] = {int(Source::random), aShape, .22f};
        set(2, {{aType, type(EngineType::modal)},
                {bType, type(EngineType::harmonic)},
                {aShape, .83f},
                {aColor, .35f},
                {aMotion, .46f},
                {bTune, .75f},
                {bLevel, .17f},
                {bShape, .30f},
                {attack, envelopeControl(.023f)},
                {sustain, .27f},
                {f1Cut, .76f},
                {subLevel, .04f},
                {shimmer, .31f},
                {roomMix, .45f}});
        set(3, {{aType, type(EngineType::harmonic)},
                {bType, type(EngineType::analog)},
                {aShape, .77f},
                {aColor, .42f},
                {aMotion, .7f},
                {bShape, .32f},
                {bTune, fromChoice(31, 49)},
                {bLevel, .24f},
                {sustain, .89f},
                {release, envelopeControl(6.f)},
                {f1Cut, .66f},
                {chorusMix, .32f}});
        set(4, {{aType, type(EngineType::sample)},
                {aColor, 1.f},
                {aShape, .07f},
                {aMotion, .64f},
                {bType, type(EngineType::wavetable)},
                {bShape, .63f},
                {bTune, .25f},
                {bLevel, .2f},
                {noiseLevel, .13f},
                {noiseColor, .45f},
                {delayFeedback, .66f},
                {delayMix, .33f},
                {f2Type, .5f},
                {f2Cut, .57f}});
        bank[4].patch.routes[0] = {int(Source::random), f2Cut, .19f};
        set(5, {{aType, type(EngineType::modal)},
                {bType, type(EngineType::modal)},
                {aShape, .32f},
                {aMotion, .76f},
                {aColor, .17f},
                {bShape, .57f},
                {bColor, .25f},
                {bTune, .75f},
                {attack, envelopeControl(.31f)},
                {f1Type, .5f},
                {f1Cut, .55f},
                {f1Res, .62f},
                {filterRoute, .89f},
                {roomSize, .91f}});
        set(6, {{aType, type(EngineType::analog)},
                {bType, type(EngineType::analog)},
                {aShape, .33f},
                {aColor, .39f},
                {bShape, .24f},
                {bColor, .31f},
                {bTune, .25f},
                {bWidth, .8f},
                {f1Cut, .52f},
                {noiseLevel, .02f},
                {chorusMix, .37f},
                {roomMix, .39f},
                {delayMix, .04f}});
        set(7, {{aType, type(EngineType::granular)},
                {bType, type(EngineType::granular)},
                {aShape, .30f},
                {bShape, .58f},
                {aColor, .89f},
                {bColor, .75f},
                {aMotion, .53f},
                {bMotion, .38f},
                {bTune, .75f},
                {bLevel, .34f},
                {aWidth, .80f},
                {bWidth, .76f},
                {attack, envelopeControl(2.5f)},
                {release, envelopeControl(8.f)},
                {roomMix, .59f},
                {shimmer, .57f},
                {roomSize, .91f}});
        bank[7].patch.routes[0] = {int(Source::lfo1), aShape, .30f};
        set(8, {{aType, type(EngineType::modal)},
                {aShape, .78f},
                {aColor, .75f},
                {bType, type(EngineType::wavetable)},
                {bLevel, .19f},
                {bShape, .30f},
                {attack, envelopeControl(.007f)},
                {decay, envelopeControl(.26f)},
                {sustain, .18f},
                {release, envelopeControl(1.04f)},
                {seqTempo, .37f},
                {seqSwing, .28f},
                {seqLength, fromChoice(12, 16)},
                {roomMix, .28f},
                {delayMix, .24f}});
        bank[8].patch.routes[2] = {int(Source::step), f2Cut, .19f};
        set(9, {{aType, type(EngineType::sample)},
                {aShape, .09f},
                {aColor, 0.f},
                {aMotion, .76f},
                {bType, type(EngineType::harmonic)},
                {bShape, .25f},
                {bLevel, .20f},
                {attack, envelopeControl(.036f)},
                {decay, envelopeControl(.81f)},
                {sustain, .4f},
                {delayTime, .56f},
                {delayFeedback, .68f},
                {delayMix, .4f}});
        set(10, {{aType, 0.f},
                 {aShape, 0.f},
                 {aColor, 0.f},
                 {aMotion, 0.f},
                 {aWidth, 0.f},
                 {aSend, 0.f},
                 {bLevel, 0.f},
                 {subLevel, 0.f},
                 {noiseLevel, 0.f},
                 {f1Cut, .96f},
                 {f2Type, 1.f},
                 {filterRoute, 1.f},
                 {filterBlend, 0.f},
                 {attack, envelopeControl(.012f)},
                 {decay, envelopeControl(.38f)},
                 {sustain, .78f},
                 {release, envelopeControl(.28f)},
                 {chorusMix, 0.f},
                 {delayMix, 0.f},
                 {roomMix, 0.f},
                 {shimmer, 0.f}});
        bank[10].patch.routes = {};
        return bank;
    }();
    return patches;
}
bool Route::valid() const
{
    return source >= 0 && source < int(Source::count) && target >= 0 && target < paramCount &&
           parameters()[std::size_t(target)].modTarget && std::isfinite(amount);
}
int pick(float v, int count)
{
    return int(std::lround(std::clamp(v, 0.f, 1.f) * float(count - 1)));
}
float fromChoice(int v, int count)
{
    return float(v) / float(count - 1);
}
float seconds(float v)
{
    return .003f * std::exp2(v * 12.f);
}
float envelopeControl(float t)
{
    return std::clamp(std::log2(t / .003f) / 12.f, 0.f, 1.f);
}
float filterFrequency(float v)
{
    return 28.f * std::exp2(v * 9.48f);
}
float lfoFrequency(float v)
{
    return .018f * std::exp2(v * 10.2f);
}
float delaySeconds(float v)
{
    return .027f + v * v * 1.6f;
}
float midiHz(int n)
{
    return 440.f * std::exp2(float(n - 69) / 12.f);
}
int voiceLimit(float v)
{
    return 4 + pick(v, 3) * 4;
}
float scalePitchValue(int n)
{
    return fromChoice(std::clamp(n + 24, 0, 48), 49);
}
int scaleOffset(int scale, int degree)
{
    static constexpr std::array<int, 6> sizes{7, 7, 7, 5, 6, 12};
    static constexpr std::array<std::array<int, 12>, 6> notes{{{0, 2, 3, 5, 7, 8, 10},
                                                               {0, 2, 3, 5, 7, 9, 10},
                                                               {0, 2, 4, 5, 7, 9, 11},
                                                               {0, 3, 5, 7, 10},
                                                               {0, 2, 4, 6, 8, 10},
                                                               {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}}};
    const auto s = std::size_t(std::clamp(scale, 0, 5));
    const int count = sizes[s], within = (degree % count + count) % count;
    return 12 * ((degree - within) / count) + notes[s][std::size_t(within)];
}
std::uint32_t packRoute(Route r)
{
    if (!r.valid())
        return 0;
    const auto depth = std::uint32_t(std::lround((std::clamp(r.amount, -1.f, 1.f) + 1.f) * 32767.f));
    return std::uint32_t(r.source + 1) | (std::uint32_t(r.target) << 5u) | (depth << 13u);
}
Route unpackRoute(std::uint32_t bits)
{
    if ((bits & 31u) == 0)
        return {};
    Route r{int(bits & 31u) - 1, int((bits >> 5u) & 255u), float((bits >> 13u) & 65535u) / 32767.f - 1.f};
    return r.valid() ? r : Route{};
}
} // namespace mica
