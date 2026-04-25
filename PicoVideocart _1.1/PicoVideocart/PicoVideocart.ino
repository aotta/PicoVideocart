/* \file PicoVideocart.ino
 */
// V.1.1  moved from LittleFS to Adafruit USB Stack

#include "loader.hpp"
#include "error.hpp"
#include "romc.hpp"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "hardware/vreg.h"
#include "hardware/structs/bus_ctrl.h"
#include "SdFat_Adafruit_Fork.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"

Adafruit_FlashTransport_RP2040 flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

FatVolume fatfs;
File32 root;
File32 file;
File32 romFile;

Adafruit_USBD_MSC usb_msc;
bool fs_formatted;
bool fs_changed = false;

// Pin definitions
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
#define INTREQ_PIN 13
#define PHI_PIN    14
#define WRITE_PIN  15

#define DATA_PIN_MASK   0x000000FFL
#define SET_DATA_MODE_OUT   gpio_set_dir_out_masked(DATA_PIN_MASK)
#define SET_DATA_MODE_IN    gpio_set_dir_in_masked(DATA_PIN_MASK)

// Callbacks USB MSC
int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  return flash.readBlocks(lba, (uint8_t*)buffer, bufsize / 512) ? bufsize : -1;
}

int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}

void msc_flush_cb(void) {
  flash.syncBlocks();
  fatfs.cacheClear();
  fs_changed = true;
}


void setup1() {
  bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS;
  
  for (int i = 0; i < 8; i++) {
    gpio_set_function(D0_PIN + i, GPIO_FUNC_SIO);
  }
  for (int i = 0; i < 5; i++) {
    gpio_set_function(ROMC0_PIN + i, GPIO_FUNC_SIO);
  }
  
  gpio_init(WRITE_PIN);
  gpio_set_dir(WRITE_PIN, GPIO_IN);
  gpio_init(PHI_PIN);
  gpio_set_dir(PHI_PIN, GPIO_IN);
  gpio_init(INTREQ_PIN);
  gpio_set_dir(INTREQ_PIN, GPIO_OUT);
  gpio_put(INTREQ_PIN, true);
  gpio_init(LED_BUILTIN);
  gpio_set_dir(LED_BUILTIN, GPIO_OUT);
  gpio_put(LED_BUILTIN, true);
}

void __not_in_flash_func(loop1)() {
  for (;;) {
    while(gpio_get(WRITE_PIN));
    SET_DATA_MODE_IN;
    while(gpio_get(WRITE_PIN) == 0);
    SET_DATA_MODE_IN;
    dbus = read_dbus();
    romc = read_romc();
    execute_romc();
  }
}

void setup() {
  
  
  // Verifica se siamo collegati alla console
  bool carton = false;
  int t = 100;
  
  while (gpio_get(PHI_PIN) == 0 && to_ms_since_boot(get_absolute_time()) < 2000) {
    if (to_ms_since_boot(get_absolute_time()) > t) {
      t += 100;
    }
    sleep_ms(1);
  }
  
 if (gpio_get(PHI_PIN) == 1) {
    carton = true;  
 }
  
  // Inizializza flash
  flash.begin();
  
  // Configura USB MSC
  usb_msc.setID("PicoVideocart", "External Flash", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  usb_msc.setCapacity(flash.size() / 512, 512);
  usb_msc.setUnitReady(true);
  usb_msc.begin();
  
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    sleep_ms(10);
    TinyUSBDevice.attach();
    sleep_ms(10);
  }
  
  // Monta filesystem
  fs_formatted = fatfs.begin(&flash);
  
  if (!carton) {
    // Modalità PC
    usb_msc.setUnitReady(true);
    while (1) {
      blink_code(1);
    }
  }
  
  // === MODALITÀ CONSOLE ===
  usb_msc.setUnitReady(false);
  
  
  // === SCANSIONE FILE ===
  uint16_t file_counter = 0;
  
  if (root.open("/", O_RDONLY)) {
    while (file_counter < FOLDER_LIMIT) {
      if (!file.openNext(&root, O_RDONLY)) break;
      
      if (!file.isDir()) {
        char filename[256];
        file.getName(filename, sizeof(filename));
        
        // Verifica estensione .bin o .chf
        int len = strlen(filename);
        bool is_valid = false;
        
        if (len > 4) {
          if (strcmp(filename + len - 4, ".bin") == 0) {
            is_valid = true;
          } else if (strcmp(filename + len - 4, ".chf") == 0) {
            is_valid = true;
          }
        }
        
        if (is_valid) {
          file_data[file_counter].isFile = true;
          file_data[file_counter].title[0] = ' ';
          
          // Copia il nome senza estensione
          char display_name[32];
          strncpy(display_name, filename, 31);
          if (len > 4 && strcmp(filename + len - 4, ".bin") == 0) {
            display_name[len - 4] = '\0';
          }
          strncpy((char*)file_data[file_counter].title + 1, display_name, 30);
          file_data[file_counter].title[31] = '\0';
          
          file_counter++;
        }
      }
      file.close();
    }
    root.close();
  }
  
  DIR_LIMIT = file_counter;
  
  // === SEGNALA QUANTI FILE SONO STATI TROVATI ===
  // Lampeggia il numero di file trovati (es: 3 file = 3 lampeggi)
  if (DIR_LIMIT == 0) {
    // Nessun file trovato: lampeggio rapido continuo
    blink_code(BLINK::NO_VALID_FILES, 5);
  }
  
  // Scrivi il primo titolo nella RAM
  if (DIR_LIMIT > 0) {
    strncpy((char*)program_rom + SRAM_START_ADDR + 2, 
            file_data[0].title, 30);
  } else {
    const char* no_data = "No Data";
    for (int i = 0; i < 8; i++) {
      program_rom[SRAM_START_ADDR + 2 + i] = no_data[i];
    }
  }
  //File32 romFile;
  // === CARICA BOOT.BIN ===
  romFile = fatfs.open("/boot.bin", O_RDONLY);
  if (!romFile) {
    romFile = fatfs.open("/boot_old.bin", O_RDONLY);
  }
  
  // Carica il boot (lampeggio durante il caricamento)
  load_game(romFile);
  
  // Overclock
  vreg_set_voltage(VREG_VOLTAGE_1_30);
  sleep_ms(1);
  if (!set_sys_clock_khz(360000, false)) {
    blink_code(BLINK::OVERCLOCK_FAILED, 2);
    set_sys_clock_khz(300000, false);
  }
  
  // LED acceso fisso = tutto OK
  gpio_put(LED_BUILTIN, true);
}

void loop() {
  sleep_ms(100);
  
  if (load_new_game_trigger) {
    load_new_game_trigger = false;
    
    // Lampeggio rapido durante il caricamento
    gpio_put(LED_BUILTIN, false);
    
    while (pc0 >= 0x800) {
      sleep_ms(1);
    }
    
    uint16_t file_index = static_cast<Launcher*>(IOPorts[0xFF])->file_index;
    
    if (file_index < DIR_LIMIT) {
      char filename[32];
      strcpy(filename, file_data[file_index].title);
      filename[0] = '/';
      // Ricostruisci il nome completo con estensione
      strcat(filename, ".bin");
      //File32 romFile;
      romFile = fatfs.open(filename, O_RDONLY);
      if (romFile) {
        load_game(romFile);
        
        if (!set_sys_clock_khz(400000, false)) {
          set_sys_clock_khz(300000, false);
        }
      }
    }
    
    gpio_put(LED_BUILTIN, true);
  }
}