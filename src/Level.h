// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Fwd.h"
#include "Core/Math/Vector3.h"

#include <functional>
#include <map>
#include "Core/Math/Matrix4x4.h"
#include "Render/ParticleSystem.h"
#include "Render/TextureAnimation.h"
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class btRigidBody;

namespace Donut
{

namespace P3D
{
class P3DChunk;
}

namespace GL
{
class ShaderProgram;
}

class BillboardBatch;
class CompositeModel;
class LineRenderer;
class Entity;
class ResourceManager;
class WorldSphere;

class Level
{
public:
	Level();
	~Level();

	void Update(double deltatime);
	// Plays, once, the animated objects of that name that wait to be worked
	// (a doorbell's button, its porch light switch). How many there were.
	int PlayAnimatedObject(const std::string& name);
	// Breaks the breakable objects within `radius` of `position` (crates,
	// vending machines, gas pumps): each plays its animation once and stays
	// as it ends. How many broke.
	int BreakObjectsNear(const Vector3& position, float radius);
	void Draw(Matrix4x4& viewProj, const Vector3& cameraPos = Vector3::Zero);
	// The shader the level's own meshes are drawn with, uniforms as Draw left
	// them -- what any other P3D Mesh (a car's, say) has to be drawn with too.
	GL::ShaderProgram& GetWorldShader() const { return *_worldShader; }
	// Its billboard program, with the frame's uniforms set by Draw.
	GL::ShaderProgram& GetBillboardShader() { return *_billboardBatchShader; }
	void LoadP3D(const std::string& filename);

	void DynaLoadData(const std::string& dynaLoadData);

	void ImGuiDebugWindow(bool* p_open) const;

	class Path
	{
	public:
		std::vector<Vector3> points;
	};

	// What a trigger volume belongs to. The values are the game's locator types,
	// confirmed against L1's data: dynamic-zone triggers carry a DynaLoad string
	// ("l1z3.p3d:l1z4.p3d:..."), interior entrances the interior's name.
	enum class LocatorType : uint32_t
	{
		Event = 0,
		Script = 1,
		Generic = 2,
		CarStart = 3,
		Spline = 4,
		DynamicZone = 5,
		Occlusion = 6,
		InteriorEntrance = 7,
		Directional = 8,
		Action = 9,
		Fov = 10,
		BreakableCamera = 11,
		StaticCamera = 12,
		PedGroup = 13,
		Coin = 14,
	};

	struct Trigger
	{
		std::string name;
		// The locator the volume hangs off, its type, and the text at the head of
		// its data (empty when it carries none).
		std::string locator;
		LocatorType type;
		std::string data;
		// An action locator's action, the third string of its data:
		// "SummonVehiclePhone" (a phone booth), "Wrench", "Doorbell"...
		std::string action;
		// Its second string: the object it works ("skinner_switch11").
		std::string actionTarget;
		Vector3 position;
		// The volume's own axes. Boxes are not all axis-aligned: L1's loader
		// triggers sit at 90 and 45 degrees.
		Vector3 axes[3];
		// Half extents along those axes for a box, radius in X for a sphere.
		Vector3 bounds;
		bool isRect;

		bool Contains(const Vector3& point) const;
	};

	const std::vector<Trigger>& GetTriggers() const { return _triggers; }

	struct TriggerEvent
	{
		const Trigger* trigger;
		bool entered; // false: left
	};

	Vector3 GetLocatorPosition(const std::string& name) const;
	// The heading a car-start locator faces, in radians about +Y (0 faces +Z,
	// pi/2 faces +X). False for a locator that carries none.
	bool GetLocatorHeading(const std::string& name, float& heading) const;
	// Where the level's own coins lie: one per coin locator, in every file loaded.
	const std::vector<Vector3>& GetCoinPositions() const { return _coinPositions; }
	// The collector cards' locators ("card11" is level 1's first), in load order.
	struct Card
	{
		std::string name;
		Vector3 position;
	};
	const std::vector<Card>& GetCards() const { return _cards; }

	// An interior, as its own P3D file lays it out, somewhere under the map:
	// where the player appears on coming in, and the spot that takes them back
	// out. Interiors share locator names (every one has an InteriorEntryStart),
	// so these are read per file rather than from the level-wide table.
	struct Interior
	{
		Vector3 entry;
		Vector3 exit;
		// The room's middle (InteriorOrigin), to face on coming in.
		Vector3 origin;
		// The short name its own locators carry, "int_<tag>_...": "kwik" for
		// the Kwik-E-Mart, "ele" for the school, "dmv", "moe"...
		std::string tag;
	};
	// By file name ("l1i01.p3d"); false when that file holds no interior.
	bool GetInterior(const std::string& file, Interior& interior) const;
	// The interior file a door leads into. A door's locator names the place
	// ("KwikEMart", "SpringfieldElementary"), the interior only its tag, so the
	// door is matched to the interior whose tag its name contains -- the
	// longest such tag. Empty when none does.
	std::string FindInteriorFile(const std::string& door) const;
	// Draw only what `file` loaded (an interior, while the player is in it), or
	// everything again when empty. Inside, the rest of the level is the town
	// overhead, seen from underneath.
	void SetDrawOnly(const std::string& file) { _drawOnly = file; }
	const std::vector<Path>& GetPaths() const { return _paths; }

	// Named camera/animation splines. Spline chunks are nested inside the level
	// graph rather than sitting at the root, so they are gathered recursively.
	const std::map<std::string, std::vector<Vector3>>& GetSplines() const { return _splines; }

	// The real road network: Intersection nodes joined by Road links, which name
	// their two endpoints. Unlike the Path loops, this graph is connected.
	//
	// A Road is not a straight line between its endpoints -- it carries an ordered
	// chain of RoadSegment children, one every ~8m, whose transforms trace the
	// road's actual shape. Without them a route between two junctions 118m apart
	// is a straight line through whatever buildings stand in the way.
	//
	// Some roads are shortcuts: cuts through car parks and forecourts, which carry
	// no traffic and come with a difficulty value. They are not streets -- the two on M1race's route cross a raised car park
	// that ends in a 3m retaining wall, and a car park closed off by a fence.
	struct RoadLink
	{
		std::string start;
		std::string end;
		std::vector<Vector3> points;
		bool shortcut = false;
	};
	const std::map<std::string, Vector3>& GetIntersections() const { return _intersections; }
	const std::vector<RoadLink>& GetRoads() const { return _roads; }
	// RoadSegments whose named RoadDataSegment was never loaded; their node sits on
	// the tile corner rather than its middle. Should be zero.
	int GetUnresolvedRoadTiles() const { return _unresolvedRoadTiles; }
	bool CheckTrigger(const Vector3& pos, const std::string& name) const;
	// Tracks which volumes `pos` is inside and reports the changes since the
	// last call: each entry and each exit once, not every frame it stays inside.
	std::vector<TriggerEvent> UpdateTriggers(const Vector3& pos);

private:
	void collectSplines(const P3D::P3DChunk& chunk);
	void loadRegion(const std::string& filename);
	void unloadRegion(const std::string& filename);

	std::unique_ptr<WorldSphere> _worldSphere;
	std::vector<std::unique_ptr<Entity>> _entities;
	std::vector<std::unique_ptr<Entity>> _instances;
	std::vector<std::unique_ptr<BillboardBatch>> _billboardBatches;
	std::unique_ptr<GL::ShaderProgram> _worldShader;
	std::unique_ptr<GL::ShaderProgram> _worldInstancedShader;
	std::unique_ptr<GL::ShaderProgram> _billboardBatchShader;

	std::vector<std::unique_ptr<CompositeModel>> _compositeModels;
	// Those of them that play an animation of their own: round and round, or
	// once when worked (a doorbell, a switch), from `startedAt`.
	struct AnimatedObject
	{
		std::size_t index;
		std::string name;
		bool loops;
		double startedAt = -1.0;
		// Played once and left as it ends: a crate run into stays broken.
		bool breaks = false;
		bool broken = false;
		Vector3 position;
	};
	std::vector<AnimatedObject> _animatedObjects;
	// The collision of animated objects (AnimCollision: the power plant's
	// platforms and fans): kinematic bodies following a joint of their object,
	// at a fixed offset from it.
	struct MovingVolume
	{
		btRigidBody* body;
		std::size_t composite;
		int joint;
		Matrix4x4 offset;
	};
	std::vector<MovingVolume> _movingVolumes;
	// Billboards that belong to an animated object (a siren's lights): the
	// object and the joint each hangs from, by batch.
	struct AttachedBillboard
	{
		std::size_t composite;
		int joint;
	};
	std::map<std::size_t, AttachedBillboard> _attachedBillboards;
	// Billboards whose colour is animated (BQG_*: a siren flashing): which quad
	// of which batch, and its colour from each key frame on.
	struct BillboardAnimation
	{
		std::size_t batch;
		int quad;
		float frameRate;
		float frameCount;
		std::vector<std::pair<float, Vector4>> colors;
		int shown = -1;
	};
	std::vector<BillboardAnimation> _billboardAnimations;
	// Particle effects (steam, smoke, a tire fire): the level's factories by
	// name, and the systems running, each where a joint of an animated object
	// is, with the file it came from (for SetDrawOnly).
	std::map<std::string, std::unique_ptr<ParticleFactory>> _particleFactories;
	struct PlacedParticles
	{
		std::unique_ptr<ParticleSystem> system;
		std::size_t composite;
		int joint;
		std::string file;
	};
	std::vector<PlacedParticles> _particles;
	void drawBillboards(Matrix4x4& viewProj, bool opaque, const std::function<bool(std::size_t)>& shown);
	void moveVolumes();
	// The level's texture animations (a river's water, a pool, an arcade
	// screen), round and round, and the texture each shows now.
	std::vector<std::pair<TextureAnimation, std::string>> _textureAnimations;
	double _animationTime = 0.0;

	std::vector<Path> _paths;
	std::map<std::string, std::vector<Vector3>> _splines;
	std::map<std::string, Vector3> _intersections;
	std::vector<RoadLink> _roads;
	// Tile-local centre of each RoadDataSegment, by name; RoadSegment names one.
	std::map<std::string, Vector3> _roadTileCentres;
	int _unresolvedRoadTiles = 0;
	std::vector<Trigger> _triggers;
	// Parallel to _triggers: whether the tracked position was inside each one.
	std::vector<bool> _insideTrigger;
	std::set<std::string> _loadedRegions;
	std::set<std::string> _loadedP3Ds;
	std::map<std::string, Vector3> _locators;
	std::map<std::string, float> _locatorHeadings;
	std::vector<Vector3> _coinPositions;
	std::vector<Card> _cards;
	std::map<std::string, Interior> _interiors;

	// What each file put in the draw lists, as [begin, end) index ranges, so a
	// single file can be drawn on its own.
	struct DrawRange
	{
		std::size_t entities[2];
		std::size_t instances[2];
		std::size_t composites[2];
		std::size_t billboards[2];
	};
	std::map<std::string, DrawRange> _drawRanges;
	std::string _drawOnly;
};

} // namespace Donut
