#pragma once

#include "ofMain.h"
#include "ofxMidi.h"
#include <algorithm>
#include <random>
#include <string>
#include <vector>

struct Clip
{
	std::string videoPath;
	std::string description;
	ofVideoPlayer video;
};

struct AnchorPoint
{
	std::string videoPath;
	std::string description;
	ofVideoPlayer video;
};

struct Topic
{
	std::string name;
	AnchorPoint anchor;
	std::vector<Clip> footage;
};

struct SplitScreenClip
{
	int id = 0;
	std::string file;
	ofVideoPlayer video;
};

struct IdleClip
{
	int id = 0;
	std::string file;
	ofVideoPlayer video;
};

class ChronologyManager : public ofxMidiListener
{
public:
	void setup();
	void update();
	void draw();
	void drawSplitScreen();
	void keyPressed(int key);

	void newMidiMessage(ofxMidiMessage& message) override;

	void registerInteraction();
	void enterIdleState();
	void playIdleClip();
	void exitIdleState();

	void selectRandomTopic();
	void randomizeFootageOrder();
	void playCurrentFootage();

	void startLooping();
	void stopLooping();

	ofVideoPlayer* getCurrentVideo();

	void toggleSplitScreen(bool enable);
	void randomizeSplitScreenOrder();

	bool isPlayingAnchor() const { return playingAnchor; }

	// These remain public because ofApp currently reads them directly.
	bool isSplitScreenActive = false;
	float splitScreenAmount = 0.0f;
	std::vector<SplitScreenClip> splitScreenClips;
	int currentSplitIndex = 0;

	bool isIdle = false;

private:
	std::vector<Topic> topics;
	Topic* currentTopic = nullptr;
	int currentFootageIndex = 0;
	bool playingAnchor = true;

	ofxMidiIn midiIn;
	ofxMidiMessage midiMessage;

	// Manual clip loop.
	bool isLooping = false;
	float loopStartTime = 0.0f;
	float loopEndTime = 0.0f;

	// Reconstructed defaults
	float loopDuration = 1.0f;
	int loopCount = 0;
	int maxLoopCount = 4;

	// Split-screen state.
	bool splitScreenMode = false;
	bool needReshuffleSplitScreen = true;
	bool note66Pressed = false;
	bool note66HasAdvanced = false;

	// Broadcast/idle state.
	std::vector<IdleClip> idleClips;
	int currentIdleClipIndex = -1;
	float lastInteractionTime = 0.0f;
	float idleTimeout = 20.0f;
};
