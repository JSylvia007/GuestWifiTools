#include <WiFi.h>
#include <WiFiClient.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <Inkplate.h>
#include <Preferences.h>

#define uS_TO_S_FACTOR 1000000LL
#define sleepInSeconds 3600

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////  CUSTOM FUNCTION TO GET VOLTAGE TO WORK ON INKPAPER 6COLOR  /////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ARDUINO_INKPLATECOLOR
// Currently needed as the new port multiplexer is not supported by the default library yet:
// https://github.com/SolderedElectronics/Inkplate-Arduino-library/issues/169#issuecomment-1331716568
// Combined with the following open PR: https://github.com/SolderedElectronics/Inkplate-Arduino-library/pull/171/commits/124462fdf49963d7227881cc4a28c28f4ff40f6e
void pcal6416ModifyReg(uint8_t _reg, uint8_t _bit, uint8_t _state)
{
  uint8_t reg;
  uint8_t mask;
  const uint8_t pcalAddress = 0b00100000;

  Wire.beginTransmission(pcalAddress);
  Wire.write(_reg);
  Wire.endTransmission();

  Wire.requestFrom(pcalAddress, (uint8_t)1);
  reg = Wire.read();

  mask = 1 << _bit;
  reg = ((reg & ~mask) | (_state << _bit));

  Wire.beginTransmission(pcalAddress);
  Wire.write(_reg);
  Wire.write(reg);
  Wire.endTransmission();
}

double readBatteryVoltage()
{
  // Set PCAL P1-1 to output. Do a ready-modify-write operation.
  pcal6416ModifyReg(0x07, 1, 0);

  // Set pin P1-1 to the high -> enable MOSFET for battrey voltage measurement.
  pcal6416ModifyReg(0x03, 1, 1);

  // Wait a little bit
  delay(5);

  // Read analog voltage. Battery measurement is connected to the GPIO35 on the ESP32.
  uint32_t batt_mv = analogReadMilliVolts(35);

  // Turn off the MOSFET.
  pcal6416ModifyReg(0x03, 1, 0);

  // Calculate the voltage
  return (double(batt_mv) / 1000 * 2);
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////  END CUSTOM FUNCTION TO GET VOLTAGE TO WORK ON INKPAPER 6COLOR  ///////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

char server[] = "YOUR WEBSERVER GOES HERE";
char keyfileName[] = "currentKey.txt";
char displayGuestImage[] = "YOUR WEBSERVER GOES HERE/finalCode.png";

// Replace with your network credentials
const char* ssid = "TRUSTED WIFI SSID";
const char* password = "TRUSTED WIFI KEY";

// NTP server to request epoch time
const char* ntpServer = "TIME SERVER";

// Variable to save current epoch time
unsigned long epochTime; 

//TimeZone Config Info
TimeChangeRule usDT = {"EDT", Second, Sun, Mar, 2, -240};  //UTC - 4 hours
TimeChangeRule usST = {"EST", First, Sun, Nov, 2, -300};   //UTC - 5 hours
Timezone myLocalTimeZone(usDT, usST);

//Existing SSID Key from Preferences
String oldSSIDKey;

//value used to store current key from server
char currKeyVal [255];
String finalKeyVal;

//Boot Counter to ensure screen updates at least once every X hours.
int bootCounter = 0;
int desiredWakeupCount = 8;

//Declare the WiFiClient Object
WiFiClient myClient;

//Declare Preferences Namespace
Preferences preferences;

// Declare Inkplate object
Inkplate display;

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

// Initialize WiFi
void initWiFi() {
  Serial.println("###--INIT--###");
  Serial.println("Initial Delay of 5 seconds...");
  delay(5000);
  Serial.println("Exiting Initial Delay of 5 seconds...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("SETTING UP & INIT VARS...");
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("");

  Serial.println((String)"WiFI SSID is: " + WiFi.SSID());
  Serial.println((String)"WiFI IP is: " + WiFi.localIP().toString());
  Serial.println((String)"DNS IP is: " + WiFi.dnsIP().toString());
  
  Serial.println("--------------------------");
  Serial.println("WIFI INIT Complete...");
  Serial.println("");
}

void getSSID() {
  if (myClient.connect(server, 80)) {
    Serial.println((String)"Connected to: " + server);
    // Make a HTTP request:
    myClient.println((String)"GET /" + keyfileName + (String)" HTTP/1.0");
    myClient.println((String)"Host: " + server);
    myClient.println();
  }

  // wait for data to be available
  unsigned long timeout = millis();
  while (myClient.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      myClient.stop();
      delay(60000);
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  Serial.println("Getting current Guest SSID Key...");

  while (myClient.available()) {
    char ch = static_cast<char>(myClient.read());
    if (ch == '\n') {
      ch = myClient.read();
      if (ch == '\r') {
        ch = myClient.read();
        if (ch == '\n') {
          int keyCount = 0;
          while(myClient.available()) {
            currKeyVal[keyCount] = myClient.read();
            keyCount++;
          }
        }
      }
    }
  }

  finalKeyVal = String(currKeyVal);
  finalKeyVal.trim();
  
  // Close the connection
  Serial.println("Key Acquired.  Closing Connection...");
  myClient.stop();
}

void reDrawScreen() {
  // Initialize Inkplate
  display.begin();        // Init Inkplate library (you should call this function ONLY ONCE)
  display.clearDisplay(); // Clear frame buffer of display
  //display.display();      // Put clear image on display

  if (!display.drawImage(displayGuestImage, 11, 15, false, false))
  {
    // If is something failed (wrong filename or format), write error message on the screen.
    display.println("Image open error");
    display.display();
  }
  
  Serial.println("");

  // Read battery voltage (NOTE: Doe to ESP32 ADC accuracy, you should calibrate the ADC!)
  float voltage;
  voltage = readBatteryVoltage();

  time_t myLocalTime;
  myLocalTime = myLocalTimeZone.toLocal(epochTime);

  Serial.println((String)"Epoch Time: " + epochTime);
  Serial.println((String)"Local Time: " + myLocalTime);

  display.setCursor(12, 425);
  display.setTextColor(INKPLATE_BLUE);
  display.setTextSize(2);
  display.print("Last Updated: ");
  display.print(year(myLocalTime));
  display.print("-");
  display.printf("%02d", month(myLocalTime));
  display.print("-");
  display.printf("%02d", day(myLocalTime));
  display.print(" @ ");
  display.printf("%02d", hour(myLocalTime));
  display.print(":");
  display.printf("%02d", minute(myLocalTime));
 
  display.setCursor(520, 425);
  if (voltage >= 3.3) {
    display.setTextColor(INKPLATE_BLUE);
    display.setTextSize(2);
    display.print(voltage, 2); // Print battery voltage
    display.print('V');
  }
  else {
    display.setTextColor(INKPLATE_RED);
    display.setTextSize(3);
    display.print("LOW"); // Print the word LOW
  }

  Serial.println("Updating Display...");

  display.display(); // Send everything to display (refresh the screen)

  Serial.println((String)"Voltage is: " + voltage);
}

void setup() {
  Serial.begin(115200);
  Serial.println((String)"WAKE UP REASON: " + esp_sleep_get_wakeup_cause());
  
  initWiFi();

  configTime(0, 0, ntpServer);
  epochTime = getTime();
  //Serial.println((String)"Epoch Time: " + epochTime);
    
  getSSID();

  preferences.begin("ssidSpace", false);

  oldSSIDKey = preferences.getString("ssidKey", "");
  bootCounter = preferences.getInt("bootCounter", 0);
  bootCounter++;

  Serial.println((String)"BOOT COUNT IS: " + bootCounter);
  //Serial.println((String)"OLD SSID KEY IS: " + oldSSIDKey);
  //Serial.println((String)"NEW SSID KEY IS: " + finalKeyVal);

  if ((oldSSIDKey == finalKeyVal) && (esp_sleep_get_wakeup_cause() != 2) && (bootCounter < desiredWakeupCount)) {
    Serial.println((String)"Keys Match.  No Update Required.");
    preferences.putInt("bootCounter", bootCounter);
  }
  else {
    Serial.println((String)"Keys DO NOT Match, Asked for a Manual Update, or Boot Count exceeded.  Update Required!!");
    preferences.putString("ssidKey", finalKeyVal);
    preferences.putInt("bootCounter", 0);
    reDrawScreen();    
  }

  preferences.end();

  Serial.println("Going to sleep...");
  //display.setPanelDeepSleep(false);
  esp_sleep_enable_timer_wakeup(sleepInSeconds * uS_TO_S_FACTOR);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, LOW);
  esp_deep_sleep_start();
}

void loop() {
  // Never here, as deepsleep restarts esp32
}
