#include "VisualStateManager.hpp"


//--------------------------------------------------------------
void VisualStateManager::setup()
{
	reverbPresence = 0.0f;
	delayPresence = 0.0f;
	bassPresence = 0.0f;

	processedAmount =
		dubCrossfader;

	reverbTouch = 0.0f;
	delayTouch = 0.0f;
	bassTouch = 0.0f;

	reverbGesture = 0.0f;
	delayGesture = 0.0f;
	bassGesture = 0.0f;

	reverbToDelayAccent = 0.0f;
	delayToReverbAccent = 0.0f;

	temporalDirection = 0.0f;
	temporalOverlap = 0.0f;

	blurToStepMorph = 0.0f;
	stepToBlurMorph = 0.0f;

	mutationAmount = 0.0f;
	mutationDrift = 0.5f;

	organicPhase =
		ofRandom(
			0.0f,
			1000.0f
		);

	organicBassModulation =
		1.0f;

	sirenImpulse =
		0.0f;

	blurWeight =
		0.0f;

	stepWeight =
		0.0f;

	fisheyeWeight =
		0.0f;

	dominantFamily =
		Family::CLEAN;
}


//--------------------------------------------------------------
float VisualStateManager::smoothTowards(
	float current,
	float target,
	float attack,
	float release,
	float deltaTime
) const
{
	const float speed =
		(
			target >
			current
		)
		?
		attack
		:
		release;


	const float amount =
		1.0f -
		std::exp(
			-speed *
			deltaTime
		);


	return ofLerp(
		current,
		target,
		amount
	);
}


//--------------------------------------------------------------
float VisualStateManager::visualCurve(
	float value
) const
{
	value =
		ofClamp(
			value,
			0.0f,
			1.0f
		);


	if (
		value <
		0.012f
	)
	{
		return 0.0f;
	}


	value =
		ofMap(
			value,
			0.012f,
			1.0f,
			0.0f,
			1.0f,
			true
		);


	// Very intentional early response.
	return pow(
		value,
		0.30f
	);
}


//--------------------------------------------------------------
float VisualStateManager::gestureAmount(
	float oldValue,
	float newValue,
	float strength
) const
{
	return ofClamp(
		std::abs(
			newValue -
			oldValue
		)
		*
		strength,
		0.0f,
		1.0f
	);
}


//--------------------------------------------------------------
float VisualStateManager::wakeAmount(
	float oldValue,
	float newValue,
	float minimumWake,
	float sensitivity
) const
{
	const float delta =
		std::abs(
			newValue -
			oldValue
		);


	if (
		delta <
		0.0005f
	)
	{
		return 0.0f;
	}


	return ofClamp(
		minimumWake +
		delta *
		sensitivity,
		minimumWake,
		1.0f
	);
}


//--------------------------------------------------------------
void VisualStateManager::update(
	float deltaTime
)
{
	deltaTime =
		ofClamp(
			deltaTime,
			0.001f,
			0.1f
		);


	// ============================================================
	// REVERB
	// ============================================================

	const float wetDrive =
		visualCurve(
			reverbWet
		);


	const float roomDrive =
		visualCurve(
			reverbRoom
		);


	// Width and Damping are bipolar around the default 0.5.
	//
	// Moving EITHER away from the starting position should visibly
	// wake MotionBlur.
	const float widthMovement =
		ofClamp(
			abs(
				reverbWidth -
				0.5f
			)
			*
			2.0f,
			0.0f,
			1.0f
		);


	const float dampingMovement =
		ofClamp(
			abs(
				reverbDamping -
				0.5f
			)
			*
			2.0f,
			0.0f,
			1.0f
		);


	const float widthDrive =
		visualCurve(
			widthMovement
		)
		*
		0.82f;


	const float dampingDrive =
		visualCurve(
			dampingMovement
		)
		*
		0.82f;


	float reverbTarget =
		std::max(
			wetDrive,
			roomDrive *
				0.88f
		);


	reverbTarget =
		std::max(
			reverbTarget,
			widthDrive
		);


	reverbTarget =
		std::max(
			reverbTarget,
			dampingDrive
		);


	reverbTarget =
		std::max(
			reverbTarget,
			reverbTouch
		);


	reverbTarget +=
		reverbGesture *
		0.18f;


	reverbTarget +=
		delayToReverbAccent *
		0.16f;


	reverbTarget =
		ofClamp(
			reverbTarget,
			0.0f,
			1.0f
		);


	// ============================================================
	// DELAY
	// ============================================================

	const float sendDrive =
		visualCurve(
			dubSend
		);


	const float feedbackDrive =
		visualCurve(
			delayFeedback
		);


	// 320ms is our neutral/default delay-time state.
	//
	// Moving Delay Time away from there can now sustain StepPrint.
	const float delayTimeDifference =
		ofClamp(
			std::abs(
				delayTimeMs -
				320.0f
			)
			/
			900.0f,
			0.0f,
			1.0f
		);


	const float timeDrive =
		visualCurve(
			delayTimeDifference
		)
		*
		0.80f;


	float delayTarget =
		std::max(
			sendDrive,
			feedbackDrive *
				0.84f
		);


	delayTarget =
		std::max(
			delayTarget,
			timeDrive
		);


	delayTarget =
		std::max(
			delayTarget,
			delayTouch
		);


	delayTarget +=
		delayGesture *
		0.16f;


	delayTarget +=
		reverbToDelayAccent *
		0.16f;


	delayTarget =
		ofClamp(
			delayTarget,
			0.0f,
			1.0f
		);


	// ============================================================
	// BASS
	// ============================================================

	float bassTarget =
		std::max(
			visualCurve(
				bass
			),
			bassTouch *
				0.82f
		);


	bassTarget +=
		bassGesture *
		0.10f;


	bassTarget =
		ofClamp(
			bassTarget,
			0.0f,
			1.0f
		);


	// ============================================================
	// INTENSITY-DEPENDENT DECAY
	// ============================================================
	//
	// IMPORTANT:
	//
	// coming down from 0.9 now hangs much longer than coming down
	// from 0.2.
	// ============================================================

	const float reverbRelease =
		ofLerp(
			2.20f,
			0.11f,
			pow(
				ofClamp(
					reverbPresence,
					0.0f,
					1.0f
				),
				0.78f
			)
		);


	const float delayRelease =
		ofLerp(
			2.20f,
			0.12f,
			pow(
				ofClamp(
					delayPresence,
					0.0f,
					1.0f
				),
				0.78f
			)
		);


	const float bassRelease =
		ofLerp(
			2.0f,
			0.55f,
			pow(
				ofClamp(
					bassPresence,
					0.0f,
					1.0f
				),
				0.78f
			)
		);


	reverbPresence =
		smoothTowards(
			reverbPresence,
			reverbTarget,
			8.5f,
			reverbRelease,
			deltaTime
		);


	delayPresence =
		smoothTowards(
			delayPresence,
			delayTarget,
			8.5f,
			delayRelease,
			deltaTime
		);


	bassPresence =
		smoothTowards(
			bassPresence,
			bassTarget,
			8.0f,
			bassRelease,
			deltaTime
		);


	processedAmount =
		smoothTowards(
			processedAmount,
			dubCrossfader,
			7.0f,
			4.0f,
			deltaTime
		);


	reverbTouch =
		smoothTowards(
			reverbTouch,
			0.0f,
			12.0f,
			0.95f,
			deltaTime
		);


	delayTouch =
		smoothTowards(
			delayTouch,
			0.0f,
			12.0f,
			0.95f,
			deltaTime
		);


	bassTouch =
		smoothTowards(
			bassTouch,
			0.0f,
			12.0f,
			1.5f,
			deltaTime
		);


	reverbGesture =
		smoothTowards(
			reverbGesture,
			0.0f,
			12.0f,
			2.2f,
			deltaTime
		);


	delayGesture =
		smoothTowards(
			delayGesture,
			0.0f,
			12.0f,
			2.2f,
			deltaTime
		);


	bassGesture =
		smoothTowards(
			bassGesture,
			0.0f,
			12.0f,
			2.6f,
			deltaTime
		);


	reverbToDelayAccent =
		smoothTowards(
			reverbToDelayAccent,
			0.0f,
			14.0f,
			1.0f,
			deltaTime
		);


	delayToReverbAccent =
		smoothTowards(
			delayToReverbAccent,
			0.0f,
			14.0f,
			1.0f,
			deltaTime
		);


	sirenImpulse =
		smoothTowards(
			sirenImpulse,
			0.0f,
			15.0f,
			1.6f,
			deltaTime
		);


	updateHierarchy();


	// ============================================================
	// TEMPORAL MORPH
	// ============================================================

	float directionTarget =
		delayPresence -
		reverbPresence;


	directionTarget +=
		reverbToDelayAccent *
		0.70f;


	directionTarget -=
		delayToReverbAccent *
		0.70f;


	directionTarget =
		ofClamp(
			directionTarget,
			-1.0f,
			1.0f
		);


	const float directionEase =
		1.0f -
		exp(
			-2.3f *
			deltaTime
		);


	temporalDirection =
		ofLerp(
			temporalDirection,
			directionTarget,
			directionEase
		);


	temporalOverlap =
		ofClamp(
			std::min(
				reverbPresence,
				delayPresence
			),
			0.0f,
			1.0f
		);


	const float overlapCharacter =
		visualCurve(
			temporalOverlap
		);


	blurToStepMorph =
		overlapCharacter *
		(
			0.30f +
			std::max(
				0.0f,
				temporalDirection
			)
			*
			0.70f
		);


	blurToStepMorph =
		ofClamp(
			blurToStepMorph +
			reverbToDelayAccent *
				0.40f,
			0.0f,
			1.0f
		);


	stepToBlurMorph =
		overlapCharacter *
		(
			0.30f +
			std::max(
				0.0f,
				-temporalDirection
			)
			*
			0.70f
		);


	stepToBlurMorph =
		ofClamp(
			stepToBlurMorph +
			delayToReverbAccent *
				0.40f,
			0.0f,
			1.0f
		);


	// ============================================================
	// HYBRID MUTATION STATE
	// ============================================================

	float mutationTarget =
		std::max(
			std::min(
				reverbPresence,
				delayPresence
			),
			std::max(
				std::min(
					bassPresence,
					reverbPresence
				),
				std::min(
					bassPresence,
					delayPresence
				)
			)
		);


	mutationTarget =
		visualCurve(
			mutationTarget
		);


	const float mutationRelease =
		ofLerp(
			1.4f,
			0.20f,
			mutationAmount
		);


	mutationAmount =
		smoothTowards(
			mutationAmount,
			mutationTarget,
			6.0f,
			mutationRelease,
			deltaTime
		);


	organicPhase +=
		deltaTime *
		(
			0.42f +
			mutationAmount *
				0.9f +
			bassPresence *
				0.4f
		);


	mutationDrift =
		ofNoise(
			organicPhase *
				0.31f +
			reverbPresence *
				1.7f +
			delayPresence *
				3.1f +
			bassPresence *
				5.2f
		);


	const float modulationDepth =
		0.05f +
		mutationAmount *
			0.10f;


	organicBassModulation =
		1.0f +
		sin(
			organicPhase *
			1.2f
		)
		*
		modulationDepth;


	organicBassModulation =
		ofClamp(
			organicBassModulation,
			0.78f,
			1.15f
		);
}


//--------------------------------------------------------------
void VisualStateManager::updateHierarchy()
{
	blurWeight =
		ofClamp(
			reverbPresence,
			0.0f,
			1.0f
		);


	stepWeight =
		ofClamp(
			delayPresence,
			0.0f,
			1.0f
		);


	fisheyeWeight =
		ofClamp(
			bassPresence,
			0.0f,
			1.0f
		);


	if (
		blurWeight <
			0.003f &&
		stepWeight <
			0.003f &&
		fisheyeWeight <
			0.003f
	)
	{
		dominantFamily =
			Family::CLEAN;

		return;
	}


	if (
		blurWeight >=
			stepWeight &&
		blurWeight >=
			fisheyeWeight
	)
	{
		dominantFamily =
			Family::REVERB;
	}
	else if (
		stepWeight >=
			blurWeight &&
		stepWeight >=
			fisheyeWeight
	)
	{
		dominantFamily =
			Family::DELAY;
	}
	else
	{
		dominantFamily =
			Family::BASS;
	}
}


//--------------------------------------------------------------
void VisualStateManager::setReverbRoom(
	float value
)
{
	value =
		ofClamp(
			value,
			0.0f,
			1.0f
		);


	reverbTouch =
		std::max(
			reverbTouch,
			wakeAmount(
				reverbRoom,
				value,
				0.62f,
				5.0f
			)
		);


	reverbGesture =
		std::max(
			reverbGesture,
			gestureAmount(
				reverbRoom,
				value,
				2.5f
			)
		);


	reverbRoom =
		value;
}


//--------------------------------------------------------------
void VisualStateManager::setReverbWet(
	float value
)
{
	value =
		ofClamp(
			value,
			0.0f,
			1.0f
		);


	reverbTouch =
		std::max(
			reverbTouch,
			wakeAmount(
				reverbWet,
				value,
				0.64f,
				5.2f
			)
		);


	reverbGesture =
		std::max(
			reverbGesture,
			gestureAmount(
				reverbWet,
				value,
				2.8f
			)
		);


	reverbWet =
		value;
}


//--------------------------------------------------------------
void VisualStateManager::setReverbDamping(
	float value
)
{
	value =
		ofClamp(
			value,
			0.0f,
			1.0f
		);


	reverbTouch =
		std::max(
			reverbTouch,
			wakeAmount(
				reverbDamping,
				value,
				0.62f,
				5.0f
			)
		);


	reverbGesture =
		std::max(
			reverbGesture,
			gestureAmount(
				reverbDamping,
				value,
				2.5f
			)
		);


	reverbDamping =
		value;
}


//--------------------------------------------------------------
void VisualStateManager::setReverbWidth(
	float value
)
{
	value =
		ofClamp(
			value,
			0.0f,
			1.0f
		);


	reverbTouch =
		std::max(
			reverbTouch,
			wakeAmount(
				reverbWidth,
				value,
				0.62f,
				5.0f
			)
		);


	reverbGesture =
		std::max(
			reverbGesture,
			gestureAmount(
				reverbWidth,
				value,
				2.5f
			)
		);


	reverbWidth =
		value;
}


//--------------------------------------------------------------
void VisualStateManager::setDelayTime(
	float milliseconds
)
{
	milliseconds =
		ofClamp(
			milliseconds,
			50.0f,
			2000.0f
		);


	const float oldValue =
		ofMap(
			delayTimeMs,
			50.0f,
			2000.0f,
			0.0f,
			1.0f,
			true
		);


	const float newValue =
		ofMap(
			milliseconds,
			50.0f,
			2000.0f,
			0.0f,
			1.0f,
			true
		);


	delayTouch =
		std::max(
			delayTouch,
			wakeAmount(
				oldValue,
				newValue,
				0.64f,
				5.0f
			)
		);


	delayGesture =
		std::max(
			delayGesture,
			gestureAmount(
				oldValue,
				newValue,
				2.5f
			)
		);


	delayTimeMs =
		milliseconds;
}


//--------------------------------------------------------------
void VisualStateManager::setDelayFeedback(
	float value
)
{
	value =
		ofClamp(
			value,
			0.0f,
			1.0f
		);


	delayTouch =
		std::max(
			delayTouch,
			wakeAmount(
				delayFeedback,
				value,
				0.64f,
				5.0f
			)
		);


	delayGesture =
		std::max(
			delayGesture,
			gestureAmount(
				delayFeedback,
				value,
				2.6f
			)
		);


	delayFeedback =
		value;
}


//--------------------------------------------------------------
void VisualStateManager::setDubSend(
	float value
)
{
	value =
		ofClamp(
			value,
			0.0f,
			1.0f
		);


	delayTouch =
		std::max(
			delayTouch,
			wakeAmount(
				dubSend,
				value,
				0.66f,
				5.2f
			)
		);


	delayGesture =
		std::max(
			delayGesture,
			gestureAmount(
				dubSend,
				value,
				2.9f
			)
		);


	dubSend =
		value;
}


//--------------------------------------------------------------
void VisualStateManager::setBass(
	float value
)
{
	value =
		ofClamp(
			value,
			0.0f,
			1.0f
		);


	bassTouch =
		std::max(
			bassTouch,
			wakeAmount(
				bass,
				value,
				0.42f,
				3.5f
			)
		);


	bassGesture =
		std::max(
			bassGesture,
			gestureAmount(
				bass,
				value,
				1.8f
			)
		);


	bass =
		value;
}


//--------------------------------------------------------------
void VisualStateManager::setMids(
	float value
)
{
	mids =
		ofClamp(
			value,
			0.0f,
			1.0f
		);
}


//--------------------------------------------------------------
void VisualStateManager::setTops(
	float value
)
{
	tops =
		ofClamp(
			value,
			0.0f,
			1.0f
		);
}


//--------------------------------------------------------------
void VisualStateManager::setDubCrossfader(
	float value
)
{
	dubCrossfader =
		ofClamp(
			value,
			0.0f,
			1.0f
		);
}


//--------------------------------------------------------------
void VisualStateManager::setSirenPitch(
	float value
)
{
	sirenPitch =
		ofClamp(
			value,
			0.0f,
			1.0f
		);
}


//--------------------------------------------------------------
void VisualStateManager::triggerSiren()
{
	sirenImpulse =
		1.0f;
}


//--------------------------------------------------------------
float VisualStateManager::getMotionBlurWeight() const
{
	return blurWeight;
}


//--------------------------------------------------------------
float VisualStateManager::getStepPrintWeight() const
{
	return stepWeight;
}


//--------------------------------------------------------------
float VisualStateManager::getFisheyeWeight() const
{
	return fisheyeWeight;
}


//--------------------------------------------------------------
float VisualStateManager::getProcessedAmount() const
{
	return ofClamp(
		processedAmount,
		0.0f,
		1.0f
	);
}


//--------------------------------------------------------------
float VisualStateManager::getTemporalDirection() const
{
	return temporalDirection;
}


//--------------------------------------------------------------
float VisualStateManager::getBlurToStepMorph() const
{
	return blurToStepMorph;
}


//--------------------------------------------------------------
float VisualStateManager::getStepToBlurMorph() const
{
	return stepToBlurMorph;
}


//--------------------------------------------------------------
float VisualStateManager::getTemporalOverlap() const
{
	return temporalOverlap;
}


//--------------------------------------------------------------
float VisualStateManager::getMutationAmount() const
{
	return mutationAmount;
}


//--------------------------------------------------------------
float VisualStateManager::getMutationDrift() const
{
	return mutationDrift;
}


//--------------------------------------------------------------
float VisualStateManager::getBlurPersistence() const
{
	if (
		blurWeight <
		0.001f
	)
	{
		return 0.0f;
	}


	return ofMap(
		reverbRoom,
		0.0f,
		1.0f,
		0.80f,
		0.994f,
		true
	);
}
//--------------------------------------------------------------
float VisualStateManager::getBlurMix() const
{
	if (
		blurWeight <
		0.001f
	)
	{
		return 0.0f;
	}


	const float wet =
		visualCurve(
			reverbWet
		);


	// If reverb has woken MotionBlur at all,
	// don't let its rendered opacity be microscopic.
	float value =
		0.48f +
		visualCurve(
			blurWeight
		)
		*
		0.27f;


	// Wet then takes it from obvious to extreme.
	value +=
		wet *
		0.24f;


	return ofClamp(
		value,
		0.0f,
		0.99f
	);
}

//--------------------------------------------------------------
float VisualStateManager::getBlurStretch() const
{
	if (
		blurWeight <
		0.001f
	)
	{
		return 0.0f;
	}


	return ofMap(
		reverbWidth,
		0.0f,
		1.0f,
		12.0f,
		150.0f,
		true
	);
}

float VisualStateManager::getBlurSoftness() const
{
	if (
		blurWeight <
		0.003f
	)
	{
		return 0.0f;
	}


	// Damping is visually translated into loss of definition.
	//
	// Low:
	// narrower, harder streak.
	//
	// High:
	// broad, soft, diffused residue.
	return pow(
		ofClamp(
			reverbDamping,
			0.0f,
			1.0f
		),
		0.72f
	);
}

//--------------------------------------------------------------
int VisualStateManager::getStepInterval() const
{
	const float sendIntensity =
		ofClamp(
			dubSend,
			0.0f,
			1.0f
		);


	const float feedbackIntensity =
		ofClamp(
			delayFeedback,
			0.0f,
			1.0f
		);


	const float timeIntensity =
		ofMap(
			delayTimeMs,
			50.0f,
			2000.0f,
			0.0f,
			1.0f,
			true
		);


	float intensity =
		std::max(
			sendIntensity,
			feedbackIntensity
		);


	intensity =
		std::max(
			intensity,
			timeIntensity *
			0.72f
		);


	// Starts fairly gentle but collapses HARD
	// towards the upper half.
	intensity =
		pow(
			intensity,
			1.18f
		);


	const float interval =
		2.0f +
		intensity *
			42.0f +
		timeIntensity *
			intensity *
			4.0f;


	return ofClamp(
		static_cast<int>(
			round(
				interval
			)
		),
		2,
		48
	);
}

//--------------------------------------------------------------
float VisualStateManager::getStepMix() const
{
	if (
		stepWeight <
		0.003f
	)
	{
		return 0.0f;
	}


	float value =
		0.48f +
		visualCurve(
			stepWeight
		)
		*
		0.50f;


	return ofClamp(
		value,
		0.0f,
		1.0f
	);
}


//--------------------------------------------------------------
float VisualStateManager::getStepFeedback() const
{
	if (
		stepWeight <
		0.003f
	)
	{
		return 0.0f;
	}


	return ofClamp(
		visualCurve(
			delayFeedback
		)
		+
		mutationAmount *
			0.08f,
		0.0f,
		1.0f
	);
}


//--------------------------------------------------------------
float VisualStateManager::getFisheyeBass() const
{
	if (
		fisheyeWeight <
		0.003f
	)
	{
		return 0.0f;
	}


	return ofClamp(
		visualCurve(
			fisheyeWeight
		)
		*
		organicBassModulation,
		0.0f,
		0.94f
	);
}


//--------------------------------------------------------------
float VisualStateManager::getFisheyeDistortion() const
{
	if (
		fisheyeWeight <
		0.003f
	)
	{
		return 0.0f;
	}


	return ofClamp(
		0.05f +
		visualCurve(
			fisheyeWeight
		)
		*
		0.72f,
		0.0f,
		1.05f
	);
}


//--------------------------------------------------------------
float VisualStateManager::getFisheyePulseFrequency() const
{
	return ofMap(
		visualCurve(
			fisheyeWeight
		),
		0.0f,
		1.0f,
		0.35f,
		4.2f,
		true
	);
}


//--------------------------------------------------------------
float VisualStateManager::getFisheyeMaxDistortion() const
{
	return ofLerp(
		1.75f,
		2.55f,
		visualCurve(
			fisheyeWeight
		)
	);
}



//--------------------------------------------------------------
VisualStateManager::Family
VisualStateManager::getDominantFamily() const
{
	return dominantFamily;
}
