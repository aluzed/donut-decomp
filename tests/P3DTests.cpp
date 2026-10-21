// Copyright 2019-2020 the donut authors. See AUTHORS.md

// The P3D parser, round trip: chunks read, written back, compared byte for
// byte. On chunk trees built here, so that the tests run without the game's
// files; and on every file under art/ when they are there (DONUT_ART names
// another folder).

#include "Test.h"

#include "P3D/P3D.generated.h"
#include "P3D/P3DChunk.h"
#include "P3D/P3DFile.h"

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace Donut;

namespace
{

using Bytes = std::vector<uint8_t>;

void put32(Bytes& out, uint32_t value)
{
	for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

void putName(Bytes& out, const std::string& name)
{
	out.push_back(static_cast<uint8_t>(name.size()));
	out.insert(out.end(), name.begin(), name.end());
}

// A chunk as a file has it: type, header size, total size, data, children.
Bytes chunk(uint32_t type, const Bytes& data, const std::vector<Bytes>& children = {})
{
	Bytes out;
	std::size_t total = 12 + data.size();
	for (const auto& child : children) total += child.size();
	put32(out, type);
	put32(out, static_cast<uint32_t>(12 + data.size()));
	put32(out, static_cast<uint32_t>(total));
	out.insert(out.end(), data.begin(), data.end());
	for (const auto& child : children) out.insert(out.end(), child.begin(), child.end());
	return out;
}

// A FrameController: version, name, type, frame offset, hierarchy, animation.
Bytes frameController(const std::string& name, const char (&type)[5], const std::string& animation)
{
	Bytes data;
	put32(data, 0);
	putName(data, name);
	data.insert(data.end(), type, type + 4);
	put32(data, 0x3f800000); // 1.0f
	putName(data, name + "_hierarchy");
	putName(data, animation);
	return chunk(static_cast<uint32_t>(P3D::ChunkType::FrameController), data);
}

Bytes sampleFile()
{
	const Bytes animation = chunk(static_cast<uint32_t>(P3D::ChunkType::Animation), Bytes(23, 0x11));
	const Bytes nested = chunk(0x00012345, {1, 2, 3}, {chunk(0x00012346, {}), chunk(0x00012347, {4, 5})});
	return chunk(static_cast<uint32_t>(P3D::FileTypes::P3D), {},
	             {frameController("windmill", "PTRN", "windmill_anim"), animation, nested});
}

std::filesystem::path writeTemp(const std::string& name, const Bytes& bytes)
{
	const auto path = std::filesystem::temp_directory_path() / name;
	std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	return path;
}

std::filesystem::path artFolder()
{
	if (const char* art = std::getenv("DONUT_ART"))
		return art;
	return std::filesystem::path(DONUT_SOURCE_DIR) / "art";
}

} // namespace

TEST(ChunkTreeRoundTrip)
{
	const Bytes file = sampleFile();
	const P3D::P3DChunk root(file);
	REQUIRE(root.GetChildren().size() == 3);
	CHECK(root.GetChildren()[2]->GetChildren().size() == 2);
	CHECK(root.GetChildren()[2]->GetData() == Bytes({1, 2, 3}));
	CHECK(root.Serialize() == file);
}

TEST(EmptyChunkRoundTrip)
{
	const Bytes empty = chunk(0x00012346, {});
	CHECK(P3D::P3DChunk(empty).Serialize() == empty);
}

// Bytes after the last chunk, a child cut short: read as far as they go, and
// not past the end (this used to read out of bounds).
TEST(DamagedChunks)
{
	Bytes trailing = sampleFile();
	trailing.insert(trailing.end(), {0, 0, 0, 0});
	const P3D::P3DChunk withTrailing(trailing);
	CHECK(withTrailing.GetChildren().size() == 3);
	CHECK(withTrailing.Serialize() == sampleFile());

	Bytes cut = sampleFile();
	cut.resize(cut.size() - 5);
	const P3D::P3DChunk withCut(cut);
	CHECK(withCut.GetChildren().size() == 2);
}

TEST(GeneratedFrameController)
{
	const Bytes bytes = frameController("windmill", "PTRN", "windmill_anim");
	const P3D::FrameController controller {P3D::P3DChunk(bytes)};
	CHECK(controller.GetName() == "windmill");
	CHECK(controller.GetType() == "PTRN");
	CHECK(controller.GetFrameOffset() == 1.0f);
	CHECK(controller.GetHierarchyName() == "windmill_hierarchy");
	CHECK(controller.GetAnimationName() == "windmill_anim");
}

TEST(FileRoundTrip)
{
	const Bytes file = sampleFile();
	const auto path = writeTemp("donut_test.p3d", file);
	const P3D::P3DFile p3d(path.string());
	std::filesystem::remove(path);
	CHECK(p3d.GetBytes() == file);
	CHECK(p3d.GetRoot().Serialize() == file);
}

// zlib's: the 'RZ' files. None of the game's is one; the branch had kept a
// size twice too big.
TEST(RZFile)
{
	const Bytes file = sampleFile();
	uLongf size = compressBound(static_cast<uLong>(file.size()));
	Bytes packed(size);
	REQUIRE(compress(packed.data(), &size, file.data(), static_cast<uLong>(file.size())) == Z_OK);
	packed.resize(size);
	Bytes bytes;
	put32(bytes, static_cast<uint32_t>(P3D::FileTypes::RZ));
	bytes.insert(bytes.end(), packed.begin(), packed.end());
	const auto path = writeTemp("donut_test_rz.p3d", bytes);
	const P3D::P3DFile p3d(path.string());
	std::filesystem::remove(path);
	CHECK(p3d.GetBytes() == file);
	CHECK(p3d.GetRoot().Serialize() == file);
}

// One block: a run of four literals, then eight bytes copied from four back.
TEST(P3DZBlock)
{
	Bytes in = {'P', '3', 'D', 'Z'};
	put32(in, 12);
	const Bytes block = {4, 'A', 'B', 'C', 'D', 0x48, 0x00};
	put32(in, static_cast<uint32_t>(block.size()));
	put32(in, 12);
	in.insert(in.end(), block.begin(), block.end());
	Bytes out;
	REQUIRE(P3D::DecompressP3DZ(in, out));
	CHECK(std::string(out.begin(), out.end()) == "ABCDABCDABCD");

	// Cut short, or copying from before the start: refused.
	Bytes cut(in.begin(), in.end() - 2);
	CHECK(!P3D::DecompressP3DZ(cut, out));
	Bytes far = in;
	far[far.size() - 1] = 0x10;
	CHECK(!P3D::DecompressP3DZ(far, out));
}

// Every file of the game, read and written back as it was; level 5's mission
// files are P3DZ, their chunks compared with what they decompress to.
TEST(GameFilesRoundTrip)
{
	const auto art = artFolder();
	if (!std::filesystem::exists(art))
		SKIP("no game files at " + art.string());
	int files = 0, compressed = 0, different = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(art))
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".p3d")
			continue;
		uint32_t magic = 0;
		std::ifstream(entry.path(), std::ios::binary).read(reinterpret_cast<char*>(&magic), sizeof(magic));
		if (magic != static_cast<uint32_t>(P3D::FileTypes::P3D) && magic != static_cast<uint32_t>(P3D::FileTypes::P3DZ))
			continue;
		const P3D::P3DFile p3d(entry.path().string());
		++files;
		compressed += magic == static_cast<uint32_t>(P3D::FileTypes::P3DZ);
		if (p3d.GetBytes().empty() || p3d.GetRoot().Serialize() != p3d.GetBytes())
		{
			if (++different <= 5)
				std::fprintf(stderr, "  differs: %s\n", entry.path().string().c_str());
		}
	}
	std::printf("     %d files, %d of them P3DZ, %d different\n", files, compressed, different);
	CHECK(files > 0);
	CHECK(compressed > 0);
	CHECK(different == 0);
}

// Every picture of the game decodes, as its chunk says it is stored: PNG
// (palettised or not) or DDS (DXT1, DXT3, DXT5), to as many pixels as it has.
TEST(GameImagesDecode)
{
	const auto art = artFolder();
	if (!std::filesystem::exists(art))
		SKIP("no game files at " + art.string());
	int images = 0, failed = 0;
	std::map<uint32_t, int> formats;
	const auto visit = [&](const auto& self, const P3D::P3DChunk& chunk) -> void
	{
		if (chunk.IsType(P3D::ChunkType::Image))
		{
			const P3D::Image image(chunk);
			const auto decoded = P3D::ImageDecoder::Decode(image.GetData(), image.GetFormat());
			++images;
			++formats[image.GetFormat()];
			if (decoded.width != static_cast<int>(image.GetWidth()) || decoded.height != static_cast<int>(image.GetHeight()) ||
			    decoded.data.size() != static_cast<std::size_t>(decoded.width) * decoded.height * 4)
				++failed;
		}
		for (const auto& child : chunk.GetChildren()) self(self, *child);
	};
	for (const auto& entry : std::filesystem::recursive_directory_iterator(art))
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".p3d")
			continue;
		const P3D::P3DFile p3d(entry.path().string());
		if (!p3d.GetBytes().empty())
			visit(visit, p3d.GetRoot());
	}
	std::printf("     %d images (", images);
	for (const auto& [format, count] : formats) std::printf(" format %u: %d", format, count);
	std::printf(" ), %d not decoded\n", failed);
	CHECK(images > 0);
	CHECK(failed == 0);
}

// The living room's facial expressions (camset_*.p3d, the only files that have
// any): each mixer names a group, each group's expressions their keys, each
// skin's offsets the vertices they move, in order, read to the last one.
TEST(GameExpressions)
{
	const auto camset = artFolder() / "frontend/scrooby2/resource/pure3d/camset_C.p3d";
	if (!std::filesystem::exists(camset))
		SKIP("no " + camset.string());
	const P3D::P3DFile p3d(camset.string());
	std::set<std::string> groups;
	int mixers = 0, expressions = 0, offsets = 0;
	const auto visit = [&](const auto& self, const P3D::P3DChunk& chunk) -> void
	{
		if (chunk.IsType(P3D::ChunkType::ExpressionGroup))
		{
			const P3D::ExpressionGroup group(chunk);
			groups.insert(group.GetName());
			for (const auto& expression : group.GetExpressions())
			{
				++expressions;
				CHECK(expression->GetKeys().size() == expression->GetNumKeys());
				CHECK(expression->GetIndices().size() == expression->GetNumKeys());
			}
		}
		else if (chunk.IsType(P3D::ChunkType::ExpressionOffsets))
		{
			const P3D::ExpressionOffsets expression(chunk);
			CHECK(expression.GetPrimGroupIndices().size() == expression.GetNumPrimGroups());
			for (const auto& list : expression.GetOffsetLists())
			{
				offsets += static_cast<int>(list->GetNumOffsets());
				CHECK(std::is_sorted(list->GetIndices().begin(), list->GetIndices().end()));
				CHECK(list->GetOffsets().size() == list->GetNumOffsets());
			}
		}
		for (const auto& child : chunk.GetChildren()) self(self, *child);
	};
	visit(visit, p3d.GetRoot());
	for (const auto& chunk : p3d.GetRoot().GetChildren())
		if (chunk->IsType(P3D::ChunkType::ExpressionMixer))
		{
			++mixers;
			CHECK(groups.count(P3D::ExpressionMixer(*chunk).GetExpressionGroupName()) == 1);
		}
	std::printf("     %zu groups, %d expressions, %d mixers, %d vertex offsets\n", groups.size(), expressions, mixers, offsets);
	CHECK(mixers > 0);
	CHECK(offsets > 0);
}
