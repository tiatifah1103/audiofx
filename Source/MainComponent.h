#pragma once

#include <JuceHeader.h>

#include "DelayEffect.h"
#include "ReverbEffect.h"
#include "FrequencyBands.h"
#include "DubSiren.h"

class MainComponent
	: public juce::AudioAppComponent,
	  private juce::MidiInputCallback,
	  private juce::Timer
{
public:
	MainComponent();
	~MainComponent() override;

	void prepareToPlay(
		int samplesPerBlockExpected,
		double sampleRate
	) override;

	void getNextAudioBlock(
		const juce::AudioSourceChannelInfo& bufferToFill
	) override;

	void releaseResources() override;

	void paint(
		juce::Graphics& g
	) override;

	void resized() override;

private:
	void handleIncomingMidiMessage(
		juce::MidiInput* source,
		const juce::MidiMessage& message
	) override;

	void initialiseMidi();

	void timerCallback() override;

	void loadPlaylist();

	bool loadTrack(
		int index
	);

	void playNextTrack();

	juce::File getAssetsFolder() const;

	void sendInteraction();

	void sendFloatOSC(
		const juce::String& address,
		float value
	);

	void sendIntOSC(
		const juce::String& address,
		int value
	);


	juce::AudioFormatManager formatManager;

	std::unique_ptr<
		juce::AudioFormatReaderSource
	> readerSource;

	juce::AudioTransportSource transportSource;


	juce::Array<juce::File> playlistFiles;
	juce::StringArray trackNames;

	int currentTrackIndex = -1;


	DelayEffect delayEffect;
	ReverbEffect reverbEffect;
	FrequencyBands frequencyBands;
	DubSiren dubSiren;


	juce::AudioBuffer<float> dryBuffer;
	juce::AudioBuffer<float> dubBuffer;
	juce::AudioBuffer<float> sirenBuffer;


	float outputGain = 1.0f;

	float dubSend = 0.0f;

	float dubCrossfader = 0.5f;

	float delayFeedback = 0.45f;

	float delayTimeMs = 320.0f;

	float reverbRoomSize = 0.0f;
	float reverbWet = 0.0f;
	float reverbDamping = 0.0f;
	float reverbWidth = 1.0f;

	float bassValue = 1.0f;
	float midsValue = 1.0f;
	float topsValue = 1.0f;
	
	float splitScreenDelaySend =
		0.0f;


	std::unique_ptr<
		juce::MidiInput
	> midiInput;

	juce::OSCSender oscSender;


	std::atomic<bool> nextTrackRequested {
		false
	};


	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
		MainComponent
	)
};
