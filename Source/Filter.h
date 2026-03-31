/*
  ==============================================================================

    Filter.h
    Created: 17 Mar 2026 1:40:38pm
    Author:  tolgainci

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
// ZDF 4-pole Moog-style ladder filter with Jupiter-8 character.
//
// Architecture:
//   input → 1-pole HPF → thermal noise → 4-pole ZDF ladder → DC block → output
//
// The ladder uses TPT integrators with tanh() saturation in the resonance
// feedback path.  4x oversampling (linear-interp upsample / averaging
// decimate) reduces aliasing from the nonlinearity.
//
// Per-instance random drift (seeded at construction) gives subtle analog
// component variation between voices / instances.
//
// All continuous parameters are smoothed internally over a 10 ms ramp so
// the caller can set targets once per buffer without zipper noise.
//==============================================================================
class LadderFilter
{
public:
    LadderFilter();

    /// Call once when the host sample rate is known / changes.
    void prepare (double newSampleRate);

    /// Reset all filter state (e.g. on note-on).
    void reset();

    //==========================================================================
    // Parameter setters — set the target value; smoothing is applied
    // automatically inside processSample().
    void setCutoff    (float hz);       ///< 20 – 20 000 Hz
    void setResonance (float r);        ///< 0.0 – 1.0  (1.0 = self-oscillation)
    void setHPFCutoff (float hz);       ///< 20 – 1 000 Hz
    void setSlope     (bool use24dB);   ///< true = 24 dB/oct, false = 12 dB/oct

    //==========================================================================
    /// Process one sample at the host sample rate.
    /// cutoffMod multiplies the smoothed cutoff at audio rate (for envelope
    /// modulation).  Pass 1.0 for no modulation.
    float processSample (float x, float cutoffMod = 1.0f) noexcept;

private:
    //--- smoothed parameters (10 ms ramp) ------------------------------------
    juce::SmoothedValue<float> cutoffSmoother;
    juce::SmoothedValue<float> resSmoother;
    juce::SmoothedValue<float> hpfSmoother;

    //--- ladder state (maintained at 4x oversampled rate) --------------------
    float s[4]   = {};
    float lastY4 = 0.0f;

    //--- pre-filter HPF state (4x rate) --------------------------------------
    float hpfS = 0.0f;

    //--- DC blocker state (original rate, 1 Hz pole) -------------------------
    float dcX1    = 0.0f;
    float dcY1    = 0.0f;
    float dcCoeff = 0.0f;

    //--- current coefficients (recomputed once per output sample) ------------
    float gLadder   = 0.0f;
    float gHPF      = 0.0f;
    float k         = 0.0f;
    float inputGain = 1.0f;

    bool is24dB = true;

    //--- sample rates --------------------------------------------------------
    double sampleRate   = 44100.0;
    double sampleRate4x = 176400.0;

    //--- per-instance analog drift (randomised once in constructor) ----------
    float cutoffDriftMult     = 1.0f;   ///< exp2(±0.3 semitones)
    float resonanceDriftScale = 1.0f;   ///< ±2 %

    //--- thermal noise -------------------------------------------------------
    juce::Random noiseRng;
    static constexpr float kNoiseFloor = 3.0e-5f;  ///< ~-90 dBFS

    //--- oversampling --------------------------------------------------------
    float prevInput = 0.0f;

    //--- helpers -------------------------------------------------------------
    void  updateCoefficients (float cutHz, float res, float hpfHz) noexcept;
    float processOnce4x      (float x) noexcept;
};
