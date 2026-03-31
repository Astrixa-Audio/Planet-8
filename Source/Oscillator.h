#pragma once
#include <JuceHeader.h>
#include <random>

class Oscillator
{
public:
    enum WaveType
    {
        Saw = 0,
        Pulse,
        Triangle,
        Sine,
        Square,
        Noise
    };

    void  prepare(double newSampleRate);
    void  setFrequency(float newFreq);
    void  setWaveType(int newType);
    void  setPulseWidth(float pw);
    void  resetPhase() { phase = 0.0f; }
    float getPhase()   const { return phase; }

    float process();

private:
    double sampleRate = 44100.0;
    float frequency = 440.0f;
    float phase = 0.0f;
    float pulseWidth = 0.5f;
    float driftPhase = 0.0f;
    float driftAmount = 0.002f;
 
    int waveType = Sine;

    std::mt19937 random;
    std::uniform_real_distribution<float> noise{ -1.0f, 1.0f };
};