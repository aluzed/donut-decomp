// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Fwd.h"
#include "Core/Math/Vector3.h"

#include <functional>
#include <map>
#include "Core/Math/Matrix4x4.h"
#include "Render/BillboardAnimation.h"
#include "Render/ParticleSystem.h"
#include "Render/TextureAnimation.h"
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class btCollisionObject;
class btRigidBody;

namespace Donut
{

namespace P3D
{
class P3DChunk;
struct StateProp;
struct SpatialTree;
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
	// The objects the level's action triggers work (the power plant's
	// platforms and doors, level 7's TD trapdoors): each on its own clock,
	// run (`speed` 1), stopped (0) or run back (-1); `wraps` round and round,
	// else held at either end. False when there is no such object.
	bool DriveAnimatedObject(const std::string& name, float speed, bool wraps);
	float GetAnimatedObjectSpeed(const std::string& name) const;
	// Where an animated object stands (its first prop, now); false when none.
	bool GetAnimatedObjectPosition(const std::string& name, Vector3& position) const;
	// The object an action trigger works: the one its locator names
	// ("PP_Splatform1": Splatform1Trans), else its target; empty when none.
	std::string FindAnimatedObject(const std::string& locator, const std::string& target) const;
	// Another object whose name ends with `suffix` ("switch1": the lever
	// z6switch1), other than `besides`, the nearest `near` of them; empty when
	// none.
	std::string FindAnimatedObjectEnding(const std::string& suffix, const std::string& besides, const Vector3& near) const;
	// Breaks the breakable objects within `radius` of `position` (crates,
	// vending machines, gas pumps), `event` done to them (HIT: struck,
	// STOMP: kicked from a jump, VEHICLE_HIT: driven into): one with states
	// (its state prop) goes to the state the event takes it to -- a crate is
	// struck three times before it gives -- one without plays its animation
	// once and stays as it ends. `solid` too, those that stand as bodies,
	// which a car breaks by running into them instead. How many it moved.
	// What the last physics step's contacts did to the props and the
	// breakable objects: knocked loose, broken, the car that hit them given
	// its speed back. Right after the step, before the car's damage is judged
	// from its speed: a crate run into stopped it dead for a step, and the
	// car took a crash's damage for it.
	void UpdateCollisions();
	int BreakObjectsNear(const Vector3& position, float radius, bool solid = true, const std::string& event = "HIT");
	// The coins the broken objects have given, where, since the last call
	// (their states' SPAWN_n_COINS callbacks).
	struct CoinDrop
	{
		Vector3 position;
		int coins = 0;
	};
	std::vector<CoinDrop> TakeCoinDrops();
	// Whether one stands unbroken within `radius`.
	bool HasBreakableNear(const Vector3& position, float radius) const;
	// The props the player can kick about (a mailbox, a bin, a lamp post
	// already down) within `radius` of `position`; and kicking them, away
	// from it: how many.
	bool HasKickablePropNear(const Vector3& position, float radius) const;
	int KickPropsNear(const Vector3& position, float radius);
	// The props knocked loose since the last call: the sound each makes (a
	// resource of sound\\scripts\\collide.spt), the effect its attribute row
	// names that the game makes itself ("eSpark"), and where.
	struct KnockedProp
	{
		std::string sound;
		std::string effect;
		Vector3 position;
	};
	std::vector<KnockedProp> TakeKnockedProps();
	// The level's lights (its terrain's light group "sun": an ambient light and
	// three directional ones) on `shader`: the cars and the characters are lit
	// by them; the world is lit already, by its vertex colours.
	void ApplyLights(GL::ShaderProgram& shader) const;
	// The way the level's sun shines (its "sun" group's first directional
	// light); false when it has none.
	bool GetSunDirection(Vector3& direction) const;
	// Its spatial tree (its terrain's: a kd-tree over the level, each node's box
	// and how many of the world's objects of each kind lie in it), for culling.
	const P3D::SpatialTree* GetSpatialTree() const { return _spatialTree.get(); }
	// Whether its sky (its world sphere: the sky, the sun's glare) is drawn:
	// not for a bonus game track, seen from far outside it.
	void SetSkyShown(bool shown) { _skyShown = shown; }
	void Draw(Matrix4x4& viewProj, const Vector3& cameraPos = Vector3::Zero);
	// The shader the level's own meshes are drawn with, uniforms as Draw left
	// them -- what any other P3D Mesh (a car's, say) has to be drawn with too.
	GL::ShaderProgram& GetWorldShader() const { return *_worldShader; }
	// Its billboard program, with the frame's uniforms set by Draw.
	GL::ShaderProgram& GetBillboardShader() { return *_billboardBatchShader; }
	void LoadP3D(const std::string& filename);
	// The level's effects file (art/lNN_fx.p3d): the particle effects a prop
	// gives off as it is hit (InstParticleSystem, 0x3001001: a hedge's
	// leaves, a mailbox's letters, a bin's rubbish).
	void LoadHitParticles(const P3D::P3DChunk& effectsRoot);

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
		// An event locator's event, its first word: the place's sounds
		// ("city", 7), its reverb ("burns_tunnel_in", 40), a jump...
		uint32_t event = 0;
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
	bool HasLocator(const std::string& name) const { return _locators.count(name) != 0; }
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
	// The effects file's particle effect of that type (the game's: 10 a
	// wheel's dirt spray, 11 its grass), or null.
	const ParticleFactory* GetHitParticleFactory(uint32_t type) const;
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
	bool _skyShown = true;
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
		// Worked by an action trigger: its own clock, how fast it runs (1,
		// 0 stopped, -1 back), and whether round and round or held at its ends.
		bool driven = false;
		double clock = 0.0;
		float speed = 0.0f;
		bool wraps = true;
		// Played once and left as it ends: a crate run into stays broken.
		bool breaks = false;
		bool broken = false;
		Vector3 position;
		// One of a breakable's breakings (BreakableObject), played where a prop
		// is hit and hidden once played; free when not playing.
		bool pooled = false;
		bool free = true;
		// Its collision, standing as static geometry until it breaks (a
		// crate, a vending machine): null when it has none. Its mass, and what
		// is heard and seen as it goes (its collision effect's sound, its
		// attribute row's particle effect).
		btRigidBody* body = nullptr;
		float mass = 100.0f;
		std::string sound, effect;
		// Its states (its wrapper's state prop: a crate's six), the one it is
		// in, when it went in, which of the state's callbacks have been
		// called, and what last hit it; null: no states, its animation played
		// through on a break.
		std::shared_ptr<const P3D::StateProp> states;
		int state = 0;
		double stateStartedAt = 0.0;
		std::vector<bool> called;
		const btCollisionObject* hitter = nullptr;
	};
	// Into state `state` of its state prop: what it shows; its callbacks are
	// called as the state's time comes to each.
	void enterState(AnimatedObject& object, CompositeModel& model, int state);
	void updateState(AnimatedObject& object, CompositeModel& model);
	void stateCallback(AnimatedObject& object, CompositeModel& model, uint32_t callback);
	std::vector<CoinDrop> _coinDrops;
	// Every light read (Light, 0x13000), by name, and the light groups
	// (LightGroup, 0x2380) naming them.
	struct Light
	{
		bool ambient = false;
		Vector3 color;
		Vector3 direction;
	};
	// The lights of the file being read, by name, and its groups' names: the
	// names repeat from file to file (each interior has its own
	// "Simpsons_sun_directionalShape3"), so a group is made of its own file's.
	std::map<std::string, Light> _lights;
	std::map<std::string, std::vector<std::string>> _fileLightGroups;
	std::unique_ptr<P3D::SpatialTree> _spatialTree;
	std::map<std::string, std::vector<Light>> _lightGroups;
	// Those that stand as bodies, by body.
	std::unordered_map<const btCollisionObject*, std::size_t> _breakingBodies;
	// Breaks one: its breaking plays, its body goes; `hitter`, given its
	// speed back, is what broke it (none, the player on foot).
	void breakObject(AnimatedObject& object, const btCollisionObject* hitter);
	// The animated objects run into hard enough break.
	void updateBreakingBodies();
	// The breakings of each breakable type: the game's breakable enum, the
	// first field of a BreakableObject (0x3001000), to the animated objects
	// that play it.
	std::map<uint32_t, std::vector<std::size_t>> _breakablePools;
	// Plays a breaking of `type` at `placement`; false when there is none, or
	// none free.
	bool playBreakable(uint32_t type, const Matrix4x4& placement);
	std::vector<AnimatedObject> _animatedObjects;
	// The props a car knocks about (DynamicPhysics: mailboxes, bins, lamp
	// posts, stop signs), each a rigid body its mesh instance follows. What
	// each does is its collision class (CollisionEffect, 0x3000600): 3 moves
	// from the first touch; 10, and the breakables but trees (4), stand until
	// something runs into them at speed. Its mass is its row of the collision
	// attribute table.
	struct Prop
	{
		btRigidBody* body;
		Matrix4x4 centre;
		std::size_t instances;
		std::size_t slot;
		float mass;
		bool fixed;
		std::string sound;
		// Heard once a tumble: a mailbox sent flying, not every bounce.
		bool heard = false;
		// Where it stood; one fallen far below, through the world, is taken
		// out of it rather than left falling for ever.
		float startY;
		bool gone = false;
		// A breakable's (class 4): its breaking's type, or -1; whether the prop
		// goes as it breaks (a hydrant, a stop sign) or stays (a tree shedding
		// leaves); where it stands; when it last broke.
		int breakable = -1;
		bool vanishes = false;
		// The particle effect it gives off as it is hit, or -1; and the name
		// its attribute row gives it ("eMail", "eSpark").
		int hitParticles = -1;
		std::string effect;
		Matrix4x4 placement;
		double brokeAt = -100.0;
	};
	std::vector<Prop> _props;
	// The fixed ones, by body.
	std::unordered_map<const void*, std::size_t> _fixedProps;
	std::vector<KnockedProp> _knockedProps;
	// A prop hit: its sound, and its particle effect, where it is.
	void propHit(const Prop& prop, const Vector3& position);
	// The hit particle effects, by the game's particle type (an
	// InstParticleSystem's first field), as factory names; and those playing:
	// each gives off what its timeline says, then lives out its debris.
	std::map<uint32_t, std::string> _hitParticleFactories;
	struct HitParticles
	{
		std::unique_ptr<ParticleSystem> system;
		Matrix4x4 placement;
		double age = 0.0;
		float frames = 0.0f;
		float frameRate = 30.0f;
	};
	std::vector<HitParticles> _hitParticles;
	void updateProps();
	// The collision attribute table (art/atc/atc.p3d, Atc 0x3000602), by the
	// row a CollisionEffect names: sound, particle effect, breakable object,
	// friction, mass, elasticity.
	struct CollisionAttributes
	{
		std::string sound, particle, breakable;
		float friction = 0.0f, mass = 100.0f, elasticity = 0.0f;
	};
	std::vector<CollisionAttributes> _collisionAttributes;
	void loadCollisionAttributes();
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
	struct AnimatedQuad
	{
		std::size_t batch;
		int quad;
		float frameRate;
		float frames;
		BillboardAnimation::Quad animation;
	};
	std::vector<AnimatedQuad> _billboardAnimations;
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
		// A breakable's (its debris, its dust): played from its breaking, keyed
		// to that animation, the index into _animatedObjects; -1 for one always on.
		int breakable = -1;
		// Its name, which its object's states key it by (EFX_<name>).
		std::string effect;
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
