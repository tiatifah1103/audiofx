#include "MainComponent.h"

namespace
{
	constexpr int dubCrossfaderCC =
		11;

	constexpr int sirenPitchCC =
		12;

	constexpr int sirenTriggerNote =
		79;


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
			return "DUB SEND AMOUNT";

		case 12:
			return "DUB SIREN PITCH";

		case 14:
			return "HEADPHONE OUTPUT GAIN";

		case 16:
			return "REVERB DAMPING";

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
//--------------------------------------------------------------
MainComponent::MainComponent()
{
	// ============================================================
	// SAFE INITIAL STATE
	// ============================================================

	nextTrackRequested.store(
		false
	);


	currentTrackIndex =
		-1;


	// ============================================================
	// AUDIO FORMAT SUPPORT
	//
	// Must happen before loading playlist or siren WAV.
	// ============================================================

	formatManager
		.registerBasicFormats();


	// ============================================================
	// OSC
	// ============================================================

	if (
		!oscSender.connect(
			"127.0.0.1",
			9000
		)
	)
	{
		juce::Logger::writeToLog(
			"WARNING: OSC sender could not initialise."
		);
	}


	// ============================================================
	// ASSETS
	// ============================================================

	loadPlaylist();


	if (
		!loadSirenSample()
	)
	{
		juce::Logger::writeToLog(
			"WARNING: Dub siren WAV is unavailable. "
			"Main playlist can continue without it."
		);
	}


	// ============================================================
	// WINDOW
	// ============================================================

	setSize(
		600,
		400
	);


	// ============================================================
	// AUDIO
	//
	// FIRST:
	// Ask JUCE for a normal stereo output.
	//
	// This ensures there is always a valid fallback.
	// ============================================================

	setAudioChannels(
		0,
		2
	);


	// ============================================================
	// THEN:
	// Try to move that stereo output to the exhibition interface.
	//
	// If GIGAPORT / the selected interface isn't connected,
	// JUCE simply keeps its working default output.
	// ============================================================

	logAvailableAudioOutputs();


	configurePreferredAudioOutput();


	// ============================================================
	// START PLAYLIST
	// ============================================================

	if (
		!playlistFiles.isEmpty()
	)
	{
		if (
			!loadTrack(
				0
			)
		)
		{
			juce::Logger::writeToLog(
				"WARNING: Initial playlist track could not be loaded."
			);
		}
	}
	else
	{
		juce::Logger::writeToLog(
			"WARNING: Playlist contains no playable audio files."
		);
	}


	// ============================================================
	// MIDI
	//
	// Only start MIDI callbacks once audio/DSP is established.
	// ============================================================

	initialiseMidi();


	// ============================================================
	// HOUSEKEEPING
	// ============================================================

	audioHealthCounter =
		0;


	midiHealthCounter =
		0;


	startTimerHz(
		20
	);
}
//--------------------------------------------------------------
//--------------------------------------------------------------
MainComponent::~MainComponent()
{
	// ============================================================
	// STOP MESSAGE-TIMER WORK
	// ============================================================

	stopTimer();


	// ============================================================
	// STOP MIDI CALLBACKS
	// ============================================================

	if (
		midiInput !=
		nullptr
	)
	{
		midiInput->stop();

		midiInput.reset();
	}


	midiDeviceIdentifier.clear();


	// ============================================================
	// CRITICAL:
	//
	// Stop the audio callback before destroying sources/effects.
	// ============================================================

	shutdownAudio();


	// ============================================================
	// NOW IT IS SAFE TO TEAR DOWN PLAYBACK OBJECTS
	// ============================================================

	transportSource.stop();


	transportSource.setSource(
		nullptr
	);


	readerSource.reset();
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void MainComponent::prepareToPlay(
	int samplesPerBlockExpected,
	double sampleRate
)
{
	const int safeBlockSize =
		std::max(
			64,
			samplesPerBlockExpected
		);


	const double safeSampleRate =
		sampleRate >
			1000.0
		?
		sampleRate
		:
		44100.0;


	// ============================================================
	// TRANSPORT
	// ============================================================

	transportSource.prepareToPlay(
		safeBlockSize,
		safeSampleRate
	);


	// ============================================================
	// EFFECTS
	// ============================================================

	delayEffect.prepare(
		safeSampleRate,
		safeBlockSize
	);


	reverbEffect.prepare(
		safeSampleRate,
		safeBlockSize
	);


	frequencyBands.prepare(
		safeSampleRate,
		safeBlockSize
	);


	dubSiren.prepare(
		safeSampleRate
	);


	// ============================================================
	// INITIAL EFFECT STATE
	// ============================================================

	delayEffect.setMix(
		0.0f
	);


	delayEffect.setFeedback(
		delayFeedback.load()
	);


	delayEffect.setDelayTime(
		static_cast<int>(
			delayTimeMs.load()
		)
	);


	reverbEffect.setRoomSize(
		reverbRoomSize.load()
	);


	reverbEffect.setWetLevel(
		reverbWet.load()
	);


	reverbEffect.setDamping(
		reverbDamping.load()
	);


	reverbEffect.setWidth(
		reverbWidth.load()
	);


	frequencyBands.setBassGain(
		bassValue.load()
	);


	frequencyBands.setMidsGain(
		midsValue.load()
	);


	frequencyBands.setTopsGain(
		topsValue.load()
	);


	// ============================================================
	// PREALLOCATE A LARGE SCRATCH CAPACITY
	//
	// No ordinary audio callback should ever need to allocate.
	// ============================================================

	scratchCapacitySamples =
		std::max(
			8192,
			safeBlockSize *
				4
		);


	dryBuffer.setSize(
		2,
		scratchCapacitySamples
	);


	dubBuffer.setSize(
		2,
		scratchCapacitySamples
	);


	sirenBuffer.setSize(
		2,
		scratchCapacitySamples
	);


	dryBuffer.clear();
	dubBuffer.clear();
	sirenBuffer.clear();
}
//--------------------------------------------------------------
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


	juce::AudioBuffer<float>& output =
		*bufferToFill.buffer;


	const int startSample =
		bufferToFill.startSample;


	const int numSamples =
		bufferToFill.numSamples;


	// ============================================================
	// VALIDATE CALLBACK RANGE
	// ============================================================

	if (
		startSample <
			0 ||
		numSamples <=
			0 ||
		output.getNumChannels() <=
			0 ||
		startSample >=
			output.getNumSamples() ||
		startSample +
			numSamples >
			output.getNumSamples()
	)
	{
		return;
	}


	// ============================================================
	// CLEAR + PLAY TRANSPORT
	// ============================================================

	bufferToFill
		.clearActiveBufferRegion();


	transportSource
		.getNextAudioBlock(
			bufferToFill
		);


	// ============================================================
	// NON-OWNING VIEW OF ONLY THIS CALLBACK REGION
	// ============================================================

	juce::AudioBuffer<float>
		activeOutput(
			output.getArrayOfWritePointers(),
			output.getNumChannels(),
			startSample,
			numSamples
		);


	const int channels =
		std::min(
			2,
			activeOutput
				.getNumChannels()
		);


	if (
		channels <=
		0
	)
	{
		return;
	}


	// ============================================================
	// APPLY REQUESTED PARAMETERS ON AUDIO THREAD
	//
	// MIDI callback merely updates atomics.
	// ============================================================

	reverbEffect.setRoomSize(
		reverbRoomSize.load()
	);


	reverbEffect.setWetLevel(
		reverbWet.load()
	);


	reverbEffect.setDamping(
		reverbDamping.load()
	);


	reverbEffect.setWidth(
		reverbWidth.load()
	);


	frequencyBands.setBassGain(
		bassValue.load()
	);


	frequencyBands.setMidsGain(
		midsValue.load()
	);


	frequencyBands.setTopsGain(
		topsValue.load()
	);


	// ============================================================
	// EQ
	// ============================================================

	juce::MidiBuffer emptyMidi;


	frequencyBands.process(
		activeOutput,
		emptyMidi
	);


	// ============================================================
	// LAST-RESORT OVERSIZED CALLBACK PROTECTION
	//
	// Rather than touching memory beyond our scratch buffers,
	// bypass the dub processing for this single block.
	// ============================================================

	if (
		numSamples >
			scratchCapacitySamples ||
		scratchCapacitySamples <=
			0
	)
	{
		const float gain =
			juce::jlimit(
				0.0f,
				1.0f,
				outputGain.load()
			);


		activeOutput.applyGain(
			gain
		);


		for (
			int channel = 0;
			channel <
				channels;
			++channel
		)
		{
			float* samples =
				activeOutput
					.getWritePointer(
						channel
					);


			for (
				int sample = 0;
				sample <
					numSamples;
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


		if (
			transportSource
				.hasStreamFinished()
		)
		{
			nextTrackRequested.store(
				true
			);
		}


		return;
	}


	// ============================================================
	// CHANGE LOGICAL SCRATCH SIZE WITHOUT REALLOCATING
	// ============================================================

	dryBuffer.setSize(
		2,
		numSamples,
		false,
		false,
		true
	);


	dubBuffer.setSize(
		2,
		numSamples,
		false,
		false,
		true
	);


	sirenBuffer.setSize(
		2,
		numSamples,
		false,
		false,
		true
	);


	dryBuffer.clear();
	dubBuffer.clear();
	sirenBuffer.clear();


	// ============================================================
	// COPY CLEAN + DUB PATHS
	// ============================================================

	for (
		int channel = 0;
		channel <
			channels;
		++channel
	)
	{
		dryBuffer.copyFrom(
			channel,
			0,
			activeOutput,
			channel,
			0,
			numSamples
		);


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
	// SIREN
	// ============================================================

	// ============================================================
	// DUB SIREN SAMPLE
	// ============================================================

	if (
		sirenSampleLoaded
	)
	{
		int position =
			sirenSamplePosition.load();


		// -1 means the siren is currently stopped.
		if (
			position >=
			0
		)
		{
			const int samplesRemaining =
				sirenSample.getNumSamples() -
				position;


			const int samplesToPlay =
				std::min(
					numSamples,
					samplesRemaining
				);


			if (
				samplesToPlay >
				0
			)
			{
				const int sourceChannels =
					sirenSample
						.getNumChannels();


				// ====================================================
				// COPY SAMPLE INTO SIREN BUFFER
				// ====================================================

				for (
					int channel = 0;
					channel < 2;
					++channel
				)
				{
					const int sourceChannel =
						std::min(
							channel,
							sourceChannels - 1
						);


					sirenBuffer.copyFrom(
						channel,
						0,
						sirenSample,
						sourceChannel,
						position,
						samplesToPlay
					);
				}


				position +=
					samplesToPlay;
			}


			// ========================================================
			// END OF SAMPLE
			// ========================================================

			if (
				position >=
				sirenSample.getNumSamples()
			)
			{
				position =
					-1;
			}


			sirenSamplePosition.store(
				position
			);
		}
	}

	for (
		int channel = 0;
		channel <
			channels;
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
	// DELAY
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
	// ============================================================

	reverbEffect.process(
		dubBuffer
	);


	// ============================================================
	// CROSSFADER
	// ============================================================

	const float x =
		juce::jlimit(
			0.0f,
			1.0f,
			dubCrossfader.load()
		);


	const float dryGain =
		1.0f -
		x;


	const float dubGain =
		x;


	// ============================================================
	// REBUILD OUTPUT
	// ============================================================

	activeOutput.clear();


	for (
		int channel = 0;
		channel <
			channels;
		++channel
	)
	{
		activeOutput.addFrom(
			channel,
			0,
			dryBuffer,
			channel,
			0,
			numSamples,
			dryGain
		);


		activeOutput.addFrom(
			channel,
			0,
			dubBuffer,
			channel,
			0,
			numSamples,
			dubGain
		);


		activeOutput.addFrom(
			channel,
			0,
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

	activeOutput.applyGain(
		juce::jlimit(
			0.0f,
			1.0f,
			outputGain.load()
		)
	);


	// ============================================================
	// HARD SAFETY LIMIT
	// ============================================================

	for (
		int channel = 0;
		channel <
			channels;
		++channel
	)
	{
		float* samples =
			activeOutput
				.getWritePointer(
					channel
				);


		for (
			int sample = 0;
			sample <
				numSamples;
			++sample
		)
		{
			float value =
				samples[sample];


			if (
				!std::isfinite(
					value
				)
			)
			{
				value =
					0.0f;
			}


			samples[sample] =
				juce::jlimit(
					-1.0f,
					1.0f,
					value
				);
		}
	}


	// ============================================================
	// PLAYLIST ADVANCE
	//
	// Loading the next file remains OFF the audio thread.
	// ============================================================

	if (
		transportSource
			.hasStreamFinished()
	)
	{
		nextTrackRequested.store(
			true
		);
	}
}
//--------------------------------------------------------------
//--------------------------------------------------------------
void MainComponent::timerCallback()
{
	// ============================================================
	// PLAYLIST
	// ============================================================

	if (
		nextTrackRequested.exchange(
			false
		)
	)
	{
		playNextTrack();
	}


	// ============================================================
	// MIDI HEALTH CHECK
	//
	// Timer runs at 20 Hz.
	// Check approximately once per second.
	// ============================================================

	++midiHealthCounter;


	if (
		midiHealthCounter <
		20
	)
	{
		return;
	}


	midiHealthCounter =
		0;


	const auto devices =
		juce::MidiInput::
			getAvailableDevices();


	// ============================================================
	// IF WE THINK MIDI IS OPEN, VERIFY THAT DEVICE STILL EXISTS
	// ============================================================

	if (
		midiInput !=
		nullptr &&
		midiDeviceIdentifier
			.isNotEmpty()
	)
	{
		bool stillConnected =
			false;


		for (
			const auto& device :
			devices
		)
		{
			if (
				device.identifier ==
				midiDeviceIdentifier
			)
			{
				stillConnected =
					true;

				break;
			}
		}


		if (
			!stillConnected
		)
		{
			midiInput->stop();

			midiInput.reset();

			midiDeviceIdentifier
				.clear();
		}
	}


	// ============================================================
	// RECONNECT IF NECESSARY
	// ============================================================

	if (
		midiInput ==
		nullptr &&
		!devices.isEmpty()
	)
	{
		initialiseMidi();
	}
	
	// ============================================================
	// AUDIO DEVICE HEALTH CHECK
	//
	// Timer runs at 20 Hz.
	//
	// Check roughly every two seconds rather than constantly.
	// ============================================================

	++audioHealthCounter;


	if (
		audioHealthCounter >=
		40
	)
	{
		audioHealthCounter =
			0;


		if (
			!isPreferredAudioOutputActive()
		)
		{
			configurePreferredAudioOutput();
		}
	}
}

//--------------------------------------------------------------
void MainComponent::releaseResources()
{
	transportSource.releaseResources();
}

//--------------------------------------------------------------
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
		currentTrackIndex >=
			0 &&
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
			static_cast<double>(
				dubSend.load()
			),
			2
		);


	text +=
		"\nDub Mix: " +
		juce::String(
			static_cast<double>(
				dubCrossfader.load()
			),
			2
		);


	g.drawFittedText(
		text,
		getLocalBounds()
			.reduced(
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
	{
		juce::Logger::writeToLog(
			"ERROR: Assets folder could not be found."
		);

		return;
	}


	const auto playlistFile =
		assetsFolder.getChildFile(
			"playlist.json"
		);


	if (
		!playlistFile.existsAsFile()
	)
	{
		juce::Logger::writeToLog(
			"ERROR: playlist.json could not be found: " +
			playlistFile.getFullPathName()
		);

		return;
	}


	// ============================================================
	// PARSE JSON
	// ============================================================

	juce::var json;


	const juce::Result parseResult =
		juce::JSON::parse(
			playlistFile.loadFileAsString(),
			json
		);


	if (
		parseResult.failed()
	)
	{
		juce::Logger::writeToLog(
			"ERROR: playlist.json could not be parsed: " +
			parseResult.getErrorMessage()
		);

		return;
	}


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
	{
		juce::Logger::writeToLog(
			"ERROR: playlist.json does not contain a valid track array."
		);

		return;
	}


	// ============================================================
	// LOAD EVERY PLAYLIST ENTRY
	// ============================================================

	int jsonIndex =
		0;


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
		{
			juce::Logger::writeToLog(
				"WARNING: Invalid playlist entry at JSON index " +
				juce::String(
					jsonIndex
				)
			);


			++jsonIndex;

			continue;
		}


		// ========================================================
		// FILE
		// ========================================================

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
		{
			juce::Logger::writeToLog(
				"WARNING: Playlist entry " +
				juce::String(
					jsonIndex
				) +
				" has no file path."
			);


			++jsonIndex;

			continue;
		}


		// ========================================================
		// NORMALISE RELATIVE PATH
		//
		// We NEVER want:
		//
		// /playlist/file.mp3
		//
		// We want:
		//
		// playlist/file.mp3
		// ========================================================

		while (
			filename.startsWithChar(
				'/'
			) ||
			filename.startsWithChar(
				'\\'
			)
		)
		{
			filename =
				filename.substring(
					1
				);
		}


		// ========================================================
		// RESOLVE AGAINST ASSETS
		// ========================================================

		juce::File file =
			assetsFolder.getChildFile(
				filename
			);


		// ========================================================
		// LEGACY FALLBACK
		//
		// Keep this temporarily in case older playlist entries
		// still expect Assets/audio/.
		// ========================================================

		if (
			!file.existsAsFile()
		)
		{
			const juce::File legacyFile =
				assetsFolder
					.getChildFile(
						"audio"
					)
					.getChildFile(
						filename
					);


			if (
				legacyFile.existsAsFile()
			)
			{
				file =
					legacyFile;
			}
		}


		// ========================================================
		// MISSING FILE
		// ========================================================

		if (
			!file.existsAsFile()
		)
		{
			juce::Logger::writeToLog(
				"PLAYLIST FILE MISSING [" +
				juce::String(
					jsonIndex
				) +
				"]: " +
				filename +
				"\nExpected at: " +
				assetsFolder
					.getChildFile(
						filename
					)
					.getFullPathName()
			);


			++jsonIndex;

			continue;
		}


		// ========================================================
		// TITLE
		// ========================================================

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
				file
					.getFileNameWithoutExtension();
		}


		// ========================================================
		// STORE VALID TRACK
		// ========================================================

		playlistFiles.add(
			file
		);


		trackNames.add(
			title
		);


		juce::Logger::writeToLog(
			"PLAYLIST LOADED [" +
			juce::String(
				jsonIndex
			) +
			"]: " +
			title +
			" -> " +
			file.getFileName()
		);


		++jsonIndex;
	}


	// ============================================================
	// SUMMARY
	// ============================================================

	juce::Logger::writeToLog(
		"PLAYLIST READY: " +
		juce::String(
			playlistFiles.size()
		) +
		" playable tracks loaded."
	);
}
//--------------------------------------------------------------
//--------------------------------------------------------------
bool MainComponent::loadTrack(
	int index
)
{
	// ============================================================
	// VALIDATE INDEX
	// ============================================================

	if (
		index <
			0 ||
		index >=
			playlistFiles.size()
	)
	{
		juce::Logger::writeToLog(
			"LOAD TRACK FAILED: invalid index " +
			juce::String(
				index
			)
		);

		return false;
	}


	// ============================================================
	// IMPORTANT:
	//
	// CANCEL ANY OLD / STALE ADVANCE REQUEST.
	//
	// A finished previous track must not be allowed to immediately
	// skip the new track that we are about to load.
	// ============================================================

	nextTrackRequested.store(
		false
	);


	// ============================================================
	// FILE
	// ============================================================

	const juce::File file =
		playlistFiles[
			index
		];


	if (
		!file.existsAsFile()
	)
	{
		juce::Logger::writeToLog(
			"Audio file missing: " +
			file.getFullPathName()
		);

		return false;
	}


	// ============================================================
	// CREATE READER
	// ============================================================

	std::unique_ptr<
		juce::AudioFormatReader
	> reader(
		formatManager
			.createReaderFor(
				file
			)
	);


	if (
		reader ==
		nullptr
	)
	{
		juce::Logger::writeToLog(
			"Could not decode audio file: " +
			file.getFullPathName()
		);

		return false;
	}


	// ============================================================
	// VALIDATE AUDIO METADATA
	// ============================================================

	if (
		reader->sampleRate <=
			1000.0 ||
		reader->lengthInSamples <=
			0
	)
	{
		juce::Logger::writeToLog(
			"Invalid audio file metadata: " +
			file.getFullPathName()
		);

		return false;
	}


	const double sourceRate =
		reader->sampleRate;


	const double durationSeconds =
		static_cast<double>(
			reader->lengthInSamples
		)
		/
		sourceRate;


	juce::Logger::writeToLog(
		"PREPARING TRACK [" +
		juce::String(
			index
		) +
		"]: " +
		file.getFileName() +
		" | duration = " +
		juce::String(
			durationSeconds,
			2
		) +
		" seconds"
	);


	// ============================================================
	// CREATE NEW SOURCE BEFORE DESTROYING OLD ONE
	// ============================================================

	auto newSource =
		std::make_unique<
			juce::AudioFormatReaderSource
		>(
			reader.release(),
			true
		);


	if (
		newSource ==
		nullptr
	)
	{
		juce::Logger::writeToLog(
			"LOAD TRACK FAILED: could not create reader source."
		);

		return false;
	}


	// ============================================================
	// STOP OLD PLAYBACK
	// ============================================================

	transportSource.stop();


	transportSource.setSource(
		nullptr
	);


	readerSource.reset();


	// ============================================================
	// INSTALL NEW SOURCE
	// ============================================================

	readerSource =
		std::move(
			newSource
		);


	transportSource.setSource(
		readerSource.get(),
		0,
		nullptr,
		sourceRate
	);


	// ============================================================
	// SET STATE BEFORE STARTING
	// ============================================================

	currentTrackIndex =
		index;


	transportSource.setPosition(
		0.0
	);


	// ============================================================
	// CLEAR ADVANCE FLAG AGAIN
	//
	// This second clear is deliberate.
	//
	// The audio callback may have observed the old transport during
	// the transition above and requested another advance.
	// ============================================================

	nextTrackRequested.store(
		false
	);


	// ============================================================
	// START
	// ============================================================

	transportSource.start();


	// ============================================================
	// DIAGNOSTIC LOG
	// ============================================================

	juce::Logger::writeToLog(
		"NOW PLAYING [" +
		juce::String(
			currentTrackIndex
		) +
		"]: " +
		file.getFileName()
	);


	repaint();


	return true;
}
//--------------------------------------------------------------
//--------------------------------------------------------------
void MainComponent::playNextTrack()
{
	if (
		playlistFiles.isEmpty()
	)
	{
		return;
	}


	const int trackCount =
		playlistFiles.size();


	int firstCandidate =
		currentTrackIndex >=
			0
		?
		(
			currentTrackIndex +
			1
		)
		%
		trackCount
		:
		0;


	// ============================================================
	// TRY EVERY TRACK ONCE
	//
	// A corrupt/missing file gets skipped rather than leaving the
	// system retrying it every timer tick forever.
	// ============================================================

	for (
		int attempt = 0;
		attempt <
			trackCount;
		++attempt
	)
	{
		const int candidate =
			(
				firstCandidate +
				attempt
			)
			%
			trackCount;


		if (
			loadTrack(
				candidate
			)
		)
		{
			return;
		}
	}


	// ============================================================
	// NOTHING PLAYABLE
	//
	// Safely detach playback instead of hammering failed files.
	// ============================================================

	transportSource.stop();


	transportSource.setSource(
		nullptr
	);


	readerSource.reset();


	currentTrackIndex =
		-1;


	nextTrackRequested.store(
		false
	);


	juce::Logger::writeToLog(
		"ERROR: No playable files remain in playlist."
	);
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void MainComponent::initialiseMidi()
{
	// Already active.
	if (
		midiInput !=
		nullptr
	)
	{
		return;
	}


	const auto devices =
		juce::MidiInput::
			getAvailableDevices();


	if (
		devices.isEmpty()
	)
	{
		midiDeviceIdentifier
			.clear();

		return;
	}


	// ============================================================
	// CURRENTLY USE FIRST AVAILABLE DECK
	// ============================================================

	const auto& device =
		devices[0];


	auto newInput =
		juce::MidiInput::openDevice(
			device.identifier,
			this
		);


	if (
		newInput ==
		nullptr
	)
	{
		midiDeviceIdentifier
			.clear();

		return;
	}


	midiDeviceIdentifier =
		device.identifier;


	midiInput =
		std::move(
			newInput
		);


	midiInput->start();
}

//--------------------------------------------------------------
bool MainComponent::loadSirenSample()
{
	// ============================================================
	// FIND SIREN FILE
	//
	// Relative to the same Assets folder already used by the
	// playlist system.
	// ============================================================

	const juce::File sirenFile =
		getAssetsFolder()
			.getChildFile(
				"dub_siren.wav"
			);


	if (
		!sirenFile.existsAsFile()
	)
	{
		juce::Logger::writeToLog(
			"ERROR: Dub siren sample not found: " +
			sirenFile.getFullPathName()
		);


		sirenSampleLoaded =
			false;


		return false;
	}


	// ============================================================
	// CREATE AUDIO READER
	// ============================================================

	std::unique_ptr<
		juce::AudioFormatReader
	> reader(
		formatManager.createReaderFor(
			sirenFile
		)
	);


	if (
		reader ==
		nullptr
	)
	{
		juce::Logger::writeToLog(
			"ERROR: Could not decode dub siren sample: " +
			sirenFile.getFullPathName()
		);


		sirenSampleLoaded =
			false;


		return false;
	}


	// ============================================================
	// VALIDATE
	// ============================================================

	if (
		reader->lengthInSamples <=
			0 ||
		reader->numChannels <=
			0
	)
	{
		juce::Logger::writeToLog(
			"ERROR: Invalid dub siren sample."
		);


		sirenSampleLoaded =
			false;


		return false;
	}


	// ============================================================
	// LOAD WHOLE SAMPLE INTO MEMORY
	//
	// Fine for one short siren sound and avoids disk access during
	// playback.
	// ============================================================

	const int numberOfChannels =
		static_cast<int>(
			std::min<unsigned int>(
				2,
				reader->numChannels
			)
		);


	const int numberOfSamples =
		static_cast<int>(
			reader->lengthInSamples
		);


	sirenSample.setSize(
		numberOfChannels,
		numberOfSamples
	);


	sirenSample.clear();


	reader->read(
		&sirenSample,
		0,
		numberOfSamples,
		0,
		true,
		true
	);


	// ============================================================
	// READY
	// ============================================================

	sirenSamplePosition.store(
		-1
	);


	sirenSampleLoaded =
		true;


	juce::Logger::writeToLog(
		"Loaded dub siren sample: " +
		sirenFile.getFullPathName()
	);


	return true;
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
			message
				.getControllerNumber();


		const int raw =
			message
				.getControllerValue();


		const float value =
			juce::jlimit(
				0.0f,
				1.0f,
				static_cast<float>(
					raw
				)
				/
				127.0f
			);


		// ========================================================
		// HEADPHONE OUTPUT
		// ========================================================

		if (
			cc ==
			14
		)
		{
			outputGain.store(
				value
			);
		}


		// ========================================================
		// REVERB ROOM SIZE
		// CC 20
		// ========================================================

		else if (
			cc ==
			20
		)
		{
			reverbRoomSize.store(
				value
			);


			sendFloatOSC(
				"/reverb/roomSize",
				value
			);
		}


		// ========================================================
		// REVERB WET
		// CC 18
		// ========================================================

		else if (
			cc ==
			18
		)
		{
			reverbWet.store(
				value
			);


			sendFloatOSC(
				"/reverb/wetLevel",
				value
			);
		}


		// ========================================================
		// REVERB DAMPING
		// CC 16
		// ========================================================

		else if (
			cc ==
			16
		)
		{
			reverbDamping.store(
				value
			);


			sendFloatOSC(
				"/reverb/damping",
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
			reverbWet.store(
				value
			);


			sendFloatOSC(
				"/reverb/wetLevel",
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
			bassValue.store(
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
			midsValue.store(
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
			topsValue.store(
				value
			);


			sendFloatOSC(
				"/eq/tops",
				value
			);
		}


		// ========================================================
		// MAIN DELAY SEND
		// ========================================================

		else if (
			cc ==
			8
		)
		{
			dubSend.store(
				value
			);


			const float effectiveDelaySend =
				std::max(
					dubSend.load(),
					splitScreenDelaySend
				);


			// DelayEffect's parameter storage is already atomic.
			delayEffect.setMix(
				effectiveDelaySend
			);


			sendFloatOSC(
				"/delay/mixValue",
				value
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
			delayFeedback.store(
				value
			);


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
			const float newDelayTime =
				juce::jmap(
					value,
					90.0f,
					900.0f
				);


			delayTimeMs.store(
				newDelayTime
			);


			delayEffect.setDelayTime(
				static_cast<int>(
					newDelayTime
				)
			);


			sendFloatOSC(
				"/delay/delayTime",
				newDelayTime
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
			dubCrossfader.store(
				value
			);


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
		// SPLIT SCREEN + SPLIT DELAY
		//
		// Physical slider direction:
		//
		// SPLIT SCREEN END
		//     = more split screen
		//     = more associated delay
		//
		// NO SPLIT END
		//     = no split screen
		//     = no associated delay
		// ========================================================

		else if (
			cc ==
			10
		)
		{
			// ====================================================
			// KEEP SENDING THE RAW CONTROLLER VALUE
			//
			// ChronologyManager already knows that this physical
			// slider is reversed and performs:
			//
			//     1.0f - rawValue
			//
			// on the visual side.
			// ====================================================

			sendFloatOSC(
				"/splitScreen/amount",
				value
			);


			// ====================================================
			// MATCH JUCE'S DELAY TO THE VISUAL DIRECTION
			//
			// Previously this used "value" directly, which meant
			// the delay became strongest at the NO-SPLIT end.
			//
			// Invert it here so:
			//
			//     split screen increases
			//          ↓
			//     associated delay increases
			// ====================================================

			const float splitAmount =
				1.0f -
				value;


			splitScreenDelaySend =
				std::pow(
					splitAmount,
					0.82f
				)
				*
				0.38f;


			// ====================================================
			// COMBINE THE NORMAL DELAY SEND WITH THE AUTOMATIC
			// SPLIT-SCREEN DELAY.
			// ====================================================

			const float effectiveDelaySend =
				std::max(
					dubSend.load(),
					splitScreenDelaySend
				);


			delayEffect.setMix(
				effectiveDelaySend
			);
		}


		// ========================================================
		// MAIN JOG
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


		// Any physical deck control counts as interaction.
		sendInteraction();


		return;
	}


	// ============================================================
	// BUTTONS
	//
	// JUCE isNoteOn() does not treat a velocity-zero release as
	// a normal press here.
	// ============================================================

	if (
		message.isNoteOn()
	)
	{
		const int note =
			message
				.getNoteNumber();


		// ========================================================
		// DUB SIREN
		// ========================================================

		if (
			note ==
			sirenTriggerNote
		)
		{
			if (
				sirenSampleLoaded
			)
			{
				sirenSamplePosition.store(
					0
				);
			}


			sendIntOSC(
				"/dub/siren",
				1
			);
		}


		// ========================================================
		// VIDEO ADVANCE
		// ========================================================

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


		// ========================================================
		// SPLIT-SCREEN ADVANCE
		// ========================================================

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


		// ========================================================
		// CHANGE TOPIC
		// ========================================================

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

void MainComponent::logAvailableAudioOutputs() const
{
	auto* deviceType =
		deviceManager
			.getCurrentDeviceTypeObject();


	if (
		deviceType ==
		nullptr
	)
	{
		juce::Logger::writeToLog(
			"AUDIO: No current audio device type."
		);


		return;
	}


	const juce::StringArray devices =
		deviceType
			->getDeviceNames(
				false
			);


	juce::Logger::writeToLog(
		"========================================"
	);

	juce::Logger::writeToLog(
		"AVAILABLE AUDIO OUTPUT DEVICES:"
	);


	if (
		devices.isEmpty()
	)
	{
		juce::Logger::writeToLog(
			"    NONE"
		);
	}
	else
	{
		for (
			const auto& device :
			devices
		)
		{
			juce::Logger::writeToLog(
				"    " +
				device
			);
		}
	}


	juce::Logger::writeToLog(
		"========================================"
	);
}

bool MainComponent::isPreferredAudioOutputActive() const
{
	// ============================================================
	// EMPTY NAME MEANS:
	//
	// deliberately use whatever JUCE/system chose.
	// ============================================================

	if (
		preferredAudioDeviceName
			.isEmpty()
	)
	{
		return true;
	}


	auto* device =
		deviceManager
			.getCurrentAudioDevice();


	if (
		device ==
		nullptr
	)
	{
		return false;
	}


	const juce::String currentName =
		device->getName();


	return (
		currentName
			.equalsIgnoreCase(
				preferredAudioDeviceName
			)
		||
		currentName
			.containsIgnoreCase(
				preferredAudioDeviceName
			)
		||
		preferredAudioDeviceName
			.containsIgnoreCase(
				currentName
			)
	);
}

bool MainComponent::configurePreferredAudioOutput()
{
	// ============================================================
	// IF NO DEVICE NAME HAS BEEN REQUESTED,
	// KEEP THE CURRENT JUCE DEVICE
	// ============================================================

	if (
		preferredAudioDeviceName
			.trim()
			.isEmpty()
	)
	{
		juce::Logger::writeToLog(
			"AUDIO: No preferred output device specified. "
			"Using system/default JUCE output."
		);


		return (
			deviceManager
				.getCurrentAudioDevice()
			!=
			nullptr
		);
	}


	// ============================================================
	// FIND CURRENT AUDIO DEVICE TYPE
	// ============================================================

	auto* deviceType =
		deviceManager
			.getCurrentDeviceTypeObject();


	if (
		deviceType ==
		nullptr
	)
	{
		juce::Logger::writeToLog(
			"WARNING: No audio device type is currently available."
		);


		return false;
	}


	// ============================================================
	// GET OUTPUT DEVICES
	//
	// false = output devices
	// ============================================================

	const juce::StringArray availableDevices =
		deviceType
			->getDeviceNames(
				false
			);


	if (
		availableDevices
			.isEmpty()
	)
	{
		juce::Logger::writeToLog(
			"WARNING: No audio output devices were found."
		);


		return false;
	}


	// ============================================================
	// FIND THE REQUESTED DEVICE
	//
	// First try an exact match.
	// Then accept a partial match.
	//
	// This makes deployment slightly less brittle if an OS calls
	// something:
	//
	//     "GIGAPORT eX"
	//
	// versus:
	//
	//     "ESI GIGAPORT eX"
	// ============================================================

	juce::String chosenDevice;


	for (
		const auto& device :
		availableDevices
	)
	{
		if (
			device
				.equalsIgnoreCase(
					preferredAudioDeviceName
				)
		)
		{
			chosenDevice =
				device;


			break;
		}
	}


	if (
		chosenDevice
			.isEmpty()
	)
	{
		for (
			const auto& device :
			availableDevices
		)
		{
			if (
				device
					.containsIgnoreCase(
						preferredAudioDeviceName
					)
				||
				preferredAudioDeviceName
					.containsIgnoreCase(
						device
					)
			)
			{
				chosenDevice =
					device;


				break;
			}
		}
	}


	// ============================================================
	// PREFERRED INTERFACE NOT PRESENT
	//
	// IMPORTANT:
	//
	// Do NOT kill the installation.
	//
	// Leave JUCE's current/default output alive so that the work
	// can still run on your development computer.
	// ============================================================

	if (
		chosenDevice
			.isEmpty()
	)
	{
		juce::Logger::writeToLog(
			"WARNING: Preferred audio device not found: " +
			preferredAudioDeviceName
		);


		logAvailableAudioOutputs();


		if (
			auto* current =
				deviceManager
					.getCurrentAudioDevice()
		)
		{
			juce::Logger::writeToLog(
				"AUDIO FALLBACK: keeping current output: " +
				current->getName()
			);
		}


		return false;
	}


	// ============================================================
	// ALREADY USING IT
	//
	// Don't unnecessarily restart the audio engine.
	// ============================================================

	if (
		auto* current =
			deviceManager
				.getCurrentAudioDevice()
	)
	{
		if (
			current
				->getName()
				.equalsIgnoreCase(
					chosenDevice
				)
		)
		{
			return true;
		}
	}


	// ============================================================
	// GET EXISTING SETUP
	// ============================================================

	juce::AudioDeviceManager::
		AudioDeviceSetup setup;


	deviceManager
		.getAudioDeviceSetup(
			setup
		);


	// ============================================================
	// CHOOSE EXTERNAL OUTPUT DEVICE
	// ============================================================

	setup.outputDeviceName =
		chosenDevice;


	// We aren't recording anything.
	setup.inputDeviceName.clear();


	setup.useDefaultInputChannels =
		false;


	setup.inputChannels.clear();


	// ============================================================
	// EXPLICIT STEREO OUTPUT PAIR
	//
	// 0 = output 1
	// 1 = output 2
	//
	// If the exhibition interface uses another physical pair,
	// change the two integers in MainComponent.h.
	// ============================================================

	setup.useDefaultOutputChannels =
		false;


	setup.outputChannels.clear();


	setup.outputChannels.setBit(
		preferredOutputLeftChannel
	);


	setup.outputChannels.setBit(
		preferredOutputRightChannel
	);


	// ============================================================
	// APPLY SETUP
	//
	// JUCE will automatically call prepareToPlay() again as needed.
	// ============================================================

	const juce::String error =
		deviceManager
			.setAudioDeviceSetup(
				setup,
				true
			);


	if (
		error.isNotEmpty()
	)
	{
		juce::Logger::writeToLog(
			"ERROR selecting audio device \"" +
			chosenDevice +
			"\": " +
			error
		);


		return false;
	}


	// ============================================================
	// VERIFY
	// ============================================================

	auto* activeDevice =
		deviceManager
			.getCurrentAudioDevice();


	if (
		activeDevice ==
		nullptr
	)
	{
		juce::Logger::writeToLog(
			"ERROR: Audio device disappeared after configuration."
		);


		return false;
	}


	juce::Logger::writeToLog(
		"========================================"
	);


	juce::Logger::writeToLog(
		"AUDIO OUTPUT ACTIVE: " +
		activeDevice->getName()
	);


	juce::Logger::writeToLog(
		"LEFT OUTPUT INDEX: " +
		juce::String(
			preferredOutputLeftChannel
		)
	);


	juce::Logger::writeToLog(
		"RIGHT OUTPUT INDEX: " +
		juce::String(
			preferredOutputRightChannel
		)
	);


	juce::Logger::writeToLog(
		"========================================"
	);


	return true;
}
