#include "FrequencyBands.h"


//--------------------------------------------------------------
FrequencyBands::FrequencyBands()
{
}


//--------------------------------------------------------------
void FrequencyBands::prepare(
	double sampleRate,
	int samplesPerBlock
)
{
	currentSampleRate =
		sampleRate >
			1000.0
		?
		sampleRate
		:
		44100.0;


	const int safeBlock =
		std::max(
			64,
			samplesPerBlock
		);


	// ============================================================
	// FILTERS
	// ============================================================

	bassLowPass.setCoefficients(
		juce::IIRCoefficients::
			makeLowPass(
				currentSampleRate,
				250.0
			)
	);


	midsBandPass.setCoefficients(
		juce::IIRCoefficients::
			makeBandPass(
				currentSampleRate,
				1200.0,
				0.5
			)
	);


	topsHighPass.setCoefficients(
		juce::IIRCoefficients::
			makeHighPass(
				currentSampleRate,
				1000.0
			)
	);


	bassLowPass.reset();
	midsBandPass.reset();
	topsHighPass.reset();


	// ============================================================
	// PREALLOCATED SCRATCH
	// ============================================================

	scratchCapacitySamples =
		std::max(
			8192,
			safeBlock *
				4
		);


	bassBuffer.setSize(
		1,
		scratchCapacitySamples
	);


	midsBuffer.setSize(
		1,
		scratchCapacitySamples
	);


	topsBuffer.setSize(
		1,
		scratchCapacitySamples
	);


	bassBuffer.clear();
	midsBuffer.clear();
	topsBuffer.clear();
}


//--------------------------------------------------------------
void FrequencyBands::process(
	juce::AudioBuffer<float>& buffer,
	const juce::MidiBuffer& midi
)
{
	juce::ignoreUnused(
		midi
	);


	const int channels =
		buffer.getNumChannels();


	const int numSamples =
		buffer.getNumSamples();


	if (
		channels <=
			0 ||
		numSamples <=
			0
	)
	{
		return;
	}


	// ============================================================
	// NEVER ALLOCATE AN OVERSIZED BUFFER ON AUDIO THREAD
	//
	// If something extremely unusual produces a block above the
	// preallocated capacity, leave that block un-EQed.
	// ============================================================

	if (
		numSamples >
			scratchCapacitySamples ||
		scratchCapacitySamples <=
			0
	)
	{
		return;
	}


	// ============================================================
	// CHANGE LOGICAL LENGTH WITHOUT REALLOCATION
	// ============================================================

	bassBuffer.setSize(
		1,
		numSamples,
		false,
		false,
		true
	);


	midsBuffer.setSize(
		1,
		numSamples,
		false,
		false,
		true
	);


	topsBuffer.setSize(
		1,
		numSamples,
		false,
		false,
		true
	);


	// ============================================================
	// COPY SOURCE CHANNEL ZERO
	// ============================================================

	bassBuffer.copyFrom(
		0,
		0,
		buffer,
		0,
		0,
		numSamples
	);


	midsBuffer.copyFrom(
		0,
		0,
		buffer,
		0,
		0,
		numSamples
	);


	topsBuffer.copyFrom(
		0,
		0,
		buffer,
		0,
		0,
		numSamples
	);


	// ============================================================
	// FILTER
	// ============================================================

	bassLowPass.processSamples(
		bassBuffer.getWritePointer(
			0
		),
		numSamples
	);


	midsBandPass.processSamples(
		midsBuffer.getWritePointer(
			0
		),
		numSamples
	);


	topsHighPass.processSamples(
		topsBuffer.getWritePointer(
			0
		),
		numSamples
	);


	// ============================================================
	// GAINS
	// ============================================================

	bassBuffer.applyGain(
		juce::jlimit(
			0.0f,
			1.0f,
			bassGain
		)
	);


	midsBuffer.applyGain(
		juce::jlimit(
			0.0f,
			1.0f,
			midsGain
		)
	);


	topsBuffer.applyGain(
		juce::jlimit(
			0.0f,
			1.0f,
			topsGain
		)
	);


	// ============================================================
	// REBUILD EXACTLY AS CURRENT IMPLEMENTATION DOES
	// ============================================================

	buffer.clear();


	buffer.addFrom(
		0,
		0,
		bassBuffer,
		0,
		0,
		numSamples
	);


	buffer.addFrom(
		0,
		0,
		midsBuffer,
		0,
		0,
		numSamples
	);


	buffer.addFrom(
		0,
		0,
		topsBuffer,
		0,
		0,
		numSamples
	);
}


//--------------------------------------------------------------
void FrequencyBands::setBassGain(
	float gain
)
{
	bassGain =
		juce::jlimit(
			0.0f,
			1.0f,
			gain
		);
}


//--------------------------------------------------------------
void FrequencyBands::setMidsGain(
	float gain
)
{
	midsGain =
		juce::jlimit(
			0.0f,
			1.0f,
			gain
		);
}


//--------------------------------------------------------------
void FrequencyBands::setTopsGain(
	float gain
)
{
	topsGain =
		juce::jlimit(
			0.0f,
			1.0f,
			gain
		);
}
