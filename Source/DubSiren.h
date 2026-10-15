#pragma once

#include <JuceHeader.h>


class DubSiren
{
public:

	DubSiren() = default;


	// ============================================================
	// LOAD WAV
	// ============================================================

	bool loadSample(
		const juce::File& file
	);


	// ============================================================
	// AUDIO SETUP
	// ============================================================

	void prepare(
		double newOutputSampleRate
	);


	// ============================================================
	// CONTROLS
	//
	// 0.0 = one octave down
	// 0.5 = original pitch
	// 1.0 = one octave up
	// ============================================================

	void setPitch(
		float normalizedPitch
	);


	// Restart the WAV from the beginning.
	void trigger();


	// ============================================================
	// AUDIO
	// ============================================================

	void process(
		juce::AudioBuffer<float>& outputBuffer,
		int numSamples
	);


private:

	// ============================================================
	// SAMPLE
	// ============================================================

	juce::AudioBuffer<float> sampleBuffer;


	double sourceSampleRate =
		44100.0;


	double outputSampleRate =
		44100.0;


	// Fractional position allows smooth pitch shifting.
	double readPosition =
		0.0;


	// ============================================================
	// STATE
	// ============================================================

	bool playing =
		false;


	std::atomic<bool> triggerRequested {
		false
	};


	std::atomic<float> pitch {
		0.5f
	};


	// ============================================================
	// HELPERS
	// ============================================================

	float getSampleInterpolated(
		int channel,
		double position
	) const;
};
