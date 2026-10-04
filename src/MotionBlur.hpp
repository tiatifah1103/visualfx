#pragma once

#include "ofMain.h"


class MotionBlur
{
public:

	MotionBlur();


	void setup(
		float blendFactor,
		float stretchAmount
	);


	void update(
		const ofTexture& videoTexture
	);


	void decayOnly();


	void apply(
		ofFbo& fbo
	);


	void clear();


	void setBlendFactor(
		float factor
	);


	float getBlendFactor() const;


	void setMixAmount(
		float amount
	);


	float getMixAmount() const;


	void setStretchAmount(
		float amount
	);


	float getStretchAmount() const;


	bool hasVisibleResidue() const;


	void resetAllParameters();


private:

	void allocateBuffers(
		int width,
		int height
	);


	float colorDistance(
		const ofColor& color1,
		const ofColor& color2
	);


	void fadeHistory(
		float keepAmount
	);


	float blendFactor =
		0.84f;


	float mixAmount =
		0.0f;


	float stretchAmount =
		24.0f;


	float residualStrength =
		0.0f;


	ofFbo distortedFrame;

	ofFbo accumulationBuffer;

	ofFbo historyScratchBuffer;

	ofFbo outputBuffer;


	ofPixels currentFramePixels;

	ofPixels previousFramePixels;
};
