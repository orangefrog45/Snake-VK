#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <filesystem>
#include <sstream>
#include <future>
#include <bitset>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <fstream>

bool IsShaderFile(const std::string& filepath) {
	return filepath.ends_with(".vert") ||
		filepath.ends_with(".frag") ||
		filepath.ends_with(".rgen") ||
		filepath.ends_with(".rchit") ||
		filepath.ends_with(".rmiss") ||
		filepath.ends_with(".comp");
}

struct ShaderFileData {
	std::string path;
	std::string content;
	std::vector<std::string> defines;
};

std::vector<std::string> SplitString(const std::string& input, const std::string& split_val) {
	std::vector<std::string> ret;
	if (split_val.empty())
		return ret;

	size_t last_split_location = 0;
	size_t split_val_location = input.find(split_val);
	while (split_val_location != std::string::npos) {
		ret.push_back(input.substr(last_split_location, split_val_location - last_split_location));
		last_split_location = split_val_location + split_val.size();
		split_val_location = input.find(split_val, last_split_location);
	}

	ret.push_back(input.substr(last_split_location));

	return ret;
}

ShaderFileData ReadShaderFile(const std::string& filepath) {
	ShaderFileData ret;
	ret.path = filepath;

	std::ifstream f(filepath);
	std::stringstream s;

	std::string line;

	// Get version line
	std::getline(f, line);
	s << line;

	// Read shader permutation line (if it exists)
	std::getline(f, line);
	if (line.starts_with("#define SNAKE_PERMUTATIONS")) {
		auto args = line.substr(line.find('(') + 1, line.find(')') - line.find('(') - 1);
		ret.defines = SplitString(args, ",");
	}
	else {
		s << line << "\n";
	}

	while (std::getline(f, line)) {
		s << line << "\n";
	}

	ret.content = s.str();
	return ret;
}

std::string g_errors;

void CompilePermutation(const ShaderFileData& data, uint8_t bits, const std::string& output_path) {
	std::string define_str;
	for (uint32_t i = 0; i < data.defines.size(); i++) {
		bool define_active = (bits & (1 << i));
		if (define_active)
			define_str += "-D" + data.defines[i] + " ";
	}

	std::bitset<8> b = bits;
	auto bit_str = b.to_string();
	// Reverse bits for more intuitive filename (each bit corresponds in the same order to a define in SNAKE_PERMUTATIONS(...))
	std::ranges::reverse(bit_str);

	auto cmd = std::format("%VULKAN_SDK%/Bin/glslc.exe --target-env=vulkan1.3 {} {} -o {}", define_str, data.path, output_path + "_" + bit_str + ".spv 2>&1");

	// Errors go here
	static std::array<char, 20'000> output_buffer;

	std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
	while (fgets(output_buffer.data(), output_buffer.size(), pipe.get()) != nullptr) {
		g_errors += output_buffer.data();
	}

	std::cout << cmd << "\n";
}

std::vector<std::future<void>> g_futures;

void CompilePermutations(const ShaderFileData& data, const std::string& base_output_path) {
	size_t num_unique_permutations = (size_t)pow(2, data.defines.size());

	if (num_unique_permutations > std::numeric_limits<uint8_t>::max()) {
		std::cout << std::format("Error: shader '{}' has num_unique_permutations={}, max supported = {}", data.path, num_unique_permutations, std::numeric_limits<uint8_t>::max());
		exit(1);
	}

	for (uint8_t i = 0; i < num_unique_permutations; i++) {
		CompilePermutation(data, i, base_output_path);
	}

}

int main() {
	while (true) {
		std::array<std::string, 1> shader_directories = { THIS_DIR "/../Core/res/shaders" };

		for (const auto& dir : shader_directories) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
				std::string path = entry.path().string();
				if (IsShaderFile(path)) {

					g_futures.push_back(std::async(std::launch::async, [=] {
						std::string filename = path.substr(path.rfind("\\") + 1);
						filename.erase(filename.rfind('.'), 1);
						std::string output_path = dir + "/spv/" + filename;
						CompilePermutations(ReadShaderFile(path), output_path);
					}));

				}
			}
		}

		for (auto& future : g_futures) {
			future.wait();
		}

		g_futures.clear();

		std::array<std::string, 2> output_directories = {
			THIS_DIR "/../out/build/x64-Debug/Editor/res/shaders/",
			THIS_DIR "/../out/build/x64-Release/Editor/res/shaders/",
		};

		for (int i = 0; i < output_directories.size(); i++) {
			try {
				if (!std::filesystem::exists(shader_directories[0] + "/spv"))
					std::filesystem::create_directory(shader_directories[0] + "/spv");

				std::filesystem::copy(shader_directories[0] + "/spv/", output_directories[i], 
					std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
			}
			catch ([[maybe_unused]] std::exception& e) {
				continue;
			}
		}

		if (!g_errors.empty()) {
			std::cout << "\n\n\nSOME SHADERS FAILED TO COMPILE:\n";
			std::cout << g_errors;
			g_errors.clear();
		}
		else {
			std::cout << "\n\n\nALL SHADERS SUCCESSFULLY COMPILED\n";
		}


		std::cout << "\n\nPRESS ANY KEY + ENTER TO CONTINUE";
		std::string temp;
		std::cin >> temp;
	}

	return 0;
}