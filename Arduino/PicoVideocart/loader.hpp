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

//#include <SPI.h>
//#include <SD.h>
#include "FS.h"
#include "LittleFS.h"

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
void read_chf_file(uint8_t program_rom[], File &romFile) { // FIXME: just use the global program_rom
    // It's guaranteed the first 16 bytes are valid & the file size is >= 64 (file_header[48] + chip_header[16])

    // Read header
    chf_header header;
    romFile.read((uint8_t*) &header, sizeof(chf_header));

    // Read title
    char title[257] = {0};
    romFile.read((uint8_t*) &title, header.title_length + 1);
    romFile.seek(header.header_length, SeekSet); // Skip padding

    // Read chip packets
    chip_header ch;
    size_t header_start = romFile.position();
    romFile.read((uint8_t*) &ch, sizeof(ch));
    while (strncmp(ch.magic_number, "CHIP", 4) == 0) {
        // Set attribute and pull data
        memset(program_attribute + ch.load_address, ch.chip_type, ch.size);
        size_t chip_types_length = sizeof(ChipTypes) / sizeof(ChipTypes[0]);
        if (ch.chip_type < chip_types_length && ChipTypes[ch.chip_type]->has_data()) {
            romFile.read((uint8_t*) (program_rom + ch.load_address), ch.size);
            romFile.seek(header_start + ch.packet_length, SeekSet); // Skip padding
        }
        
        // Next packet
        if ((romFile.size() - romFile.position()) >= 16) {
            header_start = romFile.position();
            romFile.read((uint8_t*) &ch, sizeof(ch));
        } else {
            break;
        }
    }
}

void __not_in_flash_func(load_game)(File &romFile) {
    for (uint16_t i = 0; i <= 0xFF; i++) { // Unload IOPorts
       delete IOPorts[i];
       IOPorts[i] = nullptr;
    }
  // Serial.println("load game");
    // TODO: should probably zero the following (although writing to undefined memory could simply be undefined)
    // program_attribute
    // program_rom
    // A: Zero the attribute. Then any attempt to access ROM will return 0xFF
      //Serial.print("File name:");Serial.println(romFile.name());
      if (romFile) {
        uint8_t magic_buffer[17] = {0};
        romFile.read((uint8_t*) magic_buffer, 1); // Read up to 1 byte into magic_buffer
        //Serial.print("Magic:");Serial.println(magic_buffer[0],HEX);
        if (magic_buffer[0] == 0x55) { // .bin file
            //Serial.println("Bin file");
            // TODO: perform after read to memory, use $FF (restricted) for ROM that isn't loaded (i.e. 64K - filesize)
            // Assume hardware type 2 (ROM+RAM) with 2K of RAM at 0x2800,
            // fill 64K with RESERVED, fill ROM with <filesize> ROM, fill 0x2800 with RAM
            // memset(program_attribute, RESERVED_CT::ID, 0x800);  BIOS
            // memset(program_attribute + 0x800, ROM_CT::id, 0xF800);  ROM
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
            romFile.seek(0, SeekSet);
            romFile.read((uint8_t*) (program_rom + 0x800), min(romFile.size(), 0xF7FF)); // Read up to 62K into program_rom
        } else if (magic_buffer[0] == 'C' && romFile.size() >= 64) {                     // possible .chf file
            romFile.seek(0, SeekSet);
            romFile.read((uint8_t*) magic_buffer, 16);                   // Read 16 bytes into magic_buffer
            if (strcmp((char*) magic_buffer, "CHANNEL F       ") == 0) { // .chf file
                romFile.seek(0, SeekSet);
                read_chf_file(program_rom, romFile);
            }
        }
        romFile.close();
    } else {
      Serial.println("No valid file");
      //for (int i=0;i<sizeof(_splash);i++) {
      //  program_rom[0x800+i]=_splash[i];
      //}
      blink_code(BLINK::NO_VALID_FILES);
    }
}
