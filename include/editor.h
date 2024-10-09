#include <string>
#include <vector>
enum open_status
{
    OPEN_FILE_FAILURE,
    OPEN_FILE_SUCCESS
};
const std::vector<std::string> &get_file();
const char *const &get_filename();
open_status open_file(const char *const &);
void save_file();
void write_log(std::string);
void insert_char(int &row, int &col, char ch);
void erase_char(int &row, int &col);
void new_line(const int &row, const int &col);
void erase_row(int &row);
bool file_saved();