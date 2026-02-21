
#include "Precomp.h"
#include "RmlUI/RmlUIFileInterface.h"
#include "Utils/File.h"
#include "Utils/Logger.h"
#include <algorithm>
#include <cstdio>

struct RmlFileHandle
{
	std::shared_ptr<File> file;
	int64_t fileSize;
};

RmlUIFileInterface::RmlUIFileInterface(const std::string& rootPath) : rootPath(rootPath)
{
}

Rml::FileHandle RmlUIFileInterface::Open(const Rml::String& path)
{
	if (path.empty())
		return 0;

	// Sandbox enforcement: reject paths containing ".."
	if (path.find("..") != std::string::npos)
	{
		::LogMessage("RmlUi WARNING: Rejected path with '..': " + path);
		return 0;
	}

	std::string fullPath = rootPath + "/" + path;

	auto file = File::try_open_existing(fullPath);
	if (!file)
		return 0;

	auto handle = new RmlFileHandle();
	handle->file = file;
	handle->fileSize = file->size();
	return reinterpret_cast<Rml::FileHandle>(handle);
}

void RmlUIFileInterface::Close(Rml::FileHandle file)
{
	delete reinterpret_cast<RmlFileHandle*>(file);
}

size_t RmlUIFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file)
{
	auto* handle = reinterpret_cast<RmlFileHandle*>(file);
	int64_t pos = (int64_t)handle->file->tell();
	int64_t remaining = handle->fileSize - pos;
	if (remaining <= 0)
		return 0;

	size_t toRead = std::min(size, (size_t)remaining);
	handle->file->read(buffer, toRead);
	return toRead;
}

bool RmlUIFileInterface::Seek(Rml::FileHandle file, long offset, int origin)
{
	auto* handle = reinterpret_cast<RmlFileHandle*>(file);
	SeekPoint seekPoint;
	switch (origin)
	{
	case SEEK_SET: seekPoint = SeekPoint::begin; break;
	case SEEK_CUR: seekPoint = SeekPoint::current; break;
	case SEEK_END: seekPoint = SeekPoint::end; break;
	default: return false;
	}
	handle->file->seek(offset, seekPoint);
	return true;
}

size_t RmlUIFileInterface::Tell(Rml::FileHandle file)
{
	auto* handle = reinterpret_cast<RmlFileHandle*>(file);
	return (size_t)handle->file->tell();
}
