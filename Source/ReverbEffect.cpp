/*
  ==============================================================================

    ReverbEffect.cpp
    Created: 15 Dec 2024 4:34:12pm
    Author:  Latifah Dickson

  ==============================================================================
*/

#include <JuceHeader.h>
#include "ReverbEffect.h"

//==============================================================================
ReverbEffect::ReverbEffect()
{
    
    reverbParams.roomSize = 0.0f;
    reverbParams.damping = 0.0f;
    reverbParams.wetLevel = 0.0f;  // No reverb by default
    reverbParams.dryLevel = 1.0f;  // Full dry signal
    reverbParams.width = 1.0f;
    reverbParams.freezeMode = 0;

    reverb.setParameters(reverbParams);
}

ReverbEffect::~ReverbEffect()
{
}

// Called before playback starts to set up the processor
//--------------------------------------------------------------
void ReverbEffect::prepare(
	double sampleRate,
	int samplesPerBlock
)
{
	juce::ignoreUnused(
		samplesPerBlock
	);


	const double safeSampleRate =
		sampleRate >
			1000.0
		?
		sampleRate
		:
		44100.0;


	reverb.setSampleRate(
		safeSampleRate
	);


	reverb.reset();


	reverb.setParameters(
		reverbParams
	);
}


//--------------------------------------------------------------
void ReverbEffect::process(
	juce::AudioBuffer<float>& buffer
)
{
	const int numChannels =
		buffer.getNumChannels();


	const int numSamples =
		buffer.getNumSamples();


	if (
		numChannels <=
			0 ||
		numSamples <=
			0
	)
	{
		return;
	}


	if (
		numChannels >=
		2
	)
	{
		reverb.processStereo(
			buffer.getWritePointer(
				0
			),
			buffer.getWritePointer(
				1
			),
			numSamples
		);
	}
	else
	{
		reverb.processMono(
			buffer.getWritePointer(
				0
			),
			numSamples
		);
	}
}

// Processes audio block with stereo reverb

// Sets the simulated room size (0.0 to 1.0)
void ReverbEffect::setRoomSize(float size)
{
    reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, size);
    reverb.setParameters(reverbParams);
}

// Sets the amount of damping -- high-frequency absorption
void ReverbEffect::setDamping(float damping)
{
    reverbParams.damping = juce::jlimit(0.0f, 1.0f, damping);
    reverb.setParameters(reverbParams);
}

// Sets how much of the wet / affected signal is heard
void ReverbEffect::setWetLevel(float wet)
{
    reverbParams.wetLevel = juce::jlimit(0.0f, 1.0f, wet);
    reverb.setParameters(reverbParams);
}

// Sets how much of the dry / original signal is heard
void ReverbEffect::setDryLevel(float dry)
{
    reverbParams.dryLevel = juce::jlimit(0.0f, 1.0f, dry);
    reverb.setParameters(reverbParams);
}

// Sets stereo width of the reverb (0 = mono, 1 = wide stereo)
void ReverbEffect::setWidth(float width)
{
    reverbParams.width = juce::jlimit(0.0f, 1.0f, width);
    reverb.setParameters(reverbParams);
}

// Toggle freeze mode -- holds the current reverb indefinitely
void ReverbEffect::setFreezeMode(bool freeze)
{
    reverbParams.freezeMode = freeze ? 1.0f : 0.0f;
    reverb.setParameters(reverbParams);
}
