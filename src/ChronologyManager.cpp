#include "ChronologyManager.hpp"



//--------------------------------------------------------------
void ChronologyManager::setup()
{
	// ============================================================
	// CLEAN START
	// ============================================================

	shuttingDown.store(false);

	pendingInteraction.store(false);
	pendingTopicChange.store(false);
	pendingFootageDirection.store(0);
	pendingLoopCommand.store(0);
	pendingSplitAmountRaw.store(-1);
	pendingSplitAdvance.store(false);

	note66Down.store(false);

	topics.clear();

	splitScreenClips.clear();

	idleClips.clear();

	idlePlayOrder.clear();

	idlePlayPosition =
		0;

	lastIdleClipIndex =
		-1;


	currentTopic =
		nullptr;

	currentFootageIndex =
		0;

	currentSplitIndex =
		0;

	currentIdleClipIndex =
		-1;

	playingAnchor = false;

	isIdle = false;

	splitScreenMode = false;
	isSplitScreenActive = false;
	splitScreenAmount = 0.0f;

	needReshuffleSplitScreen = true;

	isLooping = false;
	loopStartTime = 0.0f;
	loopEndTime = 0.0f;
	loopCount = 0;

	lastTopicSwitchTime = -1000.0f;


	// ============================================================
	// SAFE JSON LOADER
	// ============================================================

	auto loadJsonSafely =
		[](const std::string& path,
		   ofJson& output) -> bool
		{
			try
			{
				ofFile file(path);

				if (!file.exists())
				{
					ofLogError()
						<< "JSON file not found: "
						<< path;

					return false;
				}

				output =
					ofLoadJson(file);

				if (output.is_null())
				{
					ofLogError()
						<< "JSON file is empty/null: "
						<< path;

					return false;
				}

				return true;
			}
			catch (const std::exception& e)
			{
				ofLogError()
					<< "Could not load JSON "
					<< path
					<< ": "
					<< e.what();

				return false;
			}
			catch (...)
			{
				ofLogError()
					<< "Unknown error loading JSON: "
					<< path;

				return false;
			}
		};


	// ============================================================
	// SAFE ID READER
	// ============================================================

	auto readIdSafely =
		[](const ofJson& entry) -> int
		{
			if (!entry.is_object() ||
				!entry.contains("id"))
			{
				return 0;
			}

			try
			{
				const auto& idValue =
					entry["id"];

				if (idValue.is_number_integer())
				{
					return idValue.get<int>();
				}

				if (idValue.is_string())
				{
					return std::stoi(
						idValue.get<std::string>()
					);
				}
			}
			catch (...)
			{
			}

			return 0;
		};


	// ============================================================
	// LOAD NORMAL TOPICS
	// ============================================================

	ofJson footageJson;

	if (loadJsonSafely(
			"footage.json",
			footageJson))
	{
		if (!footageJson.is_object() ||
			!footageJson.contains("topics") ||
			!footageJson["topics"].is_array())
		{
			ofLogError()
				<< "footage.json does not contain a valid "
				<< "\"topics\" array.";
		}
		else
		{
			for (const auto& topicJson :
				 footageJson["topics"])
			{
				if (!topicJson.is_object())
				{
					ofLogWarning()
						<< "Skipping malformed topic entry.";

					continue;
				}


				// ====================================================
				// TOPIC NAME
				// ====================================================

				const std::string topicName =
					topicJson.value(
						"topic_name",
						std::string("Untitled Topic")
					);


				// ====================================================
				// ANCHOR VALIDATION
				// ====================================================

				if (!topicJson.contains(
						"anchor_points") ||
					!topicJson["anchor_points"]
						 .is_array() ||
					topicJson["anchor_points"]
						 .empty())
				{
					ofLogWarning()
						<< "Skipping topic with no anchor: "
						<< topicName;

					continue;
				}


				const auto& anchorJson =
					topicJson["anchor_points"][0];


				if (!anchorJson.is_object())
				{
					ofLogWarning()
						<< "Skipping malformed anchor for topic: "
						<< topicName;

					continue;
				}


				const std::string anchorPath =
					anchorJson.value(
						"video_path",
						std::string()
					);


				if (anchorPath.empty())
				{
					ofLogWarning()
						<< "Skipping topic with empty anchor path: "
						<< topicName;

					continue;
				}


				Topic topic;

				topic.name =
					topicName;

				topic.anchor.videoPath =
					anchorPath;

				topic.anchor.description =
					anchorJson.value(
						"description",
						std::string()
					);


				// ====================================================
				// LOAD ANCHOR
				// ====================================================

				const bool anchorLoaded =
					topic.anchor.video.load(
						topic.anchor.videoPath
					);


				if (!anchorLoaded)
				{
					ofLogError()
						<< "Skipping topic because anchor failed "
						<< "to load: "
						<< topic.anchor.videoPath;

					continue;
				}


				topic.anchor.video.setLoopState(
					OF_LOOP_NONE
				);


				topic.anchor.video.stop();


				// ====================================================
				// NORMAL FOOTAGE
				// ====================================================

				if (topicJson.contains("footage") &&
					topicJson["footage"].is_array())
				{
					for (const auto& clipJson :
						 topicJson["footage"])
					{
						if (!clipJson.is_object())
						{
							continue;
						}


						const std::string videoPath =
							clipJson.value(
								"video_path",
								std::string()
							);


						if (videoPath.empty())
						{
							ofLogWarning()
								<< "Skipping footage with empty path "
								<< "in topic: "
								<< topicName;

							continue;
						}


						Clip clip;

						clip.videoPath =
							videoPath;

						clip.description =
							clipJson.value(
								"description",
								std::string()
							);


						const bool loaded =
							clip.video.load(
								clip.videoPath
							);


						if (!loaded)
						{
							ofLogError()
								<< "Skipping footage that failed "
								<< "to load: "
								<< clip.videoPath;

							continue;
						}


						clip.video.setLoopState(
							OF_LOOP_NORMAL
						);


						clip.video.stop();


						topic.footage.push_back(
							clip
						);
					}
				}


				// ====================================================
				// REQUIRE AT LEAST ONE NORMAL CLIP
				//
				// Otherwise the topic would finish its anchor and
				// have nowhere valid to go.
				// ====================================================

				if (topic.footage.empty())
				{
					ofLogWarning()
						<< "Skipping topic with no valid normal "
						<< "footage: "
						<< topicName;

					topic.anchor.video.stop();

					continue;
				}


				topics.push_back(
					topic
				);


				ofLogNotice()
					<< "Loaded topic: "
					<< topicName
					<< " ("
					<< topic.footage.size()
					<< " clips)";
			}
		}
	}


	// ============================================================
	// LOAD SPLIT SCREEN
	// ============================================================

	ofJson splitJson;

	if (loadJsonSafely(
			"splitscreen.json",
			splitJson))
	{
		if (splitJson.is_object() &&
			splitJson.contains("splitScreens") &&
			splitJson["splitScreens"].is_array())
		{
			for (const auto& entry :
				 splitJson["splitScreens"])
			{
				if (!entry.is_object())
				{
					continue;
				}


				const std::string file =
					entry.value(
						"file",
						std::string()
					);


				if (file.empty())
				{
					ofLogWarning()
						<< "Skipping split-screen entry "
						<< "with no file.";

					continue;
				}


				SplitScreenClip clip;

				clip.id =
					readIdSafely(
						entry
					);

				clip.file =
					file;


				const std::string fullPath =
					"footage/" +
					clip.file;


				if (!clip.video.load(
						fullPath))
				{
					ofLogError()
						<< "Skipping split-screen video that "
						<< "failed to load: "
						<< fullPath;

					continue;
				}


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
		}
		else
		{
			ofLogWarning()
				<< "splitscreen.json contains no valid "
				<< "\"splitScreens\" array.";
		}
	}


	// ============================================================
	// LOAD IDLE VIDEOS
	// ============================================================

	ofJson idleJson;

	if (loadJsonSafely(
			"idle.json",
			idleJson))
	{
		if (idleJson.is_object() &&
			idleJson.contains("idleVideos") &&
			idleJson["idleVideos"].is_array())
		{
			for (const auto& entry :
				 idleJson["idleVideos"])
			{
				if (!entry.is_object())
				{
					continue;
				}


				const std::string file =
					entry.value(
						"file",
						std::string()
					);


				if (file.empty())
				{
					ofLogWarning()
						<< "Skipping idle entry with no file.";

					continue;
				}


				IdleClip clip;

				clip.id =
					readIdSafely(
						entry
					);

				clip.file =
					file;


				const std::string fullPath =
					"footage/" +
					clip.file;


				if (!clip.video.load(
						fullPath))
				{
					ofLogError()
						<< "Skipping idle video that failed "
						<< "to load: "
						<< fullPath;

					continue;
				}


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
		}
		else
		{
			ofLogWarning()
				<< "idle.json contains no valid "
				<< "\"idleVideos\" array.";
		}
	}


	// ============================================================
	// START INITIAL TOPIC
	// ============================================================

	if (!topics.empty())
	{
		selectRandomTopic();
	}
	else
	{
		currentTopic =
			nullptr;

		playingAnchor =
			false;

		ofLogError()
			<< "NO VALID TOPICS WERE LOADED. "
			<< "Application will remain safely idle/black "
			<< "instead of crashing.";
	}


	// ============================================================
	// MIDI
	// ============================================================

	try
	{
		midiIn.listInPorts();

		midiIn.openPort(
			0
		);

		midiIn.addListener(
			this
		);

		// Do not produce thousands/millions of console lines
		// during an exhibition.
		midiIn.setVerbose(
			false
		);
	}
	catch (...)
	{
		ofLogError()
			<< "Could not open MIDI input. "
			<< "Application will continue without direct MIDI.";
	}


	// ============================================================
	// IDLE TIMER
	// ============================================================

	lastInteractionTime =
		ofGetElapsedTimef();
}


//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void ChronologyManager::update()
{
	// ============================================================
	// ALL MIDI VIDEO ACTIONS ENTER THE MAIN THREAD HERE
	// ============================================================

	processPendingMidi();


	if (
		!currentTopic
	)
	{
		return;
	}


	// ============================================================
	// IDLE STATE
	// ============================================================

	if (
		isIdle
	)
	{
		// ========================================================
		// NO IDLE MEDIA
		// ========================================================

		if (
			idleClips.empty()
		)
		{
			currentIdleClipIndex =
				-1;


			return;
		}


		// ========================================================
		// RECOVER FROM INVALID CURRENT INDEX
		//
		// Never fall back to clip zero.
		//
		// Ask the shuffle bag for the next valid clip instead.
		// ========================================================

		if (
			currentIdleClipIndex <
				0 ||
			currentIdleClipIndex >=
				static_cast<int>(
					idleClips.size()
				)
		)
		{
			if (
				!playNextIdleClip()
			)
			{
				ofLogError()
					<< "Idle state has no playable idle clip.";
			}


			return;
		}


		auto& idleVideo =
			idleClips[
				currentIdleClipIndex
			]
			.video;


		// ========================================================
		// CURRENT PLAYER BECAME INVALID
		// ========================================================

		if (
			!idleVideo.isLoaded()
		)
		{
			ofLogWarning()
				<< "Current idle video became unavailable: "
				<< idleClips[
					   currentIdleClipIndex
				   ]
				   .file;


			currentIdleClipIndex =
				-1;


			playNextIdleClip();


			return;
		}


		// ========================================================
		// UPDATE CURRENT IDLE VIDEO
		// ========================================================

		idleVideo.update();


		const bool movieFinished =
			idleVideo
				.getIsMovieDone();


		const bool reachedEnd =
			idleVideo.getDuration() >
				0.0f &&
			idleVideo.getPosition() >=
				0.995f;


		// ========================================================
		// ADVANCE THROUGH SHUFFLE BAG
		// ========================================================

		if (
			movieFinished ||
			reachedEnd
		)
		{
			idleVideo.stop();


			if (
				!playNextIdleClip()
			)
			{
				currentIdleClipIndex =
					-1;


				ofLogError()
					<< "Idle playlist could not advance "
					<< "to another valid clip.";
			}
		}


		return;
	}


	// ============================================================
	// ANCHOR
	// ============================================================

	if (
		playingAnchor
	)
	{
		auto& anchorVideo =
			currentTopic
				->anchor
				.video;


		// Failed anchor file:
		//
		// never repeatedly ask AVFoundation to update an invalid
		// player forever.

		if (
			!anchorVideo.isLoaded()
		)
		{
			ofLogError()
				<< "Anchor video failed to load for topic: "
				<< currentTopic->name;


			playingAnchor =
				false;


			currentFootageIndex =
				0;


			randomizeFootageOrder();


			return;
		}


		anchorVideo.update();


		const bool anchorFinished =
			anchorVideo
				.getIsMovieDone();


		const bool reachedAnchorEnd =
			anchorVideo.getDuration() >
				0.0f &&
			anchorVideo.getPosition() >=
				0.995f;


		if (
			anchorFinished ||
			reachedAnchorEnd
		)
		{
			anchorVideo.stop();


			playingAnchor =
				false;


			currentFootageIndex =
				0;


			randomizeFootageOrder();
		}


		return;
	}


	// ============================================================
	// VALIDATE NORMAL FOOTAGE
	// ============================================================

	if (
		!hasValidCurrentFootage()
	)
	{
		if (
			currentTopic
				->footage
				.empty()
		)
		{
			const float timeSinceInteraction =
				ofGetElapsedTimef() -
				lastInteractionTime;


			if (
				timeSinceInteraction >=
				idleTimeout
			)
			{
				enterIdleState();
			}


			return;
		}


		currentFootageIndex =
			0;
	}


	auto& video =
		currentTopic
			->footage[
				currentFootageIndex
			]
			.video;


	// ============================================================
	// MANUAL LOOP
	// ============================================================

	if (
		isLooping
	)
	{
		const float duration =
			video.getDuration();


		if (
			std::isfinite(
				duration
			) &&
			duration >
				0.0f
		)
		{
			const float currentTime =
				ofClamp(
					video.getPosition(),
					0.0f,
					1.0f
				)
				*
				duration;


			// ====================================================
			// REACHED LOOP OUT-POINT
			// ====================================================

			if (
				currentTime >=
				loopEndTime
			)
			{
				++loopCount;


				// =================================================
				// TEN COMPLETE PASSES FINISHED
				//
				// Recover automatically so somebody cannot leave
				// the installation permanently stuck in a loop.
				// =================================================

				if (
					loopCount >=
					maxLoopCount
				)
				{
					ofLogNotice()
						<< "Manual loop completed "
						<< loopCount
						<< " passes. Returning to normal playback.";


					stopLooping();
				}

				// =================================================
				// ANOTHER PASS
				// =================================================

				else
				{
					const float normalizedLoopStart =
						ofClamp(
							loopStartTime /
								duration,
							0.0f,
							1.0f
						);


					video.setPosition(
						normalizedLoopStart
					);


					video.play();
				}
			}
		}

		else
		{
			stopLooping();
		}
	}


	// ============================================================
	// UPDATE MAIN FOOTAGE
	// ============================================================

	video.update();


	// ============================================================
	// UPDATE SPLIT SCREEN
	//
	// ChronologyManager owns this now.
	//
	// ofApp should NOT update this player separately.
	// ============================================================

	if (
		splitScreenMode
	)
	{
		if (
			hasValidSplitScreenClip()
		)
		{
			splitScreenClips[
				currentSplitIndex
			]
			.video
			.update();
		}
		else
		{
			toggleSplitScreen(
				false
			);
		}
	}


	// ============================================================
	// IDLE TIMER
	// ============================================================

	const float timeSinceInteraction =
		ofGetElapsedTimef() -
		lastInteractionTime;


	if (
		timeSinceInteraction >=
		idleTimeout
	)
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
	
	// ============================================================
	// PREPARE IDLE PLAYERS
	//
	// Stop everything first.
	//
	// IMPORTANT:
	// We do NOT clear idlePlayOrder here.
	//
	// If somebody interacts halfway through an idle cycle and the
	// installation later becomes idle again, it continues through
	// the remaining unseen clips.
	//
	// That preserves:
	//
	//     EVERY CLIP GETS A TURN BEFORE ANY REPEAT
	//
	// even across separate idle sessions.
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
	
	
	// ============================================================
	// ASK SHUFFLE BAG FOR NEXT CLIP
	// ============================================================
	
	currentIdleClipIndex =
	-1;
	
	
	if (
		!playNextIdleClip()
		)
	{
		ofLogError()
		<< "Could not enter idle playback because "
		<< "no valid idle clip could be selected.";
		
		
		isIdle =
		false;
		
		
		lastInteractionTime =
		ofGetElapsedTimef();
		
		
		return;
	}
	
	
	// ============================================================
	// LOG
	// ============================================================
	
	ofLogNotice()
	<< "========================================";
	
	
	ofLogNotice()
	<< "ENTERING IDLE STATE";
	
	
	ofLogNotice()
	<< "Starting shuffled idle playback: "
	<< idleClips[
				 currentIdleClipIndex
				 ]
		.file;
	
	
	ofLogNotice()
	<< "========================================";
	
}
//--------------------------------------------------------------
void ChronologyManager::rebuildIdlePlayOrder()
{
	// ============================================================
	// CLEAN OLD ORDER
	// ============================================================

	idlePlayOrder.clear();

	idlePlayPosition =
		0;


	// ============================================================
	// NOTHING TO PLAY
	// ============================================================

	if (
		idleClips.empty()
	)
	{
		currentIdleClipIndex =
			-1;


		return;
	}


	// ============================================================
	// BUILD ORDER FROM VALID / LOADED IDLE CLIPS ONLY
	//
	// We deliberately shuffle integer indices instead of moving
	// the ofVideoPlayer objects themselves.
	// ============================================================

	for (
		int i = 0;
		i <
			static_cast<int>(
				idleClips.size()
			);
		++i
	)
	{
		if (
			!idleClips[i]
				.video
				.isLoaded()
		)
		{
			ofLogWarning()
				<< "Idle clip is no longer loaded; "
				<< "excluding it from shuffle cycle: "
				<< idleClips[i].file;


			continue;
		}


		idlePlayOrder.push_back(
			i
		);
	}


	// ============================================================
	// NO VALID FILES SURVIVED
	// ============================================================

	if (
		idlePlayOrder.empty()
	)
	{
		currentIdleClipIndex =
			-1;


		ofLogError()
			<< "No valid idle clips are available.";


		return;
	}


	// ============================================================
	// RANDOMISE THE COMPLETE CYCLE
	// ============================================================

	std::random_device rd;


	std::mt19937 generator(
		rd()
	);


	std::shuffle(
		idlePlayOrder.begin(),
		idlePlayOrder.end(),
		generator
	);


	// ============================================================
	// PREVENT AN IMMEDIATE REPEAT ACROSS CYCLE BOUNDARIES
	//
	// Example:
	//
	// old cycle:
	//     2, 4, 1
	//
	// newly shuffled:
	//     1, 2, 4
	//
	// Without this guard clip 1 would play twice consecutively.
	//
	// This only matters when there is more than one clip.
	// ============================================================

	if (
		idlePlayOrder.size() >
			1 &&
		lastIdleClipIndex >=
			0 &&
		idlePlayOrder.front() ==
			lastIdleClipIndex
	)
	{
		std::swap(
			idlePlayOrder[0],
			idlePlayOrder[1]
		);
	}


	// ============================================================
	// START AT BEGINNING OF NEW BAG
	// ============================================================

	idlePlayPosition =
		0;


	ofLogNotice()
		<< "Built new idle shuffle cycle with "
		<< idlePlayOrder.size()
		<< " clips.";
}

//--------------------------------------------------------------
bool ChronologyManager::playNextIdleClip()
{
	// ============================================================
	// NOTHING AVAILABLE
	// ============================================================

	if (
		idleClips.empty()
	)
	{
		currentIdleClipIndex =
			-1;


		return false;
	}


	// ============================================================
	// BUILD / REBUILD SHUFFLE BAG WHEN NECESSARY
	//
	// Rebuild if:
	//
	// 1. no order exists yet
	// 2. the underlying clip collection changed
	// 3. every item in the current cycle has been consumed
	// ============================================================

	if (
		idlePlayOrder.empty() ||
		idlePlayPosition >=
			idlePlayOrder.size()
	)
	{
		rebuildIdlePlayOrder();
	}


	// ============================================================
	// STILL NOTHING VALID
	// ============================================================

	if (
		idlePlayOrder.empty()
	)
	{
		currentIdleClipIndex =
			-1;


		return false;
	}


	// ============================================================
	// DEFENSIVE SEARCH
	//
	// Normally the first item will be valid because
	// rebuildIdlePlayOrder() already validates them.
	//
	// But exhibition code should survive unexpected state changes,
	// so never blindly trust an index.
	// ============================================================

	while (
		idlePlayPosition <
		idlePlayOrder.size()
	)
	{
		const int candidateIndex =
			idlePlayOrder[
				idlePlayPosition
			];


		++idlePlayPosition;


		if (
			candidateIndex <
				0 ||
			candidateIndex >=
				static_cast<int>(
					idleClips.size()
				)
		)
		{
			ofLogWarning()
				<< "Ignoring invalid idle shuffle index: "
				<< candidateIndex;


			continue;
		}


		if (
			!idleClips[
				candidateIndex
			]
			.video
			.isLoaded()
		)
		{
			ofLogWarning()
				<< "Skipping unloaded idle clip: "
				<< idleClips[
					   candidateIndex
				   ]
				   .file;


			continue;
		}


		// ========================================================
		// VALID NEXT CLIP
		// ========================================================

		currentIdleClipIndex =
			candidateIndex;


		lastIdleClipIndex =
			candidateIndex;


		playIdleClip();


		return true;
	}


	// ============================================================
	// THE REMAINDER OF THIS BAG BECAME INVALID
	//
	// Rebuild once from the currently valid clips.
	//
	// Do NOT recurse indefinitely.
	// ============================================================

	rebuildIdlePlayOrder();


	if (
		idlePlayOrder.empty()
	)
	{
		currentIdleClipIndex =
			-1;


		return false;
	}


	const int candidateIndex =
		idlePlayOrder[
			idlePlayPosition
		];


	++idlePlayPosition;


	if (
		candidateIndex <
			0 ||
		candidateIndex >=
			static_cast<int>(
				idleClips.size()
			) ||
		!idleClips[
			candidateIndex
		]
		.video
		.isLoaded()
	)
	{
		currentIdleClipIndex =
			-1;


		ofLogError()
			<< "Could not select a valid idle clip "
			<< "after rebuilding idle shuffle order.";


		return false;
	}


	currentIdleClipIndex =
		candidateIndex;


	lastIdleClipIndex =
		candidateIndex;


	playIdleClip();


	return true;
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void ChronologyManager::playIdleClip()
{
	// ============================================================
	// VALIDATE INDEX
	// ============================================================

	if (
		currentIdleClipIndex <
			0 ||
		currentIdleClipIndex >=
			static_cast<int>(
				idleClips.size()
			)
	)
	{
		ofLogWarning()
			<< "Cannot play idle clip: invalid index.";


		return;
	}


	auto& idleClip =
		idleClips[
			currentIdleClipIndex
		];


	// ============================================================
	// VALIDATE PLAYER
	// ============================================================

	if (
		!idleClip.video.isLoaded()
	)
	{
		ofLogWarning()
			<< "Cannot play unloaded idle clip: "
			<< idleClip.file;


		return;
	}


	// ============================================================
	// CLEAN RESTART
	// ============================================================

	idleClip.video.stop();


	idleClip.video.setPosition(
		0.0f
	);


	// ============================================================
	// INDIVIDUAL IDLE FILES NEVER LOOP THEMSELVES
	//
	// ChronologyManager advances through the shuffled bag.
	// ============================================================

	idleClip.video.setLoopState(
		OF_LOOP_NONE
	);


	idleClip.video.setVolume(
		1.0f
	);


	idleClip.video.play();


	// ============================================================
	// DEBUG INFORMATION
	// ============================================================

	ofLogNotice()
		<< "Playing idle clip "
		<< idlePlayPosition
		<< " / "
		<< idlePlayOrder.size()
		<< " in current shuffle cycle: "
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

//--------------------------------------------------------------
void ChronologyManager::selectRandomTopic()
{
	if (
		topics.empty()
	)
	{
		ofLogError()
			<< "Cannot change topic: no topics loaded.";

		currentTopic =
			nullptr;

		playingAnchor =
			false;

		return;
	}


	// ============================================================
	// RATE LIMIT
	//
	// Still responsive, but impossible to tear down and recreate
	// several AVAssetReaders in the same fraction of a second.
	// ============================================================

	const float now =
		ofGetElapsedTimef();


	if (
		currentTopic &&
		now -
			lastTopicSwitchTime <
			topicSwitchCooldown
	)
	{
		return;
	}


	lastTopicSwitchTime =
		now;


	lastInteractionTime =
		now;


	// ============================================================
	// IF TOPIC CHANGE HAPPENS DURING IDLE:
	//
	// leave idle DIRECTLY into the new anchor.
	//
	// Do not wake old footage first.
	// ============================================================

	if (
		isIdle
	)
	{
		if (
			currentIdleClipIndex >=
				0 &&
			currentIdleClipIndex <
				static_cast<int>(
					idleClips.size()
				)
		)
		{
			idleClips[
				currentIdleClipIndex
			]
			.video
			.stop();
		}


		currentIdleClipIndex =
			-1;


		isIdle =
			false;
	}


	// ============================================================
	// RESET SPLIT SCREEN
	// ============================================================

	splitScreenMode =
		false;


	isSplitScreenActive =
		false;


	splitScreenAmount =
		0.0f;


	needReshuffleSplitScreen =
		true;


	for (
		auto& clip :
		splitScreenClips
	)
	{
		clip.video.setPaused(
			true
		);
	}


	// ============================================================
	// RESET LOOP
	// ============================================================

	isLooping =
		false;

	loopCount =
		0;

	loopStartTime =
		0.0f;

	loopEndTime =
		0.0f;


	// ============================================================
	// STOP CURRENT TOPIC
	// ============================================================

	int previousTopicIndex =
		-1;


	if (
		currentTopic
	)
	{
		for (
			int i = 0;
			i <
				static_cast<int>(
					topics.size()
				);
			++i
		)
		{
			if (
				&topics[i] ==
				currentTopic
			)
			{
				previousTopicIndex =
					i;

				break;
			}
		}


		currentTopic
			->anchor
			.video
			.stop();


		for (
			auto& clip :
			currentTopic
				->footage
		)
		{
			clip.video.stop();
		}
	}


	// ============================================================
	// PICK A DIFFERENT TOPIC
	//
	// No do/while.
	// No infinite loop when there is only one topic.
	// ============================================================

	const int topicCount =
		static_cast<int>(
			topics.size()
		);


	int randomIndex =
		0;


	if (
		topicCount ==
		1
	)
	{
		randomIndex =
			0;
	}

	else if (
		previousTopicIndex >=
		0
	)
	{
		int offset =
			1 +
			static_cast<int>(
				ofRandom(
					static_cast<float>(
						topicCount -
						1
					)
				)
			);


		offset =
			std::max(
				1,
				std::min(
					topicCount - 1,
					offset
				)
			);


		randomIndex =
			(
				previousTopicIndex +
				offset
			)
			%
			topicCount;
	}

	else
	{
		randomIndex =
			static_cast<int>(
				ofRandom(
					static_cast<float>(
						topicCount
					)
				)
			);


		randomIndex =
			std::max(
				0,
				std::min(
					topicCount - 1,
					randomIndex
				)
			);
	}


	// ============================================================
	// START NEW TOPIC
	// ============================================================

	currentTopic =
		&topics[
			randomIndex
		];


	currentFootageIndex =
		0;


	playingAnchor =
		true;


	auto& anchorVideo =
		currentTopic
			->anchor
			.video;


	anchorVideo.stop();


	anchorVideo.setLoopState(
		OF_LOOP_NONE
	);


	anchorVideo.setPosition(
		0.0f
	);


	if (
		anchorVideo.isLoaded()
	)
	{
		anchorVideo.play();


		ofLogNotice()
			<< "Switched to topic: "
			<< currentTopic->name;
	}

	else
	{
		ofLogError()
			<< "Anchor unavailable for topic: "
			<< currentTopic->name;


		playingAnchor =
			false;


		randomizeFootageOrder();
	}
}

//--------------------------------------------------------------
void ChronologyManager::randomizeFootageOrder()
{
	if (
		!currentTopic
	)
	{
		return;
	}


	if (
		currentTopic
			->footage
			.empty()
	)
	{
		currentFootageIndex =
			0;


		ofLogWarning()
			<< "Topic has no normal footage: "
			<< currentTopic->name;


		return;
	}


	// Stop everything before moving Clip objects around.

	for (
		auto& clip :
		currentTopic
			->footage
	)
	{
		clip.video.stop();
	}


	std::random_device rd;

	std::mt19937 generator(
		rd()
	);


	std::shuffle(
		currentTopic
			->footage
			.begin(),

		currentTopic
			->footage
			.end(),

		generator
	);


	currentFootageIndex =
		0;


	playCurrentFootage();
}


//--------------------------------------------------------------
void ChronologyManager::playCurrentFootage()
{
	if (
		!hasValidCurrentFootage()
	)
	{
		return;
	}


	for (
		int i = 0;
		i <
			static_cast<int>(
				currentTopic
					->footage
					.size()
			);
		++i
	)
	{
		if (
			i !=
			currentFootageIndex
		)
		{
			currentTopic
				->footage[i]
				.video
				.stop();
		}
	}


	auto& video =
		currentTopic
			->footage[
				currentFootageIndex
			]
			.video;


	if (
		!video.isLoaded()
	)
	{
		ofLogError()
			<< "Normal footage failed to load: "
			<< currentTopic
				->footage[
					currentFootageIndex
				]
				.videoPath;

		return;
	}


	video.stop();


	video.setPosition(
		0.0f
	);


	video.setLoopState(
		OF_LOOP_NORMAL
	);


	video.play();


	isLooping =
		false;


	loopCount =
		0;


	ofLogNotice()
		<< "Playing footage: "
		<< currentTopic
			->footage[
				currentFootageIndex
			]
			.videoPath;
}


//--------------------------------------------------------------
void ChronologyManager::advanceFootage(
	int direction
)
{
	if (
		!currentTopic ||
		playingAnchor ||
		currentTopic
			->footage
			.empty()
	)
	{
		return;
	}


	registerInteraction();


	const int count =
		static_cast<int>(
			currentTopic
				->footage
				.size()
		);


	if (
		!hasValidCurrentFootage()
	)
	{
		currentFootageIndex =
			0;
	}

	else if (
		direction >
		0
	)
	{
		currentFootageIndex =
			(
				currentFootageIndex +
				1
			)
			%
			count;
	}

	else
	{
		currentFootageIndex =
			(
				currentFootageIndex -
				1 +
				count
			)
			%
			count;
	}


	playCurrentFootage();
}



// Starts a short manual loop near the current playback position (for the right jogwheel)

//--------------------------------------------------------------
//--------------------------------------------------------------
void ChronologyManager::startLooping()
{
	// ========================================================
	// ONLY NORMAL FOOTAGE MAY LOOP
	// ========================================================

	if (
		playingAnchor ||
		!hasValidCurrentFootage()
	)
	{
		stopLooping();

		return;
	}


	// ========================================================
	// DON'T RE-CAPTURE A LOOP THAT IS ALREADY ACTIVE
	//
	// Clockwise 360 enters the loop.
	// Anti-clockwise 360 leaves it.
	// ========================================================

	if (
		isLooping
	)
	{
		return;
	}


	auto& video =
		currentTopic
			->footage[
				currentFootageIndex
			]
			.video;


	// ========================================================
	// PLAYER MUST BE VALID
	// ========================================================

	if (
		!video.isLoaded()
	)
	{
		stopLooping();


		ofLogWarning()
			<< "Manual loop ignored because current video "
			<< "is not loaded.";


		return;
	}


	const float duration =
		video.getDuration();


	// ========================================================
	// VALID DURATION REQUIRED
	// ========================================================

	if (
		!std::isfinite(
			duration
		) ||
		duration <=
			0.05f
	)
	{
		stopLooping();


		ofLogWarning()
			<< "Manual loop ignored because video duration "
			<< "is invalid.";


		return;
	}


	// ========================================================
	// CURRENT PLAYBACK POSITION
	// ========================================================

	const float normalizedPosition =
		ofClamp(
			video.getPosition(),
			0.0f,
			1.0f
		);


	const float currentTime =
		normalizedPosition *
		duration;


	// ========================================================
	// SIX-SECOND LOCAL LOOP
	//
	// Do NOT return to the beginning of the clip.
	//
	// The moment at which the visitor completes the clockwise
	// platter turn becomes the OUT point.
	//
	// We capture approximately the six seconds immediately
	// preceding that moment.
	//
	// Example:
	//
	// visitor reaches 38 seconds
	//
	// loop becomes:
	//
	//      32s -------- 38s
	//
	// rather than:
	//
	//       0s -------- 6s
	// ========================================================

	loopEndTime =
		ofClamp(
			currentTime,
			0.0f,
			duration
		);


	loopStartTime =
		std::max(
			0.0f,
			loopEndTime -
				loopDuration
		);


	// ========================================================
	// REQUIRE A REAL LOOP REGION
	//
	// Very beginning of an extremely short video may not have
	// enough material yet.
	// ========================================================

	if (
		loopEndTime -
			loopStartTime <
		0.05f
	)
	{
		stopLooping();


		ofLogWarning()
			<< "Manual loop ignored because available loop "
			<< "window is too short.";


		return;
	}


	// ========================================================
	// START FRESH
	// ========================================================

	loopCount =
		0;


	isLooping =
		true;


	// ========================================================
	// WE ARE MANUALLY CONTROLLING THE LOOP
	//
	// Do not allow openFrameworks to wrap the entire movie
	// from its end back to frame zero while this is active.
	// ========================================================

	video.setLoopState(
		OF_LOOP_NONE
	);


	// ========================================================
	// IMMEDIATELY RETURN TO THE START OF THE CAPTURED SECTION
	//
	// This means the first loop pass is a genuine full pass.
	// It also means maxLoopCount = 10 really gives us ten
	// repetitions rather than accidentally counting the
	// activation itself as pass number one.
	// ========================================================

	const float normalizedLoopStart =
		ofClamp(
			loopStartTime /
				duration,
			0.0f,
			1.0f
		);


	video.setPosition(
		normalizedLoopStart
	);


	video.play();


	ofLogNotice()
		<< "Started local manual loop: "
		<< loopStartTime
		<< "s -> "
		<< loopEndTime
		<< "s | target passes: "
		<< maxLoopCount;
}

//--------------------------------------------------------------
void ChronologyManager::stopLooping()
{
	// ========================================================
	// CLEAR MANUAL LOOP STATE
	// ========================================================

	isLooping =
		false;


	loopStartTime =
		0.0f;


	loopEndTime =
		0.0f;


	loopCount =
		0;


	// ========================================================
	// RESTORE ORDINARY VIDEO PLAYBACK
	// ========================================================

	if (
		hasValidCurrentFootage()
	)
	{
		auto& video =
			currentTopic
				->footage[
					currentFootageIndex
				]
				.video;


		if (
			video.isLoaded()
		)
		{
			video.setLoopState(
				OF_LOOP_NORMAL
			);


			if (
				!video.isPlaying()
			)
			{
				video.play();
			}
		}
	}
}

void ChronologyManager::newMidiMessage(
	ofxMidiMessage& message
)
{
	if (
		shuttingDown.load()
	)
	{
		return;
	}


	// ============================================================
	// NORMALISE MIDI BUTTON PRESS / RELEASE
	//
	// MIDI allows:
	//
	// NOTE_OFF
	//
	// OR
	//
	// NOTE_ON with velocity 0
	//
	// to represent a release.
	//
	// Your old topic code ignored velocity, so a release could
	// become another topic change.
	// ============================================================

	const bool notePressed =
		message.status ==
			MIDI_NOTE_ON &&
		message.velocity >
			0;


	const bool noteReleased =
		message.status ==
			MIDI_NOTE_OFF ||
		(
			message.status ==
				MIDI_NOTE_ON &&
			message.velocity ==
				0
		);


	// ============================================================
	// CHANGE TOPIC
	//
	// Allowed EVEN DURING ANCHOR.
	//
	// But this thread ONLY requests it.
	// ============================================================

	if (
		(
			message.pitch == 60 ||
			message.pitch == 51
		)
		&&
		(
			message.status ==
				MIDI_NOTE_ON ||
			message.status ==
				MIDI_NOTE_OFF
		)
	)
	{
		if (
			notePressed
		)
		{
			pendingTopicChange.store(
				true
			);


			pendingInteraction.store(
				true
			);
		}


		// RELEASE DOES NOTHING.
		//
		// This is deliberate.
		//
		// It prevents:
		//
		// press   -> topic B
		// release -> topic A/C
		//
		// which is very likely the bounce you were seeing.

		return;
	}


	// ============================================================
	// SPLIT-SCREEN ADVANCE — NOTE 66
	// ============================================================

	if (
		message.pitch ==
			66 &&
		(
			message.status ==
				MIDI_NOTE_ON ||
			message.status ==
				MIDI_NOTE_OFF
		)
	)
	{
		if (
			notePressed
		)
		{
			const bool wasAlreadyDown =
				note66Down.exchange(
					true
				);


			if (
				!wasAlreadyDown
			)
			{
				pendingSplitAdvance.store(
					true
				);


				pendingInteraction.store(
					true
				);
			}
		}


		if (
			noteReleased
		)
		{
			note66Down.store(
				false
			);
		}


		return;
	}


	// ============================================================
	// JOGWHEEL CLIP CHANGE — CC25
	// ============================================================

	if (
		message.status ==
			MIDI_CONTROL_CHANGE &&
		message.control ==
			25
	)
	{
		static bool jogwheelSpinning =
			false;


		static bool jogwheelReversed =
			false;


		static unsigned long
			lastMovementTime =
				0;


		const int movementThresholdMin =
			5;


		const int movementThresholdMax =
			10;


		const int reverseMovementThresholdMin =
			110;


		const int reverseMovementThresholdMax =
			124;


		if (
			message.value >=
				movementThresholdMin &&
			message.value <=
				movementThresholdMax
		)
		{
			jogwheelSpinning =
				true;


			jogwheelReversed =
				false;


			lastMovementTime =
				ofGetElapsedTimeMillis();


			pendingInteraction.store(
				true
			);
		}

		else if (
			message.value >=
				reverseMovementThresholdMin &&
			message.value <=
				reverseMovementThresholdMax
		)
		{
			jogwheelReversed =
				true;


			jogwheelSpinning =
				false;


			lastMovementTime =
				ofGetElapsedTimeMillis();


			pendingInteraction.store(
				true
			);
		}

		else if (
			jogwheelSpinning &&
			message.value ==
				1
		)
		{
			const unsigned long now =
				ofGetElapsedTimeMillis();


			if (
				now -
					lastMovementTime >
				100
			)
			{
				jogwheelSpinning =
					false;


				// Coalesce multiple requests.
				//
				// Never queue 40 video transitions because somebody
				// went mad on the jogwheel.

				pendingFootageDirection.store(
					1
				);


				pendingInteraction.store(
					true
				);
			}
		}

		else if (
			jogwheelReversed &&
			message.value ==
				127
		)
		{
			const unsigned long now =
				ofGetElapsedTimeMillis();


			if (
				now -
					lastMovementTime >
				100
			)
			{
				jogwheelReversed =
					false;


				pendingFootageDirection.store(
					-1
				);


				pendingInteraction.store(
					true
				);
			}
		}


		return;
	}


	// ============================================================
	// MANUAL LOOP PLATTER — CC24
	//
	// CLOCKWISE FULL TURN:
	//     enter six-second local loop
	//
	// ANTI-CLOCKWISE FULL TURN:
	//     leave local loop
	//
	// Mixtrack Pro jog MIDI is relative:
	//
	//     1...15
	//         clockwise movement
	//
	//     112...127
	//         anti-clockwise movement
	//
	// Counter-clockwise values are converted using:
	//
	//     value - 128
	//
	// We accumulate approximately 600 movement ticks, which
	// corresponds to roughly one physical platter revolution.
	// ============================================================

	if (
		message.status ==
			MIDI_CONTROL_CHANGE &&
		message.control ==
			24
	)
	{
		// ========================================================
		// PERSISTENT GESTURE STATE
		//
		// These live only inside this MIDI handler.
		// ========================================================

		static int accumulatedJogTicks =
			0;


		static unsigned long lastJogMovementTime =
			0;


		// ========================================================
		// APPROXIMATELY ONE PHYSICAL REVOLUTION
		//
		// If one real turn on your particular controller feels a
		// little early/late, THIS is the one number we calibrate.
		// ========================================================

		constexpr int ticksPerFullTurn =
			600;


		// ========================================================
		// ABANDON HALF-FINISHED GESTURES
		//
		// Don't let someone turn 1/4 of the wheel, walk away,
		// then accidentally complete the "same" gesture later.
		// ========================================================

		constexpr unsigned long gestureResetTimeMs =
			1200;


		const unsigned long now =
			ofGetElapsedTimeMillis();


		// ========================================================
		// DECODE RELATIVE JOG MOVEMENT
		// ========================================================

		int movementDelta =
			0;


		// Clockwise.
		if (
			message.value >=
				1 &&
			message.value <=
				15
		)
		{
			movementDelta =
				message.value;
		}

		// Anti-clockwise.
		else if (
			message.value >=
				112 &&
			message.value <=
				127
		)
		{
			movementDelta =
				message.value -
				128;
		}

		// Ignore unrelated/noisy values.
		else
		{
			return;
		}


		// ========================================================
		// THIS IS AN INTERACTION EVEN BEFORE 360° IS COMPLETE
		//
		// Important for your installation idle timer.
		// ========================================================

		pendingInteraction.store(
			true
		);


		// ========================================================
		// RESET AN ABANDONED PARTIAL TURN
		// ========================================================

		if (
			lastJogMovementTime >
				0 &&
			now -
				lastJogMovementTime >
			gestureResetTimeMs
		)
		{
			accumulatedJogTicks =
				0;
		}


		lastJogMovementTime =
			now;


		// ========================================================
		// ACCUMULATE PHYSICAL MOVEMENT
		//
		// Clockwise adds.
		// Anti-clockwise subtracts.
		//
		// Rocking backwards and forwards therefore cancels itself
		// rather than accidentally becoming a completed turn.
		// ========================================================

		accumulatedJogTicks +=
			movementDelta;


		// ========================================================
		// FULL CLOCKWISE TURN
		// ========================================================

		if (
			accumulatedJogTicks >=
				ticksPerFullTurn
		)
		{
			accumulatedJogTicks =
				0;


			pendingLoopCommand.store(
				1
			);


			ofLogNotice()
				<< "CC24 full clockwise turn -> local loop ON";


			return;
		}


		// ========================================================
		// FULL ANTI-CLOCKWISE TURN
		// ========================================================

		if (
			accumulatedJogTicks <=
				-ticksPerFullTurn
		)
		{
			accumulatedJogTicks =
				0;


			pendingLoopCommand.store(
				-1
			);


			ofLogNotice()
				<< "CC24 full anti-clockwise turn -> local loop OFF";


			return;
		}


		return;
	}

	// ============================================================
	// SPLIT-SCREEN / DOUBLE-EXPOSURE SLIDER — CC10
	// ============================================================

	if (
		message.status ==
			MIDI_CONTROL_CHANGE &&
		message.control ==
			10
	)
	{
		pendingSplitAmountRaw.store(
			std::max(
				0,
				std::min(
					127,
					message.value
				)
			)
		);


		pendingInteraction.store(
			true
		);


		return;
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





//--------------------------------------------------------------
void ChronologyManager::toggleSplitScreen(
	bool enable
)
{
	if (
		playingAnchor ||
		isIdle
	)
	{
		enable =
			false;
	}


	if (
		enable &&
		splitScreenClips.empty()
	)
	{
		enable =
			false;
	}


	if (
		enable
	)
	{
		if (
			needReshuffleSplitScreen
		)
		{
			randomizeSplitScreenOrder();
		}


		if (
			!hasValidSplitScreenClip()
		)
		{
			currentSplitIndex =
				0;
		}


		if (
			!hasValidSplitScreenClip()
		)
		{
			splitScreenMode =
				false;

			isSplitScreenActive =
				false;

			return;
		}


		splitScreenMode =
			true;


		isSplitScreenActive =
			true;


		for (
			auto& clip :
			splitScreenClips
		)
		{
			clip.video.setLoopState(
				OF_LOOP_NORMAL
			);
		}


		auto& splitVideo =
			splitScreenClips[
				currentSplitIndex
			]
			.video;


		if (
			splitVideo.isLoaded() &&
			!splitVideo.isPlaying()
		)
		{
			splitVideo.setPosition(
				0.0f
			);


			splitVideo.play();
		}
	}

	else
	{
		splitScreenMode =
			false;


		isSplitScreenActive =
			false;


		for (
			auto& clip :
			splitScreenClips
		)
		{
			clip.video.setPaused(
				true
			);
		}


		needReshuffleSplitScreen =
			true;
	}
}


//--------------------------------------------------------------
void ChronologyManager::randomizeSplitScreenOrder()
{
	if (
		splitScreenClips.empty()
	)
	{
		currentSplitIndex =
			0;


		needReshuffleSplitScreen =
			true;


		return;
	}


	// Stop before shuffling live player wrappers.

	for (
		auto& clip :
		splitScreenClips
	)
	{
		clip.video.stop();
	}


	std::random_device rd;

	std::mt19937 generator(
		rd()
	);


	std::shuffle(
		splitScreenClips.begin(),
		splitScreenClips.end(),
		generator
	);


	currentSplitIndex =
		0;


	needReshuffleSplitScreen =
		false;
}


//--------------------------------------------------------------
void ChronologyManager::advanceSplitScreen()
{
	registerInteraction();


	if (
		!splitScreenMode ||
		!hasValidSplitScreenClip()
	)
	{
		return;
	}


	splitScreenClips[
		currentSplitIndex
	]
	.video
	.stop();


	if (
		currentSplitIndex +
			1 >=
		static_cast<int>(
			splitScreenClips.size()
		)
	)
	{
		randomizeSplitScreenOrder();
	}

	else
	{
		currentSplitIndex++;
	}


	if (
		!hasValidSplitScreenClip()
	)
	{
		toggleSplitScreen(
			false
		);

		return;
	}


	auto& nextVideo =
		splitScreenClips[
			currentSplitIndex
		]
		.video;


	if (
		!nextVideo.isLoaded()
	)
	{
		ofLogError()
			<< "Split-screen video is not loaded.";

		return;
	}


	nextVideo.setPosition(
		0.0f
	);


	nextVideo.setLoopState(
		OF_LOOP_NORMAL
	);


	nextVideo.play();
}


//--------------------------------------------------------------
ofVideoPlayer*
ChronologyManager::getCurrentSplitScreenVideo()
{
	if (
		!splitScreenMode ||
		!hasValidSplitScreenClip()
	)
	{
		return nullptr;
	}


	return
		&splitScreenClips[
			currentSplitIndex
		]
		.video;
}




//--------------------------------------------------------------
void ChronologyManager::processPendingMidi()
{
	if (
		shuttingDown.load()
	)
	{
		return;
	}


	// ============================================================
	// TOPIC CHANGE HAS PRIORITY
	//
	// If somebody changes topic, do NOT also process a jogwheel,
	// loop toggle and split-screen change in the same frame.
	//
	// That would immediately churn several AVAssetReaders.
	// ============================================================

	if (
		pendingTopicChange.exchange(
			false
		)
	)
	{
		pendingInteraction.store(
			false
		);


		pendingFootageDirection.store(
			0
		);


		pendingLoopCommand.store(
			0
		);


		pendingSplitAdvance.store(
			false
		);


		pendingSplitAmountRaw.store(
			-1
		);


		lastInteractionTime =
			ofGetElapsedTimef();


		selectRandomTopic();


		return;
	}


	// ============================================================
	// GENERAL INTERACTION
	//
	// registerInteraction() can exit idle state and manipulate
	// video, so it MUST happen here rather than MIDI callback.
	// ============================================================

	if (
		pendingInteraction.exchange(
			false
		)
	)
	{
		registerInteraction();
	}


	// ============================================================
	// SPLIT-SCREEN AMOUNT
	// ============================================================

	const int splitRaw =
		pendingSplitAmountRaw.exchange(
			-1
		);


	if (
		splitRaw >=
		0
	)
	{
		splitScreenAmount =
			1.0f -
			ofClamp(
				static_cast<float>(
					splitRaw
				)
				/
				127.0f,

				0.0f,
				1.0f
			);


		bool shouldBeActive =
			splitScreenAmount >
			0.01f;


		if (
			playingAnchor ||
			isIdle
		)
		{
			shouldBeActive =
				false;
		}


		if (
			splitScreenMode !=
			shouldBeActive
		)
		{
			toggleSplitScreen(
				shouldBeActive
			);
		}
	}


	// ============================================================
	// FOOTAGE CHANGE
	// ============================================================

	const int footageDirection =
		pendingFootageDirection.exchange(
			0
		);


	if (
		footageDirection !=
		0
	)
	{
		advanceFootage(
			footageDirection
		);
	}


	// ============================================================
	// MANUAL LOOP
	// ============================================================

	const int loopCommand =
		pendingLoopCommand.exchange(
			0
		);


	if (
		loopCommand >
		0
	)
	{
		startLooping();
	}

	else if (
		loopCommand <
		0
	)
	{
		stopLooping();
	}


	// ============================================================
	// SPLIT ADVANCE
	// ============================================================

	if (
		pendingSplitAdvance.exchange(
			false
		)
	)
	{
		advanceSplitScreen();
	}
}

//--------------------------------------------------------------
bool ChronologyManager::hasValidCurrentFootage()
	const
{
	if (
		!currentTopic
	)
	{
		return false;
	}


	if (
		currentTopic
			->footage
			.empty()
	)
	{
		return false;
	}


	return
		currentFootageIndex >=
			0 &&
		currentFootageIndex <
			static_cast<int>(
				currentTopic
					->footage
					.size()
			);
}


//--------------------------------------------------------------
bool ChronologyManager::hasValidSplitScreenClip()
	const
{
	if (
		splitScreenClips.empty()
	)
	{
		return false;
	}


	return
		currentSplitIndex >=
			0 &&
		currentSplitIndex <
			static_cast<int>(
				splitScreenClips.size()
			);
}

//--------------------------------------------------------------
void ChronologyManager::shutdown()
{
	if (
		shuttingDown.exchange(
			true
		)
	)
	{
		return;
	}


	// ============================================================
	// FIRST:
	//
	// disconnect the external callback.
	// ============================================================

	midiIn.removeListener(
		this
	);


	midiIn.closePort();


	// ============================================================
	// THEN:
	//
	// safely stop AV players on main thread.
	// ============================================================

	for (
		auto& topic :
		topics
	)
	{
		topic.anchor.video.stop();


		for (
			auto& clip :
			topic.footage
		)
		{
			clip.video.stop();
		}
	}


	for (
		auto& clip :
		splitScreenClips
	)
	{
		clip.video.stop();
	}


	for (
		auto& clip :
		idleClips
	)
	{
		clip.video.stop();
	}


	pendingInteraction.store(
		false
	);


	pendingTopicChange.store(
		false
	);


	pendingFootageDirection.store(
		0
	);


	pendingLoopCommand.store(
		0
	);


	pendingSplitAmountRaw.store(
		-1
	);


	pendingSplitAdvance.store(
		false
	);
}
