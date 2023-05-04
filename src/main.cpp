#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>


//#define WIFI_SSID "2.4G-Vectra-WiFi-8FFD5A"
#define WIFI_SSID "Hotspot"
//#define WIFI_PASSWORD "9364C817ABBE3E49"
#define WIFI_PASSWORD "Hotspot1!"
#define WRITE_API_KEY "PNHYZHY120FBZWPR"
#define READ_API_KEY "EA45V91MO4WP7DAM"
#define DEEP_SLEEP_S_MULTIPLIER 1000000
#define TIMER_CLOCK_DIVIDER 80
#define TIMER_1S_COUNTS 1000000
#define TIMER_SECONDS 900

Adafruit_BMP280 Sensor;

IPAddress HostIP(192, 168, 0, 202);
IPAddress SubnetMask(255, 255, 255, 0);
IPAddress Gateway(192, 168, 0, 1);
IPAddress DNS(8, 8, 8, 8); 
WiFiClient ThingspeakConnectionWrite;
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
  WiFi.mode(WIFI_MODE_STA);
  //WiFi.config(HostIP, , , DNS);
  //WiFi.config(HostIP, Gateway, SubnetMask, DNS);
 
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
  ThingspeakConnectionWrite.flush();
  if(!ThingspeakConnectionWrite.connected())
  {
    if(ThingspeakConnectionWrite.connect(Thingspeak, ThingspeakPort))
    {
      Serial.println("Connected to Thingspeak!");
    }
    else
    {
      Serial.println("An error occurred!");
      return;
    }
  }

  /*Prepare Thingspeak HTTP request to send data*/
  sprintf(Message, "GET https://api.thingspeak.com/update?api_key=%s", WRITE_API_KEY);

  for(int i=4;i<=5;i++)
  {
    sprintf(tempMessage, "&field%d=%.2f", i, DataToSend[i-4]);
    strcat(Message,tempMessage);
  }
  ThingspeakConnectionWrite.println(Message);
  ThingspeakConnectionWrite.println("Connection: close");
  ThingspeakConnectionWrite.flush();
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

/**
 * @brief Function sets the interrupt flag when the timmer reaches set values.
 */
void IRAM_ATTR TimerInterruptFunction() {
  portENTER_CRITICAL_ISR(&timerMux);
  Interrupt=true;
  portEXIT_CRITICAL_ISR(&timerMux);
 
}

/**
 * @brief Function Provides the web server funmctionality.
 * 
 * The server allows for one ocnnection at a time.
 * When client is succesfully connected a webpage with sensor readouts history is displayed.
 */
void WebServer()
{
    WiFiClient client = TemperatureLookup.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,

    Serial.println("New Client connected on port 80.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {  // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        //Serial.write(c);                    // print it out the serial monitor
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
            client.println("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>ESP32 - Monitoring pogody oraz klimatu</title><style>body {font-family: -apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,'Open Sans','Helvetica Neue',sans-serif; background-color: #fafcff; margin: 0;}</style></head>");
            client.println("<body><table style=\"height: 21px; width: 925px; border-collapse: collapse; border-style: none; background-color: #36a1d4;\" border=\"1\"><tbody><tr style=\"height: 21px;\"><td style=\"width: 100%; height: 21px;\"><h1 style=\"color: #4485b8; text-shadow: 1px 2px 2px #E67235;\"><span style=\"color: #ffffff;\">Monitoring pogody oraz klimatu w mieszkaniu</span></h1>");
            client.println("</td></tr></tbody></table><p><strong>Jakub Sołtys</strong></p><p>Aplikacja monitoruje warunki klimatyczne w mieszkaniu oraz pogodę&nbsp;</p><table class=\"editorDemoTable\" style=\"height: 615px; width: 925px; vertical-align: top; border-style: hidden; border-color: black;\">");
            client.println("<thead><tr style=\"height: 23px; border-style: solid; background-color: #77bfe2;\"><td style=\"height: 23px; width: 463px;\"><span style=\"color: #ffffff;\">Czujnik w mieszkaniu</span></td><td style=\"width: 462px;\"><span style=\"color: #ffffff;\">Czujnik na zewnątrz</span></td></tr></thead>");
            client.println("<tbody><tr style=\"height: 69px;\"><td style=\"min-width: 140px; height: 69px; width: 463px; border-style: none; background-color: #fce786;\"><iframe style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/1985742/charts/4?bgcolor=%23ffffff&amp;color=%23d62020&amp;days=1&amp;dynamic=true&amp;type=line\" width=\"450\" height=\"260\"></iframe></td>");
            client.println("<td style=\"min-width: 140px; width: 462px; border-style: none; background-color: #fce786;\"><iframe style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/1985742/charts/1?bgcolor=%23ffffff&amp;color=%23d62020&amp;days=1&amp;dynamic=true&amp;type=line&amp;yaxis=Temperatura+%5B%C2%B0C%5D\" width=\"450\" height=\"260\"></iframe></td></tr>");
            client.println("<tr style=\"height: 115px;\"><td style=\"height: 115px; width: 463px; border-style: none; background-color: #fce786;\"><iframe style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/1985742/charts/5?bgcolor=%23ffffff&amp;color=%23d62020&amp;days=1&amp;dynamic=true&amp;type=line\" width=\"450\" height=\"260\"></iframe></td>");
            client.println("<td style=\"width: 462px; border-style: none; background-color: #fce786;\"><iframe style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/1985742/charts/2?bgcolor=%23ffffff&amp;color=%23d62020&amp;days=1&amp;dynamic=true&amp;type=line\" width=\"450\" height=\"260\"></iframe></td></tr><tr style=\"height: 45px;\">");
            client.println("<td style=\"height: 45px; width: 463px; border-style: none; background-color: #fce786;\">&nbsp;</td>");
            client.println("<td style=\"width: 462px; border-color: black; border-style: none; background-color: #fce786;\"><iframe style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/1985742/charts/3?bgcolor=%23ffffff&amp;color=%23d62020&amp;days=1&amp;dynamic=true&amp;type=line\" width=\"450\" height=\"260\"></iframe></td>");
            client.println("</tr></tbody></body></html>");
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
  timerAlarmWrite(timer, TIMER_1S_COUNTS * TIMER_SECONDS, true);
  timerAlarmEnable(timer);
}

void loop() {
  if(Interrupt)
  {
    portENTER_CRITICAL(&timerMux);
    Interrupt=false;
    portEXIT_CRITICAL(&timerMux);
    SendDataToCloud();
  }
  
  WebServer();
}