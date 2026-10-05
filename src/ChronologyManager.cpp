#include "ChronologyManager.hpp"



void ChronologyManager::setup()

{

	// ============================================================

	// LOAD NORMAL FOOTAGE

	// ============================================================

	ofFile file("footage.json");

	if (file.exists())

	{

		ofJson json = ofLoadJson(file);

		for (const auto& topicJson : json["topics"])

		{

			Topic topic;

			topic.name = topicJson["topic_name"];

			topic.anchor.videoPath =

				topicJson["anchor_points"][0]["video_path"];

			topic.anchor.description =

				topicJson["anchor_points"][0]["description"];

			topic.anchor.video.load(topic.anchor.videoPath);

			topic.anchor.video.setLoopState(OF_LOOP_NONE);

			for (const auto& footageJson : topicJson["footage"])

			{

				Clip clip;

				clip.videoPath =

					footageJson["video_path"];

				clip.description =

					footageJson["description"];

				clip.video.load(clip.videoPath);

				clip.video.setLoopState(OF_LOOP_NORMAL);

				topic.footage.push_back(clip);

			}

			topics.push_back(topic);

		}

		selectRandomTopic();

	}



	// ============================================================
	// LOAD SPLIT-SCREEN VIDEOS
	// ============================================================

	ofJson splitJson =
		ofLoadJson(
			"splitscreen.json"
		);


	for (
		const auto& entry :
		splitJson["splitScreens"]
	)
	{
		SplitScreenClip clip;


		if (
			entry.contains(
				"id"
			)
		)
		{
			if (
				entry["id"].is_number_integer()
			)
			{
				clip.id =
					entry["id"].get<int>();
			}
			else if (
				entry["id"].is_string()
			)
			{
				try
				{
					clip.id =
						std::stoi(
							entry["id"]
								.get<std::string>()
						);
				}
				catch (...)
				{
					clip.id =
						0;
				}
			}
			else
			{
				clip.id =
					0;
			}
		}
		else
		{
			clip.id =
				0;
		}


		clip.file =
			entry.value(
				"file",
				""
			);


		if (
			clip.file.empty()
		)
		{
			ofLogWarning()
				<< "Skipping split-screen entry with no file.";

			continue;
		}


		clip.video.load(
			"videos/" +
			clip.file
		);


		clip.video.setLoopState(
			OF_LOOP_NORMAL
		);


		clip.video.setVolume(
			0.0f
		);


		clip.video.stop();


		splitScreenClips.push_back(
			clip
		);


		ofLogNotice()
			<< "Loaded split-screen clip: "
			<< clip.file;
	}


	// ============================================================
	// LOAD IDLE VIDEOS
	// ============================================================

	ofJson idleJson =
		ofLoadJson(
			"idle.json"
		);


	for (
		const auto& entry :
		idleJson["idleVideos"]
	)
	{
		IdleClip clip;


		if (
			entry.contains(
				"id"
			)
		)
		{
			if (
				entry["id"].is_number_integer()
			)
			{
				clip.id =
					entry["id"].get<int>();
			}
			else if (
				entry["id"].is_string()
			)
			{
				try
				{
					clip.id =
						std::stoi(
							entry["id"]
								.get<std::string>()
						);
				}
				catch (...)
				{
					clip.id =
						0;
				}
			}
			else
			{
				clip.id =
					0;
			}
		}
		else
		{
			clip.id =
				0;
		}


		clip.file =
			entry.value(
				"file",
				""
			);


		if (
			clip.file.empty()
		)
		{
			ofLogWarning()
				<< "Skipping idle entry with no file.";

			continue;
		}


		clip.video.load(
			"videos/" +
			clip.file
		);


		clip.video.setLoopState(
			OF_LOOP_NONE
		);


		clip.video.setVolume(
			1.0f
		);


		clip.video.stop();


		idleClips.push_back(
			clip
		);


		ofLogNotice()
			<< "Loaded idle clip: "
			<< clip.file;
	}



	// ============================================================

	// MIDI

	// ============================================================

	midiIn.listInPorts();

	midiIn.openPort(0);

	midiIn.addListener(this);

	midiIn.setVerbose(true);



	// ============================================================

	// IDLE TIMER

	// ============================================================

	lastInteractionTime = ofGetElapsedTimef();

}



void ChronologyManager::update()

{

	if (!currentTopic)

		return;

	// ============================================================
	// IDLE STATE
	// ============================================================

	if (
		isIdle
	)
	{
		if (
			idleClips.empty()
		)
		{
			return;
		}


		if (
			currentIdleClipIndex < 0 ||
			currentIdleClipIndex >=
				static_cast<int>(
					idleClips.size()
				)
		)
		{
			currentIdleClipIndex =
				0;


			playIdleClip();


			return;
		}


		auto& idleVideo =
			idleClips[
				currentIdleClipIndex
			].video;


		idleVideo.update();


		// ========================================================
		// HAS THIS CLIP FINISHED?
		// ========================================================

		const bool movieFinished =
			idleVideo.getIsMovieDone();


		// Backup because some video backends are less reliable
		// with getIsMovieDone().
		const bool reachedEnd =
			idleVideo.getDuration() >
				0.0f &&
			idleVideo.getPosition() >=
				0.995f;


		if (
			movieFinished ||
			reachedEnd
		)
		{
			idleVideo.stop();


			// ====================================================
			// ADVANCE THROUGH idle.json IN ORDER
			//
			// After the final clip:
			//
			// final clip -> first clip
			//
			// The ARRAY loops.
			// The individual movies do not.
			// ====================================================

			currentIdleClipIndex =
				(
					currentIdleClipIndex +
					1
				)
				%
				static_cast<int>(
					idleClips.size()
				);


			playIdleClip();
		}


		return;
	}



	// ============================================================

	// NORMAL FOOTAGE

	// ============================================================

	// If we are playing the anchor, handle it separately.

	if (playingAnchor)

	{

		currentTopic->anchor.video.update();

		if (currentTopic->anchor.video.getIsMovieDone())

		{

			currentTopic->anchor.video.stop();

			playingAnchor = false;

			currentFootageIndex = 0;

			randomizeFootageOrder();

		}

		return;

	}

	// ============================================================

	// MANUAL JOGWHEEL LOOP

	// ============================================================

	if (isLooping)

	{

		auto& video =

			currentTopic->footage[currentFootageIndex].video;

		float duration = video.getDuration();

		if (duration > 0.0f)

		{

			float currentTime =

				video.getPosition() * duration;

			if (currentTime >= loopEndTime)

			{

				// One manual loop completed

				loopCount++;

				if (loopCount >= maxLoopCount)

				{

					// Automatically leave manual loop

					isLooping = false;

					loopStartTime = 0.0f;

					loopEndTime = 0.0f;

					video.setLoopState(OF_LOOP_NORMAL);

					video.play();

					ofLog()

						<< "Manual loop limit reached. "

						<< "Continuing playback.";

				}

				else

				{

					// Continue manual looping

					float normalizedLoopStart =

						loopStartTime / duration;

					video.setPosition(normalizedLoopStart);

					video.play();

					ofLog()

						<< "Manual loop "

						<< loopCount

						<< " / "

						<< maxLoopCount;

				}

			}

		}

	}

	// ============================================================

	// UPDATE NORMAL FOOTAGE

	// ============================================================

	currentTopic->footage[currentFootageIndex].video.update();

	// ============================================================

	// CHECK FOR IDLE TIMEOUT

	// ============================================================

	float timeSinceInteraction =

		ofGetElapsedTimef() - lastInteractionTime;

	if (timeSinceInteraction >= idleTimeout)

	{

		enterIdleState();

	}

}

void ChronologyManager::registerInteraction()

{

	// Reset inactivity timer

	lastInteractionTime = ofGetElapsedTimef();

	// If currently idle, wake the installation.

	if (isIdle)

	{

		exitIdleState();

	}

}

//--------------------------------------------------------------
void ChronologyManager::enterIdleState()
{
	if (
		isIdle
	)
	{
		return;
	}


	if (
		idleClips.empty()
	)
	{
		ofLogWarning()
			<< "Idle timeout reached, but no idle videos are available.";


		lastInteractionTime =
			ofGetElapsedTimef();


		return;
	}


	isIdle =
		true;


	// ============================================================
	// STOP NORMAL FOOTAGE
	// ============================================================

	if (
		currentTopic &&
		!playingAnchor &&
		!currentTopic->footage.empty()
	)
	{
		currentTopic
			->footage[
				currentFootageIndex
			]
			.video
			.stop();
	}


	// ============================================================
	// STOP SPLIT SCREEN
	// ============================================================

	splitScreenMode =
		false;


	isSplitScreenActive =
		false;


	for (
		auto& clip :
		splitScreenClips
	)
	{
		clip.video.stop();
	}


	// ============================================================
	// RESET IDLE PLAYLIST
	//
	// idle.json is treated as a playlist.
	// ============================================================

	for (
		auto& clip :
		idleClips
	)
	{
		clip.video.stop();


		clip.video.setPosition(
			0.0f
		);


		clip.video.setLoopState(
			OF_LOOP_NONE
		);
	}


	// Always begin with first idle JSON entry.
	currentIdleClipIndex =
		0;


	playIdleClip();


	ofLogNotice()
		<< "========================================";


	ofLogNotice()
		<< "ENTERING IDLE STATE";


	ofLogNotice()
		<< "Starting idle playlist: "
		<< idleClips[
			   currentIdleClipIndex
		   ].file;


	ofLogNotice()
		<< "========================================";
}

//--------------------------------------------------------------
void ChronologyManager::playIdleClip()
{
	if (
		currentIdleClipIndex < 0 ||
		currentIdleClipIndex >=
			static_cast<int>(
				idleClips.size()
			)
	)
	{
		return;
	}


	auto& idleClip =
		idleClips[
			currentIdleClipIndex
		];


	idleClip.video.stop();


	idleClip.video.setPosition(
		0.0f
	);


	// ============================================================
	// CRITICAL
	//
	// Individual idle files DO NOT LOOP.
	//
	// ChronologyManager itself moves to the next idle JSON item.
	// ============================================================

	idleClip.video.setLoopState(
		OF_LOOP_NONE
	);


	idleClip.video.setVolume(
		1.0f
	);


	idleClip.video.play();


	ofLogNotice()
		<< "Playing idle clip "
		<< currentIdleClipIndex + 1
		<< " / "
		<< idleClips.size()
		<< ": "
		<< idleClip.file;
}

void ChronologyManager::exitIdleState()

{

	if (!isIdle)

		return;

	ofLogNotice()

		<< "========================================";

	ofLogNotice()

		<< "EXITING IDLE STATE";

	ofLogNotice()

		<< "Deck interaction detected.";

	ofLogNotice()

		<< "========================================";



	// ============================================================

	// STOP IDLE VIDEO

	// ============================================================

	if (currentIdleClipIndex >= 0 &&

		currentIdleClipIndex < static_cast<int>(idleClips.size()))

	{

		idleClips[currentIdleClipIndex].video.stop();

	}

	currentIdleClipIndex = -1;

	isIdle = false;



	// ============================================================

	// RESET IDLE TIMER

	// ============================================================

	lastInteractionTime = ofGetElapsedTimef();



	// ============================================================

	// RESET MANUAL LOOP

	// ============================================================

	isLooping = false;

	loopCount = 0;

	loopStartTime = 0.0f;

	loopEndTime = 0.0f;



	// ============================================================

	// START FRESH NORMAL FOOTAGE

	// ============================================================

	randomizeFootageOrder();

	ofLogNotice()

		<< "Returned to normal footage.";

}

void ChronologyManager::draw()

{

	ofBackground(0);

	if (!currentTopic)

		return;

	// ============================================================

	// IDLE STATE

	// ============================================================

	if (isIdle)

	{

		if (currentIdleClipIndex >= 0 &&

			currentIdleClipIndex < static_cast<int>(idleClips.size()))

		{

			idleClips[currentIdleClipIndex].video.draw(

				0,

				0,

				ofGetWidth(),

				ofGetHeight()

			);

		}

		return;

	}

	// ============================================================

	// SPLIT SCREEN

	// ============================================================

	if (splitScreenMode && !splitScreenClips.empty())

	{

		drawSplitScreen();

		return;

	}

	// ============================================================

	// NORMAL FULLSCREEN

	// ============================================================

	if (playingAnchor)

	{

		currentTopic->anchor.video.draw(

			0,

			0,

			ofGetWidth(),

			ofGetHeight()

		);

	}

	else

	{

		currentTopic->footage[currentFootageIndex].video.draw(

			0,

			0,

			ofGetWidth(),

			ofGetHeight()

		);

	}

}



void ChronologyManager::drawSplitScreen() {

	float halfWidth = ofGetWidth() / 2.0f;

	float height = ofGetHeight();

	// Draw main footage - left side

	if (!playingAnchor) {

		// Ensures main video is playing

		if (!currentTopic->footage[currentFootageIndex].video.isPlaying()) {

			currentTopic->footage[currentFootageIndex].video.play();

		}

		currentTopic->footage[currentFootageIndex].video.draw(0, 0, halfWidth, height);

	} else {

		// If anchor active, draw instead

		currentTopic->anchor.video.draw(0, 0, halfWidth, height);

	}

	// Draw right side (split screen content)

	if (splitScreenMode && !splitScreenClips.empty()) {

		// Ensure split screen video is playing

		if (!splitScreenClips[currentSplitIndex].video.isPlaying()) {

			splitScreenClips[currentSplitIndex].video.play();

		}

		// Update the right side video

		splitScreenClips[currentSplitIndex].video.update();

		// Aspect-ratio correct dimensions

		float videoWidth = splitScreenClips[currentSplitIndex].video.getWidth();

		float videoHeight = splitScreenClips[currentSplitIndex].video.getHeight();

		float videoAspect = videoWidth / videoHeight;

		float screenAspect = halfWidth / height;

		float drawWidth, drawHeight;

		float drawX = halfWidth;

		float drawY = 0;

		if (videoAspect > screenAspect) {

			// Width-constrained scaling

			drawWidth = halfWidth;

			drawHeight = drawWidth / videoAspect;

			drawY = (height - drawHeight) / 2.0f;

		} else {

			// Height-constrained scaling

			drawHeight = height;

			drawWidth = drawHeight * videoAspect;

			drawX = halfWidth + (halfWidth - drawWidth) / 2.0f;

		}

		splitScreenClips[currentSplitIndex].video.draw(drawX, drawY, drawWidth, drawHeight);

	}

}

//--------------------------------------------------------------
void ChronologyManager::keyPressed(
	int key
)
{
	// ============================================================
	// DEBUG: SKIP CURRENT ANCHOR
	//
	// A = immediately leave the anchor and enter its footage.
	//
	// This exists for technical testing only.
	// ============================================================

	if (
		key == 'a' ||
		key == 'A'
	)
	{
		if (
			currentTopic &&
			playingAnchor
		)
		{
			registerInteraction();

			currentTopic
				->anchor
				.video
				.stop();

			playingAnchor =
				false;

			currentFootageIndex =
				0;

			randomizeFootageOrder();

			ofLogNotice("DEBUG")
				<< "A -> anchor skipped; entered normal footage";
		}

		return;
	}


	// ============================================================
	// CHANGE TOPIC
	//
	// Unlike the old keyboard implementation, N is allowed during
	// an anchor as well. This mirrors the intended installation
	// behaviour more closely.
	// ============================================================

	if (
		key == 'n' ||
		key == 'N'
	)
	{
		if (
			currentTopic
		)
		{
			registerInteraction();

			selectRandomTopic();

			ofLogNotice("DEBUG")
				<< "N -> changed topic / started new anchor";
		}

		return;
	}


	// ============================================================
	// THE REMAINING CHRONOLOGY CONTROLS ONLY OPERATE ON NORMAL
	// FOOTAGE.
	// ============================================================

	if (
		!currentTopic ||
		playingAnchor
	)
	{
		return;
	}


	// ============================================================
	// NEXT FOOTAGE
	// ============================================================

	if (
		key == 'q' ||
		key == 'Q'
	)
	{
		if (
			currentTopic->footage.empty()
		)
		{
			return;
		}

		registerInteraction();

		currentFootageIndex =
			(
				currentFootageIndex +
				1
			)
			%
			currentTopic
				->footage
				.size();

		playCurrentFootage();

		ofLogNotice("DEBUG")
			<< "Q -> next footage";

		return;
	}


	// ============================================================
	// PREVIOUS FOOTAGE
	// ============================================================

	if (
		key == 'w' ||
		key == 'W'
	)
	{
		if (
			currentTopic->footage.empty()
		)
		{
			return;
		}

		registerInteraction();

		currentFootageIndex =
			(
				currentFootageIndex -
				1 +
				currentTopic
					->footage
					.size()
			)
			%
			currentTopic
				->footage
				.size();

		playCurrentFootage();

		ofLogNotice("DEBUG")
			<< "W -> previous footage";

		return;
	}


	// ============================================================
	// MANUAL LOOP
	// ============================================================

	if (
		key == 'l' ||
		key == 'L'
	)
	{
		registerInteraction();

		if (
			isLooping
		)
		{
			stopLooping();

			ofLogNotice("DEBUG")
				<< "L -> manual loop OFF";
		}
		else
		{
			startLooping();

			ofLogNotice("DEBUG")
				<< "L -> manual loop ON";
		}

		return;
	}
}

void ChronologyManager::selectRandomTopic() {

	// Stop all videos from the current topic (if any) before switching

	if (currentTopic) {

		currentTopic->anchor.video.stop();

		for (auto& clip : currentTopic->footage) {

			clip.video.stop();

		}

	}

	int randomIndex;

	do {

		randomIndex = ofRandom(topics.size());

	} while (currentTopic && &topics[randomIndex] == currentTopic);

	currentTopic = &topics[randomIndex];

	playingAnchor = true;

	// Play the new topic's anchor video

	currentTopic->anchor.video.play();

	ofLog() << "Switched to topic: " << currentTopic->name;

}

void ChronologyManager::randomizeFootageOrder() {

	std::random_device rd;  // gets random seed from the hardware

	std::mt19937 g(rd());               // Seed the random number generator

	std::shuffle(currentTopic->footage.begin(), currentTopic->footage.end(), g); // Shuffles the footage vector

	playCurrentFootage(); // Plays the first video in the newly shuffled order

}

void ChronologyManager::playCurrentFootage() {

	// stops all other video clips except the one currently being played

	for (auto& clip : currentTopic->footage) {

		if (&clip != &currentTopic->footage[currentFootageIndex]) {

			clip.video.stop(); // stops non-current videos to avoid overlap

		}

	}

	// Start/restart the current video

	currentTopic->footage[currentFootageIndex].video.setLoopState(OF_LOOP_NORMAL); // loop video

	currentTopic->footage[currentFootageIndex].video.play();

	isLooping = false; // Reset manual looping

	ofLog() << "Playing footage (looped): " << currentTopic->footage[currentFootageIndex].videoPath; // Log current video

}

// Starts a short manual loop near the current playback position (for the right jogwheel)

void ChronologyManager::startLooping() {

	float currentTime = currentTopic->footage[currentFootageIndex].video.getPosition() *

						currentTopic->footage[currentFootageIndex].video.getDuration();

	loopStartTime = std::max(0.0f, currentTime - loopDuration);

	loopEndTime = currentTime;

	// Reset the loop counter whenever a new manual loop begins

	loopCount = 0;

	isLooping = true;

	ofLog() << "Started manual loop from "

			<< loopStartTime

			<< "s to "

			<< loopEndTime

			<< "s";

}

// Stops any active manual looping

void ChronologyManager::stopLooping() {

	isLooping = false;                 // Disable looping

	loopStartTime = 0;                 // Reset loop start

	loopEndTime = 0;                   // Reset loop end

	loopCount = 0;

	ofLog() << "Exited the loop.";     // Log loop exit

}

void ChronologyManager::newMidiMessage(ofxMidiMessage& message) {

	midiMessage = message; // Store the incoming message for debugging

	if (currentTopic && !playingAnchor) {

		// Handle jogwheel (Controller #25)

		if (message.status == MIDI_CONTROL_CHANGE && message.control == 25) {

			static bool jogwheelSpinning = false;

			static bool jogwheelReversed = false;

			static unsigned long lastMovementTime = 0;

			const int movementThresholdMin = 5;

			const int movementThresholdMax = 10;

			const int reverseMovementThresholdMin = 110;

			const int reverseMovementThresholdMax = 124;

			if (message.value >= movementThresholdMin && message.value <= movementThresholdMax) {

				jogwheelSpinning = true;

				jogwheelReversed = false;

				lastMovementTime = ofGetElapsedTimeMillis();

			} else if (message.value >= reverseMovementThresholdMin && message.value <= reverseMovementThresholdMax) {

				jogwheelReversed = true;

				jogwheelSpinning = false;

				lastMovementTime = ofGetElapsedTimeMillis();

			} else if (jogwheelSpinning && message.value == 1) {

				unsigned long now = ofGetElapsedTimeMillis();

				if (now - lastMovementTime > 100) {

					jogwheelSpinning = false;

					registerInteraction();

					currentFootageIndex = (currentFootageIndex + 1) % currentTopic->footage.size();

					playCurrentFootage();

					ofLog() << "Jogwheel released (clockwise): Advancing to next clip";

				}

			} else if (jogwheelReversed && message.value == 127) {

				unsigned long now = ofGetElapsedTimeMillis();

				if (now - lastMovementTime > 100) {

					jogwheelReversed = false;

					registerInteraction();

					currentFootageIndex = (currentFootageIndex - 1 + currentTopic->footage.size()) % currentTopic->footage.size();

					playCurrentFootage();

					ofLog() << "Jogwheel released (anti-clockwise): Going back to previous clip";

				}

			}

		}

		// ============================================================

		// MANUAL LOOP JOGWHEEL - CC24

		// ============================================================

		if (message.status == MIDI_CONTROL_CHANGE &&

			message.control == 24)

		{

			// Clockwise movement

			if (message.value >= 5 && message.value <= 10)

			{

				registerInteraction();

				if (!isLooping)

				{

					startLooping();

				}

				ofLog()

				<< "Jogwheel turned clockwise: "

				<< "Looping enabled.";

			}

			// Anti-clockwise movement

			if (message.value >= 110 && message.value <= 124)

			{

				registerInteraction();

				if (isLooping)

				{

					stopLooping();

				}

				ofLog()

				<< "Jogwheel turned anti-clockwise: "

				<< "Looping disabled.";

			}

		}



		// ============================================================

		// SPLIT SCREEN / DOUBLE EXPOSURE SLIDER - CC10

		// ============================================================

		if (message.status == MIDI_CONTROL_CHANGE &&

			message.control == 10)

		{

			// MIDI gives us 0 - 127.

			// Convert that into our visual range of 0.0 - 1.0.

			splitScreenAmount =

				1.0f -

				ofClamp(

					static_cast<float>(message.value) / 127.0f,

					0.0f,

					1.0f

				);

			// Moving this slider counts as interaction.

			registerInteraction();



			// Anything above essentially zero means that the

			// secondary video needs to be running.

			bool shouldBeActive =

				splitScreenAmount > 0.01f;



			// Only start/stop the actual video when crossing

			// between inactive and active.

			//

			// We DON'T toggle it constantly while the slider moves.

			if (splitScreenMode != shouldBeActive)

			{

				toggleSplitScreen(shouldBeActive);

			}



			ofLog()

				<< "CC10 composition amount: "

				<< splitScreenAmount;

		}





		// Handles split screen advancement

		if (message.status == MIDI_NOTE_ON && message.pitch == 66) {

			if (!note66Pressed && !note66HasAdvanced) {

				note66Pressed = true;

				note66HasAdvanced = true;

				if (splitScreenMode && !splitScreenClips.empty()) {

					// Stops current video

					registerInteraction();

					splitScreenClips[currentSplitIndex].video.stop();

					// Checks if reached end

					if (currentSplitIndex + 1 >= splitScreenClips.size()) {

						// Reshuffle and start from beginning

						randomizeSplitScreenOrder();

					} else {

						// Advance to next clip

						currentSplitIndex++;

					}

					// Start new video from beginning

					splitScreenClips[currentSplitIndex].video.play();

					ofLog() << "MIDI Note 66: Advanced to split screen clip "

					<< currentSplitIndex << " - " << splitScreenClips[currentSplitIndex].file;

				}

			}

		}

		else if (message.status == MIDI_NOTE_OFF && message.pitch == 66) {

			note66Pressed = false;

			note66HasAdvanced = false;

		}

	}



	if (!currentTopic) return;

	// Always allow topic switching via MIDI notes 60/51

	if (message.status == MIDI_NOTE_ON && (message.pitch == 60 || message.pitch == 51)) {

		registerInteraction();

		selectRandomTopic();

	}

}

ofVideoPlayer* ChronologyManager::getCurrentVideo()

{

	if (isIdle)

	{

		if (currentIdleClipIndex >= 0 &&

			currentIdleClipIndex < static_cast<int>(idleClips.size()))

		{

			return &idleClips[currentIdleClipIndex].video;

		}

		return nullptr;

	}

	if (!currentTopic)

		return nullptr;

	if (playingAnchor)

	{

		return &currentTopic->anchor.video;

	}

	if (currentFootageIndex >= 0 &&

		currentFootageIndex < static_cast<int>(currentTopic->footage.size()))

	{

		return &currentTopic->footage[currentFootageIndex].video;

	}

	return nullptr;

}





void ChronologyManager::toggleSplitScreen(bool enable) {

	if (playingAnchor) {

		enable = false;

		ofLog() << "Split screen disabled during anchor playback";

	}

	splitScreenMode = enable;

	isSplitScreenActive = enable;

	if (enable) {

		if (!splitScreenClips.empty()) {

			// Reshuffles when first activating split screen

			if (needReshuffleSplitScreen) {

				randomizeSplitScreenOrder();

			}

			// All split screen videos to loop

			for (auto& clip : splitScreenClips) {

				clip.video.setLoopState(OF_LOOP_NORMAL);

			}

			if (!splitScreenClips[currentSplitIndex].video.isPlaying()) {

				splitScreenClips[currentSplitIndex].video.play();

			}

			ofLog() << "Split screen activated with clip: " << splitScreenClips[currentSplitIndex].file;

			if (!playingAnchor && !currentTopic->footage[currentFootageIndex].video.isPlaying()) {

				playCurrentFootage();

			}

		} else {

			ofLogWarning() << "No split screen clips available!";

			splitScreenMode = false;

			isSplitScreenActive = false;

		}

	} else {

		for (auto& clip : splitScreenClips) {

			clip.video.setPaused(true);

		}

		// Reshuffle when split screen is activated

		needReshuffleSplitScreen = true;

		ofLog() << "Split screen deactivated (videos paused)";

	}

}

void ChronologyManager::randomizeSplitScreenOrder() {

	std::random_device rd;

	std::mt19937 g(rd());

	std::shuffle(splitScreenClips.begin(), splitScreenClips.end(), g);

	currentSplitIndex = 0;

	needReshuffleSplitScreen = false;

	ofLog() << "Randomized split screen clip order";

}

