#pragma once

#include <memory>
#include <string>

namespace Rml { class Context; }

class RmlUISystemInterface;
class RmlUIFileInterface;
class RmlUIRenderInterface;
class RenderDevice;
struct FSceneNode;

class RmlUIManager
{
public:
	RmlUIManager();
	~RmlUIManager();

	bool Initialize(const std::string& uiRootPath, int width, int height);
	void Shutdown();

	void Update();
	void Render(RenderDevice* device, FSceneNode* frame);

	void SetViewportSize(int width, int height);

	bool IsInitialized() const { return initialized; }

private:
	bool initialized = false;

	std::unique_ptr<RmlUISystemInterface> systemInterface;
	std::unique_ptr<RmlUIFileInterface> fileInterface;
	std::unique_ptr<RmlUIRenderInterface> renderInterface;

	Rml::Context* context = nullptr;
};
