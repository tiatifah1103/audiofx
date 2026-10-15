# audiofx

## Description

`audiofx` is the JUCE audio engine for **Dread Frequencies**, an interactive audiovisual documentary exploring British sound-system culture,.

The application handles music playback, real-time audio processing, MIDI controller input and exhibition audio routing. Controller interactions are also transmitted over OSC to the corresponding openFrameworks visual application so that changes to sound and image remain connected.

The audio application is intended to run alongside the openFrameworks visual application as part of the complete installation.

## Main Features

- JSON-based audio playlist.
- Relative asset loading from the project's `Assets` directory.
- Real-time delay and reverb processing.
- Bass, mids and tops control.
- Dry/dub crossfading.
- WAV-based dub siren playback with pitch control.
- MIDI control through the installation's DJ controller.
- OSC communication with the openFrameworks visual application.
- Automatic selection of the preferred exhibition audio interface.
- Explicit stereo output-pair routing.
- Automatic fallback to the system/default audio output when the exhibition interface is unavailable.
- Periodic MIDI and audio-device health checks during operation.

---

# Audio Architecture

The JUCE application produces a **stereo audio stream**.

Stereo consists of two audio channels:

- **Left**
- **Right**

A pair of headphones therefore requires two output channels. One channel carries the left side of the stereo signal and the other carries the right.

Internally, the application processes these as a stereo pair rather than treating the headphones as a single mono output.

The general signal path is:

```text
Playlist audio
      |
      v
     EQ
      |
      +--------------------+
      |                    |
      v                    v
  Dry signal           Dub signal
                           |
                    Dub siren added
                           |
                         Delay
                           |
                        Reverb
      |                    |
      +------ Crossfader --+
               |
               v
        Master output gain
               |
               v
      Stereo output pair
          Left + Right
```

The dub siren is loaded from a WAV file in the `Assets` directory and is mixed into the dub processing path.

---

# Exhibition Audio Routing

The application is designed to use an external multi-output audio interface for the exhibition.

At startup, JUCE first creates a normal stereo output. This provides a safe working output even when the exhibition interface is not connected.

The program then searches for the preferred exhibition audio device.

If the preferred interface is found, JUCE switches to that device and activates the configured stereo output pair.

Conceptually:

```text
JUCE stereo output

LEFT  -----------------> interface output A
RIGHT -----------------> interface output B

                              |
                              v

                       stereo headphones
```

The physical output pair is configured in `MainComponent.h`.

JUCE uses zero-based channel numbering internally, so:

```text
JUCE channel 0 = physical output 1
JUCE channel 1 = physical output 2
JUCE channel 2 = physical output 3
JUCE channel 3 = physical output 4
...
```

For example, a configuration using:

```cpp
preferredOutputLeftChannel  = 0;
preferredOutputRightChannel = 1;
```

routes the application to physical outputs **1 and 2** of the interface.

If another stereo pair is required for the exhibition, these values can be changed without rewriting the audio-processing system.

## Audio Interface Fallback

The preferred exhibition interface is **not required for development**.

If the configured interface cannot be found, the application keeps JUCE's existing/default audio output rather than terminating.

This allows the same program to run using:

- the exhibition audio interface;
- built-in computer audio;
- another development audio device.

During operation, the program periodically checks whether the preferred audio interface is active and attempts to restore the preferred configuration if necessary.

This is intended to make the installation more resilient to an interface being disconnected or becoming temporarily unavailable.

---

# Assets

Runtime audio assets are stored relative to the project's `Assets` directory.

The application should not rely on absolute paths such as:

```text
/Users/name/Documents/...
```

This is important when moving the work between development and exhibition computers.

The expected structure is approximately:

```text
Assets/
├── playlist.json
├── dub_siren.wav
└── playlist/
    ├── track_01.mp3
    ├── track_02.mp3
    └── ...
```

Playlist entries should therefore use relative paths such as:

```json
{
    "file": "playlist/track_01.mp3",
    "title": "Track Title",
    "artist": "Artist"
}
```

rather than paths specific to one computer.

The dub siren is loaded directly from:

```text
Assets/dub_siren.wav
```

---

# MIDI Controls

The DJ controller is the primary interface for manipulating the audio application.

The current control system includes:

| CC | Function |
|---:|---|
| 8 | Delay send |
| 9 | Delay time |
| 10 | Split screen + associated split delay |
| 11 | Dub crossfader |
| 12 | Dub siren pitch |
| 14 | Headphone/output gain |
| 16 | Reverb damping |
| 17 | Tops |
| 18 | Reverb wet level |
| 19 | Mids |
| 20 | Reverb room size |
| 21 | Bass |
| 23 | Delay feedback |
| 24 | Secondary jog |
| 25 | Main jog |

Reverb control is therefore restricted to **CC 16, 18 and 20**.

The split-screen control on CC 10 also produces an associated delay response. The audio direction is inverted to match the visual behaviour: moving towards the split-screen/double-exposure state increases the associated delay rather than increasing it at the no-split end.

The dub siren is triggered from the controller and its pitch is manipulated using CC 12.

---

# OSC

The JUCE application communicates with the openFrameworks visual application using **Open Sound Control (OSC)**.

The OSC sender connects locally to:

```text
127.0.0.1
port 9000
```

This allows controller changes processed by JUCE to be forwarded to the visual system.

Examples include:

```text
/delay/mixValue
/delay/feedbackValue
/delay/delayTime

/reverb/...

/eq/bass
/eq/mids
/eq/tops

/splitScreen/amount

/dub/crossfader
/dub/sirenPitch
```

The openFrameworks application interprets these messages and uses them to drive corresponding visual behaviour.

---

# Requirements

## Software

- JUCE Framework
- C++17-compatible compiler
- Corresponding **Dread Frequencies** openFrameworks visual application
- openFrameworks and its required addons for the visual application

The visual project currently uses addons including:

- `ofxOsc`
- `ofxMidi`
- `ofxJSON`

## Hardware

The exhibition setup requires:

- DJ/MIDI controller
- Exhibition computer
- Multi-output audio interface
- Stereo headphones
- Installation display
- Required speaker/subwoofer system for the complete installation

The exact physical audio-interface routing should be confirmed on the exhibition machine before opening.

---

# Running the Installation

## 1. Connect Hardware

Connect the required installation hardware before launching the applications:

- DJ controller
- audio interface
- headphones
- display
- speaker/subwoofer system as required

## 2. Check the Audio Interface

Ensure the exhibition audio interface is visible to the operating system.

The JUCE application will attempt to select the preferred interface automatically.

If it cannot find the interface, it will continue using the available system/default audio output so that the application remains operational.

## 3. Launch the openFrameworks Visual Application

Launch the corresponding Dread Frequencies visual application.

This receives OSC messages from the JUCE audio engine and renders the interactive documentary and visual effects.

## 4. Launch OSCaudiofx

Launch the JUCE audio application.

On startup it will:

```text
register audio formats
        |
        v
connect OSC
        |
        v
load playlist + dub siren
        |
        v
initialise stereo audio
        |
        v
look for preferred audio interface
        |
        v
configure stereo output pair
        |
        v
load playlist
        |
        v
initialise MIDI
```

## 5. Test the Installation

Before leaving the installation unattended, confirm:

- playlist audio is playing;
- left and right headphone channels are present;
- the correct audio-interface outputs are being used;
- the DJ controller is detected;
- delay responds correctly;
- reverb responds correctly;
- bass, mids and tops respond correctly;
- the dub crossfader responds;
- the dub siren triggers;
- siren pitch control responds;
- OSC-driven visual effects respond;
- topic/video navigation responds correctly.

---

# Development and Exhibition Notes

The application is designed so that development and exhibition environments do not need identical audio hardware.

During development, JUCE can use the computer's normal/default output.

For exhibition, the preferred external interface and stereo output pair can be selected automatically.

Media paths should remain relative to the project `Assets` directory so that the application can be transferred to another machine without rewriting paths.

Before producing the final exhibition build:

1. Confirm that all playlist and siren assets are present.
2. Remove any remaining absolute file paths.
3. Confirm the required audio-interface name and output pair.
4. Test the complete MIDI mapping.
5. Test OSC communication with the final openFrameworks build.
6. Run an extended stability/stress test.
7. Test startup from a fresh application launch with the complete exhibition hardware connected.
