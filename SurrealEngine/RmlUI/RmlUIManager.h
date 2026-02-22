#pragma once

#include <memory>
#include <string>
#include <vector>
#include "GameWindow.h"
#include <RmlUi/Core/DataModelHandle.h>

namespace Rml { class Context; class ElementDocument; namespace Input { enum KeyIdentifier : unsigned char; } }

class RmlUISystemInterface;
class RmlUIFileInterface;
class RmlUIRenderInterface;
class RenderDevice;
struct FSceneNode;

struct WeaponSlot
{
	bool occupied = false;
	bool selected = false;
	std::string name;
	int ammo = 0;

	bool operator==(const WeaponSlot& o) const
	{
		return occupied == o.occupied && selected == o.selected && name == o.name && ammo == o.ammo;
	}
	bool operator!=(const WeaponSlot& o) const { return !(*this == o); }
};

struct HUDViewModel
{
	int health = 0;
	int healthMax = 100;
	int armor = 0;
	int ammo = 0;
	std::string weaponName;
	std::string playerName;
	float score = 0;
	float deaths = 0;
	bool hasWeapon = false;
	int fragCount = 0;
	int crosshairIndex = 0;
	int hudMode = 0;
	std::vector<WeaponSlot> weaponSlots;
};

struct MessageEntry
{
	std::string text;
	std::string type;   // "Say", "TeamSay", "Console", "CriticalEvent", etc.
	std::string color;  // CSS color string for data-style-color
	float timeRemaining = 0.0f;

	bool operator==(const MessageEntry& o) const
	{
		return text == o.text && type == o.type && color == o.color && timeRemaining == o.timeRemaining;
	}
	bool operator!=(const MessageEntry& o) const { return !(*this == o); }
};

struct MessagesViewModel
{
	std::vector<MessageEntry> messages;  // up to 4 visible
	bool isTyping = false;
	std::string typedString;
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

	// Document management
	void ShowDocument(const std::string& name);
	void HideDocument(const std::string& name);
	void ToggleDocument(const std::string& name);
	bool IsDocumentVisible(const std::string& name) const;
	bool HasActiveInteractiveDocument() const;

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
	void UpdateMessagesData(const MessagesViewModel& data);

private:
	Rml::ElementDocument* GetDocument(const std::string& name) const;
	static Rml::Input::KeyIdentifier MapKey(EInputKey key);

	bool initialized = false;
	std::string uiRoot;

	std::unique_ptr<RmlUISystemInterface> systemInterface;
	std::unique_ptr<RmlUIFileInterface> fileInterface;
	std::unique_ptr<RmlUIRenderInterface> renderInterface;

	Rml::Context* context = nullptr;

	// Named documents
	Rml::ElementDocument* hudDocument = nullptr;
	Rml::ElementDocument* messagesDocument = nullptr;
	Rml::ElementDocument* scoreboardDocument = nullptr;
	Rml::ElementDocument* consoleDocument = nullptr;
	Rml::ElementDocument* menuDocument = nullptr;

	// Data model
	HUDViewModel hudViewModel;
	Rml::DataModelHandle hudModelHandle;

	MessagesViewModel messagesViewModel;
	Rml::DataModelHandle messagesModelHandle;
};
