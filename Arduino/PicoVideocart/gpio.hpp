/** \file gpio.hpp 
 * 
 * \brief General Purpose Input/Output (GPIO) functionality for the Raspberry Pi Pico
 *
 * \details The RP2040 has 36 multi-functional General Purpose Input / Output (GPIO) pins, divided into two banks.
 * In a typical use case, the pins in the QSPI bank (QSPI_SS, QSPI_SCLK and QSPI_SD0 to QSPI_SD3) are used to 
 * execute code from an external flash device, leaving the User bank (GPIO0 to GPIO29) for the programmer to use.
 * All GPIOs support digital input and output, but GPIO26 to GPIO29 can also be used as inputs to the chip’s
 * Analogue to Digital Converter (ADC). 
 * 
 * However, not all RP2040 boards provide access to all these pins. The Raspberry Pi Pico exposes 23 Digital GPIOs
 * (GPIO0 to GPIO22) and 3 ADC capable GPIOs (GPIO26 to GPIO 28). The remaining GPIOs are assigned for internal 
 * functions:
 * 
 *  GPIO#   | Mode   | Function
 *  --------|--------|---------
 *  GPIO23  | Output | Controls the on-board SMPS Power Save pin
 *  GPIO24  | Input  | VBUS sense - high if VBUS is present, else low
 *  GPIO25  | Output | Connected to user LED
 *  GPIO29  | Input  | Used in ADC mode (ADC3) to measure VSYS/3
 * 
 * Refer to the RP2040 and Pi Pico datasheets for more information on GPIO.
 * 
 * ### Pin Assignments
 * 
 * ```
 *                                      _____|----|_____
 *                          RX   GP0 - |      USB       | - VBUS
 *                     FRAM_CD   GP1 - |                | - VSYS     5V
 *                               GND - | * LED/GP25     | - GND
 *                SERIAL_CLOCK   GP2 - |                | - 3V3_EN
 *                          TX   GP3 - |                | - 3V3 OUT
 *                  SD_CARD_WP   GP4 - |                | - ADC_VREF
 *                  SD_CARD_CS   GP5 - |                | - GP28     EXTERNAL_INT
 *                               GND - |                | - GND
 *                      DBUS0    GP6 - |   Raspberry    | - GP27
 *                      DBUS1    GP7 - |       Pi       | - GP26     PHI
 *                      DBUS2    GP8 - |      Pico      | - RUN
 *                      DBUS3    GP9 - |                | - GP22     ROMC4
 *                               GND - |                | - GND
 *                      DBUS4   GP10 - |                | - GP21     ROMC3
 *                      DBUS5   GP11 - |                | - GP20     ROMC2
 *                      DBUS6   GP12 - |                | - GP19     ROMC1
 *                      DBUS7   GP13 - |                | - GP18     ROMC0
 *                               GND - |                | - GND
 *                   DBUS_OUT   GP14 - |                | - GP17     WRITE
 *                    DBUS_IN   GP15 - |___--__--__--___| - GP16     INTRQ
 *                             SWCLK ______/  GND   \______ SWDIO
 * ```
 */

#pragma once
// Pico pin usage definitions

   
// Core 1 pins
inline constexpr uint8_t WRITE_PIN = 15;
inline constexpr uint8_t PHI_PIN = 14;
inline constexpr uint8_t DBUS0_PIN = 0;
inline constexpr uint8_t ROMC0_PIN = 8;
inline constexpr uint8_t DBUS_IN_CE_PIN = 25;
inline constexpr uint8_t DBUS_OUT_CE_PIN = 25;

// Core 1 variables
extern uint8_t dbus;                                     // Written to by write_dbus
inline constexpr uint16_t VIDEOCART_START_ADDR = 0x800;  // Videocart address space: [0x0800 - 0x10000)
inline constexpr uint16_t VIDEOCART_SIZE = 0xF800;       // 62K

// Core 1 functions

/*! \brief Initialize a GPIO pin in input mode
 *
 * \param gpio GPIO number
 * \param out true for out, false for in
 * \param value If false clear the GPIO, otherwise set it.
 */
__force_inline void gpio_init_val(uint8_t gpio, bool out, bool value) {
    gpio_put(gpio, value);
    gpio_set_dir(gpio, out);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
}

/*! \brief Get the ROMC bus value
 *
 * \return 5-bit ROMC bus value
 */
__force_inline uint8_t read_romc() {
    gpio_set_dir_in_masked(0XFF);
    return (gpio_get_all() >> ROMC0_PIN) & 0x1F;
}

/*! \brief Get the data bus value
 *
 * \return 8-bit data bus value
 */
__force_inline uint8_t read_dbus() {
    gpio_set_dir_in_masked(0XFF);
    return (gpio_get_all() >> DBUS0_PIN) & 0xFF;
}

//#pragma GCC push_options
//#pragma GCC optimize ("O0")

/*! \brief Put a value on the data bus
 * 
 * \param value The byte to write
 * \param addr_source The address being targeted
 */
__force_inline void write_dbus(uint8_t value, uint16_t addr_source) {
    if (addr_source >= VIDEOCART_START_ADDR && addr_source < (VIDEOCART_START_ADDR + VIDEOCART_SIZE)) { //FIXME: assume flashcart takes full address space
        dbus = value;
        gpio_set_dir_out_masked(0xFF << DBUS0_PIN);  // Set DBUS to output mode
        gpio_clr_mask(0xFF << DBUS0_PIN);            // Write to DBUS
        gpio_set_mask(dbus << DBUS0_PIN);
        gpio_set_dir_out_masked(0xFF << DBUS0_PIN);  // Set DBUS to output mode
        asm inline("nop;nop;nop;nop;");
         // while(gpio_get(WRITE_PIN)==1);
    }
}

//#pragma GCC pop_options

// Core 0 pins
inline constexpr uint8_t SERIAL_CLOCK_PIN = 2;
inline constexpr uint8_t TRANSMIT_PIN = 3;            // MOSI
inline constexpr uint8_t RECEIVE_PIN = 0;             // MISO
inline constexpr uint8_t SD_CARD_CHIP_SELECT_PIN = 5;
inline constexpr uint8_t FRAM_CHIP_SELECT_PIN = 1;
inline constexpr uint8_t WRITE_PROTECT_PIN = 4;

// Core 0 variables
bool old_write_protect = gpio_get(WRITE_PROTECT_PIN); // FIXME: change to bool old_write_protect = false;
