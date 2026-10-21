// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Donut::P3D
{

class P3DChunk;

enum class FileTypes : uint32_t
{
	P3D = 0xFF443350,  // 'P3D'
	P3DZ = 0x5A443350, // 'P3DZ' proprietary compression
	RZ = 0x5A52,       // 'RZ' (zlib deflate)
};

// Radical's 'P3DZ' compression: `in` is the whole file, magic included.
// False when it is not one or does not decompress whole.
bool DecompressP3DZ(const std::vector<uint8_t>& in, std::vector<uint8_t>& out);

class P3DFile
{
public:
	P3DFile(const std::string& file);
	~P3DFile();

	const P3DChunk& GetRoot() const { return *_root.get(); }
	const std::string& GetFileName() const { return _filename; }
	// The file's chunks as read: the file itself, or what it decompresses to.
	const std::vector<uint8_t>& GetBytes() const { return _data; }

protected:
	std::string _filename;
	std::unique_ptr<P3DChunk> _root;
	std::vector<uint8_t> _data;
};

} // namespace Donut::P3D
