# OSCvisualfx

## Description
This project is part of an interactive documentary that uses openFrameworks to create dynamic visual effects synchronized with audio manipulation. The visuals respond to user interactions through a DJ controller, providing an immersive experience.

### Key Features
- Motion blur, step printing.
- Audio-synchronized visuals triggered by real-time DJ controller input.
- Modular class-based architecture for each effect.

# Requirements
### Software
- **openFrameworks** 
- A C++17-compatible compiler
- **JUCE Framework** (for the audio engine)

- Required openFrameworks addons:
  - `ofxOsc`
  - `ofxMidi` 
  - `ofxJSON` 
  
### Running Instructions

To see the visual effects synchronized with the audio manipulation:

Connect the Hardware:
Ensure the MIDI controller (e.g. DJ controller) is plugged into your computer.

Build and Run the openFrameworks Code:
Open the openFrameworks project in your preferred IDE (Xcode, Visual Studio, etc.).
Build and run the project to initialize the visual effects and MIDI controller functionality.

Build and Run the JUCE Audio Engine:
Open the JUCE project in Projucer and export it to your IDE.
Build and run the JUCE audio engine to enable Open Sound Control (OSC) communication with the openFrameworks application.

Interaction:
Use the DJ controller to adjust audio effects (e.g., reverb, delay) and navigate the timeline.
Observe synchronized visual effects on the screen and manipulated audio in from the speaker.

## Anchor points

Each topic begins with an **anchor point**: a short contextual video which plays before the interactive footage for that topic.

Anchor points are intentionally **non-interactive**. While an anchor is playing, visual effects, split screen and normal footage manipulation are not shown. This is expected behaviour rather than an input or OSC failure.

Once the anchor finishes, the work automatically enters the interactive footage for that topic and the visual controls become active again.

For debugging, press **A** to skip the current anchor and move directly into interactive footage. Press **N** to select another topic and start its anchor.

## Keyboard debug controls

- **A** — Skip current anchor
- **N** — Change topic
- **Q** — Next footage clip
- **W** — Previous footage clip
- **L** — Toggle manual loop
- **4** — Toggle split screen
- **6** — Next split-screen clip
- **5** — Trigger dub siren visual
- **1** — Toggle reverb room size
- **2** — Toggle reverb wet / motion blur
- **3** — Toggle reverb damping
- **7** — Toggle reverb width
- **8** — Toggle delay mix / step print
- **9** — Toggle delay feedback / frame persistence
- **0** — Toggle short / long delay time
- **B** — Toggle bass / fisheye
- **M** — Toggle mids visual
- **T** — Toggle tops visual
- **C** — Toggle dub crossfader visual
- **P** — Toggle siren pitch visual
- **R** — Reset all visual debug controls
- **I** — Register interaction / reset idle timer
- **F** — Toggle fullscreen

The keyboard controls are intended for technical testing without requiring the DJ controller or JUCE application. Effect keys pass through the same `VisualState` system used by incoming OSC, so they can be used to distinguish visual-rendering problems from MIDI/OSC communication problems.
