Email Alert + WhatsApp Alert + Buzzer + LED + Door Sensor (Reed Switch)

This project uses an ESP32/ESP32-CAM to send real-time alerts when a door is opened.
It includes:

📩 Email Alert (Gmail SMTP)

📷 Image Capture (ESP32-CAM)

📱 WhatsApp Alert (UltraMsg API)

🔊 Buzzer Alarm

💡 LED Indicator

🚪 Reed Door Sensor

🕒 Cooldown to prevent spam alerts

⭐ Features
Feature	Description
Door Open Detection	Reed switch triggers when door opens
Email Alert	Sends subject + body + captured image
WhatsApp Alert	Sends image + text via UltraMsg API
Camera Capture	ESP32-CAM captures photo instantly
Buzzer Alarm	Activates when door opens
LED Indicator	Built-in LED turns ON when door is open
Cooldown	Prevents spam alerts

📦 Hardware Required
1. ESP32-CAM (AI Thinker)

With OV2640 camera module

FTDI USB Programmer for uploading code

2. Reed Door Sensor
  GND → GND  
VCC → 3.3V  
Signal → GPIO 32

3. Buzzer (Active Buzzer Recommended)
Positive → GPIO 12  
Negative → GND

4. LED

Uses ESP32 inbuilt LED (GPIO 2)

🧪 Software Required

Arduino IDE

ESP32 Board Manager

ESP32-CAM library support

WiFi connection

Gmail App Password

UltraMsg WhatsApp API credentials


4. LED

Uses ESP32 inbuilt LED (GPIO 2)

🧪 Software Required

Arduino IDE

ESP32 Board Manager

ESP32-CAM library support

WiFi connection

Gmail App Password

UltraMsg WhatsApp API credentials

🔧 Wiring Diagram (ESP32-CAM)
Reed Sensor (Door Switch)
Reed Sensor	ESP32-CAM
VCC	3.3V
GND	GND
OUT	GPIO 32
Buzzer
Buzzer	ESP32-CAM
+	GPIO 12
–	GND
LED
LED	ESP32-CAM
Internal LED	GPIO 2
📷 Image Capture

When the door opens:

ESP32-CAM takes a photo

Stores it in memory

Sends it:

Via Email (SMTP)

Via WhatsApp (UltraMsg)

📱 UltraMsg WhatsApp Setup

Go to:
https://ultramsg.com

Create an account

Create an "Instance"

Scan the WhatsApp QR code

Get:

instanceID

token

📩 Gmail SMTP Setup

Go to Google Account

Enable 2-Step Verification

Create App Password

Select "Mail" and "Other"

Copy 16-digit password (no spaces)

📁 File Structure
/ESP32-Door-Alert
  ├── README.md
  ├── door_alert.ino     ← MAIN CODE
  ├── captured.jpg       ← Sample capture
  └── wiring_diagram.png ← (optional


