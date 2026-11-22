#include "PluginEditor.h"
#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

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
    const bool on = b.getToggleState() && b.getClickingTogglesState();
    g.setColour ((on ? b.findColour (TextButton::buttonOnColourId) : knobBody).brighter (down ? 0.3f : (over ? 0.15f : 0.0f)));
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
    name.setMinimumHorizontalScale (0.6f); value.setMinimumHorizontalScale (0.6f);
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
SlotCard::SlotCard (OmniChainProcessor& p, OmniChainPanel& e, int i) : proc (p), editor (e), index (i)
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
        if (id > 0 && id - 1 != proc.getSlotType (index))
        {
            proc.setSlotType (index, id - 1, true);
            if (oc::typeUsesResource (id - 1)) showLoadMenu();
        }
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

    fileLabel.setFont (Font (11.5f, Font::bold));
    fileLabel.setJustificationType (Justification::centredLeft);
    fileLabel.setMinimumHorizontalScale (0.6f);
    fileLabel.setInterceptsMouseClicks (false, false);
    addChildComponent (fileLabel);
    loadBtn.setTooltip ("Pick from the factory library or load your own file (you can also drag a file onto this slot)");
    filePrev.setTooltip ("Previous file in the same folder");
    fileNext.setTooltip ("Next file in the same folder");
    loadBtn.onClick = [this] { showLoadMenu(); };
    filePrev.onClick = [this] { proc.stepSlotFile (index, -1); };
    fileNext.onClick = [this] { proc.stepSlotFile (index, 1); };
    for (auto* b : { &loadBtn, &filePrev, &fileNext }) addChildComponent (*b);
    looperPanel = std::make_unique<LooperPanel> (proc, index);
    addChildComponent (*looperPanel);
    delayBar = std::make_unique<DelayBar> (proc, index);
    addChildComponent (*delayBar);

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
    const bool usesFile = oc::typeUsesResource (type);
    blurb.setVisible (! usesFile && type != oc::FxDelay);
    delayBar->setVisible (type == oc::FxDelay);
    for (Component* c : { (Component*) &fileLabel, (Component*) &loadBtn, (Component*) &filePrev, (Component*) &fileNext }) c->setVisible (usesFile);
    shownFileText = "\x01";
    looperPanel->setVisible (type == oc::FxLooper);
    for (int k = 0; k < oc::kParamsPerSlot; ++k)
    {
        const auto& pi = info.params[k];
        auto& kn = knobs[k];
        kn.setVisible (pi.name != nullptr && ! (type == oc::FxLooper && (k == 0 || k == 6)));
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
    resized();
    repaint();
}

void SlotCard::refresh()
{
    const int t = proc.getSlotType (index);
    if (t != shownType) rebuildForType (t);
    if (oc::typeUsesResource (t))
    {
        int sev = 0;
        const auto txt = proc.getSlotFileText (index, sev);
        if (txt != shownFileText)
        {
            shownFileText = txt;
            fileLabel.setText (txt, dontSendNotification);
            fileLabel.setTooltip (proc.getSlotFile (index).getFullPathName());
            fileLabel.setColour (Label::textColourId, sev == 2 ? Colour (0xffff6b6b) : (sev == 1 ? Colour (0xffffc94d) : ocui::text));
        }
    }

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
    {
        auto row = r.removeFromTop (22).reduced (2, 1);
        blurb.setBounds (row);
        delayBar->setBounds (row);
        fileNext.setBounds (row.removeFromRight (20));
        filePrev.setBounds (row.removeFromRight (20));
        row.removeFromRight (3);
        loadBtn.setBounds (row.removeFromRight (46));
        fileLabel.setBounds (row.withTrimmedRight (4));
    }
    auto bottom = r.removeFromBottom (36).reduced (2, 2);
    auto bl = bottom.removeFromLeft (bottom.getWidth() / 2).reduced (2, 0);
    auto br = bottom.reduced (2, 0);
    mixLabel.setBounds (bl.removeFromTop (14)); mix.setBounds (bl);
    levelLabel.setBounds (br.removeFromTop (14)); level.setBounds (br);
    r.removeFromTop (2);
    if (shownType == oc::FxLooper)
    {
        // looper: transport panel on top, its six settings in one row underneath (Mode / Footswitch stay automatable)
        looperPanel->setBounds (r.removeFromTop (92).reduced (0, 2));
        const int order[6] = { 1, 2, 3, 4, 5, 7 };
        const int cw6 = r.getWidth() / 6;
        for (int j = 0; j < 6; ++j) knobs[order[j]].setBounds (r.getX() + j * cw6, r.getY() + 4, cw6, r.getHeight() - 4);
        return;
    }
    // effects with 7 or 8 knobs use a 4-column grid; the rest keep the roomier 3 columns
    const auto& inf = oc::getInfo (shownType < 0 ? 0 : shownType);
    const int cols = (inf.params[6].name != nullptr || inf.params[7].name != nullptr) ? 4 : 3;
    const int cw = r.getWidth() / cols, ch = r.getHeight() / 2;
    for (int k = 0; k < oc::kParamsPerSlot; ++k)
        knobs[k].setBounds (r.getX() + (k % cols) * cw, r.getY() + (k / cols) * ch, cw, ch);
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
        g.drawText ("or drop a slot, a .wav IR or a .nam model here", getLocalBounds().withTrimmedTop (80), Justification::centred);
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

bool SlotCard::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (OmniChainProcessor::isIrFile (File (f)) || OmniChainProcessor::isNamFile (File (f)) || File (f).hasFileExtension ("ocslot;zip")) return true;
    return false;
}

void SlotCard::filesDropped (const StringArray& files, int, int)
{
    dragOver = false; repaint();
    for (auto& f : files)
    {
        if (File (f).hasFileExtension ("ocslot")) { proc.loadSlotPatch (index, File (f)); break; }
        if (File (f).hasFileExtension ("zip")) { importFiles ({ File (f) }); break; }
        if (OmniChainProcessor::isIrFile (File (f)) || OmniChainProcessor::isNamFile (File (f))) { proc.loadSlotFile (index, File (f)); break; }
    }
}

void SlotCard::addLibraryFolder (PopupMenu& m, const File& dir, bool nam, const File& current)
{
    auto dirs = dir.findChildFiles (File::findDirectories, false); dirs.sort();
    for (auto& d : dirs)
    {
        PopupMenu sub;
        addLibraryFolder (sub, d, nam, current);
        if (sub.getNumItems() > 0) m.addSubMenu (d.getFileName(), sub, true, nullptr, current.isAChildOf (d));
    }
    auto files = dir.findChildFiles (File::findFiles, false, nam ? "*.nam" : "*.wav"); files.sort();
    for (auto& f : files)
    {
        libraryFiles.add (f);
        m.addItem (100 + libraryFiles.size() - 1, f.getFileNameWithoutExtension(), true, f == current);
    }
}

void SlotCard::showLoadMenu()
{
    const bool nam = proc.getSlotType (index) == oc::FxNam;
    PopupMenu m;
    m.addItem (1, "Browse for your own file...");
    m.addItem (3, "Import downloads (.nam / .wav / .zip, e.g. from TONE3000)...");
    m.addItem (4, "Find more on TONE3000 (opens your browser)");
    const auto root = OmniChainProcessor::getLibraryFolder().getChildFile (nam ? "NAM" : "IR");
    const auto mine = OmniChainProcessor::getContentFolder (nam);
    libraryFiles.clear();
    m.addSeparator();
    if (root.isDirectory())
    {
        m.addSectionHeader (nam ? "Factory library: amp and pedal captures" : "Factory library: impulse responses");
        addLibraryFolder (m, root, nam, proc.getSlotFile (index));
    }
    else m.addItem (2, "Get the free factory library (opens the download page)");
    if (mine.getNumberOfChildFiles (File::findFiles, nam ? "*.nam" : "*.wav;*.aif;*.aiff;*.flac") > 0
        || mine.getNumberOfChildFiles (File::findDirectories) > 0)
    {
        m.addSectionHeader (nam ? "Your captures" : "Your IRs");
        addLibraryFolder (m, mine, nam, proc.getSlotFile (index));
    }
    SafePointer<SlotCard> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&loadBtn), [safe, nam] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1) safe->openFileChooser();
        else if (r == 2) URL ("https://davepomerlo.github.io/omnichain/#library").launchInDefaultBrowser();
        else if (r == 3) safe->openImportChooser();
        else if (r == 4) URL (nam ? "https://www.tone3000.com/neural-amp-modeler" : "https://www.tone3000.com").launchInDefaultBrowser();
        else if (isPositiveAndBelow (r - 100, safe->libraryFiles.size())) safe->proc.loadSlotFile (safe->index, safe->libraryFiles[r - 100]);
    });
}

void SlotCard::importFiles (const Array<File>& files)
{
    const bool nam = proc.getSlotType (index) == oc::FxNam || (proc.getSlotType (index) != oc::FxCabIR
                     && std::any_of (files.begin(), files.end(), [] (const File& f) { return OmniChainProcessor::isNamFile (f) || f.hasFileExtension ("zip"); }));
    auto added = OmniChainProcessor::importToneFiles (files, nam);
    if (added.isEmpty() && ! nam) added = OmniChainProcessor::importToneFiles (files, true);
    if (added.isEmpty())
    {
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Nothing to import",
                                          "No .nam captures or .wav impulse responses were found in what you picked.", "OK", this);
        return;
    }
    proc.loadSlotFile (index, added.getFirst());
    AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Imported " + String (added.size()) + (added.size() == 1 ? " file" : " files"),
                                      "Loaded \"" + added.getFirst().getFileName() + "\". The rest are in Load > "
                                      + (OmniChainProcessor::isNamFile (added.getFirst()) ? "Your captures." : "Your IRs.")
                                      + " Tip: lite / feather captures use a fraction of the CPU of standard ones.", "OK", this);
}

void SlotCard::openImportChooser()
{
    chooser = std::make_unique<FileChooser> ("Import captures or impulse responses", File::getSpecialLocation (File::userHomeDirectory).getChildFile ("Downloads"),
                                             "*.nam;*.wav;*.aif;*.aiff;*.flac;*.zip");
    SafePointer<SlotCard> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                          [safe] (const FileChooser& fc)
    {
        if (safe != nullptr && ! fc.getResults().isEmpty()) safe->importFiles (fc.getResults());
    });
}

void SlotCard::openFileChooser()
{
    const bool nam = proc.getSlotType (index) == oc::FxNam;
    File start = proc.getSlotFile (index).getParentDirectory();
    if (! start.isDirectory()) start = proc.lastFileFolder;
    if (! start.isDirectory()) start = OmniChainProcessor::getContentFolder (nam);
    chooser = std::make_unique<FileChooser> (nam ? "Load a NAM model" : "Load a cabinet impulse response", start,
                                             nam ? "*.nam" : "*.wav;*.aif;*.aiff;*.flac");
    SafePointer<SlotCard> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [safe] (const FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f.existsAsFile()) safe->proc.loadSlotFile (safe->index, f);
    });
}

void SlotCard::saveSlotPatch()
{
    const int type = proc.getSlotType (index);
    auto* aw = new AlertWindow ("Save effect patch", String ("Save these ") + oc::getInfo (type).name + " settings so you can load them into any slot later.",
                                MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", "", "Name");
    aw->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<SlotCard> safe (this);
    aw->enterModalState (true, ModalCallbackFunction::create ([safe, aw, type] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto name = File::createLegalFileName (aw->getTextEditorContents ("name").trim());
        if (name.isEmpty()) return;
        safe->proc.saveSlotPatch (safe->index, OmniChainProcessor::getSlotPatchFolder (type).getChildFile (name + ".ocslot"));
    }), true);
}

void SlotCard::showMenu()
{
    PopupMenu m, patches;
    slotPatchFiles.clear();
    auto addFolder = [this] (PopupMenu& pm, const File& dir)
    {
        auto fs = dir.findChildFiles (File::findFiles, false, "*.ocslot"); fs.sort();
        for (auto& f : fs) { slotPatchFiles.add (f); pm.addItem (1000 + slotPatchFiles.size() - 1, f.getFileNameWithoutExtension()); }
    };
    if (shownType != oc::FxNone) addFolder (patches, OmniChainProcessor::getSlotPatchFolder (shownType));
    else
    {
        auto dirs = OmniChainProcessor::getSlotPatchRoot().findChildFiles (File::findDirectories, false); dirs.sort();
        for (auto& d : dirs) { PopupMenu sub; addFolder (sub, d); if (sub.getNumItems() > 0) patches.addSubMenu (d.getFileName(), sub); }
    }
    m.addSectionHeader ("Slot " + String (index + 1));
    m.addItem (6, "Save effect patch...", shownType != oc::FxNone);
    m.addSubMenu ("Load effect patch", patches, patches.getNumItems() > 0);
    m.addItem (7, "Open effect patches folder");
    m.addSeparator();
    m.addItem (1, "Reset parameters", shownType != oc::FxNone);
    m.addItem (2, "Duplicate into next empty slot", shownType != oc::FxNone);
    m.addItem (3, "Move to start of chain", index > 0);
    m.addItem (4, "Move to end of chain", index < oc::kNumSlots - 1);
    m.addSeparator();
    m.addItem (5, "Clear slot", shownType != oc::FxNone);
    SafePointer<SlotCard> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr) return;
        auto& proc = safe->proc; const int index = safe->index;
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
            case 6: safe->saveSlotPatch(); break;
            case 7:
            {
                auto d = safe->shownType != oc::FxNone ? OmniChainProcessor::getSlotPatchFolder (safe->shownType) : OmniChainProcessor::getSlotPatchRoot();
                d.createDirectory(); d.startAsProcess(); break;
            }
            default:
                if (r >= 1000 && isPositiveAndBelow (r - 1000, safe->slotPatchFiles.size())) proc.loadSlotPatch (index, safe->slotPatchFiles[r - 1000]);
                break;
        }
    });
}

// =====================================================================================
LooperPanel::LooperPanel (OmniChainProcessor& p, int s) : proc (p), slot (s)
{
    for (auto* b : { &mainBtn, &playBtn, &undoBtn, &clearBtn }) addAndMakeVisible (*b);
    mainBtn.onClick  = [this] { proc.engine.sendCommand (slot, 8); };
    playBtn.onClick  = [this] { proc.engine.sendCommand (slot, (state == 3 || state == 4) ? 2 : 3); };
    undoBtn.onClick  = [this] { proc.engine.sendCommand (slot, 4); };
    clearBtn.onClick = [this] { proc.engine.sendCommand (slot, 5); };
    mainBtn.setTooltip ("Record, then close the loop, then overdub. A MIDI footswitch on the Footswitch knob does the same: double-tap stops, hold 1 s undoes.");
    undoBtn.setTooltip ("Undo the last overdub (press again to redo)");
    clearBtn.setTooltip ("Erase the loop");
}

void LooperPanel::timerCallback()
{
    const int st = (int) proc.engine.getSlotStatus (slot, 0);
    const float ln = proc.engine.getSlotStatus (slot, 1), ps = proc.engine.getSlotStatus (slot, 2);
    const int fl = (int) proc.engine.getSlotStatus (slot, 3);
    if (st != state || fl != flags)
    {
        static const char* main[6] = { "Record", "Start now", "Close loop", "Overdub", "Stop dub", "Record new" };
        state = st; flags = fl;
        mainBtn.setButtonText (main[jlimit (0, 5, st)]);
        if (st == 5) mainBtn.setButtonText ("Overdub");
        mainBtn.setColour (TextButton::buttonColourId, st == 2 ? Colour (0xffb3262e) : (st == 4 ? Colour (0xffb36b1e) : (st == 1 ? Colour (0xff7a2a2a) : ocui::knobBody)));
        playBtn.setButtonText ((st == 3 || st == 4) ? "Stop" : "Play");
        playBtn.setEnabled (st >= 2);
        undoBtn.setButtonText ((fl & 2) ? "Redo" : "Undo");
        undoBtn.setEnabled ((fl & 1) != 0);
        clearBtn.setEnabled (st != 0);
    }
    if (std::abs (ln - len) > 0.01f || std::abs (ps - pos) > 0.002f) { len = ln; pos = ps; repaint(); }
}

void LooperPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ocui::bg0.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 8.0f);
    auto top = r.reduced (10, 6).removeFromTop (24);
    static const char* names[6] = { "Empty: press Record", "Waiting for your first note...", "Recording", "Playing", "Overdubbing", "Stopped" };
    const int st = jlimit (0, 5, state);
    const Colour stc = st == 2 ? Colour (0xffff5a5a) : (st == 4 ? Colour (0xffffa94d) : (st == 3 ? Colour (0xff4fd1a0) : ocui::textDim));
    if (st == 2 || st == 4) { g.setColour (stc); g.fillEllipse (top.getX(), top.getCentreY() - 5, 10, 10); }
    g.setColour (stc);
    g.setFont (Font (15.0f, Font::bold));
    g.drawText (names[st], top.withTrimmedLeft (st == 2 || st == 4 ? 16.0f : 0.0f), Justification::centredLeft);
    if (len > 0.0f)
    {
        const int t10 = roundToInt (len * 10.0f);
        g.setColour (ocui::text);
        g.drawText (String (t10 / 600) + ":" + String ((t10 / 10) % 60).paddedLeft ('0', 2) + "." + String (t10 % 10), top, Justification::centredRight);
    }
    auto bar = r.reduced (10, 0).withTrimmedTop (36).removeFromTop (8);
    g.setColour (ocui::track); g.fillRoundedRectangle (bar, 4.0f);
    if (st >= 2 && st <= 4) { g.setColour (stc); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, pos)), 4.0f); }
}

void LooperPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 0).withTrimmedTop (52).removeFromTop (30);
    mainBtn.setBounds (r.removeFromLeft (r.getWidth() * 38 / 100)); r.removeFromLeft (5);
    const int w = (r.getWidth() - 10) / 3;
    playBtn.setBounds (r.removeFromLeft (w)); r.removeFromLeft (5);
    undoBtn.setBounds (r.removeFromLeft (w)); r.removeFromLeft (5);
    clearBtn.setBounds (r);
}

// =====================================================================================
DelayBar::DelayBar (OmniChainProcessor& p, int s) : proc (p), slot (s)
{
    addAndMakeVisible (tapBtn);
    addAndMakeVisible (twistBtn);
    tapBtn.setTooltip ("Tap two or more times. Free time: sets the delay time. Synced: sets the tempo (or the time, when your DAW owns the tempo).");
    twistBtn.setTooltip ("Hold for a performance swell: repeats run away into oscillation, freeze (Reverse, Glitch, Ping-Pong) or bloom into a wash (Crystal, Shimmer, Echo + Verb, Warp). Release to let go.");
    tapBtn.onClick = [this] { proc.tapDelay (slot); flash = 1.0f; };
    twistBtn.onHold = [this] (bool down) { proc.setTwist (slot, down); };
}

void DelayBar::visibilityChanged()
{
    if (isVisible()) startTimerHz (15);
    else { stopTimer(); proc.setTwist (slot, false); }
}

void DelayBar::timerCallback()
{
    const float ms = proc.engine.getSlotStatus (slot, 0), tw = proc.engine.getSlotStatus (slot, 1);
    const auto& info = oc::getInfo (oc::FxDelay);
    String txt;
    if (auto* sp = proc.param (slot, "p2"))
    {
        const int si = roundToInt (oc::paramToReal (info.params[1], sp->getValue()));
        if (si > 0) txt << oc::kSyncNames[si - 1] << "  ";
    }
    txt << (ms >= 1000.0f ? String (ms / 1000.0f, 2) + " s" : String (roundToInt (ms)) + " ms");
    if (ms <= 0.0f) txt = {};
    if (flash > 0.0f) flash = std::max (0.0f, flash - 0.2f);
    if (txt != shownText || std::abs (tw - shownTw) > 0.02f || flash > 0.0f)
    {
        shownText = txt; shownTw = tw;
        twistBtn.setColour (TextButton::buttonColourId, ocui::knobBody.interpolatedWith (Colour (0xffb36b1e), jlimit (0.0f, 1.0f, tw)));
        tapBtn.setColour (TextButton::buttonColourId, ocui::knobBody.interpolatedWith (Colour (0xff4f7bd1), flash));
        repaint();
    }
}

void DelayBar::paint (Graphics& g)
{
    g.setColour (ocui::text);
    g.setFont (Font (12.5f, Font::bold));
    g.drawText (shownText, getLocalBounds().withTrimmedLeft (112).reduced (2, 0), Justification::centredRight);
}

void DelayBar::resized()
{
    auto r = getLocalBounds();
    tapBtn.setBounds (r.removeFromLeft (48));
    r.removeFromLeft (6);
    twistBtn.setBounds (r.removeFromLeft (56));
}

// =====================================================================================
TunerOverlay::TunerOverlay (OmniChainProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (true, false);
}

void TunerOverlay::timerCallback()
{
    proc.copyTunerInput (scratch.data(), (int) scratch.size());
    const auto r = yin.detect (scratch.data(), (int) scratch.size(), proc.getSampleRate());
    if (r.hz > 0.0f && r.clarity > 0.6f) { shown = r; holdFrames = 12; }
    else if (holdFrames > 0) --holdFrames;
    else shown = {};
    needle += ((shown.hz > 0 ? shown.cents : 0.0f) - needle) * 0.45f;
    repaint();
}

void TunerOverlay::paint (Graphics& g)
{
    g.setColour (ocui::bg0);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 14.0f);
    g.setColour (ocui::track);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 14.0f, 1.0f);
    auto r = getLocalBounds().reduced (30);
    const bool has = shown.hz > 0.0f;
    const bool inTune = has && std::abs (shown.cents) < 3.0f;
    const Colour good (0xff4fd1a0), off (0xffffc94d);

    g.setColour (has ? (inTune ? good : ocui::text) : ocui::textDim);
    g.setFont (Font (150.0f, Font::bold));
    auto noteArea = r.removeFromTop (r.getHeight() / 2);
    g.drawText (has ? String (oc::YinTuner::noteName (shown.midi)) : String ("--"), noteArea, Justification::centredBottom);
    if (has)
    {
        g.setFont (Font (28.0f, Font::bold));
        g.setColour (ocui::textDim);
        g.drawText (String (shown.midi / 12 - 1), noteArea.withTrimmedLeft (noteArea.getWidth() / 2 + 70).withTrimmedBottom (20), Justification::bottomLeft);
    }

    // cents scale
    auto scale = r.removeFromTop (70).reduced (jmax (40, r.getWidth() / 6), 10).toFloat();
    g.setColour (ocui::track);
    g.fillRoundedRectangle (scale.withSizeKeepingCentre (scale.getWidth(), 6.0f), 3.0f);
    for (int c = -50; c <= 50; c += 10)
    {
        const float x = scale.getX() + scale.getWidth() * (float) (c + 50) / 100.0f;
        const float h = c == 0 ? 34.0f : (c % 50 == 0 ? 22.0f : 14.0f);
        g.setColour (c == 0 ? good : ocui::textDim);
        g.fillRect (x - (c == 0 ? 1.5f : 0.75f), scale.getCentreY() - h * 0.5f, c == 0 ? 3.0f : 1.5f, h);
    }
    if (has)
    {
        const float x = scale.getX() + scale.getWidth() * (jlimit (-50.0f, 50.0f, needle) + 50.0f) / 100.0f;
        g.setColour (inTune ? good : off);
        g.fillRoundedRectangle (x - 4.0f, scale.getCentreY() - 24.0f, 8.0f, 48.0f, 4.0f);
    }
    g.setFont (Font (18.0f));
    g.setColour (ocui::textDim);
    String info = has ? (String (shown.cents >= 0 ? "+" : "") + String (shown.cents, 1) + " cents   |   " + String (shown.hz, 2) + " Hz")
                      : String ("Play a single open string");
    g.drawText (info, r.removeFromTop (40), Justification::centred);
    g.setFont (Font (13.0f));
    g.drawText ("Output is muted while the tuner is on. Click anywhere here or press Tuner again to close.", r.removeFromTop (30), Justification::centred);
}

// =====================================================================================
StandaloneBar::StandaloneBar (OmniChainProcessor& p) : proc (p)
{
    for (auto* b : { &audioBtn, &inputBtn, &tapBtn, &metroBtn, &tunerBtn, &recBtn, &folderBtn }) addAndMakeVisible (*b);
    for (auto* b : { &inputBtn, &metroBtn, &tunerBtn, &recBtn })
    {
        b->setClickingTogglesState (true);
        b->setColour (TextButton::buttonOnColourId, Colour (0xff35518a));
    }
    recBtn.setColour (TextButton::buttonOnColourId, Colour (0xffb3262e));
    inputBtn.setColour (TextButton::buttonOnColourId, Colour (0xff1f7a5a));
    audioBtn.setTooltip ("Choose your audio interface, inputs, sample rate and buffer size (smaller buffer = lower latency)");
    inputBtn.setTooltip ("Turn your guitar input on or off. It starts muted to avoid feedback through laptop speakers.");
    tapBtn.setTooltip ("Tap four times in time to set the tempo for synced delays, tremolos, glitches and arps");
    recBtn.setTooltip ("Record what you play (with all effects) to a WAV file in Documents/OmniChain Recordings");

    audioBtn.onClick = []
    {
       #if JucePlugin_Build_Standalone
        if (auto* h = StandalonePluginHolder::getInstance()) h->showAudioSettingsDialog();
       #endif
    };
    inputBtn.onClick = [this]
    {
       #if JucePlugin_Build_Standalone
        if (auto* h = StandalonePluginHolder::getInstance()) h->getMuteInputValue() = ! inputBtn.getToggleState();
       #endif
    };
    tapBtn.onClick = [this] { tap(); };
    metroBtn.onClick = [this] { proc.metronomeOn = metroBtn.getToggleState(); };
    tunerBtn.onClick = [this] { proc.tunerOn = tunerBtn.getToggleState(); };
    recBtn.onClick = [this]
    {
        if (recBtn.getToggleState()) { if (! proc.startRecording()) recBtn.setToggleState (false, dontSendNotification); }
        else proc.stopRecording();
    };
    folderBtn.onClick = [] { OmniChainProcessor::getRecordingsFolder().startAsProcess(); };

    bpm.setSliderStyle (Slider::LinearHorizontal);
    bpm.setTextBoxStyle (Slider::TextBoxLeft, false, 64, 22);
    bpm.setRange (30.0, 300.0, 1.0);
    bpm.setTextValueSuffix (" BPM");
    bpm.setValue (proc.hostlessBpm.load(), dontSendNotification);
    bpm.setDoubleClickReturnValue (true, 120.0);
    bpm.setColour (Slider::trackColourId, Colour (0xff7c9cff));
    bpm.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    bpm.onValueChange = [this] { proc.hostlessBpm = (float) bpm.getValue(); };
    bpm.setTooltip ("Song tempo for tempo-synced effects (no DAW = no tempo, so set it here)");
    addAndMakeVisible (bpm);

    metroVol.setSliderStyle (Slider::LinearHorizontal);
    metroVol.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    metroVol.setRange (0.0, 1.0);
    metroVol.setValue (proc.metronomeVol.load(), dontSendNotification);
    metroVol.setColour (Slider::trackColourId, Colour (0xff7c9cff));
    metroVol.onValueChange = [this] { proc.metronomeVol = (float) metroVol.getValue(); };
    metroVol.setTooltip ("Metronome volume");
    addAndMakeVisible (metroVol);

    recTime.setFont (Font (13.0f, Font::bold));
    recTime.setColour (Label::textColourId, Colour (0xffff6b6b));
    addAndMakeVisible (recTime);
    startTimerHz (10);
}

void StandaloneBar::tap()
{
    const double now = Time::getMillisecondCounterHiRes();
    if (! taps.isEmpty() && now - taps.getLast() > 2000.0) taps.clear();
    taps.add (now);
    while (taps.size() > 5) taps.remove (0);
    if (taps.size() >= 2)
    {
        const double avg = (taps.getLast() - taps.getFirst()) / (taps.size() - 1);
        bpm.setValue (jlimit (30.0, 300.0, std::round (60000.0 / avg)), sendNotificationSync);
    }
}

void StandaloneBar::timerCallback()
{
   #if JucePlugin_Build_Standalone
    if (auto* h = StandalonePluginHolder::getInstance())
    {
        const bool live = ! (bool) h->getMuteInputValue().getValue();
        inputBtn.setToggleState (live, dontSendNotification);
        inputBtn.setButtonText (live ? "Input: on" : "Input: muted");
    }
   #endif
    tunerBtn.setToggleState (proc.tunerOn.load(), dontSendNotification);
    recBtn.setToggleState (proc.isRecording(), dontSendNotification);
    recBtn.setButtonText (proc.isRecording() ? "Stop" : "Record");
    if (proc.isRecording())
    {
        const int t = (int) proc.getRecordingSeconds();
        recTime.setText (String (t / 60) + ":" + String (t % 60).paddedLeft ('0', 2), dontSendNotification);
    }
    else recTime.setText (proc.lastRecording.existsAsFile() ? "Saved" : "", dontSendNotification);
    if (! bpm.isMouseButtonDown() && std::abs (bpm.getValue() - proc.hostlessBpm.load()) > 0.5) bpm.setValue (proc.hostlessBpm.load(), dontSendNotification);
}

void StandaloneBar::paint (Graphics& g)
{
    g.setColour (ocui::bg1.brighter (0.06f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    if (proc.isRecording())
    {
        g.setColour (Colour (0xffff4d4d));
        g.fillEllipse (recBtn.getRight() + 8.0f, (float) getHeight() * 0.5f - 5.0f, 10.0f, 10.0f);
    }
}

void StandaloneBar::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    audioBtn.setBounds (r.removeFromLeft (120)); r.removeFromLeft (8);
    inputBtn.setBounds (r.removeFromLeft (112)); r.removeFromLeft (22);
    bpm.setBounds (r.removeFromLeft (230)); r.removeFromLeft (6);
    tapBtn.setBounds (r.removeFromLeft (52)); r.removeFromLeft (22);
    metroBtn.setBounds (r.removeFromLeft (96)); r.removeFromLeft (6);
    metroVol.setBounds (r.removeFromLeft (90)); r.removeFromLeft (22);
    tunerBtn.setBounds (r.removeFromLeft (72));
    folderBtn.setBounds (r.removeFromRight (100)); r.removeFromRight (8);
    recTime.setBounds (r.removeFromRight (56)); r.removeFromRight (22);
    recBtn.setBounds (r.removeFromRight (80));
}

// =====================================================================================
OmniChainPanel::OmniChainPanel (OmniChainProcessor& p) : proc (p)
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
    loadBtn.onClick = [this] { showMoreMenu(); };
    saveBtn.setTooltip ("Save this sound by name (it appears in the preset list)");
    loadBtn.setTooltip ("Rename, delete, import or export presets; delay trails and NAM CPU settings");
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &loadBtn }) addAndMakeVisible (*b);
    for (int pct : { 75, 90, 100, 115, 125, 150, 175, 200 }) sizeBox.addItem ("Size " + String (pct) + "%", pct);
    sizeBox.setTooltip ("Window size (you can also drag the bottom-right corner)");
    sizeBox.onChange = [this] { if (onScaleChosen && sizeBox.getSelectedId() > 0) onScaleChosen ((float) sizeBox.getSelectedId() / 100.0f); };
    addAndMakeVisible (sizeBox);

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

    if (proc.isStandalone())
    {
        standaloneBar = std::make_unique<StandaloneBar> (proc);
        addAndMakeVisible (*standaloneBar);
    }
    addChildComponent (tunerOverlay);
    setSize (1220, proc.isStandalone() ? 836 : 790);
    startTimerHz (30);
}

OmniChainPanel::~OmniChainPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void OmniChainPanel::rebuildPresetMenu()
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

void OmniChainPanel::stepPreset (int delta)
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

void OmniChainPanel::savePreset (bool askNewName)
{
    bool isFactory = false;
    for (auto& f : getFactoryPresets()) if (proc.currentPresetName == f.name) isFactory = true;
    const String suggestion = askNewName ? String() : (isFactory ? proc.currentPresetName + " (mine)" : proc.currentPresetName);
    auto* aw = new AlertWindow ("Save preset", "Give this sound a name. It's saved in Documents > OmniChain Presets and appears in the preset list.",
                                MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", suggestion, "Name");
    aw->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<OmniChainPanel> safe (this);
    aw->enterModalState (true, ModalCallbackFunction::create ([safe, aw] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto name = File::createLegalFileName (aw->getTextEditorContents ("name").trim());
        if (name.isEmpty()) return;
        if (safe->proc.userPresetFile (name).existsAsFile() && name != safe->proc.currentPresetName)
            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Replace preset?", "A preset called \"" + name + "\" already exists. Replace it?",
                                          "Replace", "Cancel", safe.getComponent(),
                                          ModalCallbackFunction::create ([safe, name] (int ok) { if (safe != nullptr && ok) safe->doSave (name); }));
        else safe->doSave (name);
    }), true);
}

void OmniChainPanel::doSave (const String& name)
{
    if (proc.saveUserPreset (name)) { rebuildPresetMenu(); shownPreset = {}; }
    else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Couldn't save", "The preset folder couldn't be written to:\n" + OmniChainProcessor::getUserPresetFolder().getFullPathName());
}

void OmniChainPanel::importPreset (const File& f)
{
    auto dst = OmniChainProcessor::getUserPresetFolder().getChildFile (f.getFileName());
    if (f.getParentDirectory() != OmniChainProcessor::getUserPresetFolder())
    {
        dst = dst.getNonexistentSibling (false);
        f.copyFileTo (dst);
    }
    proc.loadPresetFile (dst);
    rebuildPresetMenu();
}

void OmniChainPanel::showMoreMenu()
{
    const bool user = isUserPreset();
    PopupMenu m;
    m.addItem (1, "Save as a new preset...");
    m.addItem (2, "Rename this preset...", user);
    m.addItem (3, "Delete this preset", user);
    m.addItem (8, "Revert to saved version", proc.isModified() && proc.currentPresetName != "Custom");
    m.addSeparator();
    m.addItem (4, "Import preset file...");
    m.addItem (5, "Export this preset to a file...");
    m.addItem (6, "Open presets folder");
    m.addSeparator();
    m.addItem (9, "Delay & reverb trails after bypass", true, proc.getTrails());
    m.addItem (10, "NAM: run A2 captures at their Lite size (less CPU)", true, OmniChainProcessor::getNamLight());
    SafePointer<OmniChainPanel> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&loadBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        const auto cur = pr.userPresetFile (pr.currentPresetName);
        switch (r)
        {
            case 1: safe->savePreset (true); break;
            case 2:
            {
                auto* aw = new AlertWindow ("Rename preset", "New name:", MessageBoxIconType::NoIcon, safe.getComponent());
                aw->addTextEditor ("name", pr.currentPresetName);
                aw->addButton ("Rename", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
                aw->enterModalState (true, ModalCallbackFunction::create ([safe, aw, cur] (int ok)
                {
                    if (safe == nullptr || ! ok) return;
                    const auto name = File::createLegalFileName (aw->getTextEditorContents ("name").trim());
                    const auto dst = safe->proc.userPresetFile (name);
                    if (name.isEmpty() || dst.existsAsFile()) return;
                    if (cur.moveFileTo (dst)) { safe->proc.currentPresetName = name; safe->rebuildPresetMenu(); }
                }), true);
                break;
            }
            case 3:
                AlertWindow::showOkCancelBox (MessageBoxIconType::WarningIcon, "Delete preset?", "Delete \"" + pr.currentPresetName + "\"? This can't be undone.",
                                              "Delete", "Cancel", safe.getComponent(),
                                              ModalCallbackFunction::create ([safe, cur] (int ok)
                                              {
                                                  if (safe == nullptr || ! ok) return;
                                                  cur.moveToTrash();
                                                  safe->proc.currentPresetName = "Custom";
                                                  safe->rebuildPresetMenu();
                                              }));
                break;
            case 4:
                safe->chooser = std::make_unique<FileChooser> ("Import OmniChain preset", File::getSpecialLocation (File::userHomeDirectory), "*.ocpreset");
                safe->chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [safe] (const FileChooser& fc)
                {
                    if (safe != nullptr && fc.getResult().existsAsFile()) safe->importPreset (fc.getResult());
                });
                break;
            case 5:
                safe->chooser = std::make_unique<FileChooser> ("Export preset", File::getSpecialLocation (File::userDesktopDirectory).getChildFile (File::createLegalFileName (pr.currentPresetName) + ".ocpreset"), "*.ocpreset");
                safe->chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting, [safe] (const FileChooser& fc)
                {
                    if (safe == nullptr || fc.getResult() == File()) return;
                    const auto keepName = safe->proc.currentPresetName;
                    safe->proc.savePresetFile (fc.getResult().withFileExtension ("ocpreset"));
                    safe->proc.currentPresetName = keepName;
                });
                break;
            case 6: OmniChainProcessor::getUserPresetFolder().startAsProcess(); break;
            case 9: pr.setTrails (! pr.getTrails()); break;
            case 10: pr.setNamLight (! OmniChainProcessor::getNamLight()); break;
            case 8:
                if (cur.existsAsFile()) pr.loadPresetFile (cur);
                else for (int i = 0; i < pr.getNumPrograms(); ++i) if (pr.getProgramName (i) == pr.currentPresetName) pr.setCurrentProgram (i);
                break;
            default: break;
        }
    });
}

bool OmniChainPanel::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (File (f).hasFileExtension ("ocpreset")) return true;
    return false;
}

void OmniChainPanel::filesDropped (const StringArray& files, int, int)
{
    for (auto& f : files) if (File (f).hasFileExtension ("ocpreset")) { importPreset (File (f)); break; }
}

void OmniChainPanel::showScale (float sc)
{
    const int pct = roundToInt (sc * 100.0f);
    if (sizeBox.indexOfItemId (pct) >= 0) sizeBox.setSelectedId (pct, dontSendNotification);
    else sizeBox.setText ("Size " + String (pct) + "%", dontSendNotification);
}

void OmniChainPanel::timerCallback()
{
    for (auto& c : cards) c->refresh();
    if (tunerOverlay.isVisible() != proc.tunerOn.load()) { tunerOverlay.setVisible (proc.tunerOn.load()); if (tunerOverlay.isVisible()) tunerOverlay.toFront (false); }
    const float decay = 0.86f;
    const float i = proc.engine.takeInPeak(), o = proc.engine.takeOutPeak();
    inLvl = jmax (i, inLvl * decay); outLvl = jmax (o, outLvl * decay);
    repaint (getWidth() - 46, 10, 40, 60);
    const String nm = proc.currentPresetName + (proc.isModified() ? "  (edited)" : "");
    if (nm != shownPreset) { shownPreset = nm; presetBox.setText (nm, dontSendNotification); }
}

void OmniChainPanel::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (ocui::bg1, 0, 0, ocui::bg0, 0, (float) getHeight(), false));
    g.fillAll();

    // title
    g.setColour (ocui::text);
    g.setFont (Font (28.0f, Font::bold));
    g.drawText ("OMNICHAIN", 20, 14, 260, 32, Justification::centredLeft);
    g.setFont (Font (12.0f));
    g.setColour (ocui::textDim);
    g.drawText ("modular multi-fx  |  65 effects  |  NAM + IR library  |  0 samples latency", 22, 46, 360, 16, Justification::centredLeft);

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
    g.drawText ("Drag a slot header to reorder  |  right-click a header to save or load effect patches  |  drop .nam / .wav / presets onto the window  |  drag the corner to resize",
                0, getHeight() - 22, getWidth(), 16, Justification::centred);
}

void OmniChainPanel::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (80).reduced (16, 8);
    header.removeFromLeft (360);
    header.removeFromRight (40);
    auto knobsArea = header.removeFromRight (3 * 70);
    inKnob.setBounds (knobsArea.removeFromLeft (70));
    mixKnob.setBounds (knobsArea.removeFromLeft (70));
    outKnob.setBounds (knobsArea.removeFromLeft (70));
    auto pr = header.withSizeKeepingCentre (jmin (header.getWidth() - 10, 560), 30);
    sizeBox.setBounds (pr.removeFromRight (92)); pr.removeFromRight (6);
    loadBtn.setBounds (pr.removeFromRight (56)); pr.removeFromRight (6);
    saveBtn.setBounds (pr.removeFromRight (56)); pr.removeFromRight (6);
    nextBtn.setBounds (pr.removeFromRight (28)); pr.removeFromRight (4);
    prevBtn.setBounds (pr.removeFromLeft (28));  pr.removeFromLeft (4);
    presetBox.setBounds (pr);

    if (standaloneBar != nullptr) standaloneBar->setBounds (area.removeFromTop (46).reduced (16, 2));
    area.removeFromBottom (26);
    area.reduce (16, 4);
    tunerOverlay.setBounds (area.reduced (60, 40));
    const int gapX = 22, gapY = 26;
    const int cw = (area.getWidth() - 3 * gapX) / 4, ch = (area.getHeight() - gapY) / 2;
    for (int i = 0; i < oc::kNumSlots; ++i)
    {
        const int row = i / 4, col = i % 4;
        cards[i]->setBounds (area.getX() + col * (cw + gapX), area.getY() + row * (ch + gapY), cw, ch);
    }
}

// =====================================================================================
OmniChainEditor::OmniChainEditor (OmniChainProcessor& p) : AudioProcessorEditor (&p), proc (p), panel (p)
{
    baseW = panel.getWidth(); baseH = panel.getHeight();
    addAndMakeVisible (panel);
    panel.onScaleChosen = [this] (float sc) { setSize (roundToInt (baseW * sc), roundToInt (baseH * sc)); };
    setResizable (true, true);
    setResizeLimits (roundToInt (baseW * 0.6), roundToInt (baseH * 0.6), roundToInt (baseW * 2.5), roundToInt (baseH * 2.5));
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    const float sc = proc.getUiScale();
    ready = true;
    setSize (roundToInt (baseW * sc), roundToInt (baseH * sc));
}

void OmniChainEditor::resized()
{
    if (! ready || getWidth() <= 0) return;      // ignore the zero-size calls made while the window is being built
    const float sc = (float) getWidth() / (float) baseW;
    panel.setTransform (AffineTransform::scale (sc));
    panel.setBounds (0, 0, baseW, baseH);
    proc.setUiScale (sc);
    panel.showScale (sc);
}
