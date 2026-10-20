#include "LivingRoom.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Game.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Render/CompositeModel.h"
#include "Render/Mesh.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "ResourceManager.h"

#include <cmath>

namespace Donut
{

LivingRoom::LivingRoom(const std::string& file)
{
	if (!FileSystem::exists(file))
		return;
	const P3D::P3DFile p3d(file);
	auto& resources = Game::GetInstance().GetResourceManager();
	const P3D::P3DChunk* sceneGraph = nullptr;
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		if (chunk->IsType(P3D::ChunkType::Texture))
			resources.LoadTexture(*P3D::Texture::Load(*chunk));
		else if (chunk->IsType(P3D::ChunkType::Shader))
		{
			if (auto shader = P3D::Shader::Load(*chunk); !resources.HasShader(shader->GetName()))
				resources.LoadShader(*shader);
		}
		else if (chunk->IsType(P3D::ChunkType::Geometry))
		{
			const auto geometry = P3D::Geometry::Load(*chunk);
			auto mesh = std::make_unique<Mesh>(*geometry);
			mesh->Commit();
			_meshes[geometry->GetName()] = std::move(mesh);
		}
		else if (chunk->IsType(P3D::ChunkType::Camera))
		{
			const auto camera = P3D::Camera::Load(*chunk);
			if (camera->GetName() == "FE_Camera")
			{
				_eye = camera->GetPosition();
				_forward = camera->GetForward().Normalized();
				_up = camera->GetUp().Normalized();
				_fov = camera->GetFov();
				_near = std::max(0.01f, camera->GetNearClip());
				_far = std::max(_near + 1.0f, camera->GetFarClip());
			}
		}
		else if (chunk->IsType(P3D::ChunkType::SceneGraph))
			sceneGraph = chunk.get();
	}
	if (!sceneGraph)
		return;
	// Where its scene graph puts each mesh.
	const auto graph = P3D::SceneGraph::Load(*sceneGraph);
	if (!graph->GetRoot() || !graph->GetRoot()->GetBranch())
		return;
	std::vector<P3D::SceneGraphDrawable*> drawables;
	std::vector<Matrix4x4> transforms;
	for (const auto& child : graph->GetRoot()->GetBranch()->GetChildren())
		P3D::P3DUtil::GetDrawables(child, drawables, transforms, Matrix4x4::Identity);
	for (std::size_t i = 0; i < drawables.size(); ++i)
		if (const auto mesh = _meshes.find(drawables[i]->GetDrawableName()); mesh != _meshes.end())
			_placed.push_back({mesh->second.get(), transforms[i]});
	// The television's screen and the race car, beside it.
	const std::string directory = file.substr(0, file.find_last_of('/') + 1);
	for (const char* model : {"TVscreen.p3d", "racecar.p3d"})
		if (auto loaded = CompositeModel::LoadP3D(directory + model))
		{
			if (loaded->IsAnimated())
				loaded->Pose(0.0f, false);
			_models.push_back(std::move(loaded));
		}
	Log::Info("LivingRoom: {} meshes placed of {}, {} models, camera at {} fov {:.2f}", _placed.size(), _meshes.size(),
	          _models.size(), _eye.ToString(), _fov);
}

LivingRoom::~LivingRoom() = default;

void LivingRoom::Draw(GL::ShaderProgram& worldShader, float aspect)
{
	// The camera's view: its right, up and forward as rows (left-handed, z
	// ahead), then where it stands.
	Vector3 forward = _forward;
	Vector3 right = _up.Cross(forward).Normalized();
	Vector3 up = forward.Cross(right);
	const Matrix4x4 view(right.X, up.X, forward.X, 0.0f, right.Y, up.Y, forward.Y, 0.0f, right.Z, up.Z, forward.Z, 0.0f,
	                     -right.Dot(_eye), -up.Dot(_eye), -forward.Dot(_eye), 1.0f);
	const float tanHalf = std::tan(_fov * 0.5f);
	Matrix4x4 projection = Matrix4x4::Zero;
	projection[0][0] = 1.0f / (aspect * tanHalf);
	projection[1][1] = 1.0f / tanHalf;
	projection[2][2] = (_far + _near) / (_far - _near);
	projection[2][3] = 1.0f;
	projection[3][2] = -(2.0f * _far * _near) / (_far - _near);
	const Matrix4x4 viewProj = projection * view;

	// A two-sided mesh's draw turns face culling back on after it: the 2D
	// drawn next, its quads wound the other way, went with it.
	const GLboolean culling = glIsEnabled(GL_CULL_FACE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	worldShader.Bind();
	worldShader.SetUniformValue("fogDensity", 0.0f);
	worldShader.SetUniformValue("lightmapIntensity", 0.0f);
	for (const bool opaque : {true, false})
	{
		if (opaque)
			glDisable(GL_BLEND);
		else
			glEnable(GL_BLEND);
		for (const auto& placed : _placed)
		{
			worldShader.SetUniformValue("viewProj", viewProj * placed.transform);
			placed.mesh->Draw(worldShader, opaque);
		}
		for (const auto& model : _models)
			model->Draw(worldShader, viewProj, model->GetTransform(), opaque);
	}
	glDisable(GL_DEPTH_TEST);
	if (culling)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// The level's fog, put back for the world drawn next frame.
	worldShader.SetUniformValue("fogDensity", 0.0004f);
}

} // namespace Donut
