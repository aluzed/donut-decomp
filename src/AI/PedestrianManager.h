#pragma once

#include "Core/Math/Vector3.h"
#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>
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
	// Standing a moment where they have got to, seconds left.
	float waiting = 0.0f;
	// Running clear of a car, seconds left.
	float fleeing = 0.0f;
	// How far walked since the last step heard, metres.
	float stride = 0.0f;
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
	void Draw(GL::ShaderProgram& shader, SimpleMesh& mesh, const struct Matrix4x4& viewProj);
	// The pedestrians with a body, as skinned characters.
	// Where the pedestrians with a body stand, for their shadows.
	std::vector<Vector3> GetBodyPositions() const;
	// Where the pedestrians with a body put a foot down since the last call:
	// a stride's walk each (their footsteps).
	std::vector<Vector3> TakeSteps() { return std::exchange(_steps, {}); }
	void DrawBodies(const struct Matrix4x4& viewProj, GL::ShaderProgram& skinShader, const ResourceManager& resources);

	// Reads the groups of people a level script lists for its pedestrians
	// (CreatePedGroup(1); AddPed("olady1", 2); ...; ClosePedGroup()), as far as
	// their models have been extracted, and builds the bodies of the first.
	// One per pedestrian slot, built once and reused: loading a character model
	// as each pedestrian appeared would stall the frame.
	void LoadModels(const std::string& levelScript);
	// The streets take their people from that group from now on (UsePedGroup,
	// one a district): its bodies built, the pedestrians about now cleared to
	// come back as its people.
	void UseGroup(int group);

	// The pedestrians within `radius` of a car going through them: they are
	// taken off the street (there is no knock-down animation) and counted.
	int KnockDown(const Vector3& position, float radius, const Vector3& carVelocity = Vector3::Zero);
	// Pedestrians standing within `radius` of `position`, still on their feet.
	// Those within `radius` of a car coming at `carVelocity` run clear of it,
	// sideways from its path, in a dash.
	void Scare(const Vector3& carPosition, float radius, const Vector3& carVelocity);
	int CountNear(const Vector3& position, float radius) const;

private:
	void despawnFar(float despawnRadius, const Vector3& playerPosition);

	WorldPhysics& _worldPhysics;
	std::vector<Pedestrian> _pedestrians;
	std::vector<Vector3> _steps;
	std::vector<std::unique_ptr<Character>> _bodies;
	// Each group's people, by its number: a name as many times as the script's
	// count for it, which shares out the slots.
	std::map<int, std::vector<std::string>> _groups;
	int _group = -1;
	void buildBodies(const std::vector<std::string>& people);
	std::vector<bool> _bodyInUse;
	std::mt19937 _rng;
	int _maxPedestrians = 15;
	float _spawnRadius = 50.0f;
	float _despawnRadius = 80.0f;
};

} // namespace Donut
