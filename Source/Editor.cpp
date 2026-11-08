// SPDX-License-Identifier: AGPL-3.0-only
#include "Editor.h"
#include <algorithm>
#include <cmath>

namespace mica
{
namespace
{
constexpr std::array<int, 18> keyCodes{'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y',
                                       'H', 'U', 'J', 'K', 'O', 'L', 'P', ';', 39};
constexpr float pi = 3.14159265358979323846f, arcStart = 1.20f * pi, arcEnd = 2.80f * pi;
enum FileOperation
{
    wavOperation,
    patchOpen,
    patchSave,
    sampleOpen
};
class Skin final : public juce::LookAndFeel_V4
{
  public:
    Skin()
    {
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff24262a));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe7e9eb));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff164c61));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }
};
Skin &skin()
{
    static Skin appearance;
    return appearance;
}
juce::String signedValue(int value)
{
    return (value > 0 ? "+" : "") + juce::String(value);
}
} // namespace
const Editor::Colour Editor::base(0xff0f1011), Editor::panel(0xff27282a), Editor::raised(0xff2e2f32),
    Editor::header(0xff1d1e21);
const Editor::Colour Editor::hairline(0xff3d3f42), Editor::ink(0xffe4e5e7), Editor::muted(0xff9b9da3),
    Editor::dim(0xff797c83);
const Editor::Colour Editor::cyan(0xff38c7f3), Editor::orange(0xffef8857), Editor::gold(0xffdabb60);
const Editor::Colour Editor::violet(0xffa686ed), Editor::pink(0xffec6fb6), Editor::mint(0xff78d2a1);
Editor::Editor(Instrument &instrument) : AudioProcessorEditor(&instrument), processor(instrument)
{
    setLookAndFeel(&skin());
    hits.reserve(180);
    computerNotes.fill(-1);
    for (int i = 0; i < paramCount; ++i)
        bypassRecall[std::size_t(i)] = parameters()[std::size_t(i)].initial;
    setOpaque(true);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(820, 502, 2480, 1516);
    if (auto *c = getConstrainer())
        c->setFixedAspectRatio(double(canvasWidth) / canvasHeight);
    setSize(canvasWidth, canvasHeight);
    startTimerHz(24);
}
Editor::~Editor()
{
    stopTimer();
    if (inGesture)
        processor.endGesture(pressed.index);
    if (pointerNote >= 0)
        stopNote(pointerNote, true);
    releaseComputerNotes();
    releaseLatched();
    setLookAndFeel(nullptr);
}
Editor::Colour Editor::sourceColour(int source)
{
    if (source < int(Source::amp))
        return pink;
    if (source < int(Source::lfo1))
        return orange;
    if (source < int(Source::random))
        return gold;
    if (source == int(Source::random))
        return violet;
    if (source == int(Source::step))
        return mint;
    return cyan;
}
juce::String Editor::str(std::string_view s)
{
    return juce::String::fromUTF8(s.data(), int(s.size()));
}
juce::String Editor::noteName(int note)
{
    static constexpr std::array<std::string_view, 12> names{"C",  "C#", "D",  "D#", "E",  "F",
                                                            "F#", "G",  "G#", "A",  "A#", "B"};
    note = std::clamp(note, 0, 127);
    return str(names[std::size_t(note % 12)]) + juce::String(note / 12 - 1);
}
void Editor::label(juce::Graphics &g, const juce::String &text, Rect r, Colour c, float height,
                   juce::Justification align, bool bold)
{
    g.setColour(c);
    g.setFont(juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain)));
    g.drawFittedText(text, r.toNearestInt(), align, 1, .75f);
}
void Editor::fill(juce::Graphics &g, Rect r, Colour c)
{
    g.setColour(c);
    g.fillRect(r);
}
void Editor::line(juce::Graphics &g, Point a, Point b, Colour c, float width)
{
    g.setColour(c);
    g.drawLine({a, b}, width);
}
void Editor::module(juce::Graphics &g, Rect r, Colour c)
{
    g.setColour(c);
    g.fillRoundedRectangle(r, 3.f);
    g.setColour(juce::Colours::white.withAlpha(.045f));
    g.drawRoundedRectangle(r.reduced(.5f), 3.f, 1.f);
}
void Editor::graphGrid(juce::Graphics &g, Rect r, int columns, int rows)
{
    for (int i = 1; i < columns; ++i)
    {
        const float x = r.getX() + r.getWidth() * float(i) / float(columns);
        line(g, {x, r.getY()}, {x, r.getBottom()}, hairline.withAlpha(.44f));
    }
    for (int i = 0; i <= rows; ++i)
    {
        const float y = r.getY() + r.getHeight() * float(i) / float(rows);
        line(g, {r.getX(), y}, {r.getRight(), y}, hairline.withAlpha(i * 2 == rows ? .85f : .44f));
    }
}
void Editor::trace(juce::Graphics &g, Rect r, Colour c, const std::function<float(float)> &function,
                   bool under)
{
    juce::Path path;
    for (int i = 0; i < 99; ++i)
    {
        const float p = float(i) / 98.f, x = r.getX() + r.getWidth() * p;
        const float y = r.getCentreY() - r.getHeight() * .47f * std::clamp(function(p), -1.f, 1.f);
        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }
    if (under)
    {
        auto area = path;
        area.lineTo(r.getRight(), r.getBottom());
        area.lineTo(r.getX(), r.getBottom());
        area.closeSubPath();
        g.setGradientFill(juce::ColourGradient(c.withAlpha(.27f), r.getCentreX(), r.getY(),
                                               c.withAlpha(.006f), r.getCentreX(), r.getBottom(), false));
        g.fillPath(area);
    }
    g.setColour(c);
    g.strokePath(path, juce::PathStrokeType(1.25f, juce::PathStrokeType::curved));
}
void Editor::icon(juce::Graphics &g, Rect r, const juce::String &id, Colour c)
{
    const float x = r.getCentreX(), y = r.getCentreY();
    g.setColour(c);
    if (id == "menu")
        for (int i = -1; i <= 1; ++i)
            g.drawHorizontalLine(int(y) + i * 4, x - 7, x + 7);
    else if (id == "left" || id == "right" || id == "down")
    {
        juce::Path p;
        if (id == "down")
        {
            p.startNewSubPath(x - 3.5f, y - 1.5f);
            p.lineTo(x, y + 2.f);
            p.lineTo(x + 3.5f, y - 1.5f);
        }
        else
        {
            const float sign = id == "left" ? -1.f : 1.f;
            p.startNewSubPath(x - 2.5f * sign, y - 4.f);
            p.lineTo(x + 2.f * sign, y);
            p.lineTo(x - 2.5f * sign, y + 4.f);
        }
        g.strokePath(p, juce::PathStrokeType(1.3f));
    }
    else if (id == "power")
    {
        juce::Path arc;
        arc.addCentredArc(x, y, 4.1f, 4.1f, 0.f, .58f, 5.70f, true);
        g.strokePath(arc, juce::PathStrokeType(1.1f));
        g.drawVerticalLine(int(x), y - 5.5f, y - .4f);
    }
    else if (id == "settings")
    {
        g.drawEllipse(x - 4.5f, y - 4.5f, 9.f, 9.f, 1.2f);
        g.drawEllipse(x - 1.4f, y - 1.4f, 2.8f, 2.8f, 1.f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = float(i) * pi * .25f;
            g.drawLine(x + std::sin(a) * 4.7f, y + std::cos(a) * 4.7f, x + std::sin(a) * 6.5f,
                       y + std::cos(a) * 6.5f, 1.3f);
        }
    }
    else if (id == "book")
    {
        g.drawLine(x - 5, y - 5, x - 5, y + 5, 1.3f);
        g.drawLine(x - 2, y - 5, x - 2, y + 5, 1.3f);
        g.drawLine(x + 1, y - 5, x + 5, y + 5, 1.3f);
    }
    else if (id == "record")
        g.fillEllipse(x - 4, y - 4, 8, 8);
    else if (id == "stop")
        g.fillRoundedRectangle(x - 4, y - 4, 8, 8, 1.f);
    else if (id == "play")
    {
        juce::Path p;
        p.addTriangle(x - 3, y - 5, x + 5, y, x - 3, y + 5);
        g.fillPath(p);
    }
    else if (id == "close")
    {
        g.drawLine(x - 3, y - 3, x + 3, y + 3, 1.2f);
        g.drawLine(x + 3, y - 3, x - 3, y + 3, 1.2f);
    }
}
float Editor::scale() const
{
    return std::min(float(getWidth()) / canvasWidth, float(getHeight()) / canvasHeight);
}
Editor::Point Editor::origin() const
{
    return {(float(getWidth()) - canvasWidth * scale()) * .5f,
            (float(getHeight()) - canvasHeight * scale()) * .5f};
}
Editor::Point Editor::virtualPoint(Point p) const
{
    return (p - origin()) / scale();
}
void Editor::paint(juce::Graphics &g)
{
    g.fillAll(base);
    hits.clear();
    g.addTransform(juce::AffineTransform::scale(scale()).translated(origin().x, origin().y));
    drawTop(g);
    switch (page)
    {
    case Page::play:
        drawPlay(g);
        break;
    case Page::synth:
        drawSynth(g);
        break;
    case Page::effects:
        drawEffects(g);
        break;
    case Page::sequence:
        drawSequence(g);
        break;
    }
    drawModulation(g);
    drawLower(g);
    drawFooter(g);
    if (dragging && pressed.kind == Kind::source)
    {
        const Rect tag(mouseNow.x + 12.f, mouseNow.y - 19.f, 61.f, 21.f);
        module(g, tag, header);
        label(g, str(sourceNames[std::size_t(pressed.index)]), tag, sourceColour(pressed.index), 10.f,
              juce::Justification::centred, true);
    }
    redraw = false;
}
void Editor::addHit(Kind kind, int index, Rect area, juce::String text)
{
    hits.push_back({kind, index, area, std::move(text)});
}
Editor::Hit Editor::findHit(Point p) const
{
    for (auto i = hits.rbegin(); i != hits.rend(); ++i)
        if (i->area.contains(p))
            return *i;
    return {};
}
void Editor::dial(juce::Graphics &g, Point center, float value, Colour c, float radius, bool selected,
                  float modulation, Colour modColour)
{
    const float x = center.x, y = center.y;
    const Rect face(x - radius + 3.2f, y - radius + 3.2f, 2.f * radius - 6.4f, 2.f * radius - 6.4f);
    juce::Path track;
    track.addCentredArc(x, y, radius, radius, 0.f, arcStart, arcEnd, true);
    g.setColour(selected ? hairline.brighter(.26f) : hairline);
    g.strokePath(track,
                 juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float angle = arcStart + (arcEnd - arcStart) * std::clamp(value, 0.f, 1.f);
    if (value > .001f)
    {
        juce::Path active;
        active.addCentredArc(x, y, radius, radius, 0.f, arcStart, angle, true);
        g.setColour(c.withAlpha(selected ? 1.f : .90f));
        g.strokePath(
            active, juce::PathStrokeType(1.65f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float limit = arcStart + (arcEnd - arcStart) * std::clamp(value + modulation, 0.f, 1.f);
    if (std::abs(modulation) > .002f && std::abs(limit - angle) > .005f)
    {
        juce::Path arc;
        arc.addCentredArc(x, y, radius + 3.f, radius + 3.f, 0.f, std::min(angle, limit),
                          std::max(angle, limit), true);
        g.setColour(modColour);
        g.strokePath(arc, juce::PathStrokeType(1.7f));
    }
    g.setGradientFill(juce::ColourGradient(Colour(selected ? 0xffb8b9ba : 0xff686a6d), x, face.getY(),
                                           Colour(selected ? 0xff85888c : 0xff484a4d), x, face.getBottom(),
                                           false));
    g.fillEllipse(face);
    g.setColour(juce::Colours::black.withAlpha(.48f));
    g.drawEllipse(face, 1.f);
    line(g, {x + std::sin(angle) * radius * .44f, y - std::cos(angle) * radius * .44f},
         {x + std::sin(angle) * (radius - 5.8f), y - std::cos(angle) * (radius - 5.8f)},
         selected ? base : ink.withAlpha(.72f), 1.45f);
}
void Editor::knob(juce::Graphics &g, Point point, int param, const juce::String &name, Colour c, float radius)
{
    const Hit hit{Kind::param, param, {}, {}};
    const bool active = hover.same(hit) || pressed.same(hit);
    float mod = 0.f;
    const auto &info = parameters()[std::size_t(param)];
    if (info.modTarget)
        for (int i = 0; i < maxRoutes; ++i)
        {
            const auto r = processor.route(i);
            if (r.source == selectedSource && r.target == param)
            {
                mod = r.amount;
                break;
            }
        }
    const int candidate = dragging && pressed.kind == Kind::source ? pressed.index : armedSource;
    const bool armed = candidate >= 0 && info.modTarget;
    dial(g, point, processor.value(param), armed ? sourceColour(candidate) : c, radius, active || armed, mod,
         sourceColour(selectedSource));
    const float width = std::max(59.f, radius * 2.f + 30.f);
    label(g, name, {point.x - width * .5f, point.y + radius + 3.f, width, 14.f}, active ? ink : muted, 11.f);
    label(g, valueText(param), {point.x - width * .5f, point.y + radius + 17.f, width, 12.f},
          active ? c : dim, 9.5f);
    addHit(Kind::param, param,
           {point.x - radius - 9.f, point.y - radius - 4.f, radius * 2.f + 18.f, radius * 2.f + 35.f}, name);
}
void Editor::button(juce::Graphics &g, Rect r, const juce::String &text, Kind kind, int index, bool active,
                    Colour c)
{
    const bool over = hover.same({kind, index, {}, {}});
    const auto bg = active ? header.interpolatedWith(c, .12f) : header;
    module(g, r, over ? bg.brighter(.21f) : bg);
    label(g, text, r.reduced(4.f, 1.f),
          active ? c
          : over ? ink
                 : muted,
          11.f, juce::Justification::centred, active);
    addHit(kind, index, r, text);
}
void Editor::graphicalButton(juce::Graphics &g, Rect r, const juce::String &symbol, Kind kind, int index,
                             const juce::String &text, Colour c, bool active)
{
    if (active || hover.same({kind, index, {}, {}}))
        module(g, r.reduced(2.f), raised);
    icon(g, r, symbol, c);
    addHit(kind, index, r, text);
}
juce::String Editor::choiceText(int p, int v) const
{
    if (p == aType || p == bType)
        return str(engineNames[std::size_t(v)]);
    if (p == f1Type || p == f2Type)
        return str(filterNames[std::size_t(v)]);
    if (p == lfo1Shape || p == lfo2Shape || p == lfo3Shape)
        return str(waveNames[std::size_t(v)]);
    if (p == seqDivision)
        return v == 0 ? "1/4" : v == 1 ? "1/8" : "1/16";
    if (p == seqScale)
        return str(scaleNames[std::size_t(v)]);
    if (p == polyphony)
        return "Poly " + juce::String(4 + 4 * v);
    return juce::String(v + 1);
}
juce::String Editor::valueText(int p) const
{
    const float v = processor.value(p);
    if (p == f1Cut || p == f2Cut)
    {
        const float hz = filterFrequency(v);
        return hz < 1000.f ? juce::String(int(std::round(hz))) + " Hz"
                           : juce::String(hz / 1000.f, 2) + " kHz";
    }
    if (p >= attack && p <= env3Release && (p - attack) % 4 != 2)
    {
        const float t = seconds(v);
        return t < .0995f ? juce::String(int(std::round(t * 1000.f))) + " ms" : juce::String(t, 2) + " s";
    }
    if (p == lfo1Rate || p == lfo2Rate || p == lfo3Rate || p == randomRate)
        return juce::String(lfoFrequency(v), 2) + " Hz";
    if (p == delayTime)
        return juce::String(int(std::round(delaySeconds(v) * 1000.f))) + " ms";
    if (p == chorusRate)
        return juce::String(.032f + v * v * 3.9f, 2) + " Hz";
    if (p == seqTempo)
        return juce::String(int(std::round(40.f + 160.f * v))) + " BPM";
    if (p == seqRoot)
        return noteName(24 + pick(v, 73));
    if (p == seqLength)
        return juce::String(1 + pick(v, 16)) + " steps";
    if (p == filterRoute)
        return v < .015f ? "Series" : v > .985f ? "Parallel" : juce::String(int(v * 100.f)) + "% par.";
    if (p == voicePan)
        return signedValue(int(std::round(200.f * v - 100.f)));
    if (p < subLevel)
    {
        const int id = p % bType, type = selectedEngine(p / bType);
        if (id == aTune)
            return signedValue(pick(v, 49) - 24) + " st";
        if (id == aFine)
            return signedValue(int(std::round(v * 100.f - 50.f))) + " ct";
        if (type == int(EngineType::sample) && id == aColor)
            return v >= .5f ? "Loop" : "One-shot";
        if (type == int(EngineType::granular) && id == aColor)
            return juce::String(int(24.f + v * v * 290.f)) + " ms";
        if (type == int(EngineType::harmonic) && id == aShape)
            return juce::String(2 + int(v * 10.999f));
    }
    if (p >= stepBase && (p - stepBase) % 4 == stepPitch)
        return signedValue(pick(v, 49) - 24) + " st";
    const auto &info = parameters()[std::size_t(p)];
    return info.choices > 1 ? choiceText(p, pick(v, info.choices))
                            : juce::String(int(std::round(v * 100.f))) + "%";
}
juce::String Editor::targetName(int param) const
{
    const int id = param % bType;
    if (param < subLevel && id >= aShape && id <= aMotion)
        return "E" + juce::String(param / bType + 1) + " " +
               str(engineControls[std::size_t(selectedEngine(param / bType))][std::size_t(id - aShape)]);
    return juce::String(parameters()[std::size_t(param)].name);
}
void Editor::mouseMove(const juce::MouseEvent &e)
{
    mouseNow = virtualPoint(e.position);
    const auto next = findHit(mouseNow);
    if (!next.same(hover))
    {
        hover = next;
        setMouseCursor(hover.kind == Kind::none ? juce::MouseCursor::NormalCursor
                       : hover.kind == Kind::param || hover.kind == Kind::routeDepth
                           ? juce::MouseCursor::UpDownResizeCursor
                           : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}
void Editor::mouseExit(const juce::MouseEvent &)
{
    hover = {};
    repaint();
}
void Editor::mouseDown(const juce::MouseEvent &e)
{
    grabKeyboardFocus();
    mouseStart = mouseNow = virtualPoint(e.position);
    pressed = findHit(mouseStart);
    dragging = false;
    if (e.mods.isPopupMenu())
    {
        if (pressed.kind == Kind::param)
        {
            juce::PopupMenu menu;
            menu.setLookAndFeel(&skin());
            menu.addItem(1, "Reset value");
            menu.addItem(2, "Assign selected source", parameters()[std::size_t(pressed.index)].modTarget);
            juce::Component::SafePointer<Editor> safe(this);
            const int param = pressed.index;
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                               [safe, param](int id)
                               {
                                   if (safe == nullptr)
                                       return;
                                   if (id == 1)
                                       safe->processor.change(param,
                                                              parameters()[std::size_t(param)].initial);
                                   if (id == 2)
                                       safe->assignModulation(safe->selectedSource, param);
                                   safe->repaint();
                               });
        }
        pressed = {};
        return;
    }
    if (pressed.kind == Kind::key)
    {
        pointerNote = pressed.index;
        playNote(pointerNote);
    }
    if (pressed.kind == Kind::param)
        startValue = processor.value(pressed.index);
    if (pressed.kind == Kind::routeDepth)
        startValue = processor.route(pressed.index).amount;
    repaint();
}
void Editor::mouseDrag(const juce::MouseEvent &e)
{
    mouseNow = virtualPoint(e.position);
    const auto target = findHit(mouseNow);
    hover = target;
    const auto distance = mouseNow - mouseStart;
    if (distance.getDistanceFromOrigin() > 3.f)
        dragging = true;
    if (pressed.kind == Kind::key && target.kind == Kind::key && target.index != pointerNote)
    {
        stopNote(pointerNote);
        pointerNote = target.index;
        playNote(pointerNote);
    }
    const float delta = (distance.x * .26f - distance.y) / (e.mods.isShiftDown() ? 710.f : 126.f);
    if (dragging && pressed.kind == Kind::param)
    {
        if (!inGesture)
        {
            processor.beginGesture(pressed.index);
            inGesture = true;
        }
        processor.setValue(pressed.index, startValue + delta);
    }
    if (dragging && pressed.kind == Kind::routeDepth)
    {
        auto route = processor.route(pressed.index);
        route.amount = std::clamp(startValue + delta * 2.f, -1.f, 1.f);
        processor.setRoute(pressed.index, route);
    }
    repaint();
}
void Editor::mouseUp(const juce::MouseEvent &e)
{
    mouseNow = virtualPoint(e.position);
    const auto target = findHit(mouseNow);
    if (inGesture)
    {
        processor.endGesture(pressed.index);
        inGesture = false;
    }
    if (pointerNote >= 0)
    {
        stopNote(pointerNote);
        pointerNote = -1;
    }
    if (pressed.kind == Kind::source && dragging && target.kind == Kind::param)
        assignModulation(pressed.index, target.index);
    else if (!dragging && target.same(pressed))
        perform(target);
    pressed = {};
    dragging = false;
    repaint();
}
void Editor::mouseDoubleClick(const juce::MouseEvent &e)
{
    const auto hit = findHit(virtualPoint(e.position));
    if (hit.kind == Kind::param)
        processor.change(hit.index, parameters()[std::size_t(hit.index)].initial);
    else if (hit.kind == Kind::routeDepth)
    {
        auto route = processor.route(hit.index);
        route.amount = .25f;
        processor.setRoute(hit.index, route);
    }
    repaint();
}
void Editor::mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel)
{
    const auto hit = findHit(virtualPoint(e.position));
    if (hit.kind == Kind::param && wheel.deltaY != 0.f)
    {
        const auto &info = parameters()[std::size_t(hit.index)];
        const float delta = info.choices > 1 ? std::copysign(1.f / float(info.choices - 1), wheel.deltaY)
                                             : wheel.deltaY * (e.mods.isShiftDown() ? .035f : .18f);
        processor.change(hit.index, processor.value(hit.index) + delta);
    }
    if (hit.kind == Kind::routeDepth)
    {
        auto route = processor.route(hit.index);
        route.amount = std::clamp(route.amount + wheel.deltaY * .27f, -1.f, 1.f);
        processor.setRoute(hit.index, route);
    }
    repaint();
}
void Editor::perform(const Hit &hit)
{
    switch (hit.kind)
    {
    case Kind::param:
        if (armedSource >= 0)
            assignModulation(armedSource, hit.index);
        break;
    case Kind::choice:
        chooseParameter(hit);
        break;
    case Kind::toggle:
        processor.change(hit.index, processor.value(hit.index) < .5f ? 1.f : 0.f);
        break;
    case Kind::source:
        armedSource = armedSource == hit.index ? -1 : hit.index;
        selectedSource = hit.index;
        openLowerFor(hit.index);
        break;
    case Kind::routeSelect:
        selectedRoute = hit.index;
        selectedSource = processor.route(hit.index).source;
        break;
    case Kind::routeClear:
        processor.setRoute(hit.index, {});
        setStatus("Modulation route removed.");
        break;
    case Kind::routeScroll:
        routeOffset = std::clamp(routeOffset + hit.index * 4, 0, maxRoutes - 4);
        break;
    case Kind::menu:
        showMenu();
        break;
    case Kind::presetMenu:
        showPresets();
        break;
    case Kind::presetPrevious:
        allOff();
        processor.selectFactory(std::max(0, processor.factoryIndex()) - 1);
        activeLayer = 0;
        break;
    case Kind::presetNext:
        allOff();
        processor.selectFactory(processor.factoryIndex() + 1);
        activeLayer = 0;
        break;
    case Kind::page:
        page = static_cast<Page>(hit.index);
        break;
    case Kind::lower:
        lower = static_cast<Lower>(hit.index);
        if (lower == Lower::routes)
            armedSource = -1;
        break;
    case Kind::layer:
        activeLayer = hit.index;
        break;
    case Kind::loadSample:
        showSampleMenu(hit.index);
        break;
    case Kind::layerPower:
    case Kind::filterPower:
    case Kind::effectPower:
    {
        const bool filter = hit.kind == Kind::filterPower;
        const int param = hit.kind == Kind::layerPower ? layerParameter(hit.index, aLevel) : hit.index;
        const float value = processor.value(param);
        const bool enabled = filter ? pick(value, 5) != int(FilterType::bypass) : value > .001f;
        auto &saved = bypassRecall[std::size_t(param)];
        if (enabled)
            saved = value;
        processor.change(param, enabled ? (filter ? 1.f : 0.f) : saved);
        break;
    }
    case Kind::seqStart:
        processor.toggleSequencer();
        break;
    case Kind::seqRandom:
        processor.randomiseSteps();
        setStatus("New pattern generated in the selected scale.");
        break;
    case Kind::octave:
        octave = std::clamp(octave + hit.index, 1, 7);
        break;
    case Kind::hold:
        toggleHold();
        break;
    case Kind::panic:
        allOff();
        setStatus("All notes and tails cleared.");
        break;
    case Kind::record:
        if (processor.isRecording())
        {
            const auto result = processor.stopRecording();
            setStatus(result.wasOk() ? "Stereo WAV saved." : result.getErrorMessage());
        }
        else
            pickFile(wavOperation);
        break;
    case Kind::audio:
        processor.useAudioSettings();
        break;
    case Kind::visual:
        animate = !animate;
        setStatus(animate ? "Reactive visuals enabled." : "Reactive visuals paused.");
        break;
    default:
        break;
    }
}
void Editor::chooseParameter(const Hit &hit)
{
    const auto &info = parameters()[std::size_t(hit.index)];
    if (info.choices <= 1)
        return;
    juce::PopupMenu menu;
    menu.setLookAndFeel(&skin());
    for (int i = 0; i < info.choices; ++i)
        menu.addItem(i + 1, choiceText(hit.index, i), true,
                     pick(processor.value(hit.index), info.choices) == i);
    juce::Component::SafePointer<Editor> safe(this);
    const auto area =
        hit.area.transformedBy(juce::AffineTransform::scale(scale()).translated(origin().x, origin().y))
            .getSmallestIntegerContainer();
    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetComponent(this)
                           .withTargetScreenArea(localAreaToGlobal(area))
                           .withMinimumWidth(134),
                       [safe, param = hit.index, count = info.choices](int id)
                       {
                           if (safe == nullptr || id == 0)
                               return;
                           safe->processor.change(param, fromChoice(id - 1, count));
                           if (param == aType || param == bType)
                               safe->activeLayer = param / bType;
                           safe->repaint();
                       });
}
void Editor::showMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&skin());
    menu.addSectionHeader("MICA FIELD / INSTRUMENT");
    menu.addItem(1, "Open patch...");
    menu.addItem(2, "Save patch as...");
    menu.addItem(3, "Factory bank...");
    menu.addSeparator();
    menu.addItem(4, processor.isRecording() ? "Stop recording" : "Record stereo WAV...");
    menu.addItem(5, "Audio / MIDI settings...", processor.isStandalone());
    menu.addItem(6, "Reactive graphics", true, animate);
    menu.addItem(7, "Panic / clear tails");
    juce::Component::SafePointer<Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(206),
                       [safe](int id)
                       {
                           if (safe == nullptr)
                               return;
                           if (id == 1)
                               safe->pickFile(patchOpen);
                           if (id == 2)
                               safe->pickFile(patchSave);
                           if (id == 3)
                               safe->showPresets();
                           if (id == 4)
                               safe->perform({Kind::record, 0, {}, {}});
                           if (id == 5)
                               safe->processor.useAudioSettings();
                           if (id == 6)
                               safe->perform({Kind::visual, 0, {}, {}});
                           if (id == 7)
                               safe->perform({Kind::panic, 0, {}, {}});
                           safe->repaint();
                       });
}
void Editor::showPresets()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&skin());
    menu.addSectionHeader("FACTORY BANK / 01");
    const auto &bank = factory();
    for (std::size_t i = 0; i < bank.size(); ++i)
        menu.addItem(int(i + 1), juce::String(int(i + 1)).paddedLeft('0', 2) + "   " + str(bank[i].name),
                     true, int(i) == processor.factoryIndex());
    menu.addSeparator();
    menu.addItem(100, "Open patch...");
    menu.addItem(101, "Save patch as...");
    juce::Component::SafePointer<Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(214),
                       [safe](int id)
                       {
                           if (safe == nullptr || id == 0)
                               return;
                           if (id == 100)
                               safe->pickFile(patchOpen);
                           else if (id == 101)
                               safe->pickFile(patchSave);
                           else
                           {
                               safe->allOff();
                               safe->processor.selectFactory(id - 1);
                               safe->activeLayer = 0;
                               safe->setStatus(str(factory()[std::size_t(id - 1)].description));
                           }
                           safe->repaint();
                       });
}
void Editor::showSampleMenu(int slot)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&skin());
    menu.addItem(1, "Load WAV, AIFF, FLAC or MP3...");
    menu.addItem(2, "Use Weathered Tape", processor.sample(slot) != nullptr);
    juce::Component::SafePointer<Editor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [safe, slot](int id)
                       {
                           if (safe == nullptr)
                               return;
                           if (id == 1)
                               safe->pickFile(sampleOpen, slot);
                           if (id == 2)
                           {
                               safe->processor.clearSample(slot);
                               safe->repaint();
                           }
                       });
}
void Editor::pickFile(int operation, int slot)
{
    const bool save = operation == wavOperation || operation == patchSave;
    auto initial = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    juce::String title, pattern, extension;
    switch (operation)
    {
    case wavOperation:
        title = "Record a stereo WAV";
        pattern = "*.wav";
        extension = ".wav";
        initial = initial.getChildFile("Mica-Field-" +
                                       juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S") + extension);
        break;
    case patchSave:
        title = "Save Mica Field patch";
        pattern = "*.mica";
        extension = ".mica";
        initial = initial.getChildFile(juce::File::createLegalFileName(processor.patchName()) + extension);
        break;
    case patchOpen:
        title = "Open Mica Field patch";
        pattern = "*.mica";
        break;
    default:
        title = "Load audio into Engine " + juce::String(slot + 1);
        pattern = "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.WAV;*.AIFF;*.MP3;*.FLAC";
        break;
    }
    fileDialog = std::make_unique<juce::FileChooser>(title, initial, pattern);
    juce::Component::SafePointer<Editor> safe(this);
    const int flags =
        juce::FileBrowserComponent::canSelectFiles |
        (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
              : juce::FileBrowserComponent::openMode);
    fileDialog->launchAsync(flags,
                            [safe, operation, slot, extension](const juce::FileChooser &chooser)
                            {
                                if (safe == nullptr)
                                    return;
                                auto file = chooser.getResult();
                                if (file == juce::File())
                                    return;
                                if (extension.isNotEmpty() && !file.hasFileExtension(extension))
                                    file = file.withFileExtension(extension);
                                juce::Result result = juce::Result::ok();
                                juce::String success;
                                switch (operation)
                                {
                                case wavOperation:
                                    result = safe->processor.startRecording(file);
                                    success = "Recording stereo WAV. Press REC to finish.";
                                    break;
                                case patchSave:
                                    result = safe->processor.savePatch(file);
                                    success = "Patch saved with its samples and routes.";
                                    break;
                                case patchOpen:
                                    safe->allOff();
                                    result = safe->processor.openPatch(file);
                                    success = "Patch loaded.";
                                    break;
                                default:
                                    result = safe->processor.readSample(slot, file);
                                    success = "Audio loaded. C4 plays at source pitch.";
                                    break;
                                }
                                safe->setStatus(result.wasOk() ? success : result.getErrorMessage());
                                safe->repaint();
                            });
}
void Editor::setStatus(juce::String message)
{
    status = std::move(message);
    statusUntil = juce::Time::getMillisecondCounterHiRes() + 4300.;
    redraw = true;
}
void Editor::assignModulation(int source, int target)
{
    if (!parameters()[std::size_t(target)].modTarget)
    {
        setStatus("Drop the source on an outlined knob.");
        return;
    }
    const int slot = processor.addRoute(source, target);
    if (slot < 0)
    {
        setStatus("All 16 routes are in use.");
        return;
    }
    selectedSource = source;
    selectedRoute = slot;
    routeOffset = (slot / 4) * 4;
    armedSource = -1;
    lower = Lower::routes;
    setStatus(str(sourceNames[std::size_t(source)]) + " > " + targetName(target) +
              " / Drag Depth to set the range");
}
void Editor::openLowerFor(int source)
{
    if (source >= int(Source::amp) && source <= int(Source::env3))
        lower = Lower::envelopes;
    else if (source >= int(Source::lfo1) && source <= int(Source::lfo3))
        lower = Lower::lfos;
    else if (source == int(Source::random))
        lower = Lower::random;
    else if (source <= int(Source::wheel))
        lower = Lower::keyboard;
}
void Editor::toggleHold()
{
    holdNotes = !holdNotes;
    if (!holdNotes)
        releaseLatched();
    setStatus(holdNotes ? "Hold on. Play a held note again to release it." : "Hold off.");
}
void Editor::playNote(int note)
{
    if (note < 0 || note > 127)
        return;
    if (holdNotes)
    {
        if (latched[std::size_t(note)])
        {
            stopNote(note, true);
            return;
        }
        latched[std::size_t(note)] = true;
    }
    processor.keyboard.noteOn(1, note, .81f);
}
void Editor::stopNote(int note, bool force)
{
    if (note < 0 || note > 127 || (latched[std::size_t(note)] && !force))
        return;
    latched[std::size_t(note)] = false;
    processor.keyboard.noteOff(1, note, .5f);
}
void Editor::releaseComputerNotes()
{
    for (auto &note : computerNotes)
    {
        if (note >= 0)
            stopNote(note);
        note = -1;
    }
}
void Editor::releaseLatched()
{
    for (int i = 0; i < 128; ++i)
        if (latched[std::size_t(i)])
            stopNote(i, true);
}
void Editor::allOff()
{
    releaseComputerNotes();
    releaseLatched();
    processor.requestPanic();
    pointerNote = -1;
}
void Editor::focusLost(FocusChangeType)
{
    releaseComputerNotes();
    if (pointerNote >= 0)
    {
        stopNote(pointerNote);
        pointerNote = -1;
    }
}
bool Editor::keyPressed(const juce::KeyPress &key)
{
    if (key.getModifiers().isAnyModifierKeyDown())
        return false;
    if (key == juce::KeyPress::escapeKey)
    {
        allOff();
        repaint();
        return true;
    }
    if (key == juce::KeyPress::spaceKey && page == Page::sequence)
    {
        processor.toggleSequencer();
        repaint();
        return true;
    }
    const int c = int(juce::CharacterFunctions::toUpperCase(key.getTextCharacter()));
    if (c == 'Z' || c == 'X')
    {
        octave = std::clamp(octave + (c == 'X' ? 1 : -1), 1, 7);
        repaint();
        return true;
    }
    for (std::size_t i = 0; i < keyCodes.size(); ++i)
        if (c == keyCodes[i])
        {
            if (computerNotes[i] < 0)
            {
                computerNotes[i] = (octave + 1) * 12 + int(i);
                playNote(computerNotes[i]);
            }
            repaint();
            return true;
        }
    return false;
}
bool Editor::keyStateChanged(bool)
{
    bool changed = false;
    for (std::size_t i = 0; i < keyCodes.size(); ++i)
        if (computerNotes[i] >= 0 && !juce::KeyPress::isKeyCurrentlyDown(keyCodes[i]))
        {
            stopNote(computerNotes[i]);
            computerNotes[i] = -1;
            changed = true;
        }
    if (changed)
        repaint();
    return changed;
}
void Editor::timerCallback()
{
    const bool active = processor.playingVoices.load() > 0 || processor.peakL.load() > .0005f;
    if (animate && active)
        time += 1.f / 24.f;
    juce::uint64 signature = juce::uint64(processor.factoryIndex() + 1);
    for (int i = 0; i < paramCount; ++i)
        signature = signature * 991u + juce::uint64(processor.value(i) * 65535.f);
    for (int i = 0; i < maxRoutes; ++i)
        signature = signature * 991u + packRoute(processor.route(i));
    signature = signature * 991u + juce::uint64(processor.step.load() + 1);
    signature = signature * 991u + juce::uint64(std::min(processor.cpuLoad.load() * 100.f, 150.f));
    signature = signature * 991u + juce::uint64(std::min(processor.peakL.load() * 84.f, 84.f));
    signature = signature * 2u + (processor.isEdited() ? 1u : 0u);
    signature = signature * 2u + (processor.sequencerOn() ? 1u : 0u);
    signature = signature * 2u + (statusUntil > juce::Time::getMillisecondCounterHiRes() ? 1u : 0u);
    if (signature != lastUiSignature || redraw || (animate && active) || processor.isRecording())
        repaint();
    lastUiSignature = signature;
}
} // namespace mica
