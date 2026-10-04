#pragma once

#include "ofMain.h"
#include "ofxOsc.h"

#include "ChronologyManager.hpp"
#include "VisualStateManager.hpp"
#include "MotionBlur.hpp"
#include "StepPrinting.hpp"
#include "FisheyeLens.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

class ofApp : public ofBaseApp
{
public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;
	void keyPressed(int key) override;

private:
	void applyVisualStateParameters();

	void updateEffectHistories(ofVideoPlayer* currentVideo);

	void renderTemporalMutation(ofFbo& source,
								ofFbo& destination);

	void buildProcessedFrame(ofVideoPlayer* currentVideo);

	void drawDryProcessedComposite(float x,
								   float y,
								   float width,
								   float height);

	void copyFbo(ofFbo& source,
				 ofFbo& destination);

	void blendFbo(ofFbo& base,
				  ofFbo& overlay,
				  ofFbo& destination,
				  float amount);

	void applyInteractionMemory(ofFbo& target);

	ChronologyManager chronologyManager;
	VisualStateManager visualState;

	MotionBlur motionBlur;
	StepPrinting stepPrinting;
	FisheyeLens fisheye;

	ofxOscReceiver oscReceiver;

	ofFbo videoFbo;
	ofFbo intermediateFbo;
	ofFbo processedFbo;

	ofFbo historySourceFbo;
	ofFbo historyCompositeFbo;

	ofFbo routeAFbo;
	ofFbo routeBFbo;

	ofFbo mutationFbo;

	ofFbo preWarpFbo;
	ofFbo preWarpTemporalFbo;

	ofFbo interactionMemoryFbo;
	ofFbo interactionScratchFbo;

	int standardWidth = 1920;
	int standardHeight = 1080;

	bool isMotionBlurActive = false;
	bool isStepActive = false;
	bool isBassActive = false;

	bool interactionMemoryReady = false;

	std::vector<ofVideoPlayer> videos;
	int currentVideoIndex = 0;
};
