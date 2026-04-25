/** \file romc.hpp
 *
 * \brief Emulates a 3853 Static Memory Interface with 62K of memory 
 * 
 * \details The 3853 Static Memory Interface (SMI) provided all necessary address 
 * lines and control signals to interface up to 65,536 bytes of memory to an F8
 * microcomputer system. It was used by the Chess Videocart to interface regular
 * RAM/ROM ICs. Its functionality is emulated below with 62K of memory available
 * from 0x800 to 0xFFFF.
 * 
 * Refer to the 3853 SMI datasheet for more information.
 */



#include "chips.hpp"
#include "gpio.hpp"
#include "ports.hpp"

inline uint8_t romc = 0x1C; // IDLE
inline uint8_t dbus = 0x00;
inline uint16_t pc0 = 0x00;
inline uint16_t pc1 = 0x00;
inline uint16_t dc0 = 0x00;
inline uint16_t dc1 = 0x00;
inline uint16_t tmp;
inline uint8_t io_address;

/*! \brief Process ROMC instructions */
__force_inline void execute_romc() { 
    switch (romc) {
        case 0x00:
            /*
             * Instruction Fetch. The device whose address space includes the
             * contents of the PC0 register must place on the data bus the op
             * code addressed by PC0; then all devices increment the contents
             * of PC0.
             */
            write_dbus(read_program_byte(pc0), pc0);
            pc0 += 1;
            break;
        case 0x01:
            /*
             * The device whose address space includes the contents of the PC0
             * register must place on the data bus the contents of the memory
             * location addressed by PC0; then all devices add the 8-bit value
             * on the data bus as signed binary number to PC0.
             */
            write_dbus(read_program_byte(pc0), pc0);
            pc0 += (int8_t) dbus;
            break;
        case 0x02:
            /*
             * The device whose DC0 addresses a memory word within the address
             * space of that device must place on the data bus the contents of
             * the memory location addressed by DC0; then all devices increment
             * DC0.
             */
            write_dbus(read_program_byte(dc0), dc0);
            dc0 += 1;
            break;
        case 0x03:
            /*
             * Similar to 0x00, except that it is used for immediate operands
             * fetches (using PC0) instead of instruction fetches.
             */
            write_dbus(io_address = read_program_byte(pc0), pc0);
            pc0 += 1;
            break;
        case 0x04:
            /*
             * Copy the contents of PC1 into PC0
             */
            pc0 = pc1;
            break;
        case 0x05:
            /*
             * Store the data bus contents into the memory location pointed
             * to by DC0; increment DC0.
             * 
             * CPU places "byte to be stored" on the dbus
             */
            write_program_byte(dc0, dbus);
            dc0 += 1;
            break;
        case 0x06:
            /*
             * Place the high order byte of DC0 on the data bus.
             * 
             * Note: Assumed to only apply to the device whose address space 
             * includes the contents of the DC0 register
             */
            write_dbus(dc0 >> 8, dc0);
            break;
        case 0x07:
            /*
             * Place the high order byte of PC1 on the data bus.
             * 
             * Note: Assumed to only apply to the device whose address space 
             * includes the contents of the PC1 register
             */
            write_dbus(pc1 >> 8, pc1);
            break;
        case 0x08:
            /*
             * All devices copy the contents of PC0 into PC1. The CPU outputs
             * zero on the data bus in this ROMC state. Load the data bus into
             * both halves of PC0, thus clearing the register.
             * 
             * Note: Reset button pressed
             */
            pc1 = pc0;
            pc0 = (dbus << 8) | dbus;
            break;
        case 0x09:
            /*
             * The device whose address space includes the contents of the DC0
             * register must place the low order byte of DC0 onto the data bus.
             */
            write_dbus(dc0 & 0xff, dc0);
            break;
        case 0x0A:
            /*
             * All devices add the 8-bit value on the data bus, treated as
             * signed binary number, to the data counter.
             */
            dc0 += (int8_t) dbus;
            break;
        case 0x0B:
            /*
             * The device whose address space includes the value in PC1
             * must place the low order byte of PC1 onto the data bus.
             */
            write_dbus(pc1 & 0xff, pc1);
            break;
        case 0x0C:
            /*
             * The device whose address space includes the contents of the PC0
             * register must place the contents of the memory word addressed
             * by PC0 into the data bus; then all devices move the value that
             * has just been placed on the data bus into the low order byte of PC0.
             */
            write_dbus(read_program_byte(pc0), pc0);
            pc0 = (pc0 & 0xff00) | dbus;
            break;
        case 0x0D:
            /*
             * All devices store in PC1 the current contents of PC0, incremented
             * by 1; PC0 is unaltered.
             */
            pc1 = pc0 + 1;
            break;
        case 0x0E:
            /*
             * The device whose address space includes the contents of the PC0
             * register must place the word addressed by PC0 into the data bus.
             * The value on the data bus is then moved to the low order byte
             * of DC0 by all devices.
             */
            write_dbus(read_program_byte(pc0), pc0);
            dc0 = (dc0 & 0xff00) | dbus;
            break;
        case 0x0F:
            /*
             * The interrupting device with highest priority must place the
             * low order byte of the interrupt vector on the data bus.
             * All devices must copy the contents of PC0 into PC1. All devices
             * must move the contents of the data bus into the low order
             * byte of PC0.
             */
            // TODO: ROMC 0x0F
            pc1 = pc0;
            pc0 = (pc0 & 0xff00) | dbus;
            break;
        case 0x10:
            // Serial.print("0x");
            /*
             * Inhibit any modification to the interrupt priority logic.
             * 
             * Note: Also described as
             *   "PREVENT ADDRESS VECTOR CONFLICTS"
             *   "FREEZE INTERRUPT STATUS"
             *   "Place interrupt circuitry on an inhibit state that
             *   prevents altering the interrupt chain"
             *   "A NO-OP long cycle to allow time for the internal 
             *   priority chain to settle"
             *   "A NO-OP long cycle to allow time for the PRI IN/PRI
             *   OUT chain to settle"
             * in the Mostek F8 Data Book
             */
            break;
        case 0x11:
            /*
             * The device whose address space includes the contents of PC0
             * must place the contents of the addressed memory word on the
             * data bus. All devices must then move the contents of the
             * data bus to the upper byte of DC0.
             */
            write_dbus(read_program_byte(pc0), pc0);
            dc0 = (dc0 & 0x00ff) | (dbus << 8);
            break;
        case 0x12:
        
            /*
             * All devices copy the contents of PC0 into PC1. All devices then
             * move the contents of the data bus into the low order byte of PC0.
             */
            pc1 = pc0;
            pc0 = (pc0 & 0xff00) | dbus;
            break;
        case 0x13:
            /*
             * The interrupting device with highest priority must move the high
             * order half of the interrupt vector onto the data bus. All devices
             * must then move the contents of the data bus into the high order
             * byte of PC0. The interrupting device resets its interrupt circuitry
             * (so that it is no longer requesting CPU servicing and can respond
             * to another interrupt).
             */
            // TODO: ROMC 0x13
            pc0 = (pc0 & 0x00ff) | (dbus << 8);
            break;
        case 0x14:
            /*
             * All devices move the contents of the data bus into the high
             * order byte of PC0.
             */
            pc0 = (pc0 & 0x00ff) | (dbus << 8);
            break;
        case 0x15:
            /*
             * All devices move the contents of the data bus into the high
             * order byte of PC1.
             */
            pc1 = (pc1 & 0x00ff) | (dbus << 8);
            break;
        case 0x16:
            /*
             * All devices move the contents of the data bus into the high
             * order byte of DC0.
             */
            dc0 = (dc0 & 0x00ff) | (dbus << 8);
            break;
        case 0x17:
            /*
             * All devices move the contents of the data bus into the low
             * order byte of PC0.
             */
            pc0 = (pc0 & 0xff00) | dbus;
            break;
        case 0x18:
            /*
             * All devices move the contents of the data bus into the low
             * order byte of PC1.
             */
            pc1 = (pc1 & 0xff00) | dbus;
            break;
        case 0x19:
            /*
             * All devices move the contents of the data bus into the low
             * order byte of DC0.
             */
            dc0 = (dc0 & 0xff00) | dbus;
            break;
        case 0x1A:
            /*
             * During the prior cycle, an I/O port timer or interrupt control
             * register was addressed; the device containing the addressed port
             * must place the contents of the data bus into the address port.
             */
            if (IOPorts[io_address] != nullptr) {
                IOPorts[io_address]->write(dbus);
            }
            break;
        case 0x1B:
            /*
             * During the prior cycle, the data bus specified the address of an
             * I/O port. The device containing the addressed I/O port must place
             * the contents of the I/O port on the data bus. (Note that the
             * contents of timer and interrupt control registers cannot be read
             * back onto the data bus).
             */
            if (IOPorts[io_address] != nullptr) {
                write_dbus(IOPorts[io_address]->read(), VIDEOCART_START_ADDR);
            }
            break;
        case 0x1C:
            /*
             * None.
             *
             * Note: It's function is listed as IDLE in the Fairchild F3850 CPU
             * datasheet.
             * 
             * During OUTS/INS instructions in the range 2 to 15, the data bus
             * holds the address of an I/O port
             */
            io_address = dbus;
            break;
        case 0x1D:
            /*
             * Devices with DC0 and DC1 registers must switch registers.
             * Devices without a DC1 register perform no operation.
             */
            tmp = dc0;
            dc0 = dc1;
            dc1 = tmp;
            break;
        case 0x1E:
            /*
             * The devices whose address space includes the contents of PC0
             * must place the low order byte of PC0 onto the data bus.
             */
            write_dbus(pc0 & 0xff, pc0);
            break;
        case 0x1F:
            /*
             * The devices whose address space includes the contents of PC0
             * must place the high order byte of PC0 onto the data bus.
             */
            write_dbus((pc0 >> 8) & 0xff, pc0);
            break;
      }
}
