/*
  ==============================================================================

    DubSiren.cpp
    Created: 20 Sep 2026 4:51:35pm
    Author:  Latifah Dickson

  ==============================================================================
*/

#include "DubSiren.h"
#include <cmath>

//--------------------------------------------------------------
void DubSiren::prepare(
    double newSampleRate
)
{
    sampleRate =
        newSampleRate;

    phase = 0.0;
    lfoPhase = 0.0;
    envelope = 0.0f;
}

//--------------------------------------------------------------
void DubSiren::trigger()
{
    triggerRequested.store(
        true
    );
}

//--------------------------------------------------------------
void DubSiren::setPitch(
    float normalisedPitch
)
{
    pitchControl.store(
        juce::jlimit(
            0.0f,
            1.0f,
            normalisedPitch
        )
    );
}

//--------------------------------------------------------------
void DubSiren::process(
    juce::AudioBuffer<float>& buffer,
    int numSamples
)
{
    if (
        triggerRequested.exchange(
            false
        )
    )
    {
        envelope =
            1.0f;
    }

    const float pitch =
        pitchControl.load();

    const float baseFrequency =
        juce::jmap(
            pitch,
            420.0f,
            1250.0f
        );

    const float envelopeDecay =
        std::exp(
            -1.0f /
            (
                1.7f *
                static_cast<float>(
                    sampleRate
                )
            )
        );

    for (
        int sample = 0;
        sample < numSamples;
        ++sample
    )
    {
        if (
            envelope <
            0.0001f
        )
        {
            envelope = 0.0f;
            break;
        }

        const float lfo =
            static_cast<float>(
                std::sin(
                    lfoPhase
                )
            );

        const float frequency =
            baseFrequency *
            (
                1.0f +
                lfo * 0.12f
            );

        const float oscillator =
            static_cast<float>(
                std::sin(
                    phase
                )
            );

        // Slightly squared-off analogue siren tone.
        const float dirtyOscillator =
            std::tanh(
                oscillator * 2.4f
            );

        const float sampleValue =
            dirtyOscillator *
            envelope *
            0.20f;

        for (
            int channel = 0;
            channel <
                buffer.getNumChannels();
            ++channel
        )
        {
            buffer.addSample(
                channel,
                sample,
                sampleValue
            );
        }

        phase +=
            (
                juce::MathConstants<double>::twoPi *
                frequency
            )
            /
            sampleRate;

        lfoPhase +=
            (
                juce::MathConstants<double>::twoPi *
                5.2
            )
            /
            sampleRate;

        if (
            phase >=
            juce::MathConstants<double>::twoPi
        )
            phase -=
                juce::MathConstants<double>::twoPi;

        if (
            lfoPhase >=
            juce::MathConstants<double>::twoPi
        )
            lfoPhase -=
                juce::MathConstants<double>::twoPi;

        envelope *=
            envelopeDecay;
    }
}