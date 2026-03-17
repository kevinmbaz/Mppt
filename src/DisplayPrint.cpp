#include "DisplayPrint.h"
#define BATTERY_ICON_LEVELS 6
LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD I2C address and dimensions

void initializeDisplay() {
  lcd.init();
  lcd.backlight();
}


// Icon arrays
uint8_t battery_icons[BATTERY_ICON_LEVELS][8] = {
    {0b01110, 0b11011, 0b10001, 0b10001, 0b10001, 0b10001, 0b11111, 0b00000},
    {0b01110, 0b11011, 0b10001, 0b10001, 0b10001, 0b11111, 0b11111, 0b00000},
    {0b01110, 0b11011, 0b10001, 0b10001, 0b11111, 0b11111, 0b11111, 0b00000},
    {0b01110, 0b11011, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b00000},
    {0b01110, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b00000},
    {0b01110, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b00000}
};
uint8_t solar_icon[8] = {
    0b11111, 0b10101, 0b11111, 0b10101, 0b11111, 0b10101, 0b11111, 0b00000
};

bool lcdInitialized = false;

void initDisplayLayout() {
  // Create custom characters ONCE
  lcd.createChar(0, battery_icons[0]); // we’ll overwrite icon 0 later per level if you want
  lcd.createChar(1, solar_icon);

  lcd.clear();

  // Column 0: SOLAR
  lcd.setCursor(0, 0);
  lcd.print("SOL");
  lcd.write(1); // solar icon

  lcd.setCursor(0, 1);
  lcd.print("    V ");  // reserve 4 chars for VinVoltage, then "V"
  lcd.setCursor(0, 2);
  lcd.print("    A ");  // 4 chars for current
  lcd.setCursor(0, 3);
  lcd.print("    W ");  // 4 chars for power

  // Column 6: BATTERY
  lcd.setCursor(6, 0);
  lcd.print("BAT");
  lcd.write(0); // battery icon

  lcd.setCursor(6, 1);
  lcd.print("    V ");   // 4 chars for Vbat + "V"
  lcd.setCursor(6, 3);
  lcd.print("   %");     // 3 chars for batteryLevel + "%"

  // Column 14: LOAD + STATUS
  lcd.setCursor(14, 0);
  lcd.print("LOAD");

  lcd.setCursor(19, 1);
  lcd.print("W");   // 3 chars for loadPercentage + "%"

  lcd.setCursor(14, 2);
  lcd.print("OFF");    // 3 chars, ON/OFF

  lcd.setCursor(14, 3);
  lcd.print("OK ");    // 3 chars, OK/ERR (both 3 chars)
}

void printInitialDisplay(IPAddress local_IP, const char* ssid) {
  if (!lcdInitialized) {
    initDisplayLayout();
    lcdInitialized = true;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" MPPT Charge Ctrl");
  
  lcd.setCursor(0, 1);
  lcd.print("  Connected to ");
  lcd.setCursor(0, 2);
  lcd.print(ssid);

  // Print IP address on the forth line
  lcd.setCursor(0, 3);
  lcd.print(" IP:");
  lcd.print(local_IP);
}

void errorAndRestartDisplay(const char *errMsg) {
  if (!lcdInitialized) {
    initDisplayLayout();
    lcdInitialized = true;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   ERROR!!!   ");
  lcd.setCursor(0, 1);
  lcd.print(errMsg);
  lcd.setCursor(0, 2);
  lcd.print("   Restarting in:");
  for (int i = 0; i < 5; i++)
  {
    lcd.setCursor(9, 3);
    lcd.print(5 - i);
    delay(1000);
  }
  
  ESP.restart();
}

void printToDisplay(float VinVoltage, float InCurrent, float powerIn, 
                    float Vbat, bool isCharging, int batteryLevel, 
                    float loadPercentage, bool isLoadActive, bool systemStatus) {

  if (!lcdInitialized) {
    initDisplayLayout();
    lcdInitialized = true;
  }

  // Update battery icon according to level (optional, but don't clear screen)
  int iconIndex = map(batteryLevel, 0, 100, 0, BATTERY_ICON_LEVELS - 1);
  lcd.createChar(0, battery_icons[iconIndex]); // updates custom char only
  // Re-write the icon position (once is usually enough, but cheap to re-write)
  lcd.setCursor(9, 0);   // where you originally placed BAT icon
  lcd.write(0);

  // ---- SOLAR VALUES ----
  // VinVoltage at (0,1) – 4 chars field
  lcd.setCursor(0, 1);
  lcd.print("    ");           // clear numeric part
  lcd.setCursor(0, 1);
  lcd.print(VinVoltage, 1);    // prints e.g. "12.3"
  // " V " already printed by init

  // InCurrent at (0,2)
  lcd.setCursor(0, 2);
  lcd.print("    ");
  lcd.setCursor(0, 2);
  lcd.print(InCurrent, 2);     // e.g. "3.45"
  // "A " already printed

  // powerIn at (0,3)
  lcd.setCursor(0, 3);
  lcd.print("    ");
  lcd.setCursor(0, 3);
  lcd.print(powerIn, 1);       // e.g. "45.6"
  // "W " already printed

  // ---- BATTERY VALUES ----
  // Vbat at (6,1)
  lcd.setCursor(6, 1);
  lcd.print("    ");
  lcd.setCursor(6, 1);
  lcd.print(Vbat, 1);

  // charge/discharge text right after Vbat field (col 11 or so)
  lcd.setCursor(6, 2);
  if (isCharging) {
    lcd.print("CHARG");   // 5 chars
  } else {
    lcd.print("DISCH");   // 5 chars (same length → no garbage)
  }

  // batteryLevel at (6,3), field "   %"
  lcd.setCursor(6, 3);
  lcd.print("   ");       // clear 3 digits area
  lcd.setCursor(6, 3);
  lcd.print(batteryLevel);  // up to 3 digits, "%" already printed

  // ---- LOAD + STATUS ----
  // lcd.setCursor(14, 1);
  // lcd.print("   ");
  lcd.setCursor(14, 1);
  lcd.print(loadPercentage);   // "%" already there

  // Load ON/OFF at (14,2), always 3 chars
  lcd.setCursor(14, 2);
  if (isLoadActive) {
    lcd.print("ON ");     // 3 chars
  } else {
    lcd.print("OFF");     // 3 chars
  }

  // System status OK/ERR at (14,3), always 3 chars
  lcd.setCursor(14, 3);
  if (systemStatus) {
    lcd.print("OK ");     // 3 chars
  } else {
    lcd.print("ERR");     // 3 chars
  }
}

