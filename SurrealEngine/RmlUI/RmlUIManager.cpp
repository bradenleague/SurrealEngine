
#include "Precomp.h"
#include "RmlUI/RmlUIManager.h"
#include "RmlUI/RmlUISystemInterface.h"
#include "RmlUI/RmlUIFileInterface.h"
#include "RmlUI/RmlUIRenderInterface.h"
#include "Utils/Logger.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Input.h>
#include <filesystem>

RmlUIManager::RmlUIManager()
{
}

RmlUIManager::~RmlUIManager()
{
	if (initialized)
		Shutdown();
}

bool RmlUIManager::Initialize(const std::string& uiRootPath, int width, int height)
{
	if (!std::filesystem::exists(uiRootPath))
	{
		::LogMessage("RmlUi: UI directory not found, skipping initialization: " + uiRootPath);
		return false;
	}

	::LogMessage("RmlUi: Initializing with UI root: " + uiRootPath);
	uiRoot = uiRootPath;

	systemInterface = std::make_unique<RmlUISystemInterface>();
	fileInterface = std::make_unique<RmlUIFileInterface>(uiRootPath);
	renderInterface = std::make_unique<RmlUIRenderInterface>();

	Rml::SetSystemInterface(systemInterface.get());
	Rml::SetFileInterface(fileInterface.get());
	Rml::SetRenderInterface(renderInterface.get());

	if (!Rml::Initialise())
	{
		::LogMessage("RmlUi ERROR: Failed to initialize RmlUi");
		systemInterface.reset();
		fileInterface.reset();
		renderInterface.reset();
		return false;
	}

	// Load fonts from UI/fonts/
	std::string fontsPath = uiRootPath + "/fonts";
	int fontsLoaded = 0;
	if (std::filesystem::exists(fontsPath))
	{
		for (const auto& entry : std::filesystem::directory_iterator(fontsPath))
		{
			if (!entry.is_regular_file())
				continue;

			std::string ext = entry.path().extension().string();
			for (char& c : ext) c = std::tolower(c);

			if (ext == ".ttf" || ext == ".otf")
			{
				std::string fontRelPath = "fonts/" + entry.path().filename().string();
				if (Rml::LoadFontFace(fontRelPath))
				{
					::LogMessage("RmlUi: Loaded font: " + entry.path().filename().string());
					fontsLoaded++;
				}
				else
				{
					::LogMessage("RmlUi WARNING: Failed to load font: " + fontRelPath);
				}
			}
		}
	}

	if (fontsLoaded == 0)
	{
		::LogMessage("RmlUi WARNING: No fonts found in UI/fonts/ — text rendering will not work");
	}

	// Create context
	context = Rml::CreateContext("main", Rml::Vector2i(width, height));
	if (!context)
	{
		::LogMessage("RmlUi ERROR: Failed to create context");
		Rml::Shutdown();
		systemInterface.reset();
		fileInterface.reset();
		renderInterface.reset();
		return false;
	}

	// Set up data model before loading documents
	SetupDataModel();

	// Load named documents
	struct { const char* filename; Rml::ElementDocument** target; bool showOnLoad; } docs[] = {
		{ "hud.rml",        &hudDocument,        true  },
		{ "messages.rml",   &messagesDocument,   true  },
		{ "scoreboard.rml", &scoreboardDocument, false },
		{ "console.rml",    &consoleDocument,    false },
		{ "menu.rml",       &menuDocument,       false },
	};

	for (auto& doc : docs)
	{
		std::string fullPath = uiRootPath + "/" + doc.filename;
		if (std::filesystem::exists(fullPath))
		{
			*doc.target = context->LoadDocument(doc.filename);
			if (*doc.target)
			{
				if (doc.showOnLoad)
					(*doc.target)->Show();
				::LogMessage("RmlUi: Loaded document: " + std::string(doc.filename));
			}
			else
			{
				::LogMessage("RmlUi WARNING: Failed to load " + std::string(doc.filename));
			}
		}
	}

	initialized = true;
	::LogMessage("RmlUi: Initialized successfully");
	return true;
}

void RmlUIManager::Shutdown()
{
	if (!initialized)
		return;

	hudModelHandle = {};
	messagesModelHandle = {};
	scoreboardModelHandle = {};
	consoleModelHandle = {};
	menuModelHandle = {};
	hudDocument = nullptr;
	messagesDocument = nullptr;
	scoreboardDocument = nullptr;
	consoleDocument = nullptr;
	menuDocument = nullptr;
	context = nullptr;
	Rml::Shutdown();

	systemInterface.reset();
	fileInterface.reset();
	renderInterface.reset();

	initialized = false;
	::LogMessage("RmlUi: Shut down");
}

// --- Document Management ---

Rml::ElementDocument* RmlUIManager::GetDocument(const std::string& name) const
{
	if (name == "hud") return hudDocument;
	if (name == "messages") return messagesDocument;
	if (name == "scoreboard") return scoreboardDocument;
	if (name == "console") return consoleDocument;
	if (name == "menu") return menuDocument;
	return nullptr;
}

void RmlUIManager::ShowDocument(const std::string& name)
{
	Rml::ElementDocument* doc = GetDocument(name);
	if (doc)
		doc->Show();
}

void RmlUIManager::HideDocument(const std::string& name)
{
	Rml::ElementDocument* doc = GetDocument(name);
	if (doc)
		doc->Hide();
}

void RmlUIManager::ToggleDocument(const std::string& name)
{
	Rml::ElementDocument* doc = GetDocument(name);
	if (!doc)
		return;

	if (doc->IsVisible())
		doc->Hide();
	else
		doc->Show();
}

bool RmlUIManager::IsDocumentVisible(const std::string& name) const
{
	Rml::ElementDocument* doc = GetDocument(name);
	return doc && doc->IsVisible();
}

bool RmlUIManager::HasActiveInteractiveDocument() const
{
	if (!initialized)
		return false;

	// Interactive documents are those that need mouse input: menu, console, scoreboard
	if (menuDocument && menuDocument->IsVisible())
		return true;
	if (consoleDocument && consoleDocument->IsVisible())
		return true;
	if (scoreboardDocument && scoreboardDocument->IsVisible())
		return true;
	return false;
}

bool RmlUIManager::HasAllDocuments() const
{
	if (!initialized)
		return false;

	return hudDocument && messagesDocument && scoreboardDocument
		&& consoleDocument && menuDocument;
}

void RmlUIManager::Update()
{
	if (!initialized || !context)
		return;

	context->Update();
}

void RmlUIManager::Render(RenderDevice* device, FSceneNode* frame)
{
	if (!initialized || !context)
		return;

	renderInterface->SetRenderState(device, frame);
	context->Render();
	renderInterface->ResetScissorState();
}

void RmlUIManager::SetViewportSize(int width, int height)
{
	if (!initialized || !context)
		return;

	context->SetDimensions(Rml::Vector2i(width, height));
}

// --- Input Routing ---

bool RmlUIManager::ProcessMouseMove(int x, int y, int keyModifiers)
{
	if (!initialized || !context)
		return false;

	return !context->ProcessMouseMove(x, y, keyModifiers);
}

bool RmlUIManager::ProcessMouseButtonDown(int buttonIndex, int keyModifiers)
{
	if (!initialized || !context)
		return false;

	if (buttonIndex < 0)
		return false;

	return !context->ProcessMouseButtonDown(buttonIndex, keyModifiers);
}

bool RmlUIManager::ProcessMouseButtonUp(int buttonIndex, int keyModifiers)
{
	if (!initialized || !context)
		return false;

	if (buttonIndex < 0)
		return false;

	return !context->ProcessMouseButtonUp(buttonIndex, keyModifiers);
}

bool RmlUIManager::ProcessMouseWheel(float delta, int keyModifiers)
{
	if (!initialized || !context)
		return false;

	return !context->ProcessMouseWheel(Rml::Vector2f(0.0f, delta), keyModifiers);
}

bool RmlUIManager::ProcessKeyDown(EInputKey key, int keyModifiers)
{
	if (!initialized || !context)
		return false;

	Rml::Input::KeyIdentifier rmlKey = MapKey(key);
	if (rmlKey == Rml::Input::KI_UNKNOWN)
		return false;

	return !context->ProcessKeyDown(rmlKey, keyModifiers);
}

bool RmlUIManager::ProcessKeyUp(EInputKey key, int keyModifiers)
{
	if (!initialized || !context)
		return false;

	Rml::Input::KeyIdentifier rmlKey = MapKey(key);
	if (rmlKey == Rml::Input::KI_UNKNOWN)
		return false;

	return !context->ProcessKeyUp(rmlKey, keyModifiers);
}

bool RmlUIManager::ProcessTextInput(const std::string& text)
{
	if (!initialized || !context)
		return false;

	return !context->ProcessTextInput(text);
}

bool RmlUIManager::ProcessMouseLeave()
{
	if (!initialized || !context)
		return false;

	return !context->ProcessMouseLeave();
}

bool RmlUIManager::IsCapturingMouse() const
{
	if (!initialized || !context)
		return false;

	return context->IsMouseInteracting();
}

int RmlUIManager::GetKeyModifierState()
{
	int state = 0;
	if (engine && engine->window)
	{
		if (engine->window->GetKeyState(IK_Ctrl) || engine->window->GetKeyState(IK_LControl) || engine->window->GetKeyState(IK_RControl))
			state |= Rml::Input::KM_CTRL;
		if (engine->window->GetKeyState(IK_Shift) || engine->window->GetKeyState(IK_LShift) || engine->window->GetKeyState(IK_RShift))
			state |= Rml::Input::KM_SHIFT;
		if (engine->window->GetKeyState(IK_Alt))
			state |= Rml::Input::KM_ALT;
		if (engine->window->GetKeyState(IK_CapsLock))
			state |= Rml::Input::KM_CAPSLOCK;
		if (engine->window->GetKeyState(IK_NumLock))
			state |= Rml::Input::KM_NUMLOCK;
		if (engine->window->GetKeyState(IK_ScrollLock))
			state |= Rml::Input::KM_SCROLLLOCK;
	}
	return state;
}

int RmlUIManager::MapMouseButton(EInputKey key)
{
	switch (key)
	{
	case IK_LeftMouse:  return 0;
	case IK_RightMouse: return 1;
	case IK_MiddleMouse: return 2;
	default: return -1;
	}
}

Rml::Input::KeyIdentifier RmlUIManager::MapKey(EInputKey key)
{
	switch (key)
	{
	case IK_A: return Rml::Input::KI_A;
	case IK_B: return Rml::Input::KI_B;
	case IK_C: return Rml::Input::KI_C;
	case IK_D: return Rml::Input::KI_D;
	case IK_E: return Rml::Input::KI_E;
	case IK_F: return Rml::Input::KI_F;
	case IK_G: return Rml::Input::KI_G;
	case IK_H: return Rml::Input::KI_H;
	case IK_I: return Rml::Input::KI_I;
	case IK_J: return Rml::Input::KI_J;
	case IK_K: return Rml::Input::KI_K;
	case IK_L: return Rml::Input::KI_L;
	case IK_M: return Rml::Input::KI_M;
	case IK_N: return Rml::Input::KI_N;
	case IK_O: return Rml::Input::KI_O;
	case IK_P: return Rml::Input::KI_P;
	case IK_Q: return Rml::Input::KI_Q;
	case IK_R: return Rml::Input::KI_R;
	case IK_S: return Rml::Input::KI_S;
	case IK_T: return Rml::Input::KI_T;
	case IK_U: return Rml::Input::KI_U;
	case IK_V: return Rml::Input::KI_V;
	case IK_W: return Rml::Input::KI_W;
	case IK_X: return Rml::Input::KI_X;
	case IK_Y: return Rml::Input::KI_Y;
	case IK_Z: return Rml::Input::KI_Z;

	case IK_0: return Rml::Input::KI_0;
	case IK_1: return Rml::Input::KI_1;
	case IK_2: return Rml::Input::KI_2;
	case IK_3: return Rml::Input::KI_3;
	case IK_4: return Rml::Input::KI_4;
	case IK_5: return Rml::Input::KI_5;
	case IK_6: return Rml::Input::KI_6;
	case IK_7: return Rml::Input::KI_7;
	case IK_8: return Rml::Input::KI_8;
	case IK_9: return Rml::Input::KI_9;

	case IK_NumPad0: return Rml::Input::KI_NUMPAD0;
	case IK_NumPad1: return Rml::Input::KI_NUMPAD1;
	case IK_NumPad2: return Rml::Input::KI_NUMPAD2;
	case IK_NumPad3: return Rml::Input::KI_NUMPAD3;
	case IK_NumPad4: return Rml::Input::KI_NUMPAD4;
	case IK_NumPad5: return Rml::Input::KI_NUMPAD5;
	case IK_NumPad6: return Rml::Input::KI_NUMPAD6;
	case IK_NumPad7: return Rml::Input::KI_NUMPAD7;
	case IK_NumPad8: return Rml::Input::KI_NUMPAD8;
	case IK_NumPad9: return Rml::Input::KI_NUMPAD9;

	case IK_GreyStar:  return Rml::Input::KI_MULTIPLY;
	case IK_GreyPlus:  return Rml::Input::KI_ADD;
	case IK_Separator: return Rml::Input::KI_SEPARATOR;
	case IK_GreyMinus: return Rml::Input::KI_SUBTRACT;
	case IK_NumPadPeriod: return Rml::Input::KI_DECIMAL;
	case IK_GreySlash: return Rml::Input::KI_DIVIDE;

	case IK_F1:  return Rml::Input::KI_F1;
	case IK_F2:  return Rml::Input::KI_F2;
	case IK_F3:  return Rml::Input::KI_F3;
	case IK_F4:  return Rml::Input::KI_F4;
	case IK_F5:  return Rml::Input::KI_F5;
	case IK_F6:  return Rml::Input::KI_F6;
	case IK_F7:  return Rml::Input::KI_F7;
	case IK_F8:  return Rml::Input::KI_F8;
	case IK_F9:  return Rml::Input::KI_F9;
	case IK_F10: return Rml::Input::KI_F10;
	case IK_F11: return Rml::Input::KI_F11;
	case IK_F12: return Rml::Input::KI_F12;

	case IK_Backspace: return Rml::Input::KI_BACK;
	case IK_Tab:       return Rml::Input::KI_TAB;
	case IK_Enter:     return Rml::Input::KI_RETURN;
	case IK_Pause:     return Rml::Input::KI_PAUSE;
	case IK_CapsLock:  return Rml::Input::KI_CAPITAL;
	case IK_Escape:    return Rml::Input::KI_ESCAPE;
	case IK_Space:     return Rml::Input::KI_SPACE;

	case IK_PageUp:   return Rml::Input::KI_PRIOR;
	case IK_PageDown:  return Rml::Input::KI_NEXT;
	case IK_End:       return Rml::Input::KI_END;
	case IK_Home:      return Rml::Input::KI_HOME;
	case IK_Left:      return Rml::Input::KI_LEFT;
	case IK_Up:        return Rml::Input::KI_UP;
	case IK_Right:     return Rml::Input::KI_RIGHT;
	case IK_Down:      return Rml::Input::KI_DOWN;

	case IK_Insert:    return Rml::Input::KI_INSERT;
	case IK_Delete:    return Rml::Input::KI_DELETE;

	case IK_NumLock:    return Rml::Input::KI_NUMLOCK;
	case IK_ScrollLock: return Rml::Input::KI_SCROLL;

	case IK_LShift:   return Rml::Input::KI_LSHIFT;
	case IK_RShift:   return Rml::Input::KI_RSHIFT;
	case IK_LControl: return Rml::Input::KI_LCONTROL;
	case IK_RControl: return Rml::Input::KI_RCONTROL;

	case IK_Semicolon:    return Rml::Input::KI_OEM_1;
	case IK_Equals:       return Rml::Input::KI_OEM_PLUS;
	case IK_Comma:        return Rml::Input::KI_OEM_COMMA;
	case IK_Minus:        return Rml::Input::KI_OEM_MINUS;
	case IK_Period:       return Rml::Input::KI_OEM_PERIOD;
	case IK_Slash:        return Rml::Input::KI_OEM_2;
	case IK_Tilde:        return Rml::Input::KI_OEM_3;
	case IK_LeftBracket:  return Rml::Input::KI_OEM_4;
	case IK_Backslash:    return Rml::Input::KI_OEM_5;
	case IK_RightBracket: return Rml::Input::KI_OEM_6;
	case IK_SingleQuote:  return Rml::Input::KI_OEM_7;

	default: return Rml::Input::KI_UNKNOWN;
	}
}

// --- Data Model ---

void RmlUIManager::SetupDataModel()
{
	if (!context)
		return;

	Rml::DataModelConstructor constructor = context->CreateDataModel("hud");
	if (!constructor)
	{
		::LogMessage("RmlUi WARNING: Failed to create HUD data model");
		return;
	}

	// Register WeaponSlot struct before array
	if (auto slot_handle = constructor.RegisterStruct<WeaponSlot>())
	{
		slot_handle.RegisterMember("occupied", &WeaponSlot::occupied);
		slot_handle.RegisterMember("selected", &WeaponSlot::selected);
		slot_handle.RegisterMember("name", &WeaponSlot::name);
		slot_handle.RegisterMember("ammo", &WeaponSlot::ammo);
	}
	constructor.RegisterArray<std::vector<WeaponSlot>>();

	// Scalar bindings
	constructor.Bind("health", &hudViewModel.health);
	constructor.Bind("health_max", &hudViewModel.healthMax);
	constructor.Bind("armor", &hudViewModel.armor);
	constructor.Bind("ammo", &hudViewModel.ammo);
	constructor.Bind("weapon_name", &hudViewModel.weaponName);
	constructor.Bind("player_name", &hudViewModel.playerName);
	constructor.Bind("score", &hudViewModel.score);
	constructor.Bind("deaths", &hudViewModel.deaths);
	constructor.Bind("has_weapon", &hudViewModel.hasWeapon);
	constructor.Bind("frag_count", &hudViewModel.fragCount);
	constructor.Bind("crosshair", &hudViewModel.crosshairIndex);
	constructor.Bind("hud_mode", &hudViewModel.hudMode);

	// Array binding
	constructor.Bind("weapon_slots", &hudViewModel.weaponSlots);

	hudModelHandle = constructor.GetModelHandle();

	::LogMessage("RmlUi: HUD data model created");

	// --- Messages data model ---
	Rml::DataModelConstructor msgConstructor = context->CreateDataModel("messages");
	if (!msgConstructor)
	{
		::LogMessage("RmlUi WARNING: Failed to create messages data model");
		return;
	}

	if (auto msg_handle = msgConstructor.RegisterStruct<MessageEntry>())
	{
		msg_handle.RegisterMember("text", &MessageEntry::text);
		msg_handle.RegisterMember("type", &MessageEntry::type);
		msg_handle.RegisterMember("color", &MessageEntry::color);
		msg_handle.RegisterMember("time_remaining", &MessageEntry::timeRemaining);
	}
	msgConstructor.RegisterArray<std::vector<MessageEntry>>();

	msgConstructor.Bind("messages", &messagesViewModel.messages);
	msgConstructor.Bind("is_typing", &messagesViewModel.isTyping);
	msgConstructor.Bind("typed_string", &messagesViewModel.typedString);

	messagesModelHandle = msgConstructor.GetModelHandle();

	::LogMessage("RmlUi: Messages data model created");

	// --- Scoreboard data model ---
	Rml::DataModelConstructor sbConstructor = context->CreateDataModel("scoreboard");
	if (!sbConstructor)
	{
		::LogMessage("RmlUi WARNING: Failed to create scoreboard data model");
		return;
	}

	if (auto player_handle = sbConstructor.RegisterStruct<PlayerEntry>())
	{
		player_handle.RegisterMember("name", &PlayerEntry::name);
		player_handle.RegisterMember("score", &PlayerEntry::score);
		player_handle.RegisterMember("deaths", &PlayerEntry::deaths);
		player_handle.RegisterMember("ping", &PlayerEntry::ping);
		player_handle.RegisterMember("team", &PlayerEntry::team);
		player_handle.RegisterMember("is_bot", &PlayerEntry::isBot);
	}
	sbConstructor.RegisterArray<std::vector<PlayerEntry>>();

	sbConstructor.Bind("players", &scoreboardViewModel.players);
	sbConstructor.Bind("map_name", &scoreboardViewModel.mapName);
	sbConstructor.Bind("game_name", &scoreboardViewModel.gameName);
	sbConstructor.Bind("visible", &scoreboardViewModel.visible);

	scoreboardModelHandle = sbConstructor.GetModelHandle();

	::LogMessage("RmlUi: Scoreboard data model created");

	// --- Console data model ---
	Rml::DataModelConstructor conConstructor = context->CreateDataModel("console");
	if (!conConstructor)
	{
		::LogMessage("RmlUi WARNING: Failed to create console data model");
		return;
	}

	conConstructor.RegisterArray<std::vector<std::string>>();

	conConstructor.Bind("log_lines", &consoleViewModel.logLines);
	conConstructor.Bind("typed_str", &consoleViewModel.typedStr);
	conConstructor.Bind("visible", &consoleViewModel.visible);

	consoleModelHandle = conConstructor.GetModelHandle();

	::LogMessage("RmlUi: Console data model created");

	// --- Menu data model ---
	Rml::DataModelConstructor menuConstructor = context->CreateDataModel("menu");
	if (!menuConstructor)
	{
		::LogMessage("RmlUi WARNING: Failed to create menu data model");
		return;
	}

	if (auto slot_handle = menuConstructor.RegisterStruct<SaveSlotEntry>())
	{
		slot_handle.RegisterMember("index", &SaveSlotEntry::index);
		slot_handle.RegisterMember("description", &SaveSlotEntry::description);
		slot_handle.RegisterMember("has_data", &SaveSlotEntry::hasData);
	}
	menuConstructor.RegisterArray<std::vector<SaveSlotEntry>>();

	menuConstructor.Bind("visible", &menuViewModel.visible);
	menuConstructor.Bind("show_main", &menuViewModel.showMain);
	menuConstructor.Bind("show_options", &menuViewModel.showOptions);
	menuConstructor.Bind("show_save", &menuViewModel.showSave);
	menuConstructor.Bind("show_load", &menuViewModel.showLoad);
	menuConstructor.Bind("show_quit", &menuViewModel.showQuit);
	menuConstructor.Bind("music_volume", &menuViewModel.musicVolume);
	menuConstructor.Bind("sound_volume", &menuViewModel.soundVolume);
	menuConstructor.Bind("brightness", &menuViewModel.brightness);
	menuConstructor.Bind("save_slots", &menuViewModel.saveSlots);

	menuConstructor.BindEventCallback("menu_action",
		[this](Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& arguments)
		{
			if (!arguments.empty())
				HandleMenuAction(arguments[0].Get<Rml::String>());
		});

	menuModelHandle = menuConstructor.GetModelHandle();

	::LogMessage("RmlUi: Menu data model created");
}

void RmlUIManager::UpdateHUDData(const HUDViewModel& data)
{
	if (!initialized || !hudModelHandle)
		return;

	if (hudViewModel.health != data.health)
	{
		hudViewModel.health = data.health;
		hudModelHandle.DirtyVariable("health");
	}
	if (hudViewModel.healthMax != data.healthMax)
	{
		hudViewModel.healthMax = data.healthMax;
		hudModelHandle.DirtyVariable("health_max");
	}
	if (hudViewModel.armor != data.armor)
	{
		hudViewModel.armor = data.armor;
		hudModelHandle.DirtyVariable("armor");
	}
	if (hudViewModel.ammo != data.ammo)
	{
		hudViewModel.ammo = data.ammo;
		hudModelHandle.DirtyVariable("ammo");
	}
	if (hudViewModel.weaponName != data.weaponName)
	{
		hudViewModel.weaponName = data.weaponName;
		hudModelHandle.DirtyVariable("weapon_name");
	}
	if (hudViewModel.playerName != data.playerName)
	{
		hudViewModel.playerName = data.playerName;
		hudModelHandle.DirtyVariable("player_name");
	}
	if (hudViewModel.score != data.score)
	{
		hudViewModel.score = data.score;
		hudModelHandle.DirtyVariable("score");
	}
	if (hudViewModel.deaths != data.deaths)
	{
		hudViewModel.deaths = data.deaths;
		hudModelHandle.DirtyVariable("deaths");
	}
	if (hudViewModel.hasWeapon != data.hasWeapon)
	{
		hudViewModel.hasWeapon = data.hasWeapon;
		hudModelHandle.DirtyVariable("has_weapon");
	}
	if (hudViewModel.fragCount != data.fragCount)
	{
		hudViewModel.fragCount = data.fragCount;
		hudModelHandle.DirtyVariable("frag_count");
	}
	if (hudViewModel.crosshairIndex != data.crosshairIndex)
	{
		hudViewModel.crosshairIndex = data.crosshairIndex;
		hudModelHandle.DirtyVariable("crosshair");
	}
	if (hudViewModel.hudMode != data.hudMode)
	{
		hudViewModel.hudMode = data.hudMode;
		hudModelHandle.DirtyVariable("hud_mode");
	}
	if (hudViewModel.weaponSlots != data.weaponSlots)
	{
		hudViewModel.weaponSlots = data.weaponSlots;
		hudModelHandle.DirtyVariable("weapon_slots");
	}
}

void RmlUIManager::UpdateMessagesData(const MessagesViewModel& data)
{
	if (!initialized || !messagesModelHandle)
		return;

	if (messagesViewModel.messages != data.messages)
	{
		messagesViewModel.messages = data.messages;
		messagesModelHandle.DirtyVariable("messages");
	}
	if (messagesViewModel.isTyping != data.isTyping)
	{
		messagesViewModel.isTyping = data.isTyping;
		messagesModelHandle.DirtyVariable("is_typing");
	}
	if (messagesViewModel.typedString != data.typedString)
	{
		messagesViewModel.typedString = data.typedString;
		messagesModelHandle.DirtyVariable("typed_string");
	}
}

void RmlUIManager::UpdateScoreboardData(const ScoreboardViewModel& data)
{
	if (!initialized || !scoreboardModelHandle)
		return;

	if (scoreboardViewModel.players != data.players)
	{
		scoreboardViewModel.players = data.players;
		scoreboardModelHandle.DirtyVariable("players");
	}
	if (scoreboardViewModel.mapName != data.mapName)
	{
		scoreboardViewModel.mapName = data.mapName;
		scoreboardModelHandle.DirtyVariable("map_name");
	}
	if (scoreboardViewModel.gameName != data.gameName)
	{
		scoreboardViewModel.gameName = data.gameName;
		scoreboardModelHandle.DirtyVariable("game_name");
	}
	if (scoreboardViewModel.visible != data.visible)
	{
		scoreboardViewModel.visible = data.visible;
		scoreboardModelHandle.DirtyVariable("visible");

		// Show/hide scoreboard document based on visibility
		if (data.visible)
			ShowDocument("scoreboard");
		else
			HideDocument("scoreboard");
	}
}

void RmlUIManager::UpdateConsoleData(const ConsoleViewModel& data)
{
	if (!initialized || !consoleModelHandle)
		return;

	if (consoleViewModel.logLines != data.logLines)
	{
		consoleViewModel.logLines = data.logLines;
		consoleModelHandle.DirtyVariable("log_lines");
	}
	if (consoleViewModel.typedStr != data.typedStr)
	{
		consoleViewModel.typedStr = data.typedStr;
		consoleModelHandle.DirtyVariable("typed_str");
	}
	if (consoleViewModel.visible != data.visible)
	{
		consoleViewModel.visible = data.visible;
		consoleModelHandle.DirtyVariable("visible");

		// Show/hide console document based on visibility
		if (data.visible)
			ShowDocument("console");
		else
			HideDocument("console");
	}
}

bool RmlUIManager::IsMenuOnSubScreen() const
{
	return menuViewModel.showOptions || menuViewModel.showSave
		|| menuViewModel.showLoad || menuViewModel.showQuit;
}

void RmlUIManager::UpdateMenuData(const MenuViewModel& data)
{
	if (!initialized || !menuModelHandle)
		return;

	if (menuViewModel.saveSlots != data.saveSlots)
	{
		menuViewModel.saveSlots = data.saveSlots;
		menuModelHandle.DirtyVariable("save_slots");
	}
	if (menuViewModel.musicVolume != data.musicVolume)
	{
		menuViewModel.musicVolume = data.musicVolume;
		menuModelHandle.DirtyVariable("music_volume");
	}
	if (menuViewModel.soundVolume != data.soundVolume)
	{
		menuViewModel.soundVolume = data.soundVolume;
		menuModelHandle.DirtyVariable("sound_volume");
	}
	if (menuViewModel.brightness != data.brightness)
	{
		menuViewModel.brightness = data.brightness;
		menuModelHandle.DirtyVariable("brightness");
	}
}

void RmlUIManager::HandleMenuAction(const std::string& action)
{
	if (!initialized || !menuModelHandle)
		return;

	if (action == "resume")
	{
		HideDocument("menu");
		menuViewModel.visible = false;
		menuModelHandle.DirtyVariable("visible");
		if (engine)
		{
			engine->uiSuppression.bRmlMenus = false;
			engine->SetPause(false);
		}
		SetMenuScreen("main");
	}
	else if (action == "options")
	{
		SetMenuScreen("options");
	}
	else if (action == "save")
	{
		PopulateSaveSlots();
		SetMenuScreen("save");
	}
	else if (action == "load")
	{
		PopulateSaveSlots();
		SetMenuScreen("load");
	}
	else if (action == "quit")
	{
		SetMenuScreen("quit");
	}
	else if (action == "quit_yes")
	{
		if (engine)
			engine->quit = true;
	}
	else if (action == "back")
	{
		SetMenuScreen("main");
	}
	else if (action == "music_up")
	{
		menuViewModel.musicVolume = std::min(255, menuViewModel.musicVolume + 16);
		menuModelHandle.DirtyVariable("music_volume");
	}
	else if (action == "music_down")
	{
		menuViewModel.musicVolume = std::max(0, menuViewModel.musicVolume - 16);
		menuModelHandle.DirtyVariable("music_volume");
	}
	else if (action == "sound_up")
	{
		menuViewModel.soundVolume = std::min(255, menuViewModel.soundVolume + 16);
		menuModelHandle.DirtyVariable("sound_volume");
	}
	else if (action == "sound_down")
	{
		menuViewModel.soundVolume = std::max(0, menuViewModel.soundVolume - 16);
		menuModelHandle.DirtyVariable("sound_volume");
	}
	else if (action == "bright_up")
	{
		menuViewModel.brightness = std::min(10, menuViewModel.brightness + 1);
		menuModelHandle.DirtyVariable("brightness");
	}
	else if (action == "bright_down")
	{
		menuViewModel.brightness = std::max(1, menuViewModel.brightness - 1);
		menuModelHandle.DirtyVariable("brightness");
	}
	else if (action.substr(0, 5) == "save_")
	{
		int slot = std::stoi(action.substr(5));
		if (engine)
		{
			engine->SaveGameInfo.SaveGameSlot = slot;
			engine->SaveGameInfo.SaveGameDescription = "Save " + std::to_string(slot);
		}
		// Return to main menu after save
		SetMenuScreen("main");
	}
	else if (action.substr(0, 5) == "load_")
	{
		int slot = std::stoi(action.substr(5));
		if (engine)
		{
			// Build a travel URL with load option
			std::string mapName = engine->LevelInfo ? engine->LevelInfo->URL.Map : "";
			if (!mapName.empty())
			{
				engine->ClientTravel(mapName + "?load=" + std::to_string(slot),
					ETravelType::TRAVEL_Absolute, false);
			}
		}
		// Close menu
		HandleMenuAction("resume");
	}
}

void RmlUIManager::SetMenuScreen(const std::string& screen)
{
	if (!menuModelHandle)
		return;

	bool wasMain = menuViewModel.showMain;
	bool wasOptions = menuViewModel.showOptions;
	bool wasSave = menuViewModel.showSave;
	bool wasLoad = menuViewModel.showLoad;
	bool wasQuit = menuViewModel.showQuit;

	menuViewModel.showMain = (screen == "main");
	menuViewModel.showOptions = (screen == "options");
	menuViewModel.showSave = (screen == "save");
	menuViewModel.showLoad = (screen == "load");
	menuViewModel.showQuit = (screen == "quit");

	if (menuViewModel.showMain != wasMain)
		menuModelHandle.DirtyVariable("show_main");
	if (menuViewModel.showOptions != wasOptions)
		menuModelHandle.DirtyVariable("show_options");
	if (menuViewModel.showSave != wasSave)
		menuModelHandle.DirtyVariable("show_save");
	if (menuViewModel.showLoad != wasLoad)
		menuModelHandle.DirtyVariable("show_load");
	if (menuViewModel.showQuit != wasQuit)
		menuModelHandle.DirtyVariable("show_quit");
}

void RmlUIManager::PopulateSaveSlots()
{
	menuViewModel.saveSlots.clear();

	std::string gameRoot;
	std::string saveExt;
	if (engine)
	{
		gameRoot = engine->LaunchInfo.gameRootFolder;
		saveExt = engine->packages ? engine->packages->GetSaveExtension() : "usa";
	}

	for (int i = 0; i < 10; i++)
	{
		SaveSlotEntry slot;
		slot.index = i;
		slot.description = "Slot " + std::to_string(i);

		if (!gameRoot.empty())
		{
			auto slotPath = std::filesystem::path(gameRoot) / "Save" /
				("Save" + std::to_string(i) + "." + saveExt);
			slot.hasData = std::filesystem::exists(slotPath);
			if (slot.hasData)
				slot.description = "Save " + std::to_string(i);
		}

		menuViewModel.saveSlots.push_back(slot);
	}

	menuModelHandle.DirtyVariable("save_slots");
}
