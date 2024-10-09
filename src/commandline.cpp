// this cpp manages with terminal I/O
#include <ncurses.h>
#include "editor.h"
#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

bool read_only, truncate_mode, is_scroll;
constexpr int NEWLINE = 10;    // the ascii of enter
constexpr int ESC = 27;        // the ascii of esc
constexpr int BACKSPACE = 263; // not the ascii of backspace, very strange
const char* mode_name[] = { "", "Normal Mode", "Insert Mode", "Command Mode" };
constexpr int ROW_WINDOW_SIZE = 5;
enum mode {
    QUIT,
    NORMAL_MODE,
    INSERT_MODE,
    COMMAND_MODE
} cur_mode;
int row, col;         // current cursor position (absolute, not relative to file_window)
int win_row, win_col; // currnet window left_up corner
WINDOW* file_window, * info_window, * command_window, * row_window;

inline int col_limit() {
    if (cur_mode == NORMAL_MODE) {
        return std::max(0, static_cast<int>(get_file()[row].size()) - 1);
    } else {
        return static_cast<int>(get_file()[row].size());
    }
}

void refresh_file_display() {
    int row_max, col_max;
    getmaxyx(file_window, row_max, col_max);
    wclear(file_window);
    const std::vector<std::string>& file = get_file();
    for (int i = 0; i < std::min(row_max, static_cast<int>(file.size()) - win_row); ++i) {

        if (file[win_row + i].size() > win_col) {
            mvwprintw(file_window, i, 0, "%s", file[win_row + i].substr(win_col, col_max).c_str());
        }
    }
    wmove(file_window, row - win_row, col - win_col);
    wrefresh(file_window);
}

void init_tui() {
    initscr(); /* Start curses mode */
    raw();     /* Make Input Instantly Available*/
    keypad(stdscr, TRUE);
    noecho();

    box(stdscr, 0, 0);
    refresh();

    file_window = newwin(LINES - 6, COLS - 2 - ROW_WINDOW_SIZE, 1, 1 + ROW_WINDOW_SIZE);
    row_window = newwin(LINES - 6, ROW_WINDOW_SIZE, 1, 1);
    info_window = newwin(3, COLS - 2, LINES - 5, 1);
    command_window = newwin(1, COLS - 2, LINES - 2, 1);

    mvwhline(info_window, 0, 0, '-', COLS - 2);
    mvwhline(info_window, 2, 0, '-', COLS - 2);

    win_row = win_col = row = col = 0;
    wrefresh(info_window);
    wrefresh(command_window);
    refresh_file_display();
}

void destroy_tui() {
    delwin(file_window);
    delwin(info_window);
    delwin(command_window);
    delwin(row_window);
    endwin();
}

void upd_info() {
    mvwhline(info_window, 1, 0, ' ', getmaxx(info_window));
    if (cur_mode == NORMAL_MODE && get_file()[row].size() == 0) {
        mvwprintw(info_window, 1, 0, "Mode:%s  File:%s    Line%d, Col0-1", mode_name[cur_mode], get_filename(), row + 1);
    } else {
        mvwprintw(info_window, 1, 0, "Mode:%s  File:%s    Line%d, Col%d    WinRow%d, WinCol%d", mode_name[cur_mode], get_filename(), row + 1, col + 1, win_row + 1, win_col + 1);
    }
    wrefresh(info_window);
}

void adjust_window_col() {
    int col_max = getmaxx(file_window);
    if (col < win_col)
        win_col = col;
    else if (col - win_col >= col_max)
        win_col = col - col_max + 1;
}

void mv_begin() {
    col = 0;
}

void mv_end() {
    col = col_limit();
}

void mv_direction(int ch, bool prev_is_up_down) {
    int row_max, col_max;
    static int memorized_col;
    getmaxyx(file_window, row_max, col_max);
    std::vector<std::string> file = get_file();
    switch (ch) {
    case KEY_UP:
        if (!prev_is_up_down) {
            memorized_col = col;
        }
        if (row == win_row && win_row > 0) { // move window
            --win_row;
        }
        if (row > 0) { // change line
            --row;
            col = std::min(memorized_col, col_limit());
        }
        break;
    case KEY_DOWN:
        if (!prev_is_up_down) {
            memorized_col = col;
        }
        if (row - win_row == row_max - 1 && row < file.size() - 1) { // move window
            ++win_row;
        }
        if (row < file.size() - 1) { // change line
            ++row;
            col = std::min(memorized_col, col_limit());
        }
        break;
    case KEY_LEFT:
        if (win_col == 0 && col == 0 && row > 0) {
            mv_direction(KEY_UP, true);
            mv_end();
            break;
        }
        if (win_col == col && win_col > 0) {
            --win_col;
        }
        if (col > 0) {
            --col;
        }
        break;
    case KEY_RIGHT:
        if (col == col_limit()) {
            if (row != file.size() - 1) {
                mv_direction(KEY_DOWN, true);
                mv_begin();
            }
            break;
        }
        ++col;
        if (col - win_col == col_max) {
            ++win_col;
        }
        break;
    }
}

void mv_prev_word() {
    if (col == 0) {
        mv_direction(KEY_LEFT, true);
        return;
    }
    std::vector<std::string> file = get_file();
    if (file[row][col] != ' ' && file[row][col - 1] != ' ') { // inside a word
        while (col > 0 && file[row][col - 1] != ' ') {
            --col;
        }
    } else {
        do {
            --col;
        } while (col > 0 && file[row][col] == ' ');
        if (col != 0) {
            while (col > 0 && file[row][col - 1] != ' ') {
                --col;
            }
        }
    }
}

void mv_next_word() {
    std::vector<std::string> file = get_file();
    if (col == col_limit()) {
        mv_direction(KEY_RIGHT, true);
        return;
    }
    while (col < static_cast<int>(file[row].size()) - 1 && file[row][col] != ' ') {
        ++col;
    }
    while (col < static_cast<int>(file[row].size()) - 1 && file[row][col] == ' ') {
        ++col;
    }
}

void normal_mode() {
    if (col > col_limit()) // back from insert mode
    {
        col = col_limit();
        wmove(file_window, row - win_row, col - win_col);
    }
    wrefresh(file_window); // enter mode, make cursor visible
    bool prev_is_up_down = false;
    for (int ch = getch();; ch = getch()) {
        switch (ch) { // all these functions inside the switch only changes row, col, win_row, win_col
            // they have nothing to do with display
        case 'i':
            cur_mode = INSERT_MODE;
            return;
        case ':':
            cur_mode = COMMAND_MODE;
            return;
        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
            mv_direction(ch, prev_is_up_down);
            break;
        case '0':
            mv_begin();
            break;
        case '$':
            mv_end();
            break;
        case 'b':
            mv_prev_word();
            break;
        case 'w':
            mv_next_word();
            break;
        default:
            continue;
        }
        prev_is_up_down = (ch == KEY_UP || ch == KEY_DOWN);
        adjust_window_col();
        upd_info();
        refresh_file_display();
    }
}

void insert_mode() {
    wrefresh(file_window); // enter mode, make cursor visible
    bool prev_is_up_down = false;
    for (int ch = getch();; ch = getch()) {
        switch (ch) { // all these functions inside the switch calls file-changers in editor.hpp
            // they have nothing to do with display
        case ESC:
            cur_mode = NORMAL_MODE;
            return;
        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
            mv_direction(ch, prev_is_up_down);
            break;
        case '\t':
            for (int i = 0; i < 4; ++i) {
                insert_char(row, col, ' ');
            }
            break;
        case '\n':
            new_line(row, col);
            mv_direction(KEY_RIGHT, true);
            break;
        case BACKSPACE:
            erase_char(row, col);
            break;
        default:
            insert_char(row, col, static_cast<char>(ch));
        }
        prev_is_up_down = (ch == KEY_UP || ch == KEY_DOWN);
        adjust_window_col();
        upd_info();
        refresh_file_display();
    }
}

void command_display(std::string msg) {
    wclear(command_window);
    mvwprintw(command_window, 0, 0, msg.c_str());
    wrefresh(command_window);
    wrefresh(file_window); // make sure cursor stays in file_window
    std::this_thread::sleep_for(std::chrono::seconds(1));
    wclear(command_window);
    wrefresh(command_window);
    wrefresh(file_window); // make sure cursor stays in file_window
}

void command_mode() {
    wrefresh(command_window);
    static std::thread th;
    if (th.joinable()) {
        th.join();
    }
    mvwprintw(command_window, 0, 0, ":");
    wrefresh(command_window);
    std::string str;
    int ch;
    while (ch = getch(), ch != NEWLINE && ch != ESC) {
        wechochar(command_window, ch);
        str += ch;
    }
    if (ch == ESC) {
        mvwhline(command_window, 0, 0, ' ', getmaxx(command_window));
        wrefresh(command_window);
        cur_mode = NORMAL_MODE;
        return;
    }
    if (str == "w") {
        save_file();
        th = std::thread(command_display, "File saved!");
    } else if (str == "q!") {
        cur_mode = QUIT;
    } else if (str == "wq") {
        save_file();
        cur_mode = QUIT;
    } else if (str == "q" || str == "Q") {
        if (file_saved()) {
            cur_mode = QUIT;
        } else {
            th = std::thread(command_display, "File not saved!");
            cur_mode = NORMAL_MODE;
        }
    } else {
        mvwhline(command_window, 0, 0, ' ', getmaxx(command_window));
        wrefresh(command_window);
        cur_mode = NORMAL_MODE;
    }
}

void parse_arguments(const int& argc, const char**& argv) {
    if (argc == 1) {
        printf("Usage: %s <file name>\n", argv[0]);
        exit(1);
    }
    if (argc != 2) {
        std::cout << "Optional arguments still under construction!" << std::endl;
        exit(1);
    }

    // TODO: truncate mode
    if (open_file(argv[1]) == OPEN_FILE_FAILURE) {
        printf("File %s not found, new file function not added yet\n", argv[1]);
        exit(1);
    }
}

int main(int argc, const char* argv[]) {
    parse_arguments(argc, argv);

    // write_log("opened file success");
    init_tui();
    // std::cout << getch() << std::endl;
    // getch();
    cur_mode = NORMAL_MODE;
    do {
        upd_info();
        switch (cur_mode) {
        case NORMAL_MODE:
            normal_mode();
            break;
        case INSERT_MODE:
            insert_mode();
            break;
        case COMMAND_MODE:
            command_mode();
            break;
        default:
            write_log("error: wrong next mode return value");
            return 1;
        }
    } while (cur_mode != QUIT);
    destroy_tui();
    return 0;
}