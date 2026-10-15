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
//--------------------------------------------------------------
void DelayEffect::prepare(
	double sampleRate,
	int samplesPerBlock
)
{
	juce::ignoreUnused(
		samplesPerBlock
	);


	currentSampleRate =
		sampleRate >
			1000.0
		?
		sampleRate
		:
		44100.0;


	const int maximumDelaySamples =
		std::max(
			2,
			static_cast<int>(
				std::ceil(
					currentSampleRate *
					2.5
				)
			)
			+
			2
		);


	delayBuffer.setSize(
		2,
		maximumDelaySamples
	);


	delayBuffer.clear();


	writePosition =
		0;


	wowPhase =
		0.0;


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
	if (
		bufferToFill.buffer ==
		nullptr
	)
	{
		return;
	}


	if (
		currentSampleRate <=
		1000.0
	)
	{
		return;
	}


	auto& buffer =
		*bufferToFill.buffer;


	const int totalBufferSamples =
		buffer.getNumSamples();


	const int startSample =
		bufferToFill.startSample;


	if (
		startSample <
			0 ||
		startSample >=
			totalBufferSamples
	)
	{
		return;
	}


	const int safeNumSamples =
		std::min(
			bufferToFill.numSamples,
			totalBufferSamples -
				startSample
		);


	if (
		safeNumSamples <=
		0
	)
	{
		return;
	}


	const int numChannels =
		std::min(
			2,
			buffer.getNumChannels()
		);


	if (
		numChannels <=
		0
	)
	{
		return;
	}


	const int delayBufferSize =
		delayBuffer
			.getNumSamples();


	if (
		delayBufferSize <
		2
	)
	{
		return;
	}


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


	constexpr float tapeCutoff =
		4500.0f;


	const float lowPassAmount =
		1.0f -
		std::exp(
			-2.0f *
			juce::MathConstants<float>::
				pi *
			tapeCutoff /
			static_cast<float>(
				currentSampleRate
			)
		);


	constexpr double wowRate =
		0.34;


	constexpr float wowDepthMs =
		6.0f;


	for (
		int sample = 0;
		sample <
			safeNumSamples;
		++sample
	)
	{
		const float wow =
			static_cast<float>(
				std::sin(
					wowPhase
				)
			);


		const float modulatedDelayMs =
			baseDelayMs +
			wow *
				wowDepthMs;


		const int delaySamples =
			juce::jlimit(
				1,
				delayBufferSize -
					1,
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
			readPosition <
			0
		)
		{
			readPosition +=
				delayBufferSize;
		}


		for (
			int channel = 0;
			channel <
				numChannels;
			++channel
		)
		{
			float* output =
				buffer.getWritePointer(
					channel,
					startSample
				);


			const float inputSample =
				output[sample];


			const float delayedSample =
				delayBuffer
					.getSample(
						channel,
						readPosition
					);


			filterState[channel] +=
				lowPassAmount *
				(
					delayedSample -
					filterState[channel]
				);


			float darkRepeat =
				filterState[channel];


			if (
				!std::isfinite(
					darkRepeat
				)
			)
			{
				darkRepeat =
					0.0f;

				filterState[channel] =
					0.0f;
			}


			const float saturatedFeedback =
				std::tanh(
					darkRepeat *
					1.6f
				);


			float outputSample =
				inputSample *
					(
						1.0f -
						currentMix
					)
				+
				darkRepeat *
					currentMix;


			if (
				!std::isfinite(
					outputSample
				)
			)
			{
				outputSample =
					0.0f;
			}


			output[sample] =
				outputSample;


			float feedbackSample =
				inputSample +
				saturatedFeedback *
					currentFeedback;


			if (
				!std::isfinite(
					feedbackSample
				)
			)
			{
				feedbackSample =
					0.0f;
			}


			delayBuffer.setSample(
				channel,
				writePosition,
				feedbackSample
			);
		}


		++writePosition;


		if (
			writePosition >=
			delayBufferSize
		)
		{
			writePosition =
				0;
		}


		wowPhase +=
			(
				juce::MathConstants<double>::
					twoPi *
				wowRate
			)
			/
			currentSampleRate;


		while (
			wowPhase >=
			juce::MathConstants<double>::
				twoPi
		)
		{
			wowPhase -=
				juce::MathConstants<double>::
					twoPi;
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
