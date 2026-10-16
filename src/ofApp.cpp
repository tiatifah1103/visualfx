#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
	ofSetFullscreen(true);

	chronologyManager.setup();
	visualState.setup();

	motionBlur.setup(0.84f, 40.0f);

	stepPrinting.setup(2);
	stepPrinting.setMixAmount(0.0f);
	stepPrinting.setFeedbackAmount(0.0f);

	fisheye.setup(1.5f);

	oscReceiver.setup(9000);
	ofLogNotice() << "Listening for OSC on port 9000";

	auto allocateAndClear =
		[this](ofFbo& fbo)
		{
			fbo.allocate(standardWidth,
						 standardHeight,
						 GL_RGBA);

			fbo.begin();
			ofClear(0, 0, 0, 255);
			fbo.end();
		};

	allocateAndClear(videoFbo);
	allocateAndClear(intermediateFbo);
	allocateAndClear(processedFbo);

	allocateAndClear(historySourceFbo);
	allocateAndClear(historyCompositeFbo);

	allocateAndClear(routeAFbo);
	allocateAndClear(routeBFbo);
	allocateAndClear(mutationFbo);
	allocateAndClear(preWarpFbo);
	allocateAndClear(preWarpTemporalFbo);

	allocateAndClear(interactionMemoryFbo);
	allocateAndClear(interactionScratchFbo);

	interactionMemoryReady = false;
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::update()
{
	// ============================================================
	// CHRONOLOGY
	//
	// This also processes queued MIDI work on the main thread.
	// ============================================================

	chronologyManager.update();


	// ============================================================
	// OSC
	// ============================================================

	constexpr int maxOscMessagesPerFrame =
		256;


	int oscMessagesProcessed =
		0;


	while (
		oscReceiver.hasWaitingMessages() &&
		oscMessagesProcessed <
			maxOscMessagesPerFrame
	)
	{
		++oscMessagesProcessed;


		ofxOscMessage m;

		oscReceiver.getNextMessage(
			m
		);


		const std::string address =
			m.getAddress();


		// ========================================================
		// INTERACTION REQUIRES NO VALUE
		// ========================================================

		if (
			address ==
			"/interaction"
		)
		{
			chronologyManager
				.registerInteraction();

			continue;
		}


		// ========================================================
		// EVERYTHING ELSE BELOW REQUIRES ARGUMENT ZERO
		// ========================================================

		if (
			m.getNumArgs() <
			1
		)
		{
			ofLogWarning()
				<< "Ignoring OSC message with no argument: "
				<< address;

			continue;
		}


		// ========================================================
		// SAFE ARGUMENT HELPERS
		// ========================================================

		auto readFloat =
			[&m](float& value) -> bool
			{
				try
				{
					value =
						m.getArgAsFloat(
							0
						);


					if (!std::isfinite(
							value))
					{
						return false;
					}


					return true;
				}
				catch (...)
				{
					return false;
				}
			};


		auto readInt =
			[&m](int& value) -> bool
			{
				try
				{
					value =
						m.getArgAsInt(
							0
						);

					return true;
				}
				catch (...)
				{
					return false;
				}
			};


		// ========================================================
		// REVERB
		// ========================================================

		if (
			address ==
			"/reverb/roomSize"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setReverbRoom(
						value
					);
			}
		}

		else if (
			address ==
			"/reverb/wetLevel"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setReverbWet(
						value
					);
			}
		}

		else if (
			address ==
			"/reverb/damping"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setReverbDamping(
						value
					);
			}
		}

		else if (
			address ==
			"/reverb/width"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setReverbWidth(
						value
					);
			}
		}


		// ========================================================
		// DELAY
		// ========================================================

		else if (
			address ==
			"/delay/delayTime"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setDelayTime(
						value
					);
			}
		}

		else if (
			address ==
			"/delay/feedbackValue"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setDelayFeedback(
						value
					);
			}
		}

		else if (
			address ==
			"/delay/mixValue"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setDubSend(
						value
					);
			}
		}


		// ========================================================
		// EQ
		// ========================================================

		else if (
			address ==
			"/eq/bass"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setBass(
						value
					);
			}
		}

		else if (
			address ==
			"/eq/mids"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setMids(
						value
					);
			}
		}

		else if (
			address ==
			"/eq/tops"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setTops(
						value
					);
			}
		}


		// ========================================================
		// DUB CROSSFADER
		// ========================================================

		else if (
			address ==
			"/dub/crossfader"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setDubCrossfader(
						value
					);
			}
		}


		// ========================================================
		// SIREN
		// ========================================================

		else if (
			address ==
			"/dub/sirenPitch"
		)
		{
			float value = 0.0f;

			if (readFloat(value))
			{
				visualState
					.setSirenPitch(
						value
					);
			}
		}

		else if (
			address ==
			"/dub/siren"
		)
		{
			int value = 0;

			if (
				readInt(value) &&
				value ==
					1
			)
			{
				visualState
					.triggerSiren();
			}
		}


		// ========================================================
		// LEGACY VIDEO ADVANCE
		// ========================================================

		else if (
			address ==
			"/video/advance"
		)
		{
			int value = 0;


			if (
				readInt(value) &&
				value ==
					1 &&
				!videos.empty()
			)
			{
				currentVideoIndex =
					(
						currentVideoIndex +
						1
					)
					%
					static_cast<int>(
						videos.size()
					);


				for (
					int i = 0;
					i <
						static_cast<int>(
							videos.size()
						);
					++i
				)
				{
					if (
						i ==
						currentVideoIndex
					)
					{
						videos[i]
							.play();
					}
					else
					{
						videos[i]
							.stop();
					}
				}
			}
		}
	}


	// ============================================================
	// VISUAL STATE
	// ============================================================

	visualState.update(
		ofGetLastFrameTime()
	);


	applyVisualStateParameters();


	// ============================================================
	// EFFECT HISTORIES
	// ============================================================

	if (
		!chronologyManager.isIdle &&
		!chronologyManager
			.isPlayingAnchor()
	)
	{
		ofVideoPlayer* currentVideo =
			chronologyManager
				.getCurrentVideo();


		if (
			currentVideo !=
			nullptr
		)
		{
			updateEffectHistories(
				currentVideo
			);
		}
	}
}
//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::draw()
{
	glBindFramebuffer(
		GL_FRAMEBUFFER,
		0
	);


	ofBackground(
		0,
		0,
		0
	);


	// ============================================================
	// GET CURRENT MAIN VIDEO SAFELY
	// ============================================================

	ofVideoPlayer* currentVideo =
		chronologyManager
			.getCurrentVideo();


	if (
		currentVideo ==
		nullptr
	)
	{
		return;
	}


	// ============================================================
	// IDLE
	// ============================================================

	if (
		chronologyManager
			.isIdle
	)
	{
		ofSetColor(255);


		currentVideo->draw(
			0,
			0,
			ofGetWidth(),
			ofGetHeight()
		);


		return;
	}


	// ============================================================
	// ANCHOR
	//
	// Anchors deliberately bypass the visual effects.
	// ============================================================

	if (
		chronologyManager
			.isPlayingAnchor()
	)
	{
		ofSetColor(255);


		currentVideo->draw(
			0,
			0,
			ofGetWidth(),
			ofGetHeight()
		);


		return;
	}


	// ============================================================
	// BUILD MAIN PROCESSED VIDEO
	// ============================================================

	buildProcessedFrame(
		currentVideo
	);


	// ============================================================
	// SPLIT SCREEN / DOUBLE EXPOSURE
	// ============================================================

	if (
		chronologyManager
			.isSplitScreenActive
	)
	{
		const float amount =
			ofClamp(
				chronologyManager
					.splitScreenAmount,
				0.0f,
				1.0f
			);


		const float screenWidth =
			ofGetWidth();


		const float screenHeight =
			ofGetHeight();


		const float halfWidth =
			screenWidth *
			0.5f;


		// ========================================================
		// SMOOTHING HELPER
		// ========================================================

		auto smooth01 =
			[](float value)
			{
				value =
					ofClamp(
						value,
						0.0f,
						1.0f
					);


				return
					value *
					value *
					(
						3.0f -
						2.0f *
							value
					);
			};


		// ========================================================
		// DOUBLE-EXPOSURE -> SPLIT-SCREEN TRANSITION
		// ========================================================

		const float exposurePeak =
			std::sin(
				amount *
				PI
			);


		const float separation =
			smooth01(
				ofMap(
					amount,
					0.5f,
					1.0f,
					0.0f,
					1.0f,
					true
				)
			);


		const float splitOpacity =
			ofClamp(
				exposurePeak *
					0.70f +
				separation,
				0.0f,
				1.0f
			);


		// ========================================================
		// MAIN VIDEO WIDTH
		//
		// Fullscreen at low amount.
		// Half-screen as separation approaches 1.
		// ========================================================

		const float mainWidth =
			ofLerp(
				screenWidth,
				halfWidth,
				separation
			);


		drawDryProcessedComposite(
			0,
			0,
			mainWidth,
			screenHeight
		);


		// ========================================================
		// SECONDARY VIDEO GEOMETRY
		// ========================================================

		const float exposureScale =
			0.88f;


		const float exposureWidth =
			screenWidth *
			exposureScale;


		const float exposureHeight =
			screenHeight *
			exposureScale;


		const float exposureX =
			(
				screenWidth -
				exposureWidth
			)
			*
			0.5f;


		const float exposureY =
			(
				screenHeight -
				exposureHeight
			)
			*
			0.5f;


		const float splitX =
			ofLerp(
				exposureX,
				halfWidth,
				separation
			);


		const float splitY =
			ofLerp(
				exposureY,
				0.0f,
				separation
			);


		const float splitWidth =
			ofLerp(
				exposureWidth,
				halfWidth,
				separation
			);


		const float splitHeight =
			ofLerp(
				exposureHeight,
				screenHeight,
				separation
			);


		// ========================================================
		// GET SPLIT VIDEO SAFELY
		//
		// IMPORTANT:
		//
		// We no longer do:
		//
		// splitScreenClips[currentSplitIndex]
		//
		// directly here.
		//
		// ChronologyManager checks that the index is valid first.
		// ========================================================

		ofVideoPlayer* splitVideo =
			chronologyManager
				.getCurrentSplitScreenVideo();


		if (
			splitVideo !=
			nullptr
		)
		{
			// ====================================================
			// DOUBLE-EXPOSURE BLEND
			// ====================================================

			ofEnableBlendMode(
				OF_BLENDMODE_SCREEN
			);


			ofSetColor(
				255,
				255,
				255,
				static_cast<int>(
					splitOpacity *
					255.0f
				)
			);


			// ====================================================
			// VIDEO DIMENSIONS
			// ====================================================

			const float videoWidth =
				splitVideo
					->getWidth();


			const float videoHeight =
				splitVideo
					->getHeight();


			// Never divide by zero if a video has failed to
			// initialise or AVFoundation has not supplied its
			// dimensions yet.

			if (
				videoWidth >
					0.0f &&
				videoHeight >
					0.0f &&
				splitWidth >
					0.0f &&
				splitHeight >
					0.0f
			)
			{
				const float videoAspect =
					videoWidth /
					videoHeight;


				const float destinationAspect =
					splitWidth /
					splitHeight;


				float drawWidth =
					splitWidth;


				float drawHeight =
					splitHeight;


				float drawX =
					splitX;


				float drawY =
					splitY;


				// ================================================
				// ASPECT-RATIO-CORRECT DRAW
				// ================================================

				if (
					videoAspect >
					destinationAspect
				)
				{
					// Width constrained.

					drawWidth =
						splitWidth;


					drawHeight =
						drawWidth /
						videoAspect;


					drawY =
						splitY +
						(
							splitHeight -
							drawHeight
						)
						*
						0.5f;
				}

				else
				{
					// Height constrained.

					drawHeight =
						splitHeight;


					drawWidth =
						drawHeight *
						videoAspect;


					drawX =
						splitX +
						(
							splitWidth -
							drawWidth
						)
						*
						0.5f;
				}


				splitVideo->draw(
					drawX,
					drawY,
					drawWidth,
					drawHeight
				);
			}


			ofDisableBlendMode();


			ofSetColor(255);
		}


		// Split-screen branch is finished.
		return;
	}


	// ============================================================
	// NORMAL FULLSCREEN PROCESSED VIDEO
	// ============================================================

	drawDryProcessedComposite(
		0,
		0,
		ofGetWidth(),
		ofGetHeight()
	);
}
//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::exit()
{
	chronologyManager.shutdown();
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::keyPressed(
	int key
)
{
	// ============================================================
	// CHRONOLOGY KEYS
	//
	// Q = next footage
	// W = previous footage
	// N = change topic
	// L = manual loop
	// A = skip anchor
	//
	// ChronologyManager handles these itself.
	// ============================================================

	if (
		key == 'q' ||
		key == 'Q' ||
		key == 'w' ||
		key == 'W' ||
		key == 'n' ||
		key == 'N' ||
		key == 'l' ||
		key == 'L' ||
		key == 'a' ||
		key == 'A'
	)
	{
		chronologyManager.keyPressed(
			key
		);

		return;
	}


	// ============================================================
	// FULLSCREEN
	// ============================================================

	if (
		key == 'f' ||
		key == 'F'
	)
	{
		ofToggleFullscreen();

		ofLogNotice("DEBUG")
			<< "F -> fullscreen toggled";

		return;
	}


	// ============================================================
	// SPLIT SCREEN ON / OFF
	//
	// 4 mirrors the split-screen slider at its two extremes.
	// ============================================================

	if (
		key == '4'
	)
	{
		chronologyManager
			.registerInteraction();


		const bool turnOn =
			chronologyManager
				.splitScreenAmount <
			0.5f;


		chronologyManager
			.splitScreenAmount =
			turnOn
			?
			1.0f
			:
			0.0f;


		chronologyManager
			.toggleSplitScreen(
				turnOn
			);


		ofLogNotice("DEBUG")
			<< "4 -> split screen "
			<< (
				turnOn
				?
				"ON"
				:
				"OFF"
			);


		return;
	}


	// ============================================================
	// ADVANCE SPLIT-SCREEN VIDEO
	//
	// 6 mirrors MIDI Note 66.
	// ============================================================

	if (
		key == '6'
	)
	{
		chronologyManager
			.advanceSplitScreen();


		ofLogNotice("DEBUG")
			<< "6 -> requested split-screen advance";


		return;
	}


	// ============================================================
	// DUB SIREN
	//
	// 5 mirrors the siren trigger.
	// ============================================================

	if (
		key == '5'
	)
	{
		chronologyManager
			.registerInteraction();


		visualState
			.triggerSiren();


		ofLogNotice("DEBUG")
			<< "5 -> siren visual triggered";


		return;
	}


	// ============================================================
	// REVERB / MOTION BLUR DEBUG
	//
	// These call the SAME VisualState setters used by OSC.
	// ============================================================

	static bool debugReverbRoom =
		false;

	static bool debugReverbWet =
		false;

	static bool debugReverbDamping =
		false;

	static bool debugReverbWidth =
		false;


	// ------------------------------------------------------------
	// 1 = REVERB ROOM SIZE
	// ------------------------------------------------------------

	if (
		key == '1'
	)
	{
		debugReverbRoom =
			!debugReverbRoom;


		visualState
			.setReverbRoom(
				debugReverbRoom
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "1 -> reverb room = "
			<< (
				debugReverbRoom
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ------------------------------------------------------------
	// 2 = REVERB WET
	//
	// This is the clearest standalone MotionBlur test.
	// ------------------------------------------------------------

	if (
		key == '2'
	)
	{
		debugReverbWet =
			!debugReverbWet;


		visualState
			.setReverbWet(
				debugReverbWet
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "2 -> reverb wet / motion blur = "
			<< (
				debugReverbWet
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ------------------------------------------------------------
	// 3 = REVERB DAMPING
	// ------------------------------------------------------------

	if (
		key == '3'
	)
	{
		debugReverbDamping =
			!debugReverbDamping;


		visualState
			.setReverbDamping(
				debugReverbDamping
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "3 -> reverb damping = "
			<< (
				debugReverbDamping
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ------------------------------------------------------------
	// 7 = REVERB WIDTH
	// ------------------------------------------------------------

	if (
		key == '7'
	)
	{
		debugReverbWidth =
			!debugReverbWidth;


		visualState
			.setReverbWidth(
				debugReverbWidth
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "7 -> reverb width = "
			<< (
				debugReverbWidth
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ============================================================
	// DELAY / STEP-PRINT DEBUG
	// ============================================================

	static bool debugDelayMix =
		false;

	static bool debugDelayFeedback =
		false;

	static bool debugDelayLong =
		false;


	// ------------------------------------------------------------
	// 8 = DELAY MIX / STEP PRINT
	//
	// Main switch for seeing the StepPrinting effect.
	// ------------------------------------------------------------

	if (
		key == '8'
	)
	{
		debugDelayMix =
			!debugDelayMix;


		visualState
			.setDubSend(
				debugDelayMix
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "8 -> delay mix / step print = "
			<< (
				debugDelayMix
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ------------------------------------------------------------
	// 9 = DELAY FEEDBACK
	//
	// Tests persistence / previous printed frames.
	// ------------------------------------------------------------

	if (
		key == '9'
	)
	{
		debugDelayFeedback =
			!debugDelayFeedback;


		visualState
			.setDelayFeedback(
				debugDelayFeedback
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "9 -> delay feedback = "
			<< (
				debugDelayFeedback
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ------------------------------------------------------------
	// 0 = DELAY TIME
	//
	// Toggle between the minimum and maximum expected values.
	// ------------------------------------------------------------

	if (
		key == '0'
	)
	{
		debugDelayLong =
			!debugDelayLong;


		visualState
			.setDelayTime(
				debugDelayLong
				?
				2000.0f
				:
				50.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "0 -> delay time = "
			<< (
				debugDelayLong
				?
				2000
				:
				50
			)
			<< " ms";


		return;
	}


	// ============================================================
	// EQ VISUALS
	// ============================================================

	static bool debugBass =
		false;

	static bool debugMids =
		false;

	static bool debugTops =
		false;


	// ------------------------------------------------------------
	// B = BASS / FISHEYE
	// ------------------------------------------------------------

	if (
		key == 'b' ||
		key == 'B'
	)
	{
		debugBass =
			!debugBass;


		visualState
			.setBass(
				debugBass
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "B -> bass / fisheye = "
			<< (
				debugBass
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ------------------------------------------------------------
	// M = MIDS
	// ------------------------------------------------------------

	if (
		key == 'm' ||
		key == 'M'
	)
	{
		debugMids =
			!debugMids;


		visualState
			.setMids(
				debugMids
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "M -> mids = "
			<< (
				debugMids
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ------------------------------------------------------------
	// T = TOPS
	// ------------------------------------------------------------

	if (
		key == 't' ||
		key == 'T'
	)
	{
		debugTops =
			!debugTops;


		visualState
			.setTops(
				debugTops
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "T -> tops = "
			<< (
				debugTops
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ============================================================
	// DUB CROSSFADER VISUAL
	// ============================================================

	static bool debugCrossfader =
		false;


	if (
		key == 'c' ||
		key == 'C'
	)
	{
		debugCrossfader =
			!debugCrossfader;


		visualState
			.setDubCrossfader(
				debugCrossfader
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "C -> dub crossfader = "
			<< (
				debugCrossfader
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ============================================================
	// SIREN PITCH VISUAL
	// ============================================================

	static bool debugSirenPitch =
		false;


	if (
		key == 'p' ||
		key == 'P'
	)
	{
		debugSirenPitch =
			!debugSirenPitch;


		visualState
			.setSirenPitch(
				debugSirenPitch
				?
				1.0f
				:
				0.0f
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "P -> siren pitch = "
			<< (
				debugSirenPitch
				?
				1.0f
				:
				0.0f
			);


		return;
	}


	// ============================================================
	// RESET ALL DEBUG EFFECT VALUES
	// ============================================================

	if (
		key == 'r' ||
		key == 'R'
	)
	{
		debugReverbRoom =
			false;

		debugReverbWet =
			false;

		debugReverbDamping =
			false;

		debugReverbWidth =
			false;

		debugDelayMix =
			false;

		debugDelayFeedback =
			false;

		debugDelayLong =
			false;

		debugBass =
			false;

		debugMids =
			false;

		debugTops =
			false;

		debugCrossfader =
			false;

		debugSirenPitch =
			false;


		visualState.setReverbRoom(
			0.0f
		);

		visualState.setReverbWet(
			0.0f
		);

		visualState.setReverbDamping(
			0.0f
		);

		visualState.setReverbWidth(
			0.0f
		);


		visualState.setDubSend(
			0.0f
		);

		visualState.setDelayFeedback(
			0.0f
		);

		visualState.setDelayTime(
			50.0f
		);


		visualState.setBass(
			0.0f
		);

		visualState.setMids(
			0.0f
		);

		visualState.setTops(
			0.0f
		);


		visualState.setDubCrossfader(
			0.0f
		);

		visualState.setSirenPitch(
			0.0f
		);


		motionBlur.clear();

		stepPrinting.clearFrames();


		chronologyManager
			.splitScreenAmount =
			0.0f;

		chronologyManager
			.toggleSplitScreen(
				false
			);


		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "R -> all debug effects reset";


		return;
	}


	// ============================================================
	// WAKE / REGISTER INTERACTION ONLY
	// ============================================================

	if (
		key == 'i' ||
		key == 'I'
	)
	{
		chronologyManager
			.registerInteraction();


		ofLogNotice("DEBUG")
			<< "I -> interaction registered / idle timer reset";


		return;
	}
}
//--------------------------------------------------------------
void ofApp::copyFbo(ofFbo& source,
					ofFbo& destination)
{
	destination.begin();

	ofClear(0, 0, 0, 255);
	ofSetColor(255);

	source.draw(
		0,
		0,
		destination.getWidth(),
		destination.getHeight()
	);

	destination.end();
}

//--------------------------------------------------------------
void ofApp::blendFbo(ofFbo& base,
					 ofFbo& overlay,
					 ofFbo& destination,
					 float amount)
{
	amount =
		ofClamp(
			amount,
			0.0f,
			1.0f
		);

	destination.begin();

	ofClear(0, 0, 0, 255);

	ofSetColor(255);

	base.draw(
		0,
		0,
		destination.getWidth(),
		destination.getHeight()
	);

	if (amount > 0.001f)
	{
		ofEnableBlendMode(
			OF_BLENDMODE_ALPHA
		);

		ofSetColor(
			255,
			255,
			255,
			amount *
			255.0f
		);

		overlay.draw(
			0,
			0,
			destination.getWidth(),
			destination.getHeight()
		);

		ofDisableBlendMode();
	}

	ofSetColor(255);
	destination.end();
}

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::applyVisualStateParameters()
{
	static bool previousBlurActive =
		false;

	static bool previousStepActive =
		false;


	// ============================================================
	// MOTION BLUR / REVERB
	// ============================================================

	const float blurWeight =
		ofClamp(
			visualState
				.getMotionBlurWeight(),
			0.0f,
			1.0f
		);


	// VisualStateManager already performs the easing.
	//
	// Once it reaches this floor, Reverb is genuinely OFF.
	const bool blurControlActive =
		blurWeight >
		0.018f;


	isMotionBlurActive =
		blurControlActive;


	motionBlur.setBlendFactor(
		visualState
			.getBlurPersistence()
	);


	motionBlur.setMixAmount(
		blurControlActive
		?
		visualState
			.getBlurMix()
		:
		0.0f
	);


	motionBlur.setStretchAmount(
		blurControlActive
		?
		visualState
			.getBlurStretch()
		:
		0.0f
	);


	// ============================================================
	// IMPORTANT:
	//
	// ZERO REALLY MEANS ZERO.
	//
	// The easing has already happened in VisualStateManager.
	//
	// Once blurWeight reaches the OFF threshold, destroy the old
	// accumulation completely so it cannot keep appearing through
	// hasVisibleResidue().
	// ============================================================

	if (
		previousBlurActive &&
		!blurControlActive
	)
	{
		motionBlur.clear();
	}


	// ============================================================
	// STEP PRINT / DELAY
	// ============================================================

	const float stepWeight =
		ofClamp(
			visualState
				.getStepPrintWeight(),
			0.0f,
			1.0f
		);


	isStepActive =
		stepWeight >
		0.018f;


	stepPrinting.setStepInterval(
		visualState
			.getStepInterval()
	);


	stepPrinting.setMixAmount(
		isStepActive
		?
		visualState
			.getStepMix()
		:
		0.0f
	);


	stepPrinting.setFeedbackAmount(
		isStepActive
		?
		visualState
			.getStepFeedback()
		:
		0.0f
	);


	if (
		previousStepActive &&
		!isStepActive
	)
	{
		stepPrinting.clearFrames();
	}


	// ============================================================
	// FISHEYE / BASS
	// ============================================================

	const float fisheyeWeight =
		ofClamp(
			visualState
				.getFisheyeWeight(),
			0.0f,
			1.0f
		);


	isBassActive =
		fisheyeWeight >
		0.018f;


	fisheye.setMaxDistortion(
		visualState
			.getFisheyeMaxDistortion()
	);


	fisheye.setBassLevel(
		isBassActive
		?
		visualState
			.getFisheyeBass()
		:
		0.0f
	);


	fisheye.setDistortionStrength(
		isBassActive
		?
		visualState
			.getFisheyeDistortion()
		:
		0.0f
	);


	fisheye.setPulseFrequency(
		visualState
			.getFisheyePulseFrequency()
	);


	if (
		!isBassActive
	)
	{
		fisheye.reset();
	}


	previousBlurActive =
		isMotionBlurActive;

	previousStepActive =
		isStepActive;
}

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::updateEffectHistories(
	ofVideoPlayer* currentVideo
)
{
	if (
		currentVideo ==
			nullptr ||
		!currentVideo
			->isFrameNew()
	)
	{
		return;
	}


	// ============================================================
	// CLEAN CURRENT FRAME
	// ============================================================

	videoFbo.begin();

	ofClear(
		0,
		0,
		0,
		255
	);

	ofSetColor(255);

	currentVideo->draw(
		0,
		0,
		standardWidth,
		standardHeight
	);

	videoFbo.end();


	// ============================================================
	// CURRENT EFFECT STRENGTHS
	// ============================================================

	const float blurWeight =
		ofClamp(
			visualState
				.getMotionBlurWeight(),
			0.0f,
			1.0f
		);


	const float stepWeight =
		ofClamp(
			visualState
				.getStepPrintWeight(),
			0.0f,
			1.0f
		);


	const float bassWeight =
		ofClamp(
			visualState
				.getFisheyeWeight(),
			0.0f,
			1.0f
		);


	const float mutation =
		ofClamp(
			visualState
				.getMutationAmount(),
			0.0f,
			1.0f
		);


	const float drift =
		ofClamp(
			visualState
				.getMutationDrift(),
			0.0f,
			1.0f
		);


	// IMPORTANT:
	//
	// Use the SAME threshold as applyVisualStateParameters().
	//
	// Previously this was 0.001, meaning MotionBlur could still
	// create NEW history after the renderer considered Reverb off.
	const bool blurControlActive =
		blurWeight >
		0.018f;


	// ============================================================
	// FISHEYE INTERNAL MOTION
	// ============================================================

	if (
		isBassActive
	)
	{
		fisheye.update(
			videoFbo
				.getTexture()
		);
	}


	// ============================================================
	// MOTION BLUR HISTORY
	//
	// ACTIVE:
	//     create history.
	//
	// OFF:
	//     create nothing.
	//
	// applyVisualStateParameters() clears it on the transition
	// to zero.
	// ============================================================

	if (
		blurControlActive
	)
	{
		motionBlur.update(
			videoFbo
				.getTexture()
		);
	}


	// ============================================================
	// STEP PRINT
	// ============================================================

	if (
		!isStepActive
	)
	{
		return;
	}


	copyFbo(
		videoFbo,
		historySourceFbo
	);


	// ============================================================
	// MOTION BLUR + STEP PRINT CAPTURE
	// ============================================================

	if (
		blurControlActive
	)
	{
		copyFbo(
			videoFbo,
			routeAFbo
		);


		motionBlur.apply(
			routeAFbo
		);


		const float pair =
			std::min(
				blurWeight,
				stepWeight
			);


		float captureBlurAmount =
			pair *
			(
				0.46f +
				mutation *
					0.10f
			);


		captureBlurAmount +=
			(
				drift -
				0.5f
			)
			*
			0.08f;


		captureBlurAmount =
			ofClamp(
				captureBlurAmount,
				0.0f,
				0.72f
			);


		if (
			captureBlurAmount >
			0.001f
		)
		{
			blendFbo(
				videoFbo,
				routeAFbo,
				historySourceFbo,
				captureBlurAmount
			);
		}
	}


	// ============================================================
	// IMPORTANT:
	//
	// DO NOT bake Fisheye into StepPrint history here.
	//
	// StepPrint history should remain a FULL-VISIBILITY temporal
	// frame.
	//
	// The Delay+Bass renderer later takes that SAME StepPrinted
	// world and creates the warped / unwarped horizontal splice.
	//
	// Baking Fisheye here and then warping it again was making the
	// combination needlessly muddy.
	// ============================================================


	// ============================================================
	// CAPTURE STEP PRINT
	// ============================================================

	stepPrinting.update(
		historySourceFbo
			.getTexture()
	);
}
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::renderTemporalMutation(
	ofFbo& source,
	ofFbo& destination
)
{
	auto smooth01 =
		[](float value)
		{
			value =
				ofClamp(
					value,
					0.0f,
					1.0f
				);

			return
				value *
				value *
				(
					3.0f -
					2.0f *
						value
				);
		};


	// ============================================================
	// CONTINUOUS WEIGHTS
	// ============================================================

	const float reverbWeight =
		ofClamp(
			visualState
				.getMotionBlurWeight(),
			0.0f,
			1.0f
		);


	const float delayWeight =
		ofClamp(
			visualState
				.getStepPrintWeight(),
			0.0f,
			1.0f
		);


	const float reverbAmount =
		smooth01(
			reverbWeight
		);


	const float delayAmount =
		smooth01(
			delayWeight
		);


	// ============================================================
	// ABSOLUTE CLEAN STATE
	// ============================================================

	if (
		reverbWeight <=
			0.001f &&
		delayWeight <=
			0.001f
	)
	{
		copyFbo(
			source,
			destination
		);

		return;
	}


	// ============================================================
	// MOTION-BLUR WORLD
	// ============================================================

	copyFbo(
		source,
		routeAFbo
	);


	if (
		reverbAmount >
		0.001f
	)
	{
		motionBlur.apply(
			routeAFbo
		);
	}


	// ============================================================
	// STEP-PRINT WORLD
	// ============================================================

	copyFbo(
		source,
		routeBFbo
	);


	if (
		delayAmount >
		0.001f
	)
	{
		stepPrinting.apply(
			routeBFbo
		);
	}


	// ============================================================
	// BASIC TEMPORAL STATE
	//
	// NO RESIDUE OVERRIDE.
	//
	// Reverb controls MotionBlur.
	// If Reverb = zero, blurContribution = zero.
	// ============================================================

	const float blurContribution =
		reverbAmount;


	blendFbo(
		source,
		routeAFbo,
		preWarpTemporalFbo,
		blurContribution
	);


	const float printContribution =
		delayAmount *
		(
			1.0f -
			reverbAmount *
				0.32f
		);


	blendFbo(
		preWarpTemporalFbo,
		routeBFbo,
		preWarpFbo,
		printContribution
	);


	// ============================================================
	// REVERB + DELAY OVERLAP
	// ============================================================

	const float rawOverlap =
		std::min(
			reverbWeight,
			delayWeight
		);


	float accordionAmount =
		ofMap(
			rawOverlap,
			0.025f,
			0.58f,
			0.0f,
			1.0f,
			true
		);


	accordionAmount =
		smooth01(
			accordionAmount
		);


	if (
		accordionAmount <=
		0.001f
	)
	{
		copyFbo(
			preWarpFbo,
			destination
		);

		return;
	}


	const float overlap =
		ofClamp(
			visualState
				.getTemporalOverlap(),
			0.0f,
			1.0f
		);


	const float mutation =
		ofClamp(
			visualState
				.getMutationAmount(),
			0.0f,
			1.0f
		);


	const float direction =
		ofClamp(
			visualState
				.getTemporalDirection(),
			-1.0f,
			1.0f
		);


	const float drift =
		ofClamp(
			visualState
				.getMutationDrift(),
			0.0f,
			1.0f
		);


	// ============================================================
	// BUILD VERTICAL TEMPORAL ACCORDION
	// ============================================================

	mutationFbo.begin();

	ofClear(
		0,
		0,
		0,
		255
	);

	ofSetColor(255);

	preWarpFbo.draw(
		0,
		0,
		standardWidth,
		standardHeight
	);


	const int sliceCount =
		3 +
		static_cast<int>(
			accordionAmount *
			11.0f
		);


	const float sliceWidth =
		standardWidth /
		static_cast<float>(
			std::max(
				1,
				sliceCount
			)
		);


	const float time =
		ofGetElapsedTimef();


	for (
		int i = 0;
		i < sliceCount;
		++i
	)
	{
		const float normalized =
			static_cast<float>(i)
			/
			std::max(
				1.0f,
				static_cast<float>(
					sliceCount -
					1
				)
			);


		const float wave =
			sin(
				normalized *
					PI *
					(
						2.0f +
						overlap *
							3.5f
					)
				+
				time *
					(
						0.10f +
						mutation *
							0.18f
					)
				+
				direction *
					1.8f
				+
				drift *
					2.5f
			);


		const bool chooseBlur =
			(
				wave +
				direction *
					0.55f
			)
			>
			0.0f;


		ofFbo& chosen =
			chooseBlur
			?
			routeAFbo
			:
			routeBFbo;


		const float displacement =
			wave *
			accordionAmount *
			(
				3.0f +
				overlap *
					58.0f
			);


		const float sourceX =
			ofClamp(
				i *
					sliceWidth +
					displacement,
				0.0f,
				std::max(
					0.0f,
					standardWidth -
						sliceWidth
				)
			);


		glEnable(
			GL_SCISSOR_TEST
		);


		glScissor(
			static_cast<int>(
				i *
					sliceWidth
			),
			0,
			std::max(
				1,
				static_cast<int>(
					sliceWidth +
						2.0f
				)
			),
			static_cast<int>(
				standardHeight
			)
		);


		ofSetColor(255);


		chosen
			.getTexture()
			.drawSubsection(
				i *
					sliceWidth,
				0,

				sliceWidth +
					2.0f,
				standardHeight,

				sourceX,
				0,

				sliceWidth +
					2.0f,
				standardHeight
			);


		glDisable(
			GL_SCISSOR_TEST
		);
	}


	ofSetColor(255);

	mutationFbo.end();


	blendFbo(
		preWarpFbo,
		mutationFbo,
		destination,
		accordionAmount
	);
}
//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::applyInteractionMemory(
	ofFbo& target
)
{
	const float mutation =
		ofClamp(
			visualState
				.getMutationAmount(),
			0.0f,
			1.0f
		);


	// ============================================================
	// NO MEANINGFUL COMBINATION = NO MEMORY
	// ============================================================

	if (
		mutation <
		0.08f
	)
	{
		if (
			interactionMemoryReady
		)
		{
			interactionMemoryFbo.begin();

			ofClear(
				0,
				0,
				0,
				255
			);

			interactionMemoryFbo.end();


			interactionMemoryReady =
				false;
		}

		return;
	}


	// ============================================================
	// FIRST FRAME
	// ============================================================

	if (
		!interactionMemoryReady
	)
	{
		copyFbo(
			target,
			interactionMemoryFbo
		);


		interactionMemoryReady =
			true;


		return;
	}


	const float drift =
		visualState
			.getMutationDrift();


	// ============================================================
	// SHORT MEMORY
	//
	// Strong enough to give combinations texture,
	// not strong enough to feel permanently stuck.
	// ============================================================

	float memoryOverlay =
		ofLerp(
			0.025f,
			0.14f,
			mutation
		);


	memoryOverlay +=
		(
			drift -
			0.5f
		)
		*
		0.025f;


	memoryOverlay =
		ofClamp(
			memoryOverlay,
			0.0f,
			0.16f
		);


	blendFbo(
		target,
		interactionMemoryFbo,
		interactionScratchFbo,
		memoryOverlay
	);


	copyFbo(
		interactionScratchFbo,
		target
	);


	// ============================================================
	// UPDATE MEMORY QUICKLY
	//
	// This prevents one dramatic old state hanging around.
	// ============================================================

	const float captureAmount =
		ofLerp(
			0.55f,
			0.34f,
			mutation
		);


	blendFbo(
		interactionMemoryFbo,
		target,
		routeAFbo,
		captureAmount
	);


	copyFbo(
		routeAFbo,
		interactionMemoryFbo
	);
}

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::buildProcessedFrame(
	ofVideoPlayer* currentVideo
)
{
	if (
		currentVideo ==
		nullptr
	)
	{
		return;
	}


	// ============================================================
	// SMOOTHING HELPER
	// ============================================================

	auto smooth01 =
		[](float value)
		{
			value =
				ofClamp(
					value,
					0.0f,
					1.0f
				);


			return
				value *
				value *
				(
					3.0f -
					2.0f *
						value
				);
		};


	// ============================================================
	// CLEAN SOURCE
	// ============================================================

	videoFbo.begin();

	ofClear(
		0,
		0,
		0,
		255
	);

	ofSetColor(255);

	currentVideo->draw(
		0,
		0,
		standardWidth,
		standardHeight
	);

	videoFbo.end();


	// ============================================================
	// CONTINUOUS FAMILY WEIGHTS
	// ============================================================

	const float reverbWeight =
		ofClamp(
			visualState
				.getMotionBlurWeight(),
			0.0f,
			1.0f
		);


	const float delayWeight =
		ofClamp(
			visualState
				.getStepPrintWeight(),
			0.0f,
			1.0f
		);


	const float bassWeight =
		ofClamp(
			visualState
				.getFisheyeWeight(),
			0.0f,
			1.0f
		);


	const float reverbAmount =
		smooth01(
			reverbWeight
		);


	const float delayAmount =
		smooth01(
			delayWeight
		);


	const float bassAmount =
		smooth01(
			bassWeight
		);


	const float drift =
		ofClamp(
			visualState
				.getMutationDrift(),
			0.0f,
			1.0f
		);


	// ============================================================
	// PAIR / TRIPLE TRANSITION AMOUNTS
	//
	// These are continuous.
	//
	// The unique combination grammar gradually emerges as the
	// corresponding effects overlap.
	// ============================================================

	float delayBassAmount =
		ofMap(
			std::min(
				delayWeight,
				bassWeight
			),
			0.025f,
			0.60f,
			0.0f,
			1.0f,
			true
		);


	delayBassAmount =
		smooth01(
			delayBassAmount
		);


	float reverbBassAmount =
		ofMap(
			std::min(
				reverbWeight,
				bassWeight
			),
			0.025f,
			0.60f,
			0.0f,
			1.0f,
			true
		);


	reverbBassAmount =
		smooth01(
			reverbBassAmount
		);


	float tripleAmount =
		ofMap(
			std::min(
				reverbWeight,
				std::min(
					delayWeight,
					bassWeight
				)
			),
			0.04f,
			0.68f,
			0.0f,
			1.0f,
			true
		);


	tripleAmount =
		smooth01(
			tripleAmount
		);


	// ============================================================
	// TEMPORAL WORLD
	//
	// Clean -> Blur -> Step -> Vertical accordion all transition
	// continuously inside this call.
	// ============================================================

	renderTemporalMutation(
		videoFbo,
		preWarpTemporalFbo
	);



	// ============================================================
	// FULL-FRAME BASS / FISHEYE WORLD
	//
	// IMPORTANT:
	//
	// Fisheye is NOT an alpha overlay.
	//
	// The temporal world itself is passed through the fisheye.
	//
	// When Bass = 0, FisheyeLens returns its input unchanged.
	// As Bass rises, that SAME image progressively bends.
	//
	// Therefore there is never:
	//
	// clean image underneath
	// +
	// transparent warped image on top
	//
	// There is only one complete image.
	// ============================================================

	fisheye.renderTo(
		preWarpTemporalFbo.getTexture(),
		routeAFbo
	);


	// ============================================================
	// THIS IS THE COMPLETE BASE VISUAL STATE
	// ============================================================

	copyFbo(
		routeAFbo,
		processedFbo
	);


	// ============================================================
	// DELAY + BASS
	//
	// DENSE HORIZONTAL TIME-CRATER
	//
	// IMPORTANT:
	//
	// 1. The ENTIRE image is delayed / StepPrinted first.
	//
	// 2. The ENTIRE image is then Fisheye warped.
	//
	// 3. There is NO untouched-video layer underneath.
	//
	// 4. Horizontal territories alternate between two spatial
	//    interpretations of the SAME delayed + warped image.
	//
	// 5. Much denser slicing than the recent version.
	// ============================================================

	if (
		delayBassAmount >
		0.001f
	)
	{
		// ========================================================
		// SHARED DELAYED TEMPORAL WORLD
		// ========================================================

		copyFbo(
			videoFbo,
			routeBFbo
		);


		stepPrinting.apply(
			routeBFbo
		);


		// ========================================================
		// WARP THE COMPLETE DELAYED WORLD
		//
		// routeAFbo is now:
		//
		// DELAYED
		// +
		// FISHEYE
		//
		// There is no clean image involved.
		// ========================================================

		fisheye.renderTo(
			routeBFbo.getTexture(),
			routeAFbo
		);


		// ========================================================
		// FISHEYE CENTRE
		// ========================================================

		const float centreX =
			standardWidth *
			(
				0.5f +
				(
					drift -
					0.5f
				)
				*
				0.12f
			);


		const float centreY =
			standardHeight *
			(
				0.5f +
				sin(
					ofGetElapsedTimef() *
					0.31f
				)
				*
				0.055f *
				bassAmount
			);


		// ========================================================
		// SECOND SPATIAL INTERPRETATION
		//
		// SAME delayed + fisheye frame.
		//
		// Just pulled slightly harder around the lens centre.
		// ========================================================

		const float secondaryScale =
			1.0f +
			bassAmount *
			delayBassAmount *
			0.11f;


		const float secondaryWidth =
			standardWidth *
			secondaryScale;


		const float secondaryHeight =
			standardHeight *
			secondaryScale;


		mutationFbo.begin();


		ofClear(
			0,
			0,
			0,
			255
		);


		ofSetColor(255);


		routeAFbo.draw(
			centreX -
				secondaryWidth *
					0.5f,
			centreY -
				secondaryHeight *
					0.5f,
			secondaryWidth,
			secondaryHeight
		);


		mutationFbo.end();


		// ========================================================
		// BASE IMAGE
		//
		// IMPORTANT:
		//
		// Start from FISHEYE.
		//
		// Do NOT start from routeBFbo, because routeBFbo is the
		// unwarped delayed image and would recreate the problem
		// you are seeing now.
		// ========================================================

		preWarpFbo.begin();


		ofClear(
			0,
			0,
			0,
			255
		);


		ofSetColor(255);


		routeAFbo.draw(
			0,
			0,
			standardWidth,
			standardHeight
		);


		// ========================================================
		// DENSER SPLICE FIELD
		//
		// Recent version:
		//
		//     5 -> 14 bands
		//
		// New version:
		//
		//     12 -> 36 bands
		//
		// So it becomes increasingly fragmented as the combination
		// strengthens.
		// ========================================================

		const int rings =
			12 +
			static_cast<int>(
				delayBassAmount *
				24.0f
			);


		const float maxRadius =
			sqrt(
				standardWidth *
					standardWidth +
				standardHeight *
					standardHeight
			)
			*
			0.62f;


		// ========================================================
		// HORIZONTAL NESTED SCISSOR TERRITORIES
		// ========================================================

		for (
			int ring = rings;
			ring >= 0;
			--ring
		)
		{
			const float t =
				static_cast<float>(
					ring
				)
				/
				static_cast<float>(
					std::max(
						1,
						rings
					)
				);


			const float radius =
				maxRadius *
				t;


			// ====================================================
			// MORE NUMEROUS, SLIGHTLY FINER HORIZONTAL SLICES
			// ====================================================

			const float bandHeight =
				std::max(
					5.0f,
					radius *
						0.52f
				);


			const float y =
				ofClamp(
					centreY -
						bandHeight *
							0.5f,
					0.0f,
					std::max(
						0.0f,
						standardHeight -
							bandHeight
					)
				);


			// ====================================================
			// OPENGL SCISSOR
			// ====================================================

			glEnable(
				GL_SCISSOR_TEST
			);


			const int scissorX =
				0;


			const int scissorY =
				static_cast<int>(
					standardHeight -
					(
						y +
						bandHeight
					)
				);


			const int scissorWidth =
				static_cast<int>(
					standardWidth
				);


			const int scissorHeight =
				std::max(
					1,
					static_cast<int>(
						bandHeight
					)
				);


			glScissor(
				scissorX,
				std::max(
					0,
					scissorY
				),
				scissorWidth,
				scissorHeight
			);


			// ====================================================
			// ALTERNATE BETWEEN TWO WARPED VERSIONS
			//
			// routeAFbo:
			// normal delayed + fisheye
			//
			// mutationFbo:
			// SAME delayed + fisheye frame, pulled harder
			//
			// Neither one is clean / untouched footage.
			// ====================================================

			ofFbo& chosen =
				(
					ring %
					2 ==
					0
				)
				?
				routeAFbo
				:
				mutationFbo;


			// ====================================================
			// LOCAL SLICE PULL
			//
			// This gives every nested territory a slightly different
			// spatial magnification.
			//
			// Multiply by delayBassAmount so it gradually appears
			// rather than snapping into a fully fragmented state.
			// ====================================================

			const float parityPull =
				(
					ring %
					2 ==
					0
				)
				?
				0.11f
				:
				0.20f;


			const float scale =
				1.0f +
				(
					1.0f -
					t
				)
				*
				bassAmount *
				delayBassAmount *
				parityPull;


			const float drawWidth =
				standardWidth *
				scale;


			const float drawHeight =
				standardHeight *
				scale;


			// ====================================================
			// FULL OPACITY
			// ====================================================

			ofSetColor(255);


			chosen.draw(
				centreX -
					drawWidth *
						0.5f,
				centreY -
					drawHeight *
						0.5f,
				drawWidth,
				drawHeight
			);


			// ====================================================
			// IMPORTANT:
			//
			// Never leave scissor enabled after this territory.
			// ====================================================

			glDisable(
				GL_SCISSOR_TEST
			);
		}


		ofSetColor(255);


		preWarpFbo.end();


		// ========================================================
		// COMPLETE RESULT
		//
		// Again: NO alpha blend over a clean image.
		//
		// The strength of the splice has already been encoded into
		// the number of territories and their spatial displacement.
		// ========================================================

		copyFbo(
			preWarpFbo,
			processedFbo
		);
	}

		// ========================================================
		// EASE INTO / OUT OF THE SPLICE
		//
		// Keep this because you asked for gradual transitions.
		// ========================================================


	// ============================================================
	// ALL THREE
	//
	// IMPORTANT CHANGE:
	//
	// Bass NO LONGER dominates the whole state.
	//
	// The temporal structure stays primary.
	//
	// Bass only bends it enough to make the collapse spatial.
	//
	// This specifically fixes:
	//
	// "giant faded-out fisheye".
	// ============================================================

	if (
		tripleAmount >
		0.001f
	)
	{
		// --------------------------------------------------------
		// TEMPORAL ACCORDION IS THE BASE
		// --------------------------------------------------------

		copyFbo(
			preWarpTemporalFbo,
			routeBFbo
		);


		// --------------------------------------------------------
		// WARP THAT TEMPORAL WORLD
		// --------------------------------------------------------

		fisheye.renderTo(
			routeBFbo.getTexture(),
			mutationFbo
		);


		// --------------------------------------------------------
		// VERY IMPORTANT:
		//
		// Only a restricted amount of fisheye enters the complete
		// three-way state.
		//
		// Max = roughly 30%.
		//
		// It should deform the temporal world, not swallow it.
		// --------------------------------------------------------

		const float tripleBassInfluence =
			bassAmount *
			0.30f;


		blendFbo(
			routeBFbo,
			mutationFbo,
			preWarpFbo,
			tripleBassInfluence
		);


		// ========================================================
		// DELAYED WORLD FOR FULL-OPACITY HORIZONTAL RUPTURES
		// ========================================================

		copyFbo(
			videoFbo,
			routeBFbo
		);


		stepPrinting.apply(
			routeBFbo
		);


		fisheye.renderTo(
			routeBFbo.getTexture(),
			routeAFbo
		);


		preWarpFbo.begin();


		const float centreX =
			standardWidth *
			(
				0.5f +
				(
					drift -
					0.5f
				)
				*
				0.08f
			);


		const float centreY =
			standardHeight *
			(
				0.5f +
				sin(
					ofGetElapsedTimef() *
						0.27f
				)
				*
				0.035f *
				bassAmount
			);


		const int ruptureBands =
			4 +
			static_cast<int>(
				tripleAmount *
				18.0f
			);


		const float maxRadius =
			sqrt(
				standardWidth *
					standardWidth +
				standardHeight *
					standardHeight
			)
			*
			0.52f;


		for (
			int band = ruptureBands;
			band >= 0;
			--band
		)
		{
			const float t =
				static_cast<float>(
					band
				)
				/
				static_cast<float>(
					std::max(
						1,
						ruptureBands
					)
				);


			const float radius =
				maxRadius *
				t;


			const float bandHeight =
				std::max(
					7.0f,
					radius *
						0.22f
				);


			const float y =
				ofClamp(
					centreY -
						bandHeight *
							0.5f,
					0.0f,
					std::max(
						0.0f,
						standardHeight -
							bandHeight
					)
				);


			glEnable(
				GL_SCISSOR_TEST
			);


			const int scissorY =
				static_cast<int>(
					standardHeight -
					(
						y +
						bandHeight
					)
				);


			glScissor(
				0,
				std::max(
					0,
					scissorY
				),
				static_cast<int>(
					standardWidth
				),
				std::max(
					1,
					static_cast<int>(
						bandHeight
					)
				)
			);


			// Full opacity.
			ofSetColor(255);


			const float scale =
				1.0f +
				(
					1.0f -
					t
				)
				*
				bassAmount *
				0.10f;


			const float drawWidth =
				standardWidth *
				scale;


			const float drawHeight =
				standardHeight *
				scale;


			// Both sources are delayed.
			//
			// Alternate spatial interpretation only.
			ofFbo& chosen =
				(
					band %
					2 ==
					0
				)
				?
				routeAFbo
				:
				routeBFbo;


			chosen.draw(
				centreX -
					drawWidth *
						0.5f,
				centreY -
					drawHeight *
						0.5f,
				drawWidth,
				drawHeight
			);


			glDisable(
				GL_SCISSOR_TEST
			);
		}


		ofSetColor(255);

		preWarpFbo.end();


		// --------------------------------------------------------
		// EASE INTO / OUT OF THE THREE-WAY STATE.
		// --------------------------------------------------------

		blendFbo(
			processedFbo,
			preWarpFbo,
			interactionScratchFbo,
			tripleAmount
		);


		copyFbo(
			interactionScratchFbo,
			processedFbo
		);
	}


	// ============================================================
	// IMPORTANT:
	//
	// DO NOT applyInteractionMemory() here.
	//
	// The temporal effects already contain history.
	//
	// Additional alpha-memory was muddying / fading the image and
	// making the all-three state lose visibility.
	// ============================================================

	ofSetColor(255);
}


//--------------------------------------------------------------
void ofApp::drawDryProcessedComposite(
	float x,
	float y,
	float width,
	float height
)
{
	ofSetColor(255);


	// processedFbo now always contains the complete visual state:
	//
	// clean
	// ->
	// individual effect
	// ->
	// pair interaction
	// ->
	// three-way interaction
	//
	// All transitions are already continuous before we reach draw.
	processedFbo.draw(
		x,
		y,
		width,
		height
	);
}
