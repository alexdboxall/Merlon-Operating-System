
#include <string.h>
#include <ctype.h>
#include <fs/internal/fat.h>

static bool IsValidShortChar(char c) {
    if (isupper(c)) return true;
    if (isdigit(c)) return true;
    
    const char* valid = "!#$%&'()-@^_`{}~";
    for (int i = 0; valid[i]; ++i) {
        if (c == valid[i]) {
            return true;
        }
    }

    return false;
}

static bool IsValidShortName(char* lfn) {
    int before_dot = 0;
    int after_dot = 0;
    bool found_dot = 0;

    for (int i = 0; lfn[i]; ++i) {
        if (i >= 12) {
            return false;
        }

        if (lfn[i] == '.') {
            if (found_dot) {
                return false;
            }
            found_dot = true;
        }

        if (!IsValidShortChar(lfn[i])) {
            return false;
        }

        if (found_dot) {
            after_dot++;
        } else {
            before_dot++;
        }
    }

    return before_dot <= 8 && after_dot <= 3;
}

static bool DoesFileExist(char* directory, char* dos_name, char* dos_extension) {
    char full_path[300];
    strcpy(full_path, directory);
    strcat(full_path, dos_name);
    if (dos_extension[0] != 0) {
        strcat(full_path, ".");
        strcat(full_path, dos_extension);
    }

    // TODO:

    return true;
}

/*
 * Returns LFN_BOTH if needs 8.3 and LFN
 * Returns LFN_SHORT_ONLY if can get away with just 8.3
 * Directory used to determine if there are name clashes
 */
int GetFatShortFilename(char* lfn, char* output, char* directory) {
    if (IsValidShortName(lfn)) {
        return LFN_SHORT_ONLY;
    }

    char stripped_name[7];
    char stripped_extension[4];
    int stripped_index = 0;
    inline_memset(stripped_name, 0, 7);
    inline_memset(stripped_extension, 0, 4);

    int final_dot = -1;

    for (int i = 0; lfn[i]; ++i) {
        char c = lfn[i];
        if (c == ' ') {
            // do nothing
        } else if (c == '.') {
            final_dot = i;
        } else if (isalnum(c)) {
            if (stripped_index < 7) {
                stripped_name[stripped_index++] = toupper(c);
            }
        } else {
            if (stripped_index < 7) {
                stripped_name[stripped_index++] = '_';
            }
        }
    }

    if (final_dot != -1) {
        stripped_index = 0;
        for (int i = final_dot + 1; i < lfn[i] && stripped_index < 3; ++i) {
            if (isalnum(i)) {
                stripped_extension[stripped_index++] = toupper(lfn[i]);
            } else if (lfn[i] != ' ') { // we already know there's no dot as we started on the last one
                stripped_extension[stripped_index++] = '_';
            }
        }
    }

    int num = 0;
    do {
        ++num;
        
        int digits = 1;
        if (num >= 10) ++digits;
        if (num >= 100) ++digits;
        if (num >= 1000) ++digits;
        if (num >= 10000) ++digits;
        if (num >= 100000) ++digits;
        if (num >= 1000000) {
            return LFN_ERROR;
        }

        inline_memset(output, 0, 13);
        strncpy(output, stripped_name, 7 - digits);
        int len = strlen(output);
        output[len] = '~';
        int num_copy = num;
        for (int j = 0; j < digits; ++j) {
            output[len + digits - j] = num_copy % 10 + '0';
            num_copy /= 10;
        }

     } while (DoesFileExist(output, stripped_extension, directory));

    return LFN_BOTH;
}

void FormatFatShortName(char* with_dot, char* without_dot) {
    inline_memset(without_dot, ' ', 11);
    without_dot[11] = 0;

    int i;
    for (i = 0; i < 8 && with_dot[i] && with_dot[i] != '.'; ++i) {
        without_dot[i] = with_dot[i];
    }
    if (with_dot[i] == '.') {
        ++i;
        for (int j = 0; j < 3 && with_dot[i]; ++j) {
            without_dot[8 + j] = with_dot[i];
            ++i;
        }
    }
}

void UnformatFatShortName(char* without_dot, char* with_dot) {
    int i = 0;
    int j = 0;
    while (without_dot[i]) {
        if (i == 8) {
            with_dot[j++] = '.';
        }
        if (without_dot[i] != ' ') {
            with_dot[j++] = without_dot[i];
        }
        ++i;
    }
    
    if (with_dot[j - 1] == '.') {
        /*
         * Remove trailing dot if there's one there (i.e. no file extension). This also null terminates the string.
         */
        with_dot[j - 1] = 0;
    } else {
        /*
         * Still need to null terminate.
         */
        with_dot[j] = 0;
    }
}

