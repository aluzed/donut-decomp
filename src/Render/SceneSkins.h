#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Render/TextureAnimation.h"

#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class Skeleton;
class SkinAnimation;
class SkinModel;
namespace GL
{
class ShaderProgram;
class TextureBuffer;
} // namespace GL

// The skinned characters of an animated scene: a gag's Homer on the couch,
// bound to the scene's skeleton and moved by its pose animation, as the props
// of the same file are by CompositeModel.
class SceneSkins
{
public:
	~SceneSkins();

	// Null when the file holds no skin.
	static std::unique_ptr<SceneSkins> Load(const std::string& file);

	// The pose at `frame` of the scene's animation; with `loop`, a frame past
	// the end wraps round, otherwise it holds the last.
	void Pose(float frame, bool loop);
	void Draw(const Matrix4x4& viewProjection, const Matrix4x4& model, GL::ShaderProgram& skinShader);

private:
	SceneSkins();

	std::vector<std::unique_ptr<SkinModel>> _skins;
	std::unique_ptr<Skeleton> _skeleton;
	std::unique_ptr<SkinAnimation> _animation;
	std::unique_ptr<GL::TextureBuffer> _boneBuffer;
	// The characters' eyes, keyed to the scene (TEX_eyeball_m), and the frame
	// last posed.
	std::optional<TextureAnimation> _eyes;
	float _frame = 0.0f;
};

} // namespace Donut
