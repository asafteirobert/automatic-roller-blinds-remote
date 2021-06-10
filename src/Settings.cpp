#include "Settings.h"
#include "Constants.h"
#include <EEPROM.h>

void Settings::resetToDefault()
{
  for (int i = 0; i < SETTINGS_COUNT; i++)
    this->setSettingValue(i, SETTINGS_RULES[i][0]);

  this->saveToEEPROM();
}

void Settings::loadFromEEPROM()
{
  // Load settings from EEPROM to this
  EEPROM.get(SETTINGS_EEPROM_ADDRESS, *this);

  if (this->settingsVersion != SETTINGS_VERSION_CHECK)
    this->resetToDefault();
  else
  {
    bool rewriteSettings = false;

    // Loop through all settings to check if everything is fine
    for (int i = 0; i < SETTINGS_COUNT; i++)
    {
      int val = this->getSettingValue(i);

      if (!this->inRange(val, i))
      {
        // Setting is damaged or never written. Rewrite default.
        rewriteSettings = true;
        this->setSettingValue(i, SETTINGS_RULES[i][0]);
      }
    }

    if (rewriteSettings == true)
      this->resetToDefault();
  }
}

// Write settings to the EEPROM then exiting settings menu.
void Settings::saveToEEPROM()
{
  this->settingsVersion = SETTINGS_VERSION_CHECK;
  EEPROM.put(SETTINGS_EEPROM_ADDRESS, *this);
}

int16_t Settings::getSettingValue(uint8_t index)
{
  int value;
  switch (index)
  {
  case 0: value = this->roomId;                      break;
  case 1: value = this->speed;                       break;
  case 2: value = this->displayContrast;             break;
  case 3: value = this->presetToSave;                break;
  }
  return value;
}

void Settings::setSettingValue(uint8_t index, int16_t value)
{
  int16_t result = constrain(value, SETTINGS_RULES[index][1], SETTINGS_RULES[index][2]);
  switch (index)
  {
  case 0: this->roomId = result;                      break;
  case 1: this->speed = result;                       break;
  case 2: this->displayContrast = result;             break;
  case 3: this->presetToSave = result;                break;
  }
}

void Settings::increaseDecreaseSetting(uint8_t index, int8_t direction)
{
  switch (index)
  {
  case 0:
  case 2:
  case 3:
  {
    int val = this->getSettingValue(index) + direction;
    this->setSettingValue(index, val);
    break;
  }
  case 1:
  {
    int val = this->getSettingValue(index) + 5 * direction;
    this->setSettingValue(index, val);
    break;
  }
  }
}

String Settings::getSettingString(uint8_t index)
{
  switch (index)
  {
  case 0: return String(F("Room number"));
  case 1: return String(F("Move speed"));
  case 2: return String(F("Contrast"));
  case 3: return String(F("Save preset"));
  }
  return String(F("Unknown"));
}

/*
String Settings::getSettingStringUnit(int index)
{
  switch (index)
  {
  case 0: 

    return String();
  case 2:
    return String(F("S"));
  case 3:
  case 7:
  case 8:
    return String(F("%"));
  case 9: 
  case 10:
  case 11:
    return String(F(" sec"));
  case 14:
    return String(F(" KM"));
  }
}
*/


String Settings::getSettingValueString(uint8_t index)
{
  switch (index)
  {
  case 0: return String(this->roomId);
  case 1: return String(this->speed);
  case 2: return String(this->displayContrast);
  case 3: return String(this->presetToSave);
  }
  return String(F("Unknown"));
}

bool Settings::inRange(int16_t val, uint8_t settingIndex)
{
  return ((SETTINGS_RULES[settingIndex][1] <= val) && (val <= SETTINGS_RULES[settingIndex][2]));
}
