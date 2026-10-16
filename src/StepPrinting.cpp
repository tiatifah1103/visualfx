#include "StepPrinting.hpp"

#include <cmath>
#include <algorithm>


//--------------------------------------------------------------
StepPrinting::StepPrinting()
{
	stepInterval = 3;
	frameCounter = 0;

	mixAmount = 0.0f;
	feedbackAmount = 0.0f;

	maxStoredFrames = 4;

	writeIndex = 0;
	framesStored = 0;

	shaderReady = false;
}


//--------------------------------------------------------------
void StepPrinting::setup(
	int _stepInterval
)
{
	stepInterval =
		ofClamp(
			_stepInterval,
			2,
			16
		);

	frameCounter = 0;

	mixAmount = 0.0f;
	feedbackAmount = 0.0f;

	maxStoredFrames = 4;

	writeIndex = 0;
	framesStored = 0;

	setupShader();

	allocateBuffers(
		ofGetWidth(),
		ofGetHeight()
	);
}


//--------------------------------------------------------------
void StepPrinting::setupShader()
{
	const std::string vertexShader =
		R"(
		#version 150

		uniform mat4 modelViewProjectionMatrix;

		in vec4 position;
		in vec2 texcoord;

		out vec2 texCoordV;

		void main()
		{
			texCoordV = texcoord;

			gl_Position =
				modelViewProjectionMatrix *
				position;
		}
		)";


	const std::string fragmentShader =
		R"(
		#version 150

		uniform sampler2DRect currentTex;
		uniform sampler2DRect historyTex;

		uniform float trailAlpha;
		uniform float differenceThreshold;
		uniform float posterizeLevels;

		in vec2 texCoordV;

		out vec4 outputColor;

		void main()
		{
			vec3 currentColour =
				texture(
					currentTex,
					texCoordV
				).rgb;

			vec3 historicalColour =
				texture(
					historyTex,
					texCoordV
				).rgb;

			float difference =
				length(
					currentColour -
					historicalColour
				)
				/
				1.7320508;

			float mask =
				smoothstep(
					differenceThreshold,
					differenceThreshold +
						0.04,
					difference
				);

			float levels =
				max(
					posterizeLevels,
					2.0
				);

			vec3 posterised =
				floor(
					historicalColour *
					levels
				)
				/
				levels;

			outputColor =
				vec4(
					posterised,
					mask *
					trailAlpha
				);
		}
		)";


	trailShader.setupShaderFromSource(
		GL_VERTEX_SHADER,
		vertexShader
	);

	trailShader.setupShaderFromSource(
		GL_FRAGMENT_SHADER,
		fragmentShader
	);

	trailShader.bindDefaults();

	shaderReady =
		trailShader.linkProgram();
}


//--------------------------------------------------------------
void StepPrinting::allocateBuffers(
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


	for (
		auto& frame :
		historyFrames
	)
	{
		frame.allocate(
			width,
			height,
			GL_RGBA
		);

		frame.begin();

		ofClear(
			0,
			0,
			0,
			255
		);

		frame.end();
	}


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


	frameCounter = 0;
	writeIndex = 0;
	framesStored = 0;
}


//--------------------------------------------------------------
void StepPrinting::captureFrame(
	ofFbo& destination,
	const ofTexture& texture,
	int width,
	int height
)
{
	destination.begin();

	ofClear(
		0,
		0,
		0,
		255
	);

	ofSetColor(255);

	texture.draw(
		0,
		0,
		width,
		height
	);

	destination.end();
}


//--------------------------------------------------------------
void StepPrinting::update(
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
		!historyFrames[0].isAllocated() ||
		historyFrames[0].getWidth() != width ||
		historyFrames[0].getHeight() != height
	)
	{
		allocateBuffers(
			width,
			height
		);
	}


	frameCounter++;


	// ============================================================
	// FIRST PRINT
	// ============================================================

	if (
		framesStored ==
		0
	)
	{
		captureFrame(
			historyFrames[
				writeIndex
			],
			videoTexture,
			width,
			height
		);

		writeIndex =
			(
				writeIndex +
				1
			)
			%
			HISTORY_SIZE;

		framesStored =
			1;

		frameCounter =
			0;

		return;
	}


	// ============================================================
	// CAPTURE ONLY ON STEP
	// ============================================================
	//
	// Between these captures the same frame remains visible.
	//
	// THAT is the actual StepPrint behaviour.
	// ============================================================

	if (
		frameCounter >=
		stepInterval
	)
	{
		captureFrame(
			historyFrames[
				writeIndex
			],
			videoTexture,
			width,
			height
		);

		writeIndex =
			(
				writeIndex +
				1
			)
			%
			HISTORY_SIZE;


		if (
			framesStored <
			HISTORY_SIZE
		)
		{
			framesStored++;
		}


		frameCounter =
			0;
	}
}


//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void StepPrinting::apply(
	ofFbo& fbo
)
{
	if (
		!isActive() ||
		framesStored <= 0
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
		allocateBuffers(
			width,
			height
		);

		return;
	}

	// ============================================================
	// RESPONSE
	//
	// Strong early response.
	//
	// You should NOT have to push Delay towards maximum before
	// StepPrint becomes visually identifiable.
	// ============================================================

	const float mixResponse =
		pow(
			ofClamp(
				mixAmount,
				0.0f,
				1.0f
			),
			0.30f
		);

	const float feedbackResponse =
		pow(
			ofClamp(
				feedbackAmount,
				0.0f,
				1.0f
			),
			0.42f
		);

	int newestIndex =
		writeIndex - 1;

	if (
		newestIndex < 0
	)
	{
		newestIndex +=
			HISTORY_SIZE;
	}

	// ============================================================
	// BEGIN OUTPUT
	// ============================================================

	outputBuffer.begin();

	ofClear(
		0,
		0,
		0,
		255
	);

	// ============================================================
	// CURRENT IMAGE
	//
	// KEEP IT FULLY VISIBLE.
	//
	// We do not dim the whole image in order to make StepPrint
	// visible. Historical frames are printed ON TOP.
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

	ofEnableBlendMode(
		OF_BLENDMODE_ALPHA
	);

	// ============================================================
	// MAIN HELD FRAME
	//
	// This is the source of the actual temporal chop.
	//
	// It becomes dominant quite early.
	// ============================================================

	const float heldOpacity =
		ofLerp(
			0.56f,
			0.91f,
			mixResponse
		);

	ofSetColor(
		255,
		255,
		255,
		heldOpacity *
			255.0f
	);

	historyFrames[
		newestIndex
	].draw(
		0,
		0,
		width,
		height
	);

	// ============================================================
	// SAME PRINT / REGISTRATION GHOST
	//
	// Unlike the old version this does NOT require significant
	// feedback before it appears.
	//
	// Feedback merely stretches it further away.
	// ============================================================

	const float ghostDistance =
		1.5f +
		mixResponse * 2.5f +
		feedbackResponse * 7.0f;

	const float ghostAlpha =
		mixResponse *
		(
			24.0f +
			feedbackResponse *
				54.0f
		);

	if (
		ghostAlpha >
		1.0f
	)
	{
		ofSetColor(
			255,
			255,
			255,
			ghostAlpha
		);

		historyFrames[
			newestIndex
		].draw(
			-ghostDistance,
			0.0f,
			width,
			height
		);

		ofSetColor(
			255,
			255,
			255,
			ghostAlpha * 0.48f
		);

		historyFrames[
			newestIndex
		].draw(
			ghostDistance * 0.62f,
			ghostDistance * 0.08f,
			width,
			height
		);
	}

	// ============================================================
	// PREVIOUS PRINTS
	//
	// This is the double / multiple-exposure part.
	//
	// IMPORTANT:
	//
	// These previous frames now exist from STEP MIX ALONE.
	//
	// Delay feedback increases their persistence and displacement,
	// but is not required to make them visible.
	// ============================================================

	const int availableOlderFrames =
		std::min(
			framesStored - 1,
			std::max(
				0,
				maxStoredFrames - 1
			)
		);

	const int olderFrames =
		std::min(
			availableOlderFrames,
			3
		);

	for (
		int age = 1;
		age <= olderFrames;
		++age
	)
	{
		int index =
			newestIndex -
			age;

		while (
			index < 0
		)
		{
			index +=
				HISTORY_SIZE;
		}

		const float ageFade =
			pow(
				0.62f,
				static_cast<float>(
					age - 1
				)
			);

		// Baseline exposure comes from mixResponse.
		//
		// Feedback increases it rather than creating it.
		float echoAlpha =
			mixResponse *
			ageFade *
			(
				36.0f +
				feedbackResponse *
					74.0f
			);

		echoAlpha =
			ofClamp(
				echoAlpha,
				0.0f,
				112.0f
			);

		if (
			echoAlpha <
			1.0f
		)
		{
			continue;
		}

		const float direction =
			(
				age %
				2 ==
				0
			)
			?
			1.0f
			:
			-1.0f;

		// Small imperfect registration.
		//
		// Enough to read as several exposures,
		// not enough to become generic glitch displacement.
		const float displacement =
			direction *
			(
				2.0f +
				mixResponse *
					2.0f +
				feedbackResponse *
					4.0f *
					age
			);

		const float yDisplacement =
			(
				age -
				1
			)
			*
			feedbackResponse *
			1.1f;

		const float scaleAmount =
			1.0f +
			feedbackResponse *
			0.0025f *
			static_cast<float>(
				age
			);

		const float drawWidth =
			width *
			scaleAmount;

		const float drawHeight =
			height *
			scaleAmount;

		const float correctionX =
			(
				width -
				drawWidth
			)
			*
			0.5f;

		const float correctionY =
			(
				height -
				drawHeight
			)
			*
			0.5f;

		ofSetColor(
			255,
			255,
			255,
			echoAlpha
		);

		historyFrames[
			index
		].draw(
			correctionX +
				displacement,
			correctionY +
				yDisplacement,
			drawWidth,
			drawHeight
		);
	}

	// ============================================================
	// CHANGED-REGION PRINT
	//
	// This remains secondary.
	//
	// It gives the held frames some broken / printed instability
	// without becoming a permanent glitch filter.
	// ============================================================

	if (
		shaderReady &&
		framesStored > 1 &&
		mixResponse > 0.10f
	)
	{
		int olderIndex =
			newestIndex - 1;

		if (
			olderIndex < 0
		)
		{
			olderIndex +=
				HISTORY_SIZE;
		}

		trailShader.begin();

		trailShader.setUniformTexture(
			"currentTex",
			fbo.getTexture(),
			0
		);

		trailShader.setUniformTexture(
			"historyTex",
			historyFrames[
				olderIndex
			].getTexture(),
			1
		);

		trailShader.setUniform1f(
			"trailAlpha",
			0.055f +
			mixResponse *
				0.065f +
			feedbackResponse *
				0.14f
		);

		trailShader.setUniform1f(
			"differenceThreshold",
			0.052f
		);

		trailShader.setUniform1f(
			"posterizeLevels",
			ofLerp(
				8.0f,
				4.0f,
				feedbackResponse
			)
		);

		ofSetColor(
			255
		);

		historyFrames[
			olderIndex
		].draw(
			0,
			0,
			width,
			height
		);

		trailShader.end();
	}

	ofDisableBlendMode();

	ofSetColor(
		255
	);

	outputBuffer.end();

	// ============================================================
	// COPY BACK
	// ============================================================

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
bool StepPrinting::isActive() const
{
	return (
		mixAmount >
		0.004f
	);
}


//--------------------------------------------------------------
void StepPrinting::clear()
{
	for (
		auto& frame :
		historyFrames
	)
	{
		if (
			frame.isAllocated()
		)
		{
			frame.begin();

			ofClear(
				0,
				0,
				0,
				255
			);

			frame.end();
		}
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


	frameCounter =
		0;

	writeIndex =
		0;

	framesStored =
		0;
}


//--------------------------------------------------------------
void StepPrinting::clearFrames()
{
	clear();
}


//--------------------------------------------------------------
void StepPrinting::setStepInterval(
	int interval
)
{
	stepInterval =
		ofClamp(
			interval,
			2,
			16
		);


	if (
		frameCounter >=
		stepInterval
	)
	{
		frameCounter =
			0;
	}
}


//--------------------------------------------------------------
int StepPrinting::getStepInterval() const
{
	return stepInterval;
}


//--------------------------------------------------------------
void StepPrinting::setMixAmount(
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
float StepPrinting::getMixAmount() const
{
	return mixAmount;
}


//--------------------------------------------------------------
void StepPrinting::setFadeStrength(
	float strength
)
{
	setMixAmount(
		strength
	);
}


//--------------------------------------------------------------
float StepPrinting::getFadeStrength() const
{
	return mixAmount;
}


//--------------------------------------------------------------
void StepPrinting::setFeedbackAmount(
	float amount
)
{
	feedbackAmount =
		ofClamp(
			amount,
			0.0f,
			1.0f
		);
}


//--------------------------------------------------------------
float StepPrinting::getFeedbackAmount() const
{
	return feedbackAmount;
}


//--------------------------------------------------------------
void StepPrinting::setMaxStoredFrames(
	int maxFrames,
	bool forceClear
)
{
	maxStoredFrames =
		ofClamp(
			maxFrames,
			1,
			HISTORY_SIZE
		);


	if (
		forceClear
	)
	{
		clear();
	}
}


//--------------------------------------------------------------
int StepPrinting::getMaxStoredFrames() const
{
	return maxStoredFrames;
}


//--------------------------------------------------------------
void StepPrinting::resetAllParameters()
{
	stepInterval =
		3;

	mixAmount =
		0.0f;

	feedbackAmount =
		0.0f;

	maxStoredFrames =
		4;

	clear();
}
