#pragma once

#include <RmlUi/Core/SystemInterface.h>

class RmlUISystemInterface : public Rml::SystemInterface
{
public:
	double GetElapsedTime() override;
	bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
};
