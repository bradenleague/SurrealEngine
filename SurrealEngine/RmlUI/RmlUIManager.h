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

struct PlayerEntry
{
	std::string name;
	int score = 0;
	int deaths = 0;
	int ping = 0;
	int team = 255;  // 255 = no team
	bool isBot = false;

	bool operator==(const PlayerEntry& o) const
	{
		return name == o.name && score == o.score && deaths == o.deaths
			&& ping == o.ping && team == o.team && isBot == o.isBot;
	}
	bool operator!=(const PlayerEntry& o) const { return !(*this == o); }
};

struct ScoreboardViewModel
{
	std::vector<PlayerEntry> players;
	std::string mapName;
	std::string gameName;
	bool visible = false;
};

struct ConsoleViewModel
{
	std::vector<std::string> logLines;  // ring buffer contents, most recent first
	std::string typedStr;
	bool visible = false;
};

struct SaveSlotEntry
{
	int index = 0;
	std::string description;
	bool hasData = false;

	bool operator==(const SaveSlotEntry& o) const
	{
		return index == o.index && description == o.description && hasData == o.hasData;
	}
	bool operator!=(const SaveSlotEntry& o) const { return !(*this == o); }
};

struct MenuViewModel
{
	bool visible = false;

	// Screen navigation (only one true at a time)
	bool showMain = true;
	bool showGame = false;
	bool showBotmatch = false;
	bool showNewGame = false;
	bool showOptions = false;
	bool showAudioVideo = false;
	bool showSave = false;
	bool showLoad = false;
	bool showQuit = false;

	// BotMatch config
	std::string botmatchMap;
	int botmatchMapIndex = 0;
	int botCount = 4;               // 1-15
	int botSkill = 1;               // 0-3 (Easy/Medium/Hard/Unreal)
	std::string skillLabel = "Medium";
	std::vector<std::string> availableMaps;

	// New Game
	int difficulty = 1;             // 0-3
	std::string difficultyLabel = "Medium";

	// Options (populated by Engine::ReadMenuSettings)
	float mouseSensitivity = 3.0f;
	int fov = 90;
	int crosshair = 0;             // 0-6
	float weaponHand = 1.0f;       // UE1 native: -1=left, 0=center, 1=right
	std::string weaponHandLabel = "Right";
	bool invertMouse = false;
	bool alwaysMouseLook = true;

	// Audio/Video
	int musicVolume = 128;    // 0-255 (UE1 range)
	int soundVolume = 128;    // 0-255
	int brightness = 5;       // 1-10

	std::vector<SaveSlotEntry> saveSlots;
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
	bool HasAllDocuments() const;

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
	void UpdateScoreboardData(const ScoreboardViewModel& data);
	void UpdateConsoleData(const ConsoleViewModel& data);
	void UpdateMenuData(const MenuViewModel& data);
	void HandleMenuAction(const std::string& action);
	bool IsMenuOnSubScreen() const;

	MenuViewModel& GetMenuViewModel() { return menuViewModel; }
	const MenuViewModel& GetMenuViewModel() const { return menuViewModel; }
	void DirtyAllMenuSettings();

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

	ScoreboardViewModel scoreboardViewModel;
	Rml::DataModelHandle scoreboardModelHandle;

	ConsoleViewModel consoleViewModel;
	Rml::DataModelHandle consoleModelHandle;

	MenuViewModel menuViewModel;
	Rml::DataModelHandle menuModelHandle;

	void SetupHUDModel();
	void SetupMessagesModel();
	void SetupScoreboardModel();
	void SetupConsoleModel();
	void SetupMenuModel();

	void SetMenuScreen(const std::string& screen);
	void ClampAndDirty(int& field, int delta, int lo, int hi, const char* varName);
	void PopulateSaveSlots();
	void PopulateAvailableMaps();
};
