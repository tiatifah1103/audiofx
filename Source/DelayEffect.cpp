/*
  ==============================================================================

    DelayEffect.cpp
    Created: 13 Dec 2024 3:27:36pm
    Author:  Latifah Dickson

  ==============================================================================
*/

#include "DelayEffect.h"
#include <cmath>

//--------------------------------------------------------------
void DelayEffect::prepare(
	double sampleRate,
	int samplesPerBlock
)
{
	juce::ignoreUnused(samplesPerBlock);

	currentSampleRate = sampleRate;

	const int maximumDelaySamples =
		static_cast<int>(
			currentSampleRate * 2.5
		);

	delayBuffer.setSize(
		2,
		maximumDelaySamples
	);

	delayBuffer.clear();

	writePosition = 0;
	wowPhase = 0.0;

	filterState = {
		0.0f,
		0.0f
	};
}

//--------------------------------------------------------------
void DelayEffect::process(
	const juce::AudioSourceChannelInfo& bufferToFill
)
{
	if (bufferToFill.buffer == nullptr)
		return;

	auto& buffer =
		*bufferToFill.buffer;

	const int numChannels =
		std::min(
			2,
			buffer.getNumChannels()
		);

	const int numSamples =
		bufferToFill.numSamples;

	const int delayBufferSize =
		delayBuffer.getNumSamples();

	if (
		delayBufferSize <= 0 ||
		numSamples <= 0
	)
		return;

	const float currentFeedback =
		juce::jlimit(
			0.0f,
			0.92f,
			feedback.load()
		);

	const float currentMix =
		juce::jlimit(
			0.0f,
			1.0f,
			mix.load()
		);

	const float baseDelayMs =
		juce::jlimit(
			50.0f,
			2000.0f,
			delayTimeMs.load()
		);

	// Darken every repeat like a tape echo.
	constexpr float tapeCutoff =
		4500.0f;

	const float lowPassAmount =
		1.0f -
		std::exp(
			-2.0f *
			juce::MathConstants<float>::pi *
			tapeCutoff /
			static_cast<float>(
				currentSampleRate
			)
		);

	// Slow tape-speed modulation.
	constexpr double wowRate =
		0.34;

	constexpr float wowDepthMs =
		6.0f;

	for (
		int sample = 0;
		sample < numSamples;
		++sample
	)
	{
		const float wow =
			std::sin(
				wowPhase
			);

		const float modulatedDelayMs =
			baseDelayMs +
			wow * wowDepthMs;

		const int delaySamples =
			juce::jlimit(
				1,
				delayBufferSize - 1,
				static_cast<int>(
					currentSampleRate *
					modulatedDelayMs /
					1000.0
				)
			);

		int readPosition =
			writePosition -
			delaySamples;

		while (
			readPosition < 0
		)
		{
			readPosition +=
				delayBufferSize;
		}

		for (
			int channel = 0;
			channel < numChannels;
			++channel
		)
		{
			float* output =
				buffer.getWritePointer(
					channel,
					bufferToFill.startSample
				);

			const float inputSample =
				output[sample];

			const float delayedSample =
				delayBuffer.getSample(
					channel,
					readPosition
				);

			// Tape repeats progressively lose top end.
			filterState[channel] +=
				lowPassAmount *
				(
					delayedSample -
					filterState[channel]
				);

			const float darkRepeat =
				filterState[channel];

			// Soft tape-style saturation in feedback path.
			const float saturatedFeedback =
				std::tanh(
					darkRepeat * 1.6f
				);

			output[sample] =
				inputSample *
					(1.0f - currentMix)
				+
				darkRepeat *
					currentMix;

			delayBuffer.setSample(
				channel,
				writePosition,
				inputSample +
				saturatedFeedback *
					currentFeedback
			);
		}

		++writePosition;

		if (
			writePosition >=
			delayBufferSize
		)
		{
			writePosition = 0;
		}

		wowPhase +=
			(
				juce::MathConstants<double>::twoPi *
				wowRate
			)
			/
			currentSampleRate;

		if (
			wowPhase >=
			juce::MathConstants<double>::twoPi
		)
		{
			wowPhase -=
				juce::MathConstants<double>::twoPi;
		}
	}
}

//--------------------------------------------------------------
void DelayEffect::setDelayTime(
	int newDelayTimeMs
)
{
	delayTimeMs.store(
		static_cast<float>(
			newDelayTimeMs
		)
	);
}

//--------------------------------------------------------------
void DelayEffect::setFeedback(
	float newFeedback
)
{
	feedback.store(
		juce::jlimit(
			0.0f,
			0.92f,
			newFeedback
		)
	);
}

//--------------------------------------------------------------
void DelayEffect::setMix(
	float newMix
)
{
	mix.store(
		juce::jlimit(
			0.0f,
			1.0f,
			newMix
		)
	);
}
