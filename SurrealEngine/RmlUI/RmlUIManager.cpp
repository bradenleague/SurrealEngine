
#include "Precomp.h"
#include "RmlUI/RmlUIManager.h"
#include "RmlUI/RmlUISystemInterface.h"
#include "RmlUI/RmlUIFileInterface.h"
#include "RmlUI/RmlUIRenderInterface.h"
#include "Utils/Logger.h"
#include <RmlUi/Core.h>
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

	// Load index.rml if it exists
	std::string indexPath = uiRootPath + "/index.rml";
	if (std::filesystem::exists(indexPath))
	{
		Rml::ElementDocument* document = context->LoadDocument("index.rml");
		if (document)
		{
			document->Show();
			::LogMessage("RmlUi: Loaded document: " + indexPath);
		}
		else
		{
			::LogMessage("RmlUi WARNING: Failed to load index.rml");
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

	context = nullptr;
	Rml::Shutdown();

	systemInterface.reset();
	fileInterface.reset();
	renderInterface.reset();

	initialized = false;
	::LogMessage("RmlUi: Shut down");
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
}

void RmlUIManager::SetViewportSize(int width, int height)
{
	if (!initialized || !context)
		return;

	context->SetDimensions(Rml::Vector2i(width, height));
}
