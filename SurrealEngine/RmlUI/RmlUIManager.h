#pragma once

#include <memory>
#include <string>
#include "GameWindow.h"
#include <RmlUi/Core/DataModelHandle.h>

namespace Rml { class Context; namespace Input { enum KeyIdentifier : unsigned char; } }

class RmlUISystemInterface;
class RmlUIFileInterface;
class RmlUIRenderInterface;
class RenderDevice;
struct FSceneNode;

struct HUDViewModel
{
	int health = 0;
	int armor = 0;
	int ammo = 0;
	std::string weaponName;
	std::string playerName;
	float score = 0;
	float deaths = 0;
	bool hasWeapon = false;
};

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

	// Input routing
	bool ProcessMouseMove(int x, int y, int keyModifiers);
	bool ProcessMouseButtonDown(int buttonIndex, int keyModifiers);
	bool ProcessMouseButtonUp(int buttonIndex, int keyModifiers);
	bool ProcessMouseWheel(float delta, int keyModifiers);
	bool ProcessKeyDown(EInputKey key, int keyModifiers);
	bool ProcessKeyUp(EInputKey key, int keyModifiers);
	bool ProcessTextInput(const std::string& text);
	bool ProcessMouseLeave();
	bool IsCapturingMouse() const;

	static int GetKeyModifierState();
	static int MapMouseButton(EInputKey key);

	// Data model
	void SetupDataModel();
	void UpdateHUDData(const HUDViewModel& data);

private:
	static Rml::Input::KeyIdentifier MapKey(EInputKey key);

	bool initialized = false;

	std::unique_ptr<RmlUISystemInterface> systemInterface;
	std::unique_ptr<RmlUIFileInterface> fileInterface;
	std::unique_ptr<RmlUIRenderInterface> renderInterface;

	Rml::Context* context = nullptr;

	// Data model
	HUDViewModel hudViewModel;
	Rml::DataModelHandle hudModelHandle;
};
