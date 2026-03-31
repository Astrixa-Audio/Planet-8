/*
  ==============================================================================

    Envelope.cpp
    Created: 17 Mar 2026 4:41:23pm
    Author:  tolgainci

  ==============================================================================
*/

#include "Envelope.h"

//==============================================================================
void Envelope::prepare (double sr) noexcept
{
    sampleRate = sr;
    reset();
}

void Envelope::reset () noexcept
{
    phase = Phase::Idle;
    value = 0.0f;
}

//==============================================================================
float Envelope::timeToCoef (float timeMs, double sr) noexcept
{
    // exp(-log(9) / N) reaches 99 % of target in N samples.
    double samples = std::max (1.0, (double)timeMs * sr / 1000.0);
    return (float)std::exp (-std::log (9.0) / samples);
}

//==============================================================================
void Envelope::noteOn (float attackMs, float decayMs, float sustainLvl,
                       float releaseMs, float keyScale) noexcept
{
    sustainLevel = juce::jlimit (0.0f, 1.0f, sustainLvl);

    const float a = attackMs  * keyScale;
    const float d = decayMs   * keyScale;
    const float r = releaseMs * keyScale;

    const double aSamples = std::max (1.0, (double)a * sampleRate / 1000.0);
    attackIncrement = 1.0f / (float)aSamples;

    decayCoef   = timeToCoef (d, sampleRate);
    releaseCoef = timeToCoef (r, sampleRate);

    phase = Phase::Attack;
}

void Envelope::noteOff (float releaseMs, float keyScale) noexcept
{
    if (phase == Phase::Idle) return;

    releaseCoef = timeToCoef (releaseMs * keyScale, sampleRate);
    phase = Phase::Release;
}

//==============================================================================
float Envelope::process () noexcept
{
    switch (phase)
    {
        case Phase::Attack:
            value += attackIncrement;
            if (value >= 1.0f)
            {
                value = 1.0f;
                phase = Phase::Decay;
            }
            break;

        case Phase::Decay:
            // One-pole toward sustainLevel
            value = decayCoef * value + (1.0f - decayCoef) * sustainLevel;
            if (value <= sustainLevel + 0.001f)
            {
                value = sustainLevel;
                phase = Phase::Sustain;
            }
            break;

        case Phase::Sustain:
            value = sustainLevel;
            break;

        case Phase::Release:
            // Exponential toward 0
            value *= releaseCoef;
            if (value < 0.0001f)
            {
                value = 0.0f;
                phase = Phase::Idle;
            }
            break;

        case Phase::Idle:
        default:
            value = 0.0f;
            break;
    }

    return value;
}
