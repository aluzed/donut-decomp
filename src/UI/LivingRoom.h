#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"
#include "Render/TextureAnimation.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class CompositeModel;
class Mesh;
class SkinAnimation;
namespace P3D
{
class SceneGraph;
}
namespace GL
{
class ShaderProgram;
}

// The Simpsons' living room the game's main menu stands over (scrooby2's
// 3dFE.pag: resource/pure3d/camset.p3d): its scene graph's meshes where it
// puts them, seen through its own camera, "FE_Camera".
class LivingRoom
{
public:
	// Empty when the file is not there.
	explicit LivingRoom(const std::string& file);
	~LivingRoom();

	bool IsLoaded() const { return !_placed.empty(); }
	// Clears the screen and draws the room with the world program, for a
	// viewport of that aspect.
	void Draw(GL::ShaderProgram& worldShader, float aspect, double dt);
	// The camera's flight (camset.p3d's FE_CameraController, 1036 frames at
	// 30 a second): where it is, and where it is going, played forwards or
	// backwards at that rate until it is there.
	void JumpTo(float frame) { _cameraFrame = _cameraTarget = frame; }
	// Plays the flight from one frame to another, at its rate.
	void Play(float from, float to)
	{
		_cameraFrame = from;
		_cameraTarget = to;
	}
	float GetCameraFrame() const { return _cameraFrame; }

private:
	std::map<std::string, std::unique_ptr<Mesh>> _meshes;
	struct Placed
	{
		Mesh* mesh;
		Matrix4x4 transform;
	};
	std::vector<Placed> _placed;
	// A scene graph whose transforms an animation moves (racecar.p3d: the
	// race car round its track on the rug): each node, its parent's index
	// before it, where it stands at rest, and the meshes it carries.
	struct Node
	{
		std::string name;
		int parent;
		Matrix4x4 local;
		std::vector<Mesh*> meshes;
	};
	struct AnimatedScene
	{
		std::vector<Node> nodes;
		std::unique_ptr<SkinAnimation> animation;
	};
	std::vector<AnimatedScene> _scenes;
	// The textures, shaders and meshes of `file` (its camera, FE_Camera, the
	// room's) and its scene graph; with `scene`, the scene graph's nodes and
	// the animation that moves them as well. Its meshes go by `prefix` and
	// their names: Maya's names ("f1_polySurfaceShape1") are in more than one
	// file.
	std::unique_ptr<P3D::SceneGraph> loadFile(const std::string& file, const std::string& prefix,
	                                          AnimatedScene* scene);
	// The models beside the room (the television's screen), each where its
	// own skeleton puts it.
	std::vector<std::unique_ptr<CompositeModel>> _models;
	// Their texture animations (the television's static), round and round,
	// and the texture each shows now.
	std::vector<std::pair<TextureAnimation, std::string>> _textureAnimations;
	double _time = 0.0;
	// A key-framed position: the camera's (STRN_CAM), what it looks at
	// (STRN_CAM_INTEREST1).
	struct Path
	{
		std::vector<float> frames;
		std::vector<Vector3> values;
		Vector3 At(float frame) const;
	};
	Path _eyePath, _lookPath;
	float _cameraFrame = 0.0f, _cameraTarget = 0.0f;
	Vector3 _eye, _forward = Vector3(0.0f, 0.0f, 1.0f), _up = Vector3(0.0f, 1.0f, 0.0f);
	float _fov = 1.0f, _near = 0.1f, _far = 1000.0f;
};

} // namespace Donut
