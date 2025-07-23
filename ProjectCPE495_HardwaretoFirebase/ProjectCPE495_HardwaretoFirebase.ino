#include <WiFiS3.h>
#include <DHT.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Adafruit_AMG88xx.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// DHT11
#define DHTPIN 9
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// AMG8833
Adafruit_AMG88xx amg;
float pixels[64];

// WiFi
const char* ssid = "----------";
const char* password = "---------";

// Firebase
const char* firebaseHost = "--------------------";
const char* firebaseAuth = "---------------------";

WiFiSSLClient client;

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

// TFT Display
#define TFT_CS     7
#define TFT_RST    6
#define TFT_DC     5
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  dht.begin();

  if (!amg.begin()) {
    Serial.println("❌ AMG8833 not detected. Check wiring!");
    while (1);
  }
  Serial.println("AMG8833 initialized");

  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n Connected to WiFi!");

  timeClient.begin();

  // Init TFT
  tft.initR(INITR_144GREENTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.println("Loading...");
  delay(1000);
}

void loop() {
  timeClient.update();

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Error reading from DHT sensor.");
    return;
  }

  amg.readPixels(pixels);
  float thermalSum = 0.0;
  for (int i = 0; i < 64; i++) {
    thermalSum += pixels[i];
  }
  float thermalAvg = thermalSum / 64.0;

  time_t rawTime = timeClient.getEpochTime();
  setTime(rawTime);
  String dateTime = String(year()) + "-" +
                    (month() < 10 ? "0" : "") + String(month()) + "-" +
                    (day() < 10 ? "0" : "") + String(day()) + " " +
                    (hour() < 10 ? "0" : "") + String(hour()) + ":" +
                    (minute() < 10 ? "0" : "") + String(minute());

  // -------- TFT Display --------
  tft.fillScreen(ST77XX_BLACK);

  // Header
  tft.setCursor(10, 0);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Sensor");

  // Temp
  tft.setCursor(0, 30);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("Temp: ");
  tft.setTextSize(2);
  tft.print(temperature, 1);
  tft.println(" C");

  // Humidity
  tft.setCursor(0, 60);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.print("Humidity: ");
  tft.setTextSize(2);
  tft.print(humidity, 1);
  tft.println("%");

  // Thermal
  tft.setCursor(0, 90);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_MAGENTA);
  tft.print("Thermal: ");
  tft.setTextSize(2);
  tft.print(thermalAvg, 1);
  tft.println(" C");

  // -----------------------------

  // JSON Payload
  String jsonData = "{\"temperature\": " + String(temperature) +
                    ", \"humidity\": " + String(humidity) +
                    ", \"thermal_avg\": " + String(thermalAvg, 2) +
                    ", \"timestamp\": \"" + dateTime + "\"}";

  // Send to Firebase
  String url = "/sensor/data.json?auth=" + String(firebaseAuth);

  if (client.connect(firebaseHost, ---)) {
    Serial.println("Connected to Firebase via HTTPS");

    client.println("PUT " + url + " HTTP/1.1");
    client.println("Host: " + String(firebaseHost));
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(jsonData.length()));
    client.println("Connection: close");
    client.println();
    client.println(jsonData);

    Serial.println("📤 Sent to Firebase:");
    Serial.println(jsonData);

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line.length() == 0) break;
      Serial.println("Firebase response: " + line);
    }

    client.stop();
  } else {
    Serial.println("Failed to connect to Firebase");
  }

  delay(3000);
}
