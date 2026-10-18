// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Fwd.h"
#include "Core/Math/Vector3.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

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
	void Draw(Matrix4x4& viewProj, const Vector3& cameraPos = Vector3::Zero);
	// The shader the level's own meshes are drawn with, uniforms as Draw left
	// them -- what any other P3D Mesh (a car's, say) has to be drawn with too.
	GL::ShaderProgram& GetWorldShader() const { return *_worldShader; }
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
		Vector3 position;
		// The volume's own axes. Boxes are not all axis-aligned: L1's loader
		// triggers sit at 90 and 45 degrees.
		Vector3 axes[3];
		// Half extents along those axes for a box, radius in X for a sphere.
		Vector3 bounds;
		bool isRect;

		bool Contains(const Vector3& point) const;
	};

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
	};
	// By file name ("l1i01.p3d"); false when that file holds no interior.
	bool GetInterior(const std::string& file, Interior& interior) const;
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
