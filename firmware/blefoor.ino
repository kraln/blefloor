#include <WiFiNINA.h>
#include <Wire.h>
#include <ArduinoOTA.h>

#define FILEPATH "/fs/testfile"

#define PIN_RELAY 4
#define PIN_WATCHDOG_OUT 3
#define PIN_WATCHDOG_IN 2

#define SI7020_ADDR 0x40

bool didBark = false;
bool hasWifi = true;

float last_temp = 0.0f;
float last_rh = 0.0f;

int oldRelayEnabled = LOW;
int relayEnabled = LOW;

/* wifi stuff */
int wifi_status = WL_IDLE_STATUS;
WiFiServer server(80);
WiFiUDP Udp;

typedef struct
{
  char wifi_ssid[32];
  char wifi_pass[32];
  char device_title[32];
  float on_under;
  float off_over;
  char report_ip[16];
  uint16_t report_port;
  float temp_offset;
} settings_t;

settings_t current_settings;

void setup()
{

  /* setup GPIO pins */
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_WATCHDOG_IN, INPUT);
  pinMode(PIN_WATCHDOG_OUT, OUTPUT);

  /* Pin Interrupt */
  attachInterrupt(digitalPinToInterrupt(PIN_WATCHDOG_IN), watchDogBark, FALLING);

  /* Set up i2c */
  Wire.begin();

  /* Startup Debug */
  Serial.begin(9600);

  /* Reset sensor */
  Wire.beginTransmission(SI7020_ADDR);
  Wire.endTransmission();

  /* wait before printing the reset reason for the serial port to settle */
  delay(5000);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    hasWifi = false;
  }

  if (hasWifi)
  {
    String fv = WiFi.firmwareVersion();
    if (fv < WIFI_FIRMWARE_LATEST_VERSION)
    {
      Serial.print("Your current firmware NINA FW v");
      Serial.println(fv);
      Serial.print("Please upgrade the firmware to NINA FW v");
      Serial.println(WIFI_FIRMWARE_LATEST_VERSION);
    }
  }
  /* Reset Reason */
  if (REG_PM_RCAUSE == 0b1000000)
  {
    Serial.println("Firmware updated / Soft Reset");
  }
  else
  {
    Serial.print("Device Reset (cause: 0b"); Serial.print(REG_PM_RCAUSE, BIN); Serial.println(")");
  }

  WiFiStorageFile file = WiFiStorage.open(FILEPATH);

  if (file)
  {
    if (file.size() == sizeof(settings_t))
    {
      file.read(&current_settings, sizeof(settings_t));
      Serial.println("Settings loaded:");
    }
    else
    {
      defaults();
    }
  }
  else
  {
    Serial.println("Error loading settings");
    defaults();
  }

  Serial.print("WIFI SSID:      "); Serial.println(current_settings.wifi_ssid);
  Serial.print("WIFI Pass:      "); Serial.println(current_settings.wifi_pass);
  Serial.print("Device Title:   "); Serial.println(current_settings.device_title);
  Serial.print("Engage Under:   "); Serial.println(current_settings.on_under);
  Serial.print("Disengage Over: "); Serial.println(current_settings.off_over);
  Serial.print("Reporting IP:   "); Serial.println(current_settings.report_ip);
  Serial.print("Reporting Port: "); Serial.println(current_settings.report_port);

  /* clear watchdog? */
  watchDogBark();

  if (hasWifi)
  {
    char buf[48] = "BLEFloor ";
    strncpy(&buf[9], current_settings.device_title, sizeof(current_settings.device_title));
    WiFi.setHostname(buf);

    uint8_t attempts_max = 30;
    
    /* connect to wifi */
    while (wifi_status != WL_CONNECTED && attempts_max--)
    {
      Serial.print("Attempting to connect to SSID: ");
      Serial.println(current_settings.wifi_ssid);
      wifi_status = WiFi.begin(current_settings.wifi_ssid, current_settings.wifi_pass);
      delay(5000);
    }

    if (attempts_max <= 0)
    {
      hasWifi = false;   
      Serial.println("WIFI timed out");   
    }
    else
    {
      server.begin();
      Serial.println("Connected.");
      Serial.print("SSID: ");
      Serial.println(WiFi.SSID());
  
      IPAddress ip = WiFi.localIP();
      Serial.print("IP Address: ");
      Serial.println(ip);
  
      long rssi = WiFi.RSSI();
      Serial.print("signal strength (RSSI):");
      Serial.print(rssi);
      Serial.println(" dBm");

      Udp.begin(31337);
      ArduinoOTA.begin(WiFi.localIP(), current_settings.device_title, "password", InternalStorage);
    }
  }
}
void defaults()
{
  strcpy(current_settings.wifi_ssid, "");
  strcpy(current_settings.wifi_pass, "");
  strcpy(current_settings.device_title, "Default");
  current_settings.on_under = 18.0f;
  current_settings.off_over = 25.0f;
  strcpy(current_settings.report_ip, "");
  current_settings.report_port = 8089;
  current_settings.temp_offset = 0.0f;
            
  update_settings();
  Serial.println("No settings available, setting defaults");
}
void update_settings()
{
  WiFiStorageFile file = WiFiStorage.open(FILEPATH);
  if (file)
  {
    file.erase();
  }
   
  file.write(&current_settings, sizeof(settings_t));
  file.close();
}
void watchDogBark()
{
  digitalWrite(PIN_WATCHDOG_OUT, HIGH);
  delay(2);
  digitalWrite(PIN_WATCHDOG_OUT, LOW);
  didBark = true;
}

float readHumidity()
{
  uint8_t data[2];
  Wire.beginTransmission(SI7020_ADDR);
  Wire.write(0xF5); /* read humity, no hold, master */
  Wire.endTransmission();

  delay(150); /* properly is pinging until no longer NACKs */

  Wire.requestFrom(SI7020_ADDR, 2);

  if (Wire.available() == 2)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
  }

  float humidity  = ((data[0] * 256.0) + data[1]);
  return (((125 * humidity) / 65536.0) - 6);
}

float readTemp()
{
  uint8_t data[2];
  Wire.beginTransmission(SI7020_ADDR);
  Wire.write(0xF3); /* read temp, no hold, master */
  Wire.endTransmission();

  delay(150); /* properly is pinging until no longer NACKs */

  Wire.requestFrom(SI7020_ADDR, 2);

  if (Wire.available() == 2)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
  }

  float temp  = ((data[0] * 256.0) + data[1]);
  return (((175.72 * temp) / 65536.0) - 46.85) + current_settings.temp_offset + (relayEnabled == HIGH ? -2.5f : 0.0f); // self-heating due to relay coil
}

void setRelay(int state)
{
  if (state == HIGH)
  {
    Serial.println("Relay Engaging");
  }
  else
  {
    Serial.println("Relay Disengaging");
  }

  digitalWrite(PIN_RELAY, state);
  digitalWrite(LED_BUILTIN, state);
}

void setConfig(char* key, char* val)
{
  bool updated = false;
  if (!strncmp(key, "ssid", 4))
  {
    strcpy(current_settings.wifi_ssid, val);
    updated = true;
    Serial.println("Updated SSID");
  }
  if (!strncmp(key, "pass", 4))
  {
    strcpy(current_settings.wifi_pass, val);
    updated = true;
    Serial.println("Updated Password");
  }
  if (!strncmp(key, "title", 5))
  {
    strcpy(current_settings.device_title, val);
    char buf[48] = "BLEFloor ";
    strncpy(&buf[9], current_settings.device_title, sizeof(current_settings.device_title));
    WiFi.setHostname(buf);
    updated = true;
    Serial.println("Updated Title");
  }

  if (!strncmp(key, "rip", 3))
  {
    strncpy(current_settings.report_ip, val, 16);
    updated = true;
    Serial.println("Updated Reporting IP");
  }

  if (!strncmp(key, "rport", 5))
  {
    current_settings.report_port = atoi(val);
    updated = true;
    Serial.println("Updated Reporting Port");
  }
  
  if (!strncmp(key, "on", 2))
  {
    current_settings.on_under = strtof(val, NULL);  
    if (current_settings.on_under < 5.0f)
    {
      current_settings.on_under = 5.0f;
    }
    updated = true;
    Serial.println("Updated Thermostat ON");
  }
  if (!strncmp(key, "to", 3))
  {    
    current_settings.temp_offset = strtof(val, NULL);  
    updated = true;
    Serial.println("Updated Thermostat Temp Offset");
  }
     
  if (!strncmp(key, "off", 3))
  {
    current_settings.off_over = strtof(val, NULL);  
    if (current_settings.off_over > 50.0f)
    {
      current_settings.off_over = 50.0f;
    }
    updated = true;
    Serial.println("Updated Thermostat OFF");
  }
  
  if (!strncmp(key, "save", 4))
  {
    Serial.println("Saving Config...");
    update_settings();
    updated = true;
  }

  if (!strncmp(key, "reboot", 6))
  {
    Serial.println("Rebooting");
    NVIC_SystemReset();
  }

  if (!strncmp(key, "relay", 5))
  {
    if (!strncmp(val, "on", 2))
    {
      relayEnabled = HIGH;
    } 
    else
    {
      relayEnabled = LOW;
    }
    updated = true;
    Serial.print("Set relay: "); Serial.println(relayEnabled);
  }
  
  if (!updated)
  {
    Serial.print("Unknown Key: "); Serial.println(key);
  }  
}

void handleHttp()
{
  if (!hasWifi) return;
  
  bool firstChar = true;
  bool isGet = false;
  bool isPost = false;
  WiFiClient client = server.available();
  if (client)
  {
    Serial.println("new request");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();

        if (firstChar)
        {
          firstChar = false;

          if (c == 'G')
          {
            Serial.println("GET request");
            isGet = true;
          }

          if (c == 'P')
          {
            Serial.println("POST or PUT request");
            isPost = true;
          }
        }
        Serial.write(c);
        if (c == '\n' && currentLineIsBlank)
        {
          if (isGet)
          {
            // send a standard http response header
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println("Refresh: 30"); 
            client.println();
            client.println("<!DOCTYPE HTML>");
            client.print("<html><head><meta charset=\"UTF-8\"><title>BLEFloor Thermostat - ");
            client.print(current_settings.device_title);
            client.println("</title></head><body>");
            client.print("<h1>"); client.print(current_settings.device_title); client.println(": Current Sensor Values</h1>");
            client.println("<dl>");
            client.println("<dt>Temperature</dt>");
            client.print("<dd>"); client.print(last_temp); client.print("ºC</dd>");
            client.println("<dt>Humidity</dt>");
            client.print("<dd>"); client.print(last_rh); client.print("%</dd>");
            client.println("<dt>Relay</dt>");
            client.print("<dd>"); client.print(relayEnabled); client.print("</dd>");
            client.println("<dt>Thermostat ON UNDER</dt>");
            client.print("<dd>"); client.print(current_settings.on_under); client.print("ºC</dd>");
            client.println("<dt>Thermostat OFF OVER</dt>");
            client.print("<dd>"); client.print(current_settings.off_over); client.print("ºC</dd>");            
            client.println("</dl>");
          }

          if (isPost)
          {
            char buf[1000];
            uint16_t bufidx = 0;
            memset(buf, 0, 1000);
            
            while(client.available() && bufidx < 1000)
            {
              buf[bufidx++] = client.read();                            
            }
            //Serial.println("POST/PUT Content:");
            //Serial.println(buf);
        
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");  // the connection will be closed after completion of the response
            client.println();
            client.println("OK");
            
            char* token;
            char* rest;
            token = strtok_r(buf, "&", &rest);
            do 
            {
              char* key;
              char* val;
              char* rst;
              //Serial.print("Token: ");
              //Serial.println(token);    

              key = strtok_r(token, "=", &rst);
              //Serial.print("Key: "); Serial.println(tok);
              val = strtok_r(NULL, "=", &rst);
              //Serial.print("Value: "); Serial.println(tok);              
              setConfig(key, val);
              
            } while ((token = strtok_r(NULL, "&", &rest)));
          }
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);

    client.stop();
    Serial.println("request complete");
  }
}

void handleWIFI()
{
   if (!hasWifi) return;
   int status = WiFi.status();
   if (status==WL_DISCONNECTED || status==WL_CONNECTION_LOST)
   {
    Serial.println("WIFI disconnected, reconnecting...");
     
    uint8_t retry_count = 30;
    while (status != WL_CONNECTED && retry_count--)
    {
      status = WiFi.begin(current_settings.wifi_ssid, current_settings.wifi_pass);
      delay(5000);
    }  

    if (retry_count > 0)
    {
      Serial.println("WIFI Re-Connected.");
      Serial.print("SSID: ");
      Serial.println(WiFi.SSID());
  
      IPAddress ip = WiFi.localIP();
      Serial.print("IP Address: ");
      Serial.println(ip);
  
      long rssi = WiFi.RSSI();
      Serial.print("signal strength (RSSI):");
      Serial.print(rssi);
      Serial.println(" dBm");
      Udp.stop();
      Udp.begin(31337);
    }
    else
    {
      Serial.println("WIFI Failed");
      WiFi.end();   
      WiFi.disconnect();
    }
  }
}

char* dev_title_escaped(char* buf)
{
  
  strncpy(buf, current_settings.device_title,32);
  for (uint8_t i = 0; i < strlen(buf); i++)
  {
    if (buf[i] == ' ')
    {
      buf[i] = '-';
    }
  }
  return buf;
}

bool rebooted = true;

void handleReport()
{ 
  if (!hasWifi) return;

  char buf[255] = { 0 };
  char buf2[32] = { 0 };
  strcpy(buf, "report,device=");
  strcpy(&buf[strlen(buf)], dev_title_escaped(buf2));

  uint8_t start = strlen(buf);  
  sprintf(&buf[start], " temp=%f         ", last_temp);
  
  bool attempt = Udp.beginPacket(current_settings.report_ip, current_settings.report_port);  
  if (attempt)
  {
    Serial.println(buf);
    Udp.write(buf);
    Udp.endPacket();
  } 
  else 
  {
    Serial.println("Error making temp report.");  
  }


  sprintf(&buf[start], " rh=%f          ", last_rh);
  attempt = Udp.beginPacket(current_settings.report_ip, current_settings.report_port);  
  if (attempt)
  {
    Serial.println(buf);
    Udp.write(buf);
    Udp.endPacket();
  } 
  else 
  {
    Serial.println("Error making humidity report.");  
  }

  sprintf(&buf[start], " relay=%d          ", relayEnabled);
  attempt = Udp.beginPacket(current_settings.report_ip, current_settings.report_port);  
  if (attempt)
  {
    Serial.println(buf);
    Udp.write(buf);
    Udp.endPacket();
  } 
  else 
  {
    Serial.println("Error making relay report.");  
  }

  if (rebooted)
  {
    rebooted = false;
    sprintf(&buf[start], " rebooted=1          ");
    attempt = Udp.beginPacket(current_settings.report_ip, current_settings.report_port);  
    if (attempt)
    {
      Serial.println(buf);
      Udp.write(buf);
      Udp.endPacket();
    } 
    else 
    {
      Serial.println("Error making relay report.");  
    }
  }
}

void loop()
{
  if (didBark)
  {
    didBark = false;

    last_temp = readTemp();
    last_rh = readHumidity();

    if (current_settings.on_under > current_settings.off_over)
    {
      Serial.println("Invalid temperature settings, ignoring");
    }
    else
    {
      if (last_temp <= current_settings.on_under && relayEnabled != HIGH)
      {
        relayEnabled = HIGH;
      }

      if (last_temp >= current_settings.off_over && relayEnabled != LOW)
      {
        relayEnabled = LOW;
      }
    }

    handleWIFI();
    handleReport();
    
    Serial.println("Watchdog:       Woof");
    Serial.print("Temperature:    "); Serial.print(last_temp); Serial.println("'C");
    Serial.print("Humidity:       "); Serial.print(last_rh); Serial.println("%");
    Serial.print("Relay:          "); Serial.println(relayEnabled);
    Serial.print("Thermostat ON:  Under "); Serial.print(current_settings.on_under); Serial.println("'C");
    Serial.print("Thermostat Off: Over "); Serial.print(current_settings.off_over); Serial.println("'C");
  }

  handleHttp();
  if (hasWifi)
  {
    ArduinoOTA.poll();
  } 
 
  if (oldRelayEnabled != relayEnabled)
  {
    oldRelayEnabled = relayEnabled;
    setRelay(relayEnabled);
  }

  digitalWrite(LED_BUILTIN, relayEnabled);  
  delay(50);
  digitalWrite(LED_BUILTIN, !relayEnabled);  
  delay(20);
  digitalWrite(LED_BUILTIN, relayEnabled);  
  delay(50);
}
