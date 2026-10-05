#include "MotionBlur.hpp"

#include <cmath>
#include <algorithm>


//--------------------------------------------------------------
MotionBlur::MotionBlur()
{
	blendFactor =
		0.84f;

	mixAmount =
		0.0f;

	stretchAmount =
		24.0f;

	residualStrength =
		0.0f;
}


//--------------------------------------------------------------
void MotionBlur::setup(
	float _blendFactor,
	float _stretchAmount
)
{
	blendFactor =
		ofClamp(
			_blendFactor,
			0.0f,
			0.98f
		);


	stretchAmount =
		ofClamp(
			_stretchAmount,
			0.0f,
			90.0f
		);


	mixAmount =
		0.0f;


	residualStrength =
		0.0f;


	allocateBuffers(
		ofGetWidth(),
		ofGetHeight()
	);


	clear();
}


//--------------------------------------------------------------
void MotionBlur::allocateBuffers(
	int width,
	int height
)
{
	if (
		width <= 0 ||
		height <= 0
	)
	{
		return;
	}


	distortedFrame.allocate(
		width,
		height,
		GL_RGBA
	);


	distortedFrame.begin();

	ofClear(
		0,
		0,
		0,
		0
	);

	distortedFrame.end();


	accumulationBuffer.allocate(
		width,
		height,
		GL_RGBA
	);


	accumulationBuffer.begin();

	ofClear(
		0,
		0,
		0,
		255
	);

	accumulationBuffer.end();


	historyScratchBuffer.allocate(
		width,
		height,
		GL_RGBA
	);


	historyScratchBuffer.begin();

	ofClear(
		0,
		0,
		0,
		255
	);

	historyScratchBuffer.end();


	outputBuffer.allocate(
		width,
		height,
		GL_RGBA
	);


	outputBuffer.begin();

	ofClear(
		0,
		0,
		0,
		255
	);

	outputBuffer.end();


	currentFramePixels.clear();

	previousFramePixels.clear();


	residualStrength =
		0.0f;
}


//--------------------------------------------------------------
float MotionBlur::colorDistance(
	const ofColor& color1,
	const ofColor& color2
)
{
	const float rDiff =
		static_cast<float>(
			color1.r
		)
		-
		static_cast<float>(
			color2.r
		);


	const float gDiff =
		static_cast<float>(
			color1.g
		)
		-
		static_cast<float>(
			color2.g
		);


	const float bDiff =
		static_cast<float>(
			color1.b
		)
		-
		static_cast<float>(
			color2.b
		);


	return std::sqrt(
		rDiff * rDiff +
		gDiff * gDiff +
		bDiff * bDiff
	);
}


//--------------------------------------------------------------
void MotionBlur::fadeHistory(
	float keepAmount
)
{
	if (
		!accumulationBuffer.isAllocated() ||
		!historyScratchBuffer.isAllocated()
	)
	{
		return;
	}


	keepAmount =
		ofClamp(
			keepAmount,
			0.0f,
			1.0f
		);


	// ============================================================
	// COPY OLD HISTORY AT REDUCED BRIGHTNESS
	//
	// History stays on an opaque black field.
	//
	// This keeps the trail visible and predictable while allowing
	// RGB values to genuinely fade away.
	// ============================================================

	historyScratchBuffer.begin();


	ofClear(
		0,
		0,
		0,
		255
	);


	ofEnableBlendMode(
		OF_BLENDMODE_ALPHA
	);


	ofSetColor(
		255,
		255,
		255,
		keepAmount *
			255.0f
	);


	accumulationBuffer.draw(
		0,
		0,
		historyScratchBuffer.getWidth(),
		historyScratchBuffer.getHeight()
	);


	ofDisableBlendMode();


	ofSetColor(
		255
	);


	historyScratchBuffer.end();


	accumulationBuffer.begin();


	ofClear(
		0,
		0,
		0,
		255
	);


	ofSetColor(
		255
	);


	historyScratchBuffer.draw(
		0,
		0,
		accumulationBuffer.getWidth(),
		accumulationBuffer.getHeight()
	);


	accumulationBuffer.end();
}


//--------------------------------------------------------------
void MotionBlur::update(
	const ofTexture& videoTexture
)
{
	if (
		!videoTexture.isAllocated()
	)
	{
		return;
	}


	const int width =
		static_cast<int>(
			videoTexture.getWidth()
		);


	const int height =
		static_cast<int>(
			videoTexture.getHeight()
		);


	if (
		width <= 0 ||
		height <= 0
	)
	{
		return;
	}


	if (
		!distortedFrame.isAllocated() ||
		distortedFrame.getWidth() != width ||
		distortedFrame.getHeight() != height
	)
	{
		allocateBuffers(
			width,
			height
		);
	}


	videoTexture.readToPixels(
		currentFramePixels
	);


	if (
		!currentFramePixels.isAllocated()
	)
	{
		return;
	}


	if (
		!previousFramePixels.isAllocated() ||
		previousFramePixels.getWidth() != width ||
		previousFramePixels.getHeight() != height
	)
	{
		previousFramePixels =
			currentFramePixels;


		return;
	}


	// ============================================================
	// CURRENT MOTION SMEAR
	// ============================================================

	distortedFrame.begin();


	ofClear(
		0,
		0,
		0,
		0
	);


	ofEnableAlphaBlending();


	//
	// This increases the number of visible trails without turning
	// this into a completely different algorithm.
	const int downsampleFactor =
		6;


	const float motionThreshold =
		8.0f;


	bool foundMotion =
		false;


	float strongestMotion =
		0.0f;


	for (
		int y = 0;
		y < height;
		y += downsampleFactor
	)
	{
		for (
			int x = 0;
			x < width;
			x += downsampleFactor
		)
		{
			const ofColor currentColor =
				currentFramePixels.getColor(
					x,
					y
				);


			const ofColor previousColor =
				previousFramePixels.getColor(
					x,
					y
				);


			const float difference =
				colorDistance(
					currentColor,
					previousColor
				);


			if (
				difference <=
				motionThreshold
			)
			{
				continue;
			}


			foundMotion =
				true;


			float motionAmount =
				ofMap(
					difference,
					motionThreshold,
					150.0f,
					0.0f,
					1.0f,
					true
				);


			motionAmount =
				std::pow(
					motionAmount,
					0.62f
				);


			strongestMotion =
				std::max(
					strongestMotion,
					motionAmount
				);


			// ====================================================
			// LONGER THAN THE ORIGINAL
			// ====================================================

			const float smearDistance =
				3.0f +
				motionAmount *
					stretchAmount *
					1.55f;


			const ofColor trailColor =
				currentColor.getLerped(
					previousColor,
					0.56f
				);


			// ====================================================
			// OUTER SOFT SMEAR
			// ====================================================

			const float outerDistance =
				smearDistance *
				1.18f;


			const float outerAlpha =
				ofMap(
					motionAmount,
					0.0f,
					1.0f,
					34.0f,
					104.0f,
					true
				);


			ofSetColor(
				trailColor,
				outerAlpha
			);


			ofDrawRectangle(
				x -
					outerDistance,
				y -
					2.5f,
				downsampleFactor +
					outerDistance *
						2.0f,
				downsampleFactor +
					5.0f
			);


			// ====================================================
			// MIDDLE SMEAR
			// ====================================================

			const float middleDistance =
				smearDistance *
				0.72f;


			const float middleAlpha =
				ofMap(
					motionAmount,
					0.0f,
					1.0f,
					54.0f,
					138.0f,
					true
				);


			ofSetColor(
				trailColor,
				middleAlpha
			);


			ofDrawRectangle(
				x -
					middleDistance,
				y -
					1.0f,
				downsampleFactor +
					middleDistance *
						2.0f,
				downsampleFactor +
					2.0f
			);


			// ====================================================
			// INNER SMEAR
			// ====================================================

			const float innerDistance =
				smearDistance *
				0.42f;


			const float innerAlpha =
				ofMap(
					motionAmount,
					0.0f,
					1.0f,
					76.0f,
					176.0f,
					true
				);


			ofSetColor(
				trailColor,
				innerAlpha
			);


			ofDrawRectangle(
				x -
					innerDistance,
				y,
				downsampleFactor +
					innerDistance *
						2.0f,
				downsampleFactor
			);


			// ====================================================
			// SMALL VERTICAL SOFTNESS
			//
			// Not another effect.
			//
			// Just prevents the longer horizontal rectangles from
			// becoming too graphic/hard edged.
			// ====================================================

			const float verticalAmount =
				2.0f +
				motionAmount *
					4.5f;


			ofSetColor(
				trailColor,
				28.0f +
					motionAmount *
						54.0f
			);


			ofDrawRectangle(
				x -
					innerDistance *
						0.40f,
				y -
					verticalAmount,
				downsampleFactor +
					innerDistance *
						0.80f,
				downsampleFactor +
					verticalAmount *
						2.0f
			);
		}
	}


	ofDisableAlphaBlending();


	ofSetColor(
		255
	);


	distortedFrame.end();


	// ============================================================
	// HISTORY DECAY
	// ============================================================

	const float persistence =
		ofClamp(
			ofLerp(
				0.78f,
				0.955f,
				blendFactor
			),
			0.78f,
			0.955f
		);


	fadeHistory(
		persistence
	);


	// ============================================================
	// ADD NEW MOTION TO HISTORY
	//
	// ADD happens internally on black.
	//
	// The crucial difference from the bad white version is that
	// this accumulated image is NOT then stacked onto the final
	// video five times.
	// ============================================================

	if (
		foundMotion
	)
	{
		accumulationBuffer.begin();


		ofEnableBlendMode(
			OF_BLENDMODE_ADD
		);


		ofSetColor(
			255,
			255,
			255,
			210
		);


		distortedFrame.draw(
			0,
			0,
			width,
			height
		);


		// One faint neighbouring copy gives slightly softer and
		// more abundant trails.
		ofSetColor(
			255,
			255,
			255,
			72
		);


		distortedFrame.draw(
			-2.0f,
			0,
			width,
			height
		);


		ofDisableBlendMode();


		ofSetColor(
			255
		);


		accumulationBuffer.end();


		residualStrength =
			std::max(
				residualStrength,
				0.50f +
					strongestMotion *
						0.50f
			);
	}
	else
	{
		residualStrength *=
			0.96f;
	}


	residualStrength =
		ofClamp(
			residualStrength,
			0.0f,
			1.0f
		);


	previousFramePixels =
		currentFramePixels;
}


//--------------------------------------------------------------
void MotionBlur::decayOnly()
{
	if (
		!accumulationBuffer.isAllocated()
	)
	{
		residualStrength =
			0.0f;


		return;
	}


	// ============================================================
	// CONTROLLER HAS FALLEN AWAY
	//
	// No new motion.
	//
	// Keep a short visible residue, then kill it decisively.
	// ============================================================

	fadeHistory(
		0.78f
	);


	residualStrength *=
		0.82f;


	if (
		residualStrength <
		0.008f
	)
	{
		residualStrength =
			0.0f;
	}
}


//--------------------------------------------------------------
void MotionBlur::apply(
	ofFbo& fbo
)
{
	if (
		!accumulationBuffer.isAllocated()
	)
	{
		return;
	}


	const int width =
		static_cast<int>(
			fbo.getWidth()
		);


	const int height =
		static_cast<int>(
			fbo.getHeight()
		);


	if (
		width <= 0 ||
		height <= 0
	)
	{
		return;
	}


	if (
		!outputBuffer.isAllocated() ||
		outputBuffer.getWidth() != width ||
		outputBuffer.getHeight() != height
	)
	{
		outputBuffer.allocate(
			width,
			height,
			GL_RGBA
		);
	}


	const float controllerMix =
		std::pow(
			ofClamp(
				mixAmount,
				0.0f,
				1.0f
			),
			0.30f
		);


	const float residueMix =
		residualStrength *
		0.60f;


	const float effectiveMix =
		std::max(
			controllerMix,
			residueMix
		);


	outputBuffer.begin();


	ofClear(
		0,
		0,
		0,
		255
	);


	// ============================================================
	// CLEAN IMAGE
	// ============================================================

	ofSetColor(
		255
	);


	fbo.draw(
		0,
		0,
		width,
		height
	);


	// ============================================================

	// ============================================================

	if (
		effectiveMix >
		0.001f
	)
	{
		ofEnableBlendMode(
			OF_BLENDMODE_SCREEN
		);


		// Main history.
		ofSetColor(
			255,
			255,
			255,
			effectiveMix *
				142.0f
		);


		accumulationBuffer.draw(
			0,
			0,
			width,
			height
		);


		// Two extremely restrained soft ghosts.
		//
		// They increase blur without flooding the screen.
		const float spread =
			2.0f +
			effectiveMix *
				7.0f;


		ofSetColor(
			255,
			255,
			255,
			effectiveMix *
				26.0f
		);


		accumulationBuffer.draw(
			-spread,
			0,
			width,
			height
		);


		accumulationBuffer.draw(
			spread,
			0,
			width,
			height
		);


		ofDisableBlendMode();
	}


	ofSetColor(
		255
	);


	outputBuffer.end();


	fbo.begin();


	ofClear(
		0,
		0,
		0,
		255
	);


	ofSetColor(
		255
	);


	outputBuffer.draw(
		0,
		0,
		width,
		height
	);


	fbo.end();
}


//--------------------------------------------------------------
void MotionBlur::clear()
{
	if (
		distortedFrame.isAllocated()
	)
	{
		distortedFrame.begin();

		ofClear(
			0,
			0,
			0,
			0
		);

		distortedFrame.end();
	}


	if (
		accumulationBuffer.isAllocated()
	)
	{
		accumulationBuffer.begin();

		ofClear(
			0,
			0,
			0,
			255
		);

		accumulationBuffer.end();
	}


	if (
		historyScratchBuffer.isAllocated()
	)
	{
		historyScratchBuffer.begin();

		ofClear(
			0,
			0,
			0,
			255
		);

		historyScratchBuffer.end();
	}


	if (
		outputBuffer.isAllocated()
	)
	{
		outputBuffer.begin();

		ofClear(
			0,
			0,
			0,
			255
		);

		outputBuffer.end();
	}


	currentFramePixels.clear();

	previousFramePixels.clear();


	residualStrength =
		0.0f;
}


//--------------------------------------------------------------
void MotionBlur::setBlendFactor(
	float factor
)
{
	blendFactor =
		ofClamp(
			factor,
			0.0f,
			0.98f
		);
}


//--------------------------------------------------------------
float MotionBlur::getBlendFactor() const
{
	return blendFactor;
}


//--------------------------------------------------------------
void MotionBlur::setMixAmount(
	float amount
)
{
	mixAmount =
		ofClamp(
			amount,
			0.0f,
			1.0f
		);
}


//--------------------------------------------------------------
float MotionBlur::getMixAmount() const
{
	return mixAmount;
}


//--------------------------------------------------------------
void MotionBlur::setStretchAmount(
	float amount
)
{
	stretchAmount =
		ofClamp(
			amount,
			0.0f,
			90.0f
		);
}


//--------------------------------------------------------------
float MotionBlur::getStretchAmount() const
{
	return stretchAmount;
}


//--------------------------------------------------------------
bool MotionBlur::hasVisibleResidue() const
{
	return (
		residualStrength >
		0.008f
	);
}


//--------------------------------------------------------------
void MotionBlur::resetAllParameters()
{
	mixAmount =
		0.0f;


	stretchAmount =
		0.0f;


	residualStrength =
		0.0f;


	clear();
}
