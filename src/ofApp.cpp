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
void ofApp::update()
{
	chronologyManager.update();

	if (chronologyManager.isSplitScreenActive &&
		!chronologyManager.splitScreenClips.empty())
	{
		chronologyManager
			.splitScreenClips[
				chronologyManager.currentSplitIndex
			]
			.video
			.update();
	}

	while (oscReceiver.hasWaitingMessages())
	{
		ofxOscMessage m;
		oscReceiver.getNextMessage(m);

		const std::string address =
			m.getAddress();

		if (address == "/reverb/roomSize")
		{
			visualState.setReverbRoom(
				m.getArgAsFloat(0)
			);
		}
		else if (address == "/reverb/wetLevel")
		{
			visualState.setReverbWet(
				m.getArgAsFloat(0)
			);
		}
		else if (address == "/reverb/damping")
		{
			visualState.setReverbDamping(
				m.getArgAsFloat(0)
			);
		}
		else if (address == "/reverb/width")
		{
			visualState.setReverbWidth(
				m.getArgAsFloat(0)
			);
		}

		else if (address == "/delay/delayTime")
		{
			visualState.setDelayTime(
				m.getArgAsFloat(0)
			);
		}
		else if (address == "/delay/feedbackValue")
		{
			visualState.setDelayFeedback(
				m.getArgAsFloat(0)
			);
		}
		else if (address == "/delay/mixValue")
		{
			visualState.setDubSend(
				m.getArgAsFloat(0)
			);
		}

		else if (address == "/eq/bass")
		{
			visualState.setBass(
				m.getArgAsFloat(0)
			);
		}
		else if (address == "/eq/mids")
		{
			visualState.setMids(
				m.getArgAsFloat(0)
			);
		}
		else if (address == "/eq/tops")
		{
			visualState.setTops(
				m.getArgAsFloat(0)
			);
		}

		else if (address == "/dub/crossfader")
		{
			visualState.setDubCrossfader(
				m.getArgAsFloat(0)
			);
		}

		else if (address == "/dub/sirenPitch")
		{
			visualState.setSirenPitch(
				m.getArgAsFloat(0)
			);
		}
		else if (address == "/dub/siren")
		{
			if (m.getArgAsInt(0) == 1)
			{
				visualState.triggerSiren();
			}
		}

		else if (address == "/video/advance" &&
				 m.getArgAsInt(0) == 1)
		{
			if (!videos.empty())
			{
				currentVideoIndex =
					(currentVideoIndex + 1)
					%
					static_cast<int>(videos.size());

				for (int i = 0;
					 i < static_cast<int>(videos.size());
					 ++i)
				{
					if (i == currentVideoIndex)
						videos[i].play();
					else
						videos[i].stop();
				}
			}
		}

		else if (address == "/interaction")
		{
			chronologyManager.registerInteraction();
		}
	}

	visualState.update(
		ofGetLastFrameTime()
	);

	applyVisualStateParameters();

	if (!chronologyManager.isIdle &&
		!chronologyManager.isPlayingAnchor())
	{
		updateEffectHistories(
			chronologyManager.getCurrentVideo()
		);
	}
}

//--------------------------------------------------------------
void ofApp::draw()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ofBackground(0, 0, 0);

	ofVideoPlayer* currentVideo =
		chronologyManager.getCurrentVideo();

	if (currentVideo == nullptr)
		return;

	if (chronologyManager.isIdle)
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

	if (chronologyManager.isPlayingAnchor())
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

	buildProcessedFrame(currentVideo);

	if (chronologyManager.isSplitScreenActive)
	{
		const float amount =
			ofClamp(
				chronologyManager.splitScreenAmount,
				0.0f,
				1.0f
			);

		const float screenWidth =
			ofGetWidth();

		const float screenHeight =
			ofGetHeight();

		const float halfWidth =
			screenWidth * 0.5f;

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
					(3.0f -
					 2.0f * value);
			};

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
				exposurePeak * 0.70f +
				separation,
				0.0f,
				1.0f
			);

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

		const float exposureScale =
			0.88f;

		const float exposureWidth =
			screenWidth *
			exposureScale;

		const float exposureHeight =
			screenHeight *
			exposureScale;

		const float exposureX =
			(screenWidth -
			 exposureWidth)
			*
			0.5f;

		const float exposureY =
			(screenHeight -
			 exposureHeight)
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

		if (!chronologyManager
				 .splitScreenClips
				 .empty())
		{
			ofVideoPlayer& splitVideo =
				chronologyManager
					.splitScreenClips[
						chronologyManager.currentSplitIndex
					]
					.video;

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

			const float videoWidth =
				splitVideo.getWidth();

			const float videoHeight =
				splitVideo.getHeight();

			if (videoWidth > 0.0f &&
				videoHeight > 0.0f)
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

				if (videoAspect >
					destinationAspect)
				{
					drawWidth =
						splitWidth;

					drawHeight =
						drawWidth /
						videoAspect;

					drawY =
						splitY +
						(splitHeight -
						 drawHeight)
						*
						0.5f;
				}
				else
				{
					drawHeight =
						splitHeight;

					drawWidth =
						drawHeight *
						videoAspect;

					drawX =
						splitX +
						(splitWidth -
						 drawWidth)
						*
						0.5f;
				}

				splitVideo.draw(
					drawX,
					drawY,
					drawWidth,
					drawHeight
				);
			}

			ofDisableBlendMode();
			ofSetColor(255);
		}

		return;
	}

	drawDryProcessedComposite(
		0,
		0,
		ofGetWidth(),
		ofGetHeight()
	);
}

//--------------------------------------------------------------
void ofApp::exit()
{
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
	chronologyManager.keyPressed(key);

	if (key == '4')
	{
		const bool turnOn =
			chronologyManager.splitScreenAmount <
			0.5f;

		chronologyManager.splitScreenAmount =
			turnOn
			?
			1.0f
			:
			0.0f;

		chronologyManager.toggleSplitScreen(
			turnOn
		);
	}

	if (key == '5')
	{
		visualState.triggerSiren();
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
void ofApp::applyVisualStateParameters()
{
	static bool previousBlurActive =
		false;


	static bool previousStepActive =
		false;


	// ============================================================
	// MOTION BLUR
	// ============================================================

	const float blurWeight =
		ofClamp(
			visualState
				.getMotionBlurWeight(),
			0.0f,
			1.0f
		);


	const bool blurControlActive =
		blurWeight >
		0.001f;


	motionBlur.setBlendFactor(
		visualState
			.getBlurPersistence()
	);


	motionBlur.setMixAmount(
		visualState
			.getBlurMix()
	);


	motionBlur.setStretchAmount(
		visualState
			.getBlurStretch()
	);


	// ============================================================
	// IMPORTANT
	//
	// Blur remains renderable briefly after controller input falls
	// to zero, while its accumulated trail dies.
	// ============================================================

	isMotionBlurActive =
		blurControlActive ||
		motionBlur.hasVisibleResidue();


	// ============================================================
	// STEP PRINT
	// ============================================================

	const float stepWeight =
		visualState
			.getStepPrintWeight();


	isStepActive =
		stepWeight >
		0.003f;


	stepPrinting.setStepInterval(
		visualState
			.getStepInterval()
	);


	stepPrinting.setMixAmount(
		visualState
			.getStepMix()
	);


	stepPrinting.setFeedbackAmount(
		visualState
			.getStepFeedback()
	);


	// ============================================================
	// FISHEYE
	// ============================================================

	const float fisheyeWeight =
		visualState
			.getFisheyeWeight();


	isBassActive =
		fisheyeWeight >
		0.003f;


	fisheye.setMaxDistortion(
		visualState
			.getFisheyeMaxDistortion()
	);


	fisheye.setBassLevel(
		visualState
			.getFisheyeBass()
	);


	fisheye.setDistortionStrength(
		visualState
			.getFisheyeDistortion()
	);


	fisheye.setPulseFrequency(
		visualState
			.getFisheyePulseFrequency()
	);


	// ============================================================
	// CLEAN UP AFTER RESIDUE HAS ACTUALLY FINISHED
	// ============================================================

	if (
		previousBlurActive &&
		!isMotionBlurActive
	)
	{
		motionBlur.clear();
	}


	if (
		previousStepActive &&
		!isStepActive
	)
	{
		stepPrinting.clearFrames();
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


	ofSetColor(
		255
	);


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


	const bool blurControlActive =
		blurWeight >
		0.001f;


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
	// THIS IS THE IMPORTANT CHANGE.
	//
	// Physical blur control active:
	// generate new trail material.
	//
	// Physical blur control inactive:
	// do NOT generate new trails;
	// simply decay what is already there.
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
	else if (
		motionBlur.hasVisibleResidue()
	)
	{
		motionBlur.decayOnly();
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
	// MOTION BLUR + STEP PRINT
	//
	// Only contaminate NEW StepPrint captures with MotionBlur while
	// the actual blur control is engaged.
	//
	// Residual blur by itself should not keep poisoning new prints.
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
			0.38f +
			pair *
				0.46f +
			mutation *
				0.10f;


		captureBlurAmount +=
			(
				drift -
				0.5f
			)
			*
			0.10f;


		captureBlurAmount =
			ofClamp(
				captureBlurAmount,
				0.30f,
				0.96f
			);


		blendFbo(
			videoFbo,
			routeAFbo,
			historySourceFbo,
			captureBlurAmount
		);
	}


	// ============================================================
	// STEP PRINT + BASS
	// ============================================================

	if (
		isBassActive
	)
	{
		fisheye.renderTo(
			historySourceFbo
				.getTexture(),
			preWarpFbo
		);


		const float pair =
			std::min(
				bassWeight,
				stepWeight
			);


		float captureWarpAmount =
			0.22f +
			pair *
				0.58f +
			mutation *
				0.10f;


		captureWarpAmount +=
			(
				drift -
				0.5f
			)
			*
			0.14f;


		captureWarpAmount =
			ofClamp(
				captureWarpAmount,
				0.16f,
				0.94f
			);


		blendFbo(
			historySourceFbo,
			preWarpFbo,
			mutationFbo,
			captureWarpAmount
		);


		copyFbo(
			mutationFbo,
			historySourceFbo
		);
	}


	// ============================================================
	// CAPTURE PRINT
	// ============================================================

	stepPrinting.update(
		historySourceFbo
			.getTexture()
	);
}

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::renderTemporalMutation(
	ofFbo& source,
	ofFbo& destination
)
{
	// ============================================================
	// CLEAN
	// ============================================================

	if (
		!isMotionBlurActive &&
		!isStepActive
	)
	{
		copyFbo(
			source,
			destination
		);


		return;
	}


	// ============================================================
	// MOTION BLUR ONLY
	// ============================================================

	if (
		isMotionBlurActive &&
		!isStepActive
	)
	{
		copyFbo(
			source,
			destination
		);


		motionBlur.apply(
			destination
		);


		return;
	}


	// ============================================================
	// STEP PRINT ONLY
	// ============================================================

	if (
		!isMotionBlurActive &&
		isStepActive
	)
	{
		copyFbo(
			source,
			destination
		);


		stepPrinting.apply(
			destination
		);


		return;
	}


	// ============================================================
	// MOTION BLUR + STEP PRINT
	//
	// The StepPrint history already contains partially blurred
	// captures because updateEffectHistories() changes what
	// StepPrint remembers.
	//
	// Here the live world itself moves between:
	//
	// current footage
	//     ↓
	// temporal drag
	//     ↓
	// printed smeared history
	//     ↓
	// dark temporal residue
	// ============================================================

	copyFbo(
		source,
		routeAFbo
	);


	motionBlur.apply(
		routeAFbo
	);


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


	const float overlap =
		std::min(
			blurWeight,
			stepWeight
		);


	const float direction =
		ofClamp(
			visualState
				.getTemporalDirection(),
			-1.0f,
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


	const float reverbDominance =
		ofMap(
			direction,
			-1.0f,
			1.0f,
			1.0f,
			0.0f,
			true
		);


	// ============================================================
	// LIVE SMEAR
	//
	// Stronger than the previous combination.
	// ============================================================

	float liveSmearAmount =
		0.34f +
		reverbDominance *
			0.38f +
		overlap *
			0.18f;


	liveSmearAmount +=
		(
			drift -
			0.5f
		)
		*
		mutation *
		0.12f;


	liveSmearAmount =
		ofClamp(
			liveSmearAmount,
			0.28f,
			0.94f
		);


	blendFbo(
		source,
		routeAFbo,
		destination,
		liveSmearAmount
	);


	// ============================================================
	// PRINT THE ALREADY-SMEARED HISTORY
	// ============================================================

	stepPrinting.apply(
		destination
	);


	// ============================================================
	// DARK TEMPORAL RESIDUE
	//
	// This is the part derived from the dark in-between state you
	// liked in the failed splice experiment.
	//
	// There are NO strips.
	// NO bands.
	// NO regions.
	//
	// We create a black field and ask MotionBlur to expose only
	// its accumulated temporal material.
	// ============================================================

	const float darkEntry =
		ofMap(
			overlap,
			0.18f,
			0.85f,
			0.0f,
			1.0f,
			true
		);


	if (
		darkEntry >
		0.001f
	)
	{
		mutationFbo.begin();


		ofClear(
			0,
			0,
			0,
			255
		);


		mutationFbo.end();


		motionBlur.apply(
			mutationFbo
		);


		// ========================================================
		// BREATHING / EVER-CHANGING PRESENCE
		// ========================================================

		const float breathing =
			0.62f +
			(
				drift -
				0.5f
			)
			*
			0.48f;


		float darkAmount =
			darkEntry *
			(
				0.08f +
				mutation *
					0.14f
			)
			*
			breathing;


		darkAmount =
			ofClamp(
				darkAmount,
				0.0f,
				0.22f
			);


		if (
			darkAmount >
			0.002f
		)
		{
			blendFbo(
				destination,
				mutationFbo,
				interactionScratchFbo,
				darkAmount
			);


			copyFbo(
				interactionScratchFbo,
				destination
			);
		}
	}
}
//--------------------------------------------------------------
void ofApp::applyInteractionMemory(ofFbo& target)
{
	const float mutation =
		visualState
			.getMutationAmount();

	if (mutation <= 0.003f)
	{
		if (interactionMemoryReady)
		{
			interactionMemoryFbo.begin();
			ofClear(0, 0, 0, 255);
			interactionMemoryFbo.end();

			interactionMemoryReady =
				false;
		}

		return;
	}

	if (!interactionMemoryReady)
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

	float memoryOverlay =
		ofLerp(
			0.055f,
			0.31f,
			mutation
		);

	memoryOverlay +=
		(drift -
		 0.5f)
		*
		0.075f;

	memoryOverlay =
		ofClamp(
			memoryOverlay,
			0.035f,
			0.34f
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

	float captureAmount =
		ofLerp(
			0.34f,
			0.12f,
			mutation
		);

	captureAmount +=
		(0.5f -
		 drift)
		*
		0.04f;

	captureAmount =
		ofClamp(
			captureAmount,
			0.10f,
			0.38f
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
	// CLEAN SOURCE
	// ============================================================

	videoFbo.begin();


	ofClear(
		0,
		0,
		0,
		255
	);


	ofSetColor(
		255
	);


	currentVideo->draw(
		0,
		0,
		standardWidth,
		standardHeight
	);


	videoFbo.end();


	// ============================================================
	// TEMPORAL WORLD
	// ============================================================

	renderTemporalMutation(
		videoFbo,
		intermediateFbo
	);


	// ============================================================
	// NO BASS
	// ============================================================

	if (
		!isBassActive
	)
	{
		copyFbo(
			intermediateFbo,
			processedFbo
		);


		return;
	}


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


	const bool hasTemporalEffect =
		isMotionBlurActive ||
		isStepActive;


	// ============================================================
	// BASS ONLY
	// ============================================================

	if (
		!hasTemporalEffect
	)
	{
		fisheye.renderTo(
			videoFbo
				.getTexture(),
			processedFbo
		);


		return;
	}


	// ============================================================
	// BASS + TEMPORAL EFFECTS
	//
	// The default combined state is now the LENS BENDING THE
	// TEMPORAL WORLD.
	//
	// This should feel considerably less like two effects being
	// crossfaded together.
	// ============================================================

	fisheye.renderTo(
		intermediateFbo
			.getTexture(),
		routeBFbo
	);


	copyFbo(
		routeBFbo,
		processedFbo
	);


	// ============================================================
	// RETAIN A TRACE OF PRESENT-TIME LENS AT WEAKER COMBINATIONS
	//
	// Only enough to preserve ancestry with the normal fisheye.
	// ============================================================

	fisheye.renderTo(
		videoFbo
			.getTexture(),
		routeAFbo
	);


	const float strongestTemporal =
		std::max(
			blurWeight,
			stepWeight
		);


	const float pairStrength =
		std::min(
			bassWeight,
			strongestTemporal
		);


	const float presentTrace =
		ofClamp(
			(
				1.0f -
				pairStrength
			)
			*
			0.24f,
			0.0f,
			0.24f
		);


	if (
		presentTrace >
		0.002f
	)
	{
		blendFbo(
			processedFbo,
			routeAFbo,
			mutationFbo,
			presentTrace
		);


		copyFbo(
			mutationFbo,
			processedFbo
		);
	}


	// ============================================================
	// MOTION BLUR + BASS
	//
	// LENS DRAG.
	//
	// The warped temporal world appears to possess inertia.
	// ============================================================

	if (
		isMotionBlurActive &&
		!isStepActive
	)
	{
		const float blurBass =
			std::min(
				blurWeight,
				bassWeight
			);


		const float lagAmount =
			blurBass *
			(
				0.35f +
				mutation *
					0.65f
			);


		if (
			lagAmount >
			0.02f
		)
		{
			interactionScratchFbo.begin();


			ofClear(
				0,
				0,
				0,
				255
			);


			ofSetColor(
				255
			);


			processedFbo.draw(
				0,
				0,
				standardWidth,
				standardHeight
			);


			ofEnableBlendMode(
				OF_BLENDMODE_SCREEN
			);


			const float direction =
				(
					drift -
					0.5f
				)
				*
				2.0f;


			const float displacement =
				direction *
				(
					12.0f +
					blurBass *
						42.0f
				);


			const float residueAlpha =
				lagAmount *
				86.0f;


			ofSetColor(
				255,
				255,
				255,
				residueAlpha
			);


			routeBFbo.draw(
				displacement,
				0,
				standardWidth,
				standardHeight
			);


			ofSetColor(
				255,
				255,
				255,
				residueAlpha *
					0.48f
			);


			routeBFbo.draw(
				displacement *
					-0.42f,
				displacement *
					0.12f,
				standardWidth,
				standardHeight
			);


			ofDisableBlendMode();


			ofSetColor(
				255
			);


			interactionScratchFbo.end();


			copyFbo(
				interactionScratchFbo,
				processedFbo
			);
		}
	}


	// ============================================================
	// ALL THREE
	//
	// Stronger version of the same dark temporal residue introduced
	// by MotionBlur + StepPrint.
	//
	// Bass bends it rather than introducing another unrelated
	// visual effect.
	// ============================================================

	if (
		isMotionBlurActive &&
		isStepActive
	)
	{
		const float tripleStrength =
			std::min(
				bassWeight,
				std::min(
					blurWeight,
					stepWeight
				)
			);


		const float thresholdedTriple =
			ofMap(
				tripleStrength,
				0.24f,
				1.0f,
				0.0f,
				1.0f,
				true
			);


		if (
			thresholdedTriple >
			0.001f
		)
		{
			mutationFbo.begin();


			ofClear(
				0,
				0,
				0,
				255
			);


			mutationFbo.end();


			motionBlur.apply(
				mutationFbo
			);


			fisheye.renderTo(
				mutationFbo
					.getTexture(),
				preWarpFbo
			);


			const float breathing =
				0.62f +
				(
					drift -
					0.5f
				)
				*
				0.50f;


			float undertowAmount =
				thresholdedTriple *
				(
					0.08f +
					mutation *
						0.22f
				)
				*
				breathing;


			undertowAmount =
				ofClamp(
					undertowAmount,
					0.0f,
					0.28f
				);


			if (
				undertowAmount >
				0.002f
			)
			{
				blendFbo(
					processedFbo,
					preWarpFbo,
					interactionScratchFbo,
					undertowAmount
				);


				copyFbo(
					interactionScratchFbo,
					processedFbo
				);
			}
		}
	}
}

//--------------------------------------------------------------
//--------------------------------------------------------------
void ofApp::drawDryProcessedComposite(
	float x,
	float y,
	float width,
	float height
)
{
	const bool anythingActive =
		isMotionBlurActive ||
		isStepActive ||
		isBassActive;


	ofSetColor(255);


	// ============================================================
	// PROCESSED WORLD
	//
	// When an effect is active, this IS the image.
	//
	// Do not put untouched video underneath Fisheye.
	// Do not dilute combinations with a separate dry layer here.
	// ============================================================

	if (
		anythingActive
	)
	{
		processedFbo.draw(
			x,
			y,
			width,
			height
		);


		return;
	}


	// ============================================================
	// CLEAN DOCUMENTARY WORLD
	// ============================================================

	videoFbo.draw(
		x,
		y,
		width,
		height
	);
}
