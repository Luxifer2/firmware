// LILYGO T-Deck Max (community port, based on the T-Deck Pro variant)
//
// Board facts come from the LILYGO T-Deck-MAX repository (TDeckMaxBoard.h / pinmap):
// radio, GPS, touch, keyboard and sensors are powered and reset through an XL9555 IO expander,
// which earlyInitVariant() brings up before anything else touches those parts.

#ifdef MESHTASTIC_INCLUDE_INKHUD
bool tDeckProIsV11();
void tDeckProSetVibration(int pin, bool on);
void tDeckProUpdateBacklights(bool sleeping = false);
void tDeckProToggleBacklights();
#define EXT_NOTIFY_VIBRA_WRITE(pin, on) tDeckProSetVibration(pin, on)
#endif

// Shared with the home and lock screens (defined in src/platform/extra_variants/t_deck_max/variant.cpp)
#ifdef MESHTASTIC_INCLUDE_INKHUD
extern uint16_t tDeckMaxUnread; // messages received and not yet looked at
extern char tDeckMaxDiag[48]; // last key event seen (diagnostics)
bool tDeckMaxGetLight(uint8_t which);          // 0 = screen light, 1 = keyboard light
void tDeckMaxSetLight(uint8_t which, bool on);
#endif

// Display (E-Ink, GDEQ031T10)
#define PIN_EINK_CS 34
#define PIN_EINK_BUSY 37
#define PIN_EINK_DC 35
#define PIN_EINK_RES 9
#define PIN_EINK_SCLK 36
#define PIN_EINK_MOSI 33
#define PIN_EINK_FRONTLIGHT 41 // PWM front light

#define I2C_SDA SDA
#define I2C_SCL SCL

// CST3530 touch screen + three touch keys (implementation in src/platform/extra_variants/t_deck_pro/variant.cpp)
#define HAS_TOUCHSCREEN 1
#define CST328_PIN_INT 12 // touch interrupt; reset is on the XL9555 (IO07)
#define CST328_PIN_RST -1 // unused (touch reset is on the XL9555), keeps the shared Pro code compiling

#define USE_POWERSAVE
#define SLEEP_TIME 120

// GNSS (MIA-M10Q). Power is on the XL9555 (IO02), so no PIN_GPS_EN here.
#define HAS_GPS 1
#define GPS_BAUDRATE 38400
#define GPS_RX_PIN 2
#define GPS_TX_PIN 16
#define PIN_GPS_PPS 1

#define BUTTON_PIN 0

// SD card slot
#define HAS_SDCARD
#ifndef MESHTASTIC_INCLUDE_INKHUD
#define SDCARD_USE_SPI1
#endif
#define SPI_MOSI (33)
#define SPI_SCK (36)
#define SPI_MISO (47)
#define SPI_CS (48)
#define SDCARD_CS SPI_CS
#define SD_SPI_FREQUENCY 75000000U

// TCA8418 keyboard
#define KB_BL_PIN 42
#ifdef MESHTASTIC_INCLUDE_INKHUD
#define KB_INT 15
#define WAKE_ON_TOUCH
#define SCREEN_TOUCH_INT CST328_PIN_INT
#endif

// Gyroscope BHI260AP (its 1.8V rail is on the XL9555, IO03)
#define HAS_BHI260AP
#define BHI260AP_INT 21

// Fuel gauge BQ27220; the SY6970 charger is read directly in Power.cpp
#define HAS_PPM 1
#define XPOWERS_CHIP_BQ25896 // only so the shared charger code compiles; not used on the Max
#define HAS_BQ27220 1
#define BQ27220_I2C_SDA SDA
#define BQ27220_I2C_SCL SCL
#define BQ27220_DESIGN_CAPACITY 1400

// LoRa SX1262. Power (IO01) and antenna select (IO04) are on the XL9555.
#define USE_SX1262
#define USE_SX1268

#define LORA_SCK 36
#define LORA_MISO 47
#define LORA_MOSI 33
#define LORA_CS 3

#define LORA_DIO0 -1 // not connected
#define LORA_RESET 4
#define LORA_DIO1 5 // SX1262 IRQ
#define LORA_DIO2 6 // SX1262 BUSY

#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_DIO2
#define SX126X_RESET LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 2.4

#define HAS_PHYSICAL_KEYBOARD 1
