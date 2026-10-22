// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <P3D/P3D.generated.h>
#include <Render/Font.h>
#include <Render/Mesh.h>
#include <Render/Shader.h>
#include <Render/Texture.h>
#include <ResourceManager.h>
#include <algorithm>
#include <set>

#include "Render/OpenGL/glad/glad.h"
#include "Render/imgui/imgui.h"
#include "Core/Log.h"

#include <chrono>
#include <cstdint>
#include <ctime>

namespace Donut
{

ResourceManager::ResourceManager() = default;
ResourceManager::~ResourceManager() = default;

void ResourceManager::LoadTexture(const P3D::Texture& texture)
{
	if (const auto old = _textures.find(texture.GetName()); old != _textures.end())
	{
		Log::Debug("Texture {0} loaded again (each load is kept, see GetShader)", texture.GetName());
		_retiredTextures.push_back(std::move(old->second));
	}

	addTexture(texture.GetName(), std::make_unique<Texture>(texture));
}

void ResourceManager::LoadTexture(const P3D::Sprite& sprite)
{
	if (const auto old = _textures.find(sprite.GetName()); old != _textures.end())
	{
		Log::Debug("Sprite {0} loaded again (each load is kept, see GetShader)", sprite.GetName());
		_retiredTextures.push_back(std::move(old->second));
	}

	addTexture(sprite.GetName(), std::make_unique<Texture>(sprite));
}

void ResourceManager::LoadShader(const P3D::Shader& shader)
{
	if (const auto old = _shaders.find(shader.GetName()); old != _shaders.end())
	{
		Log::Debug("Shader {0} loaded again (each load is kept, see GetShader)", shader.GetName());
		_retiredShaders.push_back(std::move(old->second));
	}

	auto loaded = std::make_unique<Shader>(shader);
	loaded->SetLoadIndex(_loadIndex);
	_shaderLoads[shader.GetName()].emplace_back(_loadIndex++, loaded.get());
	_shaders[shader.GetName()] = std::move(loaded);
}

void ResourceManager::LoadSet(const P3D::Set& set)
{
	if (const auto old = _textures.find(set.GetName()); old != _textures.end())
	{
		Log::Debug("Set {0} loaded again (each load is kept, see GetShader)", set.GetName());
		_retiredTextures.push_back(std::move(old->second));
	}

	std::srand((uint32_t)std::time(0));
	int idx = std::rand() % set.GetTextures().size();
	addTexture(set.GetName(), std::make_unique<Texture>(*set.GetTextures().at(idx)));
}

void ResourceManager::LoadGeometry(const P3D::Geometry& geo)
{
	if (const auto old = _geometries.find(geo.GetName()); old != _geometries.end())
	{
		Log::Debug("Geometry {0} loaded again (only unique names are looked up)", geo.GetName());
		_retiredGeometries.push_back(std::move(old->second));
	}

	_geometries[geo.GetName()] = std::make_unique<Mesh>(geo);
}

void ResourceManager::AddTexture(const std::string& name, std::unique_ptr<Texture> texture)
{
	if (const auto old = _textures.find(name); old != _textures.end())
		_retiredTextures.push_back(std::move(old->second));
	addTexture(name, std::move(texture));
}

void ResourceManager::addTexture(const std::string& name, std::unique_ptr<Texture> texture)
{
	_textureLoads[name].emplace_back(_loadIndex++, texture.get());
	_textures[name] = std::move(texture);
}

void ResourceManager::resolveTexture(Shader& shader) const
{
	if (shader.IsTextureResolved())
		return;
	const auto loads = _textureLoads.find(shader.GetDiffuseTextureName());
	if (loads == _textureLoads.end() || loads->second.empty())
	{
		// Not loaded yet, perhaps: another texture meanwhile, and another look
		// next time.
		static std::set<std::string> warned;
		if (warned.insert(shader.GetDiffuseTextureName()).second)
			Log::Warn("could not find texture {}", shader.GetDiffuseTextureName());
		if (!_textures.empty())
			shader.SetDiffuseTexture(_textures.begin()->second.get()); // todo: set an error texture
		return;
	}
	shader.SetTextureResolved();
	const auto nearest = std::min_element(loads->second.begin(), loads->second.end(),
	                                      [&](const auto& a, const auto& b)
	                                      {
		                                      const auto distance = [&](uint64_t index) {
			                                      return index > shader.GetLoadIndex() ? index - shader.GetLoadIndex()
			                                                                           : shader.GetLoadIndex() - index;
		                                      };
		                                      return distance(a.first) < distance(b.first);
	                                      });
	shader.SetDiffuseTexture(nearest->second);
}

void ResourceManager::AddFont(const std::string& name, std::unique_ptr<Font> font)
{
	_fonts[name] = std::move(font);
}

void ResourceManager::ImGuiDebugWindow(bool* p_open) const
{
	ImGui::SetNextWindowSize(ImVec2(330, 400), ImGuiCond_Once);
	if (!ImGui::Begin("Resource Manager", p_open))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Textures: %zu", _textures.size());
	ImGui::SameLine();
	ImGui::Text("Shaders: %zu", _shaders.size());
	ImGui::SameLine();
	ImGui::Text("Fonts: %zu", _fonts.size());

	ImGui::BeginTabBar("rmtabs");

	if (ImGui::BeginTabItem("Textures"))
	{
		const ImVec2 windowSize = ImGui::GetWindowSize();
		int perLine = static_cast<int>(windowSize.x) / 72;
		if (perLine == 0)
			perLine = 1;

		int i = 0;
		for (auto const& [name, texture] : _textures)
		{
			ImGui::Image((ImTextureID)(intptr_t)texture->GetOpenGLHandle(),
			             ImVec2((float)texture->GetWidth(), (float)texture->GetHeight()));
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", name.c_str());

			if (++i % perLine != 0)
				ImGui::SameLine();
		}

		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();

	ImGui::End();
}

bool ResourceManager::ShowTextureAs(const std::string& textureName, const std::string& shownAs)
{
	const auto texture = _textures.find(shownAs);
	if (texture == _textures.end())
		return false;
	for (auto& [name, shader] : _shaders)
		if (shader->GetDiffuseTextureName() == textureName)
		{
			shader->SetDiffuseTexture(texture->second.get());
			shader->SetTextureResolved();
		}
	return true;
}

bool ResourceManager::ShowShaderTexture(const std::string& shader, const std::string& texture)
{
	const auto foundShader = _shaders.find(shader);
	const auto foundTexture = _textures.find(texture);
	if (foundShader == _shaders.end() || foundTexture == _textures.end())
		return false;
	foundShader->second->SetDiffuseTexture(foundTexture->second.get());
	foundShader->second->SetTextureResolved();
	return true;
}

/*
 * Searches for the shader in the map, you should cache the result to avoid unnecessary lookups
 */
Shader* ResourceManager::GetShader(const std::string& name) const
{
	if (_shaders.find(name) == _shaders.end())
	{
		// Once a name: a mesh that asks for it every frame filled the log.
		static std::set<std::string> warned;
		if (warned.insert(name).second)
			Log::Warn("could not find shader {0}", name);
		return nullptr; // todo: return an error shader
	}

	auto const& shader = _shaders.at(name);
	resolveTexture(*shader);

	if (!shader->GetEnvTextureName().empty() && !shader->GetEnvTexture())
		if (const auto env = _textures.find(shader->GetEnvTextureName()); env != _textures.end())
			shader->SetEnvTexture(env->second.get());

	return shader.get();
}

Shader* ResourceManager::GetShader(const std::string& name, uint64_t near) const
{
	const auto loads = _shaderLoads.find(name);
	if (loads == _shaderLoads.end() || loads->second.empty())
		return GetShader(name);
	const auto nearest =
	    std::min_element(loads->second.begin(), loads->second.end(),
	                     [&](const auto& a, const auto& b)
	                     {
		                     const auto distance = [&](uint64_t index) { return index > near ? index - near : near - index; };
		                     return distance(a.first) < distance(b.first);
	                     });
	Shader* shader = nearest->second;
	resolveTexture(*shader);
	if (!shader->GetEnvTextureName().empty() && !shader->GetEnvTexture())
		if (const auto env = _textures.find(shader->GetEnvTextureName()); env != _textures.end())
			shader->SetEnvTexture(env->second.get());
	return shader;
}

Texture* ResourceManager::GetTexture(const std::string& name) const
{
	// todo: return missing texture
	if (_textures.find(name) == _textures.end())
		return nullptr;

	return _textures.at(name).get();
}

Mesh* ResourceManager::GetGeometry(const std::string& name) const
{
	if (_geometries.find(name) == _geometries.end())
		return nullptr;

	return _geometries.at(name).get();
}

Font* ResourceManager::GetFont(const std::string& name) const
{
	if (_fonts.find(name) == _fonts.end())
		return nullptr;

	return _fonts.at(name).get();
}

} // namespace Donut
