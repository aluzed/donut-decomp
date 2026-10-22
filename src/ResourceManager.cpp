// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <P3D/P3D.generated.h>
#include <Render/Font.h>
#include <Render/Mesh.h>
#include <Render/Shader.h>
#include <Render/Texture.h>
#include <ResourceManager.h>
#include <algorithm>
#include <set>
#include <string_view>

#include "Render/OpenGL/glad/glad.h"
#include "Render/imgui/imgui.h"
#include "Core/Log.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <unordered_set>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace Donut
{

ResourceManager::ResourceManager() = default;
ResourceManager::~ResourceManager() = default;

void ResourceManager::LoadTexture(const P3D::Texture& texture)
{
	// The same picture again (a character's swatches, every time a character
	// is loaded; a prop shared by the level's files): one loaded before, under
	// the new load's index and the name's again, not another copy.
	const auto& image = texture.GetImage();
	const std::size_t hash = image ? std::hash<std::string_view>()(std::string_view(
	                                     reinterpret_cast<const char*>(image->GetData().data()), image->GetData().size())) ^
	                                     (static_cast<std::size_t>(image->GetFormat()) << 1)
	                               : 0;
	auto& seen = _textureHashes[texture.GetName()];
	const auto same = std::find_if(seen.begin(), seen.end(), [&](const auto& load) { return load.first == hash; });
	if (const auto old = _textures.find(texture.GetName()); old != _textures.end())
	{
		if (image && same != seen.end())
		{
			if (old->second.get() != same->second)
			{
				const auto retired = std::find_if(_retiredTextures.begin(), _retiredTextures.end(),
				                                  [&](const auto& kept) { return kept.get() == same->second; });
				if (retired != _retiredTextures.end())
					std::swap(*retired, old->second);
			}
			if (old->second.get() == same->second)
			{
				asked(same->second);
				_textureLoads[texture.GetName()].emplace_back(_loadIndex++, same->second);
				return;
			}
		}
		Log::Debug("Texture {0} loaded again (each load is kept, see GetShader)", texture.GetName());
		_retiredTextures.push_back(std::move(old->second));
	}

	addTexture(texture.GetName(), std::make_unique<Texture>(texture));
	if (image)
		seen.emplace_back(hash, _textures[texture.GetName()].get());
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
	record(loaded.get(), Kind::Shader, sizeof(Shader), 0);
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

	auto mesh = std::make_unique<Mesh>(geo);
	record(mesh.get(), Kind::Geometry, mesh->GetCpuBytes(), mesh->GetGpuBytes());
	_geometries[geo.GetName()] = std::move(mesh);
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
	record(texture.get(), Kind::Texture, 0, texture->GetGpuBytes());
	_textures[name] = std::move(texture);
}

void ResourceManager::record(const void* resource, Kind kind, std::size_t cpuBytes, std::size_t gpuBytes)
{
	const std::size_t section = _sections.size() - 1;
	_records[resource] = Record {kind, section, section, cpuBytes, gpuBytes};
}

void ResourceManager::asked(const void* resource) const
{
	// Nothing is ever freed here, so a pointer is never another's again.
	if (const auto found = _records.find(resource); found != _records.end())
		found->second.lastAsked = _sections.size() - 1;
}

void ResourceManager::BeginSection(const std::string& name)
{
	Log::Info("Resources on leaving section '{}':\n{}", _sections.back(), SectionReport());
	_sections.push_back(name);
}

namespace
{
// The process's resident memory, in bytes; 0 where it is not known.
std::size_t residentBytes()
{
#if defined(__linux__)
	std::ifstream statm("/proc/self/statm");
	std::size_t pages = 0, resident = 0;
	if (statm >> pages >> resident)
		return resident * static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
	return 0;
}

// The graphics card's memory, total and in use by every process, in bytes,
// where the driver tells (GL_NVX_gpu_memory_info); 0 otherwise.
std::pair<std::size_t, std::size_t> cardBytes()
{
	static const bool known = []
	{
		GLint count = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &count);
		for (GLint i = 0; i < count; ++i)
			if (const auto* name = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
			    name && std::strcmp(name, "GL_NVX_gpu_memory_info") == 0)
				return true;
		return false;
	}();
	if (!known)
		return {0, 0};
	constexpr GLenum kTotalKb = 0x9048, kAvailableKb = 0x9049;
	GLint total = 0, available = 0;
	glGetIntegerv(kTotalKb, &total);
	glGetIntegerv(kAvailableKb, &available);
	return {static_cast<std::size_t>(total) * 1024, static_cast<std::size_t>(total - available) * 1024};
}
} // namespace

std::string ResourceManager::SectionReport() const
{
	std::unordered_set<const void*> live;
	for (const auto& [name, texture] : _textures) live.insert(texture.get());
	for (const auto& [name, shader] : _shaders) live.insert(shader.get());
	for (const auto& [name, mesh] : _geometries) live.insert(mesh.get());

	struct Totals
	{
		std::size_t count = 0, bytes = 0;
		void Add(const Record& r) { ++count, bytes += r.cpuBytes + r.gpuBytes; }
	};
	struct Row
	{
		Totals textures, geometries, shaders, replaced, asked;
	};
	const std::size_t current = _sections.size() - 1;
	std::vector<Row> rows(_sections.size());
	Row all;
	for (const auto& [resource, r] : _records)
	{
		for (Row* row : {&rows[r.section], &all})
		{
			if (!live.count(resource))
				row->replaced.Add(r);
			else if (r.kind == Kind::Texture)
				row->textures.Add(r);
			else if (r.kind == Kind::Geometry)
				row->geometries.Add(r);
			else
				row->shaders.Add(r);
			if (r.lastAsked == current)
				row->asked.Add(r);
		}
	}

	const auto cell = [](const Totals& t) { return fmt::format("{} / {:.1f} MB", t.count, t.bytes / 1048576.0); };
	const auto line = [&](const std::string& name, const Row& row)
	{
		return fmt::format("{:<20} {:>18} {:>18} {:>8} {:>18} {:>18}\n", name, cell(row.textures), cell(row.geometries),
		                   row.shaders.count, cell(row.replaced), cell(row.asked));
	};
	std::string report = fmt::format("{:<20} {:>18} {:>18} {:>8} {:>18} {:>18}\n", "loaded in", "textures", "geometries",
	                                 "shaders", "replaced", "asked in current");
	for (std::size_t i = 0; i < rows.size(); ++i)
		if (rows[i].textures.count + rows[i].geometries.count + rows[i].shaders.count + rows[i].replaced.count > 0)
			report += line(fmt::format("{} {}", i, _sections[i]), rows[i]);
	report += line("all", all);
	const auto [cardTotal, cardUsed] = cardBytes();
	report += fmt::format("process resident: {:.0f} MB; graphics card: {:.0f} of {:.0f} MB in use (every process)",
	                      residentBytes() / 1048576.0, cardUsed / 1048576.0, cardTotal / 1048576.0);
	return report;
}

void ResourceManager::resolveTexture(Shader& shader) const
{
	if (shader.IsTextureResolved())
		return;
	// Untextured (a few characters' and props' shaders name no texture): drawn
	// in their colours alone, on white, not on whichever texture came first.
	if (!_white)
	{
		const uint8_t white[4] = {255, 255, 255, 255};
		_white = std::make_unique<Texture>(white, 1, 1, "white");
	}
	if (shader.GetDiffuseTextureName().empty())
	{
		shader.SetDiffuseTexture(_white.get());
		shader.SetTextureResolved();
		return;
	}
	const auto loads = _textureLoads.find(shader.GetDiffuseTextureName());
	if (loads == _textureLoads.end() || loads->second.empty())
	{
		// Not loaded yet, perhaps: another texture meanwhile, and another look
		// next time.
		static std::set<std::string> warned;
		if (warned.insert(shader.GetDiffuseTextureName()).second)
			Log::Warn("could not find texture {}", shader.GetDiffuseTextureName());
		shader.SetDiffuseTexture(_white.get());
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

	if (ImGui::BeginTabItem("Sections"))
	{
		ImGui::TextUnformatted(SectionReport().c_str());
		ImGui::EndTabItem();
	}

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
	asked(shader.get());
	asked(shader->GetDiffuseTexture());

	if (!shader->GetEnvTextureName().empty() && !shader->GetEnvTexture())
		if (const auto env = _textures.find(shader->GetEnvTextureName()); env != _textures.end())
			shader->SetEnvTexture(env->second.get());
	asked(shader->GetEnvTexture());

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
	asked(shader);
	asked(shader->GetDiffuseTexture());
	if (!shader->GetEnvTextureName().empty() && !shader->GetEnvTexture())
		if (const auto env = _textures.find(shader->GetEnvTextureName()); env != _textures.end())
			shader->SetEnvTexture(env->second.get());
	asked(shader->GetEnvTexture());
	return shader;
}

Texture* ResourceManager::GetTexture(const std::string& name) const
{
	// todo: return missing texture
	if (_textures.find(name) == _textures.end())
		return nullptr;

	asked(_textures.at(name).get());
	return _textures.at(name).get();
}

Mesh* ResourceManager::GetGeometry(const std::string& name) const
{
	if (_geometries.find(name) == _geometries.end())
		return nullptr;

	asked(_geometries.at(name).get());
	return _geometries.at(name).get();
}

Font* ResourceManager::GetFont(const std::string& name) const
{
	if (_fonts.find(name) == _fonts.end())
		return nullptr;

	return _fonts.at(name).get();
}

} // namespace Donut
