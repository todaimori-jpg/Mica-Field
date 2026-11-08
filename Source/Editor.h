// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Instrument.h"

namespace mica
{
class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit Editor(Instrument &);
    ~Editor() override;
    void paint(juce::Graphics &) override;
    void mouseMove(const juce::MouseEvent &) override;
    void mouseExit(const juce::MouseEvent &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseDoubleClick(const juce::MouseEvent &) override;
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) override;
    bool keyPressed(const juce::KeyPress &) override;
    bool keyStateChanged(bool) override;
    void focusLost(FocusChangeType) override;

  private:
    using Colour = juce::Colour;
    using Rect = juce::Rectangle<float>;
    using Point = juce::Point<float>;
    enum class Page
    {
        play,
        synth,
        effects,
        sequence
    };
    enum class Lower
    {
        keyboard,
        envelopes,
        lfos,
        random,
        routes
    };
    enum class Kind
    {
        none,
        param,
        choice,
        toggle,
        source,
        routeDepth,
        routeSelect,
        routeClear,
        routeScroll,
        key,
        menu,
        presetMenu,
        presetPrevious,
        presetNext,
        page,
        lower,
        layer,
        loadSample,
        layerPower,
        seqStart,
        seqRandom,
        octave,
        hold,
        panic,
        record,
        audio,
        visual,
        filterPower,
        effectPower
    };
    struct Hit
    {
        Kind kind = Kind::none;
        int index = 0;
        Rect area;
        juce::String name;
        bool same(const Hit &b) const { return kind == b.kind && index == b.index; }
    };
    static constexpr int canvasWidth = 1240, canvasHeight = 758;
    static const Colour base, panel, raised, header, hairline, ink, muted, dim, cyan, orange, gold, violet,
        pink, mint;
    static Colour sourceColour(int);
    static juce::String str(std::string_view);
    static juce::String noteName(int);
    static void label(juce::Graphics &, const juce::String &, Rect, Colour, float = 12.f,
                      juce::Justification = juce::Justification::centred, bool = false);
    static void fill(juce::Graphics &, Rect, Colour);
    static void line(juce::Graphics &, Point, Point, Colour, float = 1.f);
    static void module(juce::Graphics &, Rect, Colour = panel);
    static void graphGrid(juce::Graphics &, Rect, int, int);
    static void trace(juce::Graphics &, Rect, Colour, const std::function<float(float)> &, bool = false);
    static void icon(juce::Graphics &, Rect, const juce::String &, Colour);
    void drawTop(juce::Graphics &);
    void drawSynth(juce::Graphics &);
    void drawEngineGraph(juce::Graphics &, Rect, int);
    void drawFilter(juce::Graphics &, int, Rect);
    void drawEffects(juce::Graphics &);
    void drawSequence(juce::Graphics &);
    void drawPlay(juce::Graphics &);
    void drawModulation(juce::Graphics &);
    void drawLower(juce::Graphics &);
    void drawKeyboard(juce::Graphics &, Rect);
    void drawFooter(juce::Graphics &);
    void drawEnvelope(juce::Graphics &, Rect, int);
    void drawLfo(juce::Graphics &, Rect, int);
    void drawRoutes(juce::Graphics &, Rect);
    void drawMacro(juce::Graphics &, Point, int, float = 21.f);
    void knob(juce::Graphics &, Point, int, const juce::String &, Colour, float = 16.f);
    void dial(juce::Graphics &, Point, float, Colour, float, bool, float = 0.f, Colour = cyan);
    void button(juce::Graphics &, Rect, const juce::String &, Kind, int = 0, bool = false, Colour = cyan);
    void graphicalButton(juce::Graphics &, Rect, const juce::String &, Kind, int, const juce::String &,
                         Colour, bool = false);
    float scale() const;
    Point origin() const;
    Point virtualPoint(Point) const;
    Hit findHit(Point) const;
    void addHit(Kind, int, Rect, juce::String);
    void perform(const Hit &);
    void chooseParameter(const Hit &);
    void showMenu();
    void showPresets();
    void showSampleMenu(int);
    void pickFile(int, int = 0);
    void setStatus(juce::String);
    void assignModulation(int, int);
    void openLowerFor(int);
    void toggleHold();
    void playNote(int);
    void stopNote(int, bool = false);
    void releaseComputerNotes();
    void releaseLatched();
    void allOff();
    void timerCallback() override;
    juce::String valueText(int) const;
    juce::String choiceText(int, int) const;
    juce::String targetName(int) const;
    int layerParam(Param p) const { return layerParameter(activeLayer, p); }
    int selectedEngine(int l) const { return pick(processor.value(layerParameter(l, aType)), 6); }
    Instrument &processor;
    Page page = Page::synth;
    Lower lower = Lower::envelopes;
    int activeLayer = 0, selectedSource = int(Source::lfo1), armedSource = -1, selectedRoute = 0,
        routeOffset = 0, octave = 4;
    bool holdNotes = false, animate = true;
    int pointerNote = -1;
    std::array<bool, 128> latched{};
    std::array<int, 18> computerNotes{};
    std::array<float, paramCount> bypassRecall{};
    std::vector<Hit> hits;
    Hit hover, pressed;
    Point mouseStart, mouseNow;
    float startValue = 0.f;
    bool dragging = false, inGesture = false;
    std::unique_ptr<juce::FileChooser> fileDialog;
    juce::String status;
    double statusUntil = 0.;
    float time = 0.f;
    bool redraw = true;
    juce::uint64 lastUiSignature = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};
} // namespace mica
