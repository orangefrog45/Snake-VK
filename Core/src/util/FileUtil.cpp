#include "pch/pch.h"
#include "FileUtil.h"
#include "util.h"
#include <fstream>

namespace SNAKE {
	std::vector<char> ReadFileBinary(const std::string& filepath) {
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
}