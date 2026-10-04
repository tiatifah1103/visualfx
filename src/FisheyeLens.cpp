#include "FisheyeLens.hpp"

#include <algorithm>
#include <cmath>

//--------------------------------------------------------------
FisheyeLens::FisheyeLens()
: distortionStrength(0.5f),
  baseDistortion(0.0f),
  currentDistortion(0.0f),
  distortionSmoothing(0.1f),
  bassLevel(0.0f),
  maxDistortion(2.5f),
  pulseFrequency(2.0f),
  timeCounter(0.0f),
  nextPulseTime(0.0f),
  pulseDuration(0.0f),
  currentPulseStrength(0.0f),
  movementSpeed(1.0f),
  movementAmount(0.0f),
  vibrationAmount(0.0f),
  vibrationSpeed(1.0f)
{
}

//--------------------------------------------------------------
void FisheyeLens::setup(float _distortionStrength)
{
	baseDistortion = _distortionStrength;

	currentDistortion = 0.0f;
	distortionSmoothing = 0.15f;

	currentOffset.set(0.0f, 0.0f);
	targetOffset.set(0.0f, 0.0f);

	timeCounter = 0.0f;
	nextPulseTime = 0.0f;
	currentPulseStrength = 0.0f;
}

//--------------------------------------------------------------
void FisheyeLens::setBassLevel(float level)
{
	bassLevel =
		ofClamp(level,
				0.0f,
				1.0f);

	movementAmount =
		bassLevel *
		42.0f;

	vibrationAmount =
		bassLevel *
		0.11f;

	vibrationSpeed =
		ofMap(bassLevel,
			  0.0f,
			  1.0f,
			  1.0f,
			  4.5f);

	pulseFrequency =
		ofMap(bassLevel,
			  0.0f,
			  1.0f,
			  0.2f,
			  4.5f);
}

//--------------------------------------------------------------
void FisheyeLens::setPulseFrequency(float freq)
{
	pulseFrequency =
		std::max(0.05f,
				 freq);
}

//--------------------------------------------------------------
void FisheyeLens::setMaxDistortion(float max)
{
	maxDistortion =
		std::max(0.0f,
				 max);
}

//--------------------------------------------------------------
void FisheyeLens::update(const ofTexture& videoTexture)
{
	const float deltaTime =
		ofClamp(ofGetLastFrameTime(),
				0.001f,
				0.1f);

	timeCounter += deltaTime;

	currentDistortion +=
		(distortionStrength -
		 currentDistortion)
		*
		distortionSmoothing;

	updatePulsing(deltaTime);
	updateMovement(deltaTime);

	renderTexture(videoTexture,
				  distortedFrame);
}

//--------------------------------------------------------------
void FisheyeLens::renderTo(const ofTexture& videoTexture,
						   ofFbo& destination)
{
	renderTexture(videoTexture,
				  destination);
}

//--------------------------------------------------------------
void FisheyeLens::renderTexture(const ofTexture& videoTexture,
								ofFbo& destination)
{
	if (!videoTexture.isAllocated())
		return;

	const int width =
		static_cast<int>(
			videoTexture.getWidth()
		);

	const int height =
		static_cast<int>(
			videoTexture.getHeight()
		);

	if (width <= 0 ||
		height <= 0)
	{
		return;
	}

	if (!destination.isAllocated() ||
		destination.getWidth() != width ||
		destination.getHeight() != height)
	{
		destination.allocate(width,
							 height,
							 GL_RGBA);
	}

	const float finalDistortion =
		calculateFinalDistortion();

	const float maxDim =
		static_cast<float>(
			std::max(width,
					 height)
		);

	const float halfMax =
		maxDim *
		0.5f;

	const float centreX =
		width *
		0.5f +
		currentOffset.x;

	const float centreY =
		height *
		0.5f +
		currentOffset.y;

	destination.begin();

	ofClear(0, 0, 0, 255);

	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_TRIANGLES);

	const int step =
		10;

	for (int y = 0;
		 y < height - step;
		 y += step)
	{
		for (int x = 0;
			 x < width - step;
			 x += step)
		{
			for (int dy = 0;
				 dy <= step;
				 dy += step)
			{
				for (int dx = 0;
					 dx <= step;
					 dx += step)
				{
					const float srcX =
						x +
						dx;

					const float srcY =
						y +
						dy;

					const float nx =
						(srcX -
						 centreX)
						/
						halfMax;

					const float ny =
						(srcY -
						 centreY)
						/
						halfMax;

					const float r =
						std::sqrt(
							nx * nx +
							ny * ny
						);

					const float vibration =
						vibrationAmount *
						std::sin(
							timeCounter *
							vibrationSpeed *
							10.0f
						);

					const float theta =
						std::atan(r);

					float distortedR =
						(r > 0.0f)
						?
						theta / r
						:
						1.0f;

					distortedR =
						1.0f +
						finalDistortion *
						(distortedR -
						 1.0f)
						*
						(1.0f +
						 vibration);

					if (currentPulseStrength >
						0.0f)
					{
						const float pulseDistort =
							currentPulseStrength *
							0.42f *
							std::sin(
								r *
								PI *
								2.0f
							);

						distortedR +=
							pulseDistort;
					}

					const float distortedX =
						nx *
						distortedR;

					const float distortedY =
						ny *
						distortedR;

					const float u =
						centreX +
						distortedX *
						halfMax;

					const float v =
						centreY +
						distortedY *
						halfMax;

					mesh.addVertex(
						glm::vec3(
							srcX,
							srcY,
							0.0f
						)
					);

					mesh.addTexCoord(
						glm::vec2(
							u,
							v
						)
					);
				}
			}

			const int i =
				mesh.getNumVertices();

			mesh.addIndex(i - 4);
			mesh.addIndex(i - 3);
			mesh.addIndex(i - 2);

			mesh.addIndex(i - 4);
			mesh.addIndex(i - 2);
			mesh.addIndex(i - 1);
		}
	}

	ofSetColor(255);

	videoTexture.bind();
	mesh.draw();
	videoTexture.unbind();

	destination.end();
}

//--------------------------------------------------------------
void FisheyeLens::updatePulsing(float deltaTime)
{
	if (currentPulseStrength >
		0.0f)
	{
		currentPulseStrength -=
			deltaTime *
			2.0f;

		if (currentPulseStrength <
			0.0f)
		{
			currentPulseStrength =
				0.0f;
		}
	}

	if (timeCounter >
			nextPulseTime &&
		bassLevel >
			0.28f)
	{
		const float pulseIntensity =
			ofRandom(
				0.62f,
				0.96f
			)
			*
			bassLevel;

		currentPulseStrength =
			pulseIntensity;

		const float safeFrequency =
			std::max(
				0.05f,
				pulseFrequency *
				std::max(
					bassLevel,
					0.05f
				)
			);

		const float nextPulseDelay =
			ofRandom(
				0.55f,
				1.85f
			)
			/
			safeFrequency;

		nextPulseTime =
			timeCounter +
			nextPulseDelay;

		pulseDuration =
			ofRandom(
				0.1f,
				0.3f
			)
			*
			(1.0f -
			 bassLevel *
			 0.45f);
	}
}

//--------------------------------------------------------------
void FisheyeLens::updateMovement(float deltaTime)
{
	currentOffset +=
		(targetOffset -
		 currentOffset)
		*
		movementSpeed *
		deltaTime;

	if (currentOffset.distance(
			targetOffset
		) <
			4.0f &&
		bassLevel >
			0.10f)
	{
		const float moveAmount =
			movementAmount *
			bassLevel;

		targetOffset.set(
			ofRandom(
				-moveAmount,
				moveAmount
			),
			ofRandom(
				-moveAmount,
				moveAmount
			)
		);

		movementSpeed =
			ofMap(
				bassLevel,
				0.0f,
				1.0f,
				0.45f,
				2.6f
			);
	}

	if (bassLevel <=
		0.10f)
	{
		targetOffset.set(
			0.0f,
			0.0f
		);
	}
}

//--------------------------------------------------------------
float FisheyeLens::calculateFinalDistortion() const
{
	const float bassDistortion =
		std::pow(
			bassLevel,
			3.0f
		)
		*
		maxDistortion;

	float combined =
		currentDistortion +
		bassDistortion;

	combined *=
		1.0f +
		currentPulseStrength *
		0.42f;

	return combined;
}

//--------------------------------------------------------------
void FisheyeLens::apply(float x,
						float y,
						float width,
						float height)
{
	ofSetColor(255);

	distortedFrame.draw(
		x,
		y,
		width,
		height
	);
}

//--------------------------------------------------------------
void FisheyeLens::setDistortionStrength(float strength)
{
	distortionStrength =
		ofClamp(
			strength,
			0.0f,
			maxDistortion
		);
}

//--------------------------------------------------------------
float FisheyeLens::getDistortionStrength() const
{
	return distortionStrength;
}

//--------------------------------------------------------------
void FisheyeLens::reset()
{
	setDistortionStrength(
		0.0f
	);

	bassLevel =
		0.0f;

	currentPulseStrength =
		0.0f;

	currentOffset.set(
		0.0f,
		0.0f
	);

	targetOffset.set(
		0.0f,
		0.0f
	);
}
