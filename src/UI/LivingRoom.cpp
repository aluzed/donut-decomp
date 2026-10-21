#include "LivingRoom.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Game.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Render/BillboardBatch.h"
#include "Render/CompositeModel.h"
#include "Render/Mesh.h"
#include "Render/ParticleSystem.h"
#include "Render/SceneSkins.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/SkinAnimation.h"
#include "Core/MemoryStream.h"
#include "ResourceManager.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

namespace Donut
{

std::unique_ptr<P3D::SceneGraph> LivingRoom::loadFile(const std::string& file, const std::string& prefix,
                                                      AnimatedScene* scene)
{
	if (!FileSystem::exists(file))
		return nullptr;
	const P3D::P3DFile p3d(file);
	auto& resources = Game::GetInstance().GetResourceManager();
	std::unique_ptr<P3D::SceneGraph> sceneGraph;
	std::vector<std::unique_ptr<P3D::Animation>> animations;
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
			_meshes[prefix + geometry->GetName()] = std::move(mesh);
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
		{
			sceneGraph = P3D::SceneGraph::Load(*chunk);
			if (scene)
			{
				// The transforms, read from the chunks: the generated reader
				// keeps a transform's transforms only, and the race car's
				// locator1 hangs under a visibility node.
				const std::function<void(const P3D::P3DChunk&, int)> add = [&](const P3D::P3DChunk& parent, int index) {
					for (const auto& child : parent.GetChildren())
					{
						if (child->IsType(P3D::ChunkType::SceneGraphTransform))
						{
							const auto transform = P3D::SceneGraphTransform::Load(*child);
							Node node {transform->GetName(), index, transform->GetTransform(), {}};
							for (const auto& drawable : transform->GetDrawables())
								if (const auto mesh = _meshes.find(prefix + drawable->GetDrawableName()); mesh != _meshes.end())
									node.meshes.push_back(mesh->second.get());
							scene->nodes.push_back(std::move(node));
							add(*child, static_cast<int>(scene->nodes.size()) - 1);
						}
						else
							add(*child, index);
					}
				};
				add(*chunk, -1);
			}
		}
		else if (scene && chunk->IsType(P3D::ChunkType::Animation))
			animations.push_back(P3D::Animation::Load(*chunk));
	}
	// The first animation that moves a transform of the scene graph.
	if (scene && !scene->nodes.empty())
	{
		std::vector<std::string> names;
		std::vector<Matrix4x4> rests;
		for (const Node& node : scene->nodes)
		{
			names.push_back(node.name);
			rests.push_back(node.local);
		}
		for (const auto& animation : animations)
			if (animation->GetGroupList())
			{
				scene->animation = SkinAnimation::FromP3D(*animation, names, rests);
				break;
			}
	}
	return sceneGraph;
}

LivingRoom::LivingRoom(const std::string& file)
{
	// Where its scene graph puts each mesh.
	const auto graph = loadFile(file, "", nullptr);
	if (!graph || !graph->GetRoot() || !graph->GetRoot()->GetBranch())
		return;
	std::vector<P3D::SceneGraphDrawable*> drawables;
	std::vector<Matrix4x4> transforms;
	for (const auto& child : graph->GetRoot()->GetBranch()->GetChildren())
		P3D::P3DUtil::GetDrawables(child, drawables, transforms, Matrix4x4::Identity);
	for (std::size_t i = 0; i < drawables.size(); ++i)
		if (const auto mesh = _meshes.find(drawables[i]->GetDrawableName()); mesh != _meshes.end())
			_placed.push_back({mesh->second.get(), transforms[i]});
	const std::string directory = file.substr(0, file.find_last_of('/') + 1);

	// The camera's flight, from the room's file: its two paths.
	{
		const P3D::P3DFile p3d(file);
		for (const auto& chunk : p3d.GetRoot().GetChildren())
		{
			if (!chunk->IsType(P3D::ChunkType::Animation))
				continue;
			const auto animation = P3D::Animation::Load(*chunk);
			if (animation->GetName() == "CAM_FE_Camera")
			{
				// Its float channels, which the generated reader does not keep:
				// version, name, key count, the frames, the values.
				std::map<std::string, std::vector<std::pair<float, float>>> curves;
				const std::function<void(const P3D::P3DChunk&)> read = [&](const P3D::P3DChunk& part) {
					if (part.IsType(P3D::ChunkType::Float1Channel))
					{
						MemoryStream stream(part.GetData());
						stream.Read<uint32_t>();
						std::string param = stream.ReadString(4);
						param = param.substr(0, param.find('\0'));
						std::vector<uint16_t> frames(stream.Read<uint32_t>());
						for (auto& frame : frames)
							frame = stream.Read<uint16_t>();
						for (const uint16_t frame : frames)
							curves[param].emplace_back(static_cast<float>(frame), stream.Read<float>());
						return;
					}
					for (const auto& child : part.GetChildren())
						read(*child);
				};
				read(*chunk);
				const auto& nears = curves["NCLP"];
				const auto& fars = curves["FCLP"];
				const auto at = [](const std::vector<std::pair<float, float>>& keys, float frame, float otherwise) {
					if (keys.empty())
						return otherwise;
					const auto next = std::upper_bound(keys.begin(), keys.end(), std::make_pair(frame, 1e30f));
					return next == keys.begin() ? keys.front().second : std::prev(next)->second;
				};
				// Both on the frames either is keyed at, each held to its next key.
				std::set<float> frames;
				for (const auto& keys : {nears, fars})
					for (const auto& key : keys)
						frames.insert(key.first);
				for (const float frame : frames)
				{
					_clipPath.frames.push_back(frame);
					_clipPath.values.emplace_back(at(nears, frame, _near), at(fars, frame, _far), 0.0f);
				}
			}
			Path* path = animation->GetName() == "STRN_CAM"             ? &_eyePath
			             : animation->GetName() == "STRN_CAM_INTEREST1" ? &_lookPath
			                                                            : nullptr;
			if (!path || !animation->GetGroupList() || animation->GetGroupList()->GetGroups().empty())
				continue;
			if (const auto* channel = animation->GetGroupList()->GetGroups()[0]->GetVector3ChannelsValue("TRAN"))
				for (std::size_t i = 0; i < channel->GetNumFrames(); ++i)
				{
					path->frames.push_back(static_cast<float>(channel->GetFrames()[i]));
					path->values.push_back(channel->GetValues()[i]);
				}
		}
		// Over the main menu: the room from the door, as the game opens on it.
		JumpTo(290.0f);
	}

	// The television's screen, beside the room.
	if (auto loaded = CompositeModel::LoadP3D(directory + "TVscreen.p3d"))
	{
		const P3D::P3DFile animations(directory + "TVscreen.p3d");
		for (const auto& chunk : animations.GetRoot().GetChildren())
			if (chunk->IsType(P3D::ChunkType::Animation))
				if (auto animation = TextureAnimation::FromChunk(*chunk))
					_textureAnimations.emplace_back(std::move(*animation), std::string());
		if (loaded->IsAnimated())
			loaded->Pose(0.0f, false);
		_models.push_back(std::move(loaded));
	}

	// Homer, his shaders and textures first: the skins' loader leaves them to
	// whatever else the file holds.
	loadFile(directory + "homer.p3d", "homer/", nullptr);
	_homer = SceneSkins::Load(directory + "homer.p3d");

	// The glows, each about the object of a main menu's entry.
	for (const char* name : {"tv", "homr", "sofa", "vcr", "mini", "book"})
		if (Glow glow = loadGlow(directory + "glow" + name + ".p3d", {}); glow.batch)
			_glows[name] = std::move(glow);
	// The scrapbook's border, and the sparkles round it.
	_border = loadGlow(directory + "sparkles.p3d", "borderShape", &_sparklesTransform, "sparklesShape");
	if (FileSystem::exists(directory + "sparkles.p3d"))
	{
		const P3D::P3DFile p3d(directory + "sparkles.p3d");
		for (const auto& chunk : p3d.GetRoot().GetChildren())
			if (chunk->IsType(P3D::ChunkType::ParticleSystemFactory))
				if (auto factory = ParticleFactory::FromChunk(*chunk); !factory->GetEmitters().empty())
				{
					_sparkleFactory = std::move(factory);
					_sparkles = std::make_unique<ParticleSystem>(*_sparkleFactory);
				}
	}

	// The race car on the rug: not a composite but a scene graph, its
	// locator1 driven round the track by the file's animation (FE_racing,
	// 132 frames at 30 a second).
	AnimatedScene race;
	loadFile(directory + "racecar.p3d", "racecar/", &race);
	if (!race.nodes.empty())
		_scenes.push_back(std::move(race));
	Log::Info("LivingRoom: {} meshes placed of {}, {} models, {} animated scenes, camera at {} fov {:.2f}",
	          _placed.size(), _meshes.size(), _models.size(), _scenes.size(), _eye.ToString(), _fov);
}

LivingRoom::~LivingRoom() = default;

LivingRoom::Glow LivingRoom::loadGlow(const std::string& file, const std::string& drawable, Matrix4x4* other,
                                      const std::string& otherDrawable)
{
	Glow glow;
	if (!FileSystem::exists(file))
		return glow;
	loadFile(file, file + "/", nullptr);
	const P3D::P3DFile p3d(file);
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		if (chunk->IsType(P3D::ChunkType::BillboardQuadGroup))
			glow.batch = std::make_unique<BillboardBatch>(*P3D::BillboardQuadGroup::Load(*chunk));
		else if (chunk->IsType(P3D::ChunkType::Animation))
		{
			if (auto animation = BillboardAnimation::FromChunk(*chunk))
				glow.animation = std::move(animation);
		}
		else if (chunk->IsType(P3D::ChunkType::SceneGraph))
		{
			// Where the scene graph hangs the quad group (and the other).
			const auto graph = P3D::SceneGraph::Load(*chunk);
			std::vector<P3D::SceneGraphDrawable*> drawables;
			std::vector<Matrix4x4> transforms;
			if (graph->GetRoot() && graph->GetRoot()->GetBranch())
				for (const auto& child : graph->GetRoot()->GetBranch()->GetChildren())
					P3D::P3DUtil::GetDrawables(child, drawables, transforms, Matrix4x4::Identity);
			for (std::size_t i = 0; i < drawables.size(); ++i)
			{
				if (drawable.empty() ? i == 0 : drawables[i]->GetDrawableName() == drawable)
					glow.transform = transforms[i];
				if (other && drawables[i]->GetDrawableName() == otherDrawable)
					*other = transforms[i];
			}
		}
	}
	return glow;
}

void LivingRoom::drawGlow(Glow& glow, GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj)
{
	if (glow.animation)
	{
		const float frame =
		    std::fmod(static_cast<float>(_time) * glow.animation->frameRate, std::max(1.0f, glow.animation->frames));
		for (const auto& quad : glow.animation->quads)
			if (const int index = glow.batch->FindQuad(quad.name); index >= 0)
			{
				glow.batch->SetQuadSize(index, quad.SizeAt(frame, glow.batch->GetQuadSize(index)));
				if (const auto color = quad.ColorAt(frame))
					glow.batch->SetQuadColor(index, *color);
			}
	}
	billboardShader.SetUniformValue("viewProj", viewProj * glow.transform);
	glow.batch->Draw(billboardShader, true);
	glow.batch->Draw(billboardShader, false);
}

Vector3 LivingRoom::Path::At(float frame) const
{
	if (frames.empty())
		return Vector3::Zero;
	const auto next = std::upper_bound(frames.begin(), frames.end(), frame);
	if (next == frames.begin())
		return values.front();
	if (next == frames.end())
		return values.back();
	const std::size_t i = static_cast<std::size_t>(next - frames.begin());
	const float t = (frame - frames[i - 1]) / std::max(1e-6f, frames[i] - frames[i - 1]);
	return values[i - 1] + (values[i] - values[i - 1]) * t;
}

void LivingRoom::Draw(GL::ShaderProgram& worldShader, GL::ShaderProgram& skinShader, GL::ShaderProgram& billboardShader,
                      float aspect, double dt)
{
	_time += dt;
	// The camera along its flight, towards the frame it is sent to.
	if (!_eyePath.frames.empty() && !_lookPath.frames.empty())
	{
		const float step = static_cast<float>(dt) * 30.0f;
		if (_cameraFrame < _cameraTarget)
			_cameraFrame = std::min(_cameraTarget, _cameraFrame + step);
		else
			_cameraFrame = std::max(_cameraTarget, _cameraFrame - step);
		_eye = _eyePath.At(_cameraFrame);
		if (!_clipPath.frames.empty())
		{
			const Vector3 clip = _clipPath.At(_cameraFrame);
			_near = std::max(0.005f, clip.X);
			_far = std::max(_near + 1.0f, clip.Y);
		}
		if (const Vector3 look = _lookPath.At(_cameraFrame) - _eye; look.LengthSquared() > 1e-6f)
			_forward = look.Normalized();
	}
	auto& resources = Game::GetInstance().GetResourceManager();
	for (auto& [animation, showing] : _textureAnimations)
	{
		if (animation.keys.empty())
			continue;
		const float frames = std::max(animation.frameCount, animation.keys.back().first + 1.0f);
		const std::string& texture = animation.TextureAt(std::fmod(static_cast<float>(_time) * animation.frameRate, frames));
		if (texture != showing && resources.ShowShaderTexture(animation.shader, texture))
			showing = texture;
	}
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
		for (const auto& scene : _scenes)
		{
			// Each node where its parent and the animation put it.
			std::vector<Matrix4x4> world(scene.nodes.size());
			const float frame = static_cast<float>(_time) * (scene.animation ? scene.animation->GetFrameRate() : 0.0f);
			for (std::size_t i = 0; i < scene.nodes.size(); ++i)
			{
				const Node& node = scene.nodes[i];
				const Matrix4x4 local = scene.animation ? scene.animation->Evaluate(i, frame, true) : node.local;
				world[i] = node.parent >= 0 ? world[node.parent] * local : local;
				for (Mesh* mesh : node.meshes)
				{
					worldShader.SetUniformValue("viewProj", viewProj * world[i]);
					mesh->Draw(worldShader, opaque);
				}
			}
		}
	}
	if (_homer)
	{
		glDisable(GL_BLEND);
		_homer->Pose(static_cast<float>(_time) * 30.0f, true);
		_homer->Draw(viewProj, Matrix4x4::Identity, skinShader);
		glEnable(GL_BLEND);
	}
	// The glow, over what it is about, pulsing; the scrapbook's border and
	// its sparkles.
	{
		billboardShader.Bind();
		// Its own uniforms, which the level sets only when it draws: no
		// lightmap (at 1, the quad came out black, nothing added).
		billboardShader.SetUniformValue("fogDensity", 0.0f);
		billboardShader.SetUniformValue("lightmapIntensity", 0.0f);
		glEnable(GL_BLEND);
		// Facing the camera whichever way its corners turn, and over what it
		// is about: the television's outline is the television's, Homer
		// before it.
		glDisable(GL_CULL_FACE);
		GLint depthFunc = GL_LESS;
		glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
		glDepthFunc(GL_ALWAYS);
		if (const auto glow = _glows.find(_glow); glow != _glows.end())
			drawGlow(glow->second, billboardShader, viewProj);
		if (_borderShown && _border.batch)
			drawGlow(_border, billboardShader, viewProj);
		if (_sparkles)
		{
			// Running out its sparks when the book closes.
			_sparkles->Update(dt, _sparklesTransform, -1.0f, _borderShown);
			billboardShader.SetUniformValue("viewProj", viewProj);
			_sparkles->Draw(billboardShader);
		}
		glDepthFunc(depthFunc);
	}
	glDisable(GL_DEPTH_TEST);
	if (culling)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

} // namespace Donut
