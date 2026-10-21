#pragma once
#include "OpenGL/glad/glad.h"

#include <Render/Texture.h>
#include "Core/Math/Vector3.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace Donut
{

// forward declare
namespace P3D
{
class Shader;
}

// A shader's BLMD, as Pure3D's Direct3D renderer blends it (pddidx8r.dll's
// table at 0x1005d600: operation, source, destination).
enum class BlendMode : uint32_t
{
	None,          // no blending
	Alpha,         // add, source alpha, one less source alpha
	Additive,      // add, one, one
	Subractive,    // reverse subtract, one, one: what is below, less it (ground shadows)
	Modulate,      // add, destination colour, zero
	Modulate2,     // add, destination colour, source colour
	ModulateAlpha, // add, destination colour, source alpha
	SubtractAlpha, // reverse subtract, source alpha, source alpha
};

// Sets the blending to `mode`'s (its equation and factors), blending on.
void ApplyBlendMode(BlendMode mode);
// Back to adding, the equation everything else assumes.
inline void ResetBlendEquation()
{
	glBlendEquation(GL_FUNC_ADD);
}

class Shader
{
public:
	Shader(const P3D::Shader&);
	~Shader();

	// void SetShaderEffect(const std::string& effect);
	// std::string GetShaderEffect() const { return _shaderEffect; }

	void SetDiffuseTexture(Texture* diffuseTexture);
	const Texture* GetDiffuseTexture() const { return _diffuseTexture; }

	// used to SetDiffuseTexture, won't be needed if Shader is constructed independent of P3D::Shader
	std::string GetDiffuseTextureName() const { return _textureName; }

	// A "spheremap" shader's reflection (the cars' paint): its sphere map
	// (REFL, "EnvMap.bmp") and how much of it is added (ENVB); none, black.
	const std::string& GetEnvTextureName() const { return _envTextureName; }
	void SetEnvTexture(const Texture* texture) { _envTexture = texture; }
	const Texture* GetEnvTexture() const { return _envTexture; }
	const Vector3& GetEnvColor() const { return _envColor; }

	// Diffuse Color
	//

	void Bind(GLuint unit) const;

	bool HasLighting() const { return _hasLighting; }
	// Lit by the scene's lights (LIT), and how: its ambient (AMBI), diffuse
	// (DIFF) and emissive (EMIS) colours. A car's body takes no ambient light:
	// lit only from where the sun shines.
	bool IsLit() const { return _lit; }
	const Vector3& GetAmbient() const { return _ambient; }
	const Vector3& GetDiffuse() const { return _diffuse; }
	const Vector3& GetEmissive() const { return _emissive; }
	bool IsAlphaTested() const { return _alphaTested; }
	bool IsTwoSided() const { return _twoSided; }

	bool IsTranslucent() const { return _isTranslucent; }

	BlendMode GetBlendMode() const { return _blendMode; }

	// When it was loaded (ResourceManager's load index), and whether its
	// texture has been found for it.
	uint64_t GetLoadIndex() const { return _loadIndex; }
	void SetLoadIndex(uint64_t index) { _loadIndex = index; }
	bool IsTextureResolved() const { return _textureResolved; }
	void SetTextureResolved() { _textureResolved = true; }

protected:
	std::string _name;
	std::string _textureName;

	std::string _shaderEffect;

	Texture* _diffuseTexture;
	Texture* _envmapTexture;

	GLuint _glSampler;

	bool _isTranslucent;

	bool _hasLighting;
	bool _alphaTested;
	bool _twoSided;

	BlendMode _blendMode;
	bool _lit = false;
	Vector3 _ambient = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 _diffuse = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 _emissive = Vector3::Zero;
	uint64_t _loadIndex = 0;
	bool _textureResolved = false;
	std::string _envTextureName;
	const Texture* _envTexture = nullptr;
	Vector3 _envColor = Vector3::Zero;
};

} // namespace Donut
