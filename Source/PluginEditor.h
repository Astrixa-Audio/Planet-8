/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/// Floating panel showing a searchable list of all patches (factory + user).
class PatchBrowserPanel : public juce::Component,
                          public juce::ListBoxModel
{
public:
    explicit PatchBrowserPanel (Planet8AudioProcessor& proc);

    void paint   (juce::Graphics& g)    override;
    void resized ()                     override;

    // ListBoxModel
    int  getNumRows () override;
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemClicked       (int row, const juce::MouseEvent& e) override;

    /// Rebuild filtered list from current search text.
    void refreshList ();

private:
    Planet8AudioProcessor& proc;
    juce::TextEditor searchBox;
    juce::ListBox    patchList  { "patches", this };

    /// Maps visible ListBox row → global patch index in PatchManager.
    juce::Array<int> filteredIndices;
    int              factoryRowCount = 0;  ///< # of factory rows in filtered list

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchBrowserPanel)
};

//==============================================================================
class Planet8AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    Planet8AudioProcessorEditor (Planet8AudioProcessor&);
    ~Planet8AudioProcessorEditor() override;

    //==============================================================================
    void paint   (juce::Graphics&) override;
    void resized ()                override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback      () override;
    void updatePatchDisplay ();

    Planet8AudioProcessor& audioProcessor;

    // ---- PATCH STRIP ---------------------------------------------------------
    juce::Label      patchDisplay;
    juce::TextButton patchPrevBtn   { "<"      };
    juce::TextButton patchNextBtn   { ">"      };
    juce::TextButton patchSaveBtn   { "Save"   };
    juce::TextButton patchInitBtn   { "Init"   };
    juce::TextButton patchCmpBtn    { "Cmp"    };
    juce::TextButton patchDeleteBtn { "Delete" };
    juce::TextButton patchUndoBtn   { "Undo"   };
    juce::TextButton patchRedoBtn   { "Redo"   };
    juce::TextButton patchBrowseBtn { "Browse" };

    std::unique_ptr<PatchBrowserPanel> patchBrowser;
    bool patchBrowserVisible = false;

    // ---- OSC MOD -------------------------------------------------------------
    juce::Slider oscLfoModSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> oscLfoModAttachment;

    juce::Slider oscEnvModSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> oscEnvModAttachment;

    juce::Slider pwModeSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pwModeAttachment;

    // ---- OSC 1 ---------------------------------------------------------------
    juce::Slider crossModSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> crossModAttachment;

    juce::Slider waveSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> waveAttachment;

    juce::Slider pulseWidthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulseWidthAttachment;

    juce::Slider octaveKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;

    juce::Slider modTargetSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modTargetAttachment;

    // ---- OSC 2 ---------------------------------------------------------------
    juce::Slider osc2SyncSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> osc2SyncAttachment;

    juce::Slider waveSelector2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> waveAttachment2;

    juce::Slider octaveKnob2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment2;

    juce::Slider fineTuneKnob2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fineAttachment2;

    // ---- Mix -----------------------------------------------------------------
    juce::Slider oscMixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> oscMixAttachment;

    // ---- Voice / Filter ------------------------------------------------------
    juce::Slider playModeSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> playModeAttachment;

    juce::Slider hpfCutoffSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfCutoffAttachment;

    juce::Slider cutoffSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;

    juce::Slider resonanceSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;

    juce::Slider slopeSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slopeAttachment;

    juce::Slider filtVelSensSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filtVelSensAttachment;

    juce::Slider filtEnvModSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filtEnvModAttachment;

    juce::Slider filtEnvSrcSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filtEnvSrcAttachment;

    juce::Slider filtLfoModSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filtLfoModAttachment;

    juce::Slider filtKeyFlwSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filtKeyFlwAttachment;

    // ---- KEYBRD MOD ----------------------------------------------------------
    juce::Slider portaKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> portaAttachment;

    juce::Slider legatoSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> legatoAttachment;

    juce::Slider bendOscTgtSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bendOscTgtAttachment;

    juce::Slider bendSensOscSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bendSensOscAttachment;

    juce::Slider bendSensFiltSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bendSensFiltAttachment;

    juce::Slider modSensOscSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modSensOscAttachment;

    juce::Slider modSensFiltSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modSensFiltAttachment;

    // ---- ENV 1 ---------------------------------------------------------------
    juce::Slider env1KeyFollowSwitch;
    juce::Slider env1ASlider;
    juce::Slider env1DSlider;
    juce::Slider env1SSlider;
    juce::Slider env1RSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env1KFAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env1AAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env1DAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env1SAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env1RAttachment;

    // ---- ENV 2 ---------------------------------------------------------------
    juce::Slider env2KeyFollowSwitch;
    juce::Slider env2ASlider;
    juce::Slider env2DSlider;
    juce::Slider env2SSlider;
    juce::Slider env2RSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env2KFAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env2AAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env2DAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env2SAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> env2RAttachment;

    // ---- LFO -----------------------------------------------------------------
    juce::Slider lfoRateSlider;
    juce::Slider lfoDelaySlider;
    juce::Slider lfoWaveKnob;          ///< Rotary selector with waveform glyphs
    juce::Slider lfoKeyTrigSwitch;
    juce::Slider lfoTrigEnvSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoWaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoKeyTrigAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoTrigEnvAttachment;

    // ---- VCA -----------------------------------------------------------------
    juce::Slider vcaLevelSlider;
    juce::Slider vcaLfoModSlider;
    juce::Slider vcaVelSensSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vcaLevelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vcaLfoModAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vcaVelSensAttachment;

    // ---- ARP -----------------------------------------------------------------
    juce::Slider arpModeSwitch;
    juce::Slider arpOctSwitch;
    juce::Slider arpClockSwitch;
    juce::Slider arpBpmSlider;
    juce::Slider arpDivSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpOctAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpClockAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpBpmAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpDivAttachment;

    // ---- STATUS BAR ----------------------------------------------------------
    juce::Label statusLabel;
    void attachStatus (juce::Slider& s, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Planet8AudioProcessorEditor)
};
