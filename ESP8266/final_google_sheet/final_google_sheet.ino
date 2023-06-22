#include <deprecated.h>
#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <require_cpp11.h>

#include <FS.h>

#include <ESP8266WiFi.h>

#include <DNSServer.h>

#include <ESP8266WebServer.h>


#include <WiFiManager.h>                                                     // https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>                                                     // https://github.com/bblanchon/ArduinoJson


                                                                             // Assign output variables to GPIO pins
char output[57] = "";


const char * host = "script.google.com";
const int httpsPort = 443;

WiFiClientSecure client;                                                      //--> Create a WiFiClientSecure object.

String Spreadsheet_Script_ID = "";                                            //--> spreadsheet script ID

int String_Delet;
char Uart_String_value[150] = "";                                             // Set web server port number to 80
WiFiServer server(80);                                                        // Variable to store the HTTP request
String header;                                                                // Auxiliar variables to store the current output state
String outputState = "off";                                                   //flag for saving data
bool shouldSaveConfig = false;                                                //callback notifying us of the need to save config
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void wifi_connection() {

  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {                                       //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();                                      // Allocate a buffer to store contents of the file.
        std::unique_ptr < char[] > buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject & json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(output, json["output"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  client.setInsecure();
  WiFiManagerParameter custom_output("output", "Google Sheet ID", output, 57);
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter( & custom_output);

  wifiManager.autoConnect("LOCK");                   // if you get here you have connected to the WiFi

  Serial.println("Connected.");

  strcpy(output, custom_output.getValue());                                  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject & json = jsonBuffer.createObject();
    json["output"] = output;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();                                                     //end save
  }

  Spreadsheet_Script_ID = output;

  server.begin();

  WiFiClient client = server.available();                                  // Listen for incoming clients

  Serial.println('@');
  Serial.println('@');
  Serial.println('@');

}
void setup() {

  Serial.begin(9600);
  delay(500);
  client.setInsecure();
  wifi_connection();
}

void loop() {
  byte n = (Serial.available());
  if (n != 0) {
    (Serial.readBytesUntil('\n', Uart_String_value, 150));

    Serial.println(Uart_String_value);

    sendData();

  }
}

                                                                         // sending data to Google Sheets
void sendData() {
  Serial.println("==========");
  Serial.print("connecting to ");
  Serial.println(host);

                                                                         //Connect to Google host
  if (!client.connect(host, httpsPort)) {
    Serial.println('~');
    Serial.println("connection failed");
    delay(500);
    ESP.restart();
    return;
  }

  String url = "/macros/s/" + Spreadsheet_Script_ID + "/exec?" + Uart_String_value;
  Serial.print("requesting URL: ");
  Serial.println('@');
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "User-Agent: BuildFailureDetectorESP8266\r\n" + "Connection: close\r\n\r\n");

  Serial.println("request sent");

                                                                         //Checking whether the data was sent successfully or not
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("==========");
  Serial.println();
  for (String_Delet = 0; String_Delet <= 150; String_Delet++) {
    Uart_String_value[String_Delet] = 0;
  }
}
