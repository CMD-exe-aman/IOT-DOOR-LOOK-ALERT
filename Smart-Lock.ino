// Full ESP32-CAM (AI-Thinker) sketch
// Features:
// - reed door sensor (GPIO32)
// - flash LED indicator (GPIO4)
// - active buzzer (GPIO33)
// - capture image (esp_camera)
// - send WhatsApp via UltraMsg (base64 image)
// - send Email (Gmail SMTP) with image attachment (base64 MIME)
// - debounce + cooldown

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ========== USER CONFIG - REPLACE =============
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const char* SENDER_EMAIL = "you@gmail.com";
const char* RECIPIENT_EMAIL = "recipient@gmail.com";
const char* APP_PASSWORD = "YOUR_16_CHAR_GOOGLE_APP_PASSWORD"; // Gmail App Password

// UltraMsg config
const char* ULTRAMSG_INSTANCE = "YOUR_INSTANCE_ID";
const char* ULTRAMSG_TOKEN = "YOUR_ULTRAMSG_TOKEN";
const char* RECIPIENT_PHONE = "+911234567890"; // e.g. +14155552671

// ========== PINS ==========
const int REED_PIN   = 32;  // door sensor (INPUT_PULLUP)
const int FLASH_LED  = 4;   // camera flash / indicator (AI-Thinker)
const int BUZZER_PIN = 33;  // active buzzer (digital HIGH = beep)

// ========== CAMERA (AI-Thinker) pin map ==========
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ========== SMTP / Service ==========
const char* SMTP_HOST = "smtp.gmail.com";
const uint16_t SMTP_PORT = 465; // SSL

WiFiClientSecure secureClient;
WiFiClient httpClient; // for plain HTTP (UltraMsg via HTTPS we use HTTPClient)

// ========== state ==========
int lastState = HIGH;
unsigned long lastDebounceMs = 0;
const unsigned long DEBOUNCE_MS = 80;

unsigned long lastEmailTs = 0;
const unsigned long EMAIL_COOLDOWN_MS = 60UL * 1000UL; // 60s cooldown

// ========== util: base64 encode bytes ==========
String base64_encode_bytes(const uint8_t *data, size_t len) {
  static const char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  out.reserve(((len + 2) / 3) * 4 + 4);

  size_t i = 0;
  while (i + 2 < len) {
    uint32_t triple = (data[i] << 16) | (data[i + 1] << 8) | (data[i + 2]);
    out += tbl[(triple >> 18) & 0x3F];
    out += tbl[(triple >> 12) & 0x3F];
    out += tbl[(triple >> 6) & 0x3F];
    out += tbl[triple & 0x3F];
    i += 3;
  }
  if (i < len) {
    uint32_t triple = data[i] << 16;
    if (i + 1 < len) triple |= (data[i + 1] << 8);
    out += tbl[(triple >> 18) & 0x3F];
    out += tbl[(triple >> 12) & 0x3F];
    if (i + 1 < len) out += tbl[(triple >> 6) & 0x3F]; else out += '=';
    out += '=';
  }
  while (out.length() % 4) out += '=';
  return out;
}

// ========== helper: wait SMTP response ==========
String waitResponse(WiFiClientSecure &c, unsigned long timeoutMs, const char* expectedCode, bool &ok) {
  unsigned long start = millis();
  String resp = "";
  ok = false;
  while (millis() - start < timeoutMs) {
    while (c.available()) {
      String line = c.readStringUntil('\n');
      line.trim();
      if (line.length()) resp += line + "\n";
    }
    if (resp.length()) break;
    delay(10);
  }
  if (expectedCode != NULL && resp.length() >= 3) {
    ok = (resp.substring(0, 3) == String(expectedCode));
  } else {
    ok = resp.length() > 0;
  }
  return resp;
}

void smtpPrint(WiFiClientSecure &c, const String &s) {
  c.print(s + "\r\n");
}

// ========== send email with image attachment (MIME multipart) ==========
bool sendEmailWithAttachment(const char* subject, const uint8_t* imgData, size_t imgLen) {
  if (millis() - lastEmailTs < EMAIL_COOLDOWN_MS) {
    Serial.println("Email cooldown active. Skipping email.");
    return false;
  }

  Serial.println("Connecting to SMTP...");
  secureClient.setInsecure(); // NOTE: Insecure - for simplicity; consider certificate validation
  if (!secureClient.connect(SMTP_HOST, SMTP_PORT)) {
    Serial.println("SMTP connect failed");
    return false;
  }

  bool ok;
  String resp = waitResponse(secureClient, 7000, "220", ok);
  if (!ok) { Serial.println(resp); secureClient.stop(); return false; }

  smtpPrint(secureClient, "EHLO esp32");
  resp = waitResponse(secureClient, 5000, "250", ok);
  if (!ok) { Serial.println(resp); secureClient.stop(); return false; }

  smtpPrint(secureClient, "AUTH LOGIN");
  resp = waitResponse(secureClient, 5000, "334", ok);
  if (!ok) { Serial.println(resp); secureClient.stop(); return false; }

  // send base64 username (email)
  smtpPrint(secureClient, base64_encode_bytes((const uint8_t*)SENDER_EMAIL, strlen(SENDER_EMAIL)));
  resp = waitResponse(secureClient, 5000, "334", ok);
  if (!ok) { Serial.println(resp); secureClient.stop(); return false; }

  // send base64 password (app password)
  smtpPrint(secureClient, base64_encode_bytes((const uint8_t*)APP_PASSWORD, strlen(APP_PASSWORD)));
  resp = waitResponse(secureClient, 7000, "235", ok);
  if (!ok) { Serial.println(resp); secureClient.stop(); return false; }

  // MAIL FROM
  smtpPrint(secureClient, String("MAIL FROM:<") + SENDER_EMAIL + ">");
  resp = waitResponse(secureClient, 5000, "250", ok);
  if (!ok) { Serial.println(resp); secureClient.stop(); return false; }

  // RCPT TO
  smtpPrint(secureClient, String("RCPT TO:<") + RECIPIENT_EMAIL + ">");
  resp = waitResponse(secureClient, 5000, "250", ok);
  if (!ok) { Serial.println(resp); secureClient.stop(); return false; }

  // DATA
  smtpPrint(secureClient, "DATA");
  resp = waitResponse(secureClient, 5000, "354", ok);
  if (!ok) { Serial.println(resp); secureClient.stop(); return false; }

  // Prepare MIME multipart
  String boundary = "----ESP32CAMBOUNDARY_123456";
  smtpPrint(secureClient, String("From: ") + SENDER_EMAIL);
  smtpPrint(secureClient, String("To: ") + RECIPIENT_EMAIL);
  smtpPrint(secureClient, String("Subject: ") + subject);
  smtpPrint(secureClient, "MIME-Version: 1.0");
  smtpPrint(secureClient, String("Content-Type: multipart/mixed; boundary=") + boundary);
  smtpPrint(secureClient, "");
  smtpPrint(secureClient, String("--") + boundary);
  smtpPrint(secureClient, "Content-Type: text/plain; charset=utf-8");
  smtpPrint(secureClient, "Content-Transfer-Encoding: 7bit");
  smtpPrint(secureClient, "");
  smtpPrint(secureClient, "Door opened - see attached image.");
  smtpPrint(secureClient, "");
  smtpPrint(secureClient, String("--") + boundary);

  // Attachment
  smtpPrint(secureClient, "Content-Type: image/jpeg; name=\"capture.jpg\"");
  smtpPrint(secureClient, "Content-Disposition: attachment; filename=\"capture.jpg\"");
  smtpPrint(secureClient, "Content-Transfer-Encoding: base64");
  smtpPrint(secureClient, "");

  // base64-encode image in chunks to avoid huge single String
  const size_t chunkSize = 1024; // bytes of raw data per chunk
  size_t sent = 0;
  while (sent < imgLen) {
    size_t toSend = (imgLen - sent > chunkSize) ? chunkSize : (imgLen - sent);
    String encoded = base64_encode_bytes(imgData + sent, toSend);
    // write encoded as lines of max 76 chars per RFC
    for (size_t p = 0; p < encoded.length(); p += 76) {
      smtpPrint(secureClient, encoded.substring(p, p + 76));
    }
    sent += toSend;
  }

  smtpPrint(secureClient, "");
  smtpPrint(secureClient, String("--") + boundary + "--");
  smtpPrint(secureClient, ".");
  resp = waitResponse(secureClient, 7000, "250", ok);
  if (!ok) { Serial.println(resp); secureClient.stop(); return false; }

  smtpPrint(secureClient, "QUIT");
  secureClient.stop();
  lastEmailTs = millis();
  Serial.println("Email (with attachment) sent.");
  return true;
}

// ========== send WhatsApp image via UltraMsg (base64) ==========
bool sendWhatsAppImageBase64(const uint8_t* imgData, size_t imgLen, const char* caption) {
  // UltraMsg expects the image as data URI or link. We'll send data URI: data:image/jpg;base64,<b64>
  String b64 = base64_encode_bytes(imgData, imgLen);
  String dataUri = "data:image/jpeg;base64," + b64;

  // Build URL: https://api.ultramsg.com/{instance}/messages/image
  String url = String("https://api.ultramsg.com/") + ULTRAMSG_INSTANCE + "/messages/image";

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Build body: token=...&to=...&image=...&caption=...
  String body = String("token=") + ULTRAMSG_TOKEN +
                "&to=" + RECIPIENT_PHONE +
                "&image=" + urlencode(dataUri) +
                "&caption=" + urlencode(caption);

  int httpCode = http.POST(body);
  String resp = http.getString();
  http.end();

  if (httpCode >= 200 && httpCode < 300) {
    Serial.println("WhatsApp image sent via UltraMsg.");
    return true;
  } else {
    Serial.printf("UltraMsg failed (%d): %s\n", httpCode, resp.c_str());
    return false;
  }
}

// ========== tiny URL-encode helper ==========
String urlencode(const String &str) {
  String encoded = "";
  char c;
  char buf[5];
  for (size_t i = 0; i < str.length(); i++) {
    c = str[i];
    if ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c == '-' || c == '_' || c == '.' || c == '~')) {
      encoded += c;
    } else {
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

// ========== camera init ==========
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA; // change to smaller if memory issues (QQVGA, QVGA, VGA, SVGA)
  config.jpeg_quality = 12; // 0-63 lower = better quality
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(FLASH_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(FLASH_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("Init camera...");
  if (!initCamera()) {
    Serial.println("Camera init failed - halting.");
    while (1) delay(1000);
  }

  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
    if (millis() - start > 15000) {
      Serial.println("\nFailed to connect to WiFi in 15s.");
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) Serial.println("\nWiFi connected.");
  else Serial.println("\nProceeding (network may be unavailable).");

  lastState = digitalRead(REED_PIN);
}

void beepBuzzer(unsigned long ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  int s = digitalRead(REED_PIN);

  // simple debounce
  if (s != lastState) {
    lastDebounceMs = millis();
    lastState = s;
  }
  if (millis() - lastDebounceMs < DEBOUNCE_MS) {
    delay(10);
    return;
  }

  // state changed (stable)
  static int prevState = HIGH;
  if (s != prevState) {
    prevState = s;
    if (s == HIGH) {
      Serial.println("DOOR OPEN detected!");

      // Indicator + buzzer
      digitalWrite(FLASH_LED, HIGH);
      beepBuzzer(300); // 300ms beep

      // Capture photo
      camera_fb_t * fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Camera capture failed");
      } else {
        Serial.printf("Captured image: %u bytes\n", fb->len);

        // 1) Send WhatsApp (UltraMsg)
        Serial.println("Sending WhatsApp image (UltraMsg)...");
        bool wa = sendWhatsAppImageBase64(fb->buf, fb->len, "Door opened - image attached");

        // 2) Send Email (SMTP) with attachment
        Serial.println("Sending Email with attachment...");
        bool mail = sendEmailWithAttachment("Door Open Alert", fb->buf, fb->len);

        Serial.printf("WhatsApp ok=%d, Email ok=%d\n", wa ? 1 : 0, mail ? 1 : 0);

        // free frame buffer
        esp_camera_fb_return(fb);
      }

      // small additional beep sequence
      beepBuzzer(120);
      delay(80);
      beepBuzzer(120);
    } else {
      Serial.println("DOOR CLOSED detected");
      digitalWrite(FLASH_LED, LOW);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  delay(50);
}
