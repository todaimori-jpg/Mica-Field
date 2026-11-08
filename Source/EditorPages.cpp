// SPDX-License-Identifier: AGPL-3.0-only
#include "Editor.h"
#include <algorithm>
#include <cmath>

namespace mica
{
namespace
{
constexpr float pi = 3.14159265358979323846f, twoPi = 2.f * pi;
constexpr std::array<std::string_view, 4> macros{"Timbre", "Motion", "Distance", "Space"};
constexpr std::array<std::string_view, 4> envelopeLabels{"Attack", "Decay", "Sustain", "Release"};
constexpr std::array<std::string_view, 5> lowerNames{"KEYBOARD", "ENVELOPES", "LFO", "RANDOM", "ROUTES"};
constexpr std::array<std::string_view, 4> pageNames{"PLAY", "SYNTH", "FX", "SEQ"};
bool blackKey(int note)
{
    const int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}
} // namespace
void Editor::drawTop(juce::Graphics &g)
{
    module(g, {4.f, 3.f, 1232.f, 42.f}, panel);
    graphicalButton(g, {10.f, 8.f, 37.f, 32.f}, "menu", Kind::menu, 0, "Patch and audio menu", ink);
    label(g, "MICA FIELD", {54.f, 5.f, 170.f, 37.f}, cyan, 19.f, juce::Justification::centredLeft);
    line(g, {171.f, 16.f}, {171.f, 32.f}, hairline);
    label(g, "INSTRUMENT 01", {184.f, 6.f, 115.f, 37.f}, dim, 9.5f, juce::Justification::centredLeft, true);
    module(g, {441.f, 10.f, 316.f, 28.f}, header);
    graphicalButton(g, {446.f, 10.f, 34.f, 28.f}, "book", Kind::presetMenu, 0, "Factory bank and patch files",
                    muted);
    line(g, {480.f, 15.f}, {480.f, 32.f}, hairline.withAlpha(.57f));
    label(g, processor.patchName() + (processor.isEdited() ? "  *" : ""), {484.f, 10.f, 209.f, 28.f}, ink,
          14.f);
    addHit(Kind::presetMenu, 0, {480.f, 10.f, 216.f, 28.f}, "Open the preset bank");
    graphicalButton(g, {696.f, 10.f, 29.f, 28.f}, "left", Kind::presetPrevious, 0, "Previous factory patch",
                    muted);
    graphicalButton(g, {726.f, 10.f, 26.f, 28.f}, "right", Kind::presetNext, 0, "Next factory patch", ink);
    for (int i = 0; i < 4; ++i)
        button(g, {780.f + float(i) * 57.f, 12.f, 52.f, 24.f}, str(pageNames[std::size_t(i)]), Kind::page, i,
               int(page) == i, i == int(Page::sequence) && processor.sequencerOn() ? mint : cyan);
    const bool rec = processor.isRecording();
    graphicalButton(g, {1021.f, 11.f, 26.f, 26.f}, rec ? "stop" : "record", Kind::record, 0,
                    rec ? "Stop and save recording" : "Record a stereo WAV", rec ? pink : muted, rec);
    label(g, rec ? juce::String(int(processor.recordedSeconds())) + " s" : "REC", {1046.f, 11.f, 33.f, 26.f},
          rec ? pink : dim, 9.5f, juce::Justification::centredLeft, true);
    addHit(Kind::record, 0, {1047.f, 11.f, 33.f, 26.f}, "Record stereo WAV");
    dial(g, {1102.f, 24.f}, processor.value(master), cyan, 15.f,
         hover.kind == Kind::param && hover.index == master);
    addHit(Kind::param, master, {1083.f, 5.f, 38.f, 38.f}, "Master volume");
    for (int i = 0; i < 2; ++i)
    {
        const float v =
            std::clamp(std::sqrt(i == 0 ? processor.peakL.load() : processor.peakR.load()) * 1.36f, 0.f, 1.f);
        const Rect r(1126.f + float(i) * 6.f, 11.f, 3.f, 26.f);
        fill(g, r, base);
        fill(g, r.withTop(r.getBottom() - r.getHeight() * v), cyan.withAlpha(.85f));
    }
    label(g, processor.isStandalone() ? "AUDIO" : "VST3", {1144.f, 9.f, 49.f, 30.f}, dim, 9.5f,
          juce::Justification::centred, true);
    if (processor.isStandalone())
    {
        graphicalButton(g, {1196.f, 8.f, 31.f, 32.f}, "settings", Kind::audio, 0, "Audio and MIDI settings",
                        muted);
        addHit(Kind::audio, 0, {1144.f, 8.f, 52.f, 32.f}, "Audio and MIDI settings");
    }
}
void Editor::drawSynth(juce::Graphics &g)
{
    module(g, {4.f, 49.f, 760.f, 321.f});
    for (int i = 0; i < 2; ++i)
    {
        const float x = 6.f + float(i) * 296.f;
        const bool active = activeLayer == i;
        const Rect tab(x, 51.f, 294.f, 26.f);
        module(g, tab, active ? raised : header);
        if (active)
            fill(g, tab.withHeight(1.2f), cyan.withAlpha(.82f));
        label(g, "ENGINE " + juce::String(i + 1), {x + 34.f, 52.f, 76.f, 24.f}, active ? ink : muted, 11.f,
              juce::Justification::centredLeft, true);
        addHit(Kind::layer, i, tab, "Edit Engine " + juce::String(i + 1));
        graphicalButton(g, {x + 4.f, 51.f, 28.f, 26.f}, "power", Kind::layerPower, i, "Toggle engine output",
                        processor.value(layerParameter(i, aLevel)) > .001f ? cyan : dim);
        const Rect mode(x + 114.f, 54.f, 130.f, 20.f);
        label(g, str(engineNames[std::size_t(selectedEngine(i))]), mode, active ? ink : muted, 12.f,
              juce::Justification::centredLeft);
        graphicalButton(g, {x + 243.f, 52.f, 40.f, 24.f}, "down", Kind::choice, layerParameter(i, aType),
                        "Choose engine type", muted);
        addHit(Kind::choice, layerParameter(i, aType), mode, "Choose engine type");
    }
    const int type = selectedEngine(activeLayer);
    label(g, "TUNE", {25.f, 86.f, 57.f, 18.f}, ink, 10.5f, juce::Justification::centred, true);
    line(g, {97.f, 95.f}, {97.f, 260.f}, hairline.withAlpha(.6f));
    knob(g, {53.f, 136.f}, layerParam(aTune), "Coarse", cyan);
    knob(g, {53.f, 211.f}, layerParam(aFine), "Fine", cyan);
    const bool samples = type == int(EngineType::sample) || type == int(EngineType::granular);
    const auto family = type == int(EngineType::wavetable)
                            ? tableNames[std::size_t(pick(processor.value(layerParam(aShape)), 6))]
                            : engineNames[std::size_t(type)];
    label(g, str(family).toUpperCase(), {113.f, 87.f, 196.f, 19.f}, cyan, 10.5f,
          juce::Justification::centredLeft, true);
    if (samples)
    {
        const auto clip = processor.sample(activeLayer);
        button(g, {377.f, 84.f, 201.f, 22.f},
               (clip != nullptr ? juce::String(clip->name) : "Weathered Tape") + "   v", Kind::loadSample,
               activeLayer);
    }
    drawEngineGraph(g, {112.f, 110.f, 466.f, 154.f}, type);
    fill(g, {12.f, 275.f, 581.f, 1.f}, hairline.withAlpha(.65f));
    label(g, "SHAPE", {16.f, 277.f, 270.f, 15.f}, muted, 9.5f, juce::Justification::centred, true);
    label(g, "OUTPUT", {292.f, 277.f, 298.f, 15.f}, muted, 9.5f, juce::Justification::centred, true);
    line(g, {288.f, 279.f}, {288.f, 360.f}, hairline.withAlpha(.55f));
    const auto &names = engineControls[std::size_t(type)];
    knob(g, {62.f, 319.f}, layerParam(aShape), str(names[0]), cyan);
    knob(g, {150.f, 319.f}, layerParam(aColor), str(names[1]), cyan);
    knob(g, {237.f, 319.f}, layerParam(aMotion), str(names[2]), cyan);
    knob(g, {340.f, 319.f}, layerParam(aWidth), "Stereo", cyan);
    knob(g, {441.f, 319.f}, layerParam(aLevel), "Volume", cyan);
    knob(g, {543.f, 319.f}, layerParam(aSend), "Filter Mix", cyan);
    module(g, {598.f, 51.f, 164.f, 317.f}, raised.darker(.08f));
    module(g, {599.f, 51.f, 162.f, 26.f}, header);
    label(g, "UTILITY ENGINE", {606.f, 52.f, 149.f, 24.f}, ink, 11.f, juce::Justification::centred, true);
    label(g, "SUB", {605.f, 89.f, 73.f, 16.f}, muted, 10.5f, juce::Justification::centred, true);
    label(g, "NOISE", {681.f, 89.f, 73.f, 16.f}, muted, 10.5f, juce::Justification::centred, true);
    trace(g, {615.f, 107.f, 54.f, 30.f}, cyan.withAlpha(.67f), [](float x) { return std::sin(x * twoPi); });
    trace(g, {690.f, 107.f, 53.f, 30.f}, cyan.withAlpha(.67f),
          [](float x) { return std::sin(x * 131.f) * std::cos(x * 57.f) * .76f; });
    knob(g, {642.f, 163.f}, subLevel, "Volume", cyan, 14.f);
    knob(g, {719.f, 163.f}, noiseLevel, "Volume", cyan, 14.f);
    knob(g, {642.f, 230.f}, subShape, "Shape", cyan, 14.f);
    knob(g, {719.f, 230.f}, noiseColor, "Color", cyan, 14.f);
    line(g, {608.f, 274.f}, {752.f, 274.f}, hairline.withAlpha(.65f));
    knob(g, {681.f, 314.f}, utilitySend, "Filter Mix", cyan, 15.f);
    drawFilter(g, 0, {768.f, 49.f, 360.f, 158.f});
    drawFilter(g, 1, {768.f, 211.f, 360.f, 159.f});
    module(g, {1132.f, 49.f, 104.f, 321.f});
    label(g, "FILTER ROUTING", {1137.f, 54.f, 94.f, 18.f}, ink, 10.5f, juce::Justification::centred, true);
    knob(g, {1184.f, 93.f}, filterRoute, "Routing", cyan, 14.f);
    line(g, {1141.f, 141.f}, {1227.f, 141.f}, hairline);
    knob(g, {1184.f, 164.f}, filterBlend, "F1 / F2 Blend", cyan, 14.f);
    line(g, {1141.f, 209.f}, {1227.f, 209.f}, hairline);
    knob(g, {1184.f, 232.f}, ampVelocity, "Amp Mod", cyan, 14.f);
    line(g, {1141.f, 279.f}, {1227.f, 279.f}, hairline);
    knob(g, {1184.f, 306.f}, voicePan, "Voice Pan", cyan, 14.f);
}
void Editor::drawEngineGraph(juce::Graphics &g, Rect r, int type)
{
    const float a = processor.value(layerParam(aShape)), b = processor.value(layerParam(aColor)),
                m = processor.value(layerParam(aMotion));
    const float lfo = animate ? processor.sources[std::size_t(Source::lfo1)].load() : 0.f;
    g.setGradientFill(juce::ColourGradient(base.withAlpha(.03f), r.getX(), r.getY(), base.withAlpha(.39f),
                                           r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.f);
    if (type == int(EngineType::wavetable))
    {
        for (int i = 14; i >= 0; --i)
        {
            const float z = float(i) / 14.f;
            const Rect depth(r.getX() + 34.f + 139.f * z, r.getY() + 72.f - 57.f * z,
                             r.getWidth() - 205.f - 17.f * z, 65.f - 18.f * z);
            const float position = std::clamp((a + z * .53f + lfo * .025f) * 5.f, 0.f, 5.f),
                        blend = position - float(int(position));
            const int frame = int(position);
            trace(
                g, depth, cyan.interpolatedWith(Colour(0xff16667c), z).withAlpha((1.f - z) * .67f + .18f),
                [frame, blend, b, m, z](float x)
                {
                    const float phase = x + b * .115f * std::sin(twoPi * x + m) + z * .026f;
                    return oscillatorTable(frame, phase) * (1.f - blend) +
                           oscillatorTable(frame + 1, phase) * blend;
                },
                i % 2 == 0);
        }
        label(g, "01", {r.getX() + 5.f, r.getBottom() - 17.f, 29.f, 14.f}, dim, 9.f);
        label(g, "FRAME " + juce::String(1 + pick(a, 6)),
              {r.getRight() - 86.f, r.getBottom() - 18.f, 82.f, 16.f}, cyan.withAlpha(.82f), 9.5f);
        return;
    }
    const auto plot = r.reduced(12.f, 16.f);
    graphGrid(g, plot, 12, 4);
    if (type == int(EngineType::sample) || type == int(EngineType::granular))
    {
        const auto imported = processor.sample(activeLayer);
        const auto &wave = imported != nullptr ? imported->preview : builtinClip().preview;
        const bool grains = type == int(EngineType::granular);
        const float start = grains ? a : a * .91f, startX = plot.getX() + plot.getWidth() * start;
        const float selection = grains ? .06f + b * .16f : (1.f - start) * (.11f + m * .89f);
        fill(g,
             {startX, plot.getY(), std::min(plot.getRight() - startX, selection * plot.getWidth()),
              plot.getHeight()},
             cyan.withAlpha(.13f));
        for (std::size_t i = 0; i < wave.size(); ++i)
        {
            const float x = plot.getX() + (float(i) + .5f) * plot.getWidth() / float(wave.size()),
                        y = plot.getHeight() * .48f * wave[i];
            line(g, {x, plot.getCentreY() - y}, {x, plot.getCentreY() + y}, cyan.withAlpha(.55f), 1.4f);
        }
        line(g, {startX, plot.getY()}, {startX, plot.getBottom()}, cyan, 1.2f);
        label(g,
              grains     ? "GRAIN WINDOW"
              : b >= .5f ? "LOOP REGION"
                         : "ONE SHOT",
              {plot.getX() + 4.f, plot.getY(), 104.f, 16.f}, cyan, 9.f, juce::Justification::centredLeft,
              true);
        return;
    }
    if (type == int(EngineType::analog))
    {
        for (int i = 2; i >= 0; --i)
            trace(
                g, plot.reduced(4.f, 14.f), cyan.withAlpha(.96f - float(i) * .23f),
                [a, b, m, i](float x) {
                    return oscillatorShape(std::fmod(x * (2.f + b * float(i) * .2f) + float(i) * .105f, 1.f),
                                           a, m);
                },
                i == 0);
        return;
    }
    const bool modal = type == int(EngineType::modal);
    for (int i = 0; i < (modal ? 6 : 2 + int(a * 10.999f)); ++i)
    {
        const float index = float(i + 1), u = modal ? std::pow(index / 7.4f, 1.17f + a * .49f) : index / 13.f;
        const float h =
            plot.getHeight() * (.86f - b * .22f) / std::pow(index, modal ? .49f : 1.34f - b * .95f);
        const float x = plot.getX() + plot.getWidth() * u;
        const Rect bar(x - 7.f, plot.getBottom() - h, 14.f, h);
        g.setGradientFill(juce::ColourGradient(cyan.withAlpha(.71f), x, bar.getY(), cyan.withAlpha(.04f), x,
                                               bar.getBottom(), false));
        g.fillRoundedRectangle(bar, 2.f);
        line(g, {bar.getX(), bar.getY()}, {bar.getRight(), bar.getY()}, cyan.withAlpha(.83f));
    }
}
void Editor::drawFilter(juce::Graphics &g, int filter, Rect r)
{
    module(g, r);
    const int offset = filter == 0 ? 0 : f2Type - f1Type, id = f1Type + offset,
              mode = pick(processor.value(id), 5);
    const float x = r.getX(), y = r.getY();
    const Rect area(x + 120.f, y + 4.f, 219.f, 20.f);
    graphicalButton(g, {x + 6.f, y + 3.f, 23.f, 22.f}, "power", Kind::filterPower, id, "Toggle filter bypass",
                    mode == int(FilterType::bypass) ? dim : cyan);
    label(g, "FILTER " + juce::String(filter + 1), {x + 28.f, y + 4.f, 82.f, 20.f}, ink, 11.f,
          juce::Justification::centredLeft, true);
    label(g, str(filterNames[std::size_t(mode)]).toUpperCase(), area, ink, 10.5f,
          juce::Justification::centred, true);
    graphicalButton(g, {x + 329.f, y + 4.f, 26.f, 20.f}, "down", Kind::choice, id, "Choose filter mode",
                    muted);
    addHit(Kind::choice, id, area, "Choose filter mode");
    knob(g, {x + 34.f, y + 56.f}, f1Cut + offset, "Cutoff", cyan, 14.f);
    knob(g, {x + 86.f, y + 56.f}, f1Res + offset, "Resonance", cyan, 14.f);
    knob(g, {x + 34.f, y + 114.f}, f1Drive + offset, "Drive", cyan, 13.f);
    knob(g, {x + 325.f, y + 56.f}, f1Level + offset, "Volume", cyan, 14.f);
    const Rect graph(x + 119.f, y + 37.f, 173.f, 101.f);
    graphGrid(g, graph, 8, 4);
    const float cutoff = processor.value(f1Cut + offset), resonance = processor.value(f1Res + offset);
    trace(
        g, graph, mode == int(FilterType::bypass) ? dim : cyan,
        [cutoff, resonance, mode](float f)
        {
            if (mode == int(FilterType::bypass))
                return .23f;
            const float x = (f - cutoff) * 8.2f, turn = 1.f / (1.f + std::exp(3.f * x)),
                        peak = std::exp(-x * x * 2.6f) * resonance * .89f;
            if (mode == int(FilterType::highPass))
                return .55f - turn * 1.42f + peak;
            if (mode == int(FilterType::bandPass))
                return -.84f + std::exp(-x * x * .58f) * (1.28f + resonance * .35f);
            if (mode == int(FilterType::notch))
                return .50f - std::exp(-x * x * 1.2f) * 1.22f;
            return turn * 1.40f - .85f + peak;
        },
        true);
    label(g, "FREQ", {graph.getX(), graph.getBottom() + 2.f, 42.f, 12.f}, dim, 8.5f,
          juce::Justification::centredLeft, true);
}
void Editor::drawEffects(juce::Graphics &g)
{
    module(g, {4.f, 49.f, 1232.f, 321.f});
    module(g, {7.f, 52.f, 142.f, 315.f}, header);
    label(g, "SIGNAL PATH", {10.f, 58.f, 133.f, 28.f}, ink, 11.f, juce::Justification::centred, true);
    const std::array<juce::String, 3> buses{"INSERT A", "INSERT B", "SEND"},
        names{"ORBIT CHORUS", "DRIFT DELAY", "SPACE REVERB"};
    const std::array<int, 3> mixes{chorusMix, delayMix, roomMix};
    for (int i = 0; i < 3; ++i)
    {
        const float y = 89.f + float(i) * 85.f, x = 154.f + float(i) * 360.f;
        line(g, {77.f, y - 6.f}, {77.f, y + 79.f}, hairline, 1.2f);
        module(g, {19.f, y + 6.f, 116.f, 57.f}, raised);
        label(g, buses[std::size_t(i)], {23.f, y + 8.f, 108.f, 21.f}, muted, 10.f,
              juce::Justification::centred, true);
        label(g, names[std::size_t(i)], {23.f, y + 29.f, 108.f, 24.f}, cyan, 10.5f);
        module(g, {x, 51.f, 357.f, 317.f}, i % 2 == 0 ? raised.darker(.09f) : panel);
        const int wet = mixes[std::size_t(i)];
        graphicalButton(g, {x + 9.f, 54.f, 24.f, 24.f}, "power", Kind::effectPower, wet, "Toggle effect",
                        processor.value(wet) > .001f ? cyan : dim);
        label(g, names[std::size_t(i)], {x + 38.f, 55.f, 178.f, 24.f}, ink, 12.f,
              juce::Justification::centredLeft, true);
        label(g,
              i == 0   ? "STEREO"
              : i == 1 ? "PING-PONG"
                       : "DIFFUSE + OCTAVE",
              {x + 209.f, 56.f, 135.f, 24.f}, dim, 9.5f, juce::Justification::centredRight, true);
        const Rect plot(x + 21.f, 100.f, 315.f, 140.f);
        graphGrid(g, plot, 12, 4);
        if (i == 0)
        {
            const float depth = processor.value(chorusDepth);
            trace(g, plot, cyan.withAlpha(.35f),
                  [depth](float u) { return std::sin((u * 1.38f - .30f) * twoPi) * depth; });
            trace(g, plot, cyan, [depth](float u) { return std::sin(u * 1.38f * twoPi) * depth; }, true);
            knob(g, {x + 70.f, 292.f}, chorusRate, "Rate", cyan);
            knob(g, {x + 178.f, 292.f}, chorusDepth, "Depth", cyan);
            knob(g, {x + 286.f, 292.f}, wet, "Dry / Wet", cyan);
        }
        else if (i == 1)
        {
            for (int j = 0; j < 11; ++j)
            {
                const float px =
                    plot.getX() +
                    plot.getWidth() * (.06f + float(j) * (.053f + processor.value(delayTime) * .075f));
                if (px >= plot.getRight())
                    break;
                const float h = plot.getHeight() * .74f *
                                std::pow(processor.value(delayFeedback) * .80f + .17f, float(j));
                line(g, {px, plot.getBottom() - 9.f}, {px, plot.getBottom() - 9.f - h},
                     cyan.withAlpha(.86f - .041f * float(j)), 2.f);
            }
            knob(g, {x + 70.f, 292.f}, delayTime, "Time", cyan);
            knob(g, {x + 178.f, 292.f}, delayFeedback, "Feedback", cyan);
            knob(g, {x + 286.f, 292.f}, wet, "Dry / Wet", cyan);
        }
        else
        {
            const float size = processor.value(roomSize), shine = processor.value(shimmer);
            trace(
                g, plot, cyan,
                [size, shine](float u)
                {
                    const float tail = std::exp(-u * (7.f - 4.3f * size));
                    return std::sin(u * 297.f) * std::cos(u * 97.f) * tail *
                               (.68f + .30f * std::sin(u * 45.f)) +
                           shine * tail * .21f * std::sin(u * 190.f);
                },
                true);
            knob(g, {x + 53.f, 292.f}, roomSize, "Size", cyan);
            knob(g, {x + 136.f, 292.f}, roomDamp, "Damping", cyan);
            knob(g, {x + 220.f, 292.f}, shimmer, "Shimmer", cyan);
            knob(g, {x + 303.f, 292.f}, wet, "Send Mix", cyan);
        }
        label(g,
              i == 0   ? "MODULATED DELAY"
              : i == 1 ? "CROSS-FEEDBACK"
                       : "OCTAVE FEEDBACK NETWORK",
              {x + 20.f, 350.f, 314.f, 11.f}, dim, 8.5f, juce::Justification::centred, true);
    }
}
void Editor::drawSequence(juce::Graphics &g)
{
    module(g, {4.f, 49.f, 1232.f, 321.f});
    module(g, {6.f, 51.f, 1228.f, 74.f}, raised.darker(.10f));
    const bool running = processor.sequencerOn();
    graphicalButton(g, {19.f, 62.f, 29.f, 28.f}, running ? "stop" : "play", Kind::seqStart, 0,
                    "Play / stop sequencer", running ? mint : cyan, true);
    label(g, "SEQUENCER", {56.f, 60.f, 117.f, 25.f}, ink, 12.f, juce::Justification::centredLeft, true);
    label(g, "INTERNAL CLOCK", {56.f, 87.f, 115.f, 15.f}, dim, 9.5f, juce::Justification::centredLeft, true);
    button(g, {185.f, 65.f, 105.f, 28.f}, "GENERATE", Kind::seqRandom, 0, false, mint);
    const std::array<int, 2> choices{seqScale, seqDivision};
    for (int i = 0; i < 2; ++i)
    {
        const float x = 311.f + float(i) * 129.f;
        const int id = choices[std::size_t(i)];
        label(g, i == 0 ? "SCALE" : "DIVISION", {x, 57.f, 110.f, 16.f}, dim, 9.f,
              juce::Justification::centred, true);
        button(g, {x, 75.f, 110.f, 28.f}, valueText(id) + "   v", Kind::choice, id);
    }
    knob(g, {607.f, 75.f}, seqTempo, "Tempo", cyan, 12.f);
    knob(g, {704.f, 75.f}, seqLength, "Length", cyan, 12.f);
    knob(g, {800.f, 75.f}, seqRoot, "Root", cyan, 12.f);
    knob(g, {898.f, 75.f}, seqSwing, "Swing", cyan, 12.f);
    knob(g, {996.f, 75.f}, seqGate, "Gate", cyan, 12.f);
    label(g, running ? "RUNNING" : "STOPPED", {1076.f, 63.f, 143.f, 26.f}, running ? mint : muted, 11.f,
          juce::Justification::centred, true);
    label(g, "SPACE / PLAY-STOP", {1076.f, 89.f, 143.f, 18.f}, dim, 9.f);
    const std::array<Rect, 4> rows{
        {{0.f, 151.f, 0.f, 29.f}, {0.f, 182.f, 0.f, 71.f}, {0.f, 255.f, 0.f, 46.f}, {0.f, 303.f, 0.f, 54.f}}};
    const std::array<juce::String, 4> labels{"GATE", "NOTE", "VELOCITY", "CHANCE"};
    const int length = 1 + pick(processor.value(seqLength), 16),
              root = 24 + pick(processor.value(seqRoot), 73);
    for (int j = 0; j < 4; ++j)
    {
        const auto row = rows[std::size_t(j)];
        module(g, {10.f, row.getY(), 89.f, row.getHeight()}, header);
        label(g, labels[std::size_t(j)], {14.f, row.getY(), 81.f, row.getHeight()}, muted, 10.f,
              juce::Justification::centred, true);
    }
    for (int i = 0; i < 16; ++i)
    {
        const float x = 103.f + 69.9f * float(i);
        const bool inside = i < length, current = i == processor.step.load() && running;
        const bool enabled = processor.value(stepParameter(i, stepOn)) > .5f && inside;
        const auto accent = current ? mint : cyan;
        label(g, juce::String(i + 1).paddedLeft('0', 2), {x, 128.f, 67.9f, 21.f},
              current  ? mint
              : inside ? ink
                       : dim,
              10.5f, juce::Justification::centred, i % 4 == 0);
        for (int j = 0; j < 4; ++j)
        {
            const auto r = rows[std::size_t(j)].withX(x).withWidth(67.3f);
            const int id = stepParameter(i, static_cast<StepPart>(j));
            module(g, r,
                   current  ? Colour(0xff284042)
                   : inside ? (enabled ? raised : header)
                            : base.brighter(.10f));
            if (current)
                fill(g, r.withHeight(1.f), mint.withAlpha(.75f));
            const auto c = accent.withAlpha(enabled ? .95f : .34f);
            if (j == 0)
            {
                g.setColour(enabled ? c : hairline);
                const Rect circle(r.getCentreX() - 4.5f, r.getCentreY() - 4.5f, 9.f, 9.f);
                if (enabled)
                    g.fillEllipse(circle);
                else
                    g.drawEllipse(circle, 1.f);
                addHit(Kind::toggle, id, r, "Toggle step " + juce::String(i + 1));
            }
            else
            {
                const float v = processor.value(id);
                if (j == 1)
                {
                    const float y = r.getBottom() - 13.f - v * 46.f;
                    line(g, {r.getX() + 9.f, y}, {r.getRight() - 9.f, y}, c);
                    module(g, {r.getCentreX() - 15.f, y - 8.f, 30.f, 16.f}, header);
                    label(g, noteName(root + pick(v, 49) - 24), {r.getCentreX() - 15.f, y - 8.f, 30.f, 16.f},
                          c, 10.f);
                }
                else
                {
                    const auto inset = r.reduced(8.f, 6.f);
                    const float y = inset.getBottom() - inset.getHeight() * v;
                    fill(g, inset.withTop(y), c.withAlpha(enabled ? .12f : .03f));
                    line(g, {inset.getX(), y}, {inset.getRight(), y}, c, 1.35f);
                    label(g, juce::String(int(std::round(v * 100.f))), r, enabled ? ink : dim, 10.f);
                }
                addHit(Kind::param, id, r,
                       "Step " + juce::String(i + 1) + " " + labels[std::size_t(j)].toLowerCase());
            }
        }
    }
}
void Editor::drawPlay(juce::Graphics &g)
{
    module(g, {4.f, 49.f, 1232.f, 321.f});
    module(g, {8.f, 53.f, 847.f, 313.f}, header);
    const int preset = processor.factoryIndex();
    label(g, preset >= 0 ? str(factory()[std::size_t(preset)].family) : "USER PATCH",
          {32.f, 71.f, 760.f, 23.f}, cyan, 11.f, juce::Justification::centredLeft, true);
    label(g, processor.patchName(), {29.f, 91.f, 775.f, 55.f}, ink, 37.f, juce::Justification::centredLeft);
    label(g,
          preset >= 0 ? str(factory()[std::size_t(preset)].description)
                      : "Play a note or start the sequencer.",
          {33.f, 147.f, 752.f, 25.f}, muted, 13.f, juce::Justification::centredLeft);
    const Rect plot(31.f, 187.f, 802.f, 130.f);
    graphGrid(g, plot, 20, 6);
    if (animate)
    {
        for (int i = 4; i >= 0; --i)
        {
            const float z = float(i);
            trace(
                g, plot.reduced(z * 4.f, z * 3.7f), cyan.withAlpha(.89f - z * .16f),
                [this, z](float x)
                {
                    const float raw =
                        processor.scope[std::size_t(x * float(processor.scope.size() - 1))].load();
                    return std::clamp(raw * (3.6f - z * .45f) + z * .05f * std::sin(twoPi * x * 4.f + time),
                                      -1.f, 1.f);
                },
                i == 0);
        }
    }
    else
        label(g, "VISUALIZER PAUSED", plot, dim, 11.f, juce::Justification::centred, true);
    label(g, "STEREO FIELD / OUTPUT SCOPE", {32.f, 325.f, 300.f, 20.f}, dim, 9.5f,
          juce::Justification::centredLeft, true);
    button(g, {704.f, 326.f, 129.f, 25.f}, animate ? "PAUSE VISUALS" : "RESUME VISUALS", Kind::visual);
    module(g, {859.f, 53.f, 373.f, 313.f}, raised);
    label(g, "PERFORMANCE MACROS", {866.f, 65.f, 359.f, 26.f}, ink, 12.f, juce::Justification::centred, true);
    for (int i = 0; i < 4; ++i)
        drawMacro(g, {953.f + float(i % 2) * 181.f, 140.f + float(i / 2) * 109.f}, i, 31.f);
    button(g, {882.f, 329.f, 103.f, 24.f}, "KEYBOARD", Kind::lower, int(Lower::keyboard),
           lower == Lower::keyboard);
    button(g, {993.f, 329.f, 103.f, 24.f}, "HOLD", Kind::hold, 0, holdNotes, mint);
    button(g, {1104.f, 329.f, 104.f, 24.f}, "PANIC", Kind::panic);
}
void Editor::drawModulation(juce::Graphics &g)
{
    const float width = 1232.f / float(int(Source::count));
    for (int i = 0; i < int(Source::count); ++i)
    {
        const float x = 4.f + float(i) * width;
        const Rect tile(x, 374.f, width - 2.f, 99.f);
        const auto c = sourceColour(i);
        const bool selected = i == selectedSource,
                   armed = i == armedSource || (pressed.kind == Kind::source && pressed.index == i);
        module(g, tile, armed ? panel.interpolatedWith(c, .13f) : selected ? raised : panel);
        if (selected)
            fill(g, tile.withHeight(1.7f), c.withAlpha(.85f));
        label(g, str(sourceNames[std::size_t(i)]), {x + 5.f, 379.f, width - 10.f, 20.f}, c, 10.5f,
              juce::Justification::centred, true);
        const Rect plot(x + 8.f, 408.f, width - 17.f, 45.f);
        const float value = processor.sources[std::size_t(i)].load();
        if (i < int(Source::lfo1) || i >= int(Source::macro1))
        {
            const float v =
                i >= int(Source::macro1) ? processor.value(macro1 + i - int(Source::macro1)) : value;
            const float y = plot.getBottom() - std::max(.05f, v) * plot.getHeight();
            g.setGradientFill(juce::ColourGradient(c.withAlpha(.32f), x, y, c.withAlpha(.015f), x,
                                                   plot.getBottom(), false));
            g.fillRect(plot.withTop(y));
            line(g, {plot.getX(), y}, {plot.getRight(), y}, c.withAlpha(.8f));
        }
        else
        {
            trace(g, plot, c.withAlpha(.75f),
                  [this, i, value](float x)
                  {
                      if (i <= int(Source::lfo3))
                          return lfoWave(pick(processor.value(lfo1Shape + (i - int(Source::lfo1)) * 2), 4),
                                         x + time * .18f) *
                                 .65f;
                      const std::array<float, 4> stairs{-.58f, .16f, -.13f, .57f};
                      return std::clamp(stairs[std::size_t(std::min(3, int(x * 4.f)))] + value * .27f, -.88f,
                                        .88f);
                  });
        }
        int count = 0;
        for (int j = 0; j < maxRoutes; ++j)
            if (processor.route(j).source == i)
                ++count;
        if (count > 0)
            label(g, juce::String(count), {tile.getRight() - 15.f, 456.f, 12.f, 12.f}, dim, 9.f);
        addHit(Kind::source, i, tile, "Drag " + str(sourceNames[std::size_t(i)]) + " to a parameter");
    }
}
void Editor::drawEnvelope(juce::Graphics &g, Rect area, int env)
{
    const int param = attack + env * 4;
    label(g, env == 0 ? "ENV AMP" : "ENV " + juce::String(env + 1),
          {area.getX() + 11.f, area.getY() + 4.f, 155.f, 23.f}, orange, 11.f,
          juce::Justification::centredLeft, true);
    const Rect plot(area.getX() + 11.f, area.getY() + 39.f, area.getWidth() - 22.f, 79.f);
    graphGrid(g, plot, 8, 4);
    const float a = .05f + processor.value(param) * .19f, d = .06f + processor.value(param + 1) * .21f;
    const float r = .06f + processor.value(param + 3) * .28f, s = processor.value(param + 2), end = 1.f - r;
    trace(
        g, plot, orange,
        [a, d, r, s, end](float x)
        {
            float level = 0.f;
            if (x < a)
            {
                const float p = x / a;
                level = p * p * (3.f - 2.f * p);
            }
            else if (x < a + d)
            {
                const float p = (x - a) / d;
                level = 1.f + (s - 1.f) * p * (2.f - p);
            }
            else if (x < end)
                level = s;
            else
            {
                const float p = (1.f - x) / r;
                level = s * p * p;
            }
            return 2.f * level - 1.f;
        },
        true);
    for (int i = 0; i < 4; ++i)
        knob(g, {area.getX() + (float(i) + .5f) * area.getWidth() / 4.f, area.getY() + 150.f}, param + i,
             str(envelopeLabels[std::size_t(i)]), orange, 15.f);
}
void Editor::drawLfo(juce::Graphics &g, Rect area, int lfo)
{
    const int param = lfo1Rate + lfo * 2, wave = pick(processor.value(param + 1), 4);
    label(g, "LFO " + juce::String(lfo + 1), {area.getX() + 11.f, area.getY() + 4.f, 104.f, 23.f}, gold, 11.f,
          juce::Justification::centredLeft, true);
    const Rect plot(area.getX() + 11.f, area.getY() + 39.f, area.getWidth() - 22.f, 79.f);
    graphGrid(g, plot, 8, 4);
    trace(g, plot, gold, [wave](float x) { return lfoWave(wave, x * 1.9999f) * .83f; }, true);
    knob(g, {area.getX() + 70.f, area.getY() + 151.f}, param, "Rate", gold, 15.f);
    button(g, {area.getX() + 123.f, area.getY() + 139.f, area.getWidth() - 144.f, 27.f},
           str(waveNames[std::size_t(wave)]) + "   v", Kind::choice, param + 1, false, gold);
}
void Editor::drawRoutes(juce::Graphics &g, Rect r)
{
    const float x = r.getX(), y = r.getY();
    label(g, "MODULATION ROUTES", {x + 13.f, y + 3.f, 188.f, 21.f}, ink, 11.f,
          juce::Justification::centredLeft, true);
    label(g, "DRAG A SOURCE ONTO A KNOB TO PATCH IT", {x + 225.f, y + 3.f, 384.f, 21.f}, dim, 9.5f,
          juce::Justification::centredLeft, true);
    for (int i = 0; i < 4; ++i)
    {
        const int index = i + routeOffset;
        const auto route = processor.route(index);
        const float top = y + 32.f + 38.f * float(i);
        const Rect row(x + 12.f, top, r.getWidth() - 205.f, 32.f);
        const bool selected = selectedRoute == index && route.source >= 0;
        module(g, row, selected ? Colour(0xff34363b) : header);
        label(g, juce::String(index + 1).paddedLeft('0', 2), {x + 20.f, top, 33.f, 32.f}, dim, 10.f);
        if (!route.valid())
        {
            label(g, "Empty", {x + 61.f, top, 175.f, 32.f}, dim, 11.f, juce::Justification::centredLeft);
            continue;
        }
        const auto c = sourceColour(route.source);
        fill(g, row.withWidth(2.f), c.withAlpha(selected ? .9f : .44f));
        label(g, str(sourceNames[std::size_t(route.source)]), {x + 60.f, top, 99.f, 32.f}, c, 10.5f,
              juce::Justification::centredLeft, true);
        label(g, ">", {x + 165.f, top, 43.f, 32.f}, dim, 13.f);
        label(g, targetName(route.target), {x + 224.f, top, 304.f, 32.f}, ink, 12.f,
              juce::Justification::centredLeft);
        const int percent = int(std::round(route.amount * 100.f));
        label(g, (percent > 0 ? "+" : "") + juce::String(percent) + "%",
              {row.getRight() - 119.f, top, 73.f, 32.f}, c, 11.f);
        addHit(Kind::routeSelect, index, row, "Select modulation route");
        graphicalButton(g, {row.getRight() - 37.f, top + 3.f, 30.f, 26.f}, "close", Kind::routeClear, index,
                        "Delete modulation route", muted);
    }
    const Rect detail(r.getRight() - 179.f, y + 32.f, 165.f, 147.f);
    module(g, detail, raised);
    const auto route = processor.route(selectedRoute);
    if (route.valid())
    {
        const auto c = sourceColour(route.source);
        label(g, "ROUTE " + juce::String(selectedRoute + 1).paddedLeft('0', 2), detail.withHeight(25.f), c,
              10.5f, juce::Justification::centred, true);
        const Point p{detail.getCentreX(), detail.getY() + 65.f};
        dial(g, p, .5f + route.amount * .5f, c, 25.f, hover.kind == Kind::routeDepth);
        addHit(Kind::routeDepth, selectedRoute, {p.x - 32.f, p.y - 32.f, 64.f, 90.f},
               "Drag to change modulation depth");
        label(g, "Depth", {detail.getX(), detail.getY() + 96.f, detail.getWidth(), 20.f}, ink, 11.f);
        label(g, juce::String(int(std::round(route.amount * 100.f))) + "%",
              {detail.getX(), detail.getY() + 117.f, detail.getWidth(), 17.f}, c, 10.5f);
    }
    else
        label(g, "Select a route to set depth", detail.reduced(5.f), dim, 10.5f);
    graphicalButton(g, {x + 13.f, y + 184.f, 28.f, 20.f}, "left", Kind::routeScroll, -1, "Previous routes",
                    muted);
    label(g, juce::String(1 + routeOffset / 4) + " / 4", {x + 41.f, y + 184.f, 55.f, 20.f}, dim, 10.5f);
    graphicalButton(g, {x + 98.f, y + 184.f, 28.f, 20.f}, "right", Kind::routeScroll, 1, "Next routes",
                    muted);
}
void Editor::drawMacro(juce::Graphics &g, Point p, int index, float radius)
{
    const int param = macro1 + index;
    const auto name = str(macros[std::size_t(index)]);
    const bool active = hover.same({Kind::param, param, {}, {}});
    dial(g, p, processor.value(param), cyan, radius, active);
    const Rect r(p.x - radius - 12.f, p.y - radius - 4.f, radius * 2.f + 24.f, radius * 2.f + 34.f);
    label(g, name, {r.getX(), p.y + radius + 4.f, r.getWidth(), 15.f}, active ? ink : muted, 11.f);
    label(g, valueText(param), {r.getX(), p.y + radius + 19.f, r.getWidth(), 12.f}, dim, 9.5f);
    addHit(Kind::param, param, r, name + " macro");
}
void Editor::drawKeyboard(juce::Graphics &g, Rect area)
{
    button(g, {area.getX() + 17.f, area.getY() + 8.f, 35.f, 23.f}, "-", Kind::octave, -1);
    label(g, "C" + juce::String(octave), {area.getX() + 55.f, area.getY() + 8.f, 42.f, 23.f}, ink, 11.f);
    button(g, {area.getX() + 100.f, area.getY() + 8.f, 35.f, 23.f}, "+", Kind::octave, 1);
    label(g, "A-L TO PLAY / W E T Y U FOR SHARPS / Z X TO CHANGE OCTAVE",
          {area.getX() + 152.f, area.getY() + 8.f, area.getWidth() - 304.f, 23.f}, muted, 10.f,
          juce::Justification::centredLeft, true);
    button(g, {area.getRight() - 142.f, area.getY() + 8.f, 60.f, 23.f}, "HOLD", Kind::hold, 0, holdNotes,
           mint);
    button(g, {area.getRight() - 75.f, area.getY() + 8.f, 62.f, 23.f}, "PANIC", Kind::panic);
    const Rect piano(area.getX() + 16.f, area.getY() + 44.f, area.getWidth() - 32.f, area.getHeight() - 57.f);
    const float width = piano.getWidth() / 15.f;
    const int first = (octave + 1) * 12;
    int white = 0;
    for (int note = first; note <= first + 24; ++note)
    {
        if (blackKey(note))
            continue;
        const Rect key(piano.getX() + float(white++) * width, piano.getY(), width - 1.f, piano.getHeight());
        const auto c =
            processor.keyboard.isNoteOnForChannels(0xffff, note) ? cyan.withAlpha(.87f) : Colour(0xffc5c7c8);
        g.setGradientFill(juce::ColourGradient(c.darker(.29f), key.getX(), key.getY(), c, key.getX(),
                                               key.getBottom(), false));
        g.fillRoundedRectangle(key, 2.4f);
        if (note % 12 == 0)
            label(g, noteName(note), key.withTop(key.getBottom() - 21.f), base.withAlpha(.76f), 10.5f);
        addHit(Kind::key, note, key, noteName(note));
    }
    white = 0;
    for (int note = first; note <= first + 24; ++note)
    {
        if (!blackKey(note))
        {
            ++white;
            continue;
        }
        const Rect key(piano.getX() + (float(white) - .31f) * width, piano.getY(), width * .61f,
                       piano.getHeight() * .60f);
        const bool active = processor.keyboard.isNoteOnForChannels(0xffff, note);
        g.setGradientFill(juce::ColourGradient(active ? cyan.darker(.62f) : base, key.getX(), key.getY(),
                                               active ? cyan : raised.darker(.19f), key.getX(),
                                               key.getBottom(), false));
        g.fillRoundedRectangle(key, 2.4f);
        g.setColour(base.withAlpha(.89f));
        g.drawRoundedRectangle(key, 2.4f, 1.f);
        addHit(Kind::key, note, key, noteName(note));
    }
}
void Editor::drawLower(juce::Graphics &g)
{
    module(g, {4.f, 477.f, 1025.f, 249.f});
    const std::array<Colour, 5> accents{pink, orange, gold, violet, mint};
    for (int i = 0; i < 5; ++i)
    {
        const Rect tab(5.f + float(i) * 205.f, 478.f, 203.f, 25.f);
        button(g, tab, str(lowerNames[std::size_t(i)]), Kind::lower, i, int(lower) == i,
               accents[std::size_t(i)]);
        fill(g, tab.withHeight(1.25f), accents[std::size_t(i)].withAlpha(int(lower) == i ? .9f : .47f));
    }
    const Rect work(7.f, 508.f, 1019.f, 213.f);
    if (lower == Lower::envelopes || lower == Lower::lfos)
    {
        const float width = work.getWidth() / 3.f;
        for (int i = 0; i < 3; ++i)
        {
            const Rect box(work.getX() + float(i) * width + 4.f, work.getY() + 3.f, width - 8.f,
                           work.getHeight() - 6.f);
            if (i != 0)
                line(g, {box.getX() - 4.f, box.getY() + 13.f}, {box.getX() - 4.f, box.getBottom() - 13.f},
                     hairline.withAlpha(.69f));
            if (lower == Lower::envelopes)
                drawEnvelope(g, box, i);
            else
                drawLfo(g, box, i);
        }
    }
    else if (lower == Lower::keyboard)
        drawKeyboard(g, work);
    else if (lower == Lower::routes)
        drawRoutes(g, work);
    else
    {
        label(g, "RANDOM / SAMPLE & HOLD", {23.f, 519.f, 461.f, 19.f}, violet, 11.5f,
              juce::Justification::centredLeft, true);
        const Rect plot(23.f, 549.f, 615.f, 116.f);
        graphGrid(g, plot, 11, 4);
        const float value = processor.sources[std::size_t(Source::random)].load();
        trace(
            g, plot, violet,
            [value](float x)
            {
                const std::array<float, 7> v{-.45f, .23f, -.66f, .44f, .70f, -.36f, value};
                return v[std::size_t(std::min(6, int(x * 7.f)))];
            },
            true);
        label(g, "A BIPOLAR VALUE AT EACH CLOCK TICK", {23.f, 675.f, 400.f, 21.f}, dim, 10.f,
              juce::Justification::centredLeft, true);
        knob(g, {735.f, 593.f}, randomRate, "Rate", violet, 19.f);
        label(g, "CURRENT VALUE", {814.f, 555.f, 173.f, 23.f}, muted, 10.f, juce::Justification::centred,
              true);
        label(g, juce::String(value, 2), {814.f, 581.f, 173.f, 33.f}, violet, 22.f);
    }
    module(g, {1033.f, 477.f, 203.f, 249.f}, raised.darker(.11f));
    module(g, {1034.f, 478.f, 201.f, 25.f}, header);
    fill(g, {1034.f, 478.f, 201.f, 1.3f}, cyan);
    label(g, "MACROS", {1036.f, 479.f, 197.f, 23.f}, ink, 11.f, juce::Justification::centred, true);
    for (int i = 0; i < 4; ++i)
        drawMacro(g, {1085.f + float(i % 2) * 100.f, 546.f + float(i / 2) * 100.f}, i, 22.f);
}
void Editor::drawFooter(juce::Graphics &g)
{
    module(g, {4.f, 730.f, 1232.f, 24.f}, header);
    juce::String hint;
    if (statusUntil > juce::Time::getMillisecondCounterHiRes())
        hint = status;
    else if (armedSource >= 0)
        hint = str(sourceNames[std::size_t(armedSource)]) + " armed / Click an outlined knob to route it";
    else if (hover.kind != Kind::none)
        hint = hover.kind == Kind::param ? hover.name + "  " + valueText(hover.index) + "  / Drag to adjust"
                                         : hover.name;
    else
        hint = "A-L to play / Z-X octave / Drag a modulation source onto a knob";
    label(g, hint, {17.f, 731.f, 716.f, 22.f}, muted, 10.5f, juce::Justification::centredLeft);
    button(g, {751.f, 732.f, 63.f, 19.f}, "HOLD", Kind::hold, 0, holdNotes, mint);
    button(g, {819.f, 732.f, 60.f, 19.f}, "PANIC", Kind::panic);
    label(g,
          juce::String(processor.playingVoices.load()) + " / " +
              juce::String(voiceLimit(processor.value(polyphony))) + " voices",
          {886.f, 731.f, 91.f, 22.f}, muted, 10.5f);
    button(g, {980.f, 732.f, 77.f, 19.f}, valueText(polyphony) + " v", Kind::choice, polyphony);
    label(g, "DSP " + juce::String(int(std::round(processor.cpuLoad.load() * 100.f))) + "%",
          {1066.f, 731.f, 86.f, 22.f}, dim, 10.5f);
    label(g, "0.1.0", {1167.f, 731.f, 52.f, 22.f}, dim, 10.f);
}
} // namespace mica
