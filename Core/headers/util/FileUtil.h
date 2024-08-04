#pragma once
namespace SNAKE {
	namespace files {
		std::vector<char> ReadFileBinary(const std::string& filepath);

		bool FileExists(const std::string& filepath);
	}
}