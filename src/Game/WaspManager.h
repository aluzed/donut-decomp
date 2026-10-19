#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Donut
{

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
	};
	Result Update(double dt, const Vector3& player, bool attacking, float reach);
	void Draw(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj);

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
	struct Wasp
	{
		std::string id;
		Vector3 home;
		std::unique_ptr<CompositeModel> model;
		int state = 1;
		float frame = 0.0f;
		float yaw = 0.0f;
		bool destroyed = false;
		bool gone = false;
	};

	void readStates(const P3D::P3DChunk& stateProp);
	void enter(Wasp& wasp, int state);
	void save() const;

	std::unique_ptr<P3D::P3DFile> _effects;
	std::vector<State> _states;
	std::vector<Wasp> _wasps;
	double _time = 0.0;
};

} // namespace Donut
