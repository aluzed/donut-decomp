#include "Game/GameCamera.h"

#include "AnimCamera.h"
#include "FreeCamera.h"
#include "Physics/WorldPhysics.h"
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Donut
{

GameCamera::GameCamera(): _view(std::make_unique<FreeCamera>())
{
	_view->SetPosition(Vector3(228.0f, 5.0f, -174.0f));
	_view->SetFOV(70.0f);
	_view->SetZNear(1.0f);
	_view->SetZFar(100000.0f);
}

GameCamera::~GameCamera() = default;

void GameCamera::Orbit(float yaw, float pitch)
{
	_yaw += yaw;
	_pitch = std::clamp(_pitch + pitch, -1.0f, 0.2f); // steep down, slightly up
}

void GameCamera::FollowVehicle(const Vehicle& car, double dt)
{
	const Vector3 vehPos = car.GetPosition();
	const Quaternion vehRot = car.GetRotation();
	const Vector3 camTarget = vehPos + Vector3(0, 1.5f, 0);
	// Camera sits behind the vehicle; forward is +Z (left-handed proj), so
	// "behind" is -Z in the vehicle's local frame. Use the vehicle's yaw
	// only: the chassis is a physics body that pitches and rolls over
	// bumps, and rotating the offset by the full orientation swung the
	// camera under the terrain. The look rotation below already takes
	// yaw alone.
	// Euler().Y was taken here for the yaw, but it is in degrees and folds
	// past 90: the camera stood behind only a car heading straight down +Z.
	const Quaternion vehYaw = Quaternion::MakeFromEuler(Vector3(0.0f, vehRot.Yaw(), 0.0f));
	const Vector3 targetPos = camTarget + vehYaw * Vector3(0, 3.0f, -12.0f);

	const float lerpFactor = 1.0f - std::exp(-8.0f * static_cast<float>(dt));
	if (_smoothPosition == Vector3::Zero)
		_smoothPosition = targetPos;
	_smoothPosition = _smoothPosition + (targetPos - _smoothPosition) * lerpFactor;

	// Looking at the car from wherever the camera ended up. A fixed pitch
	// along the car's heading lost it whenever the level pulled the camera
	// in: at sr2's grid, with the lawn behind, it sat 2m above the car and
	// looked out over it.
	const Vector3 cameraPos = Unoccluded(camTarget, _smoothPosition);
	const Vector3 toCar = camTarget - cameraPos;
	const float flat = std::sqrt(toCar.X * toCar.X + toCar.Z * toCar.Z);
	const float yaw = flat > 0.5f ? std::atan2(toCar.X, toCar.Z) : vehRot.Yaw();
	_view->SetPosition(cameraPos);
	_view->SetQuaternion(Looking(std::atan2(-toCar.Y, std::max(flat, 0.5f)), yaw));
}

void GameCamera::OrbitCharacter(const Vector3& character, double dt)
{
	// Mouse-controlled orbit camera: the camera yaw/pitch come from the
	// mouse, the camera orbits the character at a fixed distance and looks
	// at it. It follows the character's position but not its facing, so it
	// no longer swings around when the character turns.
	// _pitch is negative looking down; MakeFromEuler pitches +Z down for a
	// positive angle.
	const Quaternion camRot = Quaternion::MakeFromEuler(Vector3(-_pitch, _yaw, 0.0f));
	const Vector3 lookAt = character + Vector3(0.0f, 1.2f, 0.0f);
	const Vector3 forward = camRot * Vector3::Forward;
	const Vector3 targetPos = lookAt - forward * 6.0f;

	const float lerpFactor = 1.0f - std::exp(-12.0f * static_cast<float>(dt));
	if (_smoothPosition == Vector3::Zero)
		_smoothPosition = targetPos;
	_smoothPosition = _smoothPosition + (targetPos - _smoothPosition) * lerpFactor;

	_view->SetPosition(Unoccluded(lookAt, _smoothPosition));
	_view->SetQuaternion(Looking(-_pitch, _yaw));
}

void GameCamera::PlayMove(std::unique_ptr<AnimCamera> move)
{
	_move = std::move(move);
	_moveTime = 0.0;
}

bool GameCamera::UpdateMove(double dt)
{
	if (!_move)
		return false;
	const AnimCamera::Pose pose = _move->At(_moveTime);
	lookAlong(pose.position, pose.look);
	_moveTime += dt;
	if (_moveTime > _move->GetDuration())
		_move.reset();
	// Back to the player's camera from where it is, not swung in from afar.
	Snap();
	return true;
}

void GameCamera::FrameConversation(const Vector3& subject, const Vector3& facing, bool near, bool player,
                                   const Vector3* bestSide)
{
	Vector3 across = facing - subject;
	across.Y = 0.0f;
	// Standing on the same spot (the autopilot puts the player there): along
	// the way the camera was looking.
	if (across.LengthSquared() < 0.25f)
		across = Vector3(std::sin(_yaw), 0.0f, std::cos(_yaw)) * (player ? 1.0f : -1.0f);
	across = across.Normalized();
	// To one side of the line between them: the best side's, when the script
	// names one.
	Vector3 side(across.Z, 0.0f, -across.X);
	if (bestSide && (*bestSide - subject).Dot(side) < 0.0f)
		side = side * -1.0f;
	const Vector3 wanted = near ? subject + across * 1.8f + side * 0.5f
	                            : facing + across * 1.6f + side * 0.9f + Vector3(0.0f, 0.3f, 0.0f);
	const Vector3 from = Unoccluded(subject, wanted);
	lookAlong(from, subject - from);
	Snap();
}

void GameCamera::AddShake(float amount)
{
	_shake = std::max(_shake, amount);
}

void GameCamera::ApplyShake(double dt)
{
	if (_shake <= 0.0f)
		return;
	const float sx = (rand() % 100 - 50) * 0.001f * _shake;
	const float sy = (rand() % 100 - 50) * 0.001f * _shake;
	_view->SetPosition(_view->GetPosition() + Vector3(sx, sy, 0));
	_shake = std::max(0.0f, _shake - static_cast<float>(dt) * 5.0f);
}

Vector3 GameCamera::Unoccluded(const Vector3& subject, const Vector3& wanted) const
{
	// A follow camera at a fixed offset goes wherever the offset puts it: on
	// sr2's grid that was the middle of a tree, and half the screen was trunk.
	// If the level stands between the subject and the camera, bring the camera
	// in to just this side of it. Only static geometry counts -- a car passing
	// behind must not yank the view.
	constexpr float kCameraStandOff = 0.4f;
	Vector3 toCamera = wanted - subject;
	const float distance = toCamera.Length();
	if (!_physics || distance < 0.01f)
		return wanted;
	toCamera = toCamera / distance;

	Vector3 hit, normal;
	if (!_physics->CastRay(subject, toCamera, distance, hit, normal, true))
		return wanted;

	const float clear = (hit - subject).Length() - kCameraStandOff;
	return subject + toCamera * (clear > 0.0f ? clear : 0.0f);
}

Quaternion GameCamera::Looking(float pitchDown, float yaw)
{
	// FreeCamera's orientation is the view's rotation, world to camera: the
	// inverse of the camera's own. Both follow cameras used to pass their own
	// rotation, so the view turned the other way about Y. At a yaw of 0 or pi
	// that is the same thing, which is why it went unseen; anywhere else the
	// camera stood on the right side of its subject and looked away from it --
	// at sr2's grid, facing +X, it showed the lawn behind the car.
	return Quaternion::MakeFromEuler(Vector3(pitchDown, yaw, 0.0f)).Inverse();
}

void GameCamera::lookAlong(const Vector3& from, const Vector3& direction)
{
	const float flat = std::sqrt(direction.X * direction.X + direction.Z * direction.Z);
	_view->SetPosition(from);
	_view->SetQuaternion(Looking(std::atan2(-direction.Y, std::max(flat, 0.001f)), std::atan2(direction.X, direction.Z)));
}

} // namespace Donut
