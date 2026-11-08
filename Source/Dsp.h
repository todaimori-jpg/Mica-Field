// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Patch.h"
#include <vector>

namespace mica
{
struct Stereo
{
    float left = 0.f, right = 0.f;
    Stereo operator+(Stereo b) const { return {left + b.left, right + b.right}; }
    Stereo operator*(float gain) const { return {left * gain, right * gain}; }
    Stereo &operator+=(Stereo b)
    {
        left += b.left;
        right += b.right;
        return *this;
    }
};
struct SampleClip
{
    std::string name;
    std::vector<float> data;
    std::array<float, 144> preview{};
    double rate = storageSampleRate;
    std::uint64_t revision = 0;
    void buildPreview();
};
const SampleClip &builtinClip();
float oscillatorTable(int frame, float phase, float phaseStep = 0.f);
float oscillatorShape(float phase, float position, float pulseWidth, float phaseStep = 0.f);
float lfoWave(int type, float phase);

class Synth
{
  public:
    void prepare(double sampleRate);
    void beginBlock(const Patch &, const std::array<const SampleClip *, 2> &, bool runSequencer);
    void render(float *left, float *right, int frames);
    void noteOn(int channel, int note, float velocity);
    void noteOff(int channel, int note);
    void sustainPedal(int channel, bool down);
    void pitchWheel(int channel, float bend);
    void modWheel(int channel, float value);
    void allNotesOff(int channel = 0);
    void panic();
    float peakLeft() const { return meterL; }
    float peakRight() const { return meterR; }
    int voicesPlaying() const;
    int currentStep() const { return seqIndex; }
    const std::array<float, int(Source::count)> &sourceLevels() const { return sources; }

  private:
    struct Envelope
    {
        enum Stage
        {
            idle,
            rise,
            fall,
            hold,
            tail
        };
        Stage stage = idle;
        float position = 0.f, value = 0.f, initial = 0.f;
        void start(float level);
        void off();
        void tick(const std::array<float, 4> &);
    };
    struct Filter
    {
        std::array<Stereo, 2> state{};
        float g = 0.f, h = 1.f, k = 1.f, drive = 0.f, gain = 1.f;
        int type = 0;
        void update(float frequency, float resonance, float newDrive, float level, int mode, float rate);
        Stereo tick(Stereo);
    };
    struct Grain
    {
        float position = 0.f, progress = 2.f, increment = 1.f, pan = 0.f;
    };
    struct Layer
    {
        float phase = 0.f, lowerPhase = 0.f, upperPhase = 0.f;
        float freq = 0.f, down = 0.f, up = 0.f, pitchRate = 0.f;
        float shape = 0.f, color = 0.f, motion = 0.f, width = 0.f, level = 0.f, send = 0.f;
        float playhead = 0.f, grainCountdown = 0.f, hit = 1.f, hitFall = .999f;
        int type = -1, partialCount = 0;
        std::uint64_t revision = 0;
        std::array<float, 12> weights{};
        std::array<float, 6> modePhase{}, modeStep{}, modeEnergy{}, modeFall{};
        std::array<Grain, 4> grains{};
        void update(const float *, float fundamental, float rate, const SampleClip &);
        Stereo tick(const SampleClip &, float rate, std::uint32_t &seed);
    };
    struct Voice
    {
        int note = -1, channel = 0;
        std::uint64_t age = 0;
        bool keyDown = false;
        float velocity = 0.f, frequency = 0.f, subPhase = 0.f, noiseState = 0.f;
        float route = 0.f, blend = 0.f, pan = 0.f, velocityGain = 1.f;
        float sub = 0.f, subShape = 0.f, noise = 0.f, noiseTone = 0.f, utilitySend = .5f;
        std::uint32_t seed = 1;
        std::array<Envelope, 3> envelopes{};
        std::array<Layer, 2> layers{};
        std::array<Filter, 2> filters{};
        void off();
        Stereo tick(const std::array<const SampleClip *, 2> &, const std::array<std::array<float, 4>, 3> &,
                    float rate);
    };
    struct Delay
    {
        std::vector<float> data;
        std::size_t cursor = 0;
        void prepare(int frames);
        void clear();
        float read(float frames) const;
        void write(float);
    };
    struct Effects
    {
        std::array<Delay, 2> chorusLine, echoLine, octaveLine;
        std::array<Delay, 8> spaceLine;
        std::array<float, 8> damping{};
        float chorusPhase = 0.f, octavePhase = 0.f, delayFrames = 1.f;
        void prepare(float rate);
        void clear();
        Stereo tick(Stereo, const std::array<float, paramCount> &, float rate);
    };
    void controlTick();
    void updateSources(const Voice *, std::array<float, int(Source::count)> &) const;
    void updateVoice(Voice &);
    void sequenceTick();
    float rate = 48000.f, inverseRate = 1.f / 48000.f;
    Patch patch;
    std::array<const SampleClip *, 2> clips{};
    std::array<Voice, maxVoices> voices{};
    std::array<std::array<float, 4>, 3> envCoefficients{};
    std::array<float, 3> lfoPhases{};
    std::array<float, 17> bends{}, wheels{};
    std::array<bool, 17> pedals{};
    std::array<float, int(Source::count)> sources{};
    std::array<float, paramCount> fxValues{};
    std::array<float, 128> frequencies{};
    Effects effects;
    int controlCountdown = 0;
    std::uint64_t serial = 0;
    std::uint32_t randomSeed = 148651u, sequenceSeed = 91257u;
    float randomPhase = 0.f, randomValue = 0.f, stepValue = 0.f;
    bool sequencePlaying = false;
    int seqIndex = -1, seqNote = -1, seqGateFrames = 0;
    double seqRemaining = 0.;
    float outputGain = 0.f, meterL = 0.f, meterR = 0.f;
    int quietFrames = 0;
};
} // namespace mica
