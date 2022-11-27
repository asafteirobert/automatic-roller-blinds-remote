#include <Arduino.h>
#include "Constants.h"
#include "Util.h"
#include "BlindsData.h"
#include "Settings.h"
#include <U8g2lib.h>
#include <LowPower.h>
#include <RF24.h>
#include <AceButton.h>
using namespace ace_button;

//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x)  Serial.println (x)
#include "printf.h"
#else
#define DEBUG_PRINT(x)
#endif

struct ReturnDataType
{
  uint8_t roomId;
  uint8_t blindId;
  int8_t currentTargetPercent;
};

//Special values written in targetPercent for various commands
static constexpr int8_t TARGET_DISCOVER = -1; //does nothing, just send the latest ack packet
static constexpr int8_t TARGET_SET_START = -2;
static constexpr int8_t TARGET_SET_END = -3;
static constexpr int8_t TARGET_MOVE_BACK = -4;
static constexpr int8_t TARGET_MOVE_FWD = -5;
struct SentDataType
{
  int8_t targetPercent;
  uint8_t speed;
};

BlindsData blindsData;
Settings settings;
uint8_t currentSetting = 0;

uint8_t lastButtonPressed = 0;
bool charging = false;
bool chargingDone = false;
unsigned long lastActivity = 0;
//volatile bool wokeUp = false;

//Button related
AceButton button1(PINS_BUTTONS[0], HIGH, 1);
AceButton button2(PINS_BUTTONS[1], HIGH, 2);
AceButton button3(PINS_BUTTONS[2], HIGH, 3);
AceButton button4(PINS_BUTTONS[3], HIGH, 4);
AceButton button5(PINS_BUTTONS[4], HIGH, 5);

void handleButtons(AceButton*, uint8_t, uint8_t);

//Display related
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

enum class ScreenState
{
  Home,
  Settings,
  Discover,
  Calibrate
};

ScreenState screenState = ScreenState::Home;
int8_t selectedPercent = 0;
uint8_t presetToSave = 1;

bool updateDisplay = true;
char displayBuffer[20];
bool batteryBlink = false;
unsigned long lastBatteryBlink = 0;
unsigned long lastScreenUpdate = 0;
unsigned long lastPresetSaveTime = 0;
unsigned long lastPresetLoadTime = 0;
uint8_t lastLoadedPreset = 0;
unsigned long lastTransmissionTime = 0;
uint8_t lastTransmissionState = 0; //0=empty 1=ok 2=fail

uint8_t discoveryFoundTotal = 0;

float batteryLevel = -1;
unsigned long lastBatteryRead = 0;

// RF24 related
RF24 radio(PIN_NRF_CE, PIN_NRF_CS);


void drawBatteryLevel()
{
  if ((millis() - lastBatteryRead) > 5000 || batteryLevel == -1)
    batteryLevel = calculateBatteryLevel(batteryVoltage(), REMOTE_BATTERY_TYPE);

  // Position on OLED
  int x = 108; int y = 4;
  if (batteryLevel > 1)
  {
    u8g2.drawFrame(x + 2, y, 18, 9);
    u8g2.drawBox(x, y + 2, 2, 5);

    byte bars = truncf(batteryLevel / (100.0 / 6));
    for (int i = 0; (i < 5 && i < bars); i++)
      u8g2.drawBox(x + 4 + (3 * i), y + 2, 2, 5);
  }
  else
  {
    if (batteryBlink == true)
    {
      u8g2.drawFrame(x + 2, y, 18, 9);
      u8g2.drawBox(x, y + 2, 2, 5);
    }
  }
}

void drawStartScreen()
{
  u8g2.clearBuffer();

#ifndef DEBUG
  u8g2.drawXBMP(0, 4, 24, 24, ICON_LOGO);
#endif

  u8g2.setFont(u8g2_font_7x14_tr);
  u8g2.setCursor(30, 18);
  u8g2.print(F("Remote v1.0"));
#ifdef DEBUG
  u8g2.setCursor(91, 32);
  u8g2.print(F("DEBUG"));
#endif

  u8g2.sendBuffer();
  delay(500);
}

void drawSignalTransmitting()
{
  u8g2.drawXBMP(114, 18, 12, 12, ICON_SIGNAL_TRANSMITTING);
  u8g2.sendBuffer();
}

void drawSignalTransmitOk()
{
  u8g2.drawXBMP(114, 18, 12, 12, ICON_SIGNAL_TRANSMIT_OK);
  u8g2.sendBuffer();
}

void drawSignalTransmitFail()
{
  u8g2.drawXBMP(114, 18, 12, 12, ICON_SIGNAL_TRANSMIT_FAIL);
  u8g2.sendBuffer();
}

void drawClearSignal()
{
  u8g2.setDrawColor(0);
  u8g2.drawBox(114, 18, 12, 12);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void drawDiscoveryStatus(uint8_t percent, uint8_t foundUseful, uint8_t foundTotal, bool drawOk = false)
{
  u8g2.clearBuffer();
  u8g2.drawXBMP(0, 3, 8, 8, ICON_DISCOVER);
  u8g2.setFont(u8g2_font_pxplusibmvga8_tr);
  u8g2.setCursor(11, 12);
  u8g2.print(String(F("Searching ")) + String(percent) + String("%"));
  u8g2.setFont(u8g2_font_7x14_tr);
  u8g2.setCursor(10, 30);
  u8g2.print(String(F("Found ")) + String(foundUseful) + String("/") + String(foundTotal));

  if (drawOk)
  {
    u8g2.setCursor(100, 30);
    u8g2.print(F("[OK]"));
  }
  u8g2.sendBuffer();
}

void updateMainDisplay()
{
  unsigned long currentMillis = millis();
  if (currentMillis - lastScreenUpdate > 1000)
  {
    lastScreenUpdate = currentMillis;
    updateDisplay = true;
  }
  
  if ((batteryLevel <= 1) && currentMillis - lastBatteryBlink > 500)
  {
    batteryBlink = !batteryBlink;
    lastBatteryBlink = currentMillis;
    updateDisplay = true;
  }

  if (!updateDisplay)
    return;
  updateDisplay = false;

  u8g2.clearBuffer();

  if (screenState == ScreenState::Home)
  {
    if (blindsData.blindsCount == 0)
    {
      u8g2.setFont(u8g2_font_pxplusibmvga8_tr);
      u8g2.setCursor(0, 12);
      u8g2.print(F("No blinds"));
      u8g2.setFont(u8g2_font_7x14_tr);
      u8g2.setCursor(0, 30);
      u8g2.print(F("Use discover"));
    }
    else if (currentMillis - lastPresetLoadTime < 1000 && lastLoadedPreset != 0)
    {
      u8g2.setFont(u8g2_font_pxplusibmvga8_tr);
      u8g2.setCursor(0, 12);
      u8g2.print(F("Loaded"));
      u8g2.setFont(u8g2_font_7x14_tr);
      u8g2.setCursor(0, 30);
      u8g2.print(String(F("Preset ")) + String(lastLoadedPreset));
    }
    else
    {
      lastLoadedPreset = 0; //clear lastLoadedPreset so the message is not repeated on millis() overflow
      u8g2.setFont(u8g2_font_pxplusibmvga8_tr);
      u8g2.setCursor(0, 12);
      u8g2.print(F("Window"));

      u8g2.setCursor(70, 12);
      u8g2.print(F("%"));
      
      u8g2.setFont(u8g2_font_open_iconic_arrow_2x_t);
      u8g2.setCursor(0, 32);
      u8g2.drawGlyph(0, 32, 77);
      u8g2.drawGlyph(36, 32, 78);

      u8g2.drawGlyph(59, 32, 77);
      u8g2.drawGlyph(95, 32, 78);

      u8g2.setFont(u8g2_font_7x14_tr);
      u8g2.setCursor(16, 30);
      if (blindsData.selectedBlind == 0)
        u8g2.print(F("All"));
      else
        u8g2.print(blindsData.blinds[blindsData.selectedBlind-1].blindId);

      u8g2.setCursor(75, 30);
      if (blindsData.getSelectedBlindPercent() != selectedPercent)
        u8g2.setFont(u8g2_font_7x14B_tr);
      u8g2.print(selectedPercent);
    }

    drawBatteryLevel(); 

    if (charging)
    {
      u8g2.drawXBMP(96, 2, 12, 12, ICON_CHARGING);
    }
    if (chargingDone)
    {
      u8g2.drawXBMP(98, 2, 8, 12, ICON_CHARGING_DONE);
    }

    if (currentMillis - lastTransmissionTime < 1000 && lastTransmissionState != 0)
    {
      if (lastTransmissionState == 1)
        drawSignalTransmitOk();
      else
        drawSignalTransmitFail();
    }
    else
      lastTransmissionState = 0; //clear lastTransmissionState so the icon is not repeated on millis() overflow

  }
  else if (screenState == ScreenState::Settings)
  {
    //Draw setting name
    u8g2.drawXBMP(0, 3, 8, 8, ICON_SETTINGS);
    u8g2.setFont(u8g2_font_pxplusibmvga8_tr);
    u8g2.setCursor(11, 12);
    if (currentSetting < SETTINGS_COUNT)
      u8g2.print(settings.getSettingString(currentSetting));
    if (currentSetting == SETTINGS_COUNT)
      u8g2.print(F("Discover"));
    if (currentSetting == SETTINGS_COUNT + 1)
      u8g2.print(F("Calibrate"));
    if (currentSetting == SETTINGS_COUNT + 2)
      u8g2.print(F("Exit"));

    //Draw arrows
    if (currentSetting < SETTINGS_COUNT)
    {
      u8g2.setFont(u8g2_font_open_iconic_arrow_2x_t);
      u8g2.drawGlyph(39, 32, 77);
      u8g2.drawGlyph(75, 32, 78);
    }

    u8g2.setFont(u8g2_font_7x14_tr);

    //Draw setting value
    u8g2.setCursor(55, 30);
    if (currentSetting < SETTINGS_COUNT)
      u8g2.print(settings.getSettingValueString(currentSetting));

    if (currentSetting == SETTINGS_COUNT - 1 && currentMillis - lastPresetSaveTime < 1000)
    {
      u8g2.setCursor(92, 30);
      u8g2.print(F("Saved"));
    }
    else
    {
      //Draw OK for relevant settings
      u8g2.setCursor(100, 30);
      if (currentSetting >= SETTINGS_COUNT - 1)
      {
        u8g2.print(F("[OK]"));
      }
    }
    
  }
  else if (screenState == ScreenState::Discover)
  {
    drawDiscoveryStatus(100, blindsData.blindsCount, discoveryFoundTotal, true);
  }
  else if (screenState == ScreenState::Calibrate)
  {
    u8g2.drawXBMP(0, 3, 8, 8, ICON_CALIBRATE);
    u8g2.setFont(u8g2_font_pxplusibmvga8_tr);
    u8g2.setCursor(10, 12);
    u8g2.print(String(F("Calibrating ")) + String(blindsData.blinds[blindsData.selectedBlind-1].blindId));
    u8g2.setFont(u8g2_font_open_iconic_play_2x_t);
    u8g2.drawGlyph(0, 32, 73);
    u8g2.drawGlyph(25, 32, 74);
    u8g2.setFont(u8g2_font_open_iconic_arrow_2x_t);
    u8g2.drawGlyph(50, 32, 77);
    u8g2.drawGlyph(75, 32, 78);
    
    u8g2.setFont(u8g2_font_7x14_tr);
    u8g2.setCursor(100, 30);
    u8g2.print(F("[OK]"));
  }
  u8g2.sendBuffer();
}

void setupButtonsCommon()
{
  pinMode(PIN_BUTTONS_COMMON, INPUT_PULLUP);

  for (int i = 0; i < BUTTONS_COUNT; i++)
  {
    pinMode(PINS_BUTTONS[i], OUTPUT);
    digitalWrite(PINS_BUTTONS[i], LOW);
  }
}

void setupButtonsDistinct()
{
  pinMode(PIN_BUTTONS_COMMON, OUTPUT);
  digitalWrite(PIN_BUTTONS_COMMON, LOW);

  for (int i = 0; i < BUTTONS_COUNT; i++)
  {
    pinMode(PINS_BUTTONS[i], INPUT_PULLUP);
  }
}

void wakeUpHandler()
{
  //don't do anything here. See https://github.com/rocketscream/Low-Power/issues/89
}

void wakeUp()
{
  setupButtonsDistinct();
  u8g2.setPowerSave(false);
  radio.powerUp();
  delay(5);
  //wokeUp = false;
  lastActivity = millis();

  //wait around 5ms for all buttons to be unpressed so that no event is registered after the wakeup
  uint8_t noInputLoops = 0;
  while (true)
  {
    if (!button1.isPressedRaw() &&
        !button2.isPressedRaw() &&
        !button3.isPressedRaw() &&
        !button4.isPressedRaw() &&
        !button5.isPressedRaw())
      noInputLoops++;
    if (noInputLoops > 5)
      break;
    delay(1);
  }
}

void goToSleep()
{
  radio.stopListening();
  radio.powerDown();
  u8g2.setPowerSave(true);

  //wait around 5ms for all buttons to be unpressed so that we don't get stuck in sleep
  uint8_t noInputLoops = 0;
  while (true)
  {
    if (!button1.isPressedRaw() &&
        !button2.isPressedRaw() &&
        !button3.isPressedRaw() &&
        !button4.isPressedRaw() &&
        !button5.isPressedRaw())
      noInputLoops++;
    if (noInputLoops > 5)
      break;
    delay(1);
  }

  setupButtonsCommon();
  delay(5);
  //wokeUp = false;
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTONS_COMMON), wakeUpHandler, LOW);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
  detachInterrupt(digitalPinToInterrupt(PIN_BUTTONS_COMMON));
  wakeUp();
}

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
#endif
  DEBUG_PRINT(F("Starting..."));
  settings.loadFromEEPROM();
  blindsData.loadFromEEPROM();

  //high-z so we don't waste power
  pinMode(PIN_BATTERY_MEASURE_GROUND, INPUT);

  //set reference to INTERNAL for battery measurement
  analogReference(INTERNAL);
  analogRead(PIN_BATTERY_MEASURE);

  setupButtonsDistinct();
  pinMode(PIN_CHARGING, INPUT_PULLUP);
  pinMode(PIN_CHARGING_DONE, INPUT_PULLUP);

  ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
  buttonConfig->resetFeatures();
  buttonConfig->setEventHandler(handleButtons);
  buttonConfig->setClickDelay(900);
  buttonConfig->setLongPressDelay(1000);
  buttonConfig->setRepeatPressInterval(100);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);
 
  //u8g2.setDisplayRotation(U8G2_R2);
  u8g2.begin();
  u8g2.setContrast(settings.displayContrast*25.5);

  drawStartScreen();
  // Start radio communication
  radio.begin();
  radio.setChannel(NRF_CHANNEL);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.setRetries(5, 15);
  radio.openWritingPipe(NRF_PIPE);
  radio.stopListening();
#ifdef DEBUG
  printf_begin();
  radio.printDetails();
#endif
}

//discovery runs by it's self and doesn't use the program loop
void runDiscovery()
{
  drawDiscoveryStatus(0, 0, 0);
  uint8_t sendingPipe[5];
  for(uint8_t i = 1; i<5; i++)
    sendingPipe[i] = NRF_PIPE[i];
  sendingPipe[1] = settings.roomId;
  blindsData.resetToDefault();
  discoveryFoundTotal = 0;
  //loop through all ids and remember who responded
  for (uint8_t currentId = 1; currentId<=BlindsData::MAX_BLIND_COUNT; currentId++)
  {
    sendingPipe[0] = currentId;
    radio.openWritingPipe(sendingPipe);

    SentDataType sentData;
    sentData.targetPercent = TARGET_DISCOVER;
    sentData.speed = settings.speed;
    bool sendSuccess = radio.write(&sentData, sizeof(sentData));

    ReturnDataType returnedData;
    while (radio.isAckPayloadAvailable())
    {
      uint8_t len = radio.getDynamicPayloadSize();
      if (len == sizeof(returnedData))
      {
        radio.read(&returnedData, sizeof(returnedData));
        if (sendSuccess)
        {
          discoveryFoundTotal++;
          if (settings.roomId == 0 || returnedData.roomId == settings.roomId)
          {
            blindsData.blinds[blindsData.blindsCount].currentTargetPercent = returnedData.currentTargetPercent;
            blindsData.blinds[blindsData.blindsCount].blindId = returnedData.blindId;
            blindsData.blindsCount++;
          }
        }
      }
      else
      {
        radio.flush_rx();
        delay(2);
      }
    }
    drawDiscoveryStatus(uint8_t((double(currentId)/BlindsData::MAX_BLIND_COUNT)*100), blindsData.blindsCount, discoveryFoundTotal);
    lastActivity = millis();
  }
  blindsData.saveToEEPROM();
  updateDisplay = true;
}

bool sendSpecialCommand(int8_t command, uint8_t speed = 0)
{
  if (blindsData.selectedBlind == 0)
    return false;
  uint8_t sendingPipe[5];
  for(uint8_t i = 1; i<5; i++)
    sendingPipe[i] = NRF_PIPE[i];

  sendingPipe[0] = blindsData.blinds[blindsData.selectedBlind-1].blindId;
  sendingPipe[1] = settings.roomId;
  radio.openWritingPipe(sendingPipe);

  SentDataType sentData;
  sentData.targetPercent = command;
  if (speed == 0)
    sentData.speed = settings.speed;
  else
    sentData.speed = speed;
  bool sendSuccess = radio.write(&sentData, sizeof(sentData));

  ReturnDataType returnedData;
  while (radio.isAckPayloadAvailable())
  {
    uint8_t len = radio.getDynamicPayloadSize();
    DEBUG_PRINT("Got ack payload len:" + String(len));
    if (len == sizeof(returnedData))
    {
      radio.read(&returnedData, sizeof(returnedData));
      //returned data contains the previous status but it's the best we have
      blindsData.blinds[blindsData.selectedBlind-1].currentTargetPercent = selectedPercent;
    }
    else
    {
      radio.flush_rx();
      delay(2);
      DEBUG_PRINT("Discarded bad packet <<<");
    }
  }
  if (sendSuccess == true)
  {
    DEBUG_PRINT(F("Transmission success"));
  }
  else
  {
    DEBUG_PRINT(F("Failed transmission"));
  }
  return sendSuccess;
}

bool sendSelectedPositionUpdate(bool resendCurentState = false)
{
  if (blindsData.blindsCount == 0)
    return true;
  drawSignalTransmitting();
  bool result = true;

  uint8_t sendingPipe[5];
  for(uint8_t i = 1; i<5; i++)
    sendingPipe[i] = NRF_PIPE[i];
  sendingPipe[1] = settings.roomId;
  //loop to send to all. If a particular blind is selected, it will only loop once.
  for (uint8_t currentSendingBlind = 0; currentSendingBlind<blindsData.blindsCount; currentSendingBlind++)
  {
    if (blindsData.selectedBlind != 0)
      currentSendingBlind = blindsData.selectedBlind-1;
    sendingPipe[0] = blindsData.blinds[currentSendingBlind].blindId;
    radio.openWritingPipe(sendingPipe);

    SentDataType sentData;
    sentData.targetPercent = resendCurentState?blindsData.blinds[currentSendingBlind].currentTargetPercent:selectedPercent;
    sentData.speed = settings.speed;
    bool sendSuccess = radio.write(&sentData, sizeof(sentData));

    ReturnDataType returnedData;
    while (radio.isAckPayloadAvailable())
    {
      uint8_t len = radio.getDynamicPayloadSize();
      DEBUG_PRINT("Got ack payload len:" + String(len));
      if (len == sizeof(returnedData))
      {
        radio.read(&returnedData, sizeof(returnedData));
        //returned data contains the previous status so we don't do anything with it
        blindsData.blinds[currentSendingBlind].currentTargetPercent = sentData.targetPercent;
      }
      else
      {
        radio.flush_rx();
        delay(2);
        DEBUG_PRINT("Discarded bad packet <<<");
      }
    }
    if (sendSuccess == true)
    {
      DEBUG_PRINT(F("Transmission success"));
    }
    else
    {
      DEBUG_PRINT(F("Failed transmission"));
    }
    result = result && sendSuccess;
    if (blindsData.selectedBlind != 0)
      break;
  }
  updateDisplay = true;
  lastTransmissionTime = millis();
  if (result)
    lastTransmissionState = 1;
  else
    lastTransmissionState = 2;
  return result;
}

void settingsChanged()
{
  if (currentSetting == 2)
    u8g2.setContrast(uint8_t(settings.displayContrast*25.5));
}

void handleButtons(AceButton* button, uint8_t eventType, uint8_t buttonState)
{
  //DEBUG_PRINT(String(F("handleEvent(): pin: ")) + button->getPin() + String(F("; eventType: ")) + eventType + String("."));

  lastButtonPressed = button->getId();
  lastActivity = millis();
  updateDisplay = true;

  //Main screen input
  if (screenState == ScreenState::Home)
  {
    if (button->getId() == 1 && eventType == AceButton::kEventClicked)
    {
      if (blindsData.selectedBlind == 0)
        blindsData.selectedBlind = blindsData.blindsCount;
      else
        blindsData.selectedBlind--;

      selectedPercent = blindsData.getSelectedBlindPercent();
    }
    if (button->getId() == 2 && eventType == AceButton::kEventClicked)
    {
      if (blindsData.selectedBlind >= blindsData.blindsCount)
        blindsData.selectedBlind = 0;
      else
        blindsData.selectedBlind++;
  
      selectedPercent = blindsData.getSelectedBlindPercent();
    }

    if (button->getId() == 3 && eventType == AceButton::kEventClicked)
    {
      if (selectedPercent == 0) 
        selectedPercent = 100;
      else
        selectedPercent -= 10;
    }    
    if (button->getId() == 4 && eventType == AceButton::kEventClicked)
    {
      if (selectedPercent >= 100) 
        selectedPercent = 0;
      else
        selectedPercent += 10;
    }

    if (button->getId() <= 4 && button->getId() >= 1 && eventType == AceButton::kEventLongPressed)
    {
      blindsData.loadPreset(button->getId());
      //update all
      blindsData.selectedBlind = 0;
      selectedPercent = blindsData.getAveragePercent();
      lastPresetLoadTime = millis();
      lastLoadedPreset = button->getId();
      sendSelectedPositionUpdate(true);
    }

    if (button->getId() == 5 && eventType == AceButton::kEventClicked)
    {
      sendSelectedPositionUpdate();
    }

    if (button->getId() == 5 && eventType == AceButton::kEventLongPressed)
    {
      screenState = ScreenState::Settings;
      currentSetting = 0;
      settings.presetToSave = 1;
    }
  }
  //Settings screen input
  else if (screenState == ScreenState::Settings)
  {
    if (button->getId() == 1 && eventType == AceButton::kEventClicked)
    {
      if (currentSetting == 0)
        currentSetting = SETTINGS_COUNT + 2;
      else
        currentSetting--;
    }
    if (button->getId() == 2 && eventType == AceButton::kEventClicked)
    {
      if (currentSetting >= SETTINGS_COUNT + 2)
        currentSetting = 0;
      else
        currentSetting++;
    }

    if (button->getId() == 3 && (eventType == AceButton::kEventClicked || eventType == AceButton::kEventRepeatPressed))
    {
      settings.increaseDecreaseSetting(currentSetting, -1);
      settingsChanged();
    }    
    if (button->getId() == 4 && (eventType == AceButton::kEventClicked || eventType == AceButton::kEventRepeatPressed))
    {
      settings.increaseDecreaseSetting(currentSetting, 1);
      settingsChanged();
    }

    if (button->getId() == 5 && eventType == AceButton::kEventClicked)
    {
      if (currentSetting == SETTINGS_COUNT -1)
      {
        blindsData.savePreset(settings.presetToSave);
        lastPresetSaveTime = millis();
      }
      if (currentSetting == SETTINGS_COUNT)
      {
        screenState = ScreenState::Discover;
        settings.saveToEEPROM();
        runDiscovery();
      }
      if (currentSetting == SETTINGS_COUNT + 1)
      {
        if (blindsData.blindsCount == 0)
          screenState = ScreenState::Home;
        else
        {
          screenState = ScreenState::Calibrate;
          blindsData.selectedBlind = 1;
        }
        settings.saveToEEPROM();
      }
      if (currentSetting == SETTINGS_COUNT + 2)
      {
        screenState = ScreenState::Home;
        settings.saveToEEPROM();
      }      
    }

    if (button->getId() == 5 && eventType == AceButton::kEventLongPressed)
    {
      if (currentSetting < SETTINGS_COUNT)
      {
        screenState = ScreenState::Home;
        settings.saveToEEPROM();
      }  
    }
  }
  else if (screenState == ScreenState::Discover)
  {
    if (button->getId() == 5 && eventType == AceButton::kEventClicked)
    {
      screenState = ScreenState::Home;
    }
  }
  else if (screenState == ScreenState::Calibrate)
  {
    if (button->getId() == 1 && eventType == AceButton::kEventClicked)
      sendSpecialCommand(TARGET_SET_START);
    if (button->getId() == 2 && eventType == AceButton::kEventClicked)
      sendSpecialCommand(TARGET_SET_END);
    if (button->getId() == 3 && eventType == AceButton::kEventClicked)
      sendSpecialCommand(TARGET_MOVE_BACK, 20);
    if (button->getId() == 4 && eventType == AceButton::kEventClicked)
      sendSpecialCommand(TARGET_MOVE_FWD, 20);
    //move fast when holding
    if (button->getId() == 3 && eventType == AceButton::kEventRepeatPressed)
      sendSpecialCommand(TARGET_MOVE_BACK, 100);
    if (button->getId() == 4 && eventType == AceButton::kEventRepeatPressed)
      sendSpecialCommand(TARGET_MOVE_FWD, 100);
    if (button->getId() == 5 && eventType == AceButton::kEventClicked)
    {
      if (blindsData.selectedBlind >= blindsData.blindsCount)
      {
        blindsData.selectedBlind = 0;
        screenState = ScreenState::Home;
        settings.saveToEEPROM();
      }
      else
        blindsData.selectedBlind++;
    }
    if (button->getId() == 5 && eventType == AceButton::kEventLongPressed)
    {
      blindsData.selectedBlind = 0;
      screenState = ScreenState::Home;
      settings.saveToEEPROM();
    }
  }

}

void loop()
{
  //if (wokeUp)
  //  wakeUp();

  button1.check();
  button2.check();
  button3.check();
  button4.check();
  button5.check();
 
  charging = !digitalRead(PIN_CHARGING);
  chargingDone = !digitalRead(PIN_CHARGING_DONE);

  if (charging || chargingDone)
    lastActivity = millis();

  updateMainDisplay();

  if ((millis() - lastActivity) > 15000)
    goToSleep();
}