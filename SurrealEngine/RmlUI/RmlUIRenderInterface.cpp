
#include "Precomp.h"
#include "RmlUI/RmlUIRenderInterface.h"
#include "RenderDevice/RenderDevice.h"
#include "Utils/Logger.h"
#include <RmlUi/Core.h>
#include <stb_image.h>

RmlUIRenderInterface::RmlUIRenderInterface()
{
}

RmlUIRenderInterface::~RmlUIRenderInterface()
{
}

void RmlUIRenderInterface::SetRenderState(RenderDevice* device, FSceneNode* frame)
{
	currentDevice = device;
	currentFrame = frame;
}

Rml::CompiledGeometryHandle RmlUIRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
{
	auto geom = std::make_unique<RmlGeometry>();
	geom->vertices.assign(vertices.begin(), vertices.end());
	geom->indices.assign(indices.begin(), indices.end());

	uintptr_t id = nextGeometryId++;
	geometries[id] = std::move(geom);
	return id;
}

void RmlUIRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
{
	if (!currentDevice || !currentFrame)
		return;

	auto geomIt = geometries.find(geometry);
	if (geomIt == geometries.end())
		return;

	RmlGeometry* geom = geomIt->second.get();

	// Use white texture for untextured geometry
	if (texture == 0)
		texture = GetWhiteTexture();

	FTextureInfo* texInfo = nullptr;
	auto texIt = textures.find(texture);
	if (texIt != textures.end())
		texInfo = &texIt->second->info;

	// Convert Rml::Vertex to UIVertex
	int numVerts = (int)geom->vertices.size();
	int numIndices = (int)geom->indices.size();

	std::vector<UIVertex> uiVertices(numVerts);
	for (int i = 0; i < numVerts; i++)
	{
		const Rml::Vertex& v = geom->vertices[i];
		UIVertex& uv = uiVertices[i];
		uv.Position = vec2(v.position.x + translation.x, v.position.y + translation.y);
		uv.Color = vec4(
			v.colour.red / 255.0f,
			v.colour.green / 255.0f,
			v.colour.blue / 255.0f,
			v.colour.alpha / 255.0f
		);
		uv.UV = vec2(v.tex_coord.x, v.tex_coord.y);
	}

	// Convert int indices to uint32_t
	std::vector<uint32_t> uiIndices(numIndices);
	for (int i = 0; i < numIndices; i++)
		uiIndices[i] = (uint32_t)geom->indices[i];

	currentDevice->DrawUITriangles(currentFrame, texInfo, uiVertices.data(), numVerts, uiIndices.data(), numIndices);
}

void RmlUIRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
	geometries.erase(geometry);
}

Rml::TextureHandle RmlUIRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source)
{
	Rml::FileInterface* fileInterface = Rml::GetFileInterface();
	if (!fileInterface)
		return 0;

	Rml::FileHandle fileHandle = fileInterface->Open(source);
	if (!fileHandle)
	{
		::LogMessage("RmlUi WARNING: Could not open texture file: " + source);
		return 0;
	}

	// Read entire file into memory
	fileInterface->Seek(fileHandle, 0, SEEK_END);
	size_t fileSize = fileInterface->Tell(fileHandle);
	fileInterface->Seek(fileHandle, 0, SEEK_SET);

	std::vector<uint8_t> fileData(fileSize);
	size_t bytesRead = fileInterface->Read(fileData.data(), fileSize, fileHandle);
	fileInterface->Close(fileHandle);

	if (bytesRead != fileSize)
	{
		::LogMessage("RmlUi ERROR: Failed to read texture file: " + source);
		return 0;
	}

	// Decode with stb_image (force RGBA)
	int w = 0, h = 0, channels = 0;
	stbi_uc* pixels = stbi_load_from_memory(fileData.data(), (int)fileSize, &w, &h, &channels, 4);
	if (!pixels)
	{
		::LogMessage("RmlUi ERROR: Failed to decode texture: " + source + " (" + stbi_failure_reason() + ")");
		return 0;
	}

	texture_dimensions.x = w;
	texture_dimensions.y = h;

	// Delegate to GenerateTexture (handles RGBA→BGRA + FTextureInfo creation)
	Rml::TextureHandle handle = GenerateTexture(
		Rml::Span<const Rml::byte>(pixels, w * h * 4),
		Rml::Vector2i(w, h)
	);

	stbi_image_free(pixels);

	if (handle)
		::LogMessage("RmlUi: Loaded texture: " + source);

	return handle;
}

Rml::TextureHandle RmlUIRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions)
{
	int width = source_dimensions.x;
	int height = source_dimensions.y;

	auto tex = std::make_unique<RmlTexture>();
	tex->mip.Width = width;
	tex->mip.Height = height;
	tex->mip.Data.resize(width * height * 4);

	// RGBA -> BGRA conversion
	const uint8_t* src = source.data();
	uint8_t* dst = tex->mip.Data.data();
	for (int i = 0; i < width * height; i++)
	{
		dst[i * 4 + 0] = src[i * 4 + 2]; // B <- R
		dst[i * 4 + 1] = src[i * 4 + 1]; // G
		dst[i * 4 + 2] = src[i * 4 + 0]; // R <- B
		dst[i * 4 + 3] = src[i * 4 + 3]; // A
	}

	tex->info.CacheID = (uint64_t)(ptrdiff_t)tex.get();
	tex->info.Format = TextureFormat::BGRA8;
	tex->info.USize = width;
	tex->info.VSize = height;
	tex->info.NumMips = 1;
	tex->info.Mips = &tex->mip;
	tex->info.bRealtimeChanged = true;

	uintptr_t handle = (uintptr_t)tex.get();
	textures[handle] = std::move(tex);
	return handle;
}

void RmlUIRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
{
	textures.erase(texture);
}

void RmlUIRenderInterface::EnableScissorRegion(bool enable)
{
	scissorEnabled = enable;
	if (currentDevice && currentFrame)
	{
		if (enable)
			currentDevice->SetUIScissorRegion(currentFrame, true,
				scissorRegion.Left(), scissorRegion.Top(),
				scissorRegion.Width(), scissorRegion.Height());
		else
			currentDevice->SetUIScissorRegion(currentFrame, false, 0, 0, 0, 0);
	}
}

void RmlUIRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
	scissorRegion = region;
	if (scissorEnabled && currentDevice && currentFrame)
	{
		currentDevice->SetUIScissorRegion(currentFrame, true,
			region.Left(), region.Top(),
			region.Width(), region.Height());
	}
}

void RmlUIRenderInterface::ResetScissorState()
{
	if (scissorEnabled && currentDevice && currentFrame)
	{
		currentDevice->SetUIScissorRegion(currentFrame, false, 0, 0, 0, 0);
		scissorEnabled = false;
	}
}

Rml::TextureHandle RmlUIRenderInterface::GetWhiteTexture()
{
	if (whiteTextureHandle != 0)
		return whiteTextureHandle;

	// Create a 1x1 white BGRA texture
	Rml::byte white[] = { 255, 255, 255, 255 };
	whiteTextureHandle = GenerateTexture(Rml::Span<const Rml::byte>(white, 4), Rml::Vector2i(1, 1));
	return whiteTextureHandle;
}
