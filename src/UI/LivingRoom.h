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

private:
	std::map<std::string, std::unique_ptr<Mesh>> _meshes;
	struct Placed
	{
		Mesh* mesh;
		Matrix4x4 transform;
	};
	std::vector<Placed> _placed;
	// The models beside the room (the television's screen, the race car on
	// the rug), each where its own skeleton puts it.
	std::vector<std::unique_ptr<CompositeModel>> _models;
	// Their texture animations (the television's static), round and round,
	// and the texture each shows now.
	std::vector<std::pair<TextureAnimation, std::string>> _textureAnimations;
	double _time = 0.0;
	Vector3 _eye, _forward = Vector3(0.0f, 0.0f, 1.0f), _up = Vector3(0.0f, 1.0f, 0.0f);
	float _fov = 1.0f, _near = 0.1f, _far = 1000.0f;
};

} // namespace Donut
