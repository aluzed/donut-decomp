#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"
#include "Render/BillboardAnimation.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class BillboardBatch;
class ParticleFactory;
class ParticleSystem;
class CompositeModel;
class Level;
namespace GL
{
class ShaderProgram;
}
namespace P3D
{
class P3DChunk;
class P3DFile;
} // namespace P3D

// The wasp cameras: Shelley's "beecamera", hovering at the spawn points a
// level's script lists (AddSpawnPointByLocatorScript), whose locators are in
// art/missions/levelNN/wasps.p3d. Its model and its states are in the level's
// effects file (art/lNN_fx.p3d): a state prop (StaticPropData, 0x8020000)
// whose states each play a stretch of the model's one animation and show some
// of its props -- coming in, hovering, alerted, attacking, firing, destroyed.
// A wasp is destroyed by the player jumping at it or driving into it, for
// coins; those destroyed stay so (donut_wasps.dat).
class WaspManager
{
public:
	WaspManager(int level, const std::string& levelScript, const Level& world);
	~WaspManager();

	// `player` where the player is; `attacking` whether a jump or a car could
	// destroy a wasp it reaches. The coins earned, and wasps destroyed, this
	// frame.
	struct Result
	{
		int coins = 0;
		int destroyed = 0;
		// Shots that hit the player, and what they cost.
		float damage = 0.0f;
		// What is heard, and where: the buzz, a shot, a hit or a miss, a wasp
		// coming apart (sound\soundfx\wasp\).
		std::vector<std::pair<std::string, Vector3>> sounds;
	};
	Result Update(double dt, const Vector3& player, bool attacking, float reach);
	void Draw(GL::ShaderProgram& worldShader, GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj);

	int GetDestroyed() const;
	int GetTotal() const { return static_cast<int>(_wasps.size()); }

private:
	struct State
	{
		float firstFrame = 0.0f, lastFrame = 0.0f, speed = 1.0f;
		bool loops = false;
		// Played through, on to this state; -1 to stay.
		int next = -1;
		std::map<std::string, bool> visible;
	};
	// AddBehaviour: ATTRACTION (comes no closer than `nearest`, follows within
	// `farthest`, at `speed` km/h; -1 stays put) and ATTACK_PLAYER (fires within
	// `range` metres, `arc` degrees of its heading, every `period` seconds).
	struct Behaviour
	{
		float nearest = 4.0f, farthest = 10.0f, speed = 10.0f;
		float range = 10.0f, arc = 5.0f, period = 3.0f;
	};
	struct Wasp
	{
		std::string id;
		Behaviour behaviour;
		Vector3 position;
		double nextShot = 0.0;
		double nextBuzz = 0.0;
		// Out only while the player is within its spawn point's radius
		// (AddSpawnPointByLocatorScript's fifth argument): it comes in when
		// they enter it, and goes when they are well away.
		float spawnRadius = 30.0f;
		bool present = false;
		Vector3 home;
		std::unique_ptr<CompositeModel> model;
		// Its particle effects (its trail, its coming in), each at a joint and
		// shown as its state says.
		struct Effect
		{
			std::string name;
			int joint;
			std::unique_ptr<ParticleSystem> system;
		};
		std::vector<Effect> effects;
		int state = 1;
		float frame = 0.0f;
		float yaw = 0.0f;
		bool destroyed = false;
		bool gone = false;
	};

	void readStates(const P3D::P3DChunk& stateProp);
	void readBehaviours(const std::string& levelScript);
	std::map<std::string, Behaviour> _behaviours;
	float _turnRate = 1.5f;     // radians a second (SetActorRotationSpeed)
	float _shotDamage = 5.0f;   // SetProjectileStats
	void enter(Wasp& wasp, int state);
	void save() const;

	// The shot ("waspray": two glowing billboards) in flight to where the player
	// stood, hitting them if they are still there; and the wasps' own glows
	// (their lens, signal), each hung from a joint.
	struct Shot
	{
		Vector3 position;
		Vector3 velocity;
		float travelled = 0.0f;
		float range = 10.0f;
	};
	std::vector<Shot> _shots;
	float _shotSpeed = 60.0f / 3.6f;
	std::vector<std::unique_ptr<BillboardBatch>> _shotSprites;
	std::vector<std::pair<std::string, std::unique_ptr<BillboardBatch>>> _glows;
	// Their animations over the wasp's timeline (BQG_*): size, colour, visibility.
	std::vector<BillboardAnimation::Quad> _glowAnimations;
	// The effects file's particle factories, and which the beecamera's effect
	// list places where: effect name, joint, factory.
	std::map<std::string, std::unique_ptr<ParticleFactory>> _factories;
	struct EffectSlot
	{
		std::string name;
		int joint;
		std::string factory;
	};
	std::vector<EffectSlot> _effectSlots;

	std::unique_ptr<P3D::P3DFile> _effects;
	std::vector<State> _states;
	std::vector<Wasp> _wasps;
	double _time = 0.0;
};

} // namespace Donut
