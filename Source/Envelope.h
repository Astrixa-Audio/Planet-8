/*
  ==============================================================================

    Envelope.h
    Created: 17 Mar 2026 4:41:23pm
    Author:  tolgainci

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
// Jupiter-8 style ADSR envelope.
//
// Attack  : linear ramp from 0 → 1
// Decay   : one-pole exponential from 1 → sustainLevel
// Sustain : holds at sustainLevel while note is held
// Release : one-pole exponential from current value → 0
//
// Key follow: pass keyScale = pow(2, (60 - midiNote) / 24) so that
// ADR times are doubled 2 octaves below middle C and halved 2 octaves above.
// Set keyScale = 1.0 to disable key follow.
//==============================================================================
class Envelope
{
public:
    void  prepare (double sampleRate) noexcept;
    void  reset   () noexcept;

    /// Trigger note-on.  All times in milliseconds; sustain in [0, 1].
    void  noteOn  (float attackMs, float decayMs, float sustainLevel,
                   float releaseMs, float keyScale = 1.0f) noexcept;

    /// Trigger note-off.  Pass the current release-time parameter so knob
    /// changes take effect in real time.
    void  noteOff (float releaseMs, float keyScale = 1.0f) noexcept;

    /// Returns the current envelope value in [0, 1].  Call once per sample.
    float process () noexcept;

    bool  isActive () const noexcept { return phase != Phase::Idle; }

private:
    enum class Phase { Idle, Attack, Decay, Sustain, Release };

    /// One-pole coefficient reaching 99 % of target in timeMs milliseconds.
    static float timeToCoef (float timeMs, double sr) noexcept;

    Phase  phase          = Phase::Idle;
    float  value          = 0.0f;
    double sampleRate     = 44100.0;

    float  attackIncrement = 0.0f;   ///< linear attack step per sample
    float  decayCoef       = 0.0f;   ///< exp coef for decay toward sustain
    float  sustainLevel    = 0.7f;
    float  releaseCoef     = 0.0f;   ///< exp coef for release toward 0
};
