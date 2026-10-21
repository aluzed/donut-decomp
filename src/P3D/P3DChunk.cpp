// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <P3D/P3D.generated.h>
#include <P3D/P3DChunk.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <ThirdParty/stb_image.h>

#include <cstdint>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>

namespace Donut::P3D
{
namespace
{
// A 5:6:5 colour to 8-bit RGB.
void unpack565(uint16_t c, uint8_t out[3])
{
	out[0] = static_cast<uint8_t>(((c >> 11) & 31) * 255 / 31);
	out[1] = static_cast<uint8_t>(((c >> 5) & 63) * 255 / 63);
	out[2] = static_cast<uint8_t>((c & 31) * 255 / 31);
}

// A DDS image (the frontend's level and mission pictures: DXT1, DXT3, DXT5
// tiles), decoded to RGBA. False when it is none of those.
bool decodeDds(const std::vector<uint8_t>& data, ImageDecoder& out)
{
	if (data.size() < 128 || std::memcmp(data.data(), "DDS ", 4) != 0)
		return false;
	uint32_t height = 0, width = 0;
	std::memcpy(&height, &data[12], 4);
	std::memcpy(&width, &data[16], 4);
	const std::string format(reinterpret_cast<const char*>(&data[84]), 4);
	const bool dxt1 = format == "DXT1", dxt3 = format == "DXT3", dxt5 = format == "DXT5";
	if (!dxt1 && !dxt3 && !dxt5)
		return false;
	const std::size_t blockSize = dxt1 ? 8 : 16;
	const std::size_t blocksX = (width + 3) / 4, blocksY = (height + 3) / 4;
	if (data.size() < 128 + blocksX * blocksY * blockSize)
		return false;
	out.width = static_cast<int>(width);
	out.height = static_cast<int>(height);
	out.comp = 4;
	out.data.assign(static_cast<std::size_t>(width) * height * 4, 0);
	const uint8_t* block = &data[128];
	for (std::size_t by = 0; by < blocksY; ++by)
		for (std::size_t bx = 0; bx < blocksX; ++bx, block += blockSize)
		{
			// The alpha: none (DXT1), 4 bits a pixel (DXT3), or two ends and
			// 3-bit steps between them (DXT5).
			uint8_t alpha[16];
			std::fill(std::begin(alpha), std::end(alpha), 255);
			const uint8_t* colour = block;
			if (dxt3)
			{
				for (int i = 0; i < 16; ++i)
					alpha[i] = static_cast<uint8_t>(((block[i / 2] >> ((i % 2) * 4)) & 15) * 17);
				colour = block + 8;
			}
			else if (dxt5)
			{
				uint8_t ends[8] = {block[0], block[1]};
				if (ends[0] > ends[1])
					for (int i = 1; i < 7; ++i)
						ends[i + 1] = static_cast<uint8_t>(((7 - i) * ends[0] + i * ends[1]) / 7);
				else
				{
					for (int i = 1; i < 5; ++i)
						ends[i + 1] = static_cast<uint8_t>(((5 - i) * ends[0] + i * ends[1]) / 5);
					ends[6] = 0;
					ends[7] = 255;
				}
				uint64_t bits = 0;
				for (int i = 0; i < 6; ++i)
					bits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
				for (int i = 0; i < 16; ++i)
					alpha[i] = ends[(bits >> (3 * i)) & 7];
				colour = block + 8;
			}
			// The colour: two 5:6:5 ends and 2-bit steps between them.
			const uint16_t c0 = static_cast<uint16_t>(colour[0] | (colour[1] << 8));
			const uint16_t c1 = static_cast<uint16_t>(colour[2] | (colour[3] << 8));
			uint8_t palette[4][4];
			unpack565(c0, palette[0]);
			unpack565(c1, palette[1]);
			palette[0][3] = palette[1][3] = palette[2][3] = palette[3][3] = 255;
			for (int k = 0; k < 3; ++k)
			{
				if (!dxt1 || c0 > c1)
				{
					palette[2][k] = static_cast<uint8_t>((2 * palette[0][k] + palette[1][k]) / 3);
					palette[3][k] = static_cast<uint8_t>((palette[0][k] + 2 * palette[1][k]) / 3);
				}
				else
				{
					palette[2][k] = static_cast<uint8_t>((palette[0][k] + palette[1][k]) / 2);
					palette[3][k] = 0;
				}
			}
			if (dxt1 && c0 <= c1)
				palette[3][3] = 0;
			const uint32_t indices = colour[4] | (colour[5] << 8) | (colour[6] << 16) | (static_cast<uint32_t>(colour[7]) << 24);
			for (int i = 0; i < 16; ++i)
			{
				const std::size_t x = bx * 4 + i % 4, y = by * 4 + i / 4;
				if (x >= width || y >= height)
					continue;
				const uint8_t* c = palette[(indices >> (2 * i)) & 3];
				// Stored bottom row first, as the game's textures are upside
				// down in DDS: turned the right way up, as a PNG's rows are.
				uint8_t* pixel = &out.data[((height - 1 - y) * width + x) * 4];
				pixel[0] = c[0];
				pixel[1] = c[1];
				pixel[2] = c[2];
				pixel[3] = static_cast<uint8_t>(c[3] * alpha[i] / 255);
			}
		}
	return true;
}
} // namespace

// PNG, or a DDS of DXT1, DXT3 or DXT5 blocks.
ImageDecoder ImageDecoder::Decode(const std::vector<uint8_t>& data)
{
	ImageDecoder ret;
	if (decodeDds(data, ret))
		return ret;
	// Always as RGBA: asked for "as stored", a palette image's transparency
	// (its tRNS chunk: the smoke puffs' corners) came back as three opaque
	// channels, and every puff was drawn as a dark square.
	int stored = 0;
	uint8_t* image = stbi_load_from_memory(data.data(), (std::int32_t)data.size(), &ret.width, &ret.height, &stored, 4);
	ret.comp = 4;
	// Neither: nothing, rather than a vector made from a null pointer and a
	// size of garbage (the game died on the first DDS tile it met).
	if (!image)
	{
		ret.width = ret.height = 0;
		return ret;
	}
	ret.data = std::vector<uint8_t>(image, image + (ret.width * ret.height * ret.comp));
	stbi_image_free(image);

	return ret;
}

void P3DUtil::GetDrawables(const std::unique_ptr<InstanceList>& instanceList, std::vector<SceneGraphDrawable*>& drawables,
                           std::vector<Matrix4x4>& transforms)
{
	if (!instanceList)
		return;

	const auto& sceneGraph = instanceList->GetSceneGraph();
	if (!sceneGraph)
		return;
	const auto& sceneGraphRoot = sceneGraph->GetRoot();
	if (!sceneGraphRoot)
		return;
	const auto& sceneGraphBranch = sceneGraphRoot->GetBranch();
	if (!sceneGraphBranch)
		return;

	for (const auto& child : sceneGraphBranch->GetChildren())
	{ GetDrawables(child, drawables, transforms, Matrix4x4::Identity); }
}

void P3DUtil::GetDrawables(const std::unique_ptr<SceneGraphTransform>& transform, std::vector<SceneGraphDrawable*>& drawables,
                           std::vector<Matrix4x4>& transforms, const Matrix4x4& parentTransform)
{
	if (!transform)
		return;

	const auto worldTransform = parentTransform * transform->GetTransform();

	for (const auto& drawable : transform->GetDrawables())
	{
		drawables.push_back(drawable.get());
		transforms.push_back(worldTransform);
	}

	for (const auto& child : transform->GetChildren()) { GetDrawables(child, drawables, transforms, worldTransform); }
}

P3DChunk::P3DChunk(const std::vector<uint8_t>& chunk)
{
	// minimum size of a chunk
	assert(chunk.size() >= 12);

	_type = *reinterpret_cast<const ChunkType*>(&chunk[0]);

	const uint32_t dataSize = *reinterpret_cast<const uint32_t*>(&chunk[4]);
	const uint32_t totalSize = *reinterpret_cast<const uint32_t*>(&chunk[8]);

	// define our data and ignore the first 12 bytes
	_data = std::vector<uint8_t>(chunk.begin() + 12, chunk.begin() + dataSize);

	auto it = std::next(chunk.begin(), dataSize);
	while (it < chunk.end())
	{
		// cheat a little and get the chunks size so we can advance our stream
		const uint32_t tSize = *reinterpret_cast<const uint32_t*>(&it[8]);

		const auto cChunkData = std::vector<uint8_t>(it, std::next(it, tSize));
		_children.push_back(std::make_unique<P3DChunk>(cChunkData));

		std::advance(it, tSize);
	}
}

std::ostream& operator<<(std::ostream& os, ChunkType chunktype)
{
	switch (chunktype)
	{
	case ChunkType::Camera: return os << "Camera";
	case ChunkType::LightGroup: return os << "LightGroup";
	case ChunkType::Skeleton: return os << "Skeleton";
	case ChunkType::SkeletonJoint: return os << "SkeletonJoint";
	case ChunkType::SkeletonJointMirrorMap: return os << "SkeletonJointMirrorMap";
	case ChunkType::SkeletonJointBonePreserve: return os << "SkeletonJointBonePreserve";
	case ChunkType::CompositeDrawable: return os << "CompositeDrawable";
	case ChunkType::CompositeDrawableSkinList: return os << "CompositeDrawableSkinList";
	case ChunkType::CompositeDrawablePropList: return os << "CompositeDrawablePropList";
	case ChunkType::CompositeDrawableSkin: return os << "CompositeDrawableSkin";
	case ChunkType::CompositeDrawableProp: return os << "CompositeDrawableProp";
	case ChunkType::CompositeDrawableEffectList: return os << "CompositeDrawableEffectList";
	case ChunkType::CompositeDrawableEffect: return os << "CompositeDrawableEffect";
	case ChunkType::CompositeDrawableSortOrder: return os << "CompositeDrawableSortOrder";
	case ChunkType::MultiController: return os << "MultiController";
	case ChunkType::MultiControllerTracks: return os << "MultiControllerTracks";
	case ChunkType::History: return os << "History";
	case ChunkType::ExportInfo: return os << "ExportInfo";
	case ChunkType::ExportInfoNamedString: return os << "ExportInfoNamedString";
	case ChunkType::ExportInfoNamedInteger: return os << "ExportInfoNamedInteger";
	case ChunkType::Geometry: return os << "Geometry";
	case ChunkType::PolySkin: return os << "PolySkin";
	case ChunkType::PrimitiveGroup: return os << "PrimitiveGroup";
	case ChunkType::BoundingBox: return os << "BoundingBox";
	case ChunkType::BoundingSphere: return os << "BoundingSphere";
	case ChunkType::PositionList: return os << "PositionList";
	case ChunkType::NormalList: return os << "NormalList";
	case ChunkType::UVList: return os << "UVList";
	case ChunkType::ColorList: return os << "ColorList";
	case ChunkType::IndexList: return os << "IndexList";
	case ChunkType::MatrixList: return os << "MatrixList";
	case ChunkType::WeightList: return os << "WeightList";
	case ChunkType::MatrixPalette: return os << "MatrixPalette";
	case ChunkType::OffsetList: return os << "OffsetList";
	case ChunkType::PackedNormalList: return os << "PackedNormalList";
	case ChunkType::VertexShader: return os << "VertexShader";
	case ChunkType::DrawShadow: return os << "DrawShadow";
	case ChunkType::ExpressionOffsets: return os << "ExpressionOffsets";
	case ChunkType::Shader: return os << "Shader";
	case ChunkType::ShaderTextureParam: return os << "ShaderTextureParam";
	case ChunkType::ShaderIntParam: return os << "ShaderIntParam";
	case ChunkType::ShaderFloatParam: return os << "ShaderFloatParam";
	case ChunkType::ShaderColorParam: return os << "ShaderColorParam";
	case ChunkType::GameAttr: return os << "GameAttr";
	case ChunkType::GameAttrIntParam: return os << "GameAttrIntParam";
	case ChunkType::Light: return os << "Light";
	case ChunkType::LightDirection: return os << "LightDirection";
	case ChunkType::LightPosition: return os << "LightPosition";
	case ChunkType::LightShadow: return os << "LightShadow";
	case ChunkType::Todo0: return os << "Todo0";
	case ChunkType::Locator: return os << "Locator";
	case ChunkType::ParticleSystemFactory: return os << "ParticleSystemFactory";
	case ChunkType::ParticleSystem: return os << "ParticleSystem";
	case ChunkType::BaseEmitter: return os << "BaseEmitter";
	case ChunkType::SpriteEmitter: return os << "SpriteEmitter";
	case ChunkType::ParticleAnimation: return os << "ParticleAnimation";
	case ChunkType::EmitterAnimation: return os << "EmitterAnimation";
	case ChunkType::GeneratorAnimation: return os << "GeneratorAnimation";
	case ChunkType::ParticleInstancingInfo: return os << "ParticleInstancingInfo";
	case ChunkType::BillboardQuad: return os << "BillboardQuad";
	case ChunkType::BillboardQuadGroup: return os << "BillboardQuadGroup";
	case ChunkType::BillboardDisplayInfo: return os << "BillboardDisplayInfo";
	case ChunkType::BillboardPerspectiveInfo: return os << "BillboardPerspectiveInfo";
	case ChunkType::FrontendProject: return os << "FrontendProject";
	case ChunkType::FrontendScreen: return os << "FrontendScreen";
	case ChunkType::FrontendPage: return os << "FrontendPage";
	case ChunkType::FrontendLayer: return os << "FrontendLayer";
	case ChunkType::FrontendGroup: return os << "FrontendGroup";
	case ChunkType::FrontendMultiSprite: return os << "FrontendMultiSprite";
	case ChunkType::FrontendMultiText: return os << "FrontendMultiText";
	case ChunkType::FrontendObject: return os << "FrontendObject";
	case ChunkType::FrontendPolygon: return os << "FrontendPolygon";
	case ChunkType::FrontendStringTextBible: return os << "FrontendStringTextBible";
	case ChunkType::FrontendStringHardCoded: return os << "FrontendStringHardCoded";
	case ChunkType::FrontendTextBible: return os << "FrontendTextBible";
	case ChunkType::FrontendLanguage: return os << "FrontendLanguage";
	case ChunkType::FrontendImageResource: return os << "FrontendImageResource";
	case ChunkType::FrontendObjectResource: return os << "FrontendObjectResource";
	case ChunkType::FrontendTextStyleResource: return os << "FrontendTextStyleResource";
	case ChunkType::FrontendTextBibleResource: return os << "FrontendTextBibleResource";
	case ChunkType::Texture: return os << "Texture";
	case ChunkType::Image: return os << "Image";
	case ChunkType::ImageData: return os << "ImageData";
	case ChunkType::Sprite: return os << "Sprite";
	case ChunkType::AnimatedObjectFactory: return os << "AnimatedObjectFactory";
	case ChunkType::AnimatedObject: return os << "AnimatedObject";
	case ChunkType::AnimatedObjectAnimation: return os << "AnimatedObjectAnimation";
	case ChunkType::Expression: return os << "Expression";
	case ChunkType::ExpressionGroup: return os << "ExpressionGroup";
	case ChunkType::ExpressionMixer: return os << "ExpressionMixer";
	case ChunkType::TextureFont: return os << "TextureFont";
	case ChunkType::FontGlyphs: return os << "FontGlyphs";
	case ChunkType::SceneGraph: return os << "SceneGraph";
	case ChunkType::SceneGraphRoot: return os << "SceneGraphRoot";
	case ChunkType::SceneGraphBranch: return os << "SceneGraphBranch";
	case ChunkType::SceneGraphTransform: return os << "SceneGraphTransform";
	case ChunkType::SceneGraphVisibility: return os << "SceneGraphVisibility";
	case ChunkType::SceneGraphDrawable: return os << "SceneGraphDrawable";
	case ChunkType::SceneGraphLightGroup: return os << "SceneGraphLightGroup";
	case ChunkType::SceneGraphSortOrder: return os << "SceneGraphSortOrder";
	case ChunkType::Animation: return os << "Animation";
	case ChunkType::AnimationGroup: return os << "AnimationGroup";
	case ChunkType::AnimationGroupList: return os << "AnimationGroupList";
	case ChunkType::AnimationSize: return os << "AnimationSize";
	case ChunkType::AnimationHeader: return os << "AnimationHeader";
	case ChunkType::Float1Channel: return os << "Float1Channel";
	case ChunkType::Float2Channel: return os << "Float2Channel";
	case ChunkType::Vector1Channel: return os << "Vector1Channel";
	case ChunkType::Vector2Channel: return os << "Vector2Channel";
	case ChunkType::Vector3Channel: return os << "Vector3Channel";
	case ChunkType::QuaternionChannel: return os << "QuaternionChannel";
	case ChunkType::EntityChannel: return os << "EntityChannel";
	case ChunkType::BoolChannel: return os << "BoolChannel";
	case ChunkType::Color: return os << "Color";
	case ChunkType::IntChannel: return os << "IntChannel";
	case ChunkType::ChannelInterpolationMode: return os << "ChannelInterpolationMode";
	case ChunkType::CompressedQuaternionChannel: return os << "CompressedQuaternionChannel";
	case ChunkType::FrameController: return os << "FrameController";
	case ChunkType::FrameController2: return os << "FrameController2";
	case ChunkType::MultiController2: return os << "MultiController2";
	case ChunkType::MultiController2Track: return os << "MultiController2Track";
	case ChunkType::VectorOffsetList: return os << "VectorOffsetList";
	case ChunkType::VertexAnimKeyFrame: return os << "VertexAnimKeyFrame";
	case ChunkType::Fence: return os << "Fence";
	case ChunkType::RoadSegment: return os << "RoadSegment";
	case ChunkType::Road: return os << "Road";
	case ChunkType::Intersection: return os << "Intersection";
	case ChunkType::Locator2: return os << "Locator2";
	case ChunkType::TriggerVolume: return os << "TriggerVolume";
	case ChunkType::Spline: return os << "Spline";
	case ChunkType::InstanceList: return os << "InstanceList";
	case ChunkType::RoadDataSegment: return os << "RoadDataSegment";
	case ChunkType::Todo2: return os << "Todo2";
	case ChunkType::Path: return os << "Path";
	case ChunkType::LocatorMatrix: return os << "LocatorMatrix";
	case ChunkType::SurfaceTypeList: return os << "SurfaceTypeList";
	case ChunkType::FollowCameraData: return os << "FollowCameraData";
	case ChunkType::Set: return os << "Set";
	case ChunkType::CollisionEffect: return os << "CollisionEffect";
	case ChunkType::Atc: return os << "Atc";
	case ChunkType::BreakableObject: return os << "BreakableObject";
	case ChunkType::InstParticleSystem: return os << "InstParticleSystem";
	case ChunkType::StaticEntity: return os << "StaticEntity";
	case ChunkType::StaticPhysics: return os << "StaticPhysics";
	case ChunkType::DynamicPhysics: return os << "DynamicPhysics";
	case ChunkType::Intersect: return os << "Intersect";
	case ChunkType::Tree: return os << "Tree";
	case ChunkType::TreeNode: return os << "TreeNode";
	case ChunkType::TreeNode2: return os << "TreeNode2";
	case ChunkType::FenceWrapper: return os << "FenceWrapper";
	case ChunkType::AnimCollision: return os << "AnimCollision";
	case ChunkType::InstancedStaticEntity: return os << "InstancedStaticEntity";
	case ChunkType::InstancedStaticPhysics: return os << "InstancedStaticPhysics";
	case ChunkType::WorldSphere: return os << "WorldSphere";
	case ChunkType::Anim: return os << "Anim";
	case ChunkType::LensFlare: return os << "LensFlare";
	case ChunkType::AnimDynamicPhysics: return os << "AnimDynamicPhysics";
	case ChunkType::AnimDynamicPhysicsWrapper: return os << "AnimDynamicPhysicsWrapper";
	case ChunkType::AnimObjectWrapper: return os << "AnimObjectWrapper";
	case ChunkType::CollisionObject: return os << "CollisionObject";
	case ChunkType::CollisionVolume: return os << "CollisionVolume";
	case ChunkType::CollisionSphere: return os << "CollisionSphere";
	case ChunkType::CollisionCylinder: return os << "CollisionCylinder";
	case ChunkType::CollisionOBBoxVolume: return os << "CollisionOBBoxVolume";
	case ChunkType::CollisionWallVolume: return os << "CollisionWallVolume";
	case ChunkType::CollisionBBoxVolume: return os << "CollisionBBoxVolume";
	case ChunkType::CollisionVector: return os << "CollisionVector";
	case ChunkType::CollisionVolumeOwner: return os << "CollisionVolumeOwner";
	case ChunkType::CollisionVolumeOwnerName: return os << "CollisionVolumeOwnerName";
	case ChunkType::CollisionObjectAttribute: return os << "CollisionObjectAttribute";
	case ChunkType::PhysicsObject: return os << "PhysicsObject";
	case ChunkType::PhysicsInertiaMatrix: return os << "PhysicsInertiaMatrix";
	case ChunkType::PhysicsVector: return os << "PhysicsVector";
	case ChunkType::PhysicsJoint: return os << "PhysicsJoint";
	case ChunkType::StaticPropData: return os << "StaticPropData";
	case ChunkType::StaticPropStateData: return os << "StaticPropStateData";
	case ChunkType::StaticPropVisibilityData: return os << "StaticPropVisibilityData";
	case ChunkType::StaticPropFrameControllerData: return os << "StaticPropFrameControllerData";
	case ChunkType::StaticPropEventData: return os << "StaticPropEventData";
	case ChunkType::StaticPropCallbackData: return os << "StaticPropCallbackData";
	case ChunkType::P3DZRoot: return os << "P3DZRoot";
	case ChunkType::P3DRoot: return os << "P3DRoot";
	};
	return os << "Unknown " << std::hex << std::showbase << static_cast<uint32_t>(chunktype) << std::dec << std::noshowbase;
}

} // namespace Donut::P3D
