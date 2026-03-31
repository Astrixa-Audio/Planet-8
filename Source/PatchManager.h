#pragma once
#include <JuceHeader.h>

//==============================================================================
struct PatchEntry
{
    juce::String    name;
    bool            isFactory = false;
    int             bankIndex = 0;   ///< index within its bank
    juce::ValueTree state;           ///< deep-copy of APVTS state
};

class Planet8AudioProcessor;

//==============================================================================
class PatchManager : public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PatchManager (Planet8AudioProcessor& proc);
    ~PatchManager() override;

    /// Call once after processor construction. Loads factory + user banks.
    void loadBanks();

    // ---- Patch navigation ---------------------------------------------------
    int getNumPatches()    const;
    int numFactoryPatches() const { return factoryPatches.size(); }
    int numUserPatches()    const { return userPatches.size(); }

    /// Returns patch by global index (factory first, then user).
    const PatchEntry& getPatch (int globalIndex) const;

    void loadPatch     (int globalIndex);
    void loadNextPatch ();
    void loadPrevPatch ();

    // ---- User-bank management -----------------------------------------------
    /// Appends (userSlot == -1) or overwrites the given user slot.
    void saveCurrentAsUserPatch (const juce::String& name, int userSlot = -1);
    void deleteUserPatch (int userSlot);
    void initCurrentPatch ();

    // ---- State queries ------------------------------------------------------
    juce::String getCurrentPatchName () const;
    bool         currentPatchIsFactory () const;
    int          getCurrentGlobalIndex () const { return currentGlobalIndex; }
    int          getCurrentBankIndex   () const;
    bool         isModified            () const { return modifiedSinceLoad; }

    // ---- Compare ------------------------------------------------------------
    void toggleCompare ();
    bool isComparing   () const { return compareActive; }

    // ---- Undo / Redo --------------------------------------------------------
    void undo    ();
    void redo    ();
    bool canUndo () const;
    bool canRedo () const;

    // ---- DAW state recall ---------------------------------------------------
    void getStateXml     (juce::MemoryBlock& dest) const;
    void setStateFromXml (const void* data, int sizeInBytes);

    // ---- Patch bank access (for browser) ------------------------------------
    const juce::Array<PatchEntry>& getFactoryPatches () const { return factoryPatches; }
    const juce::Array<PatchEntry>& getUserPatches    () const { return userPatches;    }

    // ---- AudioProcessorValueTreeState::Listener ----------------------------
    void parameterChanged (const juce::String& paramID, float newValue) override;

private:
    Planet8AudioProcessor& processor;

    juce::Array<PatchEntry> factoryPatches;
    juce::Array<PatchEntry> userPatches;

    int  currentGlobalIndex = 0;
    bool modifiedSinceLoad  = false;
    bool ignoreParamChanges = false;

    bool            compareActive   = false;
    juce::ValueTree compareSnapshot;           ///< saved edited state for Compare

    juce::File getUserPatchFile   () const;
    void       saveUserBankToDisk ();
    void       parseXmlBank       (const juce::XmlElement& root, bool isFactory);
    void       applyState         (const juce::ValueTree& tree);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchManager)
};
