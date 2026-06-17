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
	void LoadP3D(const std::string& filename);

	void DynaLoadData(const std::string& dynaLoadData);

	void ImGuiDebugWindow(bool* p_open) const;

	class Path
	{
	public:
		std::vector<Vector3> points;
	};

	Vector3 GetLocatorPosition(const std::string& name) const;
	const std::vector<Path>& GetPaths() const { return _paths; }

private:
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
	std::set<std::string> _loadedRegions;
	std::set<std::string> _loadedP3Ds;
	std::map<std::string, Vector3> _locators;
};

} // namespace Donut
