#pragma once

#include <JuceHeader.h>


class FrequencyBands
{
public:

	FrequencyBands();


	void prepare(
		double sampleRate,
		int samplesPerBlock
	);


	void process(
		juce::AudioBuffer<float>& buffer,
		const juce::MidiBuffer& midi
	);


	void setBassGain(
		float gain
	);


	void setMidsGain(
		float gain
	);


	void setTopsGain(
		float gain
	);


private:

	double currentSampleRate =
		44100.0;


	juce::IIRFilter bassLowPass;

	juce::IIRFilter midsBandPass;

	juce::IIRFilter topsHighPass;


	float bassGain =
		1.0f;

	float midsGain =
		1.0f;

	float topsGain =
		1.0f;


	juce::AudioBuffer<float>
		bassBuffer;

	juce::AudioBuffer<float>
		midsBuffer;

	juce::AudioBuffer<float>
		topsBuffer;


	int scratchCapacitySamples =
		0;
};
