// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"

#include <memory>
#include <string>
#include <vector>

namespace Donut
{

namespace P3D
{
class P3DChunk;
}

// A camera move from a mission's camera file (art/missions/levelNN/mission2cam.p3d):
// its animation ("CAM_mission2camShape", 30 frames a second) holds where the
// camera is (TRAN, world space), the way it looks (LOOK) and its up (UP), each
// keyed at some frames; and its field of view, near and far planes (FOV, NCLP,
// FCLP), which are left to the game's own camera. A mission plays one as it
// starts (SetMissionStartCameraName), a stage as it begins (SetAnimatedCameraName).
class AnimCamera
{
public:
	// The camera `camera` ("mission2camShape") of the file, or its first; null
	// when the file is missing or holds no camera move.
	static std::unique_ptr<AnimCamera> LoadP3D(const std::string& filename, const std::string& camera = "");

	struct Pose
	{
		Vector3 position;
		Vector3 look;
		Vector3 up;
	};
	Pose At(double seconds) const;
	double GetDuration() const { return _frames / _frameRate; }

private:
	struct Track
	{
		std::vector<float> frames;
		std::vector<Vector3> values;
		Vector3 At(float frame) const;
	};
	bool load(const P3D::P3DChunk& animation);

	Track _position, _look, _up;
	float _frames = 0.0f;
	float _frameRate = 30.0f;
};
} // namespace Donut
