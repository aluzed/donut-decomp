// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Core/File.h>
#include <P3D/P3DChunk.h>
#include <P3D/P3DFile.h>

#include <zlib.h>

namespace Donut::P3D
{

P3DFile::P3DFile(const std::string& path): _filename(path)
{
	File file;
	file.Open(path, FileMode::Read);

	uint32_t type;
	file.Read(&type, 1);

	if (type == static_cast<uint32_t>(FileTypes::RZ))
	{
		const std::size_t compressedSize = file.Size() - sizeof(uint32_t);
		std::vector<uint8_t> compressed(compressedSize);
		file.ReadBytes(compressed.data(), compressedSize);

		uLongf uncompressedSize = compressedSize * 4;
		std::vector<uint8_t> uncompressed;
		int ret;
		do
		{
			uncompressed.resize(uncompressedSize);
			ret = uncompress(uncompressed.data(), &uncompressedSize, compressed.data(), static_cast<uLong>(compressedSize));
			uncompressedSize *= 2;
		} while (ret == Z_BUF_ERROR);

		if (ret != Z_OK)
		{
			file.Close();
			return;
		}

		uncompressed.resize(uncompressedSize);
		_root = std::make_unique<P3DChunk>(uncompressed);
		file.Close();
		return;
	}

	assert(type == static_cast<uint32_t>(FileTypes::P3D));

	file.Seek(0, FileSeekMode::Begin);

	const std::size_t size = file.Size();
	_data.resize(size);

	file.ReadBytes(_data.data(), size);
	file.Close();

	_root = std::make_unique<P3DChunk>(_data);
}

P3DFile::~P3DFile() = default;

} // namespace Donut::P3D
