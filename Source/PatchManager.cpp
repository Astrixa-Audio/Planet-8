#include "PatchManager.h"
#include "PluginProcessor.h"

// If Presets.xml has been added to the Projucer as a binary resource, the
// BinaryData header will define BinaryData::Presets_xml and Presets_xmlSize.
// If not present, the factory bank simply starts empty (graceful fallback).
#if __has_include(<BinaryData.h>)
  #include <BinaryData.h>
#endif

//==============================================================================
PatchManager::PatchManager (Planet8AudioProcessor& proc)
    : processor (proc)
{
}

PatchManager::~PatchManager()
{
    for (auto* p : processor.getParameters())
        if (auto* rap = dynamic_cast<juce::RangedAudioParameter*> (p))
            processor.apvts.removeParameterListener (rap->getParameterID(), this);
}

//==============================================================================
void PatchManager::loadBanks()
{
    // ---- Factory bank from BinaryData::Presets_xml --------------------------
  #if __has_include(<BinaryData.h>)
    if (BinaryData::Presets_xmlSize > 0)
    {
        juce::String xmlStr (BinaryData::Presets_xml,
                             (size_t) BinaryData::Presets_xmlSize);
        if (auto xml = juce::XmlDocument::parse (xmlStr))
            parseXmlBank (*xml, true);
    }
  #endif

    // If no factory patches available, create a single "Init" factory patch
    // from the current APVTS defaults so there is always something to navigate.
    if (factoryPatches.isEmpty())
    {
        PatchEntry init;
        init.name      = "Init";
        init.isFactory = true;
        init.bankIndex = 0;
        init.state     = processor.apvts.copyState();
        factoryPatches.add (std::move (init));
    }

    // ---- User bank from ~/Documents/Planet-8/UserPatches.xml ---------------
    auto userFile = getUserPatchFile();
    if (userFile.existsAsFile())
        if (auto xml = juce::XmlDocument::parse (userFile))
            parseXmlBank (*xml, false);

    // Register parameter listeners for modification detection
    for (auto* p : processor.getParameters())
        if (auto* rap = dynamic_cast<juce::RangedAudioParameter*> (p))
            processor.apvts.addParameterListener (rap->getParameterID(), this);

    // Load the first patch on startup
    loadPatch (0);
}

//==============================================================================
void PatchManager::parseXmlBank (const juce::XmlElement& root, bool isFactory)
{
    auto& bank = isFactory ? factoryPatches : userPatches;
    int   idx  = 0;

    for (auto* patchXml : root.getChildIterator())
    {
        if (patchXml->getTagName() != "Patch") continue;
        auto* stateXml = patchXml->getFirstChildElement();
        if (stateXml == nullptr) continue;

        PatchEntry entry;
        entry.name      = patchXml->getStringAttribute ("name",
                              juce::String (isFactory ? "F" : "U") + juce::String (idx + 1));
        entry.isFactory = isFactory;
        entry.bankIndex = idx;
        entry.state     = juce::ValueTree::fromXml (*stateXml);
        bank.add (std::move (entry));
        ++idx;
    }
}

//==============================================================================
int PatchManager::getNumPatches() const
{
    return factoryPatches.size() + userPatches.size();
}

const PatchEntry& PatchManager::getPatch (int globalIndex) const
{
    jassert (globalIndex >= 0 && globalIndex < getNumPatches());
    if (globalIndex < factoryPatches.size())
        return factoryPatches.getReference (globalIndex);
    return userPatches.getReference (globalIndex - factoryPatches.size());
}

//==============================================================================
void PatchManager::loadPatch (int globalIndex)
{
    if (globalIndex < 0 || globalIndex >= getNumPatches()) return;

    const auto& entry = getPatch (globalIndex);
    if (!entry.state.isValid()) return;

    ignoreParamChanges = true;
    currentGlobalIndex = globalIndex;
    applyState (entry.state.createCopy());
    modifiedSinceLoad  = false;
    compareActive      = false;
    ignoreParamChanges = false;
}

void PatchManager::loadNextPatch()
{
    loadPatch ((currentGlobalIndex + 1) % getNumPatches());
}

void PatchManager::loadPrevPatch()
{
    loadPatch ((currentGlobalIndex - 1 + getNumPatches()) % getNumPatches());
}

void PatchManager::applyState (const juce::ValueTree& tree)
{
    if (tree.isValid())
        processor.apvts.replaceState (tree);
}

//==============================================================================
void PatchManager::saveCurrentAsUserPatch (const juce::String& name, int userSlot)
{
    PatchEntry entry;
    entry.name      = name.trim().isEmpty() ? "Unnamed" : name.trim();
    entry.isFactory = false;
    entry.state     = processor.apvts.copyState();

    if (userSlot < 0 || userSlot >= userPatches.size())
    {
        entry.bankIndex = userPatches.size();
        userPatches.add (std::move (entry));
        currentGlobalIndex = factoryPatches.size() + userPatches.size() - 1;
    }
    else
    {
        entry.bankIndex = userSlot;
        userPatches.set (userSlot, std::move (entry));
        currentGlobalIndex = factoryPatches.size() + userSlot;
    }

    modifiedSinceLoad = false;
    saveUserBankToDisk();
}

void PatchManager::deleteUserPatch (int userSlot)
{
    if (userSlot < 0 || userSlot >= userPatches.size()) return;

    userPatches.remove (userSlot);
    for (int i = 0; i < userPatches.size(); ++i)
        userPatches.getReference (i).bankIndex = i;

    currentGlobalIndex = juce::jlimit (0, juce::jmax (0, getNumPatches() - 1),
                                       currentGlobalIndex);
    saveUserBankToDisk();
}

void PatchManager::initCurrentPatch()
{
    ignoreParamChanges = true;
    for (auto* p : processor.getParameters())
        p->setValueNotifyingHost (p->getDefaultValue());
    modifiedSinceLoad  = true;   // init != any saved patch
    ignoreParamChanges = false;
}

//==============================================================================
juce::String PatchManager::getCurrentPatchName() const
{
    if (getNumPatches() == 0) return "No Patch";
    return getPatch (currentGlobalIndex).name;
}

bool PatchManager::currentPatchIsFactory() const
{
    return currentGlobalIndex < factoryPatches.size();
}

int PatchManager::getCurrentBankIndex() const
{
    if (currentPatchIsFactory())
        return currentGlobalIndex;
    return currentGlobalIndex - factoryPatches.size();
}

//==============================================================================
void PatchManager::toggleCompare()
{
    if (!compareActive)
    {
        // Save edited state, then restore the last-loaded patch state
        compareSnapshot = processor.apvts.copyState();
        const auto& entry = getPatch (currentGlobalIndex);
        if (entry.state.isValid())
        {
            ignoreParamChanges = true;
            applyState (entry.state.createCopy());
            ignoreParamChanges = false;
        }
        compareActive = true;
    }
    else
    {
        // Restore the edited state
        if (compareSnapshot.isValid())
        {
            ignoreParamChanges = true;
            applyState (compareSnapshot.createCopy());
            ignoreParamChanges = false;
        }
        compareActive = false;
    }
}

//==============================================================================
void PatchManager::undo()
{
    if (auto* um = processor.apvts.undoManager)
        um->undo();
}

void PatchManager::redo()
{
    if (auto* um = processor.apvts.undoManager)
        um->redo();
}

bool PatchManager::canUndo() const
{
    if (auto* um = processor.apvts.undoManager)
        return um->canUndo();
    return false;
}

bool PatchManager::canRedo() const
{
    if (auto* um = processor.apvts.undoManager)
        return um->canRedo();
    return false;
}

//==============================================================================
void PatchManager::parameterChanged (const juce::String& /*paramID*/, float /*newValue*/)
{
    if (!ignoreParamChanges)
        modifiedSinceLoad = true;
}

//==============================================================================
void PatchManager::getStateXml (juce::MemoryBlock& dest) const
{
    auto xml = std::make_unique<juce::XmlElement> ("Planet8State");
    if (auto stateXml = processor.apvts.copyState().createXml())
        xml->addChildElement (stateXml.release());
    xml->setAttribute ("patchIndex",     currentGlobalIndex);
    xml->setAttribute ("patchIsFactory", currentPatchIsFactory() ? 1 : 0);
    juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void PatchManager::setStateFromXml (const void* data, int sizeInBytes)
{
    if (auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes))
    {
        // Restore the patch display position
        int savedIndex = xml->getIntAttribute ("patchIndex", 0);
        int clampedIndex = juce::jlimit (0, juce::jmax (0, getNumPatches() - 1), savedIndex);
        loadPatch (clampedIndex);

        // Then restore the exact APVTS state saved at close (may include unsaved tweaks)
        if (auto* stateXml = xml->getFirstChildElement())
        {
            auto tree = juce::ValueTree::fromXml (*stateXml);
            if (tree.isValid())
            {
                ignoreParamChanges = true;
                processor.apvts.replaceState (tree);
                ignoreParamChanges = false;

                // Mark as modified if the restored state differs from the patch's stored state
                const auto& patchState = getPatch (clampedIndex).state;
                auto restoredXml = tree.createXml();
                auto patchXml    = patchState.createXml();
                modifiedSinceLoad = (restoredXml && patchXml)
                                    && (restoredXml->toString() != patchXml->toString());
            }
        }
    }
}

//==============================================================================
juce::File PatchManager::getUserPatchFile() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Planet-8")
               .getChildFile ("UserPatches.xml");
}

void PatchManager::saveUserBankToDisk()
{
    auto userFile = getUserPatchFile();
    userFile.getParentDirectory().createDirectory();

    auto root = std::make_unique<juce::XmlElement> ("Patches");
    for (const auto& entry : userPatches)
    {
        auto* patchXml = root->createNewChildElement ("Patch");
        patchXml->setAttribute ("name", entry.name);
        if (entry.state.isValid())
            if (auto stateXml = entry.state.createXml())
                patchXml->addChildElement (stateXml.release());
    }

    // Atomic write: write to .tmp then rename
    auto tmpFile = userFile.withFileExtension (".tmp");
    if (root->writeTo (tmpFile))
        tmpFile.moveFileTo (userFile);
}
