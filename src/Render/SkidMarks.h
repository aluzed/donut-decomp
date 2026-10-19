#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

#include <cstdint>
#include <climits>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Donut
{

namespace GL
{
class IndexBuffer;
class ShaderProgram;
class VertexBinding;
class VertexBuffer;
} // namespace GL

// The marks skidding tyres leave on the ground: a strip behind each wheel,
// quad after quad, with the effects file's shader for the ground (skid_m on the
// road, grass_skidmark_shader, dirt_skidmark_shader). So many a shader are
// kept; the oldest go first.
class SkidMarks
{
public:
	SkidMarks();
	~SkidMarks();

	// Wheel `track` (any key, one a wheel) skids at `point` on the ground,
	// `normal` up from it, its axle along `axle`, `width` wide; `strength`, 0 to
	// 1, how dark. Called every frame it skids: the strip goes on from where it
	// was, or starts here.
	void Lay(uint64_t track, const Vector3& point, const Vector3& normal, const Vector3& axle, float width,
	         const std::string& shader, float strength);
	// It no longer skids: its strip ends.
	void Lift(uint64_t track) { _tracks.erase(track); }
	void Clear();
	// With the world program, its viewProj and fog set.
	void Draw(GL::ShaderProgram& worldShader);

	struct Vertex
	{
		Vector3 pos;
		Vector2 uv;
		Vector2 lightmapUV;
		Vector4 color;
	};

private:
	struct Strip
	{
		std::vector<Vertex> vertices;
		std::size_t next = 0, count = 0;
		std::shared_ptr<GL::VertexBuffer> buffer;
		std::shared_ptr<GL::VertexBinding> binding;
		// The quads written since the last upload.
		std::size_t dirtyFrom = SIZE_MAX, dirtyTo = 0;
	};
	struct Track
	{
		std::string shader;
		Vector3 left, right;
		float v = 0.0f;
		float strength = 0.0f;
	};
	std::map<std::string, Strip> _strips;
	std::unordered_map<uint64_t, Track> _tracks;
	std::shared_ptr<GL::IndexBuffer> _indices;
	void addQuad(const std::string& shader, const Track& from, const Track& to);
};

} // namespace Donut
