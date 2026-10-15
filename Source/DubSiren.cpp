#include "DubSiren.h"

#include <cmath>


//--------------------------------------------------------------
bool DubSiren::loadSample(
	const juce::File& file
)
{
	// ============================================================
	// FILE CHECK
	// ============================================================

	if (
		!file.existsAsFile()
	)
	{
		juce::Logger::writeToLog(
			"ERROR: Dub siren WAV not found: " +
			file.getFullPathName()
		);


		return false;
	}


	// ============================================================
	// WAV READER
	// ============================================================

	juce::WavAudioFormat wavFormat;


	std::unique_ptr<
		juce::AudioFormatReader
	> reader(
		wavFormat.createReaderFor(
			file.createInputStream()
				.release(),
			true
		)
	);


	if (
		reader ==
		nullptr
	)
	{
		juce::Logger::writeToLog(
			"ERROR: Could not read dub siren WAV: " +
			file.getFullPathName()
		);


		return false;
	}


	// ============================================================
	// VALIDATE
	// ============================================================

	if (
		reader->lengthInSamples <=
			0 ||
		reader->sampleRate <=
			1000.0
	)
	{
		juce::Logger::writeToLog(
			"ERROR: Invalid dub siren WAV."
		);


		return false;
	}


	// ============================================================
	// COPY WHOLE WAV INTO MEMORY
	//
	// This happens once during startup.
	//
	// Playback itself therefore requires NO disk access.
	// ============================================================

	const int channels =
		juce::jlimit(
			1,
			2,
			static_cast<int>(
				reader->numChannels
			)
		);


	const int samples =
		static_cast<int>(
			reader->lengthInSamples
		);


	sampleBuffer.setSize(
		channels,
		samples
	);


	sampleBuffer.clear();


	reader->read(
		&sampleBuffer,
		0,
		samples,
		0,
		true,
		true
	);


	sourceSampleRate =
		reader->sampleRate;


	readPosition =
		0.0;


	playing =
		false;


	juce::Logger::writeToLog(
		"Loaded dub siren WAV: " +
		file.getFullPathName()
	);


	return true;
}


//--------------------------------------------------------------
void DubSiren::prepare(
	double newOutputSampleRate
)
{
	if (
		newOutputSampleRate >
		1000.0
	)
	{
		outputSampleRate =
			newOutputSampleRate;
	}
	else
	{
		outputSampleRate =
			44100.0;
	}


	readPosition =
		0.0;


	playing =
		false;
}


//--------------------------------------------------------------
void DubSiren::setPitch(
	float normalizedPitch
)
{
	pitch.store(
		juce::jlimit(
			0.0f,
			1.0f,
			normalizedPitch
		)
	);
}


//--------------------------------------------------------------
void DubSiren::trigger()
{
	// ============================================================
	// MIDI THREAD ONLY SETS A FLAG
	//
	// Actual playback reset happens safely inside process().
	// ============================================================

	triggerRequested.store(
		true
	);
}


//--------------------------------------------------------------
float DubSiren::getSampleInterpolated(
	int channel,
	double position
) const
{
	if (
		sampleBuffer.getNumSamples() <
			2 ||
		sampleBuffer.getNumChannels() <
			1
	)
	{
		return 0.0f;
	}


	// ============================================================
	// CHANNEL SAFETY
	//
	// Mono WAV automatically feeds both output channels.
	// ============================================================

	const int safeChannel =
		juce::jlimit(
			0,
			sampleBuffer.getNumChannels() -
				1,
			channel
		);


	const int indexA =
		static_cast<int>(
			position
		);


	const int indexB =
		juce::jmin(
			indexA +
				1,
			sampleBuffer.getNumSamples() -
				1
		);


	const float fraction =
		static_cast<float>(
			position -
			static_cast<double>(
				indexA
			)
		);


	const float sampleA =
		sampleBuffer.getSample(
			safeChannel,
			indexA
		);


	const float sampleB =
		sampleBuffer.getSample(
			safeChannel,
			indexB
		);


	// ============================================================
	// LINEAR INTERPOLATION
	//
	// Prevents harsh staircase artefacts as playback rate changes.
	// ============================================================

	return sampleA +
		(
			sampleB -
			sampleA
		)
		*
		fraction;
}


//--------------------------------------------------------------
void DubSiren::process(
	juce::AudioBuffer<float>& outputBuffer,
	int numSamples
)
{
	// ============================================================
	// ALWAYS BEGIN SILENT
	// ============================================================

	outputBuffer.clear();


	if (
		numSamples <=
			0 ||
		sampleBuffer.getNumSamples() <
			2 ||
		outputSampleRate <=
			1000.0
	)
	{
		return;
	}


	// ============================================================
	// TRIGGER
	//
	// Every button press restarts the sample.
	// ============================================================

	if (
		triggerRequested.exchange(
			false
		)
	)
	{
		readPosition =
			0.0;


		playing =
			true;
	}


	if (
		!playing
	)
	{
		return;
	}


	// ============================================================
	// PITCH
	//
	// CC12 arrives as:
	//
	//     0.0 ---- 0.5 ---- 1.0
	//
	// Convert this to:
	//
	//     -12      0       +12 semitones
	//
	// So the centre of the physical knob gives the WAV's
	// original pitch.
	// ============================================================

	const float normalizedPitch =
		juce::jlimit(
			0.0f,
			1.0f,
			pitch.load()
		);


	const float semitones =
		juce::jmap(
			normalizedPitch,
			-12.0f,
			12.0f
		);


	const double pitchRatio =
		std::pow(
			2.0,
			static_cast<double>(
				semitones
			)
			/
			12.0
		);


	// ============================================================
	// SAMPLE-RATE CORRECTION
	//
	// If the WAV is 44.1 kHz and interface runs at 48 kHz,
	// centre pitch still sounds exactly like the original.
	// ============================================================

	const double sampleRateRatio =
		sourceSampleRate /
		outputSampleRate;


	const double playbackIncrement =
		sampleRateRatio *
		pitchRatio;


	const int outputChannels =
		outputBuffer.getNumChannels();


	const int sourceChannels =
		sampleBuffer.getNumChannels();


	const int sourceLength =
		sampleBuffer.getNumSamples();


	// ============================================================
	// APPROX. 5ms EDGE FADE
	//
	// Helps prevent clicks at the beginning/end.
	// ============================================================

	const double fadeLength =
		std::max(
			1.0,
			sourceSampleRate *
				0.005
		);


	// ============================================================
	// PLAY WAV
	// ============================================================

	for (
		int sample = 0;
		sample <
			numSamples;
		++sample
	)
	{
		if (
			readPosition >=
			static_cast<double>(
				sourceLength -
				1
			)
		)
		{
			playing =
				false;


			readPosition =
				0.0;


			break;
		}


		// ========================================================
		// SMALL START / END FADE
		// ========================================================

		const double distanceFromStart =
			readPosition;


		const double distanceFromEnd =
			static_cast<double>(
				sourceLength -
					1
			)
			-
			readPosition;


		const float startGain =
			static_cast<float>(
				juce::jlimit(
					0.0,
					1.0,
					distanceFromStart /
						fadeLength
				)
			);


		const float endGain =
			static_cast<float>(
				juce::jlimit(
					0.0,
					1.0,
					distanceFromEnd /
						fadeLength
				)
			);


		const float edgeGain =
			std::min(
				startGain,
				endGain
			);


		// ========================================================
		// CHANNELS
		//
		// Mono:
		//     same signal to L + R
		//
		// Stereo:
		//     preserve L + R
		// ========================================================

		for (
			int channel = 0;
			channel <
				outputChannels;
			++channel
		)
		{
			const int sourceChannel =
				sourceChannels ==
					1
				?
				0
				:
				std::min(
					channel,
					sourceChannels -
						1
				);


			const float value =
				getSampleInterpolated(
					sourceChannel,
					readPosition
				)
				*
				edgeGain;


			outputBuffer.setSample(
				channel,
				sample,
				value
			);
		}


		// ========================================================
		// THIS IS THE ACTUAL PITCH SHIFT
		//
		// Faster travel through WAV = higher pitch.
		//
		// Slower travel through WAV = lower pitch.
		// ========================================================

		readPosition +=
			playbackIncrement;
	}
}
