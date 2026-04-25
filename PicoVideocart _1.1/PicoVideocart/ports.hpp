/** \file ports.hpp
 * 
 * \brief Channel F I/O Ports
 *
 * \details The Channel F has 256 addressable I/O ports that it communicates with via the
 * OUT(S) and IN(S) instructions.
 *
 * - Four of these ports are assigned to the CPU/PSU
 *   - https://channelf.se/veswiki/index.php?title=Port
 * - Two can be found on the 3870 Single-Chip Microcomputer & 3871 PIO
 * - Four can be found on the 3853 SMI
 * - Four are used to connect a 2102 SRAM
 * - The remaining addresses were never used by any official Channel F products
 *
 * ### Default Port Assignments
 *
 *  Port Address   | Device           | Description
 *  ---------------|------------------|-------------
 *  0              | CPU              | buttons and Video RAM
 *  1              | CPU              | right controller and pixel palette
 *  4              | PSU              | left controller and horizontal video position
 *  5              | PSU              | sound and vertical video position
 *  6              | MK 3870/3871     | interrupt control port
 *  7              | MK 3870/3871     | binary Timer
 *  C              | 3853 SMI         | programmable interrupt vector (upper byte)
 *  D              | 3853 SMI         | programmable interrupt vector (lower byte)
 *  E              | 3853 SMI         | interrupt control port
 *  F              | 3853 SMI         | programmable timer
 *  20             | Videocart 18     | 2102 SRAM
 *  21             | Videocart 18     | 2102 SRAM
 *  24             | Videocart 10     | 2102 SRAM
 *  25             | Videocart 10     | 2102 SRAM
 */

#pragma once

#include "default_rom.hpp"
#include "file_cache.hpp"

inline constexpr uint16_t SRAM_START_ADDR = 0x2800;
inline bool load_new_game_trigger = false;

/*! \brief Abstract base class for ports 
 * 
 * \details This interface is used by the Videocart emulation code to read and
 * write to I/O ports. New devices can be added by implementing this 
 * interface.
*/
class IOPort {
    public:
        virtual uint8_t read() = 0;
        virtual void write(uint8_t) = 0;
};

/*! \brief A mapping from addresses to I/O ports */
inline IOPort* IOPorts[256];

/*!
 * \brief Implementation of a 2102 SRAM IC
 *
 * \details The 2102 is an asynchronous 1024 x 1-bit static random access
 * read/write memory. It's only used in Videocart 10 (Maze) and 18 (Hangman).
 * Data is normally written when the `read/WRITE` pin is low, but because the
 * ports invert the data, we write when it's high.
 *
 * More info found at http://seanriddle.com/mazepat.asm or any 2102 SRAM
 * datasheet.
 *
 * ### Port Details
 *
 *         19       18
 *  Bit | Port A | Port B
 *  ----|--------|--------
 *  7   | OUT    | A9
 *  6   | -      | A8
 *  5   | -      | A7
 *  4   | -      | A1
 *  3   | IN     | A0
 *  2   | A2     | A5
 *  1   | A3     | A4
 *  0   | RW     | A0
 *
 */
class Sram2102 : public IOPort {
    private:
        uint8_t portIndex;
        uint16_t address = 0;
        inline static bool sramData[1024];
        inline static uint8_t portA = 0;
        inline static uint8_t portB = 0;
        static constexpr uint8_t OUT_FLAG = 0x80;
        static constexpr uint8_t IN_FLAG = 0x8;
        static constexpr uint8_t ADDR_MASK = 0x6;
        static constexpr uint8_t WRITE_FLAG = 0x1;

    public:
        Sram2102(uint8_t portIndex): portIndex(portIndex) {}

        uint8_t read() {
            return portIndex ? portB : portA;
        }

        void write(uint8_t data) {
            if (portIndex) {
                portB = data;
            } else {
                portA = data & 0xF;
            }

            // Update DATA OUT
            address = (portA & ADDR_MASK) << 7 | portB;
            if (portA & WRITE_FLAG) {
                sramData[address] = portA & IN_FLAG;
            }
            portA = sramData[address] << 7 | portA & ~OUT_FLAG;
        }
};

/*!
 * \brief Communicate SD card contents through a port
 * 
 * \details The launcher port allows a menu program to query the Pico for filesystem information,
 * as well as launch a specified program.
 * 
 * ### Commands
 * 
 * | Byte from OUT $FF | Action name | Action
 * |-------------------|-------------|-------
 * | $01               | Next file   | Place previous file title in [$2800, $2900)
 * | $02               | Select      | Begin the loading process
 * | $04               | Prev file   | Place next file title in [$2800, $2900)
 * | $08               | None active | No controller buttons are active (needed to ignore repeat $01/$04)
 * 
 * ### Loading Process 
 * 
 * | Stage | BIOS         | Menu                       | Pico
 * |-------|--------------|----------------------------|-----
 * | 1     |              | Sends $02 (select) command |
 * | 2     |              | Jumps to $0000             | Sets $0800 to $00
 * |       |              |                            | Wait until PC0 < $0800
 * | 3     |              |                            | Disconnect memory
 * |       |              |                            | Rewrite memory, ports, etc.
 * |       |              |                            | Reconnect memory
 * | 4     | Runs program |                            |
 */
class Launcher : public IOPort {
    private:
        file_info (&file_data)[FOLDER_LIMIT];
        inline static uint8_t previous_command = 0;
        inline static uint8_t command = 0;
        static constexpr uint8_t NEXT_FLAG = 0x1;
        static constexpr uint8_t SELECT_FLAG = 0x2;
        static constexpr uint8_t PREV_FLAG = 0x4;
        static constexpr uint8_t NONE_FLAG = 0x8;

    public:
        inline static uint16_t file_index = 0;

        Launcher(file_info (&file_data)[FOLDER_LIMIT]): file_data(file_data) {
            previous_command = 0;
        }

        uint8_t read() {return 0xFF;}

        void write(uint8_t command) {
            if (command != previous_command) {
                if (DIR_LIMIT == 0) {
                    string_copy((char*)program_rom+SRAM_START_ADDR+2, (char*)"No Data", 32, true, '\0');
                } else {
                    switch (command) {
                        case NEXT_FLAG:
                          //  Serial.println("next game");
                            if (file_index != DIR_LIMIT - 1) {
                                file_index++;
                            }
                            string_copy((char*)program_rom+SRAM_START_ADDR+2, file_data[file_index].title, 32, true, '\0');
                            break;
                        case PREV_FLAG:
                         //   Serial.println("prev game");
                            if (file_index != 0) {
                                file_index--;
                            }
                            string_copy((char*)program_rom+SRAM_START_ADDR+2, file_data[file_index].title, 32, true, '\0');
                            break;
                        case SELECT_FLAG:
                          //  Serial.println("select game:");
                             if (file_data[file_index].isFile) {
                                load_new_game_trigger = true; // FIXME: talk to core 0 through the stack instead
                                //gpio_put(LED_BUILTIN, false); // turn off led
                            }
                            break;
                        case NONE_FLAG:
                           // Serial.println("None");
                            if (previous_command == 0) {
                                string_copy((char*)program_rom+SRAM_START_ADDR+2, file_data[file_index].title, 32, true, '\0');
                            }
                            break;
                    }
                }
            }
            previous_command = command;
        }
};