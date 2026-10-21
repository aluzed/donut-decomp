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

enum class BlendMode : uint32_t
{
	None,
	Alpha,
	Additive,
	Subractive,
};

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
};

} // namespace Donut
