/*
  ==============================================================================

    LFO.cpp
    Created: 18 Mar 2026
    Author:  tolgainci

  ==============================================================================
*/

#include "LFO.h"

//==============================================================================
void LFO::prepare (double sr) noexcept
{
    sampleRate = sr;
    phase      = 0.0f;
    cycleTick  = false;
    delayRamp  = 1.0f;
    delayInc   = 0.0f;
    rndValue   = rng.nextFloat() * 2.0f - 1.0f;
    phaseInc   = rate / (float)sampleRate;
}

//==============================================================================
void LFO::setRate (float hz) noexcept
{
    rate     = juce::jmax (0.001f, hz);
    phaseInc = rate / (float)sampleRate;
}

void LFO::setDelayTime (float secs) noexcept
{
    delayTime = juce::jmax (0.0f, secs);
}

void LFO::setWave (int wave) noexcept
{
    waveType = juce::jlimit (0, 4, wave);
}

//==============================================================================
void LFO::noteOn () noexcept
{
    if (keyTrig)
        phase = 0.0f;

    if (delayTime > 0.0f)
    {
        delayRamp = 0.0f;
        delayInc  = 1.0f / (float)(delayTime * sampleRate);
    }
    else
    {
        delayRamp = 1.0f;
        delayInc  = 0.0f;
    }
}

//==============================================================================
bool LFO::takeCycleTick () noexcept
{
    const bool t = cycleTick;
    cycleTick    = false;
    return t;
}

//==============================================================================
float LFO::process () noexcept
{
    // Advance delay ramp
    if (delayRamp < 1.0f)
    {
        delayRamp += delayInc;
        if (delayRamp > 1.0f) delayRamp = 1.0f;
    }

    // Advance phase; detect cycle wrap
    phase += phaseInc;
    cycleTick = false;

    if (phase >= 1.0f)
    {
        phase    -= 1.0f;
        cycleTick = true;

        // RND: new random value at the start of each new cycle
        if (waveType == static_cast<int>(Wave::Rnd))
            rndValue = rng.nextFloat() * 2.0f - 1.0f;
    }

    // Generate waveform in [-1, 1]
    float out = 0.0f;

    switch (waveType)
    {
        case static_cast<int>(Wave::Sine):
            out = std::sin (juce::MathConstants<float>::twoPi * phase);
            break;

        case static_cast<int>(Wave::Triangle):
            // -1 at phase 0, +1 at phase 0.5, -1 at phase 1
            out = phase < 0.5f ? 4.0f * phase - 1.0f
                               : 3.0f - 4.0f * phase;
            break;

        case static_cast<int>(Wave::Saw):
            out = 2.0f * phase - 1.0f;
            break;

        case static_cast<int>(Wave::Square):
            out = phase < 0.5f ? 1.0f : -1.0f;
            break;

        case static_cast<int>(Wave::Rnd):
            out = rndValue;       // held until next cycle tick
            break;

        default:
            break;
    }

    return out * delayRamp;
}
