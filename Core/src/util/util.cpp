#include "pch/pch.h"
#include "util/FileUtil.h"
#include "util/util.h"
#include <fstream>
#include <filesystem>
#include <Windows.h>
#include <ShlObj.h>

namespace SNAKE::files {

bool FileCopy(const std::string& file_to_copy, const std::string& copy_location, bool recursive) {
	try {
		if (recursive)
			std::filesystem::copy(file_to_copy, copy_location, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
		else
			std::filesystem::copy_file(file_to_copy, copy_location, std::filesystem::copy_options::overwrite_existing);

		return true;
	}
	catch (const std::exception& e) {
		SNK_CORE_ERROR("std::filesystem::copy_file failed : '{0}'", e.what());
		return false;
	}
}

bool TryFileDelete(const std::string& filepath) {
	if (PathExists(filepath)) {
		FileDelete(filepath);
		return true;
	}

	return false;
}

bool TryDirectoryDelete(const std::string& filepath) {
	if (PathExists(filepath)) {
		try {
			std::filesystem::remove_all(filepath);
		}
		catch (std::exception& e) {
			SNK_CORE_ERROR("std::filesystem::remove_all failed : '{0}'", e.what());
		}
		return true;
	}

	return false;
}



void FileDelete(const std::string& filepath) {
	try {
		std::filesystem::remove(filepath);
	}
	catch (std::exception& e) {
		SNK_CORE_ERROR("std::filesystem::remove error, '{0}'", e.what());
	}
}

bool PathExists(const std::string& filepath) {
	try {
		return std::filesystem::exists(filepath);
	}
	catch (std::exception& e) {
		SNK_CORE_ERROR("std::filesystem::exists error, '{0}'", e.what());
		return false;
	}
}

unsigned StringReplace(std::string& input, const std::string& text_to_replace, const std::string& replacement_text) {
	size_t pos = input.find(text_to_replace, 0);
	unsigned num_replacements = 0;

	while (pos < input.size() && pos != std::string::npos) {
		input.replace(pos, text_to_replace.size(), replacement_text);
		num_replacements++;

		pos = input.find(text_to_replace, pos + replacement_text.size());
	}

	return num_replacements;
}


void Create_Directory(const std::string& path) {
	try {
		if (!std::filesystem::exists(path))
			std::filesystem::create_directory(path);
	}
	catch (std::exception& e) {
		SNK_CORE_ERROR("std::filesystem::create_directory error: '{0}'", e.what());
	}
}


std::string SelectFileFromExplorer(const std::string& start_dir) {
	if (!start_dir.empty() && !PathExists(start_dir)) {
		SNK_CORE_ERROR("SelectFileFromExplorer failed, start_dir '{}' doesn't exist", start_dir);
		return "";
	}

	auto current_path = std::filesystem::current_path();
	char buffer[MAX_PATH];
	ZeroMemory(&buffer, sizeof(buffer));
		
	OPENFILENAME ofn{};
	ZeroMemory(&ofn, sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.hwndOwner = GetFocus();
	ofn.lpstrFilter = nullptr;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = nullptr;
	ofn.lpstrCustomFilter = nullptr;
	ofn.lpstrDefExt = 0;
	ofn.Flags = OFN_FILEMUSTEXIST;
	ofn.lpstrFile = buffer;

	if (!start_dir.empty())
		ofn.lpstrInitialDir = start_dir.c_str();

	GetOpenFileName(&ofn);
	// Reset current path as file explorer changes it
	std::filesystem::current_path(current_path);
	return ofn.lpstrFile;
}


std::string SelectDirectoryFromExplorer(const std::string& start_dir) {
	if (!start_dir.empty() && !std::filesystem::exists(start_dir)) {
		SNK_CORE_ERROR("SelectDirectoryFromExplorer failed, start_dir '{}' doesn't exist", start_dir);
		return "";
	}

	BROWSEINFO bi{};
	char buffer[MAX_PATH];
	ZeroMemory(buffer, sizeof(buffer));

	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.hwndOwner = GetFocus();
	bi.lpszTitle = "Select a directory";
	bi.pszDisplayName = buffer;

	if (!start_dir.empty()) {
		bi.lpfn = [](HWND hwnd, UINT uMsg, [[maybe_unused]] LPARAM lParam, LPARAM lpData) -> int {
			if (uMsg == BFFM_INITIALIZED) {
				SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
			}
			return 0;
			};
		bi.lParam = (LPARAM)start_dir.c_str();
	}

	PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&bi);
	if (pidl) {
		SHGetPathFromIDList(pidl, buffer);
		CoTaskMemFree(pidl);  // Free the memory allocated for the PIDL
		return std::string(buffer);
	}
	return "";
}


std::string GetFileDirectory(const std::string& filepath) {
	size_t forward_pos = filepath.rfind("/");
	size_t back_pos = filepath.rfind("\\");

	if (back_pos == std::string::npos) {
		if (forward_pos == std::string::npos) {
			SNK_CORE_ERROR("GetFileDirectory with filepath '{0}' failed, no directory found", filepath);
			return filepath;
		}
		return filepath.substr(0, forward_pos);
	}
	else if (forward_pos == std::string::npos) {
		return filepath.substr(0, back_pos);
	}


	if (forward_pos > back_pos)
		return filepath.substr(0, forward_pos);
	else
		return filepath.substr(0, back_pos);
}

std::string GetFilename(const std::string& filepath) {
	size_t end = filepath.size() - 1;
	size_t forward_pos = filepath.rfind("/");
	size_t back_pos = filepath.rfind("\\");

	if (back_pos == std::string::npos) {
		if (forward_pos == std::string::npos) {
			// Filepath is just the filename
			return filepath;
		}
		return filepath.substr(forward_pos, end);
	}
	else if (forward_pos == std::string::npos) {
		return filepath.substr(back_pos, end);
	}


	if (forward_pos > back_pos)
		return filepath.substr(forward_pos, end);
	else
		return filepath.substr(back_pos, end);
}


bool PathEqualTo(const std::string& path1, const std::string& path2) {
	if (path1.empty() || path2.empty())
		return false;
	std::string c1 = path1;
	std::string c2 = path2;
	c1.erase(std::remove_if(c1.begin(), c1.end(), [](char c) { return c == '\\' || c == '/' || c == '.'; }), c1.end());
	c2.erase(std::remove_if(c2.begin(), c2.end(), [](char c) { return c == '\\' || c == '/' || c == '.'; }), c2.end());

	auto cur = std::filesystem::current_path().string();
	cur.erase(std::remove_if(cur.begin(), cur.end(), [](char c) { return c == '\\' || c == '/' || c == '.'; }), cur.end());

	return c1 == c2 || cur + c1 == c2 || c1 == cur + c2;
}

bool IsEntryAFile(const std::filesystem::directory_entry& entry) {
	try {
		return std::filesystem::is_regular_file(entry);
	}
	catch (std::exception& e) {
		SNK_CORE_ERROR("std::filesystem::is_regular_file err with path '{0}', '{1}'", entry.path().string(), e.what());
		return false;
	}
}


std::string GetFileLastWriteTime(const std::string& filepath) {
	std::string formatted = "";

	try {
		std::filesystem::file_time_type file_time = std::filesystem::last_write_time(filepath);
		auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(file_time);
		std::time_t time = std::chrono::system_clock::to_time_t(sys_time);

		char buffer[80];
		std::tm* time_info = nullptr;
		localtime_s(time_info, &time);
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", time_info);
		formatted = buffer;
	}
	catch (std::exception& e) {
		SNK_CORE_ERROR("std::filesystem::last_write_time error, '{0}'", e.what());
	}

	return formatted;
}

std::string ReplaceFileExtension(const std::string& filepath, const std::string& new_extension) {
	auto extension = filepath.substr(filepath.rfind('.'));
	std::string ret = filepath;
	StringReplace(ret, extension, new_extension);
	return ret;
}

bool WriteTextFile(const std::string& filepath, const std::string& content) {
	std::ofstream out{ filepath };

	if (!out.is_open()) {
		SNK_CORE_ERROR("Failed to open text file '{0}' for reading", filepath);
		return false;
	}

	out << content;
	out.close();

	return true;
}

bool WriteBinaryFile(const std::string& filepath, void* data, size_t size) {
	std::ofstream out{ filepath, std::ios::binary };

	if (!out.is_open()) {
		SNK_CORE_ERROR("Failed to open text file '{0}' for reading", filepath);
		return false;
	}

	out.write(reinterpret_cast<char*>(data), size);
	out.close();

	return true;
}

std::string ReadTextFile(const std::string& filepath) {
	std::stringstream stream;
	std::string line;
	std::ifstream file{ filepath };

	if (!file.is_open()) {
		SNK_CORE_ERROR("Failed to open text file '{0}' for reading", filepath);
		return "";
	}

	while (std::getline(file, line)) {
		stream << line << "\n";
	}

	return stream.str();
}

bool ReadBinaryFile(const std::string& filepath, std::vector<std::byte>& output) {

	std::ifstream file{ filepath, std::ios::binary | std::ios::ate };

	if (!file.is_open()) {
		SNK_CORE_ERROR("Failed to open binary file '{0}' for reading", filepath);
		return false;
	}

	auto file_size = file.tellg();

	output.resize(file_size);
	file.seekg(0, std::ios::beg);

	if (!file.read(reinterpret_cast<char*>(output.data()), file_size)) {
		SNK_CORE_ERROR("Failed to read binary file '{0}' after opening successfully", filepath);
		return false;
	}

	return true;
}

} // namespace files

void StripNonAlphaNumChars(std::string& str) {
	for (size_t i = 0; i < str.size(); i++) {
		if (!std::isalnum(str[i])) {
			str.erase(str.begin() + i);
			i--;
		}
	}
}
