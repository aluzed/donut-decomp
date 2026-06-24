// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "ShaderProgram.h"

#include "Core/Log.h"
#include "Core/Math/Matrix3x3.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <functional>
#include <vector>

namespace Donut::GL
{

namespace
{

// On-disk cache layout: a short header (magic + binary format) followed by the
// raw program blob. Keyed by a hash of the GLSL sources so any edit invalidates.
constexpr uint32_t kCacheMagic = 0x4453544Bu; // "DSTK" (donut shader cache)

std::filesystem::path cachePathFor(uint64_t hash)
{
	return std::filesystem::path("cache") / "shaders" / (std::to_string(hash) + ".bin");
}

uint64_t sourceHash(const std::string& vert, const std::string& frag)
{
	uint64_t h = 1469598103934665603ull; // FNV-1a offset basis
	auto mix = [&](const std::string& s) {
		for (unsigned char c : s)
		{
			h ^= c;
			h *= 1099511628211ull;
		}
		h ^= '|';
		h *= 1099511628211ull;
	};
	mix(vert);
	mix(frag);
	return h;
}

bool cacheDisabled()
{
	return std::getenv("DONUT_NO_SHADER_CACHE") != nullptr;
}

bool tryLoadFromCache(GLuint program, uint64_t hash)
{
	if (cacheDisabled())
		return false;

	auto path = cachePathFor(hash);
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;

	uint32_t magic = 0;
	GLenum format = 0;
	in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	in.read(reinterpret_cast<char*>(&format), sizeof(format));
	if (!in || magic != kCacheMagic)
		return false;

	std::vector<uint8_t> blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (blob.empty())
		return false;

	glProgramBinary(program, format, blob.data(), static_cast<GLsizei>(blob.size()));

	GLint linkStatus = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
	return linkStatus == GL_TRUE;
}

void trySaveToCache(GLuint program, uint64_t hash)
{
	if (cacheDisabled())
		return;

	GLint length = 0;
	glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &length);
	if (length <= 0)
		return;

	std::vector<uint8_t> blob(static_cast<std::size_t>(length));
	GLenum format = 0;
	GLsizei written = 0;
	glGetProgramBinary(program, length, &written, &format, blob.data());
	if (written <= 0)
		return;

	auto path = cachePathFor(hash);
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);

	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out)
		return;
	out.write(reinterpret_cast<const char*>(&kCacheMagic), sizeof(kCacheMagic));
	out.write(reinterpret_cast<const char*>(&format), sizeof(format));
	out.write(reinterpret_cast<const char*>(blob.data()), written);
}

} // namespace

ShaderProgram::ShaderProgram(const std::string& vertexSource, const std::string& fragmentSource)
{
	_program = glCreateProgram();

	assert(_program != 0);

	const uint64_t hash = sourceHash(vertexSource, fragmentSource);

	auto populateUniforms = [this]() {
		int uniformCount = -1;
		glGetProgramiv(_program, GL_ACTIVE_UNIFORMS, &uniformCount);
		for (int i = 0; i < uniformCount; i++)
		{
			int name_len = -1, num = -1;
			GLenum type = GL_ZERO;
			char name[64];
			glGetActiveUniform(_program, GLuint(i), sizeof(name) - 1, &name_len, &num, &type, name);
			name[name_len] = 0;
			_uniforms[std::string(name)] = glGetUniformLocation(_program, name);
		}
	};

	// Fast path: reuse a previously cached program binary if the sources match.
	if (tryLoadFromCache(_program, hash))
	{
		populateUniforms();
		trySaveToCache(_program, hash); // refresh in case the driver changed format
		return;
	}

	GLuint vertexShader = createSubShader(GL_VERTEX_SHADER, vertexSource.c_str());
	GLuint fragmentShader = createSubShader(GL_FRAGMENT_SHADER, fragmentSource.c_str());

	assert(vertexShader != 0 && fragmentShader != 0);

	glAttachShader(_program, vertexShader);
	glAttachShader(_program, fragmentShader);
	glLinkProgram(_program);

	GLint linkStatus = GL_FALSE;
	glGetProgramiv(_program, GL_LINK_STATUS, &linkStatus);

	if (linkStatus == GL_FALSE)
	{
		GLint infoLogLen = 0;
		glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &infoLogLen);

		std::string log;
		if (infoLogLen > 0)
		{
			log.resize(infoLogLen);
			glGetProgramInfoLog(_program, infoLogLen, &infoLogLen, &log[0]);
		}
		Log::Error("ShaderProgram link FAILED (vert={} bytes, frag={} bytes): {}",
		           vertexSource.size(), fragmentSource.size(),
		           log.empty() ? "(no log)" : log);

		glDeleteProgram(_program);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return;
	}

	// we can delete these now they exist in the program
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	populateUniforms();

	// Warm the cache for next launch (best-effort, driver may not support it).
	trySaveToCache(_program, hash);
}

ShaderProgram::~ShaderProgram()
{
	if (_program != 0)
		glDeleteProgram(_program);
}

void ShaderProgram::Bind()
{
	glUseProgram(_program);
}

void ShaderProgram::Unbind()
{
	glUseProgram(0);
}

void ShaderProgram::SetUniformValue(const char* uniformName, int value)
{
	glUniform1i(_uniforms[uniformName], value);
}

void ShaderProgram::SetUniformValue(const char* uniformName, float value)
{
	glUniform1f(_uniforms[uniformName], value);
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Vector2& v)
{
	glUniform2fv(_uniforms[uniformName], 1, &v.X);
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Vector3& v)
{
	glUniform3fv(_uniforms[uniformName], 1, v.Data());
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Vector4& v)
{
	glUniform4fv(_uniforms[uniformName], 1, &v.X);
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Matrix3x3& m)
{
	glUniformMatrix3fv(_uniforms[uniformName], 1, GL_FALSE, &m.M[0][0]);
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Matrix4x4& m)
{
	glUniformMatrix4fv(_uniforms[uniformName], 1, GL_FALSE, m.M16);
}

void ShaderProgram::SetUniformValue(const char* uniformName, std::size_t count, const Matrix4x4* m)
{
	glUniformMatrix4fv(_uniforms[uniformName], (GLsizei)count, GL_FALSE, &m[0].M[0][0]);
}

GLuint ShaderProgram::createSubShader(GLenum type, const std::string& source)
{
	GLuint shader = glCreateShader(type);

	const GLchar* glsource = (const GLchar*)source.c_str();
	glShaderSource(shader, 1, &glsource, nullptr);
	glCompileShader(shader);

	GLint compileStatus = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);

	if (compileStatus == GL_FALSE)
	{
		GLint infoLogLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);

		char* infoLog = new char[infoLogLen];
		glGetShaderInfoLog(shader, infoLogLen, &infoLogLen, infoLog);
		Log::Error("ShaderProgram compile errors:\n{}", infoLog);
		delete[] infoLog;

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

} // namespace Donut::GL
