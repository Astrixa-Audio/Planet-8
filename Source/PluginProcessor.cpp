/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Planet8AudioProcessor::Planet8AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    apvts(*this, &undoManager, "PARAMS", createParams())
#endif
{
    patchManager = std::make_unique<PatchManager>(*this);

    waveParam      = apvts.getRawParameterValue("WAVE");
    osc2FineParam  = apvts.getRawParameterValue("OSC2_FINE");
    oscMixParam    = apvts.getRawParameterValue("OSC_MIX");
    playModeParam  = apvts.getRawParameterValue("PLAY_MODE");
    cutoffParam    = apvts.getRawParameterValue("CUTOFF");
    resonanceParam = apvts.getRawParameterValue("RESONANCE");
    hpfCutoffParam = apvts.getRawParameterValue("HPF_CUTOFF");
    slopeParam     = apvts.getRawParameterValue("SLOPE");

    env1AParam  = apvts.getRawParameterValue("ENV1_A");
    env1DParam  = apvts.getRawParameterValue("ENV1_D");
    env1SParam  = apvts.getRawParameterValue("ENV1_S");
    env1RParam  = apvts.getRawParameterValue("ENV1_R");
    env1KFParam = apvts.getRawParameterValue("ENV1_KEYFOLLOW");

    env2AParam  = apvts.getRawParameterValue("ENV2_A");
    env2DParam  = apvts.getRawParameterValue("ENV2_D");
    env2SParam  = apvts.getRawParameterValue("ENV2_S");
    env2RParam  = apvts.getRawParameterValue("ENV2_R");
    env2KFParam = apvts.getRawParameterValue("ENV2_KEYFOLLOW");

    lfoRateParam    = apvts.getRawParameterValue("LFO_RATE");
    lfoDelayParam   = apvts.getRawParameterValue("LFO_DELAY");
    lfoWaveParam    = apvts.getRawParameterValue("LFO_WAVE");
    lfoKeyTrigParam = apvts.getRawParameterValue("LFO_KEYTRIG");
    lfoTrigEnvParam = apvts.getRawParameterValue("LFO_TRIGENV");

    vcaLevelParam   = apvts.getRawParameterValue("VCA_LEVEL");
    vcaLfoModParam  = apvts.getRawParameterValue("VCA_LFO_MOD");
    vcaVelSensParam = apvts.getRawParameterValue("VCA_VEL_SENS");

    oscLfoModParam = apvts.getRawParameterValue("OSC_LFO_MOD");
    oscEnvModParam = apvts.getRawParameterValue("OSC_ENV_MOD");
    pwModeParam    = apvts.getRawParameterValue("PW_MODE");
    crossModParam  = apvts.getRawParameterValue("CROSS_MOD");
    osc2SyncParam  = apvts.getRawParameterValue("OSC2_SYNC");

    portaParam        = apvts.getRawParameterValue("PORTAMENTO");
    legatoParam       = apvts.getRawParameterValue("LEGATO");
    bendOscTgtParam   = apvts.getRawParameterValue("BEND_OSC_TGT");
    bendSensOscParam  = apvts.getRawParameterValue("BEND_SENS_OSC");
    bendSensFiltParam = apvts.getRawParameterValue("BEND_SENS_FILT");
    modSensOscParam   = apvts.getRawParameterValue("MOD_SENS_OSC");
    modSensFiltParam  = apvts.getRawParameterValue("MOD_SENS_FILT");

    filtVelSensParam = apvts.getRawParameterValue("FILT_VEL_SENS");
    filtEnvModParam  = apvts.getRawParameterValue("FILT_ENV_MOD");
    filtEnvSrcParam  = apvts.getRawParameterValue("FILT_ENV_SRC");
    filtLfoModParam  = apvts.getRawParameterValue("FILT_LFO_MOD");
    filtKeyFlwParam  = apvts.getRawParameterValue("FILT_KEY_FLW");

    arpModeParam  = apvts.getRawParameterValue("ARP_MODE");
    arpOctParam   = apvts.getRawParameterValue("ARP_OCT");
    arpClockParam = apvts.getRawParameterValue("ARP_CLOCK");
    arpBpmParam   = apvts.getRawParameterValue("ARP_BPM");
    arpDivParam   = apvts.getRawParameterValue("ARP_DIV");

    patchManager->loadBanks();
}

Planet8AudioProcessor::~Planet8AudioProcessor()
{
}

//==============================================================================
const juce::String Planet8AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Planet8AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Planet8AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Planet8AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Planet8AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Planet8AudioProcessor::getNumPrograms()
{
    return 1;
}

int Planet8AudioProcessor::getCurrentProgram()
{
    return 0;
}

void Planet8AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Planet8AudioProcessor::getProgramName (int index)
{
    return {};
}

void Planet8AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Planet8AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int v = 0; v < NUM_VOICES; ++v)
    {
        voices[v].osc1.prepare(sampleRate);
        voices[v].osc2.prepare(sampleRate);
        voices[v].env1.prepare(sampleRate);
        voices[v].env2.prepare(sampleRate);
        voices[v].active = false;
        voices[v].held   = false;
        voices[v].portaSmoothed.setCurrentAndTargetValue(
            (float)juce::MidiMessage::getMidiNoteInHertz(60));
    }

    lastNoteFreq = (float)juce::MidiMessage::getMidiNoteInHertz(60);
    smoothedPulseWidth.reset(sampleRate, 0.02);
    filter.prepare(sampleRate);
    filterEnv.prepare(sampleRate);
    lfo.prepare(sampleRate);

    arpHeldNotes.clear();
    arpPattern.clear();
    arpStepIndex   = 0;
    arpCurrentNote = -1;
    arpGateOpen    = false;
    arpSamplePos   = 0;
    prevArpMode    = 0;
}

void Planet8AudioProcessor::releaseResources()
{
}

//==============================================================================
int Planet8AudioProcessor::findFreeVoice() const
{
    // 1. Prefer an inactive voice
    for (int v = 0; v < NUM_VOICES; ++v)
        if (!voices[v].active) return v;

    // 2. Prefer the oldest releasing voice (held=false)
    int oldest = -1;
    for (int v = 0; v < NUM_VOICES; ++v)
        if (!voices[v].held && (oldest == -1 || voices[v].timestamp < voices[oldest].timestamp))
            oldest = v;
    if (oldest != -1) return oldest;

    // 3. Fall back to the oldest held voice
    oldest = 0;
    for (int v = 1; v < NUM_VOICES; ++v)
        if (voices[v].timestamp < voices[oldest].timestamp)
            oldest = v;
    return oldest;
}

//==============================================================================
void Planet8AudioProcessor::buildArpPattern (int mode, int numOct)
{
    arpPattern.clear();
    if (arpHeldNotes.isEmpty()) return;

    juce::Array<int> base = arpHeldNotes;
    base.sort();

    juce::Array<int> fullUp;
    for (int oct = 0; oct < numOct; ++oct)
        for (int n : base)
            fullUp.add (n + oct * 12);

    switch (mode)
    {
        case 1: // Up
            arpPattern = fullUp;
            break;
        case 2: // Down
            for (int i = fullUp.size() - 1; i >= 0; --i)
                arpPattern.add (fullUp[i]);
            break;
        case 3: // Up+Down (ping-pong, no repeat at extremes)
            arpPattern = fullUp;
            for (int i = fullUp.size() - 2; i >= 1; --i)
                arpPattern.add (fullUp[i]);
            break;
        case 4: // Random — shuffle
            arpPattern = fullUp;
            for (int i = arpPattern.size() - 1; i > 0; --i)
            {
                int j = arpRandom.nextInt (i + 1);
                std::swap (arpPattern.getReference (i), arpPattern.getReference (j));
            }
            break;
        default:
            break;
    }
}

//==============================================================================
#ifndef JucePlugin_PreferredChannelConfigurations
bool Planet8AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void Planet8AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto rawOct = apvts.getRawParameterValue("OCT");
    int octave = rawOct ? (int)rawOct->load() : 1;

    auto rawPW = apvts.getRawParameterValue("PW");
    smoothedPulseWidth.setTargetValue(rawPW ? rawPW->load() : 0.5f);

    auto rawWave2 = apvts.getRawParameterValue("WAVE2");
    int wave2 = rawWave2 ? (int)rawWave2->load() : 3;

    auto rawOct2 = apvts.getRawParameterValue("OCT2");
    float semitones2 = rawOct2 ? rawOct2->load() : 0.0f;

    auto rawModTarget = apvts.getRawParameterValue("MOD_TARGET");
    int modTarget = rawModTarget ? (int)rawModTarget->load() : 2;

    const int pwMode = pwModeParam ? (int)pwModeParam->load() : 3;  // 3=MAN, 2=LFO, 1=ENV1, 0=ENV2

    int wave     = (int)waveParam->load();
    int playMode = playModeParam ? (int)playModeParam->load() : 0;

    // Filter parameter targets (smoothing handled inside LadderFilter)
    if (cutoffParam)    filter.setCutoff    (cutoffParam->load());
    if (resonanceParam) filter.setResonance (resonanceParam->load());
    if (hpfCutoffParam) filter.setHPFCutoff (hpfCutoffParam->load());
    if (slopeParam)     filter.setSlope     ((int)slopeParam->load() == 1);

    // Read envelope parameters once per buffer
    const float env1A  = env1AParam  ? env1AParam->load()  : 2.0f;
    const float env1D  = env1DParam  ? env1DParam->load()  : 200.0f;
    const float env1S  = env1SParam  ? env1SParam->load()  : 0.7f;
    const float env1R  = env1RParam  ? env1RParam->load()  : 300.0f;
    const bool  env1KF = env1KFParam ? (int)env1KFParam->load() == 1 : false;

    const float env2A  = env2AParam  ? env2AParam->load()  : 2.0f;
    const float env2D  = env2DParam  ? env2DParam->load()  : 500.0f;
    const float env2S  = env2SParam  ? env2SParam->load()  : 0.3f;
    const float env2R  = env2RParam  ? env2RParam->load()  : 500.0f;
    const bool  env2KF = env2KFParam ? (int)env2KFParam->load() == 1 : false;

    // Read LFO parameters once per buffer and push to the LFO object
    lfo.setRate      (lfoRateParam    ? lfoRateParam->load()              : 1.0f);
    lfo.setDelayTime (lfoDelayParam   ? lfoDelayParam->load()             : 0.0f);
    lfo.setWave      (lfoWaveParam    ? (int)lfoWaveParam->load()         : 0);
    lfo.setKeyTrig   (lfoKeyTrigParam ? (int)lfoKeyTrigParam->load() == 1 : false);
    lfo.setTrigEnv   (lfoTrigEnvParam ? (int)lfoTrigEnvParam->load() == 1 : false);

    // Read VCA parameters once per buffer
    const float vcaLevel   = vcaLevelParam   ? vcaLevelParam->load()   : 0.8f;
    const float vcaLfoMod  = vcaLfoModParam  ? vcaLfoModParam->load()  : 0.0f;
    const float vcaVelSens = vcaVelSensParam ? vcaVelSensParam->load() : 0.5f;

    const float oscLfoMod = oscLfoModParam ? oscLfoModParam->load() : 0.0f;
    const float oscEnvMod = oscEnvModParam ? oscEnvModParam->load() : 0.0f;
    const float crossMod         = crossModParam  ? crossModParam->load()  : 0.0f;
    const bool  osc2Sync         = osc2SyncParam  ? (int)osc2SyncParam->load() == 1 : false;

    // Filter modulation parameters
    const float filtVelSens = filtVelSensParam ? filtVelSensParam->load() : 0.0f;
    const float filtEnvMod  = filtEnvModParam  ? filtEnvModParam->load()  : 0.0f;
    const int   filtEnvSrc  = filtEnvSrcParam  ? (int)filtEnvSrcParam->load() : 0;
    const float filtLfoMod  = filtLfoModParam  ? filtLfoModParam->load()  : 0.0f;
    const float filtKeyFlw  = filtKeyFlwParam  ? filtKeyFlwParam->load()  : 0.0f;

    // KEYBRD MOD parameters
    const float portaTimeMs  = portaParam        ? portaParam->load()               : 0.0f;
    const bool  legatoMode   = legatoParam       ? (int)legatoParam->load() == 1    : false;
    const int   bendOscTgt   = bendOscTgtParam   ? (int)bendOscTgtParam->load()     : 1;
    const float bendSensOsc  = bendSensOscParam  ? bendSensOscParam->load()         : 2.0f;
    const float bendSensFilt = bendSensFiltParam ? bendSensFiltParam->load()        : 0.0f;
    const float modSensOsc   = modSensOscParam   ? modSensOscParam->load()          : 0.2f;
    const float modSensFilt  = modSensFiltParam  ? modSensFiltParam->load()         : 0.0f;

    const bool  applyOscPitchMod = (oscLfoMod > 0.0f || oscEnvMod > 0.0f || (modWheelNorm > 0.0f && modSensOsc > 0.0f));

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Silence all voices when the play mode changes
    if (playMode != prevPlayMode)
    {
        for (int v = 0; v < NUM_VOICES; ++v)
        {
            voices[v].held   = false;
            voices[v].active = false;
            voices[v].env1.reset();
            voices[v].env2.reset();
        }
        filterEnv.reset();
        lfo.prepare(getSampleRate());  // full reset (phase, delay ramp)
        noteStack.clear();
        prevPlayMode = playMode;
    }

    // ---- ARP parameters -----------------------------------------------------
    const int   arpMode  = arpModeParam  ? (int)arpModeParam->load()    : 0;
    const int   arpOctRg = arpOctParam   ? (int)arpOctParam->load() + 1 : 1; // 0→1oct … 3→4oct
    const int   arpClock = arpClockParam ? (int)arpClockParam->load()   : 0;
    const float arpBpm   = arpBpmParam   ? arpBpmParam->load()          : 120.0f;
    const int   arpDiv   = arpDivParam   ? (int)arpDivParam->load()     : 1;

    // Compute ARP step length (samples)
    const double divMuls[4] = { 1.0, 2.0, 4.0, 8.0 };
    double bpmToUse = (double)arpBpm;
    if (arpClock == 1)  // Host sync
    {
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
                if (pos->getBpm().hasValue())
                    bpmToUse = *pos->getBpm();
        }
    }
    const int64_t arpStepSamples = (int64_t)(getSampleRate() * 60.0 / bpmToUse / divMuls[arpDiv]);

    // ARP mode change: clear state on transition
    if (arpMode != prevArpMode)
    {
        // Release any held arp note by directly releasing voice 0
        if (arpGateOpen && arpCurrentNote >= 0)
        {
            const float ks1 = env1KF ? std::pow (2.0f, (60.0f - (float)arpCurrentNote) / 24.0f) : 1.0f;
            const float ks2 = env2KF ? std::pow (2.0f, (60.0f - (float)arpCurrentNote) / 24.0f) : 1.0f;
            voices[0].held = false;
            voices[0].env1.noteOff (env1R, ks1);
            voices[0].env2.noteOff (env2R, ks2);
            if (filtEnvSrc == 0) filterEnv.noteOff (env1R, 1.0f);
            else                 filterEnv.noteOff (env2R, ks2);
        }
        arpHeldNotes.clear();
        arpPattern.clear();
        arpStepIndex   = 0;
        arpCurrentNote = -1;
        arpGateOpen    = false;
        arpSamplePos   = 0;
        prevArpMode    = arpMode;
    }

    // ---- MIDI ---------------------------------------------------------------
    bool arpNotesChanged = false;
    const bool wasArpEmpty = arpHeldNotes.isEmpty();

    // Arp note-on helper (always fires on voice 0, no portamento between arp steps)
    auto arpFireNoteOn = [&](int note, float vel)
    {
        const float ks1 = env1KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
        const float ks2 = env2KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
        const float freq = (float)juce::MidiMessage::getMidiNoteInHertz (note);
        filterCurrentNote = note;
        filterCurrentVel  = vel;
        if (filtEnvSrc == 0) filterEnv.noteOn (env1A, env1D, env1S, env1R, 1.0f);
        else                 filterEnv.noteOn (env2A, env2D, env2S, env2R, ks2);
        lfo.noteOn();
        voices[0].portaSmoothed.setCurrentAndTargetValue (freq);
        voices[0].midiNote  = note;
        voices[0].active    = true;
        voices[0].held      = true;
        voices[0].timestamp = ++voiceTimestamp;
        voices[0].velocity  = vel;
        voices[0].env1.noteOn (env1A, env1D, env1S, env1R, ks1);
        voices[0].env2.noteOn (env2A, env2D, env2S, env2R, ks2);
        lastNoteFreq = freq;
    };

    // Arp note-off helper
    auto arpFireNoteOff = [&](int note)
    {
        const float ks1 = env1KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
        const float ks2 = env2KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
        voices[0].held = false;
        voices[0].env1.noteOff (env1R, ks1);
        voices[0].env2.noteOff (env2R, ks2);
        if (filtEnvSrc == 0) filterEnv.noteOff (env1R, 1.0f);
        else                 filterEnv.noteOff (env2R, ks2);
    };

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            const int   note = msg.getNoteNumber();
            const float vel  = msg.getVelocity() / 127.0f;

            if (arpMode != 0)
            {
                // ARP active: add note to held pool; voice triggering is handled by the arp clock
                if (!arpHeldNotes.contains (note))
                {
                    arpHeldNotes.add (note);
                    arpNotesChanged = true;
                }
            }
            else
            {
                // Normal voice triggering path
                const float ks1  = env1KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
                const float ks2  = env2KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;

                noteStack.removeAllInstancesOf(note);
                noteStack.add(note);

                if (filtEnvSrc == 0)
                    filterEnv.noteOn(env1A, env1D, env1S, env1R, 1.0f);
                else
                    filterEnv.noteOn(env2A, env2D, env2S, env2R, ks2);
                lfo.noteOn();

                filterCurrentNote = note;
                filterCurrentVel  = vel;

                const float newRawFreq = juce::MidiMessage::getMidiNoteInHertz (note);

                if (playMode == 0) // Mono
                {
                    const bool doPorta = (portaTimeMs > 0.0f) && (!legatoMode || voices[0].held);
                    if (doPorta) {
                        voices[0].portaSmoothed.reset (getSampleRate(), portaTimeMs / 1000.0f);
                        voices[0].portaSmoothed.setCurrentAndTargetValue (lastNoteFreq);
                        voices[0].portaSmoothed.setTargetValue (newRawFreq);
                    } else {
                        voices[0].portaSmoothed.setCurrentAndTargetValue (newRawFreq);
                    }
                    voices[0].midiNote  = note;
                    voices[0].active    = true;
                    voices[0].held      = true;
                    voices[0].timestamp = ++voiceTimestamp;
                    voices[0].velocity  = vel;
                    voices[0].env1.noteOn (env1A, env1D, env1S, env1R, ks1);
                    voices[0].env2.noteOn (env2A, env2D, env2S, env2R, ks2);
                }
                else if (playMode == 1) // Unison
                {
                    const bool doPorta = (portaTimeMs > 0.0f) && (!legatoMode || voices[0].held);
                    for (int v = 0; v < NUM_VOICES; ++v)
                    {
                        if (doPorta) {
                            voices[v].portaSmoothed.reset (getSampleRate(), portaTimeMs / 1000.0f);
                            voices[v].portaSmoothed.setCurrentAndTargetValue (lastNoteFreq);
                            voices[v].portaSmoothed.setTargetValue (newRawFreq);
                        } else {
                            voices[v].portaSmoothed.setCurrentAndTargetValue (newRawFreq);
                        }
                        voices[v].midiNote = note;
                        voices[v].active   = true;
                        voices[v].held     = true;
                        voices[v].velocity = vel;
                        voices[v].env1.noteOn (env1A, env1D, env1S, env1R, ks1);
                        voices[v].env2.noteOn (env2A, env2D, env2S, env2R, ks2);
                    }
                }
                else // Poly
                {
                    bool anyHeld = false;
                    for (int v = 0; v < NUM_VOICES; ++v)
                        if (voices[v].held) { anyHeld = true; break; }
                    const bool doPorta = (portaTimeMs > 0.0f) && (!legatoMode || anyHeld);

                    int vi = findFreeVoice();
                    if (doPorta) {
                        voices[vi].portaSmoothed.reset (getSampleRate(), portaTimeMs / 1000.0f);
                        voices[vi].portaSmoothed.setCurrentAndTargetValue (lastNoteFreq);
                        voices[vi].portaSmoothed.setTargetValue (newRawFreq);
                    } else {
                        voices[vi].portaSmoothed.setCurrentAndTargetValue (newRawFreq);
                    }
                    voices[vi].midiNote  = note;
                    voices[vi].active    = true;
                    voices[vi].held      = true;
                    voices[vi].timestamp = ++voiceTimestamp;
                    voices[vi].velocity  = vel;
                    voices[vi].env1.noteOn (env1A, env1D, env1S, env1R, ks1);
                    voices[vi].env2.noteOn (env2A, env2D, env2S, env2R, ks2);
                }
                lastNoteFreq = newRawFreq;
            }
        }

        if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();

            if (arpMode != 0)
            {
                // ARP active: remove from held pool
                arpHeldNotes.removeAllInstancesOf (note);
                arpNotesChanged = true;
                if (arpHeldNotes.isEmpty())
                {
                    // All notes released: silence immediately
                    if (arpGateOpen && arpCurrentNote >= 0)
                        arpFireNoteOff (arpCurrentNote);
                    arpCurrentNote = -1;
                    arpGateOpen    = false;
                    arpStepIndex   = 0;
                    arpSamplePos   = 0;
                    arpPattern.clear();
                }
            }
            else
            {
            noteStack.removeAllInstancesOf(note);

            if (playMode == 0) // Mono
            {
                if (noteStack.isEmpty())
                {
                    const float ks1 = env1KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
                    const float ks2 = env2KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
                    voices[0].held = false;
                    voices[0].env1.noteOff (env1R, ks1);
                    voices[0].env2.noteOff (env2R, ks2);
                    if (filtEnvSrc == 0) filterEnv.noteOff(env1R, 1.0f);
                    else                filterEnv.noteOff(env2R, ks2);
                }
                else
                {
                    // Legato: retrigger on last held note
                    const int   retrig = noteStack.getLast();
                    const float ks1    = env1KF ? std::pow (2.0f, (60.0f - (float)retrig) / 24.0f) : 1.0f;
                    const float ks2    = env2KF ? std::pow (2.0f, (60.0f - (float)retrig) / 24.0f) : 1.0f;
                    {
                        const float retrigFreq = juce::MidiMessage::getMidiNoteInHertz (retrig);
                        if (portaTimeMs > 0.0f) {
                            voices[0].portaSmoothed.reset (getSampleRate(), portaTimeMs / 1000.0f);
                            voices[0].portaSmoothed.setCurrentAndTargetValue (lastNoteFreq);
                            voices[0].portaSmoothed.setTargetValue (retrigFreq);
                        } else {
                            voices[0].portaSmoothed.setCurrentAndTargetValue (retrigFreq);
                        }
                        lastNoteFreq = retrigFreq;
                    }
                    voices[0].midiNote = retrig;
                    voices[0].env1.noteOn (env1A, env1D, env1S, env1R, ks1);
                    voices[0].env2.noteOn (env2A, env2D, env2S, env2R, ks2);
                    filterEnv.noteOn (env2A, env2D, env2S, env2R, ks2);
                }
            }
            else if (playMode == 1) // Unison
            {
                if (noteStack.isEmpty())
                {
                    const float ks1 = env1KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
                    const float ks2 = env2KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
                    for (int v = 0; v < NUM_VOICES; ++v)
                    {
                        voices[v].held = false;
                        voices[v].env1.noteOff (env1R, ks1);
                        voices[v].env2.noteOff (env2R, ks2);
                    }
                    if (filtEnvSrc == 0) filterEnv.noteOff(env1R, 1.0f);
                    else                filterEnv.noteOff(env2R, ks2);
                }
                else
                {
                    const int   retrig = noteStack.getLast();
                    const float ks1    = env1KF ? std::pow (2.0f, (60.0f - (float)retrig) / 24.0f) : 1.0f;
                    const float ks2    = env2KF ? std::pow (2.0f, (60.0f - (float)retrig) / 24.0f) : 1.0f;
                    {
                        const float retrigFreq = juce::MidiMessage::getMidiNoteInHertz (retrig);
                        for (int v = 0; v < NUM_VOICES; ++v)
                        {
                            if (portaTimeMs > 0.0f) {
                                voices[v].portaSmoothed.reset (getSampleRate(), portaTimeMs / 1000.0f);
                                voices[v].portaSmoothed.setCurrentAndTargetValue (lastNoteFreq);
                                voices[v].portaSmoothed.setTargetValue (retrigFreq);
                            } else {
                                voices[v].portaSmoothed.setCurrentAndTargetValue (retrigFreq);
                            }
                            voices[v].midiNote = retrig;
                            voices[v].env1.noteOn (env1A, env1D, env1S, env1R, ks1);
                            voices[v].env2.noteOn (env2A, env2D, env2S, env2R, ks2);
                        }
                        lastNoteFreq = retrigFreq;
                    }
                    filterEnv.noteOn (env2A, env2D, env2S, env2R, ks2);
                }
            }
            else // Poly
            {
                for (int v = 0; v < NUM_VOICES; ++v)
                {
                    if (voices[v].midiNote == note && voices[v].held)
                    {
                        const float ks1 = env1KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
                        const float ks2 = env2KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
                        voices[v].held = false;
                        voices[v].env1.noteOff (env1R, ks1);
                        voices[v].env2.noteOff (env2R, ks2);
                        break;
                    }
                }
                // Release filter env only when no voices remain held
                bool anyHeld = false;
                for (int v = 0; v < NUM_VOICES; ++v)
                    if (voices[v].held) { anyHeld = true; break; }
                if (!anyHeld)
                {
                    const float ks2 = env2KF ? std::pow (2.0f, (60.0f - (float)note) / 24.0f) : 1.0f;
                    if (filtEnvSrc == 0) filterEnv.noteOff(env1R, 1.0f);
                    else                filterEnv.noteOff(env2R, ks2);
                }
            }
            } // end else (non-arp path)
        }

        if (msg.isPitchWheel())
            pitchBendNorm = msg.getPitchWheelValue() / 8192.0f;

        if (msg.isController() && msg.getControllerNumber() == 1)
            modWheelNorm = msg.getControllerValue() / 127.0f;
    }

    // ---- ARP clock ----------------------------------------------------------
    if (arpMode != 0 && arpStepSamples > 0)
    {
        // Rebuild note pattern if held notes changed
        if (arpNotesChanged && !arpHeldNotes.isEmpty())
        {
            buildArpPattern (arpMode, arpOctRg);
            if (arpStepIndex >= arpPattern.size())
                arpStepIndex = 0;

            // Start immediately when the first note is added
            if (wasArpEmpty && !arpPattern.isEmpty())
            {
                if (arpGateOpen && arpCurrentNote >= 0)
                    arpFireNoteOff (arpCurrentNote);
                arpStepIndex   = 0;
                arpCurrentNote = arpPattern[0];
                arpFireNoteOn  (arpCurrentNote, 1.0f);
                arpGateOpen    = true;
                arpSamplePos   = 0;
            }
        }

        if (!arpPattern.isEmpty())
        {
            // Close gate at 50% duty cycle
            if (arpGateOpen && arpSamplePos >= arpStepSamples / 2)
            {
                arpFireNoteOff (arpCurrentNote);
                arpGateOpen = false;
            }

            // Advance to next step
            if (arpSamplePos >= arpStepSamples)
            {
                arpSamplePos -= arpStepSamples;
                arpStepIndex = (arpStepIndex + 1) % arpPattern.size();

                // Reshuffle when Random pattern cycles
                if (arpMode == 4 && arpStepIndex == 0)
                    buildArpPattern (arpMode, arpOctRg);

                arpCurrentNote = arpPattern[arpStepIndex];
                arpFireNoteOn  (arpCurrentNote, 1.0f);
                arpGateOpen    = true;
            }

            arpSamplePos += buffer.getNumSamples();
        }
    }

    // Clear extra output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // ---- Per-buffer constants -----------------------------------------------
    float octMul1 = 1.0f;
    switch (octave)
    {
        case 0: octMul1 = 0.5f; break;
        case 1: octMul1 = 1.0f; break;
        case 2: octMul1 = 2.0f; break;
        case 3: octMul1 = 4.0f; break;
    }

    float octMul2 = std::pow(2.0f, semitones2 / 12.0f);

    float fineMul = std::pow (2.0f, osc2FineParam->load() / 1200.0f);
    float mix     = oscMixParam->load();
    float oscGain1 = juce::jmap (mix, -1.0f, 1.0f, 1.0f, 0.0f);
    float oscGain2 = juce::jmap (mix, -1.0f, 1.0f, 0.0f, 1.0f);

    // Precompute per-voice unison detune multipliers (5 cents per step)
    float unisonDetune[NUM_VOICES];
    for (int v = 0; v < NUM_VOICES; ++v)
        unisonDetune[v] = std::pow (2.0f, (v - 3.5f) * 5.0f / 1200.0f);

    // Set frequencies and waveforms once per buffer; store base freqs for per-sample pitch mod
    float baseFreq1[NUM_VOICES] = {};
    float baseFreq2[NUM_VOICES] = {};

    for (int v = 0; v < NUM_VOICES; ++v)
    {
        if (!voices[v].active) continue;

        float freq = voices[v].portaSmoothed.getTargetValue();
        if (playMode == 1)
            freq *= unisonDetune[v];

        baseFreq1[v] = freq * octMul1;
        baseFreq2[v] = freq * octMul2 * fineMul;

        voices[v].osc1.setFrequency (baseFreq1[v]);
        voices[v].osc2.setFrequency (baseFreq2[v]);
        voices[v].osc1.setWaveType (wave);
        voices[v].osc2.setWaveType (wave2);
    }

    // Normalisation: Unison stacks all 8 voices on one pitch, so compensate.
    // Mono and Poly give each voice a consistent individual level (no cross-voice scaling).
    const float norm = (playMode == 1) ? 1.0f / std::sqrt ((float)NUM_VOICES) : 1.0f;

    // ---- Sample loop --------------------------------------------------------
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float pw  = smoothedPulseWidth.getNextValue();
        float sig = 0.0f;

        // Portamento: advance smoothers and update base frequencies per-sample
        for (int v = 0; v < NUM_VOICES; ++v)
        {
            if (!voices[v].active || !voices[v].portaSmoothed.isSmoothing()) continue;
            float pFreq = voices[v].portaSmoothed.getNextValue();
            if (playMode == 1) pFreq *= unisonDetune[v];
            baseFreq1[v] = pFreq * octMul1;
            baseFreq2[v] = pFreq * octMul2 * fineMul;
            voices[v].osc1.setFrequency (baseFreq1[v]);
            voices[v].osc2.setFrequency (baseFreq2[v]);
        }

        // Shared modulation sources — computed once per sample before voice loop
        const float lfoOut = lfo.process();
        lfo.takeCycleTick();  // consume tick (reserved for TRIG ENV)

        // LFO pitch semitone offset (±24 semitones at full depth) — routed by MOD TARGET
        const float lfoSemi = oscLfoMod * lfoOut * 24.0f;
        // Mod wheel pitch semitone offset — routed by BEND_OSC_TGT (independent of MOD TARGET)
        const float mwSemi  = modWheelNorm * modSensOsc * lfoOut * 24.0f;

        const float filtEnvVal = filterEnv.process();  // sourced from ENV1 or ENV2 per FILT_ENV_SRC

        // LFO tremolo: bipolar [-1,1] modulates multiplicatively around gain level
        // (biased so it cannot push below zero: gain * (1 + lfoMod * lfoOut) ≥ 0
        //  as long as lfoMod ≤ 1, which is guaranteed by the parameter range)
        const float lfoTremolo = 1.0f + vcaLfoMod * lfoOut;

        for (int v = 0; v < NUM_VOICES; ++v)
        {
            if (!voices[v].active) continue;

            const float env1Val = voices[v].env1.process();
            const float env2Val = voices[v].env2.process();  // ENV2 → VCA

            // Pitch bend multiplier (applied to OSC target per BEND_OSC_TGT)
            const float bendOscMul = std::exp2 (pitchBendNorm * bendSensOsc * (1.0f / 12.0f));
            float freq1Eff = baseFreq1[v] * ((bendOscTgt >= 1) ? bendOscMul : 1.0f);
            float freq2Eff = baseFreq2[v] * ((bendOscTgt <= 1) ? bendOscMul : 1.0f);

            // OSC pitch modulation — LFO/ENV routes via MOD TARGET; mod wheel routes via BEND_OSC_TGT
            bool osc1Updated = false, osc2Updated = false;

            if (oscLfoMod > 0.0f || oscEnvMod > 0.0f)
            {
                const float lfoEnvMul = std::exp2 ((lfoSemi + oscEnvMod * env1Val * 24.0f) * (1.0f / 12.0f));
                if (modTarget >= 1) { freq1Eff *= lfoEnvMul; osc1Updated = true; }
                if (modTarget <= 1) { freq2Eff *= lfoEnvMul; osc2Updated = true; }
            }

            if (mwSemi != 0.0f)
            {
                const float mwMul = std::exp2 (mwSemi * (1.0f / 12.0f));
                if (bendOscTgt >= 1) { freq1Eff *= mwMul; osc1Updated = true; }
                if (bendOscTgt <= 1) { freq2Eff *= mwMul; osc2Updated = true; }
            }

            if (pitchBendNorm != 0.0f)
            {
                if (bendOscTgt >= 1) osc1Updated = true;
                if (bendOscTgt <= 1) osc2Updated = true;
            }

            if (osc1Updated) voices[v].osc1.setFrequency (freq1Eff);
            if (osc2Updated) voices[v].osc2.setFrequency (freq2Eff);

            // VCA: LEVEL × velocityScale × ENV2 × LFO tremolo, clamped [0, 1]
            const float velScale = vcaVelSens * voices[v].velocity
                                 + (1.0f - vcaVelSens);
            const float vcaGain  = juce::jlimit (0.0f, 1.0f,
                                       vcaLevel * velScale * env2Val * lfoTremolo);

            // Compute actual pulse width from PW SRC mode.
            // MAN: slider is direct PW. LFO/ENV: slider is depth, centred at 0.5.
            float actualPw;
            switch (pwMode)
            {
                case 2:  // LFO  — bipolar modulation around 0.5
                    actualPw = juce::jlimit (0.05f, 0.95f, 0.5f + (pw - 0.5f) * lfoOut);
                    break;
                case 1:  // ENV1 — unipolar, slider sets target PW at envelope peak
                    actualPw = juce::jlimit (0.05f, 0.95f, 0.5f + (pw - 0.5f) * env1Val);
                    break;
                case 0:  // ENV2 — same but driven by per-voice ENV2
                    actualPw = juce::jlimit (0.05f, 0.95f, 0.5f + (pw - 0.5f) * env2Val);
                    break;
                default: // MAN (3) — slider directly sets pulse width
                    actualPw = pw;
                    break;
            }

            if (modTarget >= 1) voices[v].osc1.setPulseWidth (actualPw);
            if (modTarget <= 1) voices[v].osc2.setPulseWidth (actualPw);

            // Process OSC2 first so its output can drive cross mod on OSC1.
            const float s2 = voices[v].osc2.process();

            // Cross mod: standard FM — OSC2 output modulates OSC1 instantaneous frequency.
            // s2 is scaled by 0.2 inside process(), so multiply by 5 to normalise to [-1,1].
            // Modulation index β = crossMod × 10 (range 0–10 at full depth).
            // Formula: fi = fc + β × fm × s2_norm  →  freq1Eff + crossMod × 10 × baseFreq2 × s2 × 5
            if (crossMod > 0.0f)
                voices[v].osc1.setFrequency (juce::jmax (1.0f, freq1Eff + crossMod * 50.0f * freq2Eff * s2));

            const float s1 = voices[v].osc1.process();

            // Hard sync: reset OSC2 phase whenever OSC1 completes a cycle.
            // A wrap is detected by the phase decreasing (rolls over from ~1 back to ~0).
            if (osc2Sync)
            {
                const float newPhase = voices[v].osc1.getPhase();
                if (newPhase < voices[v].prevOsc1Phase)
                    voices[v].osc2.resetPhase();
                voices[v].prevOsc1Phase = newPhase;
            }

            sig += (s1 * oscGain1 + s2 * oscGain2) * vcaGain;
        }

        // Filter cutoff modulation: key follow × velocity sens × ENV mod × LFO mod
        // VEL SENS: direct boost up to +4 octaves at full depth + full velocity (independent of ENV MOD)
        // ENV MOD: ~10 octaves at full depth — enough to push 20 Hz (fully closed) to 20 kHz (fully open)
        const float cutoffMod = std::exp2 (filtKeyFlw   * (filterCurrentNote - 60) * (1.0f / 12.0f))
                              * std::exp2 (filtVelSens   * filterCurrentVel * 4.0f)
                              * std::exp2 (filtEnvMod    * filtEnvVal * 10.0f)
                              * std::exp2 (filtLfoMod    * lfoOut * 2.0f)
                              * std::exp2 (pitchBendNorm * bendSensFilt * 4.0f)
                              * std::exp2 (modWheelNorm  * modSensFilt  * lfoOut * 2.0f);

        sig *= norm;
        sig  = filter.processSample (sig, cutoffMod);

        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            buffer.setSample (ch, sample, sig);
    }

    // Deactivate voices whose ENV2 (VCA) has completed its release
    for (int v = 0; v < NUM_VOICES; ++v)
        if (!voices[v].held && !voices[v].env2.isActive())
            voices[v].active = false;
}

//==============================================================================
bool Planet8AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* Planet8AudioProcessor::createEditor()
{
    return new Planet8AudioProcessorEditor (*this);
}

//==============================================================================
void Planet8AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    patchManager->getStateXml (destData);
}

void Planet8AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    patchManager->setStateFromXml (data, sizeInBytes);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
Planet8AudioProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ---- Oscillators --------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "WAVE", "Wave Type",
        juce::StringArray{ "Saw", "Pulse", "Triangle", "Sine", "Square", "Noise" }, 3));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "OCT", "Octave",
        juce::StringArray{ "16'", "8'", "4'", "2'" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PW", "Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.001f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "WAVE2", "Wave Type 2",
        juce::StringArray{ "Saw", "Pulse", "Triangle", "Sine", "Square", "Noise" }, 3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "OCT2", "Octave 2",
        juce::NormalisableRange<float>(-12.0f, 24.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "OSC2_FINE", "OSC2 Fine",
        juce::NormalisableRange<float>(-100.0f, 100.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "OSC_MIX", "Osc Mix",
        juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "MOD_TARGET", "Mod Target",
        juce::StringArray{ "OSC2", "Both", "OSC1" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "OSC_LFO_MOD", "OSC LFO Mod Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "OSC_ENV_MOD", "OSC ENV1 Mod Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "PW_MODE", "PW Mode",
        juce::StringArray{ "ENV2", "ENV1", "LFO", "MAN" }, 3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "CROSS_MOD", "Cross Mod",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "OSC2_SYNC", "OSC2 Sync",
        juce::StringArray{ "OFF", "ON" }, 0));

    // ---- Voice / Filter -----------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "PLAY_MODE", "Play Mode",
        juce::StringArray{ "Mono", "Unison", "Poly" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "CUTOFF", "Filter Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.3f), 20000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "RESONANCE", "Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "HPF_CUTOFF", "HPF Cutoff",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 0.01f, 0.3f), 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "SLOPE", "Filter Slope",
        juce::StringArray{ "12dB", "24dB" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "FILT_VEL_SENS", "Filter Velocity Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "FILT_ENV_MOD", "Filter Envelope Mod Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "FILT_ENV_SRC", "Filter Envelope Source",
        juce::StringArray{ "ENV1", "ENV2" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "FILT_LFO_MOD", "Filter LFO Mod Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "FILT_KEY_FLW", "Filter Key Follow",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    // ---- KEYBRD MOD ---------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PORTAMENTO", "Portamento Time",
        juce::NormalisableRange<float>(0.0f, 2000.0f, 0.1f, 0.3f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "LEGATO", "Legato Mode",
        juce::StringArray{ "OFF", "ON" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "BEND_OSC_TGT", "Bend/Mod OSC Target",
        juce::StringArray{ "OSC2", "BOTH", "OSC1" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "BEND_SENS_OSC", "Pitch Bend Sensitivity OSC",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "BEND_SENS_FILT", "Pitch Bend Sensitivity Filter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "MOD_SENS_OSC", "Mod Wheel Sensitivity OSC",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "MOD_SENS_FILT", "Mod Wheel Sensitivity Filter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    // ---- ENV1 ---------------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ENV1_A", "ENV1 Attack",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.01f, 0.3f), 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ENV1_D", "ENV1 Decay",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.01f, 0.3f), 200.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ENV1_S", "ENV1 Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ENV1_R", "ENV1 Release",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.01f, 0.3f), 300.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "ENV1_KEYFOLLOW", "ENV1 Key Follow",
        juce::StringArray{ "Off", "On" }, 0));

    // ---- ENV2 ---------------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ENV2_A", "ENV2 Attack",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.01f, 0.3f), 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ENV2_D", "ENV2 Decay",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.01f, 0.3f), 500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ENV2_S", "ENV2 Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ENV2_R", "ENV2 Release",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.01f, 0.3f), 500.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "ENV2_KEYFOLLOW", "ENV2 Key Follow",
        juce::StringArray{ "Off", "On" }, 0));

    // ---- LFO ----------------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "LFO_RATE", "LFO Rate",
        juce::NormalisableRange<float>(0.01f, 20.0f, 0.001f, 0.3f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "LFO_DELAY", "LFO Delay Time",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "LFO_WAVE", "LFO Waveform",
        juce::StringArray{ "Sine", "Triangle", "Saw", "Square", "RND" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "LFO_KEYTRIG", "LFO Key Trig",
        juce::StringArray{ "Off", "On" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "LFO_TRIGENV", "LFO Trig Env",
        juce::StringArray{ "Off", "On" }, 0));

    // ---- VCA ----------------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "VCA_LEVEL", "VCA Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "VCA_LFO_MOD", "VCA LFO Mod",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "VCA_VEL_SENS", "VCA Vel Sens",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    // ---- ARP ----------------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "ARP_MODE", "Arp Mode",
        juce::StringArray{ "Off", "Up", "Down", "Up+Down", "Random" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "ARP_OCT", "Arp Octave Range",
        juce::StringArray{ "1 Oct", "2 Oct", "3 Oct", "4 Oct" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "ARP_CLOCK", "Arp Clock Source",
        juce::StringArray{ "Internal", "Host" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ARP_BPM", "Arp BPM",
        juce::NormalisableRange<float>(40.0f, 240.0f, 0.1f), 120.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "ARP_DIV", "Arp Note Division",
        juce::StringArray{ "1/4", "1/8", "1/16", "1/32" }, 1));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Planet8AudioProcessor();
}
