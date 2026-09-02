#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// --- Configured Pins for STM32 - ESP32 Serial2 Communication ---
#define RXD2 16  // Connects to STM32 PA9 (TX1)
#define TXD2 17  // Connects to STM32 PA10 (RX1)

// --- Wi-Fi Credentials ---
const char* ssid     = "2";                  
const char* password = "12345679";           

// --- AWS IoT Core Endpoint ---
const char* aws_endpoint = "a3kvmfldux152s-ats.iot.us-east-1.amazonaws.com"; 
const int   aws_port     = 8883;

// AWS Topics
const char* mqtt_topic_telemetry = "car_safety/telemetry"; 
const char* mqtt_topic_alert     = "car/safety/alert";

// ============================================================================
// AWS CERTIFICATES & KEYS (PROGMEM)
// ============================================================================

// 1. Amazon Root CA 1
const char AWS_CERT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

// 2. Device Certificate
const char AWS_CERT_CRT[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----
MIIDWTCCAkGgAwIBAgIUZY/1Vxpj8Sy2h0cA3npxhZiMGZswDQYJKoZIhvcNAQEL
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI2MDgwNDExNTQw
N1oXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMBeeOeZTFITO1d5VzXC
KjSXbM1LhRTXtViQrS1tVQo6yeIttvRsq4bDl/v1ShYrfXXDxqoKshGkkbBX+1Bg
YNRpiD4LZY0B5fLMfeQiU4SN9iRkBBhdPwbCJXK5Gofb78yQ8moi5hVWcNl5MLju
Y7H4yT1aJaDV2YXp5IeGqm/9C5V0pBCX9TiUXKu5FwrOZdelGV3lDqU9Bbb+3t7b
wHpt2m5OEWx2dQh0cuyOzBpsA+DswKBacTofe3mLcdZvQcXWfFBFklAC38pwRgz9
cD5to5QuZRy5JhbgVo/11RLWhhBI0+tEttDiE4mnUam/X+BJsu2WCobFucK8BU+U
J0kCAwEAAaNgMF4wHwYDVR0jBBgwFoAUec5MzsqoLI7XaqcICKFulOJe0ycwHQYD
VR0OBBYEFDp+iGVgEYB5NRVw+HQjQJpHuQNnMAwGA1UdEwEB/wQCMAAwDgYDVR0P
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQCd5dHJSqxOsRmq47/45mT0QLI7
GipyoYyXG1EpHOO1qporYcjblW5fehMkyYk8HNEeD6ERv8QANkRKvqN/Lc9n/UDB
NCaHK2MT2acRmrJpKEKrXJaEltU+0wS4T0U0NojnepL/cX11coBS76f0WsA0OddE
4EPAm5Ja7kJiSzI4AVLppsZne3XMl+wI/aDvc+2VZD+aTEOPRiY4q/37VInAXzJF
vOPbHtSebpQQ8Ym97bDbrvUvo8icQJn+nSP5HXYZplTBxa21M6Xwek7XPxyqYOG1
T016nQ/lNl7PjGOYMlpGvJ6Uxv/+6Ixa+6aGtqN+cnu9QA0HQkovaWbRGp3U
-----END CERTIFICATE-----
)KEY";

// 3. Private Key
const char AWS_CERT_PRIVATE[] PROGMEM = R"KEY(
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAwF5455lMUhM7V3lXNcIqNJdszUuFFNe1WJCtLW1VCjrJ4i22
9GyrhsOX+/VKFit9dcPGqgqyEaSRsFf7UGBg1GmIPgtljQHl8sx95CJThI32JGQE
GF0/BsIlcrkah9vvzJDyaiLmFVZw2XkwuO5jsfjJPVoloNXZhenkh4aqb/0LlXSk
EJf1OJRcq7kXCs5l16UZXeUOpT0Ftv7e3tvAem3abk4RbHZ1CHRy7I7MGmwD4OzA
oFpxOh97eYtx1m9BxdZ8UEWSUALfynBGDP1wPm2jlC5lHLkmFuBWj/XVEtaGEEjT
60S20OITiadRqb9f4Emy7ZYKhsW5wrwFT5QnSQIDAQABAoIBAQCZMqNWzd7Z/jbk
Et5BEcBK4czkMaBqWN8zCQThiJCQ9QCR/5YUUfbH/Dyti0rVHQ1tG9y6zonBQy5D
Ic4i1J5Ii0LVJn5ZLYnTMsePR0b76ZJ8qKoaPShUWYk5M/DNAXqXj1d+7wwNMint
B3al0DPVKCwbkA8nZyc0XnCA/d3+EEWSTRsuVAUyRvSx2JQ7KP0doO91YnTWW2Pe
rBM5e7xF/AG8RZHUe+v5UU6aqXkDauI8X3AtTl9qjDokB9LCKQoaacbIe5ec9bL3
96J2UtECg8m24cFQKl7wYnbZ+16lvM2QCvYZJxWiP3wWRDUNvLVWV1UYayIyuIZe
PBU5Pv4BAoGBAOoWh4z6Tv95ZXMxdZB0l/mIRytiNijuM9q4avrh+ex4fizptMUQ
Gdv2QV7h3vq6u278fAM0RPHpMoRZ3nBprjriqLsV387NhZV2awBJSI4qfCF3CSeN
peaPRVN7WZqFwBSw/Jug/Mw2xOXxsI22H0MPFsE0f3fM3QRC8hRVf/9NAoGBANJg
Oj9yqMQd7UTc7QL1V+SqQXSzjRncT/wXxRaAVyBwKMXibKccudRMIXXEuNMxroNv
LxHNuDOKKJYzSTmRMGhXi3QSXICuh5JOKT5RpmgsU7Rd6cqTugV7Crs19AFBdK41
lVUmghNdlMn5l0dlLTQcUfdL1/kS7jyHd8Ggq4HtAoGASgFLY09zECNh5lQZlcy2
iyYBUf7fnnsIG7q33473g0HoqexMwQxBEKA+tG92HhBQ11qtHho1PcF6vgrnXuSa
N3WW4Gae9fNVqxXf6BxC+ucFjVLjqwSGEWj0Att5TXfBbQkzI0R3B1y9TPDm3zZX
lcy8ZeJh9g7nRMShYbpSF/UCgYAA5UX6NSvAwfvbmjEsHQ1FvO/QZl9IZ+azRQqi
wOMeETwrM36Q649i9vwBe1fqFkEO8C88HSsWlRT9JrS+GP8iwpSmZtmb9qI/HjQQ
vto9gUrN7sRrB1v4YAC4sU8bnkK35yR+m05cdL6IaZaaSDT8Ds7OhbUiq1D2UwFQ
grWYNQKBgQCTF8LbNDd+7aOM0bVs+LhwC529fViFEA/bRE8eTVrIASXFzmPaCKQI
DB/sue4kN9tdk9N0BFzjqMzkIx7/pXo2tLgYebBuhAPUUr4lcfU3S6gI2FeRmcwe
Dc1XScRMBm1XF829aDyWdzjsJWdiFzyCA7Vzw2/XJO+LYg/DtyGlig==
-----END RSA PRIVATE KEY-----
)KEY";

// Global Objects
WiFiClientSecure espClient;
PubSubClient client(espClient);

// Global Sensor Variables
int   pirState     = 0;
int   co2Ppm       = 0;
float temperature  = 0.0;

// Time tracking for 5-minute PIR Cooldown (5 mins = 300,000 ms)
unsigned long lastPirAlertTime = 0;
const unsigned long PIR_COOLDOWN = 300000; 

// State Tracking Flags
bool cond2_triggered = false;
bool cond3_triggered = false;

// Function Declarations
void setupWifi();
void syncTime();
void reconnectAWS();
void parseAndEvaluateLogic(String rawData);
void sendMotorCommand();

void setupWifi() {
  delay(10);
  Serial.println("\n[WiFi] Connecting to network...");
  WiFi.begin(ssid, password);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected successfully!");
    Serial.print("[WiFi] ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Connection timeout. Retrying...");
  }
}

void syncTime() {
  Serial.print("[NTP] Syncing time with NTP Server...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\n[NTP] Time synchronized successfully!");
}

void reconnectAWS() {
  int retryCount = 0;
  while (!client.connected() && retryCount < 3) {
    Serial.print("[AWS IoT] Connecting...");
    String clientId = "ESP32_CarSafety_" + String(random(0xffff), HEX);
   
    if (client.connect(clientId.c_str())) {
      Serial.println(" Connected to AWS IoT Core!");
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 2 seconds...");
      delay(2000);
      retryCount++;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===========================================");
  Serial.println("  ESP32 AWS IoT Car Safety Gateway Started ");
  Serial.println("===========================================");

  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial2.setTimeout(50);

  setupWifi();
  syncTime(); 

  espClient.setCACert(AWS_CERT_CA);
  espClient.setCertificate(AWS_CERT_CRT);
  espClient.setPrivateKey(AWS_CERT_PRIVATE);

  client.setServer(aws_endpoint, aws_port);
  client.setKeepAlive(60);
  client.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setupWifi();
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnectAWS();
    }
    client.loop();
  }

  if (Serial2.available() > 0) {
    String rawData = Serial2.readStringUntil('\n');
    rawData.trim(); 

    String cleanData = "";
    for (size_t i = 0; i < rawData.length(); i++) {
      if (rawData[i] >= 32 && rawData[i] <= 126) {
        cleanData += rawData[i];
      }
    }

    if (cleanData.length() > 5) {
      Serial.print("\n[STM32 RAW]: ");
      Serial.println(cleanData);

      parseAndEvaluateLogic(cleanData);

      String jsonPayload = "{\"pir\":" + String(pirState) + 
                           ",\"co2\":" + String(co2Ppm) + 
                           ",\"temp\":" + String(temperature, 1) + "}";

      Serial.print("[Publishing JSON]: ");
      Serial.println(jsonPayload);

      if (client.connected()) {
        client.publish(mqtt_topic_telemetry, jsonPayload.c_str());
      }
    }
  }

  delay(10);
}

void parseAndEvaluateLogic(String data) {
  int pirIdx  = data.indexOf("PIR:");
  int co2Idx  = data.indexOf("CO2:");
  int tempIdx = data.indexOf("TEMP:");

  if (pirIdx != -1 && co2Idx != -1 && tempIdx != -1) {
    int firstComma = data.indexOf(',', pirIdx);
    pirState       = data.substring(pirIdx + 4, firstComma).toInt();

    int secondComma = data.indexOf(',', co2Idx);
    co2Ppm          = data.substring(co2Idx + 4, secondComma).toInt();

    temperature     = data.substring(tempIdx + 5).toFloat();

    Serial.println("--------------- PARSED DATA ---------------");
    Serial.printf(" Motion (PIR)  : %s\n", pirState ? "DETECTED" : "CLEAR");
    Serial.printf(" Air Quality   : %d PPM\n", co2Ppm);
    Serial.printf(" Temperature   : %.1f °C\n", temperature);
    Serial.println("-------------------------------------------");

    unsigned long currentTime = millis();

    // =========================================================================
    // CONDITION 1: PIR Motion Alert (With 5-Minute Timer Cooldown)
    // =========================================================================
    if (pirState == 1) {
      // If it is the first alert or 5 minutes have passed since the last alert
      if (lastPirAlertTime == 0 || (currentTime - lastPirAlertTime >= PIR_COOLDOWN)) {
        if (client.connected()) {
          client.publish(mqtt_topic_alert, "{\"alert\":\"ALERT: Motion detected inside car!\"}");
        }
        Serial.println(">>> [AWS ALERT 1]: Motion Detected (5-Min Timer Reset) <<<");
        lastPirAlertTime = currentTime; // Update timer timestamp
      } else {
        Serial.println("[INFO]: Motion active, but 5-min alert cooldown is active.");
      }
    }

    // =========================================================================
    // CONDITION 2: PIR Detected + Rising Heat (>= 30°C) OR CO2 (2900 - 3000 PPM)
    // =========================================================================
    if (pirState == 1 && ((co2Ppm >= 2900 && co2Ppm <= 3000) || temperature >= 30.0)) {
      if (!cond2_triggered) {
        if (client.connected()) {
          client.publish(mqtt_topic_alert, "{\"alert\":\"WARNING: Suffocation or Heat rising inside car!\"}");
        }
        Serial.println(">>> [AWS ALERT 2]: Warning Suffocation/Heat <<<");
        cond2_triggered = true;
      }
    } else {
      cond2_triggered = false; // Reset trigger when conditions cool down
    }

    // =========================================================================
    // CONDITION 3: Critical Danger (CO2 > 2400 AND Temp > 27°C) -> Trigger Motor
    // =========================================================================
    if (co2Ppm > 2400 && temperature > 27.0) {
      if (!cond3_triggered) {
        if (client.connected()) {
          client.publish(mqtt_topic_alert, "{\"alert\":\"CRITICAL_ALERT: Severe CO2 & Extreme Heat! Opening Window!\"}");
        }
        Serial.println(">>> [AWS ALERT 3]: CRITICAL EMERGENCY ALERT! <<<");

        sendMotorCommand(); // Send command to STM32 over UART
        cond3_triggered = true;
      }
    } else {
      cond3_triggered = false; // Reset trigger when back to safety
    }

  } else {
    Serial.println("[WARNING] Data format mismatch!");
  }
}

void sendMotorCommand() {
  Serial.println("[ESP32 -> STM32] Transmitting Command: ROTATE_MOTOR");
  Serial2.println("ROTATE_MOTOR");
}
