#pragma once

#include "ofMain.h"
#include <algorithm>
#include <cmath>

class VisualStateManager
{
public:
	enum class Family
	{
		CLEAN,
		REVERB,
		DELAY,
		BASS
	};

	void setup();
	void update(float deltaTime);

	void setReverbRoom(float value);
	void setReverbWet(float value);
	void setReverbDamping(float value);
	void setReverbWidth(float value);

	void setDelayTime(float milliseconds);
	void setDelayFeedback(float value);
	void setDubSend(float value);

	void setBass(float value);
	void setMids(float value);
	void setTops(float value);

	void setDubCrossfader(float value);

	void setSirenPitch(float value);
	void triggerSiren();

	float getMotionBlurWeight() const;
	float getStepPrintWeight() const;
	float getFisheyeWeight() const;
	float getProcessedAmount() const;

	float getTemporalDirection() const;
	float getBlurToStepMorph() const;
	float getStepToBlurMorph() const;
	float getTemporalOverlap() const;

	float getMutationAmount() const;
	float getMutationDrift() const;

	float getBlurPersistence() const;
	float getBlurMix() const;
	float getBlurStretch() const;
	float getBlurSoftness() const;

	int getStepInterval() const;
	float getStepMix() const;
	float getStepFeedback() const;

	float getFisheyeBass() const;
	float getFisheyeDistortion() const;
	float getFisheyePulseFrequency() const;
	float getFisheyeMaxDistortion() const;

	Family getDominantFamily() const;

private:
	float smoothTowards(float current,
						float target,
						float attack,
						float release,
						float deltaTime) const;

	float visualCurve(float value) const;
	float gestureAmount(float oldValue,
						float newValue,
						float strength) const;

	float wakeAmount(float oldValue,
					 float newValue,
					 float minimumWake,
					 float sensitivity) const;

	void updateHierarchy();

	float reverbRoom = 0.0f;
	float reverbWet = 0.0f;
	float reverbDamping = 0.5f;
	float reverbWidth = 0.5f;

	float delayTimeMs = 320.0f;
	float delayFeedback = 0.0f;
	float dubSend = 0.0f;

	float bass = 0.0f;
	float mids = 0.5f;
	float tops = 0.5f;

	float dubCrossfader = 1.0f;
	float sirenPitch = 0.45f;

	float reverbPresence = 0.0f;
	float delayPresence = 0.0f;
	float bassPresence = 0.0f;
	float processedAmount = 1.0f;

	float reverbTouch = 0.0f;
	float delayTouch = 0.0f;
	float bassTouch = 0.0f;

	float reverbGesture = 0.0f;
	float delayGesture = 0.0f;
	float bassGesture = 0.0f;

	float reverbToDelayAccent = 0.0f;
	float delayToReverbAccent = 0.0f;

	float temporalDirection = 0.0f;
	float temporalOverlap = 0.0f;
	float blurToStepMorph = 0.0f;
	float stepToBlurMorph = 0.0f;

	float mutationAmount = 0.0f;
	float mutationDrift = 0.5f;

	float organicPhase = 0.0f;
	float organicBassModulation = 1.0f;

	float sirenImpulse = 0.0f;

	float blurWeight = 0.0f;
	float stepWeight = 0.0f;
	float fisheyeWeight = 0.0f;

	Family dominantFamily = Family::CLEAN;
};
