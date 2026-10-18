#pragma once

#include "Core/Math/Vector3.h"
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace Donut
{

class Character;
class ResourceManager;
class SimpleMesh;
class WorldPhysics;
namespace GL { class ShaderProgram; }

struct Pedestrian
{
	Vector3 position;
	Vector3 velocity;
	Vector3 targetDirection;
	float targetTimer;
	bool active;
	// Which of the pre-built bodies walks this pedestrian, or -1 for none.
	int body = -1;
	// Knocked flying by a car: seconds in the air, or -1 walking.
	float flying = -1.0f;
};

class PedestrianManager
{
	// Plays on a body the animation whose name ends with `suffix`.
	static bool playOn(Character& body, const std::string& suffix, bool loop);

public:
	PedestrianManager(WorldPhysics& physics);
	~PedestrianManager();

	void Spawn(const Vector3& nearPosition, float radius);
	void Update(double deltaTime, const Vector3& playerPosition);
	void Draw(GL::ShaderProgram& shader, SimpleMesh& mesh, const class Matrix4x4& viewProj);
	// The pedestrians with a body, as skinned characters.
	void DrawBodies(const class Matrix4x4& viewProj, GL::ShaderProgram& skinShader, const ResourceManager& resources);

	// Builds bodies from the people a level script lists for its pedestrians
	// (AddPed("male6", 2) ...), as far as their models have been extracted.
	// One per pedestrian slot, built once and reused: loading a character model
	// as each pedestrian appeared would stall the frame.
	void LoadModels(const std::string& levelScript);

	// The pedestrians within `radius` of a car going through them: they are
	// taken off the street (there is no knock-down animation) and counted.
	int KnockDown(const Vector3& position, float radius, const Vector3& carVelocity = Vector3::Zero);
	// Pedestrians standing within `radius` of `position`, still on their feet.
	int CountNear(const Vector3& position, float radius) const;

private:
	void despawnFar(float despawnRadius, const Vector3& playerPosition);

	WorldPhysics& _worldPhysics;
	std::vector<Pedestrian> _pedestrians;
	std::vector<std::unique_ptr<Character>> _bodies;
	std::vector<bool> _bodyInUse;
	std::mt19937 _rng;
	int _maxPedestrians = 15;
	float _spawnRadius = 50.0f;
	float _despawnRadius = 80.0f;
};

} // namespace Donut
