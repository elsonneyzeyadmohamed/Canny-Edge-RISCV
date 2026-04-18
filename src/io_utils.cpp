#include "image_types.hpp"
#include <fstream>

bool load_raw(const std::string& path, Image& img) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.read(reinterpret_cast<char*>(img.data.data()), img.data.size());
    return file.good();
}

bool save_raw(const std::string& path, const Image& img) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(img.data.data()), img.data.size());
    return file.good();
}
