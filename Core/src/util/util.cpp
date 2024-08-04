#include "pch/pch.h"
#include "util/FileUtil.h"
#include "util/util.h"
#include <fstream>
#include <filesystem>

namespace SNAKE {
	std::vector<char> files::ReadFileBinary(const std::string& filepath) {
		std::ifstream file(filepath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			SNK_CORE_ERROR("Failed to open file: '{0}'", filepath);
			return {};
		}

		size_t file_size = (size_t)file.tellg();
		std::vector<char> buffer(file_size);

		file.seekg(0);
		file.read(buffer.data(), file_size);

		file.close();
		return buffer;
	}

	bool files::FileExists(const std::string& filepath) {
		try {
			return std::filesystem::exists(filepath);
		}
		catch (std::exception e) {
			return false;
		}
	}
}