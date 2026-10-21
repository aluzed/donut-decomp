// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Core/File.h>
#include <Core/Log.h>
#include <P3D/P3DChunk.h>
#include <P3D/P3DFile.h>

#include <zlib.h>

#include <cstdint>

namespace Donut::P3D
{

// Radical's own compression, the 'P3DZ' files (all of level 5's mission art).
// After the magic: the uncompressed size, then blocks of [compressed size]
// [uncompressed size][data], each data a byte-oriented LZ. A code byte under 16
// starts a run of literals, of that length; above, a copy from earlier output:
// its length the low four bits, its distance the high four plus the next byte
// shifted up four (a 4 KB window, the block size). A length of 0 means 15 plus
// extension bytes, each 0 adding 255 more, the first non-zero one ending it.
bool DecompressP3DZ(const std::vector<uint8_t>& in, std::vector<uint8_t>& out)
{
	const auto u32 = [&](std::size_t at) {
		return static_cast<uint32_t>(in[at]) | static_cast<uint32_t>(in[at + 1]) << 8 |
		       static_cast<uint32_t>(in[at + 2]) << 16 | static_cast<uint32_t>(in[at + 3]) << 24;
	};
	if (in.size() < 8)
		return false;
	const uint32_t total = u32(4);
	out.clear();
	out.reserve(total);

	std::size_t pos = 8;
	while (out.size() < total && pos + 8 <= in.size())
	{
		const uint32_t compressed = u32(pos);
		const uint32_t uncompressed = u32(pos + 4);
		pos += 8;
		const std::size_t blockEnd = pos + compressed;
		const std::size_t outEnd = out.size() + uncompressed;
		if (blockEnd > in.size())
			return false;

		const auto extended = [&](std::size_t& at) -> std::size_t {
			std::size_t length = 15;
			while (at < blockEnd && in[at] == 0)
			{
				length += 255;
				++at;
			}
			return at < blockEnd ? length + in[at++] : 0;
		};

		while (out.size() < outEnd && pos < blockEnd)
		{
			const uint8_t code = in[pos++];
			std::size_t length = code > 15 ? code & 15 : code;
			if (length == 0)
				length = extended(pos);
			if (code > 15)
			{
				if (pos >= blockEnd)
					return false;
				const std::size_t distance = (code >> 4) | (static_cast<std::size_t>(in[pos++]) << 4);
				if (distance == 0 || distance > out.size())
					return false;
				for (std::size_t i = 0; i < length; ++i)
					out.push_back(out[out.size() - distance]);
			}
			else
			{
				if (pos + length > blockEnd)
					return false;
				out.insert(out.end(), in.begin() + pos, in.begin() + pos + length);
				pos += length;
			}
		}
		if (out.size() != outEnd || pos != blockEnd)
			return false;
	}
	return out.size() == total;
}

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

		// Twice as much room each time it does not fit. The size it came to is
		// what uncompress leaves in uncompressedSize, not the room given.
		uLongf room = compressedSize * 4;
		uLongf uncompressedSize = 0;
		std::vector<uint8_t> uncompressed;
		int ret;
		do
		{
			uncompressed.resize(room);
			uncompressedSize = room;
			ret = uncompress(uncompressed.data(), &uncompressedSize, compressed.data(), static_cast<uLong>(compressedSize));
			room *= 2;
		} while (ret == Z_BUF_ERROR);

		if (ret != Z_OK)
		{
			file.Close();
			return;
		}

		uncompressed.resize(uncompressedSize);
		_data = std::move(uncompressed);
		_root = std::make_unique<P3DChunk>(_data);
		file.Close();
		return;
	}

	if (type == static_cast<uint32_t>(FileTypes::P3DZ))
	{
		file.Seek(0, FileSeekMode::Begin);
		std::vector<uint8_t> packed(file.Size());
		file.ReadBytes(packed.data(), packed.size());
		file.Close();
		if (!DecompressP3DZ(packed, _data))
		{
			Log::Error("P3DFile: '{}' does not decompress", path);
			_data.clear();
			return;
		}
		_root = std::make_unique<P3DChunk>(_data);
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
