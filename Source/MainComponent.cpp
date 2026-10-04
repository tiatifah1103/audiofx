#include "MainComponent.h"

namespace
{
	constexpr int dubCrossfaderCC =
		13;

	constexpr int sirenPitchCC =
		16;

	constexpr int sirenTriggerNote =
		65;


	juce::String getMidiControlName(
		int cc
	)
	{
		switch (
			cc
		)
		{
			case 8:
				return "DELAY SEND";

			case 9:
				return "DELAY TIME";

			case 10:
				return "SPLIT SCREEN + SPLIT DELAY";

			case 11:
				return "REVERB WIDTH";

			case 12:
				return "REVERB DAMPING";

			case 13:
				return "DUB CROSSFADER";

			case 14:
				return "HEADPHONE OUTPUT GAIN";

			case 16:
				return "SIREN PITCH";

			case 17:
				return "TOPS";

			case 18:
				return "REVERB WET";

			case 19:
				return "MIDS";

			case 20:
				return "REVERB ROOM SIZE";

			case 21:
				return "BASS";

			case 23:
				return "DELAY FEEDBACK";

			case 24:
				return "SECONDARY JOG";

			case 25:
				return "JOG";

			default:
				return "UNMAPPED CC";
		}
	}


	juce::String getMidiNoteName(
		int note
	)
	{
		switch (
			note
		)
		{
			case 51:
			case 60:
				return "CHANGE TOPIC";

			case 64:
				return "VIDEO ADVANCE";

			case 65:
				return "DUB SIREN";

			case 66:
				return "SPLIT SCREEN ADVANCE";

			default:
				return "UNMAPPED NOTE";
		}
	}
}

//--------------------------------------------------------------
MainComponent::MainComponent()
{
	formatManager.registerBasicFormats();

	oscSender.connect(
		"127.0.0.1",
		9000
	);

	loadPlaylist();
	initialiseMidi();

	setSize(
		600,
		400
	);

	setAudioChannels(
		0,
		2
	);

	startTimerHz(
		20
	);

	if (
		!playlistFiles.isEmpty()
	)
	{
		loadTrack(
			0
		);
	}
}

//--------------------------------------------------------------
MainComponent::~MainComponent()
{
	stopTimer();

	if (
		midiInput != nullptr
	)
	{
		midiInput->stop();
		midiInput.reset();
	}

	transportSource.stop();

	transportSource.setSource(
		nullptr
	);

	readerSource.reset();

	shutdownAudio();
}

//--------------------------------------------------------------
void MainComponent::prepareToPlay(
	int samplesPerBlockExpected,
	double sampleRate
)
{
	transportSource.prepareToPlay(
		samplesPerBlockExpected,
		sampleRate
	);

	delayEffect.prepare(
		sampleRate,
		samplesPerBlockExpected
	);

	reverbEffect.prepare(
		sampleRate,
		samplesPerBlockExpected
	);

	frequencyBands.prepare(
		sampleRate,
		samplesPerBlockExpected
	);

	dubSiren.prepare(
		sampleRate
	);


	// Start delay completely dry.
	// CC8 controls how much tape echo is heard.
	delayEffect.setMix(
		0.0f
	);

	delayEffect.setFeedback(
		delayFeedback
	);

	delayEffect.setDelayTime(
		static_cast<int>(
			delayTimeMs
		)
	);


	dryBuffer.setSize(
		2,
		samplesPerBlockExpected
	);

	dubBuffer.setSize(
		2,
		samplesPerBlockExpected
	);

	sirenBuffer.setSize(
		2,
		samplesPerBlockExpected
	);
}

//--------------------------------------------------------------
void MainComponent::getNextAudioBlock(
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


	auto& output =
		*bufferToFill.buffer;


	bufferToFill.clearActiveBufferRegion();


	// ============================================================
	// PLAY CURRENT MUSIC
	// ============================================================

	transportSource.getNextAudioBlock(
		bufferToFill
	);


	// ============================================================
	// EQ / FREQUENCY BANDS
	// ============================================================

	juce::MidiBuffer emptyMidi;


	frequencyBands.process(
		output,
		emptyMidi
	);


	const int channels =
		std::min(
			2,
			output.getNumChannels()
		);


	const int numSamples =
		bufferToFill.numSamples;


	// ============================================================
	// PREPARE PARALLEL AUDIO PATHS
	// ============================================================

	dryBuffer.clear();
	dubBuffer.clear();
	sirenBuffer.clear();


	for (
		int channel = 0;
		channel < channels;
		++channel
	)
	{
		// --------------------------------------------------------
		// CLEAN RECORD
		// --------------------------------------------------------

		dryBuffer.copyFrom(
			channel,
			0,
			output,
			channel,
			bufferToFill.startSample,
			numSamples
		);


		// --------------------------------------------------------
		// DUB VERSION
		//
		// Starts as an identical copy.
		// We then process this copy with echo/reverb.
		// --------------------------------------------------------

		dubBuffer.copyFrom(
			channel,
			0,
			dryBuffer,
			channel,
			0,
			numSamples
		);
	}


	// ============================================================
	// DUB SIREN
	// ============================================================

	dubSiren.process(
		sirenBuffer,
		numSamples
	);


	// ============================================================
	// ADD SIREN INTO DUB PATH
	//
	// This means the siren also catches the tape echo/reverb.
	// ============================================================

	for (
		int channel = 0;
		channel < channels;
		++channel
	)
	{
		dubBuffer.addFrom(
			channel,
			0,
			sirenBuffer,
			channel,
			0,
			numSamples,
			0.65f
		);
	}


	// ============================================================
	// TAPE ECHO
	//
	// CC8 now controls the DelayEffect wet/dry amount directly.
	// ============================================================

	juce::AudioSourceChannelInfo dubInfo(
		&dubBuffer,
		0,
		numSamples
	);


	delayEffect.process(
		dubInfo
	);


	// ============================================================
	// REVERB
	//
	// CC18 controls its wet level exactly as before.
	// Room size / damping / width remain independent controls.
	// ============================================================

	reverbEffect.process(
		dubBuffer
	);


	// ============================================================
	// CUSTOM DUB CROSSFADER
	//
	// 0.0 = original record
	//
	// 0.5 = original + dub treatment
	//
	// 1.0 = fully processed dub version
	//
	// Linear crossfade is deliberate here:
	// because both sides are related signals, this avoids the
	// unnecessary gain boost that equal-power mixing caused.
	// ============================================================

	const float x =
		juce::jlimit(
			0.0f,
			1.0f,
			dubCrossfader
		);


	const float dryGain =
		1.0f -
		x;


	const float dubGain =
		x;


	// ============================================================
	// REBUILD OUTPUT
	// ============================================================

	output.clear(
		bufferToFill.startSample,
		numSamples
	);


	for (
		int channel = 0;
		channel < channels;
		++channel
	)
	{
		// Clean record.

		output.addFrom(
			channel,
			bufferToFill.startSample,
			dryBuffer,
			channel,
			0,
			numSamples,
			dryGain
		);


		// Processed dub version.

		output.addFrom(
			channel,
			bufferToFill.startSample,
			dubBuffer,
			channel,
			0,
			numSamples,
			dubGain
		);


		// Some direct siren remains present even if the
		// crossfader is towards the clean record.

		output.addFrom(
			channel,
			bufferToFill.startSample,
			sirenBuffer,
			channel,
			0,
			numSamples,
			0.35f
		);
	}


	// ============================================================
	// MASTER OUTPUT
	// ============================================================

	output.applyGain(
		bufferToFill.startSample,
		numSamples,
		outputGain
	);


	// ============================================================
	// SAFETY LIMIT
	// ============================================================

	for (
		int channel = 0;
		channel < channels;
		++channel
	)
	{
		float* samples =
			output.getWritePointer(
				channel,
				bufferToFill.startSample
			);


		for (
			int sample = 0;
			sample < numSamples;
			++sample
		)
		{
			samples[sample] =
				juce::jlimit(
					-1.0f,
					1.0f,
					samples[sample]
				);
		}
	}


	// ============================================================
	// PLAYLIST ADVANCE
	// ============================================================

	if (
		transportSource.hasStreamFinished()
	)
	{
		nextTrackRequested.store(
			true
		);
	}
}
//--------------------------------------------------------------
void MainComponent::timerCallback()
{
	if (
		nextTrackRequested.exchange(
			false
		)
	)
	{
		playNextTrack();
	}
}

//--------------------------------------------------------------
void MainComponent::releaseResources()
{
	transportSource.releaseResources();
}

//--------------------------------------------------------------
void MainComponent::paint(
	juce::Graphics& g
)
{
	g.fillAll(
		juce::Colours::black
	);

	g.setColour(
		juce::Colours::white
	);

	g.setFont(
		18.0f
	);

	juce::String text =
		"Dread Frequencies";

	if (
		currentTrackIndex >= 0 &&
		currentTrackIndex <
			trackNames.size()
	)
	{
		text +=
			"\n\n" +
			trackNames[
				currentTrackIndex
			];
	}

	text +=
		"\n\nDub Send: " +
		juce::String(
			dubSend,
			2
		);

	text +=
		"\nDub Mix: " +
		juce::String(
			dubCrossfader,
			2
		);

	g.drawFittedText(
		text,
		getLocalBounds().reduced(
			20
		),
		juce::Justification::centred,
		8
	);
}

//--------------------------------------------------------------
void MainComponent::resized()
{
}

//--------------------------------------------------------------
juce::File MainComponent::getAssetsFolder() const
{
	juce::File current =
		juce::File::getSpecialLocation(
			juce::File::currentApplicationFile
		);

	if (
		current.existsAsFile()
	)
	{
		current =
			current.getParentDirectory();
	}

	for (
		int i = 0;
		i < 12;
		++i
	)
	{
		auto assets =
			current.getChildFile(
				"Assets"
			);

		if (
			assets.isDirectory()
		)
			return assets;

		assets =
			current.getChildFile(
				"assets"
			);

		if (
			assets.isDirectory()
		)
			return assets;

		auto parent =
			current.getParentDirectory();

		if (
			parent ==
			current
		)
			break;

		current =
			parent;
	}

	auto cwd =
		juce::File::
			getCurrentWorkingDirectory();

	auto fallback =
		cwd.getChildFile(
			"Assets"
		);

	if (
		fallback.isDirectory()
	)
		return fallback;

	return cwd.getChildFile(
		"assets"
	);
}

//--------------------------------------------------------------
void MainComponent::loadPlaylist()
{
	playlistFiles.clear();
	trackNames.clear();

	const auto assetsFolder =
		getAssetsFolder();

	if (
		!assetsFolder.isDirectory()
	)
		return;

	const auto playlistFile =
		assetsFolder.getChildFile(
			"playlist.json"
		);

	if (
		!playlistFile.existsAsFile()
	)
		return;

	const juce::var json =
		juce::JSON::parse(
			playlistFile.loadFileAsString()
		);

	juce::var tracks =
		json;

	if (
		auto* object =
			json.getDynamicObject()
	)
	{
		tracks =
			object->getProperty(
				"tracks"
			);
	}

	if (
		!tracks.isArray()
	)
		return;

	for (
		const auto& track :
		*tracks.getArray()
	)
	{
		auto* object =
			track.getDynamicObject();

		if (
			object ==
			nullptr
		)
			continue;

		juce::String filename =
			object->getProperty(
				"file"
			).toString();

		if (
			filename.isEmpty()
		)
		{
			filename =
				object->getProperty(
					"filename"
				).toString();
		}

		if (
			filename.isEmpty()
		)
			continue;

		auto file =
			assetsFolder.getChildFile(
				filename
			);

		if (
			!file.existsAsFile()
		)
		{
			file =
				assetsFolder
					.getChildFile(
						"audio"
					)
					.getChildFile(
						filename
					);
		}

		if (
			!file.existsAsFile()
		)
			continue;

		juce::String title =
			object->getProperty(
				"name"
			).toString();

		if (
			title.isEmpty()
		)
		{
			title =
				object->getProperty(
					"title"
				).toString();
		}

		if (
			title.isEmpty()
		)
		{
			title =
				file.getFileNameWithoutExtension();
		}

		playlistFiles.add(
			file
		);

		trackNames.add(
			title
		);
	}
}

//--------------------------------------------------------------
bool MainComponent::loadTrack(
	int index
)
{
	if (
		index < 0 ||
		index >=
			playlistFiles.size()
	)
		return false;

	auto file =
		playlistFiles[
			index
		];

	std::unique_ptr<
		juce::AudioFormatReader
	> reader(
		formatManager.createReaderFor(
			file
		)
	);

	if (
		reader == nullptr
	)
		return false;

	transportSource.stop();

	transportSource.setSource(
		nullptr
	);

	readerSource.reset();

	auto newSource =
		std::make_unique<
			juce::AudioFormatReaderSource
		>(
			reader.release(),
			true
		);

	const double sourceRate =
		newSource
			->getAudioFormatReader()
			->sampleRate;

	transportSource.setSource(
		newSource.get(),
		0,
		nullptr,
		sourceRate
	);

	readerSource =
		std::move(
			newSource
		);

	currentTrackIndex =
		index;

	transportSource.setPosition(
		0.0
	);

	transportSource.start();

	repaint();

	return true;
}

//--------------------------------------------------------------
void MainComponent::playNextTrack()
{
	if (
		playlistFiles.isEmpty()
	)
		return;

	int next =
		currentTrackIndex + 1;

	if (
		next >=
		playlistFiles.size()
	)
		next = 0;

	loadTrack(
		next
	);
}

//--------------------------------------------------------------
void MainComponent::initialiseMidi()
{
	auto devices =
		juce::MidiInput::
			getAvailableDevices();

	if (
		devices.isEmpty()
	)
		return;

	midiInput =
		juce::MidiInput::openDevice(
			devices[0].identifier,
			this
		);

	if (
		midiInput != nullptr
	)
	{
		midiInput->start();
	}
}

//--------------------------------------------------------------
void MainComponent::sendFloatOSC(
	const juce::String& address,
	float value
)
{
	juce::OSCMessage message(
		address
	);

	message.addFloat32(
		value
	);

	oscSender.send(
		message
	);
}

//--------------------------------------------------------------
void MainComponent::sendIntOSC(
	const juce::String& address,
	int value
)
{
	juce::OSCMessage message(
		address
	);

	message.addInt32(
		value
	);

	oscSender.send(
		message
	);
}

//--------------------------------------------------------------
void MainComponent::sendInteraction()
{
	sendIntOSC(
		"/interaction",
		1
	);
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void MainComponent::handleIncomingMidiMessage(
	juce::MidiInput* source,
	const juce::MidiMessage& message
)
{
	juce::ignoreUnused(
		source
	);


	// ============================================================
	// CONTINUOUS CONTROLS
	// ============================================================

	if (
		message.isController()
	)
	{
		const int cc =
			message.getControllerNumber();


		const int raw =
			message.getControllerValue();


		const float value =
			juce::jlimit(
				0.0f,
				1.0f,
				raw /
					127.0f
			);


		// ========================================================
		// DEBUG LOG
		// ========================================================

		juce::Logger::writeToLog(
			"[MIDI CC] "
			+
			getMidiControlName(
				cc
			)
			+
			" | CC "
			+
			juce::String(
				static_cast<int>(
					cc
				)
			)
			+
			" | raw "
			+
			juce::String(
				static_cast<int>(
					raw
				)
			)
			+
			" | normalised "
			+
			juce::String(
				static_cast<double>(
					value
				),
				3
			)
		);

		// ========================================================
		// HEADPHONE VOLUME
		// ========================================================

		if (
			cc ==
			14
		)
		{
			outputGain =
				value;
		}


		// ========================================================
		// REVERB ROOM
		// ========================================================

		else if (
			cc ==
			20
		)
		{
			reverbRoomSize =
				value;


			reverbEffect.setRoomSize(
				value
			);


			sendFloatOSC(
				"/reverb/roomSize",
				value
			);
		}


		// ========================================================
		// REVERB WET
		// ========================================================

		else if (
			cc ==
			18
		)
		{
			reverbWet =
				value;


			reverbEffect.setWetLevel(
				value
			);


			sendFloatOSC(
				"/reverb/wetLevel",
				value
			);
		}


		// ========================================================
		// REVERB DAMPING
		// ========================================================

		else if (
			cc ==
			12
		)
		{
			reverbDamping =
				value;


			reverbEffect.setDamping(
				value
			);


			sendFloatOSC(
				"/reverb/damping",
				value
			);
		}


		// ========================================================
		// REVERB WIDTH
		// ========================================================

		else if (
			cc ==
			11
		)
		{
			reverbWidth =
				value;


			reverbEffect.setWidth(
				value
			);


			sendFloatOSC(
				"/reverb/width",
				value
			);
		}


		// ========================================================
		// BASS
		// ========================================================

		else if (
			cc ==
			21
		)
		{
			bassValue =
				value;


			frequencyBands.setBassGain(
				value
			);


			sendFloatOSC(
				"/eq/bass",
				value
			);
		}


		// ========================================================
		// MIDS
		// ========================================================

		else if (
			cc ==
			19
		)
		{
			midsValue =
				value;


			frequencyBands.setMidsGain(
				value
			);


			sendFloatOSC(
				"/eq/mids",
				value
			);
		}


		// ========================================================
		// TOPS
		// ========================================================

		else if (
			cc ==
			17
		)
		{
			topsValue =
				value;


			frequencyBands.setTopsGain(
				value
			);


			sendFloatOSC(
				"/eq/tops",
				value
			);
		}


		// ========================================================
		// MAIN DELAY SEND
		//
		// CC8 remains the proper intentional dub send.
		//
		// The split-screen slider can contribute additional delay,
		// but it never reduces a stronger CC8 setting.
		// ========================================================

		else if (
			cc ==
			8
		)
		{
			dubSend =
				value;


			const float effectiveDelaySend =
				std::max(
					dubSend,
					splitScreenDelaySend
				);


			delayEffect.setMix(
				effectiveDelaySend
			);


			sendFloatOSC(
				"/delay/mixValue",
				dubSend
			);


			juce::Logger::writeToLog(
				"[AUDIO] Effective Delay Send = "
				+
									 juce::String(
										 static_cast<double>(
											 effectiveDelaySend
										 ),
										 3
									 )
			);
		}


		// ========================================================
		// DELAY FEEDBACK
		// ========================================================

		else if (
			cc ==
			23
		)
		{
			delayFeedback =
				value;


			delayEffect.setFeedback(
				value
			);


			sendFloatOSC(
				"/delay/feedbackValue",
				value
			);
		}


		// ========================================================
		// DELAY TIME
		// ========================================================

		else if (
			cc ==
			9
		)
		{
			delayTimeMs =
				juce::jmap(
					value,
					90.0f,
					900.0f
				);


			delayEffect.setDelayTime(
				static_cast<int>(
					delayTimeMs
				)
			);


			sendFloatOSC(
				"/delay/delayTime",
				delayTimeMs
			);
		}


		// ========================================================
		// DUB CROSSFADER
		// ========================================================

		else if (
			cc ==
			dubCrossfaderCC
		)
		{
			dubCrossfader =
				value;


			sendFloatOSC(
				"/dub/crossfader",
				value
			);
		}


		// ========================================================
		// SIREN PITCH
		// ========================================================

		else if (
			cc ==
			sirenPitchCC
		)
		{
			dubSiren.setPitch(
				value
			);


			sendFloatOSC(
				"/dub/sirenPitch",
				value
			);
		}


		// ========================================================
		// SPLIT SCREEN
		//
		// The visual slider now also creates a SUBTLE parallel
		// delay send.
		//
		// At 0:
		// no contribution.
		//
		// At 1:
		// roughly 38% delay send.
		//
		// Existing CC8 dubSend always takes priority if stronger.
		// ========================================================

		else if (
			cc ==
			10
		)
		{
			sendFloatOSC(
				"/splitScreen/amount",
				value
			);


			splitScreenDelaySend =
				pow(
					value,
					0.82f
				)
				*
				0.38f;


			const float effectiveDelaySend =
				std::max(
					dubSend,
					splitScreenDelaySend
				);


			delayEffect.setMix(
				effectiveDelaySend
			);


			juce::Logger::writeToLog(
				"[SPLIT AUDIO] split="
				+
				juce::String(
					static_cast<double>(
						value
					),
					3
				)
				+
				" delayContribution="
				+
				juce::String(
					static_cast<double>(
						splitScreenDelaySend
					),
					3
				)
				+
				" effectiveDelay="
				+
				juce::String(
					static_cast<double>(
						effectiveDelaySend
					),
					3
				)
			);
		}


		// ========================================================
		// JOG
		// ========================================================

		else if (
			cc ==
			25
		)
		{
			sendIntOSC(
				"/chronology/jog",
				raw
			);
		}


		// ========================================================
		// SECONDARY JOG
		// ========================================================

		else if (
			cc ==
			24
		)
		{
			sendIntOSC(
				"/chronology/jogSecondary",
				raw
			);
		}


		sendInteraction();


		return;
	}


	// ============================================================
	// BUTTONS
	// ============================================================

	if (
		message.isNoteOn()
	)
	{
		const int note =
			message.getNoteNumber();


		juce::Logger::writeToLog(
			"[MIDI NOTE] "
			+
			getMidiNoteName(
				note
			)
			+
			" | note "
			+
			juce::String(
				static_cast<int>(
					note
				)
			)
			+
			" | velocity "
			+
			juce::String(
				static_cast<double>(
					message.getVelocity()
				),
				3
			)
		);


		if (
			note ==
			sirenTriggerNote
		)
		{
			dubSiren.trigger();


			sendIntOSC(
				"/dub/siren",
				1
			);
		}


		else if (
			note ==
			64
		)
		{
			sendIntOSC(
				"/video/advance",
				1
			);
		}


		else if (
			note ==
			66
		)
		{
			sendIntOSC(
				"/splitScreen/advance",
				1
			);
		}


		else if (
			note ==
				60 ||
			note ==
				51
		)
		{
			sendIntOSC(
				"/chronology/randomTopic",
				1
			);
		}


		sendInteraction();
	}
}
