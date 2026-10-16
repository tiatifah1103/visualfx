#pragma once

#include "ofMain.h"
#include "ofxMidi.h"

#include <algorithm>
#include <atomic>
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


class ChronologyManager :
	public ofxMidiListener
{
public:

	void setup();

	void update();

	void draw();

	void drawSplitScreen();

	void keyPressed(
		int key
	);


	void shutdown();


	void newMidiMessage(
		ofxMidiMessage& message
	) override;


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

	ofVideoPlayer* getCurrentSplitScreenVideo();


	void toggleSplitScreen(
		bool enable
	);

	void randomizeSplitScreenOrder();

	void advanceSplitScreen();


	bool isPlayingAnchor() const
	{
		return playingAnchor;
	}


	// These remain public because ofApp currently reads them.
	//
	// We reduce direct access below, but do not need to rewrite
	// the whole application's interface right now.

	bool isSplitScreenActive =
		false;

	float splitScreenAmount =
		0.0f;

	std::vector<SplitScreenClip>
		splitScreenClips;

	int currentSplitIndex =
		0;


	bool isIdle =
		false;


private:

	// ============================================================
	// MAIN-THREAD MIDI PROCESSING
	// ============================================================

	void processPendingMidi();


	void advanceFootage(
		int direction
	);


	bool hasValidCurrentFootage()
		const;


	bool hasValidSplitScreenClip()
		const;

	void rebuildIdlePlayOrder();

	bool playNextIdleClip();

	// ============================================================
	// NORMAL CHRONOLOGY
	// ============================================================

	std::vector<Topic> topics;

	Topic* currentTopic =
		nullptr;


	int currentFootageIndex =
		0;


	bool playingAnchor =
		true;


	// ============================================================
	// MIDI
	// ============================================================

	ofxMidiIn midiIn;


	// IMPORTANT:
	//
	// The MIDI callback is allowed to modify ONLY these atomic
	// request values.
	//
	// It must never touch an ofVideoPlayer.

	std::atomic<bool>
		pendingInteraction{
			false
		};


	std::atomic<bool>
		pendingTopicChange{
			false
		};


	std::atomic<int>
		pendingFootageDirection{
			0
		};


	// +1 = start
	// -1 = stop
	//  0 = nothing

	std::atomic<int>
		pendingLoopCommand{
			0
		};


	// Latest raw MIDI CC10 value.
	//
	// -1 means nothing pending.

	std::atomic<int>
		pendingSplitAmountRaw{
			-1
		};


	std::atomic<bool>
		pendingSplitAdvance{
			false
		};


	// Note 66 needs a proper press/release latch.
	//
	// This also understands NOTE_ON velocity 0 as release.

	std::atomic<bool>
		note66Down{
			false
		};


	std::atomic<bool>
		shuttingDown{
			false
		};


	// Protect AVFoundation from absurd repeated topic teardown /
	// startup while somebody stress-mashes the button.

	float lastTopicSwitchTime =
		-1000.0f;


	float topicSwitchCooldown =
		0.35f;


	// ============================================================
	// MANUAL CLIP LOOP
	// ============================================================

	// ============================================================
	// MANUAL JOGWHEEL LOOP
	// ============================================================

	bool isLooping = false;

	float loopStartTime = 0.0f;
	float loopEndTime = 0.0f;


	// Six-second audiovisual loop.
	float loopDuration = 6.0f;


	// Automatically recover after ten complete passes.
	int loopCount = 0;
	int maxLoopCount = 10;



	// ============================================================
	// SPLIT SCREEN
	// ============================================================

	bool splitScreenMode =
		false;


	bool needReshuffleSplitScreen =
		true;


	// ============================================================
	// IDLE
	// ============================================================

	std::vector<IdleClip>
		idleClips;


	int currentIdleClipIndex =
		-1;


	float lastInteractionTime =
		0.0f;


	float idleTimeout =
		240.0f;
	
	
	// ============================================================
	// IDLE SHUFFLE BAG
	//
	// idleClips itself never moves.
	//
	// This contains indices into idleClips, for example:
	//
	//     3, 0, 5, 1, 4, 2
	//
	// Every index is consumed once before the order is rebuilt.
	// ============================================================

	std::vector<int> idlePlayOrder;

	std::size_t idlePlayPosition = 0;

	// Remembers the last clip actually selected.
	// Used to stop the last clip of one shuffle cycle becoming
	// the first clip of the next cycle.
	int lastIdleClipIndex = -1;
};
