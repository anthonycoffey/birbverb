#include "PluginEditor.h"
#include "dsp/CircuitModel.h"
#include "dsp/TankConfig.h"

using namespace juce;

//==============================================================================
// The fallback palette. When the sprite art lands, almost none of this is drawn
// any more — the background atlas carries the paint, the labels and the knob
// wells. Until then this is the whole UI, and it is what lets SPEC-001 through
// SPEC-004 proceed without waiting on a single PNG.
namespace colours
{
    const Colour bodyEdge   { 0xff17221c };
    const Colour face       { 0xff20302a };
    const Colour faceTop    { 0xff27392f };
    const Colour cream      { 0xffe8e3d1 };
    const Colour creamRim   { 0xffb3ab93 };
    const Colour pointer    { 0xff1d2a24 };
    const Colour amber      { 0xffffb545 };
    const Colour textLight  { 0xffd9e2da };
    const Colour textMute   { 0xff8aa094 };
    const Colour wellDark   { 0xff121b16 };
    const Colour wellStroke { 0xff38503f };
    const Colour screenBg   { 0xff0b120e };
    const Colour metal      { 0xffc8ccd2 };
}

// SPEC-005 layout. Positions are the top-left of each control's frame rectangle
// in the 600 x 650 editor space; the manifest can override them without a code
// change, which is the point of having a manifest.
namespace layout
{
    constexpr int editorWidth  = 600;
    constexpr int editorHeight = 650;

    constexpr int largeKnob = 104;
    constexpr int smallKnob = 72;

    constexpr int labelHeight = 18;
}

//==============================================================================
BirbLookAndFeel::BirbLookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, colours::screenBg);
    setColour (PopupMenu::textColourId, colours::textLight);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::wellStroke);
    setColour (PopupMenu::highlightedTextColourId, colours::amber);
    setColour (AlertWindow::backgroundColourId, colours::face);
    setColour (AlertWindow::textColourId, colours::textLight);
    setColour (TextEditor::backgroundColourId, colours::screenBg);
    setColour (TextEditor::textColourId, colours::textLight);
}

void BirbLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                        float pos, float startAngle, float endAngle,
                                        Slider&)
{
    auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const auto centre = bounds.getCentre();
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Well
    g.setColour (colours::wellDark);
    g.fillEllipse (bounds);

    // Travel arc
    Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 1.5f, radius - 1.5f,
                       0.0f, startAngle, angle, true);
    g.setColour (colours::amber.withAlpha (0.8f));
    g.strokePath (arc, PathStrokeType (2.0f));

    // Cap
    const float capRadius = radius * 0.72f;
    g.setColour (colours::cream);
    g.fillEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);
    g.setColour (colours::creamRim);
    g.drawEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

    // Pointer
    Path needle;
    needle.addRoundedRectangle (-1.5f, -capRadius + 2.0f, 3.0f, capRadius * 0.62f, 1.5f);
    g.setColour (colours::pointer);
    g.fillPath (needle, AffineTransform::rotation (angle).translated (centre));
}

//==============================================================================
SpriteKnob::SpriteKnob()
{
    setSliderStyle (Slider::RotaryVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f,
                         MathConstants<float>::pi * 2.75f, true);   // 270 degrees
}

void SpriteKnob::paint (Graphics& g)
{
    if (atlas != nullptr)
    {
        const double normalised = valueToProportionOfLength (getValue());
        atlas->drawFrameForValue (g, normalised, getLocalBounds());
        return;
    }

    Slider::paint (g);   // falls through to BirbLookAndFeel::drawRotarySlider
}

//==============================================================================
SpriteSwitch::SpriteSwitch (RangedAudioParameter& param, StringArray labels,
                            String switchName)
    : parameter (param), positionLabels (std::move (labels)),
      name (std::move (switchName)),
      attachment (param, [this] (float v)
                  {
                      position = jlimit (0, jmax (0, positionLabels.size() - 1), (int) v);
                      repaint();
                  })
{
    attachment.sendInitialUpdate();
}

void SpriteSwitch::paint (Graphics& g)
{
    if (atlas != nullptr)
    {
        atlas->drawFrame (g, position, getLocalBounds());
        return;
    }

    auto b = getLocalBounds();
    auto nameArea = b.removeFromBottom (layout::labelHeight);

    g.setColour (colours::wellDark);
    g.fillRoundedRectangle (b.toFloat(), 4.0f);
    g.setColour (colours::wellStroke);
    g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 4.0f, 1.0f);

    const int n = positionLabels.size();
    if (n > 0)
    {
        const float slotHeight = (float) b.getHeight() / (float) n;

        // The bat, sitting in the active slot.
        auto bat = Rectangle<float> ((float) b.getX() + 4.0f,
                                     (float) b.getY() + slotHeight * (float) position + 2.0f,
                                     (float) b.getWidth() - 8.0f,
                                     slotHeight - 4.0f);
        g.setColour (colours::metal);
        g.fillRoundedRectangle (bat, 3.0f);

        g.setFont (FontOptions (n >= 4 ? 8.5f : 10.0f, Font::bold));
        for (int i = 0; i < n; ++i)
        {
            auto slot = Rectangle<int> (b.getX(), b.getY() + (int) (slotHeight * (float) i),
                                        b.getWidth(), (int) slotHeight);
            g.setColour (i == position ? colours::pointer : colours::textMute);
            g.drawText (positionLabels[i], slot, Justification::centred);
        }
    }

    g.setColour (colours::textLight);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (name, nameArea, Justification::centred);
}

void SpriteSwitch::mouseDown (const MouseEvent&)
{
    const int n = jmax (1, positionLabels.size());
    const int next = (position + 1) % n;

    attachment.setValueAsCompleteGesture ((float) next);
}

//==============================================================================
SpriteFootswitch::SpriteFootswitch (RangedAudioParameter& bypassParam)
    : parameter (bypassParam),
      attachment (bypassParam, [this] (float v)
                  {
                      bypassed = v > 0.5f;
                      repaint();
                  })
{
    attachment.sendInitialUpdate();
}

void SpriteFootswitch::paint (Graphics& g)
{
    if (atlas != nullptr)
    {
        atlas->drawFrame (g, bypassed ? 0 : 1, getLocalBounds());
        return;
    }

    auto b = getLocalBounds();
    auto labelArea = b.removeFromBottom (layout::labelHeight);
    auto circle = b.toFloat().reduced (4.0f);

    g.setColour (colours::wellDark);
    g.fillEllipse (circle);
    g.setColour (colours::metal.withAlpha (bypassed ? 0.55f : 1.0f));
    g.fillEllipse (circle.reduced (8.0f));
    g.setColour (colours::wellStroke);
    g.drawEllipse (circle.reduced (8.0f), 1.5f);

    g.setColour (colours::textLight);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (bypassed ? "BYPASSED" : "ENGAGE", labelArea, Justification::centred);
}

void SpriteFootswitch::mouseDown (const MouseEvent&)
{
    attachment.setValueAsCompleteGesture (bypassed ? 0.0f : 1.0f);
}

//==============================================================================
void SpriteLED::setLevel (float newLevel)
{
    if (std::abs (newLevel - level) < 0.01f) return;
    level = jlimit (0.0f, 1.0f, newLevel);
    repaint();
}

void SpriteLED::paint (Graphics& g)
{
    if (atlas != nullptr)
    {
        atlas->drawFrameForValue (g, (double) level, getLocalBounds());
        return;
    }

    auto b = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (colours::wellDark);
    g.fillEllipse (b);
    g.setColour (colours::amber.withAlpha (0.25f + 0.75f * level));
    g.fillEllipse (b.reduced (3.0f));
    g.setColour (colours::wellStroke);
    g.drawEllipse (b, 1.0f);
}

//==============================================================================
PresetBar::PresetBar (PresetManager& p) : presets (p)
{
    displayed = presets.displayName();
}

void PresetBar::setAtlases (const bv::ui::SpriteAtlas* screenAtlas,
                            const bv::ui::SpriteAtlas* arrowAtlas)
{
    screenArt = screenAtlas;
    arrowArt  = arrowAtlas;
    repaint();
}

void PresetBar::refresh()
{
    const auto name = presets.displayName();
    if (name == displayed) return;
    displayed = name;
    repaint();
}

void PresetBar::resized()
{
    auto b = getLocalBounds();
    leftArrow  = b.removeFromLeft (36);
    rightArrow = b.removeFromRight (36);
    screen     = b.reduced (6, 0);
}

void PresetBar::paint (Graphics& g)
{
    if (screenArt != nullptr)
        screenArt->drawFrame (g, 0, screen);
    else
    {
        g.setColour (colours::screenBg);
        g.fillRoundedRectangle (screen.toFloat(), 4.0f);
        g.setColour (colours::wellStroke);
        g.drawRoundedRectangle (screen.toFloat().reduced (0.5f), 4.0f, 1.0f);
    }

    g.setColour (colours::amber);
    g.setFont (FontOptions (16.0f, Font::bold));
    g.drawText (displayed, screen, Justification::centred);

    auto drawArrow = [this, &g] (Rectangle<int> area, bool pointingLeft)
    {
        if (arrowArt != nullptr)
        {
            arrowArt->drawFrame (g, 0, area);
            return;
        }

        g.setColour (colours::wellDark);
        g.fillRoundedRectangle (area.toFloat().reduced (3.0f), 3.0f);

        Path tri;
        auto c = area.toFloat().getCentre();
        const float s = 6.0f;
        if (pointingLeft) tri.addTriangle (c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s, c.x - s * 0.6f, c.y);
        else              tri.addTriangle (c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s, c.x + s * 0.6f, c.y);
        g.setColour (colours::textLight);
        g.fillPath (tri);
    };

    drawArrow (leftArrow, true);
    drawArrow (rightArrow, false);
}

void PresetBar::mouseDown (const MouseEvent& e)
{
    if (leftArrow.contains (e.getPosition()))       { presets.prev(); refresh(); }
    else if (rightArrow.contains (e.getPosition())) { presets.next(); refresh(); }
    else                                            showBrowser();
}

void PresetBar::showBrowser()
{
    PopupMenu menu;

    // Foundations first, then derived — SPEC-004 requirement 7. The separation is
    // the point of the bank: a foundation is a complete circuit, a derived preset
    // is somewhere someone went from one.
    PopupMenu foundations, derived;
    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        auto& target = presets.isFoundation (i) ? foundations : derived;
        target.addItem (i + 1, presets.getName (i), true, i == presets.currentIndex());
    }
    menu.addSectionHeader ("Foundations");
    menu.addSubMenu ("Amps", foundations, true);
    menu.addSectionHeader ("Built on a foundation");
    menu.addSubMenu ("Variations", derived, true);

    const auto userFiles = presets.userPresetFiles();
    if (! userFiles.isEmpty())
    {
        PopupMenu userMenu;
        for (int i = 0; i < userFiles.size(); ++i)
            userMenu.addItem (1000 + i, userFiles[i].getFileNameWithoutExtension());
        menu.addSeparator();
        menu.addSubMenu ("User presets", userMenu, true);
    }

    menu.addSeparator();
    menu.addItem (900, "Save current...");
    if (! presets.isFoundation (presets.currentIndex()))
        menu.addItem (901, "Back to foundation");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
                        [this, userFiles] (int result)
                        {
                            if (result == 0) return;

                            if (result == 900)      showSaveDialog();
                            else if (result == 901) presets.loadFoundation();
                            else if (result >= 1000) presets.loadUserPreset (userFiles[result - 1000]);
                            else                    presets.load (result - 1);

                            refresh();
                        });
}

void PresetBar::showSaveDialog()
{
    saveDialog = std::make_unique<AlertWindow> ("Save preset", "Name:", MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", presets.displayName().removeCharacters ("*").trim());
    saveDialog->addButton ("Save", 1);
    saveDialog->addButton ("Cancel", 0);

    saveDialog->enterModalState (true, ModalCallbackFunction::create (
        [this] (int result)
        {
            if (result == 1 && saveDialog != nullptr)
                presets.saveUserPreset (saveDialog->getTextEditorContents ("name"));

            saveDialog.reset();
            refresh();
        }), false);
}

//==============================================================================
BirbverbAudioProcessorEditor::BirbverbAudioProcessorEditor (BirbverbAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), presetBar (p.presets)
{
    setLookAndFeel (&lnf);

    styleKnob (dwell, bv::ids::dwell, "knob_large");
    styleKnob (decay, bv::ids::decay, "knob_large");
    styleKnob (tone,  bv::ids::tone,  "knob_large");
    styleKnob (locut, bv::ids::locut, "knob_small");
    styleKnob (mix,   bv::ids::mix,   "knob_small");

    // boing and level are effect controls rather than modelled circuit
    // behaviour, so they get the alternate cap — SPEC-005 requirement 10. The
    // distinction lives in the art, not in a tint.
    styleKnob (boing, bv::ids::boing, "knob_small_alt");
    styleKnob (level, bv::ids::level, "knob_small_alt");

    dwellAtt = std::make_unique<SA> (p.apvts, bv::ids::dwell, dwell);
    decayAtt = std::make_unique<SA> (p.apvts, bv::ids::decay, decay);
    toneAtt  = std::make_unique<SA> (p.apvts, bv::ids::tone,  tone);
    locutAtt = std::make_unique<SA> (p.apvts, bv::ids::locut, locut);
    boingAtt = std::make_unique<SA> (p.apvts, bv::ids::boing, boing);
    mixAtt   = std::make_unique<SA> (p.apvts, bv::ids::mix,   mix);
    levelAtt = std::make_unique<SA> (p.apvts, bv::ids::level, level);

    if (auto* circuitParam = p.apvts.getParameter (bv::ids::circuit))
    {
        circuitSwitch = std::make_unique<SpriteSwitch> (
            *circuitParam,
            StringArray { bv::circuitModelName (bv::CircuitModel::ab763),
                          bv::circuitModelName (bv::CircuitModel::vibroverb6G16),
                          bv::circuitModelName (bv::CircuitModel::reverbUnit6G15),
                          bv::circuitModelName (bv::CircuitModel::ampegRocket) },
            "CIRCUIT");
        circuitSwitch->setAtlas (sprites.atlas ("switch_4way"));
        addAndMakeVisible (*circuitSwitch);
    }

    if (auto* tankParam = p.apvts.getParameter (bv::ids::tank))
    {
        tankSwitch = std::make_unique<SpriteSwitch> (
            *tankParam,
            StringArray { "SHORT 2", "LONG 2", "LONG 3" },
            "TANK");
        tankSwitch->setAtlas (sprites.atlas ("switch_3way"));
        addAndMakeVisible (*tankSwitch);
    }

    if (auto* bypassParam = p.apvts.getParameter (bv::ids::bypass))
    {
        footswitch = std::make_unique<SpriteFootswitch> (*bypassParam);
        footswitch->setAtlas (sprites.atlas ("footswitch"));
        addAndMakeVisible (*footswitch);
    }

    presetBar.setAtlases (sprites.atlas ("screen"), sprites.atlas ("arrow"));
    addAndMakeVisible (presetBar);

    led.setAtlas (sprites.atlas ("led"));
    addAndMakeVisible (led);

    setSize (layout::editorWidth, layout::editorHeight);
    startTimerHz (30);
}

BirbverbAudioProcessorEditor::~BirbverbAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void BirbverbAudioProcessorEditor::styleKnob (SpriteKnob& knob, const String& controlId,
                                              const char* atlasName)
{
    knob.setAtlas (sprites.atlas (atlasName));
    knob.setName (controlId);
    addAndMakeVisible (knob);
}

void BirbverbAudioProcessorEditor::timerCallback()
{
    led.setLevel (jlimit (0.0f, 1.0f, processor.getWetLevel() * 2.0f));
    presetBar.refresh();
}

void BirbverbAudioProcessorEditor::paint (Graphics& g)
{
    if (auto* background = sprites.atlas ("background"))
    {
        background->drawFrame (g, 0, getLocalBounds());
        return;
    }

    // Fallback enclosure.
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (colours::faceTop, 0.0f, 0.0f,
                                       colours::face, 0.0f, b.getHeight(), false));
    g.fillAll();
    g.setColour (colours::bodyEdge);
    g.drawRect (getLocalBounds(), 4);

    g.setColour (colours::cream);
    g.setFont (FontOptions (34.0f, Font::bold));
    g.drawText ("birbverb", 0, 28, getWidth(), 44, Justification::centred);

    g.setColour (colours::textMute);
    g.setFont (FontOptions (11.0f));
    g.drawText ("vintage spring reverb", 0, 70, getWidth(), 18, Justification::centred);

    // Knob labels. Once the background sprite lands, these are painted into it.
    g.setFont (FontOptions (11.0f, Font::bold));
    auto label = [&g] (const String& text, Component& c)
    {
        g.setColour (colours::textLight);
        g.drawText (text, c.getX() - 10, c.getBottom() + 2,
                    c.getWidth() + 20, layout::labelHeight, Justification::centred);
    };

    label ("DWELL", dwell);
    label ("DECAY", decay);
    label ("TONE",  tone);
    label ("LO CUT", locut);
    label ("MIX",   mix);

    // The two effect controls are marked, because the project's claim is that
    // behaviour traces to hardware and these two do not.
    g.setColour (colours::amber);
    g.drawText ("BOING",  boing.getX() - 10, boing.getBottom() + 2,
                boing.getWidth() + 20, layout::labelHeight, Justification::centred);
    g.drawText ("LEVEL",  level.getX() - 10, level.getBottom() + 2,
                level.getWidth() + 20, layout::labelHeight, Justification::centred);
}

void BirbverbAudioProcessorEditor::resized()
{
    // Positions come from the manifest when there is one, with the SPEC-005
    // layout as the default. Moving a control is then a manifest edit.
    auto place = [this] (Component& c, const String& id, int fx, int fy, int size)
    {
        const auto p = sprites.controlPosition (id, { fx, fy });
        c.setBounds (p.x, p.y, size, size);
    };

    presetBar.setBounds (134, 100, 332, 48);

    place (dwell, bv::ids::dwell,  84, 200, layout::largeKnob);
    place (decay, bv::ids::decay, 248, 200, layout::largeKnob);
    place (tone,  bv::ids::tone,  412, 200, layout::largeKnob);

    place (locut, bv::ids::locut,  60, 356, layout::smallKnob);
    place (boing, bv::ids::boing, 196, 356, layout::smallKnob);
    place (mix,   bv::ids::mix,   332, 356, layout::smallKnob);
    place (level, bv::ids::level, 468, 356, layout::smallKnob);

    if (circuitSwitch != nullptr)
    {
        const auto p = sprites.controlPosition (bv::ids::circuit, { 140, 470 });
        circuitSwitch->setBounds (p.x, p.y, 64, 112 + layout::labelHeight);
    }

    if (tankSwitch != nullptr)
    {
        const auto p = sprites.controlPosition (bv::ids::tank, { 396, 470 });
        tankSwitch->setBounds (p.x, p.y, 64, 112 + layout::labelHeight);
    }

    if (footswitch != nullptr)
    {
        const auto p = sprites.controlPosition (bv::ids::bypass, { 236, 500 });
        footswitch->setBounds (p.x, p.y, 128, 128 + layout::labelHeight);
    }

    led.setBounds (284, 616, 32, 32);
}
