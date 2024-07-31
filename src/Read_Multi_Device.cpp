/*
// INFO                      Xiao ESp32S3                                         .
// INFO                      Xiao ESp32S3                                         .

THE NEW ESP32S3 HAS NOT THE LINEARITY PROBLEM OF HIS PREDECESSOR!
    SO THEORTICALLY IT IS NOT NEEDED THO CALIBRATE THE ADC!
    https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/peripherals/adc.html#adc-calibration

    The calibration sofware is using a ADC as reference output for
    the ADC BUT THE ESP32S3 doesn't hav an ADC anymore !!!!!!!!!!
    https://github.com/e-tinkers/esp32-adc-calibrate

Reading an analog value with the ESP32 means you can measure varying voltage levels between 0 V and 3.3 V


// INFO                                                                           .
The 1K resistor mod for PZEM-004T v3 is wrong . Issue #9518 . arendst/Tasmota
https://github.com/arendst/Tasmota/issues/9518#issuecomment-1471439315


*/

#include <Arduino.h>
#include <PZEM004Tv30.h> // https://github.com/mandulaj/PZEM-004T-v30
#include <WiFi.h>

#include "ProfileTimer.h"

#define ONE_SECOND 1000

#define PZEM_RX_PIN GPIO_NUM_43 // --> TX PIN OF PZEM
#define PZEM_TX_PIN GPIO_NUM_44 // --> RX PIN OF PZEM
#define PZEM_SERIAL Serial2

// HardwareSerial PZEM_Serial(1); // Serial for 3 Modbus PZEM004T Clients

// This device communicates only if 240VAC is connected!!
PZEM004Tv30 Solar(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN, 0x01);
PZEM004Tv30 Consumption(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN, 0x02);
PZEM004Tv30 Grid(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN, 0x03);

const uint16_t EmonCmsWiFiTimeout = 5000; // ms

// --- Network ---
const char *ESP_HOSTNAME = "PowerMeter3";

const char *WIFI_SSID     = "";
const char *WIFI_PASSWORD = "";

bool WIFI_DHCP = true;

const char *EMONCMS_HOST             = "192.168.0.194";
uint16_t EMONCMS_PORT                = 80;
const char *EMONCMS_NODE_SOLAR       = "SolarDuino";
const char *EMONCMS_NODE_CONSUMPTION = "ConsumptionDuino";
const char *EMONCMS_NODE_GRID        = "PowerDuino";
const char *EMONCMS_API_KEY          = ""; // Your RW apikey
uint16_t EMONCMS_LOOP_TIME           = 5;                                  // SECONDS!
uint32_t EMONCMS_TIMEOUT             = 1500;

// PZEM-004T data
struct PZEM_004T_Sensor_t
{
    float Voltage;
    float Current;
    float Power;
    float Energy;
    float Frequency;
    float PowerFactor;
};

enum PZEM_004T_SENSOR
// **********************************************************************
// **********************************************************************
{
    SOLAR,
    CONSUMPTION,
    GRID
};

// use LED to display status of relay
void handleLED()
// **********************************************************************
// **********************************************************************
{
    static bool blink = false;

    static unsigned long ledBlinkLoopPM = 0;
    unsigned long ledBlinkLoopCM        = millis();
    if (ledBlinkLoopCM - ledBlinkLoopPM >= (500)) // show time every second
    {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        Serial.printf("Tick %i\r\n", digitalRead(LED_BUILTIN));
        // -------- ledBlinkLoop end --------
        ledBlinkLoopPM = ledBlinkLoopCM;
    }
}

void ElapsedRuntime(uint16_t &dd, byte &hh, byte &mm, byte &ss, uint16_t &ms)
// returns the elapsed time since startup of the ESP
{
    unsigned long now = millis();
    int nowSeconds    = now / 1000;

    dd = nowSeconds / 60 / 60 / 24;
    hh = (nowSeconds / 60 / 60) % 24;
    mm = (nowSeconds / 60) % 60;
    ss = nowSeconds % 60;
    ms = now % 1000;
}

// **********************************************************************
// read all data from PZEM-004T
// INFO  reading of PZEM-004T takes ~600ms in BLOCKING mode        !!!!!!
// INFO  currently it is about 76 ms !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// **********************************************************************
void readPZEM004Data(PZEM004Tv30 &pzem, PZEM_004T_Sensor_t &PZEM004data)
{
    // ProfileTimer t("read PZEM004_0"); // 76 ms !
    {
        // tests if value is "Nan" (then set to 0.0) or a valid float ...
        // generates "Nan" for testing ==>  float voltage = float(sqrt(-2.0));
        PZEM004data.Voltage     = isnan(pzem.voltage()) ? 0.0 : pzem.voltage();
        PZEM004data.Current     = isnan(pzem.current()) ? 0.0 : pzem.current();
        PZEM004data.Power       = isnan(pzem.power()) ? 0.0 : pzem.power();
        PZEM004data.Energy      = isnan(pzem.energy()) ? 0.0 : pzem.energy();
        PZEM004data.Frequency   = isnan(pzem.frequency()) ? 0.0 : pzem.frequency();
        PZEM004data.PowerFactor = isnan(pzem.pf()) ? 0.0 : pzem.pf();
    }
}

void PrintToConsole(PZEM_004T_SENSOR Sensor, PZEM_004T_Sensor_t &PZEM004Data)
{
    uint8_t address = 0;
    char Name[20]   = {'\0'};

    if (Sensor == SOLAR)
    {
        address = Solar.readAddress();
        snprintf(Name, sizeof(Name), "SOLAR");
    }
    else if (Sensor == CONSUMPTION)
    {
        address = Consumption.readAddress();
        snprintf(Name, sizeof(Name), "CONSUMPTION");
    }
    else if (Sensor == GRID)
    {
        address = Grid.readAddress();
        snprintf(Name, sizeof(Name), "GRID");
    }
    uint16_t dd = 0;
    byte hh     = 0;
    byte ss     = 0;
    byte mm     = 0;
    uint16_t ms = 0;
    ElapsedRuntime(dd, hh, mm, ss, ms);

    Serial.printf("%05d|%02i:%02i:%02i:%03i | %-15s (0x%02x) || Voltage: %.2f V | Current %.2f A  | Power %.2f W  | Energy %.2f kWh  | Frequency %.2f Hz  | "
                  "PowerFactor %.2f\r\n",
                  dd,
                  hh,
                  mm,
                  ss,
                  ms,
                  Name,
                  address,
                  PZEM004Data.Voltage,
                  PZEM004Data.Current,
                  PZEM004Data.Power,
                  PZEM004Data.Energy,
                  PZEM004Data.Frequency,
                  PZEM004Data.PowerFactor);
}

/*
  reconnection method without callbacks
  maybe the callback causes a guru medidation?!

  only reconnect if mode is STA, in AP mode reconnection is not needed!
*/
void checkWIFIandReconnect()
// **********************************************************************
// **********************************************************************
{
    static unsigned long checkWiFiLoopPM = 0;
    unsigned long checkWiFiLoopCM        = millis();
    if ((checkWiFiLoopCM - checkWiFiLoopPM >= 5000) && (WiFi.getMode() == WIFI_MODE_STA))
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.printf("Disconnected from WiFi - trying to reconnect...\r\n");
            WiFi.disconnect();
            WiFi.reconnect();
        } // -------- checkWiFiLoop end
        checkWiFiLoopPM = checkWiFiLoopCM;
    }
}

bool setupWIFI()
// **********************************************************************
// **********************************************************************
{
    /*
    int16_t n = WiFi.scanNetworks();                      // scan for available Networks ...
   Serial.printf( "Scan available AP's: %i", n);          //
   Serial.printf( "   network(s) found");                 //
    for (int i = 0; i < n; i++)                           //
    {                                                     //
       Serial.printf( "     %s", (WiFi.SSID(i).c_str())); //
    }                                                     //
    if (n == 0)                                           //
    {                                                     //
        return false;                                     // if no AP's available exit
    }                                                     // scanning for AP's  <----<<
    */

    WiFi.persistent(false);       // reset WiFi and erase credentials
    WiFi.disconnect(false, true); // Wifi adapter off - not good! / delete ap values
    WiFi.eraseAP();               // new function because (disconnect(false, true) doesn't erase the credentials...)
    WiFi.mode(WIFI_OFF);          // mode is off (no AP, STA, ...)
    delay(500);

    Serial.println(F("after clearing WiFI credentials"));
    // WiFi.printDiag(Serial);  // ERROR THIS causes a guru meditation!

    // setting hostname in ESP32 always before setting the mode!
    // https://github.com/matthias-bs/BresserWeatherSensorReceiver/issues/19
    WiFi.setHostname(ESP_HOSTNAME);
    WiFi.mode(WIFI_MODE_STA);

    delay(1000); // MANDATORY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    if (WiFi.status() == WL_NO_SHIELD)
    {
        Serial.printf("WiFi shield was not found.\r\n");
        return false;
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // , Credentials::ESP_NOW_CHANNEL

    Serial.println(F("----------------> after begin"));
    WiFi.printDiag(Serial);
    Serial.printf("WiFi IP-Address:%s\r\n", WiFi.localIP().toString().c_str());

    Serial.printf("connecting to WiFi\r\n");

    // WiFi timeout
    int16_t timeout = 20; // 20 * 0.5 sec to connect to a WiFi, otherwise create an AP

    while (WiFi.status() != WL_CONNECTED && timeout > 0) // better timeout handling!
    {
        delay(500);
        Serial.print('.');
        timeout--;
    }
    Serial.println(); // add a new line
    if (timeout == 0)
    {
        Serial.printf("WiFI timeout!\r\n"); // detect if WiFi is connected or run into timeout...
    }
    return true;
}

void sendToEMON(PZEM_004T_SENSOR Sensor, PZEM_004T_Sensor_t &PZEM004data)

{
    if (!(WiFi.status() == WL_CONNECTED))
    {
        Serial.printf("EmonCMS - no WiFi connection!\r\n");
        Serial.printf("WiFi IP-Address:%s\r\n", WiFi.localIP().toString().c_str());
        return;
    }

    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    if (!client.connect(EMONCMS_HOST, EMONCMS_PORT, EMONCMS_TIMEOUT))
    {
        Serial.printf("EmonCMS connection failed: %s|%i\r\n", EMONCMS_HOST, EMONCMS_PORT);
        Serial.printf("WiFi IP-Address:%s\r\n", WiFi.localIP().toString().c_str());
        return;
    }

    // INFO                                                .
    // THIS IS WORKING:
    // http://192.168.0.194/emoncms/input/post.json?node=Test&apikey=1ce596688fc9a1e40d25d855a1336dad&json={Power:21.4,Current:4.25,Energy:7.7}

    client.print("GET /emoncms/input/post.json"); // make sure there is a [space] between GET and /input
    client.print("?node=");
    if (Sensor == SOLAR)
    {
        client.print(EMONCMS_NODE_SOLAR);
    }
    else if (Sensor == CONSUMPTION)
    {
        client.print(EMONCMS_NODE_CONSUMPTION);
    }
    else if (Sensor == GRID)
    {
        client.print(EMONCMS_NODE_GRID);
    }
    
    // http://192.168.0.194/emoncms/input/post.json?node=1&json={power1:100,power2:200,power3:300}

    client.print("&apikey=");      // apikey is mandatory, otherwise only
    client.print(EMONCMS_API_KEY); // integer (!) instead of float will be stored !!!
    client.print("&json={");
    client.print("1:"); // tagname of Current in old EmonCMS
    client.print(PZEM004data.Current);
    client.print(",2:"); // tagname of Power in old EmonCMS
    client.print(PZEM004data.Power);
    client.print(",Energy:");
    client.print(PZEM004data.Energy);
    client.print(",Frequency:");
    client.print(PZEM004data.Frequency);
    client.print(",Voltage:");
    client.print(PZEM004data.Voltage);
    client.print(",PowerFactor:");
    client.print(PZEM004data.PowerFactor);
    client.print("}");
    client.println(" HTTP/1.1"); // make sure there is a [space] BEFORE the HTTP
    client.print(F("Host: "));
    client.println(EMONCMS_HOST);
    // client.print(F("User-Agent: ESP32-Wifi"));
    client.println(F("Connection: close")); //    Although not technically necessary, I found this helpful
    client.println();

    unsigned long timeout = millis();
    while (client.available() == 0)
    {
        if (millis() - timeout > EmonCmsWiFiTimeout)
        {
            Serial.printf(">>> EmonCMS client timeout !\r\n");
            client.stop();
            return;
        }
    }

    // DEBUG                                                              .
    // Read all the lines of the reply from server and print them to Serial
    while (client.available())
    {
        // String line = client.readStringUntil('\r');
        // Serial.printf( "%s", line);
        char c = client.read();
        Serial.write(c);
    }

    // client.stop(); // Stopping client // INFO               is this needed?!
    // Serial.println();
    Serial.printf("\r\nclosing EmonCMS connection\r\n");
}

void setup()
{
    Serial.begin(115200);

    // pinMode(LED_BUILTIN, OUTPUT); // only for blink

    delay(2000);
    Serial.println("M5Atom Read Multiple PZEM004T Test"); // Console print
    Serial.println();
    Serial.println();
    Serial.printf(" Current Solar address:       0x%02x\r\n", Solar.readAddress());
    Serial.printf(" Current Consumption address: 0x%02x\r\n", Consumption.readAddress());
    Serial.printf(" Current Grid address:        0x%02x\r\n", Grid.readAddress());
    Serial.println();
    /*    delay(2000);
        uint8_t addr = 0x03;
        if (!pzem.setAddress(addr))
        {
            // Setting custom address failed. Probably no PZEM connected
            Serial.println("Error setting address.");
        }
        else
        {
            // Print out the new custom address
            Serial.printf(" New PZEM_004T address: 0x%02x\r\n", pzem.readAddress());
        }
    */
    setupWIFI();
}

void loop()
{
    // handleLED();

    PZEM_004T_Sensor_t SolarData;
    PZEM_004T_Sensor_t ConsumptionData;
    PZEM_004T_Sensor_t GridData;

    checkWIFIandReconnect();

    static unsigned long ReadLoopPM = 0;
    unsigned long ReadLoopCM        = millis();
    if (ReadLoopCM - ReadLoopPM >= (ONE_SECOND * EMONCMS_LOOP_TIME)) //
    {
        // Read the data from the sensor
        readPZEM004Data(Solar, SolarData);
        readPZEM004Data(Consumption, ConsumptionData);
        readPZEM004Data(Grid, GridData);

        PrintToConsole(SOLAR, SolarData);
        PrintToConsole(CONSUMPTION, ConsumptionData);
        PrintToConsole(GRID, GridData);

        sendToEMON(SOLAR, SolarData);
        sendToEMON(CONSUMPTION, ConsumptionData);
        sendToEMON(GRID, GridData);

        // -------- ReadLoop end ----------------------------------------------------------------
        ReadLoopPM = ReadLoopCM;
    }
}
