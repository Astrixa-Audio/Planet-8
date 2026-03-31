/*
  ==============================================================================

    Filter.cpp
    Created: 17 Mar 2026 1:40:38pm
    Author:  tolgainci

  ==============================================================================
*/

#include "Filter.h"

//==============================================================================
LadderFilter::LadderFilter()
{
    // Seed per-instance drift from wall-clock time so every instance
    // gets a unique (but reproducible within a session) character.
    juce::Random seedRng ((juce::int64) juce::Time::currentTimeMillis());

    float driftSemitones  = (seedRng.nextFloat() * 2.0f - 1.0f) * 0.3f;
    cutoffDriftMult       = std::pow (2.0f, driftSemitones / 12.0f);
    resonanceDriftScale   = 1.0f + (seedRng.nextFloat() * 2.0f - 1.0f) * 0.02f;

    noiseRng.setSeed (seedRng.nextInt64());
}

//==============================================================================
void LadderFilter::prepare (double newSampleRate)
{
    sampleRate   = newSampleRate;
    sampleRate4x = newSampleRate * 4.0;

    // DC blocker: 1 Hz first-order HPF  y[n] = x[n] - x[n-1] + R * y[n-1]
    dcCoeff = 1.0f - (juce::MathConstants<float>::twoPi / (float) sampleRate);

    // Initialise smoothers with safe starting values
    cutoffSmoother.reset (sampleRate, 0.01);
    cutoffSmoother.setCurrentAndTargetValue (1000.0f);

    resSmoother.reset (sampleRate, 0.01);
    resSmoother.setCurrentAndTargetValue (0.0f);

    hpfSmoother.reset (sampleRate, 0.01);
    hpfSmoother.setCurrentAndTargetValue (20.0f);

    reset();
}

void LadderFilter::reset()
{
    s[0] = s[1] = s[2] = s[3] = 0.0f;
    lastY4    = 0.0f;
    hpfS      = 0.0f;
    dcX1      = 0.0f;
    dcY1      = 0.0f;
    prevInput = 0.0f;
}

//==============================================================================
void LadderFilter::setCutoff    (float hz)      { cutoffSmoother.setTargetValue (hz); }
void LadderFilter::setResonance (float r)       { resSmoother.setTargetValue (r);     }
void LadderFilter::setHPFCutoff (float hz)      { hpfSmoother.setTargetValue (hz);    }
void LadderFilter::setSlope     (bool use24dB)  { is24dB = use24dB;                   }

//==============================================================================
void LadderFilter::updateCoefficients (float cutHz, float res, float hpfHz) noexcept
{
    // ---- Ladder ----
    float fc = juce::jlimit (20.0f, (float) (sampleRate4x * 0.499), cutHz * cutoffDriftMult);
    float G  = std::tan (juce::MathConstants<float>::pi * fc / (float) sampleRate4x);
    gLadder  = G / (1.0f + G);

    // ---- HPF ----
    float fh = juce::jlimit (20.0f, 1000.0f, hpfHz);
    float Gh = std::tan (juce::MathConstants<float>::pi * fh / (float) sampleRate4x);
    gHPF     = Gh / (1.0f + Gh);

    // ---- Resonance ----
    float r  = juce::jlimit (0.0f, 1.0f, res * resonanceDriftScale);
    k        = r * 4.0f;

    // Loudness compensation: resonance sucks energy from the passband;
    // boost the input proportionally to keep perceived level stable.
    inputGain = 1.0f + k * 0.2f;
}

//==============================================================================
float LadderFilter::processSample (float x, float cutoffMod) noexcept
{
    // Advance smoothers; multiply cutoff by envelope modulator at audio rate.
    updateCoefficients (cutoffSmoother.getNextValue() * cutoffMod,
                        resSmoother.getNextValue(),
                        hpfSmoother.getNextValue());

    // Thermal noise floor mixed into filter input.
    x += kNoiseFloor * (noiseRng.nextFloat() * 2.0f - 1.0f);

    // 4x oversampling:
    //   Upsample  – linear interpolation between consecutive input samples.
    //   Decimate  – average the four sub-sample outputs (boxcar FIR).
    float delta = (x - prevInput) * 0.25f;
    float y = 0.0f;
    for (int i = 0; i < 4; ++i)
        y += processOnce4x (prevInput + delta * (float) (i + 1));
    prevInput = x;
    y *= 0.25f;

    // DC blocker: 1 Hz HPF applied at the original (non-oversampled) rate.
    float out = y - dcX1 + dcCoeff * dcY1;
    dcX1 = y;
    dcY1 = out;

    return out;
}

//==============================================================================
float LadderFilter::processOnce4x (float x) noexcept
{
    // --- 1-pole TPT high-pass pre-filter ---
    // v   = (x - s) * g
    // LP  = s + v       →  HP = x - LP
    // s' = LP + v  =  s + 2v
    float hpfV  = (x - hpfS) * gHPF;
    float hpfLP = hpfS + hpfV;
    hpfS       += 2.0f * hpfV;
    float xf    = x - hpfLP;   // high-pass output

    // --- Implicit ZDF resonance feedback ---
    // The delay-one approximation (using lastY4) makes the loop gain
    // increase with cutoff, causing self-oscillation when ENV MOD sweeps
    // the cutoff up while resonance is high.
    //
    // Instead, solve the implicit equation exactly for the linear part:
    //   y4 = g4*(xf*inputGain - k*tanh(y4)) + S
    //
    // where S is the state-only contribution to y4.  Linearising tanh
    // gives the closed-form estimate:
    //   y4_lin = (g4 * xf * inputGain + S) / (1 + k*g4)
    //
    // Using y4_lin as the tanh argument removes the destabilising delay
    // while preserving the soft-saturation nonlinearity.
    const float g  = gLadder;
    const float g2 = g * g;
    const float g4 = g2 * g2;
    const float gm = 1.0f - g;

    // Contribution of current ladder states to y4 (zero-input response)
    const float S = gm * (g2 * g * s[0] + g2 * s[1] + g * s[2] + s[3]);

    // Implicit linear estimate of y4 for this sample
    const float y4_lin = (g4 * xf * inputGain + S) / (1.0f + k * g4);

    float xIn = xf * inputGain - k * std::tanh (y4_lin);

    // --- 4-pole ZDF ladder (TPT integrators) ---
    // Each stage:  v = (in - s) * g,  out = s + v,  s' = s + 2v
    float v0 = (xIn  - s[0]) * g;  float y0 = s[0] + v0;  s[0] += 2.0f * v0;
    float v1 = (y0   - s[1]) * g;  float y1 = s[1] + v1;  s[1] += 2.0f * v1;
    float v2 = (y1   - s[2]) * g;  float y2 = s[2] + v2;  s[2] += 2.0f * v2;
    float v3 = (y2   - s[3]) * g;  float y3 = s[3] + v3;  s[3] += 2.0f * v3;

    lastY4 = y3;

    // 24 dB/oct: tap at pole 4 (y3).  12 dB/oct: tap at pole 2 (y1).
    return is24dB ? y3 : y1;
}
