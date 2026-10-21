#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

#include <memory>
#include <vector>

namespace Donut
{

class Texture;
namespace GL
{
class IndexBuffer;
class ShaderProgram;
class VertexBinding;
class VertexBuffer;
} // namespace GL

// The dark patch on the ground under each character and car, as the game draws
// them: a soft disc under a character, a soft-edged rectangle under a car (its
// corners moved by its SetShadowAdjustments). Laid every frame, drawn at once.
class BlobShadows
{
public:
	BlobShadows();
	~BlobShadows();

	bool enabled = true;

	void Clear();
	// On the ground at `ground`, `radius` across, `darkness` 0 to 1.
	void AddDisc(const Vector3& ground, float radius, float darkness);
	// On the ground at `ground`, turned `yaw` about the vertical, its half
	// width across and half length along.
	void AddBox(const Vector3& ground, float yaw, float halfWidth, float halfLength, float darkness);
	// With the world program (texture unit 0); its viewProj set here.
	void Draw(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj);

	struct Vertex
	{
		Vector3 pos;
		Vector2 uv;
		Vector2 lightmapUV;
		Vector4 color;
	};

private:
	static constexpr std::size_t kQuads = 512;
	void addQuad(std::vector<Vertex>& quads, const Vector3 corners[4], float darkness);
	std::vector<Vertex> _discs, _boxes;
	std::unique_ptr<Texture> _discTexture, _boxTexture;
	std::shared_ptr<GL::VertexBuffer> _buffer;
	std::shared_ptr<GL::IndexBuffer> _indices;
	std::shared_ptr<GL::VertexBinding> _binding;
};

} // namespace Donut
