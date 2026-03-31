/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Oscillator.h"
#include "Filter.h"
#include "Envelope.h"
#include "LFO.h"
#include "PatchManager.h"

static constexpr int NUM_VOICES = 8;

struct Voice
{
    Oscillator osc1;
    Oscillator osc2;
    int   midiNote  = 60;
    bool  active    = false;
    bool  held      = false;
    int   timestamp = 0;
    float velocity  = 1.0f;   ///< normalised MIDI velocity [0, 1] for VEL SENS
    Envelope env1;             ///< ENV1 — disconnected, reserved for future use
    Envelope env2;             ///< ENV2 — per-voice VCA envelope
    float prevOsc1Phase = 0.0f;  ///< used to detect OSC1 cycle reset for hard sync
    juce::SmoothedValue<float> portaSmoothed;  ///< per-voice portamento frequency smoother
};

//==============================================================================
/**
*/
class Planet8AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    Planet8AudioProcessor();
    ~Planet8AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();
    juce::UndoManager undoManager;                 ///< must be declared before apvts
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<PatchManager> patchManager;

private:
    //==============================================================================
    Voice voices[NUM_VOICES];
    juce::Array<int> noteStack;
    int voiceTimestamp = 0;
    int prevPlayMode   = -1;

    // Oscillator / mix params
    std::atomic<float>* waveParam      = nullptr;
    std::atomic<float>* oscMixParam    = nullptr;
    std::atomic<float>* osc2FineParam  = nullptr;
    std::atomic<float>* playModeParam  = nullptr;

    // Filter params
    std::atomic<float>* cutoffParam      = nullptr;
    std::atomic<float>* resonanceParam   = nullptr;
    std::atomic<float>* hpfCutoffParam   = nullptr;
    std::atomic<float>* slopeParam       = nullptr;
    std::atomic<float>* filtVelSensParam = nullptr;
    std::atomic<float>* filtEnvModParam  = nullptr;
    std::atomic<float>* filtEnvSrcParam  = nullptr;
    std::atomic<float>* filtLfoModParam  = nullptr;
    std::atomic<float>* filtKeyFlwParam  = nullptr;

    // ENV1 params
    std::atomic<float>* env1AParam  = nullptr;
    std::atomic<float>* env1DParam  = nullptr;
    std::atomic<float>* env1SParam  = nullptr;
    std::atomic<float>* env1RParam  = nullptr;
    std::atomic<float>* env1KFParam = nullptr;

    // ENV2 params
    std::atomic<float>* env2AParam  = nullptr;
    std::atomic<float>* env2DParam  = nullptr;
    std::atomic<float>* env2SParam  = nullptr;
    std::atomic<float>* env2RParam  = nullptr;
    std::atomic<float>* env2KFParam = nullptr;

    juce::SmoothedValue<float> smoothedPulseWidth;
    LadderFilter filter;
    Envelope filterEnv;   ///< VCF envelope — ADSR sourced from ENV1 or ENV2 per FILT_ENV_SRC
    LFO lfo;
    int   filterCurrentNote = 60;   ///< most-recent MIDI note for key follow
    float filterCurrentVel  = 1.0f; ///< most-recent normalised velocity for filter vel sens

    // LFO params
    std::atomic<float>* lfoRateParam    = nullptr;
    std::atomic<float>* lfoDelayParam   = nullptr;
    std::atomic<float>* lfoWaveParam    = nullptr;
    std::atomic<float>* lfoKeyTrigParam = nullptr;
    std::atomic<float>* lfoTrigEnvParam = nullptr;

    // VCA params
    std::atomic<float>* vcaLevelParam   = nullptr;
    std::atomic<float>* vcaLfoModParam  = nullptr;
    std::atomic<float>* vcaVelSensParam = nullptr;

    // OSC Mod params
    std::atomic<float>* oscLfoModParam = nullptr;
    std::atomic<float>* oscEnvModParam = nullptr;
    std::atomic<float>* pwModeParam    = nullptr;
    std::atomic<float>* crossModParam  = nullptr;
    std::atomic<float>* osc2SyncParam  = nullptr;

    // KEYBRD MOD params
    std::atomic<float>* portaParam        = nullptr;
    std::atomic<float>* legatoParam       = nullptr;
    std::atomic<float>* bendOscTgtParam   = nullptr;
    std::atomic<float>* bendSensOscParam  = nullptr;
    std::atomic<float>* bendSensFiltParam = nullptr;
    std::atomic<float>* modSensOscParam   = nullptr;
    std::atomic<float>* modSensFiltParam  = nullptr;

    float pitchBendNorm = 0.0f;  ///< current pitch bend wheel value [-1, +1]
    float modWheelNorm  = 0.0f;  ///< current mod wheel value [0, 1]
    float lastNoteFreq  = 261.63f; ///< frequency of the most recently triggered note — portamento start point

    // ARP params
    std::atomic<float>* arpModeParam  = nullptr;
    std::atomic<float>* arpOctParam   = nullptr;
    std::atomic<float>* arpClockParam = nullptr;
    std::atomic<float>* arpBpmParam   = nullptr;
    std::atomic<float>* arpDivParam   = nullptr;

    // ARP state
    juce::Array<int>  arpHeldNotes;
    juce::Array<int>  arpPattern;
    int               arpStepIndex   = 0;
    int               arpCurrentNote = -1;
    bool              arpGateOpen    = false;
    int64_t           arpSamplePos   = 0;
    int               prevArpMode    = 0;
    juce::Random      arpRandom;

    void buildArpPattern (int mode, int numOct);
    int findFreeVoice() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Planet8AudioProcessor)
};
