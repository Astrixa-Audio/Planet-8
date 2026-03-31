/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// PatchBrowserPanel
//==============================================================================
PatchBrowserPanel::PatchBrowserPanel (Planet8AudioProcessor& p)
    : proc (p)
{
    searchBox.setTextToShowWhenEmpty ("Search patches...", juce::Colours::grey);
    searchBox.setFont (juce::Font (12.0f));
    searchBox.onTextChange = [this]() { refreshList(); };
    addAndMakeVisible (searchBox);

    patchList.setRowHeight (20);
    patchList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1a1a1a));
    patchList.setColour (juce::ListBox::outlineColourId,    juce::Colours::grey.withAlpha (0.3f));
    patchList.setOutlineThickness (1);
    addAndMakeVisible (patchList);

    refreshList();
}

void PatchBrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));
    g.setColour (juce::Colours::grey.withAlpha (0.5f));
    g.drawRect (getLocalBounds(), 1);
}

void PatchBrowserPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    searchBox.setBounds (area.removeFromTop (22));
    area.removeFromTop (2);
    patchList.setBounds (area);
}

int PatchBrowserPanel::getNumRows()
{
    return filteredIndices.size();
}

void PatchBrowserPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= filteredIndices.size()) return;

    int globalIdx = filteredIndices[row];
    auto& pm = *proc.patchManager;
    if (globalIdx >= pm.getNumPatches()) return;

    const auto& entry = pm.getPatch (globalIdx);

    if (selected)
        g.fillAll (juce::Colours::royalblue.withAlpha (0.7f));
    else if (entry.isFactory)
        g.fillAll (juce::Colour (0xff282828));
    else
        g.fillAll (juce::Colour (0xff1e1e1e));

    // Visual divider at the start of user patches
    if (!entry.isFactory && entry.bankIndex == 0)
    {
        g.setColour (juce::Colours::grey.withAlpha (0.6f));
        g.drawHorizontalLine (0, 0.0f, (float) w);
    }

    // Highlight current patch
    if (globalIdx == pm.getCurrentGlobalIndex())
    {
        g.setColour (juce::Colours::yellow.withAlpha (0.15f));
        g.fillRect (0, 0, w, h);
    }

    juce::String prefix = entry.isFactory
        ? "F/" + juce::String (entry.bankIndex + 1).paddedLeft ('0', 2) + "  "
        : "U/" + juce::String (entry.bankIndex + 1).paddedLeft ('0', 2) + "  ";

    g.setColour (selected ? juce::Colours::white : juce::Colours::lightgrey);
    g.setFont (juce::Font (11.0f));
    g.drawFittedText (prefix + entry.name, 6, 0, w - 10, h,
                      juce::Justification::centredLeft, 1);
}

void PatchBrowserPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= filteredIndices.size()) return;
    proc.patchManager->loadPatch (filteredIndices[row]);
    patchList.repaint();
}

void PatchBrowserPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (!e.mods.isRightButtonDown()) return;
    if (row < 0 || row >= filteredIndices.size()) return;

    int globalIdx = filteredIndices[row];
    auto& pm = *proc.patchManager;
    if (globalIdx >= pm.getNumPatches()) return;

    const auto& entry = pm.getPatch (globalIdx);
    if (entry.isFactory) return;   // read-only

    int userSlot = entry.bankIndex;

    juce::PopupMenu menu;
    menu.addItem (1, "Load");
    menu.addItem (2, "Save Here");
    menu.addItem (3, "Init Slot");
    menu.addSeparator();
    menu.addItem (4, "Delete");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
        [this, globalIdx, userSlot, &pm] (int choice)
        {
            switch (choice)
            {
            case 1:
                pm.loadPatch (globalIdx);
                break;
            case 2:
            {
                auto* window = new juce::AlertWindow ("Save Patch", "", juce::AlertWindow::NoIcon);
                window->addTextEditor ("name", pm.getPatch (globalIdx).name, "Name:");
                window->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
                window->addButton ("Cancel", 0);
                window->enterModalState (true,
                    juce::ModalCallbackFunction::create ([this, userSlot, window](int r)
                    {
                        if (r == 1)
                        {
                            auto name = window->getTextEditorContents ("name").trim();
                            if (name.isNotEmpty())
                                proc.patchManager->saveCurrentAsUserPatch (name, userSlot);
                        }
                        refreshList();
                    }),
                    true);
                break;
            }
            case 3:
                proc.patchManager->initCurrentPatch();
                proc.patchManager->saveCurrentAsUserPatch ("Init", userSlot);
                refreshList();
                break;
            case 4:
            {
                juce::AlertWindow::showOkCancelBox (
                    juce::AlertWindow::WarningIcon,
                    "Delete Patch",
                    "Delete \"" + pm.getPatch (globalIdx).name + "\"?",
                    "Delete", "Cancel", nullptr,
                    juce::ModalCallbackFunction::create ([this, userSlot](int r)
                    {
                        if (r == 1)
                            proc.patchManager->deleteUserPatch (userSlot);
                        refreshList();
                    }));
                break;
            }
            default: break;
            }
        });
}

void PatchBrowserPanel::refreshList()
{
    auto& pm = *proc.patchManager;
    juce::String filter = searchBox.getText().trim().toLowerCase();

    filteredIndices.clear();
    factoryRowCount = 0;

    for (int i = 0; i < pm.numFactoryPatches(); ++i)
    {
        const auto& e = pm.getFactoryPatches().getReference (i);
        if (filter.isEmpty() || e.name.toLowerCase().contains (filter))
        {
            filteredIndices.add (i);
            ++factoryRowCount;
        }
    }
    for (int i = 0; i < pm.numUserPatches(); ++i)
    {
        const auto& e = pm.getUserPatches().getReference (i);
        int globalIdx = pm.numFactoryPatches() + i;
        if (filter.isEmpty() || e.name.toLowerCase().contains (filter))
            filteredIndices.add (globalIdx);
    }

    patchList.updateContent();
    patchList.repaint();
}

//==============================================================================
// Planet8AudioProcessorEditor
//==============================================================================
Planet8AudioProcessorEditor::Planet8AudioProcessorEditor (Planet8AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setWantsKeyboardFocus (true);

    // ---- Patch strip ---------------------------------------------------------
    patchDisplay.setFont (juce::Font (12.0f, juce::Font::bold));
    patchDisplay.setJustificationType (juce::Justification::centredLeft);
    patchDisplay.setColour (juce::Label::textColourId,       juce::Colours::white);
    patchDisplay.setColour (juce::Label::backgroundColourId, juce::Colour (0xff121212));
    patchDisplay.setColour (juce::Label::outlineColourId,    juce::Colours::white.withAlpha (0.2f));
    addAndMakeVisible (patchDisplay);

    auto styleBtn = [](juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2a2a2a));
        b.setColour (juce::TextButton::textColourOffId,  juce::Colours::white);
    };

    styleBtn (patchPrevBtn);
    styleBtn (patchNextBtn);
    styleBtn (patchSaveBtn);
    styleBtn (patchInitBtn);
    styleBtn (patchCmpBtn);
    styleBtn (patchDeleteBtn);
    styleBtn (patchUndoBtn);
    styleBtn (patchRedoBtn);
    styleBtn (patchBrowseBtn);

    addAndMakeVisible (patchPrevBtn);
    addAndMakeVisible (patchNextBtn);
    addAndMakeVisible (patchSaveBtn);
    addAndMakeVisible (patchInitBtn);
    addAndMakeVisible (patchCmpBtn);
    addAndMakeVisible (patchDeleteBtn);
    addAndMakeVisible (patchUndoBtn);
    addAndMakeVisible (patchRedoBtn);
    addAndMakeVisible (patchBrowseBtn);

    patchPrevBtn.onClick = [this]() { audioProcessor.patchManager->loadPrevPatch(); };
    patchNextBtn.onClick = [this]() { audioProcessor.patchManager->loadNextPatch(); };

    patchSaveBtn.onClick = [this]()
    {
        auto* window = new juce::AlertWindow ("Save Patch", "", juce::AlertWindow::NoIcon);
        window->addTextEditor ("name", audioProcessor.patchManager->getCurrentPatchName(), "Name:");
        window->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0);
        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, window](int result)
            {
                if (result == 1)
                {
                    auto name = window->getTextEditorContents ("name").trim();
                    if (name.isNotEmpty())
                    {
                        // Check for an existing user patch with the same name
                        const auto& userPatches = audioProcessor.patchManager->getUserPatches();
                        int existingSlot = -1;
                        for (int i = 0; i < userPatches.size(); ++i)
                        {
                            if (userPatches[i].name.equalsIgnoreCase (name))
                            {
                                existingSlot = i;
                                break;
                            }
                        }

                        if (existingSlot >= 0)
                        {
                            juce::AlertWindow::showOkCancelBox (
                                juce::AlertWindow::WarningIcon,
                                "Overwrite Patch?",
                                "A patch named \"" + name + "\" already exists. Overwrite it?",
                                "Overwrite", "Cancel", nullptr,
                                juce::ModalCallbackFunction::create ([this, name, existingSlot](int overwrite)
                                {
                                    if (overwrite == 1)
                                        audioProcessor.patchManager->saveCurrentAsUserPatch (name, existingSlot);
                                }));
                        }
                        else
                        {
                            audioProcessor.patchManager->saveCurrentAsUserPatch (name);
                        }
                    }
                }
            }),
            true);
    };

    patchInitBtn.onClick = [this]()
    {
        juce::AlertWindow::showOkCancelBox (
            juce::AlertWindow::WarningIcon,
            "Init Patch",
            "Reset all parameters to defaults?",
            "OK", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([this](int result)
            {
                if (result == 1)
                    audioProcessor.patchManager->initCurrentPatch();
            }));
    };

    patchCmpBtn.onClick = [this]()
    {
        audioProcessor.patchManager->toggleCompare();
        bool comparing = audioProcessor.patchManager->isComparing();
        patchCmpBtn.setColour (juce::TextButton::buttonColourId,
            comparing ? juce::Colours::darkorange.darker() : juce::Colour (0xff2a2a2a));
    };

    patchDeleteBtn.onClick = [this]()
    {
        auto& pm = *audioProcessor.patchManager;
        juce::AlertWindow::showOkCancelBox (
            juce::AlertWindow::WarningIcon,
            "Delete Patch",
            "Delete \"" + pm.getCurrentPatchName() + "\"?",
            "Delete", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([this](int result)
            {
                if (result == 1)
                {
                    auto& pm2 = *audioProcessor.patchManager;
                    pm2.deleteUserPatch (pm2.getCurrentBankIndex());
                }
            }));
    };

    patchUndoBtn.onClick = [this]() { audioProcessor.patchManager->undo(); };
    patchRedoBtn.onClick = [this]() { audioProcessor.patchManager->redo(); };

    patchBrowseBtn.onClick = [this]()
    {
        patchBrowserVisible = !patchBrowserVisible;
        patchBrowser->setVisible (patchBrowserVisible);
        if (patchBrowserVisible)
        {
            patchBrowser->refreshList();
            patchBrowser->toFront (false);
        }
        patchBrowseBtn.setColour (juce::TextButton::buttonColourId,
            patchBrowserVisible ? juce::Colours::slategrey : juce::Colour (0xff2a2a2a));
    };

    patchBrowser = std::make_unique<PatchBrowserPanel> (audioProcessor);
    patchBrowser->setVisible (false);
    addChildComponent (*patchBrowser);

    updatePatchDisplay();
    startTimer (100);

    // OSC LFO mod depth
    oscLfoModSlider.setSliderStyle(juce::Slider::LinearVertical);
    oscLfoModSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(oscLfoModSlider);

    // OSC ENV1 mod depth
    oscEnvModSlider.setSliderStyle(juce::Slider::LinearVertical);
    oscEnvModSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(oscEnvModSlider);

    // Mod target switch
    modTargetSwitch.setSliderStyle(juce::Slider::LinearVertical);
    modTargetSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(modTargetSwitch);

    // Pulse width
    pulseWidthSlider.setSliderStyle(juce::Slider::LinearVertical);
    pulseWidthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(pulseWidthSlider);

    // PW mode switch
    pwModeSwitch.setSliderStyle(juce::Slider::LinearVertical);
    pwModeSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(pwModeSwitch);

    // Cross mod slider (OSC2 → OSC1 FM)
    crossModSlider.setSliderStyle(juce::Slider::LinearVertical);
    crossModSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(crossModSlider);

    // Wave selector OSC1
    waveSelector.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    waveSelector.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    waveSelector.setRange(0, 5, 1);
    addAndMakeVisible(waveSelector);

    // Play mode switch
    playModeSwitch.setSliderStyle(juce::Slider::LinearVertical);
    playModeSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(playModeSwitch);

    // OSC mix
    oscMixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    oscMixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(oscMixSlider);

    // Octave knob OSC1
    octaveKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    octaveKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(octaveKnob);

    // OSC2 hard sync switch
    osc2SyncSwitch.setSliderStyle(juce::Slider::LinearVertical);
    osc2SyncSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(osc2SyncSwitch);

    // Wave selector OSC2
    waveSelector2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    waveSelector2.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(waveSelector2);

    // Octave knob OSC2
    octaveKnob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    octaveKnob2.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(octaveKnob2);

    // Fine tune knob OSC2
    fineTuneKnob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    fineTuneKnob2.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(fineTuneKnob2);

    // Attachments
    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "WAVE", waveSelector);

    pulseWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "PW", pulseWidthSlider);

    octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "OCT", octaveKnob);

    waveAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "WAVE2", waveSelector2);

    octaveAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "OCT2", octaveKnob2);

    fineAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "OSC2_FINE", fineTuneKnob2);

    oscMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "OSC_MIX", oscMixSlider);

    modTargetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "MOD_TARGET", modTargetSwitch);

    crossModAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "CROSS_MOD", crossModSlider);

    osc2SyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "OSC2_SYNC", osc2SyncSwitch);

    oscLfoModAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "OSC_LFO_MOD", oscLfoModSlider);

    oscEnvModAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "OSC_ENV_MOD", oscEnvModSlider);

    pwModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "PW_MODE", pwModeSwitch);

    playModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "PLAY_MODE", playModeSwitch);

    // ---- Filter controls ------------------------------------------------
    hpfCutoffSlider.setSliderStyle(juce::Slider::LinearVertical);
    hpfCutoffSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(hpfCutoffSlider);

    cutoffSlider.setSliderStyle(juce::Slider::LinearVertical);
    cutoffSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(cutoffSlider);

    resonanceSlider.setSliderStyle(juce::Slider::LinearVertical);
    resonanceSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(resonanceSlider);

    slopeSwitch.setSliderStyle(juce::Slider::LinearVertical);
    slopeSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(slopeSwitch);

    filtVelSensSlider.setSliderStyle(juce::Slider::LinearVertical);
    filtVelSensSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(filtVelSensSlider);

    filtEnvModSlider.setSliderStyle(juce::Slider::LinearVertical);
    filtEnvModSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(filtEnvModSlider);

    filtEnvSrcSwitch.setSliderStyle(juce::Slider::LinearVertical);
    filtEnvSrcSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(filtEnvSrcSwitch);

    filtLfoModSlider.setSliderStyle(juce::Slider::LinearVertical);
    filtLfoModSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(filtLfoModSlider);

    filtKeyFlwSlider.setSliderStyle(juce::Slider::LinearVertical);
    filtKeyFlwSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(filtKeyFlwSlider);

    hpfCutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "HPF_CUTOFF", hpfCutoffSlider);
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "CUTOFF", cutoffSlider);
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "RESONANCE", resonanceSlider);
    slopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "SLOPE", slopeSwitch);
    filtVelSensAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "FILT_VEL_SENS", filtVelSensSlider);
    filtEnvModAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "FILT_ENV_MOD", filtEnvModSlider);
    filtEnvSrcAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "FILT_ENV_SRC", filtEnvSrcSwitch);
    filtLfoModAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "FILT_LFO_MOD", filtLfoModSlider);
    filtKeyFlwAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "FILT_KEY_FLW", filtKeyFlwSlider);

    // ---- LFO controls -------------------------------------------------------
    lfoRateSlider.setSliderStyle(juce::Slider::LinearVertical);
    lfoRateSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(lfoRateSlider);

    lfoDelaySlider.setSliderStyle(juce::Slider::LinearVertical);
    lfoDelaySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(lfoDelaySlider);

    lfoWaveKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lfoWaveKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(lfoWaveKnob);

    lfoKeyTrigSwitch.setSliderStyle(juce::Slider::LinearVertical);
    lfoKeyTrigSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(lfoKeyTrigSwitch);

    lfoTrigEnvSwitch.setSliderStyle(juce::Slider::LinearVertical);
    lfoTrigEnvSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(lfoTrigEnvSwitch);

    lfoRateAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "LFO_RATE",    lfoRateSlider);
    lfoDelayAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "LFO_DELAY",   lfoDelaySlider);
    lfoWaveAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "LFO_WAVE",    lfoWaveKnob);
    lfoKeyTrigAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "LFO_KEYTRIG", lfoKeyTrigSwitch);
    lfoTrigEnvAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "LFO_TRIGENV", lfoTrigEnvSwitch);

    // ---- ENV 1 controls -----------------------------------------------------
    auto initSlider = [&](juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);
    };
    auto initSwitch = [&](juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);
    };

    initSwitch(env1KeyFollowSwitch);
    initSlider(env1ASlider);
    initSlider(env1DSlider);
    initSlider(env1SSlider);
    initSlider(env1RSlider);

    env1KFAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV1_KEYFOLLOW", env1KeyFollowSwitch);
    env1AAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV1_A", env1ASlider);
    env1DAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV1_D", env1DSlider);
    env1SAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV1_S", env1SSlider);
    env1RAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV1_R", env1RSlider);

    // ---- ENV 2 controls -----------------------------------------------------
    initSwitch(env2KeyFollowSwitch);
    initSlider(env2ASlider);
    initSlider(env2DSlider);
    initSlider(env2SSlider);
    initSlider(env2RSlider);

    env2KFAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV2_KEYFOLLOW", env2KeyFollowSwitch);
    env2AAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV2_A", env2ASlider);
    env2DAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV2_D", env2DSlider);
    env2SAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV2_S", env2SSlider);
    env2RAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ENV2_R", env2RSlider);

    // ---- VCA controls -------------------------------------------------------
    initSlider(vcaLevelSlider);
    initSlider(vcaLfoModSlider);
    initSlider(vcaVelSensSlider);

    vcaLevelAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "VCA_LEVEL",    vcaLevelSlider);
    vcaLfoModAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "VCA_LFO_MOD", vcaLfoModSlider);
    vcaVelSensAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "VCA_VEL_SENS", vcaVelSensSlider);

    // ---- KEYBRD MOD controls ------------------------------------------------
    portaKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    portaKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(portaKnob);

    legatoSwitch.setSliderStyle(juce::Slider::LinearVertical);
    legatoSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(legatoSwitch);

    bendOscTgtSwitch.setSliderStyle(juce::Slider::LinearVertical);
    bendOscTgtSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(bendOscTgtSwitch);

    bendSensOscSlider.setSliderStyle(juce::Slider::LinearVertical);
    bendSensOscSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(bendSensOscSlider);

    bendSensFiltSlider.setSliderStyle(juce::Slider::LinearVertical);
    bendSensFiltSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(bendSensFiltSlider);

    modSensOscSlider.setSliderStyle(juce::Slider::LinearVertical);
    modSensOscSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(modSensOscSlider);

    modSensFiltSlider.setSliderStyle(juce::Slider::LinearVertical);
    modSensFiltSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(modSensFiltSlider);

    portaAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "PORTAMENTO",    portaKnob);
    legatoAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "LEGATO",        legatoSwitch);
    bendOscTgtAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "BEND_OSC_TGT",  bendOscTgtSwitch);
    bendSensOscAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "BEND_SENS_OSC", bendSensOscSlider);
    bendSensFiltAttachment= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "BEND_SENS_FILT",bendSensFiltSlider);
    modSensOscAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "MOD_SENS_OSC",  modSensOscSlider);
    modSensFiltAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "MOD_SENS_FILT", modSensFiltSlider);

    // ---- ARP controls -------------------------------------------------------
    arpModeSwitch.setSliderStyle(juce::Slider::LinearVertical);
    arpModeSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(arpModeSwitch);

    arpOctSwitch.setSliderStyle(juce::Slider::LinearVertical);
    arpOctSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(arpOctSwitch);

    arpClockSwitch.setSliderStyle(juce::Slider::LinearVertical);
    arpClockSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(arpClockSwitch);

    arpBpmSlider.setSliderStyle(juce::Slider::LinearVertical);
    arpBpmSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(arpBpmSlider);

    arpDivSwitch.setSliderStyle(juce::Slider::LinearVertical);
    arpDivSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(arpDivSwitch);

    arpModeAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ARP_MODE",  arpModeSwitch);
    arpOctAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ARP_OCT",   arpOctSwitch);
    arpClockAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ARP_CLOCK", arpClockSwitch);
    arpBpmAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ARP_BPM",   arpBpmSlider);
    arpDivAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "ARP_DIV",   arpDivSwitch);


    // ---- Status bar label ---------------------------------------------------
    statusLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId,       juce::Colours::white);
    statusLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (statusLabel);

    // Hook every slider so dragging it updates the status bar
    attachStatus (oscLfoModSlider,      "LFO MOD");
    attachStatus (oscEnvModSlider,      "ENV MOD");
    attachStatus (pwModeSwitch,         "PW SOURCE");
    attachStatus (crossModSlider,       "CROSS MOD");
    attachStatus (waveSelector,         "OSC1 WAVE");
    attachStatus (pulseWidthSlider,     "PULSE WIDTH");
    attachStatus (octaveKnob,           "OSC1 OCT");
    attachStatus (modTargetSwitch,      "MOD TARGET");
    attachStatus (osc2SyncSwitch,       "OSC2 SYNC");
    attachStatus (waveSelector2,        "OSC2 WAVE");
    attachStatus (octaveKnob2,          "OSC2 OCT");
    attachStatus (fineTuneKnob2,        "OSC2 FINE");
    attachStatus (oscMixSlider,         "OSC MIX");
    attachStatus (playModeSwitch,       "PLAY MODE");
    attachStatus (hpfCutoffSlider,      "HPF");
    attachStatus (cutoffSlider,         "CUTOFF");
    attachStatus (resonanceSlider,      "RESONANCE");
    attachStatus (slopeSwitch,          "SLOPE");
    attachStatus (filtVelSensSlider,    "FILT VEL SENS");
    attachStatus (filtEnvModSlider,     "FILT ENV MOD");
    attachStatus (filtEnvSrcSwitch,     "FILT ENV SRC");
    attachStatus (filtLfoModSlider,     "FILT LFO MOD");
    attachStatus (filtKeyFlwSlider,     "FILT KEY FOLLOW");
    attachStatus (lfoRateSlider,        "LFO RATE");
    attachStatus (lfoDelaySlider,       "LFO DELAY");
    attachStatus (lfoWaveKnob,          "LFO WAVE");
    attachStatus (lfoKeyTrigSwitch,     "LFO KEY TRIG");
    attachStatus (lfoTrigEnvSwitch,     "LFO TRIG ENV");
    attachStatus (env1KeyFollowSwitch,  "ENV1 KEY FOLLOW");
    attachStatus (env1ASlider,          "ENV1 ATTACK");
    attachStatus (env1DSlider,          "ENV1 DECAY");
    attachStatus (env1SSlider,          "ENV1 SUSTAIN");
    attachStatus (env1RSlider,          "ENV1 RELEASE");
    attachStatus (env2KeyFollowSwitch,  "ENV2 KEY FOLLOW");
    attachStatus (env2ASlider,          "ENV2 ATTACK");
    attachStatus (env2DSlider,          "ENV2 DECAY");
    attachStatus (env2SSlider,          "ENV2 SUSTAIN");
    attachStatus (env2RSlider,          "ENV2 RELEASE");
    attachStatus (vcaLevelSlider,       "VCA LEVEL");
    attachStatus (vcaLfoModSlider,      "VCA LFO MOD");
    attachStatus (vcaVelSensSlider,     "VCA VEL SENS");
    attachStatus (portaKnob,            "PORTAMENTO");
    attachStatus (legatoSwitch,         "LEGATO");
    attachStatus (bendOscTgtSwitch,     "BEND OSC TARGET");
    attachStatus (bendSensOscSlider,    "BEND SENS OSC");
    attachStatus (bendSensFiltSlider,   "BEND SENS FILT");
    attachStatus (modSensOscSlider,     "MOD SENS OSC");
    attachStatus (modSensFiltSlider,    "MOD SENS FILT");
    attachStatus (arpModeSwitch,        "ARP MODE");
    attachStatus (arpOctSwitch,         "ARP OCT");
    attachStatus (arpClockSwitch,       "ARP CLOCK");
    attachStatus (arpBpmSlider,         "ARP BPM");
    attachStatus (arpDivSwitch,         "ARP DIV");

    setSize (1046, 822);
}

Planet8AudioProcessorEditor::~Planet8AudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void Planet8AudioProcessorEditor::timerCallback()
{
    updatePatchDisplay();
}

void Planet8AudioProcessorEditor::updatePatchDisplay()
{
    auto& pm = *audioProcessor.patchManager;
    juce::String prefix = pm.currentPatchIsFactory()
        ? ("F/" + juce::String (pm.getCurrentBankIndex() + 1).paddedLeft ('0', 2) + "  ")
        : ("U/" + juce::String (pm.getCurrentBankIndex() + 1).paddedLeft ('0', 2) + "  ");
    juce::String text = prefix + pm.getCurrentPatchName();
    if (pm.isModified())   text += "  *";
    if (pm.isComparing())  text += "  [CMP]";
    patchDisplay.setText (text, juce::dontSendNotification);

    bool isUser = !pm.currentPatchIsFactory();
    patchDeleteBtn.setEnabled (isUser);
    patchDeleteBtn.setAlpha (isUser ? 1.0f : 0.35f);
}

void Planet8AudioProcessorEditor::attachStatus (juce::Slider& s, const juce::String& name)
{
    auto update = [this, &s, name]()
    {
        const double val = s.getValue();
        const juce::String valStr = (s.getInterval() >= 1.0)
            ? juce::String (juce::roundToInt (val))
            : juce::String (val, 2);
        statusLabel.setText (name + "   " + valStr, juce::dontSendNotification);
    };

    s.onDragStart  = update;
    s.onValueChange = update;
}

bool Planet8AudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    auto& pm = *audioProcessor.patchManager;

    // Ctrl/Cmd + Z  → Undo,  Ctrl/Cmd + Y or Shift+Z → Redo
    const auto cmd = juce::ModifierKeys::commandModifier;
    if (key == juce::KeyPress ('z', cmd, 0))
    { pm.undo(); return true; }
    if (key == juce::KeyPress ('y', cmd, 0))
    { pm.redo(); return true; }
    if (key == juce::KeyPress ('z', cmd | juce::ModifierKeys::shiftModifier, 0))
    { pm.redo(); return true; }

    return false;
}

//==============================================================================
// Layout constants — single source of truth used by both resized() and paint()
namespace Layout
{
    // Patch strip at the top (height 50, then 30px gap before row 1)
    static constexpr int patchStripY = 4;
    static constexpr int patchStripH = 50;
    static constexpr int patchGap    = 30;
    static constexpr int patchOffset = patchStripH + patchGap; // 80

    // Row 1: oscillator section
    static constexpr int r1HeaderY  = 8   + patchOffset;  // 88
    static constexpr int r1HeaderH  = 14;
    static constexpr int r1CtrlY    = 26  + patchOffset;  // 106
    static constexpr int r1SliderH  = 120;  // vertical sliders
    static constexpr int r1KnobH    = 80;   // rotary knobs
    static constexpr int r1LabelY   = 150  + patchOffset;  // 230
    static constexpr int r1LabelH   = 12;

    // Row 2: voice + filter section
    static constexpr int r2HeaderY  = 175 + patchOffset;  // 255
    static constexpr int r2HeaderH  = 14;
    static constexpr int r2CtrlY    = 193 + patchOffset;  // 273
    static constexpr int r2SliderH  = 120;
    static constexpr int r2LabelY   = 317 + patchOffset;  // 397
    static constexpr int r2LabelH   = 12;

    // Vertical separator x positions — rows 1 & 2
    static constexpr int sepOscModOsc1 = 218;  // between OSC MOD and OSC 1
    static constexpr int sepOsc1Osc2   = 488;  // between OSC 1 and OSC 2
    static constexpr int sepOsc2Mix    = 854;  // between OSC 2 and MIX
    static constexpr int sepVoiceHpf =  86;  // between VOICE and HPF
    static constexpr int sepHpfFilt  = 164;  // between HPF and FILTER
    static constexpr int sepFiltSlop = 324;  // between FILTER and SLOPE
    static constexpr int sepSlopLFO  = 577;  // between SLOPE/new filter controls and LFO

    // LFO control x positions (scaled so lfoTEX right edge = 946)
    static constexpr int lfoRateX   = 596;   // RATE vertical slider
    static constexpr int lfoDelayX  = 659;   // DELAY vertical slider
    static constexpr int lfoWaveX   = 731;   // waveform rotary knob
    static constexpr int lfoKTX     = 842;   // KEY TRIG switch
    static constexpr int lfoTEX     = 926;   // TRIG ENV switch  (right = 946)
    static constexpr int lfoSliderW = 30;
    static constexpr int lfoWaveW   = 80;
    static constexpr int lfoSwitchW = 20;
    static constexpr int lfoSwitchH = 45;    // binary (2-position) switch height

    // Row 3: envelope section
    static constexpr int r3HeaderY  = 350 + patchOffset;  // 430
    static constexpr int r3HeaderH  = 14;
    static constexpr int r3CtrlY    = 368 + patchOffset;  // 448
    static constexpr int r3SliderH  = 120;
    static constexpr int r3SwitchH  = 60;    // key-follow binary switch
    static constexpr int r3LabelY   = 492 + patchOffset;  // 572
    static constexpr int r3LabelH   = 12;

    static constexpr int sepEnv1Env2 = 547;  // centre divider between ENV1 / ENV2

    // ENV1 control x positions (scaled so ENV2 right edge = 946)
    static constexpr int e1KFX =  30;
    static constexpr int e1AX  = 105;
    static constexpr int e1DX  = 191;
    static constexpr int e1SX  = 277;
    static constexpr int e1RX  = 363;

    // ENV2 control x positions
    static constexpr int e2KFX = 581;
    static constexpr int e2AX  = 656;
    static constexpr int e2DX  = 742;
    static constexpr int e2SX  = 828;
    static constexpr int e2RX  = 916;   // right = 946

    static constexpr int envSliderW  = 30;
    static constexpr int envSwitchW  = 20;

    // Row 4: VCA section
    static constexpr int r4HeaderY  = 518 + patchOffset;  // 598
    static constexpr int r4HeaderH  = 14;
    static constexpr int r4CtrlY    = 536 + patchOffset;  // 616
    static constexpr int r4SliderH  = 120;
    static constexpr int r4LabelY   = 660 + patchOffset;  // 740
    static constexpr int r4LabelH   = 12;

    // VCA control x positions (scaled so arpDivX right edge = 946)
    static constexpr int vcaLevelX    =  30;
    static constexpr int vcaLfoModX   = 102;
    static constexpr int vcaVelSensX  = 173;
    static constexpr int vcaSliderW   =  30;

    // KEYBRD MOD section (Row 4, after VCA)
    static constexpr int kbdSep       = 245;   // separator between VCA and KEYBRD MOD
    static constexpr int kbdPortaX    = 257;
    static constexpr int kbdPortaW    =  70;
    static constexpr int kbdLegatoX   = 362;
    static constexpr int kbdOscTgtX   = 419;
    static constexpr int kbdBendOscX  = 481;
    static constexpr int kbdBendFiltX = 533;
    static constexpr int kbdModOscX   = 586;
    static constexpr int kbdModFiltX  = 638;
    static constexpr int kbdSliderW   =  28;

    // ARP section (Row 4, after KEYBRD MOD)
    static constexpr int arpSep      = 688;   // vertical separator
    static constexpr int arpModeX    = 702;   // MODE: 5-position switch
    static constexpr int arpOctX     = 760;   // OCT: 4-position switch
    static constexpr int arpClockX   = 817;   // CLOCK: 2-position switch
    static constexpr int arpBpmX     = 867;   // BPM: vertical slider
    static constexpr int arpDivX     = 926;   // DIV: 4-position switch  (right = 946)
    static constexpr int arpSwitchW  =  20;
    static constexpr int arpSliderW  =  28;

    // Status bar (bottom of canvas)
    static constexpr int statusBarGap = 20;
    static constexpr int statusBarH   = 30;
    static constexpr int statusBarY   = r4LabelY + r4LabelH + statusBarGap; // 772
}

//==============================================================================
void Planet8AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    namespace L = Layout;

    // =========================================================================
    // PATCH STRIP
    // =========================================================================
    {
        const int sy = L::patchStripY;
        const int sh = L::patchStripH;

        // Background
        g.setColour (juce::Colour (0xff111111));
        g.fillRect (0, sy, getWidth(), sh);

        // "PATCH MEMORY" label
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::Font (9.0f, juce::Font::bold));
        g.drawFittedText ("PATCH MEMORY",
            getWidth() - 100, sy + 2, 96, 10,
            juce::Justification::centredRight, 1);

        // Bottom separator line
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawHorizontalLine (sy + sh + L::patchGap / 2, 0.0f, (float) getWidth());
    }

    // =========================================================================
    // ROW 1: OSC 1 | OSC 2 | MIX
    // =========================================================================

    // Separator lines
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawVerticalLine(L::sepOscModOsc1, (float)L::r1HeaderY,  (float)(L::r1LabelY + L::r1LabelH));
    g.drawVerticalLine(L::sepOsc1Osc2,   (float)L::r1HeaderY,  (float)(L::r1LabelY + L::r1LabelH));
    g.drawVerticalLine(L::sepOsc2Mix,    (float)L::r1HeaderY,  (float)(L::r1LabelY + L::r1LabelH));

    // Section headers
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawFittedText("OSC MOD", 10,                    L::r1HeaderY - 10, L::sepOscModOsc1 - 10,                     L::r1HeaderH, juce::Justification::centred, 1);
    g.drawFittedText("OSC 1",   L::sepOscModOsc1 + 2,  L::r1HeaderY - 10, L::sepOsc1Osc2 - L::sepOscModOsc1 - 4,    L::r1HeaderH, juce::Justification::centred, 1);
    g.drawFittedText("OSC 2",   L::sepOsc1Osc2 + 2,    L::r1HeaderY - 10, L::sepOsc2Mix - L::sepOsc1Osc2 - 4,       L::r1HeaderH, juce::Justification::centred, 1);
    g.drawFittedText("MIX",     L::sepOsc2Mix + 2,      L::r1HeaderY - 10, 960 - L::sepOsc2Mix,                      L::r1HeaderH, juce::Justification::centred, 1);

    // Individual control labels (below controls)
    g.setFont(juce::Font(9.0f));

    auto labelBelow = [&](juce::Component& c, const juce::String& text, int w = 36)
    {
        g.drawFittedText(text,
            c.getBounds().getCentreX() - w / 2, L::r1LabelY, w, L::r1LabelH,
            juce::Justification::centred, 1);
    };

    labelBelow(oscLfoModSlider,  "LFO");
    labelBelow(oscEnvModSlider,  "ENV1");
    labelBelow(modTargetSwitch,  "MOD");
    labelBelow(pulseWidthSlider, "PW");
    labelBelow(pwModeSwitch,     "PW SRC", 44);
    labelBelow(crossModSlider,   "CROSS", 40);
    labelBelow(waveSelector,     "WAVE");
    labelBelow(octaveKnob,       "OCT");
    labelBelow(waveSelector2,    "WAVE");
    labelBelow(octaveKnob2,      "OCT");
    labelBelow(osc2SyncSwitch,   "SYNC");
    labelBelow(fineTuneKnob2,    "FINE");
    labelBelow(oscMixSlider,     "MIX");

    // OSC1 / OSC2 side labels below the MIX knob
    {
        auto mb = oscMixSlider.getBounds();
        g.drawFittedText("OSC1", mb.getX(),          L::r1LabelY, 36, L::r1LabelH, juce::Justification::centredLeft,  1);
        g.drawFittedText("OSC2", mb.getRight() - 36, L::r1LabelY, 36, L::r1LabelH, juce::Justification::centredRight, 1);
    }

    // Mod target option labels (to the right of the switch)
    {
        auto sb = modTargetSwitch.getBounds();
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("OSC1", sb.getRight() + 2, sb.getY(),           28, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("Both", sb.getRight() + 2, sb.getCentreY() - 6, 28, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("OSC2", sb.getRight() + 2, sb.getBottom() - 12, 28, 12, juce::Justification::centredLeft, 1);
    }

    // OSC2 sync switch option labels
    {
        auto sb = osc2SyncSwitch.getBounds();
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("ON",  sb.getRight() + 2, sb.getY(),            28, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("OFF", sb.getRight() + 2, sb.getBottom() - 12,  28, 12, juce::Justification::centredLeft, 1);
    }

    // PW mode option labels (to the right of the switch, top=MAN, bottom=ENV2)
    {
        auto pwb = pwModeSwitch.getBounds();
        const int h3 = pwb.getHeight() / 3;
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("MAN",  pwb.getRight() + 2, pwb.getY(),              22, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("LFO",  pwb.getRight() + 2, pwb.getY() + h3 - 6,    22, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("ENV1", pwb.getRight() + 2, pwb.getY() + 2*h3 - 6,  22, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("ENV2", pwb.getRight() + 2, pwb.getBottom() - 12,    22, 12, juce::Justification::centredLeft, 1);
    }

    // ---- OSC1 waveform icons -----------------------------------------------
    {
        auto bounds = waveSelector.getBounds().toFloat();
        auto centre = bounds.getCentre();
        float radius = bounds.getWidth() * 0.6f;

        g.setColour(juce::Colours::white);
        for (int i = 0; i < 6; ++i)
        {
            float angle = juce::MathConstants<float>::twoPi * ((float)i / 6.0f)
                - juce::MathConstants<float>::halfPi
                + juce::MathConstants<float>::pi / 6.0f;

            float x = centre.x + std::cos(angle) * radius;
            float y = centre.y + std::sin(angle) * radius;
            juce::Rectangle<float> ic(x - 8, y - 6, 16, 12);
            juce::Path p;
            int order[6] = { 3, 4, 5, 0, 1, 2 };
            int wave = order[i];
            switch (wave)
            {
            case 0: p.startNewSubPath(ic.getX(), ic.getBottom()); p.lineTo(ic.getRight(), ic.getY()); p.lineTo(ic.getRight(), ic.getBottom()); break;
            case 1: p.startNewSubPath(ic.getX(), ic.getBottom()); p.lineTo(ic.getX(), ic.getY()); p.lineTo(ic.getCentreX(), ic.getY()); p.lineTo(ic.getCentreX(), ic.getBottom()); p.lineTo(ic.getRight(), ic.getBottom()); break;
            case 2: p.startNewSubPath(ic.getX(), ic.getBottom()); p.lineTo(ic.getCentreX(), ic.getY()); p.lineTo(ic.getRight(), ic.getBottom()); break;
            case 3: for (int j = 0; j < 20; ++j) { float px = ic.getX() + ic.getWidth()*j/19.0f; float py = ic.getCentreY() + std::sin((float)j/19.0f*juce::MathConstants<float>::twoPi)*ic.getHeight()*0.4f; if (j==0) p.startNewSubPath(px,py); else p.lineTo(px,py); } break;
            case 4: p.startNewSubPath(ic.getX(), ic.getBottom()); p.lineTo(ic.getX(), ic.getY()); p.lineTo(ic.getCentreX(), ic.getY()); p.lineTo(ic.getCentreX(), ic.getBottom()); p.lineTo(ic.getRight(), ic.getBottom()); p.lineTo(ic.getRight(), ic.getY()); break;
            case 5: for (int j = 0; j < 10; ++j) { float px1=ic.getX()+ic.getWidth()*(j/10.0f); float py1=ic.getY()+juce::Random::getSystemRandom().nextFloat()*ic.getHeight(); float px2=ic.getX()+ic.getWidth()*((j+1)/10.0f); float py2=ic.getY()+juce::Random::getSystemRandom().nextFloat()*ic.getHeight(); p.startNewSubPath(px1,py1); p.lineTo(px2,py2); } break;
            }
            g.strokePath(p, juce::PathStrokeType(1.0f));
        }
    }

    // ---- OSC1 octave labels ------------------------------------------------
    {
        auto bounds = octaveKnob.getBounds().toFloat();
        auto centre = bounds.getCentre();
        float radius = bounds.getWidth() * 0.65f;
        juce::String lbl[4] = { "8'", "4'", "2'", "16'" };
        float ang[4] = { juce::MathConstants<float>::pi*1.25f, juce::MathConstants<float>::pi*1.75f,
                         juce::MathConstants<float>::pi*2.25f, juce::MathConstants<float>::pi*2.75f };
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        for (int i = 0; i < 4; ++i)
        {
            float x = centre.x + std::cos(ang[i]) * radius;
            float y = centre.y + std::sin(ang[i]) * radius;
            g.drawFittedText(lbl[i], (int)x-20, (int)y-10, 40, 20, juce::Justification::centred, 1);
        }
    }

    // ---- OSC2 waveform icons -----------------------------------------------
    {
        auto bounds2  = waveSelector2.getBounds().toFloat();
        auto centre2  = bounds2.getCentre();
        float radius2 = bounds2.getWidth() * 0.6f;

        g.setColour(juce::Colours::white);
        for (int i = 0; i < 6; ++i)
        {
            float angle = juce::MathConstants<float>::twoPi * ((float)i / 6.0f)
                - juce::MathConstants<float>::halfPi
                + juce::MathConstants<float>::pi / 6.0f;

            float x = centre2.x + std::cos(angle) * radius2;
            float y = centre2.y + std::sin(angle) * radius2;
            juce::Rectangle<float> ic(x - 8, y - 6, 16, 12);
            juce::Path p;
            int order[6] = { 3, 4, 5, 0, 1, 2 };
            int wave = order[i];
            switch (wave)
            {
            case 0: p.startNewSubPath(ic.getX(), ic.getBottom()); p.lineTo(ic.getRight(), ic.getY()); p.lineTo(ic.getRight(), ic.getBottom()); break;
            case 1: p.startNewSubPath(ic.getX(), ic.getBottom()); p.lineTo(ic.getX(), ic.getY()); p.lineTo(ic.getCentreX(), ic.getY()); p.lineTo(ic.getCentreX(), ic.getBottom()); p.lineTo(ic.getRight(), ic.getBottom()); break;
            case 2: p.startNewSubPath(ic.getX(), ic.getBottom()); p.lineTo(ic.getCentreX(), ic.getY()); p.lineTo(ic.getRight(), ic.getBottom()); break;
            case 3: for (int j = 0; j < 20; ++j) { float px=ic.getX()+ic.getWidth()*j/19.0f; float py=ic.getCentreY()+std::sin((float)j/19.0f*juce::MathConstants<float>::twoPi)*ic.getHeight()*0.4f; if (j==0) p.startNewSubPath(px,py); else p.lineTo(px,py); } break;
            case 4: p.startNewSubPath(ic.getX(), ic.getBottom()); p.lineTo(ic.getX(), ic.getY()); p.lineTo(ic.getCentreX(), ic.getY()); p.lineTo(ic.getCentreX(), ic.getBottom()); p.lineTo(ic.getRight(), ic.getBottom()); p.lineTo(ic.getRight(), ic.getY()); break;
            case 5: for (int j = 0; j < 10; ++j) { float px1=ic.getX()+ic.getWidth()*(j/10.0f); float py1=ic.getY()+juce::Random::getSystemRandom().nextFloat()*ic.getHeight(); float px2=ic.getX()+ic.getWidth()*((j+1)/10.0f); float py2=ic.getY()+juce::Random::getSystemRandom().nextFloat()*ic.getHeight(); p.startNewSubPath(px1,py1); p.lineTo(px2,py2); } break;
            }
            g.strokePath(p, juce::PathStrokeType(1.0f));
        }
    }

    // ---- OSC2 octave labels ------------------------------------------------
    // Parameter range: -12 to +24 semitones (step 1). The four octave footages
    // land at the same normalized positions as a 4-choice parameter:
    //   16' = -12st (norm 0.0), 8' = 0st (norm 1/3), 4' = 12st (norm 2/3), 2' = 24st (norm 1.0)
    // JUCE rotary sweep: jStart=1.2π, jEnd=2.8π; drawAngle = juceAngle - π/2
    {
        auto bounds2  = octaveKnob2.getBounds().toFloat();
        auto centre2  = bounds2.getCentre();
        float radius2 = bounds2.getWidth() * 0.65f;
        juce::String lbl[4] = { "16'", "8'", "4'", "2'" };
        float ang[4] = { juce::MathConstants<float>::pi*0.7f,    // norm 0.0  (-12st)
                         juce::MathConstants<float>::pi*1.233f,   // norm 1/3  (  0st)
                         juce::MathConstants<float>::pi*1.767f,   // norm 2/3  (+12st)
                         juce::MathConstants<float>::pi*2.3f };   // norm 1.0  (+24st)
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        for (int i = 0; i < 4; ++i)
        {
            float x = centre2.x + std::cos(ang[i]) * radius2;
            float y = centre2.y + std::sin(ang[i]) * radius2;
            g.drawFittedText(lbl[i], (int)x-20, (int)y-10, 40, 20, juce::Justification::centred, 1);
        }
    }

    // =========================================================================
    // ROW 2: VOICE | HPF | FILTER | LFO
    // =========================================================================

    // Separator lines (no separator between FILTER and SLOPE — SLOPE is part of FILTER)
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawVerticalLine(L::sepVoiceHpf, (float)L::r2HeaderY, (float)(L::r2LabelY + L::r2LabelH));
    g.drawVerticalLine(L::sepHpfFilt,  (float)L::r2HeaderY, (float)(L::r2LabelY + L::r2LabelH));
    g.drawVerticalLine(L::sepSlopLFO,  (float)L::r2HeaderY, (float)(L::r2LabelY + L::r2LabelH));

    // Section headers — FILTER spans FREQ, RES and SLOPE controls
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawFittedText("VOICE",  10,                L::r2HeaderY, L::sepVoiceHpf - 10,                L::r2HeaderH, juce::Justification::centred, 1);
    g.drawFittedText("HPF",    L::sepVoiceHpf+2,  L::r2HeaderY, L::sepHpfFilt - L::sepVoiceHpf - 4, L::r2HeaderH, juce::Justification::centred, 1);
    g.drawFittedText("FILTER", L::sepHpfFilt+2,   L::r2HeaderY, L::sepSlopLFO - L::sepHpfFilt - 4,  L::r2HeaderH, juce::Justification::centred, 1);
    g.drawFittedText("LFO",    L::sepSlopLFO+2,   L::r2HeaderY, 1014 - L::sepSlopLFO,               L::r2HeaderH, juce::Justification::centred, 1);

    // Filter slider labels (below sliders)
    g.setFont(juce::Font(9.0f));
    {
        auto r2Lbl = [&](juce::Component& c, const juce::String& text, int w = 36)
        {
            g.drawFittedText(text, c.getBounds().getCentreX() - w / 2, L::r2LabelY, w, L::r2LabelH,
                             juce::Justification::centred, 1);
        };
        r2Lbl(hpfCutoffSlider, "CUT");
        r2Lbl(cutoffSlider,    "FREQ");
        r2Lbl(resonanceSlider, "RES");
        r2Lbl(slopeSwitch,     "SLOPE");
        r2Lbl(filtVelSensSlider, "VEL SENS", 48);
        r2Lbl(filtEnvModSlider,  "ENV MOD",  44);
        r2Lbl(filtEnvSrcSwitch,  "ENV SRC",  44);
        r2Lbl(filtLfoModSlider,  "LFO MOD",  44);
        r2Lbl(filtKeyFlwSlider,  "KEY FLW",  44);
    }

    // Play mode option labels
    {
        auto pmb = playModeSwitch.getBounds();
        g.setFont(juce::Font(10.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("Poly",   pmb.getRight() + 1, pmb.getY(),             46, 14, juce::Justification::centredLeft, 1);
        g.drawFittedText("Unison", pmb.getRight() + 1, pmb.getCentreY() - 7,   46, 14, juce::Justification::centredLeft, 1);
        g.drawFittedText("Mono",   pmb.getRight() + 1, pmb.getBottom() - 14,   46, 14, juce::Justification::centredLeft, 1);
    }

    // Slope option labels (24dB / 12dB beside the switch)
    {
        auto slb = slopeSwitch.getBounds();
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("24dB", slb.getRight() + 2, slb.getY(),            38, 13, juce::Justification::centredLeft, 1);
        g.drawFittedText("12dB", slb.getRight() + 2, slb.getBottom() - 13, 38, 13, juce::Justification::centredLeft, 1);
    }

    // ENV SRC switch option labels (ENV1 at top, ENV2 at bottom)
    {
        auto sb = filtEnvSrcSwitch.getBounds();
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("ENV2", sb.getRight() + 2, sb.getY(),            28, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("ENV1", sb.getRight() + 2, sb.getBottom() - 12,  28, 12, juce::Justification::centredLeft, 1);
    }

    // =========================================================================
    // LFO labels (still part of Row 2)
    // =========================================================================
    g.setFont(juce::Font(9.0f));
    g.setColour(juce::Colours::white);

    // Slider labels below controls
    auto r2Label = [&](juce::Component& c, const juce::String& text, int extraW = 0)
    {
        const int cx = c.getBounds().getCentreX();
        const int w  = std::max(36, extraW);
        g.drawFittedText(text, cx - w / 2, L::r2LabelY, w, L::r2LabelH,
                         juce::Justification::centred, 1);
    };

    r2Label(lfoRateSlider,   "RATE");
    r2Label(lfoDelaySlider,  "DELAY",  40);
    r2Label(lfoWaveKnob,     "WAVE");
    r2Label(lfoKeyTrigSwitch,"KEY TRIG", 52);
    r2Label(lfoTrigEnvSwitch,"TRIG ENV", 52);

    // ON/OFF labels to the right of KEY TRIG and TRIG ENV switches
    auto lfoSwitchLabels = [&](juce::Slider& sw)
    {
        auto b = sw.getBounds();
        g.drawFittedText("ON",  b.getRight() + 2, b.getY(),            34, 12,
                         juce::Justification::centredLeft, 1);
        g.drawFittedText("OFF", b.getRight() + 2, b.getBottom() - 12,  34, 12,
                         juce::Justification::centredLeft, 1);
    };
    lfoSwitchLabels(lfoKeyTrigSwitch);
    lfoSwitchLabels(lfoTrigEnvSwitch);

    // ---- LFO waveform glyphs around the rotary knob ------------------------
    // Order: Sine(3), Triangle(2), Saw(0), Square(4), RND(5) — same glyph
    // codes as the OSC wave selectors.
    // Icons are placed at the five parameter positions along the JUCE rotary
    // knob sweep arc (start ≈ 7 o'clock → top → end ≈ 5 o'clock).
    {
        auto  bounds  = lfoWaveKnob.getBounds().toFloat();
        auto  centre  = bounds.getCentre();
        float radius  = bounds.getWidth() * 0.62f;

        // JUCE default rotary sweep: startAngle = 1.2π, endAngle = 2.8π
        // (measured from 12 o'clock, clockwise).
        // Convert to std::cos/sin draw angles: drawAngle = juceAngle - π/2
        const float jStart     = juce::MathConstants<float>::pi * 1.2f;
        const float jRange     = juce::MathConstants<float>::pi * 1.6f;  // 2.8π - 1.2π
        const float drawOffset = -juce::MathConstants<float>::halfPi;

        // OSC-style glyph codes for: Sine, Triangle, Saw, Square, RND
        const int glyphCode[5] = { 3, 2, 0, 4, 5 };

        g.setColour(juce::Colours::white);

        for (int i = 0; i < 5; ++i)
        {
            const float drawAngle = (jStart + ((float)i / 4.0f) * jRange) + drawOffset;
            const float x = centre.x + std::cos(drawAngle) * radius;
            const float y = centre.y + std::sin(drawAngle) * radius;
            juce::Rectangle<float> ic(x - 8, y - 6, 16, 12);
            juce::Path p;

            switch (glyphCode[i])
            {
            case 0: // Saw
                p.startNewSubPath(ic.getX(), ic.getBottom());
                p.lineTo(ic.getRight(), ic.getY());
                p.lineTo(ic.getRight(), ic.getBottom());
                break;
            case 2: // Triangle
                p.startNewSubPath(ic.getX(), ic.getBottom());
                p.lineTo(ic.getCentreX(), ic.getY());
                p.lineTo(ic.getRight(), ic.getBottom());
                break;
            case 3: // Sine
                for (int j = 0; j < 20; ++j)
                {
                    float px = ic.getX() + ic.getWidth() * j / 19.0f;
                    float py = ic.getCentreY() + std::sin((float)j / 19.0f * juce::MathConstants<float>::twoPi) * ic.getHeight() * 0.4f;
                    if (j == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
                }
                break;
            case 4: // Square
                p.startNewSubPath(ic.getX(), ic.getBottom());
                p.lineTo(ic.getX(), ic.getY());
                p.lineTo(ic.getCentreX(), ic.getY());
                p.lineTo(ic.getCentreX(), ic.getBottom());
                p.lineTo(ic.getRight(), ic.getBottom());
                p.lineTo(ic.getRight(), ic.getY());
                break;
            case 5: // RND / Noise
                for (int j = 0; j < 10; ++j)
                {
                    float px1 = ic.getX() + ic.getWidth() * (j / 10.0f);
                    float py1 = ic.getY() + juce::Random::getSystemRandom().nextFloat() * ic.getHeight();
                    float px2 = ic.getX() + ic.getWidth() * ((j + 1) / 10.0f);
                    float py2 = ic.getY() + juce::Random::getSystemRandom().nextFloat() * ic.getHeight();
                    p.startNewSubPath(px1, py1);
                    p.lineTo(px2, py2);
                }
                break;
            }
            g.strokePath(p, juce::PathStrokeType(1.0f));
        }
    }

    // =========================================================================
    // ROW 3: ENV 1 | ENV 2
    // =========================================================================

    // Centre divider
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawVerticalLine(L::sepEnv1Env2,
                       (float)L::r3HeaderY,
                       (float)(L::r3LabelY + L::r3LabelH));

    // Section headers
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawFittedText("ENV 1", 10,                  L::r3HeaderY,
                     L::sepEnv1Env2 - 10,           L::r3HeaderH,
                     juce::Justification::centred, 1);
    g.drawFittedText("ENV 2", L::sepEnv1Env2 + 2,  L::r3HeaderY,
                     1014 - L::sepEnv1Env2,         L::r3HeaderH,
                     juce::Justification::centred, 1);

    // Control labels at the bottom
    g.setFont(juce::Font(9.0f));

    auto envLabel = [&](juce::Component& c, const juce::String& text, int extraW = 0)
    {
        const int cx = c.getBounds().getCentreX();
        const int w  = std::max(36, extraW);
        g.drawFittedText(text, cx - w / 2, L::r3LabelY, w, L::r3LabelH,
                         juce::Justification::centred, 1);
    };

    // ENV1 labels
    envLabel(env1KeyFollowSwitch, "KEY FLW", 50);
    envLabel(env1ASlider,         "A");
    envLabel(env1DSlider,         "D");
    envLabel(env1SSlider,         "S");
    envLabel(env1RSlider,         "R");

    // ENV2 labels
    envLabel(env2KeyFollowSwitch, "KEY FLW", 50);
    envLabel(env2ASlider,         "A");
    envLabel(env2DSlider,         "D");
    envLabel(env2SSlider,         "S");
    envLabel(env2RSlider,         "R");

    // Key-follow option labels (On/Off beside the switch)
    auto kfOptionLabels = [&](juce::Slider& sw)
    {
        auto b = sw.getBounds();
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("On",  b.getRight() + 2, b.getY(),            34, 12,
                         juce::Justification::centredLeft, 1);
        g.drawFittedText("Off", b.getRight() + 2, b.getBottom() - 12,  34, 12,
                         juce::Justification::centredLeft, 1);
    };
    kfOptionLabels(env1KeyFollowSwitch);
    kfOptionLabels(env2KeyFollowSwitch);

    // =========================================================================
    // ROW 4: VCA
    // =========================================================================

    // Section header
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawFittedText("VCA", L::vcaLevelX, L::r4HeaderY,
                     L::vcaVelSensX + L::vcaSliderW - L::vcaLevelX, L::r4HeaderH,
                     juce::Justification::centred, 1);

    // "LEVEL" label above the LEVEL slider
    g.setFont(juce::Font(9.0f));
    g.drawFittedText("LEVEL",
        vcaLevelSlider.getBounds().getCentreX() - 18, L::r4CtrlY - 13, 36, 12,
        juce::Justification::centred, 1);

    // Bottom labels
    auto vcaLabel = [&](juce::Component& c, const juce::String& text)
    {
        g.drawFittedText(text,
            c.getBounds().getCentreX() - 24, L::r4LabelY, 48, L::r4LabelH,
            juce::Justification::centred, 1);
    };

    vcaLabel(vcaLevelSlider,   "ENV2");
    vcaLabel(vcaLfoModSlider,  "LFO MOD");
    vcaLabel(vcaVelSensSlider, "VEL SENS");

    // =========================================================================
    // ROW 4: KEYBRD MOD section
    // =========================================================================
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawVerticalLine(L::kbdSep, (float)L::r4HeaderY, (float)(L::r4LabelY + L::r4LabelH));

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawFittedText("KEYBRD MOD",
                     L::kbdSep + 2, L::r4HeaderY,
                     L::kbdModFiltX + L::kbdSliderW - L::kbdSep - 4, L::r4HeaderH,
                     juce::Justification::centred, 1);

    // Bottom labels
    g.setFont(juce::Font(9.0f));
    auto kbdLabel = [&](juce::Component& c, const juce::String& text, int w = 44)
    {
        g.drawFittedText(text, c.getBounds().getCentreX() - w / 2, L::r4LabelY, w, L::r4LabelH,
                         juce::Justification::centred, 1);
    };
    kbdLabel(portaKnob,         "PORTA",      44);
    kbdLabel(legatoSwitch,      "LEGATO",     40);
    kbdLabel(bendOscTgtSwitch,  "OSC TGT",    44);
    kbdLabel(bendSensOscSlider, "BEND OSC",   48);
    kbdLabel(bendSensFiltSlider,"BEND FILT",  52);
    kbdLabel(modSensOscSlider,  "MOD OSC",    44);
    kbdLabel(modSensFiltSlider, "MOD FILT",   44);

    // LEGATO switch option labels
    {
        auto b = legatoSwitch.getBounds();
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("ON",  b.getRight() + 2, b.getY(),            28, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("OFF", b.getRight() + 2, b.getBottom() - 12,  28, 12, juce::Justification::centredLeft, 1);
    }

    // OSC TARGET switch option labels (top=OSC1, middle=BOTH, bottom=OSC2)
    {
        auto b = bendOscTgtSwitch.getBounds();
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("OSC1", b.getRight() + 2, b.getY(),              28, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("BOTH", b.getRight() + 2, b.getCentreY() - 6,   28, 12, juce::Justification::centredLeft, 1);
        g.drawFittedText("OSC2", b.getRight() + 2, b.getBottom() - 12,   28, 12, juce::Justification::centredLeft, 1);
    }

    // =========================================================================
    // ROW 4: ARP section
    // =========================================================================
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawVerticalLine(L::arpSep, (float)L::r4HeaderY, (float)(L::r4LabelY + L::r4LabelH));

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawFittedText("ARP",
                     L::arpSep + 2, L::r4HeaderY,
                     L::arpDivX + L::arpSwitchW - L::arpSep - 4, L::r4HeaderH,
                     juce::Justification::centred, 1);

    // Bottom labels
    g.setFont(juce::Font(9.0f));
    auto arpLabel = [&](juce::Component& c, const juce::String& text, int w = 40)
    {
        g.drawFittedText(text, c.getBounds().getCentreX() - w / 2, L::r4LabelY, w, L::r4LabelH,
                         juce::Justification::centred, 1);
    };
    arpLabel(arpModeSwitch,  "MODE");
    arpLabel(arpOctSwitch,   "OCT");
    arpLabel(arpClockSwitch, "CLOCK", 44);
    arpLabel(arpBpmSlider,   "BPM");
    arpLabel(arpDivSwitch,   "DIV");

    // MODE switch option labels (Off/Up/Down/Up+Down/Random)
    {
        auto b  = arpModeSwitch.getBounds();
        const int h5 = b.getHeight() / 4;  // 5 positions, 4 gaps
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("Rnd",    b.getRight() + 2, b.getY(),              32, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("U+D",    b.getRight() + 2, b.getY() + h5 - 5,    32, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("Down",   b.getRight() + 2, b.getY() + 2*h5 - 5,  32, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("Up",     b.getRight() + 2, b.getY() + 3*h5 - 5,  32, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("Off",    b.getRight() + 2, b.getBottom() - 11,   32, 11, juce::Justification::centredLeft, 1);
    }

    // OCT switch option labels (1/2/3/4 Oct)
    {
        auto b   = arpOctSwitch.getBounds();
        const int h3 = b.getHeight() / 3;
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("4",  b.getRight() + 2, b.getY(),            20, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("3",  b.getRight() + 2, b.getY() + h3 - 5,  20, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("2",  b.getRight() + 2, b.getY() + 2*h3-5,  20, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("1",  b.getRight() + 2, b.getBottom() - 11, 20, 11, juce::Justification::centredLeft, 1);
    }

    // CLOCK switch option labels (Internal/Host)
    {
        auto b = arpClockSwitch.getBounds();
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("Host", b.getRight() + 2, b.getY(),            32, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("Int",  b.getRight() + 2, b.getBottom() - 11, 32, 11, juce::Justification::centredLeft, 1);
    }

    // DIV switch option labels (1/4 / 1/8 / 1/16 / 1/32)
    {
        auto b   = arpDivSwitch.getBounds();
        const int h3 = b.getHeight() / 3;
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText("1/32", b.getRight() + 2, b.getY(),            28, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("1/16", b.getRight() + 2, b.getY() + h3 - 5,  28, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("1/8",  b.getRight() + 2, b.getY() + 2*h3-5,  28, 11, juce::Justification::centredLeft, 1);
        g.drawFittedText("1/4",  b.getRight() + 2, b.getBottom() - 11, 28, 11, juce::Justification::centredLeft, 1);
    }

    // =========================================================================
    // STATUS BAR
    // =========================================================================
    {
        const int sy = L::statusBarY;
        const int sh = L::statusBarH;
        g.setColour (juce::Colour (0xff111111));
        g.fillRect (0, sy, getWidth(), sh);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawHorizontalLine (sy, 0.0f, (float) getWidth());
    }
}

void Planet8AudioProcessorEditor::resized()
{
    namespace L = Layout;

    // ---- Patch strip --------------------------------------------------------
    {
        const int sy  = L::patchStripY;
        const int cy  = sy + (L::patchStripH - 24) / 2;  // vertically centred
        const int bh  = 24;

        patchDisplay .setBounds (10,  cy, 340, bh);
        patchPrevBtn .setBounds (356, cy,  22, bh);
        patchNextBtn .setBounds (382, cy,  22, bh);
        patchSaveBtn .setBounds (420, cy,  44, bh);
        patchInitBtn .setBounds (468, cy,  38, bh);
        patchCmpBtn   .setBounds (510, cy,  36, bh);
        patchDeleteBtn.setBounds (550, cy,  50, bh);
        patchUndoBtn  .setBounds (606, cy,  38, bh);
        patchRedoBtn  .setBounds (648, cy,  38, bh);
        patchBrowseBtn.setBounds (692, cy,  56, bh);

        // Browser panel: just below the strip, 720×290, always positioned to left
        patchBrowser->setBounds (0, sy + L::patchStripH + 2, 720, 290);
    }

    // ---- Row 1: OSC MOD group -----------------------------------------------
    oscLfoModSlider .setBounds(10,  L::r1CtrlY, 28, L::r1SliderH);
    oscEnvModSlider .setBounds(48,  L::r1CtrlY, 28, L::r1SliderH);
    modTargetSwitch .setBounds(82,  L::r1CtrlY, 20, L::r1SliderH);
    pulseWidthSlider.setBounds(133, L::r1CtrlY, 35, L::r1SliderH);
    pwModeSwitch    .setBounds(171, L::r1CtrlY, 20, L::r1SliderH);

    // ---- Row 1: OSC 1 / OSC 2 / MIX controls --------------------------------
    crossModSlider  .setBounds(228, L::r1CtrlY, 30, L::r1SliderH);
    waveSelector    .setBounds(280, L::r1CtrlY, 80, L::r1KnobH);
    octaveKnob      .setBounds(388, L::r1CtrlY, 80, L::r1KnobH);
    waveSelector2   .setBounds(506, L::r1CtrlY, 80, L::r1KnobH);
    octaveKnob2     .setBounds(626, L::r1CtrlY, 80, L::r1KnobH);
    osc2SyncSwitch  .setBounds(714, L::r1CtrlY + (L::r1SliderH - L::lfoSwitchH) / 2, 20, L::lfoSwitchH);
    fineTuneKnob2   .setBounds(764, L::r1CtrlY, 80, L::r1KnobH);
    oscMixSlider    .setBounds(866, L::r1CtrlY, 80, L::r1KnobH);

    // ---- Row 2: voice + filter controls ------------------------------------
    playModeSwitch  .setBounds( 20, L::r2CtrlY,       20,  90);
    hpfCutoffSlider .setBounds( 93, L::r2CtrlY,       30, L::r2SliderH);
    cutoffSlider    .setBounds(182, L::r2CtrlY,       30, L::r2SliderH);
    resonanceSlider .setBounds(218, L::r2CtrlY,       30, L::r2SliderH);
    slopeSwitch     .setBounds(268, L::r2CtrlY + 25,  20,  70);

    // New filter modulation controls (after slope switch, spread evenly before LFO section)
    const int filtSwitchY = L::r2CtrlY + (L::r2SliderH - L::lfoSwitchH) / 2;
    filtVelSensSlider.setBounds(341, L::r2CtrlY,  28, L::r2SliderH);
    filtEnvModSlider .setBounds(391, L::r2CtrlY,  28, L::r2SliderH);
    filtEnvSrcSwitch .setBounds(439, filtSwitchY,  20, L::lfoSwitchH);
    filtLfoModSlider .setBounds(478, L::r2CtrlY,  28, L::r2SliderH);
    filtKeyFlwSlider .setBounds(526, L::r2CtrlY,  28, L::r2SliderH);

    // ---- LFO controls (right side of Row 2) --------------------------------
    const int lfoSwitchY = L::r2CtrlY + (L::r2SliderH - L::lfoSwitchH) / 2;

    lfoRateSlider  .setBounds(L::lfoRateX,  L::r2CtrlY,  L::lfoSliderW, L::r2SliderH);
    lfoDelaySlider .setBounds(L::lfoDelayX, L::r2CtrlY,  L::lfoSliderW, L::r2SliderH);
    lfoWaveKnob    .setBounds(L::lfoWaveX,  L::r2CtrlY,  L::lfoWaveW,   L::r2SliderH - 30);
    lfoKeyTrigSwitch.setBounds(L::lfoKTX,  lfoSwitchY,   L::lfoSwitchW, L::lfoSwitchH);
    lfoTrigEnvSwitch.setBounds(L::lfoTEX,  lfoSwitchY,   L::lfoSwitchW, L::lfoSwitchH);

    // ---- Row 3: envelope controls ------------------------------------------
    // Key-follow switches are centred vertically within the slider column
    const int kfY = L::r3CtrlY + (L::r3SliderH - L::r3SwitchH) / 2;

    env1KeyFollowSwitch.setBounds(L::e1KFX, kfY,         L::envSwitchW, L::r3SwitchH);
    env1ASlider        .setBounds(L::e1AX,  L::r3CtrlY,  L::envSliderW, L::r3SliderH);
    env1DSlider        .setBounds(L::e1DX,  L::r3CtrlY,  L::envSliderW, L::r3SliderH);
    env1SSlider        .setBounds(L::e1SX,  L::r3CtrlY,  L::envSliderW, L::r3SliderH);
    env1RSlider        .setBounds(L::e1RX,  L::r3CtrlY,  L::envSliderW, L::r3SliderH);

    env2KeyFollowSwitch.setBounds(L::e2KFX, kfY,         L::envSwitchW, L::r3SwitchH);
    env2ASlider        .setBounds(L::e2AX,  L::r3CtrlY,  L::envSliderW, L::r3SliderH);
    env2DSlider        .setBounds(L::e2DX,  L::r3CtrlY,  L::envSliderW, L::r3SliderH);
    env2SSlider        .setBounds(L::e2SX,  L::r3CtrlY,  L::envSliderW, L::r3SliderH);
    env2RSlider        .setBounds(L::e2RX,  L::r3CtrlY,  L::envSliderW, L::r3SliderH);

    // ---- Row 4: VCA controls -----------------------------------------------
    vcaLevelSlider  .setBounds(L::vcaLevelX,   L::r4CtrlY, L::vcaSliderW, L::r4SliderH);
    vcaLfoModSlider .setBounds(L::vcaLfoModX,  L::r4CtrlY, L::vcaSliderW, L::r4SliderH);
    vcaVelSensSlider.setBounds(L::vcaVelSensX, L::r4CtrlY, L::vcaSliderW, L::r4SliderH);

    // ---- Row 4: KEYBRD MOD controls ----------------------------------------
    const int kbdSwitchY2 = L::r4CtrlY + (L::r4SliderH - L::lfoSwitchH) / 2;
    const int kbdTgt3Y    = L::r4CtrlY + (L::r4SliderH - 90) / 2;
    portaKnob       .setBounds(L::kbdPortaX,   L::r4CtrlY,  L::kbdPortaW,  80);
    legatoSwitch    .setBounds(L::kbdLegatoX,  kbdSwitchY2, 20, L::lfoSwitchH);
    bendOscTgtSwitch.setBounds(L::kbdOscTgtX,  kbdTgt3Y,    20, 90);
    bendSensOscSlider .setBounds(L::kbdBendOscX,  L::r4CtrlY, L::kbdSliderW, L::r4SliderH);
    bendSensFiltSlider.setBounds(L::kbdBendFiltX, L::r4CtrlY, L::kbdSliderW, L::r4SliderH);
    modSensOscSlider  .setBounds(L::kbdModOscX,   L::r4CtrlY, L::kbdSliderW, L::r4SliderH);
    modSensFiltSlider .setBounds(L::kbdModFiltX,  L::r4CtrlY, L::kbdSliderW, L::r4SliderH);

    // ---- Row 4: ARP controls -----------------------------------------------
    const int arpMode5Y  = L::r4CtrlY + (L::r4SliderH - 120) / 2;  // 5-pos: full height
    const int arpSwitch4Y = L::r4CtrlY + (L::r4SliderH - 80) / 2;  // 4-pos: 80px tall
    const int arpSwitch2Y = L::r4CtrlY + (L::r4SliderH - L::lfoSwitchH) / 2; // 2-pos: 45px

    arpModeSwitch .setBounds(L::arpModeX,  arpMode5Y,   L::arpSwitchW, 120);
    arpOctSwitch  .setBounds(L::arpOctX,   arpSwitch4Y, L::arpSwitchW, 80);
    arpClockSwitch.setBounds(L::arpClockX, arpSwitch2Y, L::arpSwitchW, L::lfoSwitchH);
    arpDivSwitch  .setBounds(L::arpDivX,   arpSwitch4Y, L::arpSwitchW, 80);

    arpBpmSlider .setBounds(L::arpBpmX, L::r4CtrlY, L::arpSliderW, L::r4SliderH);

    // Status bar label
    {
        const int cy = L::statusBarY + (L::statusBarH - 16) / 2;
        statusLabel.setBounds (10, cy, getWidth() - 20, 16);
    }
}
