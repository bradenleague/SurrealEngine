#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include "RenderDevice/RenderDevice.h"
#include <map>
#include <memory>
#include <vector>

struct RmlTexture
{
	FTextureInfo info;
	UnrealMipmap mip;
};

struct RmlGeometry
{
	std::vector<Rml::Vertex> vertices;
	std::vector<int> indices;
};

class RmlUIRenderInterface : public Rml::RenderInterface
{
public:
	RmlUIRenderInterface();
	~RmlUIRenderInterface() override;

	void SetRenderState(RenderDevice* device, FSceneNode* frame);

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

	Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
	void ReleaseTexture(Rml::TextureHandle texture) override;

	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(Rml::Rectanglei region) override;

private:
	Rml::TextureHandle GetWhiteTexture();

	RenderDevice* currentDevice = nullptr;
	FSceneNode* currentFrame = nullptr;

	std::map<uintptr_t, std::unique_ptr<RmlTexture>> textures;
	std::map<uintptr_t, std::unique_ptr<RmlGeometry>> geometries;

	uintptr_t nextGeometryId = 1;

	Rml::TextureHandle whiteTextureHandle = 0;

	bool scissorEnabled = false;
	Rml::Rectanglei scissorRegion;
};
