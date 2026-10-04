#pragma once

#include "ofMain.h"
#include <array>

class StepPrinting
{
public:
	StepPrinting();

	void setup(
		int stepInterval
	);

	void update(
		const ofTexture& videoTexture
	);

	void apply(
		ofFbo& fbo
	);

	bool isActive() const;

	void clear();
	void clearFrames();

	void setStepInterval(
		int interval
	);

	int getStepInterval() const;

	void setMixAmount(
		float amount
	);

	float getMixAmount() const;

	void setFadeStrength(
		float strength
	);

	float getFadeStrength() const;

	void setFeedbackAmount(
		float amount
	);

	float getFeedbackAmount() const;

	void setMaxStoredFrames(
		int maxFrames,
		bool forceClear = false
	);

	int getMaxStoredFrames() const;

	void resetAllParameters();

private:
	static constexpr int HISTORY_SIZE = 6;

	void setupShader();

	void allocateBuffers(
		int width,
		int height
	);

	void captureFrame(
		ofFbo& destination,
		const ofTexture& texture,
		int width,
		int height
	);

	int stepInterval;
	int frameCounter;

	float mixAmount;
	float feedbackAmount;

	int maxStoredFrames;

	int writeIndex;
	int framesStored;

	std::array<ofFbo, HISTORY_SIZE> historyFrames;
	ofFbo outputBuffer;

	ofShader trailShader;
	bool shaderReady;
};
