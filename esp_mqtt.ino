#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define R1 26
#define R2 27
#define R3 32
#define R4 33

WiFiClient espClient;
PubSubClient client(espClient);
Preferences prefs;

// Initial fallback Wi-Fi
const char* default_ssid = "wifi_name";
const char* default_password = "wifi_password";

// Raspberry Pi IP
const char* mqtt_server = "pi's_IP";

String savedSSID;
String savedPASS;

void handleRelay(int pin, String msg) {
  if (msg == "ON") {
    digitalWrite(pin, LOW);   // Active LOW relay
    Serial.println("Relay ON");
  } 
  else if (msg == "OFF") {
    digitalWrite(pin, HIGH);
    Serial.println("Relay OFF");
  }
}

void connectSavedWiFi() {
  prefs.begin("wifi", true);

  savedSSID = prefs.getString("ssid", default_ssid);
  savedPASS = prefs.getString("pass", default_password);

  prefs.end();

  Serial.println("Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(savedSSID);

  WiFi.begin(savedSSID.c_str(), savedPASS.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi CONNECTED");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";

  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("] -> ");
  Serial.println(msg);

  // ----------------------------
  // WIFI UPDATE TOPIC
  // ----------------------------
  if (String(topic) == "home/wifi/update") {
    StaticJsonDocument<200> doc;

    DeserializationError error = deserializeJson(doc, msg);

    if (error) {
      Serial.println("JSON Parse Failed");
      return;
    }

    String newSSID = doc["ssid"];
    String newPASS = doc["password"];

    Serial.println("Updating WiFi credentials...");

    prefs.begin("wifi", false);
    prefs.putString("ssid", newSSID);
    prefs.putString("pass", newPASS);
    prefs.end();

    Serial.println("Credentials saved");

    WiFi.disconnect(true);
    delay(1000);

    Serial.println("Reconnecting to new WiFi...");
    WiFi.begin(newSSID.c_str(), newPASS.c_str());

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("\nConnected to NEW WiFi");
    Serial.println(WiFi.localIP());

    return;
  }

  // ----------------------------
  // RELAY CONTROL
  // ----------------------------
  if (String(topic) == "home/relay/1") handleRelay(R1, msg);
  if (String(topic) == "home/relay/2") handleRelay(R2, msg);
  if (String(topic) == "home/relay/3") handleRelay(R3, msg);
  if (String(topic) == "home/relay/4") handleRelay(R4, msg);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker... ");

    if (client.connect("ESP32_RELAY_BOARD")) {
      Serial.println("CONNECTED");

      client.subscribe("home/relay/#");
      client.subscribe("home/wifi/update");

      Serial.println("Subscribed to relay + wifi topics");
    } else {
      Serial.print("FAILED, rc=");
      Serial.println(client.state());

      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(R1, OUTPUT);
  pinMode(R2, OUTPUT);
  pinMode(R3, OUTPUT);
  pinMode(R4, OUTPUT);

  digitalWrite(R1, HIGH);
  digitalWrite(R2, HIGH);
  digitalWrite(R3, HIGH);
  digitalWrite(R4, HIGH);

  connectSavedWiFi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectSavedWiFi();
  }

  if (!client.connected()) {
    reconnect();
  }

  client.loop();
}