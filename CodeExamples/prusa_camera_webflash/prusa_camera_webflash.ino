//#define HTTP_MAX_POST_WAIT 10000 // Wait up to 10 seconds to post data
//#define HTTP_TCP_BUFFER_SIZE (8 * 1460) // Use an 48K buffer size
//https://reqbin.com/
//
//PUT /c/info HTTP/1.1
//Host: connect.prusa3d.com
//Token: DFFshJocX34Gt6AdNjcR
//Fingerprint: ZTUo2W5fWkw4UjpqLWvE
//Content-Type: application/json
//Content-Length: 82
//
//{
//  "config": {
//    "name": "esp32cam-api",
//    "trigger_scheme": "TEN_SEC"
//
//  }
//}


#include <WiFi.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "esp_camera.h"

// The server URL
const char* serverEndpoint = "/c/snapshot";
const char* serverUrl = "connect.prusa3d.com";

int contentLength = 0;
char serverToken[30] = "serverToken";
char serverFingerprint[30] = "fingerPrint";

// Camera configuration for ESP32-CAM AI-Thinker
// Note: Different modules might have different pinouts
camera_config_t config = {
  .pin_pwdn = 32,
  .pin_reset = -1,
  .pin_xclk = 0,
  .pin_sscb_sda = 26,
  .pin_sscb_scl = 27,
  .pin_d7 = 35,
  .pin_d6 = 34,
  .pin_d5 = 39,
  .pin_d4 = 36,
  .pin_d3 = 21,
  .pin_d2 = 19,
  .pin_d1 = 18,
  .pin_d0 = 5,
  .pin_vsync = 25,
  .pin_href = 23,
  .pin_pclk = 22,
  // You might need to tune these settings for your own camera module.
  .xclk_freq_hz = 20000000,
  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,
  .pixel_format = PIXFORMAT_JPEG,
  .frame_size = FRAMESIZE_VGA, // May need to lower this setting if images are too largeFRAMESIZE_QVGA, VGA, FRAMESIZE_SVGA
  .jpeg_quality = 30,
  .fb_count = 1
};
void configInitCamera() {
  sensor_t * s = esp_camera_sensor_get();
  s->set_saturation(s, -1);     // -2 to 2
  s->set_contrast(s, 1);       // -2 to 2
}
bool connectWifi(String ssid, String pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  int retry = 0;
  while (retry < 10 && WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    retry++;
    delay(500);
  }

  return WiFi.status() == WL_CONNECTED;
}

bool autoConnectToWifi() {
  String ssid;
  String pass;

  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);

  ssid = String(reinterpret_cast<const char*>(conf.sta.ssid));
  pass = String(reinterpret_cast<char*>(conf.sta.password));

  Serial.println("autoConnectToWifi");
  Serial.println(ssid);
  Serial.println(pass);

  bool connected = false;
  // Check if the ESP auto connected
  if (WiFi.status() == WL_CONNECTED) {
    connected = true;
  }

  // If it didn't auto connect, try connect with the stored
  // details
  if (connected || connectWifi(ssid, pass)) {

    return true; // connected success
  }

  return false;
}

void setup() {
  delay(300);
  Serial.begin(115200);

  bool forceToDemo = true; // Will force it to always enter config mode

  //AutoConnect will try connect to Wifi using details stored in flash
  if (forceToDemo || !autoConnectToWifi()) {
    bool exitWifiConnect = false;
    String ssid = "";
    String pass = "";
    String token = "";
    String fingerprint = "";
    int stage = 0;
    Serial.setTimeout(20000); //20 seconds
    //Wait here til we get Wifi details
    while (!exitWifiConnect) {

      Serial.println("Please enter your Wifi SSID:");
      ssid = Serial.readStringUntil('\r'); // Web Flash tools use '\r\n' for line endings
      Serial.read();
      if (ssid != "") {
        Serial.println("Please enter your Wifi password:");
        pass = Serial.readStringUntil('\r');
        Serial.read();
        Serial.println("Please enter your token");
        token = Serial.readStringUntil('\r');
        Serial.read();
        Serial.println("Please enter your fingerprint");
        fingerprint = Serial.readStringUntil('\r');
        Serial.read();

        exitWifiConnect = connectWifi(ssid, pass);
      }
      if (!exitWifiConnect) {
        Serial.println("Failed to connect:");
      } else {
        Serial.println("connect");
      }
    }
  }

  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  configInitCamera();

}

void loop() {
  // Capture a photo
  Serial.println("Getting Frame buffer");
  camera_fb_t * frameBuffer = NULL;
  frameBuffer = esp_camera_fb_get();
  if (!frameBuffer) {
    Serial.println("Frame buffer could not be acquired");
    esp_camera_fb_return(frameBuffer);
    return;
  }

  WiFiClientSecure client;

  // Verify the fingerprint, if your server uses certificate pinning
  // Comment this line out if your server does not require or support certificate pinning
  //client.setCACert(serverFingerprint);
  client.setInsecure();
  if (!client.connect(serverUrl, 443)) {

    Serial.println("Connection to server failed");
  }
  else {
    uint32_t imageLength = frameBuffer->len;
    Serial.print("Content length: ");
    Serial.println(String(imageLength));
    Serial.println("Posting image");
    client.println("PUT https://" + String(serverUrl) + String(serverEndpoint) + " HTTP/1.1");
    client.println("Host: " + String(serverUrl));
    client.println("Token: " + String(serverToken));
    client.println("Fingerprint: " + String(serverFingerprint));
    client.println("Content-Type: image/jpg");
    client.println("Content-Length: " + String(imageLength));
    client.println();
    uint8_t *fbBuf = frameBuffer->buf;
    size_t fbLen = frameBuffer->len;
    for (size_t n = 0; n < fbLen; n = n + 1024) {
      if (n + 1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen % 1024 > 0) {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }
    esp_camera_fb_return(frameBuffer);
    Serial.println("Done sending");
    Serial.println("Response:");
    while (client.connected()) {
      if (client.available()) {
        String response = client.readStringUntil('\n');
        Serial.println(response.c_str());
      }
    }
    delay(3000);
    Serial.println("Stopping wifiClientSecure");
    client.stop();
    delay(50);
    Serial.println("wifiClientSecure stopped");

    // Add some delay between uploads
    delay(5000);
  }
}
