#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "ArduinoJson.h"

#define WIFI_SSID "2.4G-Vectra-WiFi-8FFD5A"
#define WIFI_PASSWORD "9364C817ABBE3E49"
#define WRITE_API_KEY "PNHYZHY120FBZWPR"
#define READ_API_KEY "EA45V91MO4WP7DAM"
#define DEEP_SLEEP_S_MULTIPLIER 1000000
#define TIMER_CLOCK_DIVIDER 80
#define TIMER_1S_COUNTS 1000000
#define TIMER_SECONDS 15

Adafruit_BMP280 Sensor;

IPAddress HostIP(192, 168, 0, 202);
IPAddress SubnetMask(255, 255, 255, 0);
IPAddress Gateway(192, 168, 0, 1);
IPAddress DNS(8, 8, 8, 8); 
WiFiClient ThingspeakConnection;
WiFiServer TemperatureLookup(80);
IPAddress Thingspeak(184, 106, 153, 149); 
uint16_t ThingspeakPort(80);

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool Interrupt=false;

// Variable to store the HTTP request
String header;



/**
 * @brief Setup WiFi interface as client device
 *
 * Configure the WiFi interface in Station mode with provided IP address.
 * Sets the provided subnet and gateway  address, as wel as DNS server IP.
 *
 * @param HostIP  Station designated IP address
 * @param SubnetMask  Subnet mask for provided IP address
 * @param Gateway  default gateway address
 * @param DNS  Default DNS server address
 */
void WiFiClientServerSetup(IPAddress HostIP, IPAddress SubnetMask, IPAddress Gateway, IPAddress DNS)
{
  WiFi.config(HostIP, Gateway, SubnetMask, DNS);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("----------");
  Serial.println("Connecting to WiFi, please wait...");
  
  while(WiFi.status()!=WL_CONNECTED)
  {
    Serial.println("Connecting...");
    delay(1000);
  }
  
  Serial.println("WiFi Connected!");
  Serial.print("IPv4 address: ");
  Serial.print(WiFi.localIP());
  Serial.print("\r\n");

  Serial.print("Subnet mask: ");
  Serial.print(WiFi.subnetMask());
  Serial.print("\r\n");

  Serial.print("Local Gateway: ");
  Serial.print(WiFi.gatewayIP());
  Serial.print("\r\n");

  Serial.print("DNS server: ");
  Serial.print(WiFi.dnsIP());
  Serial.print("\r\n");
  Serial.println("----------");
}

/**
 * @brief Sends the sensor readouts to Thingspeak
 *
 * Function reads out the BME280 sensor.
 * Then the HTTPS request is prepared in order to send data to server.
 */
void SendDataToCloud()
{
  float DataToSend[2]= {0};
  char Message[200]={0};
  char tempMessage[40]={0};
  DataToSend[0]=Sensor.readTemperature();
  DataToSend[1]=Sensor.readPressure()/100.0F;
  ThingspeakConnection.flush();
  if(!ThingspeakConnection.connected())
  {
    if(ThingspeakConnection.connect(Thingspeak, ThingspeakPort))
    {
      Serial.println("Connected to Thingspeak!");
    }
    else
    {
      Serial.println("An error occurred!");
    }
  }
}

/**
 * @brief Sends the sensor readouts to Thingspeak
 *
 * Function reads out the BME280 sensor.
 * Then the HTTPS request is prepared in order to send data to server.
 */
void ReadDataFromCloud()
{
  float DataToSend[2]= {0};
  char Message[200]={0};
  char tempMessage[40]={0};
  DataToSend[0]=Sensor.readTemperature();
  DataToSend[1]=Sensor.readPressure()/100.0F;
  ThingspeakConnection.flush();
  if(!ThingspeakConnection.connected())
  {
    if(ThingspeakConnection.connect(Thingspeak, ThingspeakPort))
    {
      Serial.println("Connected to Thingspeak!");
    }
    else
    {
      Serial.println("An error occurred!");
    }
  }
}

/**
 * @brief Prints the sensor readout through serial interface.
 */
void PrintValues() {
  Serial.print("Temperature = ");
  Serial.print(Sensor.readTemperature());
  Serial.println(" *C");
  
  Serial.print("Pressure = ");
  Serial.print(Sensor.readPressure() / 100.0F);
  Serial.println(" hPa");

  Serial.println();
}

void IRAM_ATTR TimerInterruptFunction() {
  portENTER_CRITICAL_ISR(&timerMux);
  Interrupt=true;
  portEXIT_CRITICAL_ISR(&timerMux);
 
}

/**
 * @brief Setup the peripherals, read out the sensor ans send data to Thingspeak, then enhter deep sleep for set amount of time.
 */
void setup() {
  Serial.begin(9200);
  Serial.println("----------");
  Serial.println("Serial initialized!");
  Serial.println("----------");
  WiFiClientServerSetup(HostIP, SubnetMask, Gateway, DNS);

  Serial.println("----------");
  Serial.println("BMP280 initializing...");
  if(Sensor.begin(0x76))
  {
    Serial.println("BMP280 initialized!");
  }
  else
  {
    Serial.println("BMP280 not initialized!");
  }
  Serial.println("----------");

  TemperatureLookup.begin(80);
  
  timer = timerBegin(0, TIMER_CLOCK_DIVIDER, true);
  timerAttachInterrupt(timer, &TimerInterruptFunction, true);
  timerAlarmWrite(timer, TIMER_1S_COUNTS* TIMER_SECONDS, true);
  timerAlarmEnable(timer);
}

void loop() {
  if(Interrupt)
  {
    portENTER_CRITICAL(&timerMux);
    Interrupt=false;
    portEXIT_CRITICAL(&timerMux);
    //
    //INTERRUPT CODE
    //
    PrintValues();
    SendDataToCloud();
  }
  
  WiFiClient client = TemperatureLookup.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,

    Serial.println("New Client connected on port 80.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {  // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // Display the HTML web page
            client.println("<!doctype html><html>");
            client.println("<head><title>Our Funky HTML Page</title>");
            client.println("<meta name=\"description\" content=\"Our first page\">");
            client.println("<meta name=\"keywords\" content=\"html tutorial template\"></head>");
            client.println("<body>Content goes here.</body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}