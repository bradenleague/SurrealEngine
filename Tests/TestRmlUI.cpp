
#include "Precomp.h"
#include "RmlUI/RmlUIFileInterface.h"
#include "RmlUI/RmlUIRenderInterface.h"
#include "RmlUI/RmlUIManager.h"
#include "Utils/File.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>

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

	std::cout << "\nRmlUIManager:\n";
	TestManagerInitGate();
	TestManagerLifecycle();

	CleanupTestDir();

	std::cout << "\nAll tests passed!\n";
	return 0;
}
