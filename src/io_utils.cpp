#include "image_types.hpp"
#include <cstdio>
#include <iostream>
#include <string>

using namespace std;

/**
 * Loads a raw grayscale image from the specified path.
 * Returns true if successful, false otherwise.
 */
bool load_raw(const string& path, Image& img) {
    FILE* file = fopen(path.c_str(), "rb");
    
    if (!file) {
        cerr << "Error: Could not open file for reading: " << path << endl;
        return false;
    }

    // Read pixel data into the image buffer
    size_t read_size = fread(img.data.data(), 1, img.data.size(), file);
    
    if (read_size != img.data.size()) {
        cerr << "Error: Size mismatch while reading " << path << endl;
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

/**
 * Saves a raw grayscale image to the specified path.
 * Returns true if successful, false otherwise.
 */
bool save_raw(const string& path, const Image& img) {
    FILE* file = fopen(path.c_str(), "wb");
    
    if (!file) {
        cerr << "Error: Could not open file for writing: " << path << endl;
        return false;
    }

    // Write pixel data from the image buffer
    size_t write_size = fwrite(img.data.data(), 1, img.data.size(), file);
    
    if (write_size != img.data.size()) {
        cerr << "Error: Failed to write all data to " << path << endl;
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}