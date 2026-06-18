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
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Donut::GL
{

ShaderProgram::ShaderProgram(const std::string& vertexSource, const std::string& fragmentSource)
{
	_program = glCreateProgram();

	// lazy assert, todo: better error handling
	assert(_program != 0);

	GLuint vertexShader = createSubShader(GL_VERTEX_SHADER, vertexSource.c_str());
	GLuint fragmentShader = createSubShader(GL_FRAGMENT_SHADER, fragmentSource.c_str());

	// lazy assert, todo: better error handling
	assert(vertexShader != 0 && fragmentShader != 0);

	glAttachShader(_program, vertexShader);
	glAttachShader(_program, fragmentShader);
	glLinkProgram(_program);

	GLint linkStatus = GL_FALSE;
	glGetProgramiv(_program, GL_LINK_STATUS, &linkStatus);

	if (linkStatus == GL_FALSE)
	{
		GLint infoLogLen = 0;
		glGetShaderiv(_program, GL_INFO_LOG_LENGTH, &infoLogLen);

		char* infoLog = new char[infoLogLen];
		glGetProgramInfoLog(_program, infoLogLen, &infoLogLen, infoLog);
		Log::Error("ShaderProgram linking errors:\n{}", infoLog);
		delete[] infoLog;

		glDeleteProgram(_program);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return;
	}

	// we can delete these now they exist in the program
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

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
	auto it = _uniforms.find(uniformName);
	if (it != _uniforms.end())
		glUniform1i(it->second, value);
}

void ShaderProgram::SetUniformValue(const char* uniformName, float value)
{
	auto it = _uniforms.find(uniformName);
	if (it != _uniforms.end())
		glUniform1f(it->second, value);
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Vector2& v)
{
	auto it = _uniforms.find(uniformName);
	if (it != _uniforms.end())
		glUniform2fv(it->second, 1, &v.X);
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Vector3& v)
{
	auto it = _uniforms.find(uniformName);
	if (it != _uniforms.end())
		glUniform3fv(it->second, 1, v.Data());
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Vector4& v)
{
	auto it = _uniforms.find(uniformName);
	if (it != _uniforms.end())
		glUniform4fv(it->second, 1, &v.X);
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Matrix3x3& m)
{
	auto it = _uniforms.find(uniformName);
	if (it != _uniforms.end())
		glUniformMatrix3fv(it->second, 1, GL_FALSE, &m.M[0][0]);
}

void ShaderProgram::SetUniformValue(const char* uniformName, const Matrix4x4& m)
{
	auto it = _uniforms.find(uniformName);
	if (it != _uniforms.end())
		glUniformMatrix4fv(it->second, 1, GL_FALSE, m.M16);
}

void ShaderProgram::SetUniformValue(const char* uniformName, std::size_t count, const Matrix4x4* m)
{
	auto it = _uniforms.find(uniformName);
	if (it != _uniforms.end())
		glUniformMatrix4fv(it->second, (GLsizei)count, GL_FALSE, &m[0].M[0][0]);
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
