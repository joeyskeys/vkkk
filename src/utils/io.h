#pragma once

#include "utils/common.h"

std::vector<char> load_file(const fs::path& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.good()) {
        std::cerr << "Failed open file : " << path << std::endl;
        return std::vector<char>();
    }

    size_t file_size = fs::file_size(path);
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    return buffer;
}