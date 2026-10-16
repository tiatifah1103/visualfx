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
	// INTENSITY CURVE
	//
	// IMPORTANT:
	//
	// The old visualCurve() is intentionally extremely eager:
	//
	// pow(value, 0.30)
	//
	// That is useful in some places but makes 0.15 and 0.80 look
	// much too similar.
	//
	// For FAMILY STRENGTH we use a slower curve so controller
	// travel actually has a visible minimum -> maximum progression.
	// ============================================================

	auto familyCurve =
		[](float value)
		{
			value =
				ofClamp(
					value,
					0.0f,
					1.0f
				);

			if (
				value <
				0.015f
			)
			{
				return 0.0f;
			}

			value =
				ofMap(
					value,
					0.015f,
					1.0f,
					0.0f,
					1.0f,
					true
				);

			return pow(
				value,
				1.20f
			);
		};


	// ============================================================
	// REVERB / MOTION BLUR
	// ============================================================

	const float wetDrive =
		familyCurve(
			reverbWet
		);

	const float roomDrive =
		familyCurve(
			reverbRoom
		);

	const float widthDrive =
		familyCurve(
			reverbWidth
		);

	const float dampingDrive =
		familyCurve(
			reverbDamping
		);


	// Minimum on ALL Reverb controls must mean OFF.
	//
	// We no longer interpret width / damping as bipolar around 0.5.
	const bool reverbControlsOff =
		reverbWet <
			0.015f &&
		reverbRoom <
			0.015f &&
		reverbWidth <
			0.015f &&
		reverbDamping <
			0.015f;


	float reverbTarget =
		std::max(
			wetDrive,
			roomDrive *
				0.88f
		);

	reverbTarget =
		std::max(
			reverbTarget,
			widthDrive *
				0.78f
		);

	reverbTarget =
		std::max(
			reverbTarget,
			dampingDrive *
				0.62f
		);


	// Controller movement can still give a tiny expressive bump,
	// but it must never dominate the actual knob position.
	if (
		!reverbControlsOff
	)
	{
		reverbTarget =
			std::max(
				reverbTarget,
				reverbTouch *
					0.18f
			);

		reverbTarget +=
			reverbGesture *
			0.055f;

		reverbTarget +=
			delayToReverbAccent *
			0.06f;
	}
	else
	{
		// Turning DOWN must not create another effect impulse.
		reverbTouch =
			0.0f;

		reverbGesture =
			0.0f;

		delayToReverbAccent =
			0.0f;

		reverbTarget =
			0.0f;
	}


	reverbTarget =
		ofClamp(
			reverbTarget,
			0.0f,
			1.0f
		);


	// ============================================================
	// DELAY / STEP PRINT
	// ============================================================

	const float sendDrive =
		familyCurve(
			dubSend
		);

	const float feedbackDrive =
		familyCurve(
			delayFeedback
		);


	// Delay time modifies an active Delay.
	//
	// It does NOT activate StepPrint by itself.
	const float delayTimeNormalised =
		ofMap(
			delayTimeMs,
			90.0f,
			900.0f,
			0.0f,
			1.0f,
			true
		);


	const bool delayControlsOff =
		dubSend <
			0.015f &&
		delayFeedback <
			0.015f;


	float delayTarget =
		std::max(
			sendDrive,
			feedbackDrive *
				0.86f
		);


	if (
		!delayControlsOff
	)
	{
		// Delay time shapes an already-active delay state.
		delayTarget *=
			(
				0.74f +
				delayTimeNormalised *
					0.26f
			);


		delayTarget =
			std::max(
				delayTarget,
				delayTouch *
					0.16f
			);


		delayTarget +=
			delayGesture *
			0.05f;


		delayTarget +=
			reverbToDelayAccent *
			0.06f;
	}
	else
	{
		delayTouch =
			0.0f;

		delayGesture =
			0.0f;

		reverbToDelayAccent =
			0.0f;

		delayTarget =
			0.0f;
	}


	delayTarget =
		ofClamp(
			delayTarget,
			0.0f,
			1.0f
		);


	// ============================================================
	// BASS
	// ============================================================

	const float bassDrive =
		familyCurve(
			bass
		);


	const bool bassControlsOff =
		bass <
		0.015f;


	float bassTarget =
		bassDrive;


	if (
		!bassControlsOff
	)
	{
		bassTarget =
			std::max(
				bassTarget,
				bassTouch *
					0.16f
			);


		bassTarget +=
			bassGesture *
			0.045f;
	}
	else
	{
		bassTouch =
			0.0f;

		bassGesture =
			0.0f;

		bassTarget =
			0.0f;
	}


	bassTarget =
		ofClamp(
			bassTarget,
			0.0f,
			1.0f
		);


	// ============================================================
	// PRESENCE SMOOTHING
	//
	// FAST, READABLE RELEASE.
	//
	// We still ease away rather than snapping instantly.
	//
	// But once somebody physically turns a control down, the work
	// should visibly start returning toward clean immediately.
	// ============================================================

	reverbPresence =
		smoothTowards(
			reverbPresence,
			reverbTarget,
			7.5f,
			6.2f,
			deltaTime
		);


	delayPresence =
		smoothTowards(
			delayPresence,
			delayTarget,
			7.5f,
			6.5f,
			deltaTime
		);


	bassPresence =
		smoothTowards(
			bassPresence,
			bassTarget,
			7.0f,
			6.8f,
			deltaTime
		);


	// ============================================================
	// DECISIVE CLEAN SNAP
	//
	// Prevent microscopic smoothed values keeping the rendering
	// branches alive forever.
	// ============================================================

	if (
		reverbControlsOff &&
		reverbPresence <
		0.018f
	)
	{
		reverbPresence =
			0.0f;
	}


	if (
		delayControlsOff &&
		delayPresence <
		0.018f
	)
	{
		delayPresence =
			0.0f;
	}


	if (
		bassControlsOff &&
		bassPresence <
		0.018f
	)
	{
		bassPresence =
			0.0f;
	}


	// ============================================================
	// DUB CROSSFADER
	// ============================================================

	processedAmount =
		smoothTowards(
			processedAmount,
			dubCrossfader,
			7.0f,
			5.0f,
			deltaTime
		);


	// ============================================================
	// TRANSIENTS
	//
	// These are now genuinely transient.
	// ============================================================

	reverbTouch =
		smoothTowards(
			reverbTouch,
			0.0f,
			12.0f,
			7.0f,
			deltaTime
		);


	delayTouch =
		smoothTowards(
			delayTouch,
			0.0f,
			12.0f,
			7.0f,
			deltaTime
		);


	bassTouch =
		smoothTowards(
			bassTouch,
			0.0f,
			12.0f,
			7.0f,
			deltaTime
		);


	reverbGesture =
		smoothTowards(
			reverbGesture,
			0.0f,
			12.0f,
			7.5f,
			deltaTime
		);


	delayGesture =
		smoothTowards(
			delayGesture,
			0.0f,
			12.0f,
			7.5f,
			deltaTime
		);


	bassGesture =
		smoothTowards(
			bassGesture,
			0.0f,
			12.0f,
			7.5f,
			deltaTime
		);


	reverbToDelayAccent =
		smoothTowards(
			reverbToDelayAccent,
			0.0f,
			14.0f,
			7.0f,
			deltaTime
		);


	delayToReverbAccent =
		smoothTowards(
			delayToReverbAccent,
			0.0f,
			14.0f,
			7.0f,
			deltaTime
		);


	sirenImpulse =
		smoothTowards(
			sirenImpulse,
			0.0f,
			15.0f,
			3.0f,
			deltaTime
		);


	// ============================================================
	// FAMILY HIERARCHY
	// ============================================================

	updateHierarchy();


	// ============================================================
	// TEMPORAL DIRECTION
	// ============================================================

	float directionTarget =
		delayPresence -
		reverbPresence;


	directionTarget +=
		reverbToDelayAccent *
		0.42f;


	directionTarget -=
		delayToReverbAccent *
		0.42f;


	directionTarget =
		ofClamp(
			directionTarget,
			-1.0f,
			1.0f
		);


	const float directionEase =
		1.0f -
		exp(
			-5.5f *
			deltaTime
		);


	temporalDirection =
		ofLerp(
			temporalDirection,
			directionTarget,
			directionEase
		);


	// ============================================================
	// TEMPORAL OVERLAP
	// ============================================================

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
		familyCurve(
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
				0.20f,
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
				0.20f,
			0.0f,
			1.0f
		);


	// ============================================================
	// HYBRID MUTATION
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
		familyCurve(
			mutationTarget
		);


	// No two families active:
	// kill mutation rather than preserving some vague memory state.
	const int activeFamilies =
		(
			reverbPresence >
			0.02f
			?
			1
			:
			0
		)
		+
		(
			delayPresence >
			0.02f
			?
			1
			:
			0
		)
		+
		(
			bassPresence >
			0.02f
			?
			1
			:
			0
		);


	if (
		activeFamilies <
		2
	)
	{
		mutationTarget =
			0.0f;
	}


	mutationAmount =
		smoothTowards(
			mutationAmount,
			mutationTarget,
			6.0f,
			7.0f,
			deltaTime
		);


	if (
		mutationTarget <=
			0.001f &&
		mutationAmount <
			0.015f
	)
	{
		mutationAmount =
			0.0f;
	}


	// ============================================================
	// ORGANIC DRIFT
	// ============================================================

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
		0.025f +
		mutationAmount *
			0.075f;


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
			0.84f,
			1.12f
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
//--------------------------------------------------------------
float VisualStateManager::getBlurPersistence() const
{
	if (
		blurWeight <
		0.01f
	)
	{
		return 0.0f;
	}


	const float intensity =
		pow(
			ofClamp(
				blurWeight,
				0.0f,
				1.0f
			),
			1.25f
		);


	const float room =
		ofClamp(
			reverbRoom,
			0.0f,
			1.0f
		);


	const float damping =
		ofClamp(
			reverbDamping,
			0.0f,
			1.0f
		);


	float persistence =
		0.20f +
		intensity *
			0.68f +
		room *
			intensity *
			0.10f;


	// More damping slightly shortens the visible history.
	persistence -=
		damping *
		intensity *
		0.06f;


	return ofClamp(
		persistence,
		0.0f,
		0.97f
	);
}


//--------------------------------------------------------------
float VisualStateManager::getBlurMix() const
{
	if (
		blurWeight <
		0.01f
	)
	{
		return 0.0f;
	}


	const float intensity =
		pow(
			ofClamp(
				blurWeight,
				0.0f,
				1.0f
			),
			1.35f
		);


	const float wet =
		ofClamp(
			reverbWet,
			0.0f,
			1.0f
		);


	const float room =
		ofClamp(
			reverbRoom,
			0.0f,
			1.0f
		);


	const float width =
		ofClamp(
			reverbWidth,
			0.0f,
			1.0f
		);


	const float damping =
		ofClamp(
			reverbDamping,
			0.0f,
			1.0f
		);


	const float parameterCharacter =
		std::max(
			wet,
			std::max(
				room * 0.85f,
				std::max(
					width * 0.78f,
					damping * 0.62f
				)
			)
		);


	// Low values now genuinely look low.
	//
	// Maximum still reaches the strange replacement state.
	const float mix =
		intensity *
		(
			0.12f +
			parameterCharacter *
				0.88f
		);


	return ofClamp(
		mix,
		0.0f,
		1.0f
	);
}


//--------------------------------------------------------------
float VisualStateManager::getBlurStretch() const
{
	if (
		blurWeight <
		0.01f
	)
	{
		return 0.0f;
	}


	const float intensity =
		pow(
			ofClamp(
				blurWeight,
				0.0f,
				1.0f
			),
			1.22f
		);


	const float width =
		ofClamp(
			reverbWidth,
			0.0f,
			1.0f
		);


	const float damping =
		ofClamp(
			reverbDamping,
			0.0f,
			1.0f
		);


	const float stretch =
		3.0f +
		intensity *
		(
			18.0f +
			width *
				150.0f +
			damping *
				22.0f
		);


	return ofClamp(
		stretch,
		0.0f,
		195.0f
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
			feedbackIntensity * 0.88f
		);

	intensity =
		std::max(
			intensity,
			timeIntensity * 0.68f
		);

	// Early response:
	// even fairly small Delay values now create an obvious hold.
	const float response =
		pow(
			ofClamp(
				intensity,
				0.0f,
				1.0f
			),
			0.55f
		);

	const float interval =
		4.0f +
		response * 20.0f +
		timeIntensity * response * 5.0f;

	return ofClamp(
		static_cast<int>(
			round(
				interval
			)
		),
		4,
		30
	);
}
//--------------------------------------------------------------
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

	const float response =
		visualCurve(
			stepWeight
		);

	// Once StepPrint is awake it should already be legible.
	//
	// The controller then moves it from obvious -> dominant,
	// rather than invisible -> obvious.
	const float value =
		0.60f +
		response * 0.38f;

	return ofClamp(
		value,
		0.0f,
		0.98f
	);
}

//--------------------------------------------------------------
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

	const float stepPresence =
		visualCurve(
			stepWeight
		);

	const float actualFeedback =
		visualCurve(
			delayFeedback
		);

	float value =
		stepPresence * 0.22f +
		actualFeedback * 0.75f +
		mutationAmount * 0.05f;

	return ofClamp(
		value,
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
