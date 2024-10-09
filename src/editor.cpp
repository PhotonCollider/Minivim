// this cpp provides functions to be called by commandline.cpp, and handles text changes
#include <fstream>
#include <editor.h>
#include <vector>
#include <string>

bool saved;
const char* filename;
const char* pathname;
std::vector<std::string> file;
std::ofstream fo; // log

const std::vector<std::string>& get_file() {
    return file;
}

const char* const& get_filename() {
    return filename;
}

open_status open_file(const char* const& _pathname) {
    std::ifstream fs;
    fs.open(_pathname, std::ifstream::in);
    if (!fs) {
        return OPEN_FILE_FAILURE;
    }
    pathname = _pathname;
    filename = _pathname;
    while (*filename) {
        ++filename;
    }
    while (filename >= _pathname && *filename != '/') {
        --filename;
    }
    ++filename;

    saved = true;
    std::string str;
    while (getline(fs, str)) {
        file.push_back(str);
    }
    fs.close();
    if (file.empty()) {
        file.push_back(std::string(""));
    }
    return OPEN_FILE_SUCCESS;
}

void write_log(std::string str) {
    if (!fo.is_open()) {
        fo.open("log.txt", std::ofstream::out);
    }
    fo << str << std::endl;
}

void save_file() {
    saved = true;
    std::ofstream ofs;
    ofs.open(pathname, std::ofstream::out);
    if (file.size() == 1 && file[0].empty()) {
        ofs.close();
        return;
    }
    for (std::string str : file) {
        ofs << str << std::endl;
    }
    ofs.close();
}

void insert_char(int& row, int& col, char ch) {
    saved = false;
    if (col > file[row].size()) {
        write_log("ERROR: trying to input beyong line limit");
        return;
    }
    file[row].insert(file[row].begin() + col, ch);
    ++col;
}

void erase_char(int& row, int& col) {
    if (col == 0) {
        if (row > 0) {
            saved = false;
            col = file[row - 1].size();
            file[row - 1] += file[row];
            file.erase(file.begin() + row);
            --row;
        }
    } else {
        saved = false;
        file[row].erase(file[row].begin() + col - 1);
        --col;
    }
}

void new_line(const int& row, const int& col) {
    saved = false;
    file.insert(file.begin() + row + 1, file[row].substr(col));
    file[row] = file[row].substr(0, col);
}

bool file_saved() {
    return saved;
}