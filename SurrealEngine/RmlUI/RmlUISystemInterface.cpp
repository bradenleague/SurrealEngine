
#include "Precomp.h"
#include "RmlUI/RmlUISystemInterface.h"
#include "Engine.h"

double RmlUISystemInterface::GetElapsedTime()
{
	return engine ? engine->TotalTime : 0.0;
}

bool RmlUISystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message)
{
	std::string prefix;
	switch (type)
	{
	case Rml::Log::LT_ERROR:   prefix = "RmlUi ERROR: "; break;
	case Rml::Log::LT_WARNING: prefix = "RmlUi WARNING: "; break;
	case Rml::Log::LT_INFO:    prefix = "RmlUi INFO: "; break;
	case Rml::Log::LT_DEBUG:   prefix = "RmlUi DEBUG: "; break;
	default:                    prefix = "RmlUi: "; break;
	}
	::LogMessage(prefix + message);
	return true;
}
