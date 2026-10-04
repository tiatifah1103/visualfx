#pragma once

#include "ofMain.h"

class FisheyeLens
{
public:
	FisheyeLens();

	void setup(float distortionStrength);

	void setBassLevel(float level);
	void setPulseFrequency(float freq);
	void setMaxDistortion(float max);

	void update(const ofTexture& videoTexture);

	void renderTo(const ofTexture& videoTexture,
				  ofFbo& destination);

	void apply(float x,
			   float y,
			   float width,
			   float height);

	void setDistortionStrength(float strength);
	float getDistortionStrength() const;

	void reset();

private:
	void updatePulsing(float deltaTime);
	void updateMovement(float deltaTime);

	float calculateFinalDistortion() const;

	void renderTexture(const ofTexture& videoTexture,
					   ofFbo& destination);

	float distortionStrength;
	float baseDistortion;
	float currentDistortion;
	float distortionSmoothing;

	float bassLevel;
	float maxDistortion;

	float pulseFrequency;
	float timeCounter;
	float nextPulseTime;
	float pulseDuration;
	float currentPulseStrength;

	float movementSpeed;
	float movementAmount;

	float vibrationAmount;
	float vibrationSpeed;

	ofVec2f currentOffset = ofVec2f(0.0f, 0.0f);
	ofVec2f targetOffset = ofVec2f(0.0f, 0.0f);

	ofFbo distortedFrame;
};
