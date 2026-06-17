// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Render/imgui/imgui.h"
#include <Core/File.h>
#include <Core/Log.h>
#include <Entity.h>
#include <Game.h>
#include <Level.h>
#include <P3D/P3D.generated.h>
#include <P3D/P3DFile.h>
#include <Physics/WorldPhysics.h>
#include <Render/BillboardBatch.h>
#include <Render/Font.h>
#include <Render/LineRenderer.h>
#include <Render/Mesh.h>
#include <Render/OpenGL/ShaderProgram.h>
#include <Render/Shader.h>
#include <Render/WorldSphere.h>
#include <ResourceManager.h>
#include <array>

namespace Donut
{

Level::Level()
{
	const auto worldVertSrc = File::ReadAll("shaders/world.vert");
	const auto worldFragSrc = File::ReadAll("shaders/world.frag");
	const auto worldInstancedVertSrc = File::ReadAll("shaders/world_instanced.vert");
	const auto billboardBatchVertSrc = File::ReadAll("shaders/billboard_batch.vert");

	_worldShader = std::make_unique<GL::ShaderProgram>(worldVertSrc, worldFragSrc);
	_worldInstancedShader = std::make_unique<GL::ShaderProgram>(worldInstancedVertSrc, worldFragSrc);
	_billboardBatchShader = std::make_unique<GL::ShaderProgram>(billboardBatchVertSrc, worldFragSrc);

	// todo: move this into Game.cpp or something else ?
}

Level::~Level() = default;

void Level::LoadP3D(const std::string& filename)
{
	if (_loadedP3Ds.find(filename) != _loadedP3Ds.end())
		return;
	_loadedP3Ds.insert(filename);

	std::string fullpath = "./art/" + filename;

	if (!FileSystem::exists(fullpath))
	{
		Log::Warn("Level not found: {}", filename);
		return;
	}

	Log::Info("Loading level: {}", filename);

	const auto p3d = P3D::P3DFile(fullpath);

	auto& rm = Game::GetInstance().GetResourceManager();
	const auto& root = p3d.GetRoot();

	for (const auto& chunk : root.GetChildren())
	{
		switch (chunk->GetType())
		{
		case P3D::ChunkType::Shader: rm.LoadShader(*P3D::Shader::Load(*chunk)); break;
		case P3D::ChunkType::Texture: rm.LoadTexture(*P3D::Texture::Load(*chunk)); break;
		case P3D::ChunkType::Set: rm.LoadSet(*P3D::Set::Load(*chunk)); break;
		case P3D::ChunkType::Geometry: rm.LoadGeometry(*P3D::Geometry::Load(*chunk)); break;
		case P3D::ChunkType::StaticEntity:
			_entities.emplace_back(std::make_unique<StaticEntity>(*P3D::StaticEntity::Load(*chunk)));
			break;
		case P3D::ChunkType::StaticPhysics:
		{
			const auto& ent = P3D::StaticPhysics::Load(*chunk);
			if (auto const& volume = ent->GetCollisionObject()->GetVolume())
				Game::GetInstance().GetWorldPhysics().AddCollisionVolume(*volume);
			break;
		}
		case P3D::ChunkType::InstancedStaticPhysics:
		{
			const auto& staticPhys = P3D::InstancedStaticPhysics::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<Matrix4x4> transforms;
			P3D::P3DUtil::GetDrawables(staticPhys->GetInstanceList(), drawables, transforms);

			const auto& geometries = staticPhys->GetGeometries();
			std::map<std::string, size_t> meshesNameIndex;
			for (const auto& geometry : geometries) { meshesNameIndex.insert({geometry->GetName(), meshesNameIndex.size()}); }

			std::unordered_map<std::string, std::vector<Matrix4x4>> meshTransforms;

			for (size_t i = 0; i < drawables.size(); ++i)
			{
				const auto& drawable = drawables.at(i);
				const auto& transform = transforms.at(i);
				const auto& meshName = drawable->GetName();

				auto& transforms = meshTransforms[meshName];
				transforms.push_back(transform);
			}

			for (const auto& meshTransformsPair : meshTransforms)
			{
				const auto& geometry = geometries.at(meshesNameIndex.at(meshTransformsPair.first));
				_instances.emplace_back(std::make_unique<InstancedStaticEntity>(*geometry, meshTransformsPair.second));
			}

			break;
		}
		case P3D::ChunkType::DynamicPhysics:
		{
			const auto& dynaPhys = P3D::DynamicPhysics::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<Matrix4x4> transforms;
			P3D::P3DUtil::GetDrawables(dynaPhys->GetInstanceList(), drawables, transforms);

			const auto& geometries = dynaPhys->GetGeometries();
			std::map<std::string, size_t> meshesNameIndex;
			for (const auto& geometry : geometries) { meshesNameIndex.insert({geometry->GetName(), meshesNameIndex.size()}); }

			std::unordered_map<std::string, std::vector<Matrix4x4>> meshTransforms;

			for (size_t i = 0; i < drawables.size(); ++i)
			{
				const auto& drawable = drawables.at(i);
				const auto& transform = transforms.at(i);
				const auto& meshName = drawable->GetName();

				auto& transforms = meshTransforms[meshName];
				transforms.push_back(transform);
			}

			for (const auto& meshTransformsPair : meshTransforms)
			{
				const auto& mesh = geometries.at(meshesNameIndex.at(meshTransformsPair.first));
				_instances.emplace_back(std::make_unique<InstancedStaticEntity>(*mesh, meshTransformsPair.second));
			}

			break;
		}
		case P3D::ChunkType::AnimDynamicPhysics:
		{
			const auto& dynaPhys = P3D::AnimDynamicPhysics::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<Matrix4x4> transforms;
			P3D::P3DUtil::GetDrawables(dynaPhys->GetInstanceList(), drawables, transforms);

			const auto& animObjectWrapper = dynaPhys->GetAnimObjectWrapper();

			for (size_t i = 0; i < drawables.size(); ++i)
			{
				const auto& drawable = drawables.at(i);
				const auto& transform = transforms.at(i);

				auto compositeModel = std::make_unique<CompositeModel>(CompositeModel_AnimObjectWrapper(*animObjectWrapper));
				compositeModel->SetTransform(transform);
				_compositeModels.push_back(std::move(compositeModel));
			}

			break;
		}
		case P3D::ChunkType::Intersect:
		{
			auto intersect = P3D::Intersect::Load(*chunk);
			Game::GetInstance().GetWorldPhysics().AddIntersect(*intersect);
			break;
		}
		case P3D::ChunkType::WorldSphere: _worldSphere = std::make_unique<WorldSphere>(*P3D::WorldSphere::Load(*chunk)); break;
		case P3D::ChunkType::Locator2:
		{
			auto loc = P3D::Locator2::Load(*chunk);
			const auto& triggers = loc->GetTriggers();
			if (!triggers.empty())
			{
				Matrix4x4 t = triggers[0]->GetTransform();
				Vector3 pos(t.M[3][0], t.M[3][1], t.M[3][2]);
				std::string name = loc->GetName();
				_locators.insert({name, pos});
			}
			break;
		}
		case P3D::ChunkType::FenceWrapper:
		{
			auto const& fence = P3D::FenceWrapper::Load(*chunk);
			Game::GetInstance().GetWorldPhysics().AddP3DFence(*fence->GetFence());
			break;
		}
		case P3D::ChunkType::BillboardQuadGroup:
		{
			_billboardBatches.push_back(std::make_unique<BillboardBatch>(*P3D::BillboardQuadGroup::Load(*chunk)));

			break;
		}
		case P3D::ChunkType::Path:
		{
			auto path = P3D::Path::Load(*chunk);
			_paths.push_back(Path {path->GetPoints()});

			break;
		}
		default: break;
		}
	}
}

void Level::DynaLoadData(const std::string& dynaLoadData)
{
	std::vector<std::string> regionsLoad, regionsUnload;

	std::size_t prev = 0, pos;
	while ((pos = dynaLoadData.find_first_of(";:@$", prev)) != std::string::npos)
	{
		const std::string file = dynaLoadData.substr(prev, pos - prev);
		switch (dynaLoadData.at(pos))
		{
		case ';': regionsLoad.push_back(file); break;
		case ':': regionsUnload.push_back(file); break;
		case '@': regionsLoad.push_back(file); Log::Info("Level: load interior {}", file); break;
		case '$': regionsUnload.push_back(file); Log::Info("Level: unload interior {}", file); break;
		}

		prev = pos + 1;
	}

	for (auto const& region : regionsUnload)
		unloadRegion(region);

	for (auto const& region : regionsLoad)
		loadRegion(region);
}

void Level::ImGuiDebugWindow(bool* p_open) const
{
	ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Level", p_open))
	{
		ImGui::End();
		return;
	}

	for (const auto& ent : _entities)
	{
		ImGui::TextDisabled("%s", ent->GetClassName().c_str());
		ImGui::SameLine();
		ImGui::TextUnformatted(ent->GetName().c_str());
	}

	ImGui::End();
}

Vector3 Level::GetLocatorPosition(const std::string& name) const
{
	auto it = _locators.find(name);
	if (it != _locators.end())
		return it->second;
	return Vector3::Zero;
}

void Level::loadRegion(const std::string& filename)
{
	if (_loadedRegions.find(filename) != _loadedRegions.end())
		return;

	Log::Info("load region: {}", filename);
	_loadedRegions.insert(filename);
	LoadP3D(filename);
}

void Level::unloadRegion(const std::string& filename)
{
	Log::Info("unload region: {}", filename);
	_loadedRegions.erase(filename);
	_loadedP3Ds.erase(filename);
}

void Level::Update(double deltatime)
{
	// draws debug shit
	_worldSphere->Update(deltatime);
}

void Level::Draw(Matrix4x4& viewProj, const Vector3& cameraPos)
{
	_worldShader->Bind();
	_worldShader->SetUniformValue("viewProj", viewProj);
	_worldShader->SetUniformValue("fogDensity", 0.0004f);
	_worldShader->SetUniformValue("fogColor", Vector4(0.62f, 0.78f, 1.0f, 1.0f));
	_worldShader->SetUniformValue("cameraPos", cameraPos);

	glDisable(GL_DEPTH_TEST);

	if (_worldSphere != nullptr)
	{
		glDisable(GL_BLEND);
		_worldSphere->Draw(*_worldShader, viewProj, true);
		glEnable(GL_BLEND);
		_worldSphere->Draw(*_worldShader, viewProj, false);
	}

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	for (const auto& ent : _entities) ent->Draw(*_worldShader, true);

	for (const auto& compositeModel : _compositeModels)
		compositeModel->Draw(*_worldShader, viewProj, compositeModel->GetTransform(), true);

	_billboardBatchShader->Bind();
	_billboardBatchShader->SetUniformValue("viewProj", viewProj);
	for (const auto& billboardBatch : _billboardBatches) billboardBatch->Draw(*_billboardBatchShader, true);

	_worldInstancedShader->Bind();
	_worldInstancedShader->SetUniformValue("viewProj", viewProj);

	for (const auto& ent : _instances) ent->Draw(*_worldInstancedShader, true);

	glEnable(GL_BLEND);

	_worldShader->Bind();
	_worldShader->SetUniformValue("viewProj", viewProj);

	for (const auto& ent : _entities) ent->Draw(*_worldShader, false);

	for (const auto& compositeModel : _compositeModels)
		compositeModel->Draw(*_worldShader, viewProj, compositeModel->GetTransform(), false);

	_billboardBatchShader->Bind();
	_billboardBatchShader->SetUniformValue("viewProj", viewProj);
	for (const auto& billboardBatch : _billboardBatches) billboardBatch->Draw(*_billboardBatchShader, false);

	_worldInstancedShader->Bind();
	_worldInstancedShader->SetUniformValue("viewProj", viewProj);

	for (const auto& ent : _instances) ent->Draw(*_worldInstancedShader, false);
}

} // namespace Donut
