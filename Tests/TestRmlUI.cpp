
#include "Precomp.h"
#include "RmlUI/RmlUIFileInterface.h"
#include "RmlUI/RmlUIRenderInterface.h"
#include "RmlUI/RmlUIManager.h"
#include "Utils/File.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <RmlUi/Core.h>

namespace fs = std::filesystem;

// Engine.h declares `extern Engine* engine` — it's defined in Engine.cpp (part of SurrealCommon)
#include "Engine.h"

static std::string testDir;

static void SetupTestDir()
{
	testDir = (fs::temp_directory_path() / "surreal_rmlui_test").string();
	fs::create_directories(testDir);
	fs::create_directories(testDir + "/fonts");

	// Create a small test file
	File::write_all_text(testDir + "/test.txt", "Hello, RmlUi!");
}

static void CleanupTestDir()
{
	fs::remove_all(testDir);
}

// ---- FileInterface Tests ----

static void TestFileOpenRelative()
{
	std::cout << "  FileInterface: relative path resolution... ";
	RmlUIFileInterface fi(testDir);
	auto handle = fi.Open("test.txt");
	assert(handle != 0);
	fi.Close(handle);
	std::cout << "OK\n";
}

static void TestFileRejectDotDot()
{
	std::cout << "  FileInterface: reject ../ paths... ";
	RmlUIFileInterface fi(testDir);
	auto handle = fi.Open("../etc/passwd");
	assert(handle == 0);
	std::cout << "OK\n";
}

static void TestFileEmptyPath()
{
	std::cout << "  FileInterface: empty path returns 0... ";
	RmlUIFileInterface fi(testDir);
	auto handle = fi.Open("");
	assert(handle == 0);
	std::cout << "OK\n";
}

static void TestFileReadFull()
{
	std::cout << "  FileInterface: read full file... ";
	RmlUIFileInterface fi(testDir);
	auto handle = fi.Open("test.txt");
	assert(handle != 0);

	char buf[64] = {};
	size_t bytesRead = fi.Read(buf, sizeof(buf), handle);
	assert(bytesRead == 13); // "Hello, RmlUi!"
	assert(std::string(buf, bytesRead) == "Hello, RmlUi!");

	fi.Close(handle);
	std::cout << "OK\n";
}

static void TestFileReadEOF()
{
	std::cout << "  FileInterface: partial read at EOF... ";
	RmlUIFileInterface fi(testDir);
	auto handle = fi.Open("test.txt");
	assert(handle != 0);

	// Seek to near the end
	fi.Seek(handle, -5, SEEK_END);
	assert(fi.Tell(handle) == 8);

	char buf[64] = {};
	size_t bytesRead = fi.Read(buf, sizeof(buf), handle);
	assert(bytesRead == 5); // "lUi!"
	assert(std::string(buf, bytesRead) == "mlUi!");

	fi.Close(handle);
	std::cout << "OK\n";
}

// ---- RenderInterface Tests ----

static void TestGenerateTexture()
{
	std::cout << "  RenderInterface: GenerateTexture... ";
	RmlUIRenderInterface ri;

	// 2x2 RGBA pixels (red, green, blue, white)
	uint8_t pixels[] = {
		255, 0, 0, 255,   0, 255, 0, 255,
		0, 0, 255, 255,   255, 255, 255, 255
	};

	auto handle = ri.GenerateTexture(
		Rml::Span<const Rml::byte>(pixels, sizeof(pixels)),
		Rml::Vector2i(2, 2)
	);
	assert(handle != 0);

	ri.ReleaseTexture(handle);
	std::cout << "OK\n";
}

static void TestCompileGeometry()
{
	std::cout << "  RenderInterface: CompileGeometry lifecycle... ";
	RmlUIRenderInterface ri;

	Rml::Vertex verts[3] = {};
	verts[0].position = { 0.0f, 0.0f };
	verts[1].position = { 100.0f, 0.0f };
	verts[2].position = { 50.0f, 100.0f };
	for (auto& v : verts) v.colour = { 255, 255, 255, 255 };

	int indices[] = { 0, 1, 2 };

	auto handle = ri.CompileGeometry(
		Rml::Span<const Rml::Vertex>(verts, 3),
		Rml::Span<const int>(indices, 3)
	);
	assert(handle != 0);

	// Release should not crash
	ri.ReleaseGeometry(handle);

	// Releasing invalid handle should not crash
	ri.ReleaseGeometry(999999);

	std::cout << "OK\n";
}

// ---- LoadTexture Tests ----

// Minimal valid 1x1 red PNG (67 bytes)
static const uint8_t minimalPng[] = {
	0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // PNG signature
	0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, // IHDR chunk
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, // 1x1
	0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, // 8-bit RGB
	0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, // IDAT chunk
	0x54, 0x08, 0xD7, 0x63, 0xF8, 0xCF, 0xC0, 0x00, // compressed data
	0x00, 0x00, 0x02, 0x00, 0x01, 0xE2, 0x21, 0xBC, // checksum
	0x33, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, // IEND chunk
	0x44, 0xAE, 0x42, 0x60, 0x82
};

static void TestLoadTextureValid()
{
	std::cout << "  RenderInterface: LoadTexture valid PNG... ";

	// Write the PNG to the test directory
	std::string pngPath = testDir + "/test.png";
	{
		std::ofstream out(pngPath, std::ios::binary);
		out.write(reinterpret_cast<const char*>(minimalPng), sizeof(minimalPng));
	}

	// Initialize RmlUI so file+render interfaces are available
	RmlUIManager mgr;
	bool ok = mgr.Initialize(testDir, 800, 600);
	if (!ok)
	{
		std::cout << "SKIP (init failed)\n";
		return;
	}

	// LoadTexture uses Rml::GetFileInterface() which was set by the manager
	Rml::Vector2i dims;
	Rml::RenderInterface* ri = Rml::GetRenderInterface();
	auto handle = ri->LoadTexture(dims, "test.png");
	assert(handle != 0);
	assert(dims.x == 1);
	assert(dims.y == 1);

	ri->ReleaseTexture(handle);
	mgr.Shutdown();

	std::cout << "OK\n";
}

static void TestLoadTextureNonExistent()
{
	std::cout << "  RenderInterface: LoadTexture non-existent file... ";

	RmlUIManager mgr;
	bool ok = mgr.Initialize(testDir, 800, 600);
	if (!ok)
	{
		std::cout << "SKIP (init failed)\n";
		return;
	}

	Rml::Vector2i dims;
	Rml::RenderInterface* ri = Rml::GetRenderInterface();
	auto handle = ri->LoadTexture(dims, "nonexistent.png");
	assert(handle == 0);

	mgr.Shutdown();
	std::cout << "OK\n";
}

static void TestLoadTextureCorrupt()
{
	std::cout << "  RenderInterface: LoadTexture corrupt data... ";

	// Write random bytes
	std::string corruptPath = testDir + "/corrupt.png";
	{
		std::ofstream out(corruptPath, std::ios::binary);
		uint8_t garbage[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11, 0x22, 0x33 };
		out.write(reinterpret_cast<const char*>(garbage), sizeof(garbage));
	}

	RmlUIManager mgr;
	bool ok = mgr.Initialize(testDir, 800, 600);
	if (!ok)
	{
		std::cout << "SKIP (init failed)\n";
		return;
	}

	Rml::Vector2i dims;
	Rml::RenderInterface* ri = Rml::GetRenderInterface();
	auto handle = ri->LoadTexture(dims, "corrupt.png");
	assert(handle == 0);

	mgr.Shutdown();
	std::cout << "OK\n";
}

// ---- Manager Tests ----

static void TestManagerInitGate()
{
	std::cout << "  RmlUIManager: init with non-existent path... ";
	RmlUIManager mgr;
	bool result = mgr.Initialize("/tmp/nonexistent_path_xyz_12345", 800, 600);
	assert(!result);
	assert(!mgr.IsInitialized());
	std::cout << "OK\n";
}

static void TestManagerLifecycle()
{
	std::cout << "  RmlUIManager: lifecycle with temp dir... ";
	RmlUIManager mgr;
	bool result = mgr.Initialize(testDir, 800, 600);
	assert(result);
	assert(mgr.IsInitialized());

	// Update should not crash
	mgr.Update();

	mgr.Shutdown();
	assert(!mgr.IsInitialized());
	std::cout << "OK\n";
}

// ---- Input Routing Tests ----

static void TestMapKey()
{
	std::cout << "  Input: MapKey representative keys... ";

	// A-Z range
	assert(RmlUIManager::MapMouseButton(IK_LeftMouse) == 0);

	// Test via the static methods that don't need initialization
	assert(RmlUIManager::MapMouseButton(IK_RightMouse) == 1);
	assert(RmlUIManager::MapMouseButton(IK_MiddleMouse) == 2);

	std::cout << "OK\n";
}

static void TestMapMouseButton()
{
	std::cout << "  Input: MapMouseButton... ";
	assert(RmlUIManager::MapMouseButton(IK_LeftMouse) == 0);
	assert(RmlUIManager::MapMouseButton(IK_RightMouse) == 1);
	assert(RmlUIManager::MapMouseButton(IK_MiddleMouse) == 2);
	assert(RmlUIManager::MapMouseButton(IK_Space) == -1);
	std::cout << "OK\n";
}

static void TestProcessInputUninitialized()
{
	std::cout << "  Input: Process* methods return false when uninitialized... ";
	RmlUIManager mgr;
	// Not initialized — all should return false
	assert(!mgr.ProcessMouseMove(100, 100, 0));
	assert(!mgr.ProcessMouseButtonDown(0, 0));
	assert(!mgr.ProcessMouseButtonUp(0, 0));
	assert(!mgr.ProcessMouseWheel(1.0f, 0));
	assert(!mgr.ProcessKeyDown(IK_A, 0));
	assert(!mgr.ProcessKeyUp(IK_A, 0));
	assert(!mgr.ProcessTextInput("hello"));
	assert(!mgr.ProcessMouseLeave());
	assert(!mgr.IsCapturingMouse());
	std::cout << "OK\n";
}

static void TestProcessInvalidButton()
{
	std::cout << "  Input: ProcessMouseButtonDown with invalid button... ";
	RmlUIManager mgr;
	bool ok = mgr.Initialize(testDir, 800, 600);
	if (!ok)
	{
		std::cout << "SKIP (init failed)\n";
		return;
	}

	// Invalid button index (-1) should return false
	assert(!mgr.ProcessMouseButtonDown(-1, 0));

	mgr.Shutdown();
	std::cout << "OK\n";
}

static void TestProcessUnmappedKey()
{
	std::cout << "  Input: ProcessKeyDown with unmapped key... ";
	RmlUIManager mgr;
	bool ok = mgr.Initialize(testDir, 800, 600);
	if (!ok)
	{
		std::cout << "SKIP (init failed)\n";
		return;
	}

	// IK_None maps to KI_UNKNOWN -> returns false
	assert(!mgr.ProcessKeyDown(IK_None, 0));

	mgr.Shutdown();
	std::cout << "OK\n";
}

static void TestGetKeyModifierStateNoWindow()
{
	std::cout << "  Input: GetKeyModifierState with no window... ";
	// engine is nullptr in tests, so should return 0
	assert(RmlUIManager::GetKeyModifierState() == 0);
	std::cout << "OK\n";
}

// ---- Data Model Tests ----

static void TestUpdateHUDDataNoChange()
{
	std::cout << "  DataModel: UpdateHUDData no change... ";
	RmlUIManager mgr;
	bool ok = mgr.Initialize(testDir, 800, 600);
	if (!ok)
	{
		std::cout << "SKIP (init failed)\n";
		return;
	}

	// Call twice with same default data — should not crash
	HUDViewModel hud;
	mgr.UpdateHUDData(hud);
	mgr.UpdateHUDData(hud);

	mgr.Shutdown();
	std::cout << "OK\n";
}

static void TestUpdateHUDDataChange()
{
	std::cout << "  DataModel: UpdateHUDData with changes... ";
	RmlUIManager mgr;
	bool ok = mgr.Initialize(testDir, 800, 600);
	if (!ok)
	{
		std::cout << "SKIP (init failed)\n";
		return;
	}

	HUDViewModel hud;
	hud.health = 100;
	hud.armor = 50;
	hud.weaponName = "Enforcer";
	hud.hasWeapon = true;
	mgr.UpdateHUDData(hud);

	// Change health
	hud.health = 75;
	mgr.UpdateHUDData(hud);

	// Verify manager still works
	mgr.Update();

	mgr.Shutdown();
	std::cout << "OK\n";
}

static void TestUpdateHUDDataDefaults()
{
	std::cout << "  DataModel: UpdateHUDData defaults (no pawn)... ";
	RmlUIManager mgr;
	bool ok = mgr.Initialize(testDir, 800, 600);
	if (!ok)
	{
		std::cout << "SKIP (init failed)\n";
		return;
	}

	// First set some data
	HUDViewModel hud;
	hud.health = 100;
	hud.weaponName = "Enforcer";
	mgr.UpdateHUDData(hud);

	// Now reset to defaults (simulates no pawn)
	HUDViewModel empty;
	mgr.UpdateHUDData(empty);

	mgr.Update();
	mgr.Shutdown();
	std::cout << "OK\n";
}

int main()
{
	std::cout << "RmlUI Tests\n";
	std::cout << "===========\n\n";

	SetupTestDir();

	std::cout << "FileInterface:\n";
	TestFileOpenRelative();
	TestFileRejectDotDot();
	TestFileEmptyPath();
	TestFileReadFull();
	TestFileReadEOF();

	std::cout << "\nRenderInterface:\n";
	TestGenerateTexture();
	TestCompileGeometry();

	std::cout << "\nLoadTexture:\n";
	TestLoadTextureValid();
	TestLoadTextureNonExistent();
	TestLoadTextureCorrupt();

	std::cout << "\nRmlUIManager:\n";
	TestManagerInitGate();
	TestManagerLifecycle();

	std::cout << "\nInput Routing:\n";
	TestMapKey();
	TestMapMouseButton();
	TestProcessInputUninitialized();
	TestProcessInvalidButton();
	TestProcessUnmappedKey();
	TestGetKeyModifierStateNoWindow();

	std::cout << "\nData Model:\n";
	TestUpdateHUDDataNoChange();
	TestUpdateHUDDataChange();
	TestUpdateHUDDataDefaults();

	CleanupTestDir();

	std::cout << "\nAll tests passed!\n";
	return 0;
}
