#pragma once

#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector3.h"

#include <memory>

namespace Donut
{

class AnimCamera;
class FreeCamera;
class Vehicle;
class WorldPhysics;

// The game's camera: where the view is, and how it gets there -- behind the car
// driven, orbiting the character on foot under the mouse, a mission's camera
// move, a conversation's shot -- shaken by impacts, and kept this side of walls.
class GameCamera
{
public:
	GameCamera();
	~GameCamera();

	// The view drawn with: position, orientation, field of view, planes.
	FreeCamera& View() { return *_view; }
	const FreeCamera& View() const { return *_view; }

	// The level's collision, which the camera keeps in front of; none between levels.
	void SetPhysics(const WorldPhysics* physics) { _physics = physics; }

	// The mouse's orbit on foot, already scaled to radians.
	void Orbit(float yaw, float pitch);
	float GetYaw() const { return _yaw; }
	// Facing that way (into a room just entered).
	void SetYaw(float yaw) { _yaw = yaw; }

	// Behind the car, eased after it, looking at it.
	void FollowVehicle(const Vehicle& car, double dt);
	// Around the character at the mouse's angle, eased after it.
	void OrbitCharacter(const Vector3& character, double dt);
	// The next follow or orbit starts where it is wanted, not eased in from
	// wherever the view was (after a teleport, a camera move, a shot).
	void Snap() { _smoothPosition = Vector3::Zero; }

	// A mission's camera move, played from its start; true while it plays.
	void PlayMove(std::unique_ptr<AnimCamera> move);
	bool UpdateMove(double dt);

	// A conversation's shot of `subject` over the shoulder of `facing` (both
	// head heights): from near the subject, or far behind the one facing it,
	// on the side of `bestSide` when there is one.
	void FrameConversation(const Vector3& subject, const Vector3& facing, bool near, bool player,
	                       const Vector3* bestSide);

	void AddShake(float amount);
	// The view jolted by the shake left, which dies away.
	void ApplyShake(double dt);

	// `wanted`, or nearer `subject` when the level stands between them.
	Vector3 Unoccluded(const Vector3& subject, const Vector3& wanted) const;
	// The view's rotation looking down by `pitchDown` along `yaw`.
	static Quaternion Looking(float pitchDown, float yaw);

private:
	void lookAlong(const Vector3& from, const Vector3& direction);

	std::unique_ptr<FreeCamera> _view;
	const WorldPhysics* _physics = nullptr;
	Vector3 _smoothPosition = Vector3::Zero;
	float _yaw = 0.0f;    // the orbit's, on foot
	float _pitch = -0.2f; // negative looks down
	float _shake = 0.0f;
	std::unique_ptr<AnimCamera> _move;
	double _moveTime = 0.0;
};

} // namespace Donut
