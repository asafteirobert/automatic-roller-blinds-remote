#ifndef _SETTINGS_h
#define _SETTINGS_h

#include "arduino.h"
#include "Constants.h"

const static int SETTINGS_EEPROM_ADDRESS = 400;
const static uint8_t SETTINGS_COUNT = 4;
const static long SETTINGS_VERSION_CHECK = 4977802; //change to rewrite settings

const static int16_t SETTINGS_RULES[SETTINGS_COUNT][3] =
  {
    // Setting rules format: default, min, max.
    { 1, 1, 20 },
    { 50, 10, 100 },
    { 10, 0, 10 },
    { 1, 1, 4 },
  };

class Settings
{
public:
  void resetToDefault();
  void loadFromEEPROM();
  void saveToEEPROM();
  // Get settings value by index (usefull when iterating through settings).
  int16_t getSettingValue(uint8_t index);
  // Set a value of a specific setting by index.
  void setSettingValue(uint8_t index, int16_t value);
  void increaseDecreaseSetting(uint8_t index, int8_t direction);
  String getSettingString(uint8_t index);
  //String getSettingStringUnit(uint8_t index);
  String getSettingValueString(uint8_t index);
  // Check if a setting is within a min and max value
  bool inRange(int16_t val, uint8_t settingIndex);

public:
  // data to hold setting values
  long settingsVersion;
  uint8_t roomId;
  uint8_t speed;
  uint8_t displayContrast;
  uint8_t presetToSave; //not a real setting, but we use it in the settings menu to simplyfy some code
};
static_assert(sizeof(Settings)+SETTINGS_EEPROM_ADDRESS < E2END, "Settings won't fit in eeprom");
#endif
