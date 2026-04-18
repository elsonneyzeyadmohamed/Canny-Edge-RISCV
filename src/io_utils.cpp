#include "image_types.hpp"
#include <fstream>
#include <iostream>

// Implementation to load the RAW file
bool load_raw(const std::string& path, Image& img) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open " << path << " for reading." << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(img.data.data()), img.data.size());
    
    if (file.gcount() != (std::streamsize)img.data.size()) {
        std::cerr << "Error: File size mismatch for " << path << std::endl;
        return false;
    }
    return true;
}

// Implementation to save the processed image
bool save_raw(const std::string& path, const Image& img) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open " << path << " for writing." << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(img.data.data()), img.data.size());
    return true;
}#include "image_types.hpp"
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
