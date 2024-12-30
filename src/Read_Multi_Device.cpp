/*


This is a simple sketch to read data from multiple PZEM-004T v3 devices and send it to EmonCMS.
It is still a beta version and needs some improvements - but it works.

(c) Armin Pressler 2024


// INFO                                                                           .
The 1K resistor mod for PZEM-004T v3 is wrong . Issue #9518 . arendst/Tasmota
https://github.com/arendst/Tasmota/issues/9518#issuecomment-1471439315


*/

#include <Arduino.h>
#include <PZEM004Tv30.h> // https://github.com/mandulaj/PZEM-004T-v30
#include <WiFi.h>
#include <ArduinoOTA.h>

#include "Credentials.h"  // private data, like WiFi and EmonCMS credentials
#include "ProfileTimer.h" // for debugging purposes only

#define ONE_SECOND 1000

#define PZEM_RX_PIN GPIO_NUM_43 // --> TX PIN OF PZEM
#define PZEM_TX_PIN GPIO_NUM_44 // --> RX PIN OF PZEM
#define PZEM_SERIAL Serial2     // modbus communication with PZEM004T

// The sensor communicates only if 240VAC is connected!!
PZEM004Tv30 Solar(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN, 0x01);
PZEM004Tv30 Consumption(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN, 0x02);
PZEM004Tv30 Grid(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN, 0x03);

// --- Network ---
const char *ESP_HOSTNAME = "PowerMeter3";
bool WIFI_DHCP           = true;

// --- EmonCMS ---
uint16_t EMONCMS_PORT                = 80;
const char *EMONCMS_NODE_SOLAR       = "SolarDuino";
const char *EMONCMS_NODE_CONSUMPTION = "ConsumptionDuino";
const char *EMONCMS_NODE_GRID        = "PowerDuino";

const uint16_t EmonCmsWiFiTimeout = 5000; // ms
const uint16_t EMONCMS_LOOP_TIME  = 5;    // SECONDS!
const uint32_t EMONCMS_TIMEOUT    = 1500;

WiFiServer WebServer(80);

/* 
PZEM-004T data
*/
struct PZEM_004T_Sensor_t
// **********************************************************************
// **********************************************************************
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

/* 
 use LED to display status of the ESP
*/
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

/*
non-blocking delay, needs a global/static variable for &expirationTime to work.
is a little bit more economic, than writing the same code for everey needed delay
*/
boolean nonBlockinigDelay(unsigned long &expirationTime, unsigned long DelayMS)
// **********************************************************************
// **********************************************************************
{
    unsigned long currentMillis = millis();
    if (currentMillis - expirationTime >= DelayMS)
    {
        expirationTime = currentMillis;
        return true; // delay expired
    }
    else
    {
        return false; // delay not expired
    }
}

/* 
 returns the elapsed time since startup of the ESP
*/
void ElapsedRuntime(uint16_t &dd, byte &hh, byte &mm, byte &ss, uint16_t &ms)
// **********************************************************************
// **********************************************************************
{
    unsigned long now = millis();
    int nowSeconds    = now / 1000;

    dd = nowSeconds / 60 / 60 / 24;
    hh = (nowSeconds / 60 / 60) % 24;
    mm = (nowSeconds / 60) % 60;
    ss = nowSeconds % 60;
    ms = now % 1000;
}


/* 
 reads all data from PZEM-004T
// INFO  reading of PZEM-004T takes ~600ms in BLOCKING mode        !!!!!!
// INFO  currently it is about 76 ms !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/
void readPZEM004Data(PZEM004Tv30 &pzem, PZEM_004T_Sensor_t &PZEM004data)
// **********************************************************************
// **********************************************************************
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

/*
 for debugging...
*/
void PrintToConsole(PZEM_004T_SENSOR Sensor, PZEM_004T_Sensor_t &PZEM004Data)
// **********************************************************************
// **********************************************************************
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

/*
 setup WiFi
*/
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
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL); // for mesh/repeater (FritzBox) use the strongest AP!
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);     // for mesh/repeater (FritzBox) use the strongest AP!

    delay(1000); // MANDATORY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    if (WiFi.status() == WL_NO_SHIELD)
    {
        Serial.printf("WiFi shield was not found.\r\n");
        return false;
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // , Credentials::ESP_NOW_CHANNEL

    WiFi.printDiag(Serial);
    Serial.printf("WiFi IP-Address:%s\r\n", WiFi.localIP().toString().c_str());

    Serial.printf("connecting to WiFi\r\n");

    WebServer.begin();

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

/*
 sends data to EmonCMS
*/
void sendToEMON(PZEM_004T_SENSOR Sensor, PZEM_004T_Sensor_t &PZEM004data)
// **********************************************************************
// **********************************************************************
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
        client.stop();
        return;
    }

    // INFO                                                .
    // THIS IS WORKING:
    // http://192.168.0.194/emoncms/input/post.json?node=Test&apikey=876757ccuztfzfiuf775r7rzhcf&json={Power:21.4,Current:4.25,Energy:7.7}

    client.print("GET /emoncms/input/post.json"); // make sure there is a [space] between GET and /emoncms/input
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

    // http://192.168.0.xxx/emoncms/input/post.json?node=1&json={power1:100,power2:200,power3:300}

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
        char c = client.read();
        Serial.write(c);
    }
    // INFO client stop is needed                                      !!!
    // https://github.com/esp8266/Arduino/issues/72#issuecomment-94782500
    //     "So the .stop() call should only be necessary if client is not a local
    //     variable which gets destroyed when the loop function returns."
    // INFO client stop is needed                                      !!!
    client.stop(); // stopping client
    Serial.println();
    Serial.printf("\r\nclosing EmonCMS connection\r\n");
}

/*
 simple static webserver, needs to be manually reloaded in the browser (F5 or reload button)
*/
void doWebserver(Client &client, PZEM_004T_Sensor_t &SolarData, PZEM_004T_Sensor_t &ConsumptionData, PZEM_004T_Sensor_t &GridData)
// **********************************************************************
// **********************************************************************
{
    if (client)
    {                                             // if you get a client,
        Serial.printf("\r\nnew WiFi client\r\n"); // print a message out the serial port
        String currentLine = "";                  // make a String to hold incoming data from the client
        while (client.connected())                // loop while the client's connected)
        {                                         // loop while the client's connected
            if (client.available())
            {                           // if there's bytes to read from the client,
                char c = client.read(); // read a byte, then
                Serial.write(c);        // print it out the serial monitor
                if (c == '\n')
                { // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0)
                    {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();
                        client.println("<!DOCTYPE html><html>");
                        client.println("<head>");
                        client.println("    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        client.println("    <style>");
                        client.println("        html {font-size:1.5vw; font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
                        client.println("    </style>");
                        client.println("</head>");
                        // web page body
                        client.println("<body>");
                        client.println("<h1>Powermeter Multi V3 Beta</h1>");
                        client.println("<div style=\"display:flex; justify-content:space-around;\">");
                        client.println("<div>");
                        client.print("<p style=\"font-size:2vw;font-weight:bold;\"><br>Solar Data:</p>");
                        client.printf("<p>Power: %5.2f W</p>", SolarData.Power);
                        client.printf("<p>Current: %5.2f A</p>", SolarData.Current);
                        client.printf("<p>Energy: %5.2f kWh</p>", SolarData.Energy);
                        client.printf("<p>Frequency: %5.2f Hz</p>", SolarData.Frequency);
                        client.printf("<p>Voltage: %5.2f V</p>", SolarData.Voltage);
                        client.printf("<p>PowerFactor: %5.2f</p>", SolarData.PowerFactor);
                        client.println("</div>");
                        client.println("<div>");
                        client.print("<p style=\"font-size:2vw;font-weight:bold;\"><br>Consumption Data:</p>");
                        client.printf("<p>Power: %5.2f W</p>", ConsumptionData.Power);
                        client.printf("<p>Current: %5.2f A</p>", ConsumptionData.Current);
                        client.printf("<p>Energy: %5.2f kWh</p>", ConsumptionData.Energy);
                        client.printf("<p>Frequency: %5.2f Hz</p>", ConsumptionData.Frequency);
                        client.printf("<p>Voltage: %5.2f V</p>", ConsumptionData.Voltage);
                        client.printf("<p>PowerFactor: %5.2f</p>", ConsumptionData.PowerFactor);
                        client.println("</div>");
                        client.println("<div>");
                        client.print("<p style=\"font-size:2vw;font-weight:bold;\"><br>Grid Data:</p>");
                        client.printf("<p>Power: %5.2f W</p>", GridData.Power);
                        client.printf("<p>Current: %5.2f A</p>", GridData.Current);
                        client.printf("<p>Energy: %5.2f kWh</p>", GridData.Energy);
                        client.printf("<p>Frequency: %5.2f Hz</p>", GridData.Frequency);
                        client.printf("<p>Voltage: %5.2f V</p>", GridData.Voltage);
                        client.printf("<p>PowerFactor: %5.2f</p>", GridData.PowerFactor);
                        client.println("</div>");
                        client.println("</div>");
                        client.printf("<p>rssi: %d</p>", WiFi.RSSI());

                        uint16_t dd = 0;
                        byte hh     = 0;
                        byte ss     = 0;
                        byte mm     = 0;
                        uint16_t ms = 0;
                        ElapsedRuntime(dd, hh, mm, ss, ms);

                        client.printf("<p>Runtime: %05d|%02i:%02i:%02i:%03i </p>", dd, hh, mm, ss, ms); 
                        client.println("</body>");
                        client.println("</html>");
                        // The HTTP response ends with another blank line:
                        client.println();
                        // break out of the while loop:
                        break;
                    }
                    else
                    { // if you got a newline, then clear currentLine:
                        currentLine = "";
                    }
                }
                else if (c != '\r')
                {                     // if you got anything else but a carriage return character,
                    currentLine += c; // add it to the end of the currentLine
                }
            }
        }
        // close the connection:
        client.stop();
        Serial.printf("\r\nWiFI client disconnected\r\n");
    }
}

void setup()
// **********************************************************************
// **********************************************************************
// **********************************************************************
// **********************************************************************
// **********************************************************************
// **********************************************************************
{
    Serial.begin(115200);

    // pinMode(LED_BUILTIN, OUTPUT); // only for blink

    delay(2000);
    Serial.println("Powermeter with multiple PZEM004T sensors"); // Console print
    Serial.println();
    Serial.println();
    Serial.printf(" Current Solar address:       0x%02x\r\n", Solar.readAddress());
    Serial.printf(" Current Consumption address: 0x%02x\r\n", Consumption.readAddress());
    Serial.printf(" Current Grid address:        0x%02x\r\n", Grid.readAddress());
    Serial.println();
    /*  
        This is only needed once to set the address of the PZEM004T

        delay(2000);
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

    ArduinoOTA.setPassword(OTA_PASSWORD); // <---- change this!
    ArduinoOTA.begin();
}

void loop()
// **********************************************************************
// **********************************************************************
// **********************************************************************
// **********************************************************************
// **********************************************************************
// **********************************************************************
// **********************************************************************
// **********************************************************************
{
    // handleLED(); // is only needed if enclosure is transparent ;-)

    // make this variables static/global to avoid invalid data in the webserver
    static PZEM_004T_Sensor_t SolarData;
    static PZEM_004T_Sensor_t ConsumptionData;
    static PZEM_004T_Sensor_t GridData;

    checkWIFIandReconnect();
    ArduinoOTA.handle();

    // Read the data from the sensor every 5 seconds
    static unsigned long readDelay = 0;
    if (nonBlockinigDelay(readDelay, ONE_SECOND * EMONCMS_LOOP_TIME))
    {
        readPZEM004Data(Solar, SolarData);
        readPZEM004Data(Consumption, ConsumptionData);
        readPZEM004Data(Grid, GridData);

        PrintToConsole(SOLAR, SolarData);
        PrintToConsole(CONSUMPTION, ConsumptionData);
        PrintToConsole(GRID, GridData);

        sendToEMON(SOLAR, SolarData);
        sendToEMON(CONSUMPTION, ConsumptionData);
        sendToEMON(GRID, GridData);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        WiFiClient WIFIclient = WebServer.available();
        // WIFIclient.setTimeout(1000);
        doWebserver(WIFIclient, SolarData, ConsumptionData, GridData); // simple webserver
    }
}
