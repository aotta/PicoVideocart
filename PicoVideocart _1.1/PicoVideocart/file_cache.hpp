/** \file file_cache.hpp
 * 
 * \brief Handles most code related to loading ROMS
 * 
 * \details Unfortunately, the SD card cannot be accessed while a program is running
 * on core 1. To allow a menu program to work, a cache must be built that can store the
 * directory structure of the SD card.
 * 
 * The current incarnation only supports a flat directory of 100 files or less
 * 
 * ### Limitations
 * 
 * | Name                 | Min | Max  | Same as FAT32
 * |----------------------|-----|------|--------------
 * | File size            |   0 | 4 GB | Yes
 * | File name            |   1 |  255 | Yes
 * | File/Dir per SD card |   0 |  100 | No (268,435,437)
 * | File/Dir per Dir     |   0 |  100 | No (65,536)
 * | Directory depth      |   0 |    0 | No (128)
 */

#pragma once

inline constexpr uint16_t FOLDER_LIMIT = 100; // Max files displayed per folder
inline uint16_t DIR_LIMIT = 0;                // Max index into file_data

struct __attribute__((packed)) file_info {
    char title[32];
    bool isFile;
};

inline file_info file_data[FOLDER_LIMIT] = {0};

void string_copy(char* destination, char* source, uint8_t size, bool write_null=false, char pad_char=' ');

__force_inline void string_copy(char* destination, char* source, uint8_t size, bool write_null, char pad_char) {
    size_t source_len = strlen(source);
    for (uint16_t i = 0; (i < size) && (i < source_len); i++) {
        destination[i] = source[i];
    }
    for (uint16_t i = source_len; i < size; i++) {
        destination[i] = pad_char;
    }
    if (write_null) {
        destination[size] = '\0';
    }
}