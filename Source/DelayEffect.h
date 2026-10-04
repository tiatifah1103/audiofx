#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class DelayEffect
{
public:
	DelayEffect() = default;
	~DelayEffect() = default;

	void prepare(double sampleRate, int samplesPerBlock);
	void process(const juce::AudioSourceChannelInfo& bufferToFill);

	void setDelayTime(int newDelayTimeMs);
	void setFeedback(float newFeedback);
	void setMix(float newMix);

private:
	juce::AudioBuffer<float> delayBuffer;

	int writePosition = 0;

	double currentSampleRate = 44100.0;
	double wowPhase = 0.0;

	std::array<float, 2> filterState { 0.0f, 0.0f };

	std::atomic<float> delayTimeMs { 320.0f };
	std::atomic<float> feedback { 0.45f };
	std::atomic<float> mix { 1.0f };
};
