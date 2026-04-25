/** \file loader.hpp
 * 
 * \brief Handles loading both .bin and .chf ROM files
 * 
 * \details BIN files are just raw chunks of ROM that can be loaded directly
 * into memory. CHF files are a special container specifically designed for
 * Channel F programs, providing all the necessary information to preserve
 * and load it.
 * 
 * Refer to the [CHF repository](https://github.com/ZX-80/Videocart-Image-Format)
 * for more information.
 */

#pragma once

#include "chips.hpp"
#include "error.hpp"
#include "ports.hpp"

#include "SdFat_Adafruit_Fork.h"
#include "Adafruit_SPIFlash.h"

#include <cstring>
#include <algorithm>

using namespace std;

struct __attribute__((packed)) chf_header {
    char magic_number[16];
    uint32_t header_length;
    uint8_t minor_version;
    uint8_t major_version;
    uint16_t hardware_type;
    uint64_t reserved;
    uint8_t title_length;
};

struct __attribute__((packed)) chip_header {
    char magic_number[4];
    uint32_t packet_length;
    uint16_t chip_type;
    uint16_t bank_number;
    uint16_t load_address;
    uint16_t size;
};

/*! \brief Load a program from the CHF File into program_rom
 * 
 * \param romFile the CHF file to load
 */
void read_chf_file(uint8_t program_rom[], File32 &romFile) {
    // It's guaranteed the first 16 bytes are valid & the file size is >= 64 (file_header[48] + chip_header[16])

    // Read header
    chf_header header;
    romFile.read((uint8_t*) &header, sizeof(chf_header));

    // Read title
    char title[257] = {0};
    romFile.read((uint8_t*) &title, header.title_length + 1);
    romFile.seekSet(header.header_length); // Skip padding (modificato)

    // Read chip packets
    chip_header ch;
    size_t header_start = romFile.curPosition(); // modificato: position() -> curPosition()
    romFile.read((uint8_t*) &ch, sizeof(ch));
    while (strncmp(ch.magic_number, "CHIP", 4) == 0) {
        // Set attribute and pull data
        memset(program_attribute + ch.load_address, ch.chip_type, ch.size);
        size_t chip_types_length = sizeof(ChipTypes) / sizeof(ChipTypes[0]);
        if (ch.chip_type < chip_types_length && ChipTypes[ch.chip_type]->has_data()) {
            romFile.read((uint8_t*) (program_rom + ch.load_address), ch.size);
            romFile.seekSet(header_start + ch.packet_length); // Skip padding (modificato)
        }
        
        // Next packet
        if ((romFile.fileSize() - romFile.curPosition()) >= 16) { // modificato: size() -> fileSize(), position() -> curPosition()
            header_start = romFile.curPosition(); // modificato: position() -> curPosition()
            romFile.read((uint8_t*) &ch, sizeof(ch));
        } else {
            break;
        }
    }
}

void __not_in_flash_func(load_game)(File32 &romFile) {
    
        IOPort* savedLauncher = IOPorts[0xFF];
    
    for (uint16_t i = 0; i <= 0xFF; i++) {
       if (i != 0xFF) {  // Non cancellare il launcher
         delete IOPorts[i];
         IOPorts[i] = nullptr;
       }
    }
    
    // Ripristina il launcher se esisteva
    if (savedLauncher) {
        IOPorts[0xFF] = savedLauncher;
    }
    
    for (uint16_t i = 0; i <= 0xFF; i++) { // Unload IOPorts
       delete IOPorts[i];
       IOPorts[i] = nullptr;
    }
  
    if (romFile) {
        uint8_t magic_buffer[17] = {0};
        romFile.read((uint8_t*) magic_buffer, 1); // Read up to 1 byte into magic_buffer
        
        if (magic_buffer[0] == 0x55) { // .bin file
            // Assume hardware type 2 (ROM+RAM) with 2K of RAM at 0x2800,
            // fill 64K with RESERVED, fill ROM with <filesize> ROM, fill 0x2800 with RAM
            memset(program_attribute + 0x2800, RAM_CT::id, 0x800);

            // Clear RAM: attempt at fixing hangman
            memset(program_rom + 0x2800, 0, 0x800);

            // Assume 2012 SRAM on ports $20/$21/$24/$25
            IOPorts[0x20] = new Sram2102(0);
            IOPorts[0x21] = new Sram2102(1);
            IOPorts[0x24] = new Sram2102(0);
            IOPorts[0x25] = new Sram2102(1);
            IOPorts[0xFF] = new Launcher(file_data);

            // Read up to 62K into program_rom
            romFile.seekSet(0); // modificato: seek(0, SeekSet) -> seekSet(0)
            romFile.read((uint8_t*) (program_rom + 0x800), min(romFile.fileSize(), (uint32_t)0xF7FF)); // modificato: size() -> fileSize()
            
        } else if (magic_buffer[0] == 'C' && romFile.fileSize() >= 64) { // modificato: size() -> fileSize()
            romFile.seekSet(0); // modificato: seek(0, SeekSet) -> seekSet(0)
            romFile.read((uint8_t*) magic_buffer, 16); // Read 16 bytes into magic_buffer
            if (strcmp((char*) magic_buffer, "CHANNEL F       ") == 0) { // .chf file
                romFile.seekSet(0); // modificato: seek(0, SeekSet) -> seekSet(0)
                read_chf_file(program_rom, romFile);
            }
        }
        romFile.close();
    } else {
        Serial.println("No valid file");
        blink_code(BLINK::NO_VALID_FILES);
    }
}