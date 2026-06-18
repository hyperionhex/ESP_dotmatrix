/*
  ESP32-C3 Super Mini + MAX7219 4-in-1 Dot Matrix Display
  Control scrolling text via a web page hosted on the ESP32 itself.

  Library required (Arduino Library Manager):
    - MD_Parola
    - MD_MAX72XX

  Wiring:
    VCC -> 5V
    GND -> GND
    DIN -> GPIO 6
    CS  -> GPIO 7
    CLK -> GPIO 10
*/

#include <WiFi.h>
#include <WebServer.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// ---------- WiFi Credentials ----------
const char* ssid     = "SINE-Wireless";
const char* password = "123456789";

// ---------- Display Setup ----------
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW   // most 4-in-1 modules use FC16
#define MAX_DEVICES   4                      // 4 modules chained

#define CLK_PIN  10
#define DATA_PIN 6
#define CS_PIN   7

MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

WebServer server(80);

String currentText = "HELLO";
uint8_t scrollSpeed = 50;       // lower = faster
textEffect_t scrollEffect = PA_SCROLL_LEFT;

// ---------- Web Page (served from ESP32) ----------
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>LED Matrix Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; background:#111; color:#eee; text-align:center; padding:30px; }
    input[type=text] { width:80%%; padding:10px; font-size:18px; border-radius:6px; border:none; }
    input[type=range] { width:80%%; }
    button { padding:10px 20px; font-size:16px; margin-top:15px; border:none; border-radius:6px; background:#ff7300; color:#fff; cursor:pointer; }
    button:hover { background:#e06400; }
    h2 { color:#ff7300; }
    .box { max-width:400px; margin:auto; }
  </style>
</head>
<body>
  <div class="box">
    <h2>Dot Matrix Display Control</h2>
    <input type="text" id="msg" placeholder="Enter text..." value="%CURRENT_TEXT%">
    <br><br>
    <label>Scroll Speed</label><br>
    <input type="range" id="speed" min="10" max="150" value="%CURRENT_SPEED%">
    <br><br>
    <button onclick="sendText()">Update Display</button>
  </div>

<script>
function sendText(){
  var text = document.getElementById('msg').value;
  var speed = document.getElementById('speed').value;
  fetch('/update?text=' + encodeURIComponent(text) + '&speed=' + speed)
    .then(r => r.text())
    .then(d => console.log(d));
}
</script>
</body>
</html>
)rawliteral";

// ---------- Handlers ----------
void handleRoot() {
  String page = htmlPage;
  page.replace("%CURRENT_TEXT%", currentText);
  page.replace("%CURRENT_SPEED%", String(scrollSpeed));
  server.send(200, "text/html", page);
}

void handleUpdate() {
  if (server.hasArg("text")) {
    currentText = server.arg("text");
  }
  if (server.hasArg("speed")) {
    scrollSpeed = server.arg("speed").toInt();
  }

  myDisplay.displayClear();
  myDisplay.displayText(currentText.c_str(), PA_CENTER, scrollSpeed, 0, scrollEffect, scrollEffect);

  server.send(200, "text/plain", "OK: " + currentText);
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // wait for Serial Monitor to connect before printing

  // ---- Display init ----
  myDisplay.begin();
  myDisplay.setIntensity(5);     // brightness 0-15
  myDisplay.displayClear();

  // ---- WiFi connect ----
  myDisplay.displayText("WiFi...", PA_CENTER, 0, 1000, PA_PRINT, PA_NO_EFFECT);
  while (!myDisplay.displayAnimate()) {}  // hold message until pause elapses

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // scroll the IP on the matrix so it's always readable without Serial Monitor
  String ipMsg = "IP: " + WiFi.localIP().toString();
  static char ipBuf[32];
  ipMsg.toCharArray(ipBuf, sizeof(ipBuf));
  myDisplay.displayText(ipBuf, PA_CENTER, 60, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  while (!myDisplay.displayAnimate()) {}

  // hand off to user message
  myDisplay.displayText(currentText.c_str(), PA_CENTER, scrollSpeed, 0, scrollEffect, scrollEffect);

  // ---- Web server routes ----
  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.begin();
}

void loop() {
  server.handleClient();
  if (myDisplay.displayAnimate()) {
    myDisplay.displayReset();   // loop scrolling text continuously
  }
}
