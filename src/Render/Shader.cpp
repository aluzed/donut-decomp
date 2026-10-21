// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Core/Log.h>
#include <P3D/P3D.generated.h>
#include <Render/Shader.h>

#include <cstdint>

namespace Donut
{

void ApplyBlendMode(BlendMode mode)
{
	glEnable(GL_BLEND);
	switch (mode)
	{
	case BlendMode::Additive:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_ONE, GL_ONE);
		break;
	case BlendMode::Subractive:
		glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		glBlendFunc(GL_ONE, GL_ONE);
		break;
	case BlendMode::Modulate:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_DST_COLOR, GL_ZERO);
		break;
	case BlendMode::Modulate2:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		break;
	case BlendMode::ModulateAlpha:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_DST_COLOR, GL_SRC_ALPHA);
		break;
	case BlendMode::SubtractAlpha:
		glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
		break;
	default:
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	}
}

Shader::Shader(const P3D::Shader& shader): _name(shader.GetName())
{
	auto const& pddiName = shader.GetPddiShaderName();
	if (pddiName != "simple")
		Log::Warn("shader {}: unhandled pddi type: {}", _name, pddiName);

	_isTranslucent = shader.GetIsTrans() == 1;

	for (const auto& textureParam : shader.GetTextureParams())
	{
		if (textureParam->GetKey() == "TEX")
			_textureName = textureParam->GetValue();
	}

	glGenSamplers(1, &_glSampler);

	for (const auto& param : shader.GetFloatParams())
	{
		if (param->GetKey() == "CBVV") {}
		else if (param->GetKey() == "MSHP")
		{
		}
	}

	uint32_t uvmd = 0;

	for (const auto& param : shader.GetIntegerParams())
	{
		if (param->GetKey() == "FIMD")
		{
			switch (param->GetValue())
			{
			case 0: // NN
				glSamplerParameteri(_glSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
				glSamplerParameteri(_glSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				break;
			case 1: // Linear
				glSamplerParameteri(_glSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glSamplerParameteri(_glSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				break;
			case 2: // NN, Mip NN
				glSamplerParameteri(_glSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
				glSamplerParameteri(_glSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				break;
			case 3: // Linear, Mip NN
				glSamplerParameteri(_glSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
				glSamplerParameteri(_glSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				break;
			case 4: // Linear, Mip Linear
				glSamplerParameteri(_glSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glSamplerParameteri(_glSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				break;
			default: throw std::runtime_error("bad filter mode");
			}
		}
		else if (param->GetKey() == "UVMD")
		{
			uvmd = param->GetValue();
			GLint glparam = (uvmd == 0) ? GL_REPEAT : GL_CLAMP_TO_EDGE;

			glSamplerParameteri(_glSampler, GL_TEXTURE_WRAP_S, glparam);
			glSamplerParameteri(_glSampler, GL_TEXTURE_WRAP_T, glparam);
		}
		else if (param->GetKey() == "2SID")
		{
			_twoSided = param->GetValue() == 1;
		}
		else if (param->GetKey() == "ATST")
		{
			_alphaTested = param->GetValue() == 1;
		}
		else if (param->GetKey() == "BLMD")
		{
			_blendMode = (BlendMode)param->GetValue();
		}
		else if (param->GetKey() == "LIT")
			_lit = param->GetValue() != 0;
	}
	for (const auto& param : shader.GetColorParams())
	{
		const Vector3 color(param->GetR() / 255.0f, param->GetG() / 255.0f, param->GetB() / 255.0f);
		if (param->GetKey() == "AMBI")
			_ambient = color;
		else if (param->GetKey() == "DIFF")
			_diffuse = color;
		else if (param->GetKey() == "EMIS")
			_emissive = color;
	}

	glSamplerParameterf(_glSampler, 0x84FE, 16);
}

Shader::~Shader()
{
	if (_glSampler != 0)
		glDeleteSamplers(1, &_glSampler);
}

void Shader::SetDiffuseTexture(Texture* diffuseTexture)
{
	_diffuseTexture = diffuseTexture;
}

void Shader::Bind(GLuint unit) const
{
	_diffuseTexture->Bind(unit);
	glBindSampler(unit, _glSampler);
}

} // namespace Donut
