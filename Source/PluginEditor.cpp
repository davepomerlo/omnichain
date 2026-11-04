#include "PluginEditor.h"

using namespace juce;

// =====================================================================================
namespace ocui
{
LookAndFeel::LookAndFeel()
{
    setColour (ComboBox::backgroundColourId, knobBody);
    setColour (ComboBox::textColourId, text);
    setColour (ComboBox::outlineColourId, Colours::transparentBlack);
    setColour (ComboBox::arrowColourId, textDim);
    setColour (PopupMenu::backgroundColourId, Colour (0xff1d2129));
    setColour (PopupMenu::textColourId, text);
    setColour (PopupMenu::headerTextColourId, Colour (0xff9aa6bd));
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3a4458));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextButton::buttonColourId, knobBody);
    setColour (TextButton::textColourOffId, text);
    setColour (TextButton::textColourOnId, text);
    setColour (Label::textColourId, text);
    setColour (BubbleComponent::backgroundColourId, Colour (0xf0101216));
    setColour (BubbleComponent::outlineColourId, Colour (0xff3a404d));
    setColour (TooltipWindow::backgroundColourId, Colour (0xf0101216));
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    const float r = jmin (w, h) * 0.5f - 3.0f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float ang = start + pos * (end - start);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

    Path trackP; trackP.addCentredArc (cx, cy, r, r, 0.0f, start, end, true);
    g.setColour (track); g.strokePath (trackP, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    const float from = bipolar ? (start + end) * 0.5f : start;
    Path valP; valP.addCentredArc (cx, cy, r, r, 0.0f, jmin (from, ang), jmax (from, ang), true);
    g.setColour (s.isEnabled() ? accent : accent.withSaturation (0.1f));
    g.strokePath (valP, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    const float br = r - 6.0f;
    ColourGradient grad (knobBody.brighter (0.25f), cx, cy - br, knobBody.darker (0.35f), cx, cy + br, false);
    g.setGradientFill (grad);
    g.fillEllipse (cx - br, cy - br, br * 2, br * 2);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (cx - br, cy - br, br * 2, br * 2, 1.0f);

    Path ptr;
    ptr.addRoundedRectangle (-1.6f, -br + 3.0f, 3.2f, br * 0.55f, 1.5f);
    g.setColour (text);
    g.fillPath (ptr, AffineTransform::rotation (ang).translated (cx, cy));
}

void LookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s)
{
    const auto accent = s.findColour (Slider::trackColourId);
    auto r = Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (track); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (accent); g.fillRoundedRectangle (r.withRight (pos), 3.0f);
    g.setColour (text); g.fillEllipse (pos - 5.0f, r.getCentreY() - 5.0f, 10.0f, 10.0f);
}

void LookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    if (box.isMouseOver (true)) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r, 5.0f); }
    Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (box.findColour (ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void LookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if ((bool) b.getProperties().getWithDefault ("power", false))
    {
        const bool on = ! b.getToggleState(); // toggle = bypassed
        const auto accent = b.findColour (TextButton::buttonOnColourId);
        g.setColour (on ? accent.withAlpha (0.22f) : knobBody);
        g.fillRoundedRectangle (r, 5.0f);
        const float cx = r.getCentreX(), cy = r.getCentreY(), rad = jmin (r.getWidth(), r.getHeight()) * 0.27f;
        Path p; p.addCentredArc (cx, cy, rad, rad, 0.0f, 0.65f, MathConstants<float>::twoPi - 0.65f, true);
        g.setColour (on ? accent : textDim.withAlpha (0.6f));
        g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        g.drawLine (cx, cy - rad - 1.5f, cx, cy - 1.0f, 1.8f);
        return;
    }
    g.setColour (knobBody.brighter (down ? 0.3f : (over ? 0.15f : 0.0f)));
    g.fillRoundedRectangle (r, 5.0f);
}

Label* LookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (Label::textColourId, text);
    return l;
}

Knob::Knob()
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (180);
    slider.setPopupDisplayEnabled (true, false, nullptr);
    slider.onValueChange = [this] { value.setText (slider.getTextFromValue (slider.getValue()), dontSendNotification); };
    name.setJustificationType (Justification::centred);
    name.setFont (Font (11.5f, Font::bold));
    name.setColour (Label::textColourId, textDim);
    value.setJustificationType (Justification::centred);
    value.setFont (Font (11.5f));
    name.setInterceptsMouseClicks (false, false);
    value.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider); addAndMakeVisible (name); addAndMakeVisible (value);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromTop (14));
    value.setBounds (r.removeFromBottom (15));
    slider.setBounds (r.reduced (2));
}

void Knob::setAccent (Colour c) { slider.setColour (Slider::rotarySliderFillColourId, c); }
} // namespace ocui

// =====================================================================================
SlotCard::SlotCard (OmniChainProcessor& p, OmniChainEditor& e, int i) : proc (p), editor (e), index (i)
{
    typeBox.addItem (oc::getInfo (0).name, 1);
    for (int c = 1; c < oc::NumCategories; ++c)
    {
        typeBox.addSectionHeading (oc::categoryName ((oc::Category) c));
        for (int t = 1; t < oc::NumEffectTypes; ++t)
            if (oc::getInfo (t).category == c) typeBox.addItem (oc::getInfo (t).name, t + 1);
    }
    typeBox.setTooltip ("Choose the effect for this slot");
    typeBox.onChange = [this]
    {
        const int id = typeBox.getSelectedId();
        if (id > 0 && id - 1 != proc.getSlotType (index)) proc.setSlotType (index, id - 1, true);
    };
    addAndMakeVisible (typeBox);

    power.setClickingTogglesState (true);
    power.getProperties().set ("power", true);
    power.setTooltip ("Bypass on / off");
    addAndMakeVisible (power);
    for (auto* b : { &left, &right })
    {
        b->setTooltip (b == &left ? "Move earlier in the chain" : "Move later in the chain");
        addAndMakeVisible (*b);
    }
    left.onClick  = [this] { if (index > 0) proc.moveSlot (index, index - 1); };
    right.onClick = [this] { if (index < oc::kNumSlots - 1) proc.moveSlot (index, index + 1); };

    blurb.setFont (Font (11.0f));
    blurb.setColour (Label::textColourId, ocui::textDim);
    blurb.setJustificationType (Justification::centredLeft);
    blurb.setMinimumHorizontalScale (0.75f);
    blurb.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (blurb);

    for (auto* s : { &mix, &level })
    {
        s->setSliderStyle (Slider::LinearHorizontal);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s->setPopupDisplayEnabled (true, false, nullptr);
        addAndMakeVisible (*s);
    }
    mix.setDoubleClickReturnValue (true, 100.0);
    level.setDoubleClickReturnValue (true, 0.0);
    for (auto* l : { &mixLabel, &levelLabel })
    {
        l->setFont (Font (11.0f, Font::bold));
        l->setColour (Label::textColourId, ocui::textDim);
        l->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*l);
    }
    mix.onValueChange = [this] { mixLabel.setText ("MIX " + mix.getTextFromValue (mix.getValue()), dontSendNotification); };
    level.onValueChange = [this] { levelLabel.setText ("LEVEL " + level.getTextFromValue (level.getValue()), dontSendNotification); };

    auto& st = proc.apvts;
    for (int k = 0; k < oc::kParamsPerSlot; ++k)
    {
        addAndMakeVisible (knobs[k]);
        knobAtt[k] = std::make_unique<SA> (st, OmniChainProcessor::pid (index, "p" + String (k + 1)), knobs[k].slider);
    }
    mixAtt = std::make_unique<SA> (st, OmniChainProcessor::pid (index, "mix"), mix);
    levelAtt = std::make_unique<SA> (st, OmniChainProcessor::pid (index, "lvl"), level);
    powerAtt = std::make_unique<BA> (st, OmniChainProcessor::pid (index, "byp"), power);
    mix.onValueChange(); level.onValueChange();
    refresh();
}

SlotCard::~SlotCard() = default;

void SlotCard::rebuildForType (int type)
{
    shownType = type;
    const auto& info = oc::getInfo (type);
    accent = Colour (0xff000000 | oc::categoryColour (info.category));
    typeBox.setSelectedId (type + 1, dontSendNotification);
    blurb.setText (info.blurb, dontSendNotification);
    const bool empty = type == oc::FxNone;
    for (int k = 0; k < oc::kParamsPerSlot; ++k)
    {
        const auto& pi = info.params[k];
        auto& kn = knobs[k];
        kn.setVisible (pi.name != nullptr);
        if (pi.name == nullptr) continue;
        kn.name.setText (String (pi.name).toUpperCase(), dontSendNotification);
        kn.setAccent (accent);
        kn.slider.getProperties().set ("bipolar", pi.min < 0.0f && pi.max > 0.0f && std::abs (pi.min + pi.max) < 1.0e-3f);
        kn.slider.setDoubleClickReturnValue (true, oc::paramToNorm (pi, pi.def));
        kn.slider.setTooltip (String (pi.name) + "  (double-click to reset)");
        kn.value.setText (kn.slider.getTextFromValue (kn.slider.getValue()), dontSendNotification);
        kn.slider.repaint();
    }
    mix.setDoubleClickReturnValue (true, info.defaultMix * 100.0);
    for (auto* s : { &mix, &level }) { s->setVisible (! empty); s->setColour (Slider::trackColourId, accent); }
    mixLabel.setVisible (! empty); levelLabel.setVisible (! empty);
    power.setColour (TextButton::buttonOnColourId, accent);
    power.setVisible (! empty);
    repaint();
}

void SlotCard::refresh()
{
    const int t = proc.getSlotType (index);
    if (t != shownType) rebuildForType (t);
    // dim the free-running rate/time knob while tempo sync is active
    int syncIdx = -1;
    if (t == oc::FxDelay) syncIdx = 1; else if (t == oc::FxFlanger) syncIdx = 5; else if (t == oc::FxTremolo) syncIdx = 4;
    const bool synced = syncIdx >= 0 && knobs[syncIdx].slider.getValue() > 0.001;
    const float alpha = synced ? 0.35f : 1.0f;
    if (std::abs (knobs[0].getAlpha() - alpha) > 0.01f) knobs[0].setAlpha (alpha);

    const float m = proc.engine.getSlotMeter (index);
    if (std::abs (m - meter) > 0.01f) { meter = m; repaint (headerArea().withTrimmedTop (31)); }
}

void SlotCard::resized()
{
    auto r = getLocalBounds().reduced (6, 0);
    auto h = r.removeFromTop (34).reduced (0, 5);
    h.removeFromLeft (26);
    power.setBounds (h.removeFromRight (26));
    h.removeFromRight (4);
    right.setBounds (h.removeFromRight (20));
    left.setBounds (h.removeFromRight (20));
    h.removeFromRight (4);
    typeBox.setBounds (h);
    blurb.setBounds (r.removeFromTop (18).reduced (2, 0));
    auto bottom = r.removeFromBottom (36).reduced (2, 2);
    auto bl = bottom.removeFromLeft (bottom.getWidth() / 2).reduced (2, 0);
    auto br = bottom.reduced (2, 0);
    mixLabel.setBounds (bl.removeFromTop (14)); mix.setBounds (bl);
    levelLabel.setBounds (br.removeFromTop (14)); level.setBounds (br);
    r.removeFromTop (2);
    const int cw = r.getWidth() / 3, ch = r.getHeight() / 2;
    for (int k = 0; k < oc::kParamsPerSlot; ++k)
        knobs[k].setBounds (r.getX() + (k % 3) * cw, r.getY() + (k / 3) * ch, cw, ch);
}

void SlotCard::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool empty = shownType == oc::FxNone;
    g.setColour (empty ? ocui::card.darker (0.25f) : ocui::card);
    g.fillRoundedRectangle (r, 10.0f);

    auto head = r.removeFromTop (34.0f);
    Path hp; hp.addRoundedRectangle (head.getX(), head.getY(), head.getWidth(), head.getHeight(), 10.0f, 10.0f, true, true, false, false);
    g.setColour (accent.withAlpha (empty ? 0.06f : 0.16f));
    g.fillPath (hp);

    // meter strip under the header
    g.setColour (ocui::track.withAlpha (0.6f));
    g.fillRect (head.getX() + 8, head.getBottom() - 2.0f, head.getWidth() - 16, 2.0f);
    if (! empty && meter > 0.0f)
    {
        g.setColour (accent);
        g.fillRect (head.getX() + 8, head.getBottom() - 2.0f, (head.getWidth() - 16) * jlimit (0.0f, 1.0f, meter), 2.0f);
    }

    // slot number badge
    auto badge = Rectangle<float> (head.getX() + 7, head.getCentreY() - 10, 20, 20);
    g.setColour (empty ? ocui::track : accent);
    g.fillEllipse (badge);
    g.setColour (empty ? ocui::text : Colours::black.withAlpha (0.8f));
    g.setFont (Font (12.0f, Font::bold));
    g.drawText (String (index + 1), badge, Justification::centred);

    if (empty)
    {
        g.setColour (ocui::textDim.withAlpha (0.7f));
        g.setFont (Font (14.0f));
        g.drawText ("+  pick an effect above", getLocalBounds().withTrimmedTop (40), Justification::centred);
        g.setFont (Font (11.0f));
        g.drawText ("or drop a slot here", getLocalBounds().withTrimmedTop (80), Justification::centred);
    }

    g.setColour (dragOver ? Colours::white.withAlpha (0.8f) : accent.withAlpha (empty ? 0.15f : 0.45f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 10.0f, dragOver ? 2.0f : 1.0f);
}

void SlotCard::paintOverChildren (Graphics& g)
{
    if (shownType != oc::FxNone && power.getToggleState())
    {
        g.setColour (ocui::bg0.withAlpha (0.62f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().withTrimmedTop (36).reduced (2.0f), 8.0f);
        auto pill = getLocalBounds().withTrimmedTop (36).toFloat().withSizeKeepingCentre (110.0f, 26.0f);
        g.setColour (ocui::bg1);
        g.fillRoundedRectangle (pill, 13.0f);
        g.setColour (accent.withAlpha (0.7f));
        g.drawRoundedRectangle (pill, 13.0f, 1.0f);
        g.setColour (ocui::text);
        g.setFont (Font (12.5f, Font::bold));
        g.drawText ("BYPASSED", pill, Justification::centred);
    }
}

void SlotCard::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu() && headerArea().contains (e.getPosition())) showMenu();
}

void SlotCard::mouseDrag (const MouseEvent& e)
{
    if (! headerArea().contains (e.getMouseDownPosition()) || e.getDistanceFromDragStart() < 5) return;
    if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
        if (! c->isDragAndDropActive()) c->startDragging ("slot:" + String (index), this);
}

bool SlotCard::isInterestedInDragSource (const SourceDetails& d) { return d.description.toString().startsWith ("slot:"); }

void SlotCard::itemDropped (const SourceDetails& d)
{
    dragOver = false; repaint();
    const int from = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
    proc.moveSlot (from, index);
}

void SlotCard::showMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Slot " + String (index + 1));
    m.addItem (1, "Reset parameters", shownType != oc::FxNone);
    m.addItem (2, "Duplicate into next empty slot", shownType != oc::FxNone);
    m.addItem (3, "Move to start of chain", index > 0);
    m.addItem (4, "Move to end of chain", index < oc::kNumSlots - 1);
    m.addSeparator();
    m.addItem (5, "Clear slot", shownType != oc::FxNone);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        switch (r)
        {
            case 1: proc.resetSlotParams (index); break;
            case 2:
                for (int s = 0; s < oc::kNumSlots; ++s)
                    if (s != index && proc.getSlotType (s) == oc::FxNone) { proc.copySlot (index, s); break; }
                break;
            case 3: proc.moveSlot (index, 0); break;
            case 4: proc.moveSlot (index, oc::kNumSlots - 1); break;
            case 5: proc.setSlotType (index, oc::FxNone, true); break;
            default: break;
        }
    });
}

// =====================================================================================
OmniChainEditor::OmniChainEditor (OmniChainProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    for (int i = 0; i < oc::kNumSlots; ++i)
    {
        cards[i] = std::make_unique<SlotCard> (proc, *this, i);
        addAndMakeVisible (*cards[i]);
    }

    rebuildPresetMenu();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < 1000) { proc.setCurrentProgram (id - 1); proc.updateHostDisplay(); }
        else if (isPositiveAndBelow (id - 1001, userPresets.size())) proc.loadPresetFile (userPresets[id - 1001]);
        shownPreset = proc.currentPresetName;
    };
    addAndMakeVisible (presetBox);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { savePreset(); };
    loadBtn.onClick = [this] { loadPreset(); };
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &loadBtn }) addAndMakeVisible (*b);

    const Colour gAcc (0xff7c9cff);
    struct G { ocui::Knob* k; const char* id; const char* name; bool bip; double def; };
    for (auto g : { G { &inKnob, "in", "INPUT", true, 0.0 }, G { &mixKnob, "mix", "MIX", false, 100.0 }, G { &outKnob, "out", "OUTPUT", true, 0.0 } })
    {
        g.k->name.setText (g.name, dontSendNotification);
        g.k->setAccent (gAcc);
        g.k->slider.getProperties().set ("bipolar", g.bip);
        g.k->slider.setDoubleClickReturnValue (true, g.def);
        addAndMakeVisible (*g.k);
    }
    inAtt  = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "in", inKnob.slider);
    mixAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "mix", mixKnob.slider);
    outAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "out", outKnob.slider);
    for (auto* k : { &inKnob, &mixKnob, &outKnob }) k->slider.onValueChange();

    setSize (1220, 790);
    startTimerHz (30);
}

OmniChainEditor::~OmniChainEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void OmniChainEditor::rebuildPresetMenu()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    const auto& f = getFactoryPresets();
    for (int i = 0; i < (int) f.size(); ++i) presetBox.addItem (f[(size_t) i].name, i + 1);
    userPresets = OmniChainProcessor::getUserPresetFolder().findChildFiles (File::findFiles, false, "*.ocpreset");
    userPresets.sort();
    if (! userPresets.isEmpty())
    {
        presetBox.addSectionHeading ("User  (Documents/OmniChain Presets)");
        for (int i = 0; i < userPresets.size(); ++i) presetBox.addItem (userPresets[i].getFileNameWithoutExtension(), 1001 + i);
    }
    presetBox.setText (proc.currentPresetName, dontSendNotification);
    shownPreset = proc.currentPresetName;
}

void OmniChainEditor::stepPreset (int delta)
{
    const int nF = (int) getFactoryPresets().size(), total = nF + userPresets.size();
    int cur = -1;
    for (int i = 0; i < total; ++i)
    {
        const String n = i < nF ? String (getFactoryPresets()[(size_t) i].name) : userPresets[i - nF].getFileNameWithoutExtension();
        if (n == proc.currentPresetName) { cur = i; break; }
    }
    const int next = (cur < 0 ? 0 : (cur + delta + total) % total);
    presetBox.setSelectedId (next < nF ? next + 1 : 1001 + (next - nF), sendNotificationSync);
}

void OmniChainEditor::savePreset()
{
    auto folder = OmniChainProcessor::getUserPresetFolder();
    chooser = std::make_unique<FileChooser> ("Save OmniChain preset", folder.getChildFile (File::createLegalFileName (proc.currentPresetName) + ".ocpreset"), "*.ocpreset");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [this] (const FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == File()) return;
                              proc.savePresetFile (f.withFileExtension ("ocpreset"));
                              rebuildPresetMenu();
                          });
}

void OmniChainEditor::loadPreset()
{
    chooser = std::make_unique<FileChooser> ("Load OmniChain preset", OmniChainProcessor::getUserPresetFolder(), "*.ocpreset");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f.existsAsFile()) { proc.loadPresetFile (f); rebuildPresetMenu(); }
                          });
}

void OmniChainEditor::timerCallback()
{
    for (auto& c : cards) c->refresh();
    const float decay = 0.86f;
    const float i = proc.engine.takeInPeak(), o = proc.engine.takeOutPeak();
    inLvl = jmax (i, inLvl * decay); outLvl = jmax (o, outLvl * decay);
    repaint (getWidth() - 46, 10, 40, 60);
    if (proc.currentPresetName != shownPreset) { shownPreset = proc.currentPresetName; presetBox.setText (shownPreset, dontSendNotification); }
}

void OmniChainEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (ocui::bg1, 0, 0, ocui::bg0, 0, (float) getHeight(), false));
    g.fillAll();

    // title
    g.setColour (ocui::text);
    g.setFont (Font (28.0f, Font::bold));
    g.drawText ("OMNICHAIN", 20, 14, 260, 32, Justification::centredLeft);
    g.setFont (Font (12.0f));
    g.setColour (ocui::textDim);
    g.drawText ("modular multi-fx  |  29 effects  |  0 samples latency", 22, 46, 360, 16, Justification::centredLeft);

    // meters
    auto drawMeter = [&] (int x, float lvl, const char* lbl)
    {
        const Rectangle<float> r ((float) x, 14.0f, 9.0f, 46.0f);
        g.setColour (ocui::track); g.fillRoundedRectangle (r, 3.0f);
        const float db = Decibels::gainToDecibels (lvl, -60.0f);
        const float frac = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        auto fill = r.withTop (r.getBottom() - r.getHeight() * frac);
        g.setColour (db > -0.5f ? Colour (0xffff5252) : (db > -9.0f ? Colour (0xffffc94d) : Colour (0xff4fd1a0)));
        g.fillRoundedRectangle (fill, 3.0f);
        g.setColour (ocui::textDim); g.setFont (Font (9.0f, Font::bold));
        g.drawText (lbl, x - 4, 61, 18, 10, Justification::centred);
    };
    drawMeter (getWidth() - 42, inLvl, "IN");
    drawMeter (getWidth() - 26, outLvl, "OUT");

    // signal-flow arrows between cards
    g.setColour (ocui::textDim.withAlpha (0.55f));
    for (int i = 0; i < oc::kNumSlots - 1; ++i)
    {
        const auto a = cards[i]->getBounds(), b = cards[i + 1]->getBounds();
        if (a.getY() == b.getY())
        {
            const float x = (float) (a.getRight() + b.getX()) * 0.5f, y = (float) a.getCentreY();
            Path p; p.addTriangle (x - 4, y - 6, x - 4, y + 6, x + 5, y);
            g.fillPath (p);
        }
        else
        {
            const float y = (float) (a.getBottom() + b.getY()) * 0.5f;
            Path p;
            p.startNewSubPath ((float) a.getCentreX(), (float) a.getBottom() + 2);
            p.lineTo ((float) a.getCentreX(), y);
            p.lineTo ((float) b.getCentreX(), y);
            p.lineTo ((float) b.getCentreX(), (float) b.getY() - 4);
            g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
            Path t; const float bx = (float) b.getCentreX(), by = (float) b.getY() - 1;
            t.addTriangle (bx - 5, by - 7, bx + 5, by - 7, bx, by);
            g.fillPath (t);
        }
    }

    g.setColour (ocui::textDim.withAlpha (0.8f));
    g.setFont (Font (11.5f));
    g.drawText ("Drag a slot header to reorder  |  right-click a header for more  |  double-click a knob to reset  |  hold Ctrl/Cmd for fine control",
                0, getHeight() - 22, getWidth(), 16, Justification::centred);
}

void OmniChainEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (80).reduced (16, 8);
    header.removeFromLeft (360);
    header.removeFromRight (40);
    auto knobsArea = header.removeFromRight (3 * 70);
    inKnob.setBounds (knobsArea.removeFromLeft (70));
    mixKnob.setBounds (knobsArea.removeFromLeft (70));
    outKnob.setBounds (knobsArea.removeFromLeft (70));
    auto pr = header.withSizeKeepingCentre (jmin (header.getWidth() - 20, 470), 30);
    loadBtn.setBounds (pr.removeFromRight (56)); pr.removeFromRight (6);
    saveBtn.setBounds (pr.removeFromRight (56)); pr.removeFromRight (6);
    nextBtn.setBounds (pr.removeFromRight (28)); pr.removeFromRight (4);
    prevBtn.setBounds (pr.removeFromLeft (28));  pr.removeFromLeft (4);
    presetBox.setBounds (pr);

    area.removeFromBottom (26);
    area.reduce (16, 4);
    const int gapX = 22, gapY = 26;
    const int cw = (area.getWidth() - 3 * gapX) / 4, ch = (area.getHeight() - gapY) / 2;
    for (int i = 0; i < oc::kNumSlots; ++i)
    {
        const int row = i / 4, col = i % 4;
        cards[i]->setBounds (area.getX() + col * (cw + gapX), area.getY() + row * (ch + gapY), cw, ch);
    }
}
