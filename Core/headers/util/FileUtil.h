#pragma once
namespace SNAKE {
	namespace files {
		bool FileCopy(const std::string& file_to_copy, const std::string& copy_location, bool recursive = false);

		void FileDelete(const std::string& filepath);

		bool PathEqualTo(const std::string& path1, const std::string& path2);

		bool PathExists(const std::string& filepath);

		bool TryFileDelete(const std::string& filepath);

		bool TryDirectoryDelete(const std::string& filepath);

		std::string GetFileDirectory(const std::string& filepath);

		std::string GetFilename(const std::string& filepath);

		std::string GetFileLastWriteTime(const std::string& filepath);

		void Create_Directory(const std::string& path);

		bool IsEntryAFile(const std::filesystem::directory_entry& entry);

		bool ReadBinaryFile(const std::string& filepath, std::vector<std::byte>& output);

		std::string ReadTextFile(const std::string& filepath);

		bool WriteTextFile(const std::string& filepath, const std::string& content);

		bool WriteBinaryFile(const std::string& filepath, void* data, size_t size);

		std::string SelectFileFromExplorer(const std::string& start_dir = "");

		std::string SelectDirectoryFromExplorer(const std::string& start_dir = "");

		// Returns filepath with modified extension, "new_extension" should include the '.', e.g ".png", ".jpg"
		std::string ReplaceFileExtension(const std::string& filepath, const std::string& new_extension);
	}
}