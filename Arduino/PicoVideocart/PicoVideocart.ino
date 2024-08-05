/** \file PicoVideocart.ino
 *
 * \brief Pico Videocart Firmware (rev 2A)
 * 
 * \details A Videocart that allows games to be loaded from an SD card and
 * played on a Fairchild Channel F.
 * 
 * Created 2022 by 3DMAZE @ AtariAge
 * 
 * ### Limitations
 * 
 * To save space in both program_attribute and ChipTypes, chip_type is assumed to be an 8-bit value.
 * This shouldn't be a problem until there are more than 256 chip types defined in the standard.
 */

// TODO: read/write memory according to program_attribute
// TODO: Disconnecting when loading
// TODO: Minor menu work (remove ".bin", use .chf title, reload menu when holding reset, etc.) 
// TODO: Special char support 
// TODO: set ports according to hardware type
// TODO: Directories
// TODO: Double reset issue
// TODO: Cache(?) issue

#include "loader.hpp"
#include "romc.hpp"

#include <SPI.h>
//#include <SD.h>
#include "hardware/flash.h"
#include <pico/sem.h>
#include <pico/stdlib.h>               // Overclocking functions
#include <pico/multicore.h>            // Allow code to be run on both cores
#include <hardware/gpio.h>
#include <hardware/vreg.h>             // Voltage control for overclocking
#include <hardware/structs/sio.h>
#include <hardware/structs/iobank0.h>
#include <hardware/structs/xip_ctrl.h>
#include <hardware/structs/bus_ctrl.h>
// include for Flash files
#include "FS.h"
#include "LittleFS.h"


#define D0_PIN     0
#define D1_PIN     1
#define D2_PIN     2
#define D3_PIN     3
#define D4_PIN     4
#define D5_PIN     5
#define D6_PIN     6
#define D7_PIN     7
#define ROMC0_PIN  8
#define ROMC1_PIN  9
#define ROMC2_PIN 10
#define ROMC3_PIN 11
#define ROMC4_PIN 12
#define INTREQ_PIN  13
#define PHI_PIN   14
#define WRITE_PIN    15 

// Pico pin usage masks

#define D0_PIN_MASK     0x00000001L //gpio 0
#define D1_PIN_MASK     0x00000002L
#define D2_PIN_MASK     0x00000004L
#define D3_PIN_MASK     0x00000008L
#define D4_PIN_MASK     0x00000010L
#define D5_PIN_MASK     0x00000020L
#define D6_PIN_MASK     0x00000040L
#define D7_PIN_MASK     0x00000080L
#define ROMC0_PIN_MASK  0x00000100L
#define ROMC1_PIN_MASK  0x00000200L
#define ROMC2_PIN_MASK  0x00000400L
#define ROMC3_PIN_MASK  0x00000800L
#define ROMC4_PIN_MASK  0x00001000L
#define INTREQ_PIN_MASK 0x00002000L
#define PHI_PIN_MASK    0x00004000L  //gpio 14
#define WRITE_PIN_MASK  0x00008000L 


// Aggregate Pico pin usage masks
#define ALL_GPIO_MASK  	0x0000FFFFL
#define BUS_PIN_MASK    0x00007F00L
#define DATA_PIN_MASK   0x000000FFL
#define FLAG_MASK       0x0000ff00L
#define ALWAYS_IN_MASK  (FLAG_MASK)
#define ALWAYS_OUT_MASK (DATA_PIN_MASK | INTREQ_PIN_MASK )

#define SET_DATA_MODE_OUT   gpio_set_dir_out_masked(DATA_PIN_MASK)
#define SET_DATA_MODE_IN    gpio_set_dir_in_masked(DATA_PIN_MASK)



void setup1() { // Core 1
    // Set Core 1 priority to high
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS;

    // Initialize data bus pins
    gpio_set_dir_in_masked(0xFF << DBUS0_PIN);        // Set DBUS to input mode
    gpio_clr_mask(0xFF << DBUS0_PIN);                 // Set DBUS data to 0
    gpio_set_function(DBUS0_PIN + 0, GPIO_FUNC_SIO);  // Set DBUS pins to software controlled
    gpio_set_function(DBUS0_PIN + 1, GPIO_FUNC_SIO);
    gpio_set_function(DBUS0_PIN + 2, GPIO_FUNC_SIO);
    gpio_set_function(DBUS0_PIN + 3, GPIO_FUNC_SIO);
    gpio_set_function(DBUS0_PIN + 4, GPIO_FUNC_SIO);
    gpio_set_function(DBUS0_PIN + 5, GPIO_FUNC_SIO);
    gpio_set_function(DBUS0_PIN + 6, GPIO_FUNC_SIO);
    gpio_set_function(DBUS0_PIN + 7, GPIO_FUNC_SIO);
    
    // Initialize ROMC pins
    gpio_set_dir_in_masked(0x1F << ROMC0_PIN);        // Set ROMC to input mode
    gpio_clr_mask(0x1F << ROMC0_PIN);                 // Set ROMC data to 0
    gpio_set_function(ROMC0_PIN + 0, GPIO_FUNC_SIO);  // Set ROMC pins to software controlled
    gpio_set_function(ROMC0_PIN + 1, GPIO_FUNC_SIO);
    gpio_set_function(ROMC0_PIN + 2, GPIO_FUNC_SIO);
    gpio_set_function(ROMC0_PIN + 3, GPIO_FUNC_SIO);
    gpio_set_function(ROMC0_PIN + 4, GPIO_FUNC_SIO);
 
    // Initialize other cartridge pins
    gpio_init_val(WRITE_PIN, GPIO_IN, false);
    gpio_init_val(PHI_PIN, GPIO_IN, false);
    gpio_init_val(INTREQ_PIN, GPIO_OUT, true);
    gpio_init_val(DBUS_OUT_CE_PIN, GPIO_OUT, true);
    gpio_init_val(DBUS_IN_CE_PIN, GPIO_OUT, false);
    gpio_init_val(LED_BUILTIN, GPIO_OUT, true);
    gpio_put(INTREQ_PIN,true);

    
}

void __not_in_flash_func(loop1)() { // Core 1
    for (;;) {
        while(gpio_get(WRITE_PIN));
        // Falling edge
        gpio_set_dir_in_masked(0xFF << DBUS0_PIN);  // Set DBUS to input mode 

        while(gpio_get(WRITE_PIN)==0);
        gpio_set_dir_in_masked(0xFF << DBUS0_PIN);  // Set DBUS to input mode    
        // Rising edge
        dbus = read_dbus();
        romc = read_romc();
        execute_romc();
   
    }
}

void __not_in_flash_func(setup)() { // Core 0

    // Setup SD card pins
    //SPI.setSCK(SERIAL_CLOCK_PIN);
    //SPI.setTX(TRANSMIT_PIN);
    //SPI.setRX(RECEIVE_PIN);
    //SPI.setCS(SD_CARD_CHIP_SELECT_PIN);
    //gpio_init_val(WRITE_PROTECT_PIN, GPIO_IN, false);
    //gpio_pull_up(WRITE_PROTECT_PIN);
    //gpio_init_val(FRAM_CHIP_SELECT_PIN, GPIO_OUT, true);

    LittleFS.begin();
    //Serial.begin(115200);
    //while(!Serial);
    //Serial.println("start");
   
    // Load the game
    //FIXME: SD.begin(SD_CARD_CHIP_SELECT_PIN, SD_SCK_MHZ(50))
    // fall back to 25, then 4 if it doesn't work (but only if there's an SD inserted)
    //while (!SD.begin(SD_CARD_CHIP_SELECT_PIN)) { // wait for SD card
    //    sleep_ms(250);
    //}
    File romFile = LittleFS.open("/boot.bin","r");
    //File romFile = LittleFS.open("/boot_old.bin","r");
    Serial.println("load boot");
    load_game(romFile);


    uint16_t file_counter = 0;
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
        //if (dir.isDirectory()) {
        //    file_data[file_counter].title[0] = '/';
        //} else {
            file_data[file_counter].title[0] = ' ';
        //}
        file_data[file_counter].isFile = true; //!dir.isDirectory();
        char filename[32];
       dir.fileName().toCharArray(filename, 32);
        string_copy((char*) file_data[file_counter].title + 1, (char*)filename, 30, true, '\0');
        Serial.println(file_data[file_counter].title);
        file_counter++;
        //dir.next();
    }
    DIR_LIMIT = file_counter;
    Serial.print("files: ");Serial.println(DIR_LIMIT);
       // Shift into maximum overdrive (aka 400 MHz @ 1.3 V)
 
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(1);  
    if (!set_sys_clock_khz(360000, false)) { // 428000 is known to work on some devices
        blink_code(BLINK::OVERCLOCK_FAILED);
   //     //panic("Overclock was unsuccessful");
       }
   
};

void __not_in_flash_func(loop)() { // Core 0
    // Re-run setup if SD card inserted
    //gpio_put(INTREQ_PIN,true);
    // FIXME: make a SD_DETECT function in gpio.hpp
    sleep_ms(250);
    //Serial.println(pc0);
    
   
    if ((load_new_game_trigger)||(gpio_get(25)==0)) {
        load_new_game_trigger = false;
          while (pc0 >= 0x800) {
            ; // We need to wait until the menu has jumped to 0 before disconnecting
        }
      
        // Get file from index
         uint16_t file_index = static_cast<Launcher*>(IOPorts[0xFF])->file_index;
         char filename[32];
        strcpy(filename,file_data[file_index].title);
        filename[0]='/';
        Serial.println(filename);
        File romFile = LittleFS.open(filename,"r");
        
        load_game(romFile);
       
 
        if (!set_sys_clock_khz(400000, false)) { // 428000 is known to work on some devices
          blink_code(BLINK::OVERCLOCK_FAILED);
        // panic("Overclock was unsuccessful");
        }
         gpio_put(25,1);
    
    }
     
};
