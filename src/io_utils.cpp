#include "image_types.hpp"
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

// Function to load the RAW image file
bool load_raw(const string& path, Image& img) {
    ifstream file(path, ios::binary);
    if (!file) {
        cerr << "Error: Could not open " << path << " for reading." << endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(img.data.data()), img.data.size());
    
    if (file.gcount() != (streamsize)img.data.size()) {
        cerr << "Error: File size mismatch for " << path << endl;
        return false;
    }
    return true;
}

// Function to save the processed image
bool save_raw(const string& path, const Image& img) {
    ofstream file(path, ios::binary);
    if (!file) {
        cerr << "Error: Could not open " << path << " for writing." << endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(img.data.data()), img.data.size());
    return file.good();
}