
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>
#include <unistd.h>     // STDIN_FILENO

/*
 * TODO: `filepath` leaks memory when we save a file.
 */

int REAL_SCREEN_WIDTH = 80;
int SCREEN_WIDTH = 80;
int SCREEN_HEIGHT = 24;

struct line {
    char* str;
    int allocation_size;
    int len;
    struct line* next;
};

struct line* doc_head;
struct line* doc_tail;
char* filepath = NULL;
int cursor_pos = 0;
int cursor_line = 0;
int scroll = 0;
bool dirty = false;
int num_lines = 0;
int scroll_mode = 2;    // 0 = smooth, 1 = half-pages, 2 = full pages

uint8_t* double_buffer_chars;
uint8_t* double_buffer_cols;
uint8_t* prev_chars;
uint8_t* prev_cols;
char* scrprintf_buffer;
int double_buffer_cur_x = 0;
int double_buffer_cur_y = 0;
bool double_buffer_header = false;
int programmers_newline = 0;
int prog_col = 0;
int prog_col_len = 0;

static void ScrPutchar(char c) {
    int index = double_buffer_cur_y * REAL_SCREEN_WIDTH + double_buffer_cur_x;
    if (c != '\n') {
        double_buffer_chars[index] = c;
        if ((programmers_newline == 2 || programmers_newline == 3) && !double_buffer_header) {
            if (isdigit(c)) {
                double_buffer_cols[index] = 2;
            } else if (c == '\'' || c == '"') {
                double_buffer_cols[index] = 5;
            } else if (ispunct(c)) {
                double_buffer_cols[index] = 4;
            } else {
                double_buffer_cols[index] = 0;
            }

        } else {
            double_buffer_cols[index] = double_buffer_header;
        }
    }
    if (c == '\n' || double_buffer_cur_x >= REAL_SCREEN_WIDTH) {
        double_buffer_cur_x = 0;
        double_buffer_cur_y++;
    } else {
        ++double_buffer_cur_x;
    }
}

static void AnsiSetCursor(int x, int y) {
    printf("\x1B[%d;%dH", y + 1, x + 1);
}

static void ScrFlush(void) {
    AnsiSetCursor(0, 0);

    uint8_t col = false;
    bool cursor_in_place = false;
    for (int i = 0; i < SCREEN_HEIGHT * REAL_SCREEN_WIDTH; ++i) {
        bool same = double_buffer_chars[i] == prev_chars[i] && 
                    double_buffer_cols[i] == prev_cols[i];

        if (!same) {
            if (!cursor_in_place) {
                AnsiSetCursor(i % REAL_SCREEN_WIDTH, i / REAL_SCREEN_WIDTH);
            }

            if (double_buffer_cols[i] != col) {
                switch (double_buffer_cols[i]) {    
                case 0: printf("\x1B[0m"); break;               // normal
                case 1: printf("\x1B[107m\x1B[30m"); break;     // header
                case 2: printf("\x1B[40m\x1B[92m"); break;      // l. green
                case 3: printf("\x1B[40m\x1B[32m"); break;      // d. green
                case 4: printf("\x1B[40m\x1B[95m"); break;      // pink
                case 5: printf("\x1B[40m\x1B[31m"); break;      // red
                case 6: printf("\x1B[40m\x1B[93m"); break;      // yellow
                case 7: printf("\x1B[40m\x1B[34m"); break;      // blue
                }
                col = double_buffer_cols[i];
            }
            
            if (double_buffer_chars[i]) {
                putchar(double_buffer_chars[i]);
            } else {
                putchar(' ');
            }
            cursor_in_place = true;
        } else {
            cursor_in_place = false;
        }
    }
    if (col) {
        printf("\x1B[0m");
    }
    fflush(stdout);

    memcpy(prev_chars, double_buffer_chars, SCREEN_HEIGHT * REAL_SCREEN_WIDTH);
    memcpy(prev_cols, double_buffer_cols, SCREEN_HEIGHT * REAL_SCREEN_WIDTH);
}

static void TruelyWipeScreen() {
    printf("\x1B[2J\x1B[1;1H");
    double_buffer_cur_x = 0;
    double_buffer_cur_y = 0;
    memset(double_buffer_chars, 0, REAL_SCREEN_WIDTH * SCREEN_HEIGHT);
    memset(double_buffer_cols, 0, REAL_SCREEN_WIDTH * SCREEN_HEIGHT);
    memset(prev_chars, 0, REAL_SCREEN_WIDTH * SCREEN_HEIGHT);
    memset(prev_cols, 0, REAL_SCREEN_WIDTH * SCREEN_HEIGHT);
}

static void AnsiClear(void) {
    double_buffer_cur_x = 0;
    double_buffer_cur_y = 0;
    memset(double_buffer_chars, 0, REAL_SCREEN_WIDTH * SCREEN_HEIGHT);
    memset(double_buffer_cols, 0, REAL_SCREEN_WIDTH * SCREEN_HEIGHT);
}

static void AnsiReset(void) {
    double_buffer_header = false;
}

static void AnsiHeader(void) {
    double_buffer_header = true;
}

static void ScrPrintf(const char* format, ...) {
    va_list list;
	va_start(list, format);
    vsprintf(scrprintf_buffer, format, list);
	va_end(list);

    for (int i = 0; scrprintf_buffer[i]; ++i) {
        ScrPutchar(scrprintf_buffer[i]);
    }
}

char line_num_format[16];
char line_num_blank[16];
bool line_num_enable = false;

static struct line* CreateLine(int default_size) {
    struct line* l = malloc(sizeof(struct line));
    if (l == NULL) {
        abort();
    }
    l->allocation_size = default_size;
    l->next = NULL;
    l->len = 0;
    l->str = calloc(l->allocation_size, 1);
    if (l->str == NULL) {
        abort();
    }
    return l;
}

/* 
 * -1 for the last line.
 */
static struct line* GetLine(int num) {
    if (num == -1) {
        return doc_tail;
    }
    struct line* l = doc_head;
    while (num--) {
        if (l->next == NULL) {
            return NULL;
        }
        l = l->next;
    }
    return l;
}

/* 
 * Puts the character before the index `pos` -> i.e. the inserted character
 * will now be at this index. If -1 is passed in, it will be inserted at the
 * end.
 */
static void AddCharToLine(struct line* l, int pos, char c) {
    /* 
     * Ensure we have enough room, and that the unused part of the buffer is
     * entirely full of NULLs. Need a `+ 1` to ensure we don't overwrite the
     * last NULL.
     */
    if (l->len + 1 >= l->allocation_size) {
        l->allocation_size *= 2;
        char* new_line = calloc(l->allocation_size, 1);
        if (new_line == NULL) {
            abort();
        }
        memset(new_line, 0, l->allocation_size);
        memcpy(new_line, l->str, l->len);
        free(l->str);
        l->str = new_line;
    }

    if (pos == -1) {
        l->str[l->len] = c;
    } else {
        memmove(l->str + pos + 1, l->str + pos, l->len - pos);
        l->str[pos] = c;
    }
    l->len++;
}

static void MergeLineWithPrevious(int num) {
    struct line* prev = GetLine(num - 1);
    struct line* curr = prev->next;

    for (int i = 0; i < curr->len; ++i) {
        AddCharToLine(prev, -1, curr->str[i]);
    }

    prev->next = curr->next;
    if (curr == doc_tail) {
        prev = doc_tail;
    }
    free(curr->str);
    free(curr);

    --num_lines;
}

static void RemoveCharacterFromLine(struct line* l, int pos) {
    memmove(l->str + pos, l->str + pos + 1, l->len - pos - 1);
    l->str[l->len - 1] = 0;
    l->len--;
}

static int LoadFile(char* path) {
    errno = 0;
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return errno;
    }

    int c;
    struct line* l = doc_head;
    
    while ((c = fgetc(f)) != EOF) {
        if (c != '\n') {
            AddCharToLine(l, -1, c);
        } else {
            ++num_lines;
            l->next = CreateLine(50);
            l = l->next;
            doc_tail = l;
        }
    }
    doc_tail = l;

    fclose(f);

    filepath = path;
    dirty = false;
    return 0;
}

static int GetWrappedHeightOfLine(struct line* l) {
    return l->len / (SCREEN_WIDTH - 2) + 1;
}

static void Init(void) {
    double_buffer_chars = calloc(REAL_SCREEN_WIDTH * SCREEN_HEIGHT, 1);
    double_buffer_cols = calloc(REAL_SCREEN_WIDTH * SCREEN_HEIGHT, 1);
    prev_chars = calloc(REAL_SCREEN_WIDTH * SCREEN_HEIGHT, 1);
    prev_cols = calloc(REAL_SCREEN_WIDTH * SCREEN_HEIGHT, 1);
    scrprintf_buffer = malloc(REAL_SCREEN_WIDTH * SCREEN_HEIGHT + 512);

    TruelyWipeScreen();

    dirty = false;
    scroll = 0;
    cursor_line = 0;
    cursor_pos = 0;
    filepath = NULL;
    SCREEN_WIDTH = REAL_SCREEN_WIDTH;
    
    AnsiReset();
    doc_head = CreateLine(50);
    doc_tail = doc_head;
    num_lines = 1;

    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON);

    /* 
     * Prevent the default behaviour of CTRL+S / CTRL+O.
     */
    //raw.c_cc[VSTOP] = _POSIX_VDISABLE;
    //raw.c_cc[VSUSP] = _POSIX_VDISABLE;
    //raw.c_cc[VDISCARD] = _POSIX_VDISABLE;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void ClearFile(void) {
    struct line* l = doc_head;
    while (l != NULL) {
        struct line* next = l->next;
        free(l->str);
        free(l);
        l = next;
    }

    // TODO: are there other things that need to be freed?

    Init();
}

static int GetLineAtTopOfScroll(int* lines_in) {
    int total_lines_in = 0;
    int line_num = 0;
    struct line* l = doc_head;
    while (true) {
        if (total_lines_in >= scroll) {
            break;
        }
        total_lines_in += GetWrappedHeightOfLine(l);
        l = l->next;
        if (l->next == NULL) {
            break; 
        }
        ++line_num;
    }
    *lines_in = total_lines_in - scroll;
    if (line_num == 0) {
        return 0;
    }
    return (*lines_in) == 0 ? line_num : line_num - 1;
}

static int GetTotalWrappedHeight(void) {
    int y = 0;
    struct line* l = doc_head;
    while (l != NULL) {
        y += GetWrappedHeightOfLine(l);
        l = l->next;
    }
    return y;
}

static int GetCursorLineWrapped(void) {
    int y = 0;
    struct line* l = doc_head;
    int n = cursor_line;
    while (n--) {
        y += GetWrappedHeightOfLine(l);
        l = l->next;
    }
    return y;
}

static bool Putchar(char c, int x, int y, int clr) {
    int cur_x = cursor_pos % (SCREEN_WIDTH - 2);
    int cur_y = (clr + cursor_pos / (SCREEN_WIDTH - 2)) - scroll;
    bool on_cursor = (cur_x == x && cur_y == y) || (x == -1 && y == -1);
    if (on_cursor) {
        AnsiHeader();
    }
    ScrPutchar(c);
    if (on_cursor) {
        AnsiReset();
    }
    return on_cursor;
}

static void DrawDocumentBody(void) {
    int lines_into_scroll;
    int scroll_line = GetLineAtTopOfScroll(&lines_into_scroll);
    struct line* l = GetLine(scroll_line);
    int x = 0;
    int y = 0;
    int line = scroll_line;
    int clr = GetCursorLineWrapped();
    while (y < SCREEN_HEIGHT - 2) {
        if (l != NULL) {
            bool disp_cur = false;
            if (line_num_enable) ScrPrintf(line_num_format, line + 1);

            /*
             * May need to start partway through the line if this is a long line
             * that is partially scrolled off screen.
             */
            int start_pos = 0;
            if (y == 0) {
                start_pos = lines_into_scroll * (SCREEN_WIDTH - 2);
            }

            for (int i = start_pos; i < l->len; ++i) {
                disp_cur |= Putchar(l->str[i], x, y, clr);
                ++x;
                if (x == SCREEN_WIDTH - 2) {
                    x = 0;
                    ++y;
                    if (y >= SCREEN_HEIGHT - 2) break;
                    ScrPutchar('\n');
                    if (line_num_enable) ScrPrintf(line_num_blank, "");
                }
            }
            if (cursor_pos == l->len && line == cursor_line) {
                /* i.e. cursor is at end of line */
                Putchar(' ', -1, -1, clr);
            }
            ++y;
            x = 0;
            ScrPutchar('\n');
            ++line;
            l = l->next;
        } else {
            if (line < num_lines) {
                if (line_num_enable) ScrPrintf(line_num_format, line);
            }
            ScrPutchar('\n');
            x = 0;
            ++line;
            ++y;
        }
    }
}

static void DrawScreen(void) {
    AnsiReset();
    AnsiClear();
    AnsiHeader();
    char format_str[32];
    sprintf(format_str, "%%c%%-%ds\n", REAL_SCREEN_WIDTH - 2);
    ScrPrintf(format_str, dirty ? '*' : ' ', filepath == NULL ? "" : filepath);
    AnsiReset();
    
    DrawDocumentBody();

    AnsiHeader();
    sprintf(format_str, "%%-%ds", REAL_SCREEN_WIDTH - 2);
    char out[128];
    sprintf(out, "line %d / %d, col %d. %c%c%c%c",
        cursor_line + 1, num_lines, cursor_pos + 1, 
        scroll_mode == 0 ? 'S' : scroll_mode == 1 ? 'H' : 'F',
        programmers_newline != 0 ? '*' : ' ',
        (programmers_newline == 2 || programmers_newline == 4) ? '}' : ' ',
        (programmers_newline == 3 || programmers_newline == 4) ? ':' : ' '
        );
    ScrPrintf(format_str, out);
    ScrFlush();
    AnsiReset();
}

static void CursorLeft(void) {
    cursor_pos--;
    if (cursor_pos < 0) {
        cursor_line--;
        if (cursor_line < 0) {
            cursor_line = 0;
            cursor_pos = 0;
        } else {
            cursor_pos = GetLine(cursor_line)->len;
        }
    }
}

static void CursorRight(void) {
    cursor_pos++;
    if (cursor_pos > GetLine(cursor_line)->len) {
        if (GetLine(cursor_line)->next) {
            cursor_pos = 0;
            ++cursor_line;
        } else {
            cursor_pos = GetLine(cursor_line)->len;
        }
    }
}

static void CursorDown(void) {
    if (GetLine(cursor_line)->next != NULL) {
        ++cursor_line;
        if (cursor_pos > GetLine(cursor_line)->len) {
            cursor_pos = GetLine(cursor_line)->len;
        }
    } else {
        cursor_pos = GetLine(cursor_line)->len;
    }
}

static void CursorUp(void) {
    if (cursor_line > 0) {
        --cursor_line;
        if (cursor_pos > GetLine(cursor_line)->len) {
            cursor_pos = GetLine(cursor_line)->len;
        }
    } else {
        cursor_pos = 0;
    }
}

static void FixupLineNumberWidth(void) {
    if (line_num_enable) {
        int digits = 0;
        if (num_lines < 1000) {
            digits = 3;
        } else {
            int lines = num_lines;
            do {
                ++digits;
                lines /= 10;
            } while (lines > 0);
        }
        sprintf(line_num_format, "%%%dd| ", digits);
        sprintf(line_num_blank, "%%%ds| ", digits);
        SCREEN_WIDTH = REAL_SCREEN_WIDTH - (digits + 2);
    } else {
        SCREEN_WIDTH = REAL_SCREEN_WIDTH;
    }
}

static void ToggleLineNumbers(void) {
    line_num_enable = !line_num_enable;
    FixupLineNumberWidth();
}

static void InsertNewline(void) {
    struct line* current = GetLine(cursor_line);
    int chars_remaining = current->len - cursor_pos;
    struct line* newline = CreateLine(chars_remaining + 20);

    int num_spaces = 0;
    if (programmers_newline) {
        for (int i = 0; current->str[i]; ++i) {
            if (current->str[i] == ' ') {
                ++num_spaces;
            } else {
                break;
            }
        }

        if (programmers_newline == 2 || programmers_newline == 4) {
            if (current->str[current->len - 1] == '{') {
                num_spaces += 4;
            }
        }
        if (programmers_newline == 3 || programmers_newline == 4) {
            if (current->str[current->len - 1] == ':') {
                num_spaces += 4;
            }
        }
    }
    for (int i = 0; i < num_spaces; ++i) {
        newline->str[i] = ' ';
    }
    strcpy(newline->str + num_spaces, current->str + cursor_pos);
    newline->len = chars_remaining + num_spaces;

    memset(current->str + cursor_pos, 0, current->allocation_size - cursor_pos);
    current->len = cursor_pos;
    cursor_pos = num_spaces;
    ++cursor_line;
    ++num_lines;
    newline->next = current->next;
    current->next = newline;
    if (doc_tail == current) {
        doc_tail = newline;
    }
    FixupLineNumberWidth();
}

static void SaveFailed(int err) {
    TruelyWipeScreen();
    AnsiClear();
    AnsiReset();
    ScrPrintf("File could not be saved due to the following error:\n\n    %s\n\nPress any key to continue...", strerror(err));
    ScrFlush();
    getchar();
    return;
}

int GetSingleLineInput(struct line* save_line, const char* prompt) {
    TruelyWipeScreen();
    int save_cur = save_line->len;

    while (true) {
        AnsiClear();
        AnsiHeader();
        ScrPrintf("%s", prompt);
        for (int i = 0; i < REAL_SCREEN_WIDTH - 1 - (int) strlen(prompt); ++i) {
            if (i == save_cur) {
                AnsiReset();
            }
            if (i >= save_line->len) {
                ScrPutchar(' ');
            } else {
                ScrPutchar(save_line->str[i]);
            }
            if (i == save_cur) {
                AnsiHeader();
            }
        }
        AnsiReset();
        ScrPutchar('\n');
        DrawDocumentBody();
        ScrFlush();

        char c = getchar();

        if (c == '\x1B') {
            c = getchar();
            if (c == '[') {
                c = getchar();
                while (isdigit(c)) {
                    c = getchar();
                }
                if (c == 'C') {
                    if (save_cur < save_line->len) {
                        ++save_cur;
                    }
                } else if (c == 'D') {
                    if (save_cur > 0) {
                        --save_cur;
                    }
                }
            } else {
                /*
                 * Cancel the save prompt. User will need to press ESC twice (or
                 * ESC, then something else, as the program needs to check that
                 * it isn't followed by a '['.
                 * 
                 * TODO: it seems that ioctl with FIONREAD can give us the
                 * number of characters available, we could use that before
                 * calling getchar() the second time.
                 */
                return ECANCELED;
            }
        } else if (c == '\x08' || c == '\x7F') {
            if (save_cur != 0) {
                RemoveCharacterFromLine(save_line, --save_cur);
            }
        } else if (c == '\n') {
            break;
        } else if (isprint(c)) {
            AddCharToLine(save_line, save_cur++, c);
        }
    }

    return 0;
}


int Search(void) {
    // DO NOT DESTROY THIS ONE! IT'S STATIC AND IS MEANT TO BE USED!
    static struct line* save_line = NULL;
    if (save_line == NULL) {
        save_line = CreateLine(100);
        strcpy(save_line->str, "");
        save_line->len = strlen(save_line->str);
    }
    
    int res = GetSingleLineInput(save_line, "Find: ");
    if (res != 0) {
        return res;
    }

    struct line* line = GetLine(cursor_line);
    if (line->next == NULL && cursor_pos >= line->len) {
        line = GetLine(0);
        cursor_line = 0;
        cursor_pos = 0;
    }
    while (line != NULL) {
        if (cursor_pos >= line->len) {
            ++cursor_line;
            line = line->next;
            cursor_pos = 0;
            continue;
        }
        char* str = line->str + cursor_pos;
        char* match = strstr(str, save_line->str);
        if (match == str) {
            ++cursor_pos;
            continue;
        }
        if (match == NULL) {
            cursor_pos = 0;
            ++cursor_line;
            line = line->next;
        } else {
            cursor_pos += match - str;
            break;
        }
    }

    if (line == NULL) {
        --cursor_line;
        cursor_pos = GetLine(cursor_line)->len;
    }

    return 0;
}

int GoToLine(void) {
    struct line* save_line = CreateLine(100);
    strcpy(save_line->str, "");
    save_line->len = strlen(save_line->str);
    
    int res = GetSingleLineInput(save_line, "Go to line: ");
    if (res != 0) {
        return res;
    }

    int line = atoi(save_line->str);
    if (line == -1) {
        line = num_lines;
    }
    if (line < 1) {
        line = 1;
    }
    if (line > num_lines) {
        line = num_lines;
    }

    cursor_line = line - 1;
    cursor_pos = 0;

    // TODO: destroy the line...

    return 0;
}

int OpenFile(void) {
    struct line* save_line = CreateLine(100);
    strcpy(save_line->str, "");
    save_line->len = strlen(save_line->str);
    
    int res = GetSingleLineInput(save_line, "Open file: ");
    if (res != 0) {
        return res;
    }

    res = LoadFile(save_line->str);

    // TODO: destroy the line at the end...

    if (res != 0) {
        TruelyWipeScreen();
        AnsiClear();
        AnsiReset();
        ScrPrintf("File could not be opened due to the following error:\n\n    %s\n\nPress any key to continue...", strerror(res));
        ScrFlush();
        getchar();
    }

    return res;
}

int SaveFile(void) {
    struct line* save_line = CreateLine(100);
    strcpy(save_line->str, filepath == NULL ? "" : filepath);
    save_line->len = strlen(save_line->str);
    
    int res = GetSingleLineInput(save_line, "Save file: ");
    if (res != 0) {
        return res;
    }

    // TODO: destroy the line at the end...

    errno = 0;
    FILE* f = fopen(save_line->str, "w");
    if (f == NULL) {
        SaveFailed(errno);
        return errno;
    }
    struct line* l = doc_head;
    while (l != NULL) {
        errno = 0;
        if (l != doc_head) {
            res = fputc('\n', f);
            if (res == EOF) {
                int err = errno;
                fclose(f);
                SaveFailed(err);
                return err;
            }
        }
        res = fprintf(f, "%s", l->str);
        if (res < 0) {
            int err = errno;
            fclose(f);
            SaveFailed(err);
            return err;
        }
        l = l->next;
    }

    fclose(f);

    dirty = false;
    filepath = save_line->str;
    return 0;
}

int EditorMain(int argc, char** argv) {
    char stdoutbf[2048];
    
    /*
     * We need to call setvbuf, but the shell has already used stdout (this is
     * only an issue as we aren't using fork/exec yet) - so reopen stdout.
     */
    stdout = fopen("con:", "w");
    setvbuf(stdout, stdoutbf, _IOFBF, 2047);
    
    Init();

    if (argc > 2) {
        fprintf(stderr, "Too many arguments!\n");
        return EINVAL;
    }
    if (argc == 2) {
        int res = LoadFile(argv[1]);
        if (res != 0) {
            fprintf(stderr, "%s\n", strerror(res));
            return res;
        }
    }

    /*
     * Show line numbers by default - must do this after loading the file so
     * that the number of lines is chosen correctly.
     */
    ToggleLineNumbers();

    dirty = false;

    bool needs_update = true;
    while (true) {
        if (needs_update) {
            int div = 2;
            int off = 3;
            int mask = 0;

            if (scroll_mode == 2) {
                div = 5;
                off = 15;
                mask = 15;
            } else if (scroll_mode == 1) {
                div = 3;
                off = 7;
                mask = 7;
            }

            scroll = GetCursorLineWrapped() - (SCREEN_HEIGHT / div);
            if (scroll + SCREEN_HEIGHT >= GetTotalWrappedHeight() + off) {
                scroll = GetTotalWrappedHeight() - SCREEN_HEIGHT + off;

            } else {
                /*
                * To reduce screen drawing cost, only scroll after moving down
                * 4 lines past the scroll point.
                */
                scroll &= ~mask;
            }

            if (scroll < 0) {
                scroll = 0;
            }
            
            DrawScreen();
        }

        needs_update = true;

        char c = getchar();
        if (c == '\x1B') {
            c = getchar();
            if (c == '[') {
                c = getchar();
                while (isdigit(c)) {
                    c = getchar();
                }
                if (c == 'A') {
                    CursorUp();
                } else if (c == 'B') {
                    CursorDown();
                } else if (c == 'C') {
                    CursorRight();
                } else if (c == 'D') {
                    CursorLeft();
                } else if (c == 'S') {
                    for (int i = 0; i < SCREEN_HEIGHT; ++i) {
                        CursorUp();
                    }
                } else if (c == 'T') {
                    for (int i = 0; i < SCREEN_HEIGHT; ++i) {
                        CursorDown();
                    }
                } else {
                    needs_update = false;
                }
            } else {
                needs_update = false;
            }
        } else if (c == '\x0C') /* CTRL+L*/ {
            ToggleLineNumbers();
        } else if (c == '\x02') /* CTRL+B*/ {
            GoToLine();
        } else if (c == '\x13') /* CTRL+S*/ {
            SaveFile();
        } else if (c == '\x14') /* CTRL+T*/ {
            scroll_mode = (scroll_mode + 1) % 3;
        } else if (c == '\x15') /* CTRL+U*/ {
            programmers_newline = (programmers_newline + 1) % 5;
        } else if (c == '\x06') /* CTRL+F*/ {
            Search();
        } else if (c == '\x0E' || c == '\x0F' || c == '\x18') /* CTRL+N, CTRL+O, CTRL+X*/ {
            TruelyWipeScreen();
            AnsiClear();
            AnsiReset();
            bool do_action = false;
            if (dirty) {
                ScrPrintf("File isn't saved! Press one of the following:\n\n"
                       " To save your file, press ENTER\n"
                       " To act without saving, press CTRL+@\n"
                       " To cancel, press any other key\n\n"
                );
                ScrFlush();
                char c = getchar();
                if (c == '\n') {
                    do_action = SaveFile() == 0;
                } else if (c == '\x00') {
                    do_action = true;
                }
            } else {
                do_action = true;
            }
            if (do_action) {
                if (c == '\x0E') {
                    ClearFile();
                } else if (c == '\x18') {
                    TruelyWipeScreen();
                    AnsiClear();
                    AnsiReset();
                    struct termios raw;
                    tcgetattr(STDIN_FILENO, &raw);
                    raw.c_lflag |= (ECHO | ICANON);
                    return 0;
                } else {
                    OpenFile();
                }
            }
        } else if (c == '\x08' || c == '\x7F') {
            if (cursor_pos == 0) {
                if (cursor_line != 0) {
                    cursor_pos = GetLine(cursor_line - 1)->len;
                    MergeLineWithPrevious(cursor_line--);
                    dirty = true;         
                }
            } else {
                RemoveCharacterFromLine(GetLine(cursor_line), --cursor_pos);
                dirty = true;
            }
        
        } else if (c == '\n') {
            dirty = true;
            InsertNewline();
        } else if (isprint(c)) {
            dirty = true;
            AddCharToLine(GetLine(cursor_line), cursor_pos, c);
            ++cursor_pos;
        } else {
            needs_update = false;
        }
    }

    return 0;
}