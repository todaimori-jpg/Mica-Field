// SPDX-License-Identifier: AGPL-3.0-only
#include "../Source/Dsp.h"
#include <emscripten/emscripten.h>
#include <algorithm>
#include <memory>

namespace
{
mica::Synth synth;
mica::Patch patch;
std::array<mica::SampleClip, 2> clips;
std::array<bool, 2> custom {};
std::array<float, 512> left {}, right {};
std::array<float, 15> modulation {};
std::uint64_t revision = 0;
bool sequence = false;
}
extern "C"
{
EMSCRIPTEN_KEEPALIVE void mica_init(double rate)
{
    synth.prepare(rate);
    patch = mica::factory().front().patch;
    patch.values[mica::polyphony] = 0.f;
    synth.beginBlock(patch, {nullptr, nullptr}, false);
    synth.render(left.data(), right.data(), 1);
}
EMSCRIPTEN_KEEPALIVE void mica_param(int id, float value)
{
    if (id >= 0 && id < mica::paramCount)
        patch.values[std::size_t(id)] = std::clamp(value, 0.f, 1.f);
}
EMSCRIPTEN_KEEPALIVE void mica_route(int slot, int source, int target, float amount)
{
    if (slot < 0 || slot >= mica::maxRoutes) return;
    mica::Route route {source, target, amount};
    patch.routes[std::size_t(slot)] = route.valid() ? route : mica::Route {};
}
EMSCRIPTEN_KEEPALIVE void mica_note(int on, int note, float velocity)
{
    if (on) synth.noteOn(1, note, velocity);
    else synth.noteOff(1, note);
}
EMSCRIPTEN_KEEPALIVE void mica_sequence(int enabled) { sequence = enabled != 0; }
EMSCRIPTEN_KEEPALIVE void mica_panic() { sequence = false; synth.panic(); }
EMSCRIPTEN_KEEPALIVE float* mica_clip_buffer(int slot, int frames)
{
    if (slot < 0 || slot > 1 || frames < 3 || frames > mica::maxClipFrames) return nullptr;
    clips[std::size_t(slot)].data.resize(std::size_t(frames));
    return clips[std::size_t(slot)].data.data();
}
EMSCRIPTEN_KEEPALIVE void mica_clip_ready(int slot)
{
    if (slot < 0 || slot > 1) return;
    auto& clip = clips[std::size_t(slot)];
    clip.rate = mica::storageSampleRate;
    clip.revision = ++revision;
    custom[std::size_t(slot)] = true;
}
EMSCRIPTEN_KEEPALIVE void mica_clip_clear(int slot)
{
    if (slot >= 0 && slot < 2) custom[std::size_t(slot)] = false;
}
EMSCRIPTEN_KEEPALIVE void mica_render(int frames)
{
    frames = std::clamp(frames, 0, 512);
    synth.beginBlock(patch, {custom[0] ? &clips[0] : nullptr, custom[1] ? &clips[1] : nullptr}, sequence);
    synth.render(left.data(), right.data(), frames);
    modulation = synth.sourceLevels();
}
EMSCRIPTEN_KEEPALIVE float* mica_left() { return left.data(); }
EMSCRIPTEN_KEEPALIVE float* mica_right() { return right.data(); }
EMSCRIPTEN_KEEPALIVE float* mica_sources() { return modulation.data(); }
EMSCRIPTEN_KEEPALIVE int mica_voices() { return synth.voicesPlaying(); }
EMSCRIPTEN_KEEPALIVE int mica_step() { return synth.currentStep(); }
}
