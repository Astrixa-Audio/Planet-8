/*
  ==============================================================================

    LFO.h
    Created: 18 Mar 2026
    Author:  tolgainci

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
// Jupiter-8 style LFO.
//
// Waveforms:   Sine, Triangle, Sawtooth, Square, RND (sample-and-hold)
// Delay ramp:  on noteOn(), LFO depth scales from 0 → 1 over DELAY TIME seconds
// Key Trig:    when on, phase resets to 0 on every noteOn()
// Trig Env:    caller checks takeCycleTick() once per sample; if true the
//              amplitude envelope should be retriggered (wired later)
//
// Output range: [-1, 1].  Not wired to any destination yet.
//==============================================================================
class LFO
{
public:
    enum class Wave { Sine = 0, Triangle, Saw, Square, Rnd };

    void  prepare      (double sampleRate) noexcept;

    /// Call on every note-on: resets phase (if KEY TRIG) and restarts delay ramp.
    void  noteOn       () noexcept;

    /// Advance by one sample and return the current LFO value in [-1, 1].
    float process      () noexcept;

    /// Returns true exactly once per LFO cycle (used for TRIG ENV).
    /// Clears the flag after reading.
    bool  takeCycleTick () noexcept;

    // ---- parameter setters --------------------------------------------------
    void setRate      (float hz)   noexcept;
    void setDelayTime (float secs) noexcept;
    void setWave      (int   wave) noexcept;
    void setKeyTrig   (bool  on)   noexcept { keyTrig  = on; }
    void setTrigEnv   (bool  on)   noexcept { trigEnv  = on; }

    bool getTrigEnv   ()    const  noexcept { return trigEnv; }

private:
    double sampleRate = 44100.0;

    // Phase oscillator
    float  phase     = 0.0f;
    float  phaseInc  = 1.0f / 44100.0f;   // rate / sampleRate
    float  rate      = 1.0f;

    int    waveType  = 0;
    bool   keyTrig   = false;
    bool   trigEnv   = false;
    bool   cycleTick = false;

    // Delay ramp (scales output 0 → 1 over delayTime seconds after noteOn)
    float  delayRamp = 1.0f;
    float  delayInc  = 0.0f;
    float  delayTime = 0.0f;

    // Sample-and-hold value for RND waveform
    float        rndValue = 0.0f;
    juce::Random rng;
};
