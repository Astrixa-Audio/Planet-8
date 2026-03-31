#include "Oscillator.h"

void Oscillator::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    phase = 0.0f;
}

void Oscillator::setFrequency(float newFreq)
{
    frequency = newFreq;
}

void Oscillator::setWaveType(int newType)
{
    waveType = newType;
}

float polyBLEP(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    else if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    else
    {
        return 0.0f;
    }
}

void Oscillator::setPulseWidth(float pw)
{
    pulseWidth = pw;
}

float Oscillator::process()
{
    float out = 0.0f;

    switch (waveType)
    {
    case Saw:
    {
        float dt = frequency / sampleRate;

        out = 2.0f * phase - 1.0f;
        out -= polyBLEP(phase, dt);

        break;
    }

    case Pulse:
    {
        float width = 0.2f;
        float dt = frequency / sampleRate;

        out = phase < pulseWidth ? 1.0f : -1.0f;

        out += polyBLEP(phase, dt);
        out -= polyBLEP(fmod(phase + (1.0f - width), 1.0f), dt);

        break;
    }

    case Triangle:
        out = 1.0f - 4.0f * std::abs(phase - 0.5f);
        break;

    case Sine:
        out = std::sin(juce::MathConstants<float>::twoPi * phase);
        break;

    case Square:
    {
        float dt = frequency / sampleRate;

        out = phase < 0.5f ? 1.0f : -1.0f;

        out += polyBLEP(phase, dt);
        out -= polyBLEP(fmod(phase + 0.5f, 1.0f), dt);

        break;
    }

    case Noise:
        out = noise(random);
        break;
    }

    float drift = noise(random) * driftAmount;
    float freqWithDrift = frequency * (1.0f + drift);

    phase += freqWithDrift / sampleRate;

    if (phase >= 1.0f)
        phase -= 1.0f;

    return out * 0.2f;
}