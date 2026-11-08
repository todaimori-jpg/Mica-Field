// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace mica
{
enum class EngineType
{
    wavetable,
    analog,
    harmonic,
    modal,
    sample,
    granular
};
enum class FilterType
{
    lowPass,
    highPass,
    bandPass,
    notch,
    bypass
};
enum class Source
{
    velocity,
    keytrack,
    wheel,
    amp,
    env2,
    env3,
    lfo1,
    lfo2,
    lfo3,
    random,
    step,
    macro1,
    macro2,
    macro3,
    macro4,
    count
};

enum Param : int
{
    aType,
    aTune,
    aFine,
    aShape,
    aColor,
    aMotion,
    aWidth,
    aLevel,
    aSend,
    bType,
    bTune,
    bFine,
    bShape,
    bColor,
    bMotion,
    bWidth,
    bLevel,
    bSend,
    subLevel,
    subShape,
    noiseLevel,
    noiseColor,
    utilitySend,
    f1Type,
    f1Cut,
    f1Res,
    f1Drive,
    f1Level,
    f2Type,
    f2Cut,
    f2Res,
    f2Drive,
    f2Level,
    filterRoute,
    filterBlend,
    ampVelocity,
    voicePan,
    attack,
    decay,
    sustain,
    release,
    env2Attack,
    env2Decay,
    env2Sustain,
    env2Release,
    env3Attack,
    env3Decay,
    env3Sustain,
    env3Release,
    lfo1Rate,
    lfo1Shape,
    lfo2Rate,
    lfo2Shape,
    lfo3Rate,
    lfo3Shape,
    randomRate,
    macro1,
    macro2,
    macro3,
    macro4,
    chorusRate,
    chorusDepth,
    chorusMix,
    delayTime,
    delayFeedback,
    delayMix,
    roomSize,
    roomDamp,
    shimmer,
    roomMix,
    master,
    polyphony,
    seqTempo,
    seqLength,
    seqRoot,
    seqDivision,
    seqScale,
    seqSwing,
    seqGate,
    stepBase,
    paramCount = stepBase + 16 * 4
};
enum StepPart
{
    stepOn,
    stepPitch,
    stepVelocity,
    stepChance
};
constexpr int maxVoices = 12, maxRoutes = 16, maxSteps = 16;
constexpr int sampleSeconds = 8, storageSampleRate = 48000;
constexpr int maxClipFrames = sampleSeconds * storageSampleRate;
constexpr int stepParameter(int step, StepPart part)
{
    return stepBase + step * 4 + part;
}
constexpr int layerParameter(int layer, Param param)
{
    return int(param) + layer * int(bType);
}

struct ParameterInfo
{
    std::string id, name;
    float initial = 0.f;
    int choices = 0;
    bool modTarget = true;
};
struct Route
{
    int source = -1, target = -1;
    float amount = 0.f;
    bool valid() const;
};
struct Patch
{
    std::array<float, paramCount> values{};
    std::array<Route, maxRoutes> routes{};
};
struct FactoryPatch
{
    std::string_view name, family, description;
    Patch patch;
};

const std::array<ParameterInfo, paramCount> &parameters();
const std::array<FactoryPatch, 11> &factory();
Patch initialPatch();
int pick(float value, int choices);
float fromChoice(int choice, int choices);
float seconds(float value);
float envelopeControl(float time);
float filterFrequency(float value);
float lfoFrequency(float value);
float delaySeconds(float value);
float midiHz(int note);
int voiceLimit(float value);
int scaleOffset(int scale, int degree);
float scalePitchValue(int semitones);
std::uint32_t packRoute(Route route);
Route unpackRoute(std::uint32_t bits);

extern const std::array<std::string_view, 6> engineNames, tableNames, scaleNames;
extern const std::array<std::array<std::string_view, 3>, 6> engineControls;
extern const std::array<std::string_view, 5> filterNames;
extern const std::array<std::string_view, 4> waveNames;
extern const std::array<std::string_view, int(Source::count)> sourceNames;
} // namespace mica
