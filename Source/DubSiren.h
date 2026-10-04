/*
  ==============================================================================

    DubSiren.h
    Created: 20 Sep 2026 4:51:35pm
    Author:  Latifah Dickson

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>

class DubSiren
{
public:
    void prepare(double sampleRate);

    void trigger();

    void setPitch(float normalisedPitch);

    void process(
        juce::AudioBuffer<float>& buffer,
        int numSamples
    );

private:
    double sampleRate = 44100.0;

    double phase = 0.0;
    double lfoPhase = 0.0;

    float envelope = 0.0f;

    std::atomic<bool> triggerRequested {
        false
    };

    std::atomic<float> pitchControl {
        0.45f
    };
};