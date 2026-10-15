#pragma once

#include <JuceHeader.h>

#include <atomic>

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
	
	bool loadSirenSample();

	
	// ============================================================
	// AUDIO OUTPUT / EXHIBITION ROUTING
	// ============================================================

	// Change this at exhibition install if the interface has
	// a different name.
	//
	// Leave empty "" if you deliberately want the system default.
	juce::String preferredAudioDeviceName {
		"External Headphones"
	};


	// JUCE channel indices are zero-based:
	//
	// 0 = physical output 1
	// 1 = physical output 2
	// 2 = physical output 3
	// 3 = physical output 4
	//
	// etc.
	int preferredOutputLeftChannel =
		0;

	int preferredOutputRightChannel =
		1;


	// Attempts to use the preferred interface.
	//
	// Returns true if the preferred device is successfully active.
	// Falls back safely if it cannot be found.
	bool configurePreferredAudioOutput();


	// Prints the available output devices to the JUCE log.
	// Extremely useful during exhibition setup.
	void logAvailableAudioOutputs() const;


	// Checks whether the preferred interface is currently active.
	bool isPreferredAudioOutputActive() const;


	// Used by the housekeeping timer to avoid checking every frame.
	int audioHealthCounter =
		0;

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
	
	juce::AudioBuffer<float> sirenSample;

	std::atomic<int> sirenSamplePosition { -1 };

	bool sirenSampleLoaded = false;


	// ============================================================
	// THREAD-SAFE LIVE CONTROLS
	//
	// Written by MIDI callback.
	// Read by audio/message threads.
	// ============================================================

	std::atomic<float> outputGain{
		1.0f
	};


	std::atomic<float> dubCrossfader{
		1.0f
	};


	std::atomic<float> dubSend{
		0.0f
	};


	std::atomic<float> delayFeedback{
		0.0f
	};


	std::atomic<float> delayTimeMs{
		320.0f
	};


	std::atomic<float> reverbRoomSize{
		0.0f
	};


	std::atomic<float> reverbWet{
		0.0f
	};


	std::atomic<float> reverbDamping{
		0.0f
	};


	std::atomic<float> reverbWidth{
		1.0f
	};


	std::atomic<float> bassValue{
		1.0f
	};


	std::atomic<float> midsValue{
		1.0f
	};


	std::atomic<float> topsValue{
		1.0f
	};


	// This one is only calculated inside the MIDI callback.
	float splitScreenDelaySend =
		0.0f;


	// ============================================================
	// AUDIO SCRATCH CAPACITY
	// ============================================================

	int scratchCapacitySamples =
		0;


	// ============================================================
	// MIDI HEALTH / HOTPLUG
	// ============================================================

	juce::String midiDeviceIdentifier;

	int midiHealthCounter =
		0;


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
