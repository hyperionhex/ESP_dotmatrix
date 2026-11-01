/*
 * WiFi Dot Matrix Display
 * ------------------------------------------------------------------
 * Board   : ESP32-C3 Super Mini   (Arduino IDE board: "ESP32C3 Dev Module")
 * Display : MAX7219 4-in-1 dot matrix module (FC-16 type, 32x8)
 *
 * Wiring (hardware SPI):
 *   Matrix VCC -> 5V
 *   Matrix GND -> GND
 *   Matrix DIN -> GPIO 6
 *   Matrix CS  -> GPIO 7
 *   Matrix CLK -> GPIO 4
 *
 * Arduino IDE : Tools > USB CDC On Boot = Enabled (for Serial Monitor)
 * Libraries   : MD_Parola, MD_MAX72XX, ArduinoJson (v7)
 *
 * First boot : join WiFi "DotMatrix-XXXX" and open http://192.168.4.1
 * After setup: open http://dotmatrix.local or the IP scrolled on the display
 *
 * HTTP API (for Home Assistant, scripts, etc.):
 *   GET  /api/say?text=Hello&repeat=2   show a one-off message, then resume
 *   GET  /api/say?clear=1               cancel a one-off message
 *   GET  /api/state                     full config + status as JSON
 *   POST /api/config  {"brightness":8}  change any setting (partial JSON)
 *   POST /api/draw    {"bitmap":"..."}  set pixel art (2 hex chars per column, bit0 = top row)
 *   POST /api/time    {"ms":1757590000000,"tz":"IST-5:30"}  set clock (unix ms) and/or POSIX time zone
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <SPI.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <ArduinoJson.h>
#include <vector>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include "web_ui.h"

#define FW_VERSION    "1.0.0"
#define CS_PIN        7
#define MAX_MODULES   16
#define MAX_MESSAGES  20
#define MAX_MSG_LEN   200
#define TEXT_BUF_SIZE 320
#define BLANK_COLON   '\x01'   // custom glyph: same width as ':' but empty (blinking colon)

// ---------------------------------------------------------------- effects

struct EffectDef { const char *name; textEffect_t fx; };

const EffectDef EFFECTS[] = {
  {"None",            PA_NO_EFFECT},
  {"Print",           PA_PRINT},
  {"Scroll up",       PA_SCROLL_UP},
  {"Scroll down",     PA_SCROLL_DOWN},
  {"Scroll left",     PA_SCROLL_LEFT},
  {"Scroll right",    PA_SCROLL_RIGHT},
  {"Slice",           PA_SLICE},
  {"Mesh",            PA_MESH},
  {"Fade",            PA_FADE},
  {"Dissolve",        PA_DISSOLVE},
  {"Blinds",          PA_BLINDS},
  {"Random dots",     PA_RANDOM},
  {"Wipe",            PA_WIPE},
  {"Wipe cursor",     PA_WIPE_CURSOR},
  {"Scan horizontal", PA_SCAN_HORIZ},
  {"Scan horiz. X",   PA_SCAN_HORIZX},
  {"Scan vertical",   PA_SCAN_VERT},
  {"Scan vert. X",    PA_SCAN_VERTX},
  {"Opening",         PA_OPENING},
  {"Opening cursor",  PA_OPENING_CURSOR},
  {"Closing",         PA_CLOSING},
  {"Closing cursor",  PA_CLOSING_CURSOR},
  {"Scroll up-left",  PA_SCROLL_UP_LEFT},
  {"Scroll up-right", PA_SCROLL_UP_RIGHT},
  {"Scroll down-left",  PA_SCROLL_DOWN_LEFT},
  {"Scroll down-right", PA_SCROLL_DOWN_RIGHT},
  {"Grow up",         PA_GROW_UP},
  {"Grow down",       PA_GROW_DOWN},
};
const int EFFECT_COUNT   = sizeof(EFFECTS) / sizeof(EFFECTS[0]);
const int FX_SCROLL_LEFT = 4;
const int FX_RANDOM      = 255;
const int FX_DEFAULT     = -1;

// ---------------------------------------------------------------- config

enum Mode : uint8_t { MODE_TEXT, MODE_CLOCK, MODE_DRAW };
enum Showing : uint8_t { SH_NONE, SH_TEXT, SH_ALERT, SH_CLOCK, SH_DRAW };

struct Message {
  String  text;
  bool    en    = true;
  int16_t fi    = FX_DEFAULT;   // effect in
  int16_t fo    = FX_DEFAULT;   // effect out
  int16_t al    = -1;           // align
  int32_t sp    = -1;           // speed (ms per frame)
  int32_t pa    = -1;           // pause (ms)
};

struct Config {
  // display
  uint8_t  mode       = MODE_TEXT;
  uint8_t  fxIn       = FX_SCROLL_LEFT;
  uint8_t  fxOut      = FX_SCROLL_LEFT;
  uint8_t  align      = 1;      // 0 left, 1 center, 2 right
  uint16_t speed      = 35;
  uint16_t pause      = 2000;
  uint8_t  spacing    = 1;
  uint8_t  brightness = 3;
  bool     invert     = false;
  bool     flipV      = false;
  bool     flipH      = false;
  bool     power      = true;
  bool     autoScroll = true;
  // clock
  bool     clock24    = true;
  bool     clockSec   = false;
  bool     clockBlink = true;
  String   tz         = "UTC0";
  String   dateFmt    = "%d %b %Y";
  // night mode
  bool     nightOn    = false;
  uint8_t  nightStart = 22;
  uint8_t  nightEnd   = 7;
  uint8_t  nightBright = 0;
  bool     nightBlank = false;
  // hardware
  uint8_t  hwType     = MD_MAX72XX::FC16_HW;
  uint8_t  modules    = 4;
  // drawing
  String   bitmap;
  // network
  String   ssid, pass;
  String   host       = "dotmatrix";
  String   webPass;
};

// ---------------------------------------------------------------- globals

MD_Parola  *P = nullptr;
WebServer   server(80);
DNSServer   dnsServer;
Preferences prefs;
Config      cfg;
std::vector<Message> playlist;

bool     apMode = false;
String   apName;
Showing  showing = SH_NONE;
bool     restartDisplay = true;
int      msgIndex = -1;
char     textBuf[TEXT_BUF_SIZE];

String   alertText;
int      alertRepeat = 0;
int      alertFxIn = FX_SCROLL_LEFT, alertFxOut = FX_SCROLL_LEFT;

bool          bitmapDirty = false;
unsigned long bitmapChangedAt = 0;
unsigned long rebootAt = 0;
bool          uploadAuthorized = false;
uint8_t       blankColonGlyph[9];
const char   *timeSource = "none";   // "internet" (NTP), "device" (browser) or "none"

// ---------------------------------------------------------------- helpers

int displayCols() { return cfg.modules * 8; }

bool timeValid() { return time(nullptr) > 1700000000; }

bool localNow(struct tm &t) {
  if (!timeValid()) return false;
  time_t now = time(nullptr);
  localtime_r(&now, &t);
  return true;
}

// Current local offset from UTC in minutes (includes DST), derived from the TZ rules.
int utcOffsetMinutes() {
  time_t now = time(nullptr);
  struct tm lt, gt;
  localtime_r(&now, &lt);
  gmtime_r(&now, &gt);
  int diff = (lt.tm_hour - gt.tm_hour) * 60 + (lt.tm_min - gt.tm_min);
  if (lt.tm_year != gt.tm_year)      diff += lt.tm_year > gt.tm_year ? 1440 : -1440;
  else if (lt.tm_yday != gt.tm_yday) diff += lt.tm_yday > gt.tm_yday ? 1440 : -1440;
  return diff;
}

String strftimeNow(const char *fmt) {
  struct tm t;
  if (!localNow(t)) return "--";
  char b[64];
  strftime(b, sizeof(b), fmt, &t);
  return b;
}

String formatTime(bool withSec, char sep) {
  struct tm t;
  if (!localNow(t)) {
    String s = String("--") + sep + "--";
    if (withSec) s += String(sep) + "--";
    return s;
  }
  int h = t.tm_hour;
  if (!cfg.clock24) { h %= 12; if (h == 0) h = 12; }
  char b[16];
  if (withSec) snprintf(b, sizeof(b), cfg.clock24 ? "%02d%c%02d%c%02d" : "%d%c%02d%c%02d", h, sep, t.tm_min, sep, t.tm_sec);
  else         snprintf(b, sizeof(b), cfg.clock24 ? "%02d%c%02d" : "%d%c%02d", h, sep, t.tm_min);
  return b;
}

String ipString() {
  return apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

String expandTokens(String s) {
  if (s.indexOf('{') < 0) return s;
  s.replace("{TIME}",  formatTime(false, ':'));
  s.replace("{TIMES}", formatTime(true, ':'));
  s.replace("{DATE}",  strftimeNow(cfg.dateFmt.c_str()));
  s.replace("{DAY}",   strftimeNow("%A"));
  s.replace("{AMPM}",  strftimeNow("%p"));
  s.replace("{IP}",    ipString());
  return s;
}

// The built-in font is Latin-1 ordered: convert UTF-8 from the browser, drop anything else.
String toDisplayCharset(const String &s) {
  String out;
  out.reserve(s.length());
  size_t i = 0, n = s.length();
  while (i < n) {
    uint8_t c = (uint8_t)s[i];
    if (c < 0x80) { out += (char)c; i += 1; }
    else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
      uint16_t cp = ((c & 0x1F) << 6) | ((uint8_t)s[i + 1] & 0x3F);
      if (cp >= 0xA0 && cp < 0x100) out += (char)cp;
      i += 2;
    }
    else if ((c & 0xF0) == 0xE0) i += 3;
    else if ((c & 0xF8) == 0xF0) i += 4;
    else i += 1;
  }
  return out;
}

textEffect_t resolveFx(int idx, bool entering) {
  if (idx == FX_RANDOM) idx = random(2, EFFECT_COUNT);   // skip None / Print
  if (idx < 0 || idx >= EFFECT_COUNT) idx = FX_SCROLL_LEFT;
  textEffect_t fx = EFFECTS[idx].fx;
  if (entering && fx == PA_NO_EFFECT) fx = PA_PRINT;     // "None" as entry would show nothing
  return fx;
}

textPosition_t alignOf(int a) {
  return a == 0 ? PA_LEFT : a == 2 ? PA_RIGHT : PA_CENTER;
}

bool isNight() {
  struct tm t;
  if (!cfg.nightOn || cfg.nightStart == cfg.nightEnd || !localNow(t)) return false;
  int h = t.tm_hour;
  if (cfg.nightStart < cfg.nightEnd) return h >= cfg.nightStart && h < cfg.nightEnd;
  return h >= cfg.nightStart || h < cfg.nightEnd;
}

uint8_t hexVal(char c) {
  return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
}

// ---------------------------------------------------------------- persistence

void playlistFromJson(JsonArrayConst arr) {
  playlist.clear();
  for (JsonObjectConst o : arr) {
    if (playlist.size() >= MAX_MESSAGES) break;
    Message m;
    m.text = String(o["t"] | "").substring(0, MAX_MSG_LEN);
    m.en   = o["en"] | true;
    m.fi   = o["fi"] | FX_DEFAULT;
    m.fo   = o["fo"] | FX_DEFAULT;
    m.al   = o["al"] | -1;
    m.sp   = o["sp"] | -1;
    m.pa   = o["pa"] | -1;
    playlist.push_back(m);
  }
}

void playlistToJson(JsonArray arr) {
  for (const Message &m : playlist) {
    JsonObject o = arr.add<JsonObject>();
    o["t"] = m.text; o["en"] = m.en;
    o["fi"] = m.fi;  o["fo"] = m.fo; o["al"] = m.al;
    o["sp"] = m.sp;  o["pa"] = m.pa;
  }
}

void defaultPlaylist() {
  playlist.clear();
  Message a; a.text = "Hello World!";
  Message b; b.text = "{TIME}"; b.fi = 18; b.fo = 20; b.pa = 5000;   // opening / closing
  Message c; c.text = "{DAY} {DATE}";
  playlist.push_back(a); playlist.push_back(b); playlist.push_back(c);
}

void loadConfig() {
  prefs.begin("dm", false);
  cfg.mode        = prefs.getUChar("mode", cfg.mode);
  cfg.fxIn        = prefs.getUChar("fxIn", cfg.fxIn);
  cfg.fxOut       = prefs.getUChar("fxOut", cfg.fxOut);
  cfg.align       = prefs.getUChar("align", cfg.align);
  cfg.speed       = prefs.getUShort("speed", cfg.speed);
  cfg.pause       = prefs.getUShort("pause", cfg.pause);
  cfg.spacing     = prefs.getUChar("spacing", cfg.spacing);
  cfg.brightness  = prefs.getUChar("brightness", cfg.brightness);
  cfg.invert      = prefs.getBool("invert", cfg.invert);
  cfg.flipV       = prefs.getBool("flipV", cfg.flipV);
  cfg.flipH       = prefs.getBool("flipH", cfg.flipH);
  cfg.power       = prefs.getBool("power", cfg.power);
  cfg.autoScroll  = prefs.getBool("autoScroll", cfg.autoScroll);
  cfg.clock24     = prefs.getBool("clock24", cfg.clock24);
  cfg.clockSec    = prefs.getBool("clockSec", cfg.clockSec);
  cfg.clockBlink  = prefs.getBool("clockBlink", cfg.clockBlink);
  cfg.tz          = prefs.getString("tz", cfg.tz);
  cfg.dateFmt     = prefs.getString("dateFmt", cfg.dateFmt);
  cfg.nightOn     = prefs.getBool("nightOn", cfg.nightOn);
  cfg.nightStart  = prefs.getUChar("nightStart", cfg.nightStart);
  cfg.nightEnd    = prefs.getUChar("nightEnd", cfg.nightEnd);
  cfg.nightBright = prefs.getUChar("nightBright", cfg.nightBright);
  cfg.nightBlank  = prefs.getBool("nightBlank", cfg.nightBlank);
  cfg.hwType      = prefs.getUChar("hwType", cfg.hwType);
  cfg.modules     = prefs.getUChar("modules", cfg.modules);
  cfg.bitmap      = prefs.getString("bitmap", "");
  cfg.ssid        = prefs.getString("ssid", "");
  cfg.pass        = prefs.getString("pass", "");
  cfg.host        = prefs.getString("host", cfg.host);
  cfg.webPass     = prefs.getString("webPass", "");

  size_t len = prefs.getBytesLength("playlist");
  bool loaded = false;
  if (len > 0) {
    std::vector<char> buf(len);
    prefs.getBytes("playlist", buf.data(), len);
    JsonDocument d;
    if (!deserializeJson(d, buf.data(), len) && d.is<JsonArray>()) {
      playlistFromJson(d.as<JsonArrayConst>());
      loaded = true;
    }
  }
  if (!loaded) defaultPlaylist();
  prefs.end();

  cfg.modules = constrain(cfg.modules, 1, MAX_MODULES);
  cfg.hwType  = constrain(cfg.hwType, 0, 3);
}

void saveConfig() {
  prefs.begin("dm", false);
  prefs.putUChar("mode", cfg.mode);
  prefs.putUChar("fxIn", cfg.fxIn);
  prefs.putUChar("fxOut", cfg.fxOut);
  prefs.putUChar("align", cfg.align);
  prefs.putUShort("speed", cfg.speed);
  prefs.putUShort("pause", cfg.pause);
  prefs.putUChar("spacing", cfg.spacing);
  prefs.putUChar("brightness", cfg.brightness);
  prefs.putBool("invert", cfg.invert);
  prefs.putBool("flipV", cfg.flipV);
  prefs.putBool("flipH", cfg.flipH);
  prefs.putBool("power", cfg.power);
  prefs.putBool("autoScroll", cfg.autoScroll);
  prefs.putBool("clock24", cfg.clock24);
  prefs.putBool("clockSec", cfg.clockSec);
  prefs.putBool("clockBlink", cfg.clockBlink);
  prefs.putString("tz", cfg.tz);
  prefs.putString("dateFmt", cfg.dateFmt);
  prefs.putBool("nightOn", cfg.nightOn);
  prefs.putUChar("nightStart", cfg.nightStart);
  prefs.putUChar("nightEnd", cfg.nightEnd);
  prefs.putUChar("nightBright", cfg.nightBright);
  prefs.putBool("nightBlank", cfg.nightBlank);
  prefs.putUChar("hwType", cfg.hwType);
  prefs.putUChar("modules", cfg.modules);
  prefs.putString("bitmap", cfg.bitmap);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putString("host", cfg.host);
  prefs.putString("webPass", cfg.webPass);

  JsonDocument d;
  playlistToJson(d.to<JsonArray>());
  String out;
  serializeJson(d, out);
  prefs.putBytes("playlist", out.c_str(), out.length());
  prefs.end();
  bitmapDirty = false;
}

// ---------------------------------------------------------------- display

void updatePowerAndIntensity(bool force) {
  static int lastIntensity = -1, lastOff = -1;
  bool night = isNight();
  int intensity = night ? cfg.nightBright : cfg.brightness;
  int off = (!cfg.power || (night && cfg.nightBlank)) ? 1 : 0;
  if (force || intensity != lastIntensity) { P->setIntensity(intensity); lastIntensity = intensity; }
  if (force || off != lastOff)             { P->displayShutdown(off);    lastOff = off; }
}

void applyDisplaySettings() {
  P->setInvert(cfg.invert);
  P->setCharSpacing(cfg.spacing);
  P->setZoneEffect(0, cfg.flipV, PA_FLIP_UD);
  P->setZoneEffect(0, cfg.flipH, PA_FLIP_LR);
  updatePowerAndIntensity(true);
}

void renderBitmap() {
  MD_MAX72XX *mx = P->getGraphicObject();
  int W = displayCols();
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  mx->clear();
  for (int x = 0; x < W; x++) {
    uint8_t bits = 0;
    if ((size_t)(x * 2 + 1) < cfg.bitmap.length())
      bits = (hexVal(cfg.bitmap[x * 2]) << 4) | hexVal(cfg.bitmap[x * 2 + 1]);
    for (int y = 0; y < 8; y++) {
      bool on = bits & (1 << y);
      if (cfg.invert) on = !on;
      int sx = cfg.flipH ? W - 1 - x : x;
      int sy = cfg.flipV ? 7 - y : y;
      mx->setPoint(sy, W - 1 - sx, on);   // MD_MAX72XX column 0 is the right-hand edge
    }
  }
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

// Blocking one-shot text, used during boot / OTA before the main loop runs.
void displayStatic(const char *s) {
  strlcpy(textBuf, s, sizeof(textBuf));
  P->displayClear();
  P->displayText(textBuf, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  for (int i = 0; i < 50 && !P->displayAnimate(); i++) delay(1);
  restartDisplay = true;
}

void showText(const String &raw, int fxIn, int fxOut, int align, int speed, int pause) {
  String s = toDisplayCharset(expandTokens(raw));
  strlcpy(textBuf, s.c_str(), sizeof(textBuf));
  textEffect_t in  = resolveFx(fxIn, true);
  textEffect_t out = resolveFx(fxOut, false);
  if (cfg.autoScroll && P->getTextColumns(textBuf) > displayCols() &&
      in != PA_SCROLL_LEFT && in != PA_SCROLL_RIGHT) {
    in = out = PA_SCROLL_LEFT;
  }
  P->displayText(textBuf, alignOf(align), speed, pause, in, out);
}

void updateClock(bool force) {
  bool colonOn = true;
  if (cfg.clockBlink) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    colonOn = tv.tv_usec < 500000;
  }
  String s = formatTime(cfg.clockSec, colonOn ? ':' : BLANK_COLON);
  if (!force && s == textBuf) return;
  strlcpy(textBuf, s.c_str(), sizeof(textBuf));
  P->displayText(textBuf, alignOf(cfg.align), 0, 0, PA_PRINT, PA_NO_EFFECT);
}

void nextItem() {
  if (alertRepeat > 0) {
    alertRepeat--;
    showing = SH_ALERT;
    showText(alertText, alertFxIn, alertFxOut, cfg.align, cfg.speed, cfg.pause);
    return;
  }
  if (apMode && WiFi.softAPgetStationNum() == 0 && cfg.mode == MODE_TEXT) {
    showing = SH_ALERT;
    showText("WiFi setup: join " + apName + " > 192.168.4.1", FX_SCROLL_LEFT, FX_SCROLL_LEFT, 1, 30, 500);
    return;
  }
  if (cfg.mode == MODE_DRAW) {
    showing = SH_DRAW;
    P->displayClear();
    renderBitmap();
    return;
  }
  if (cfg.mode == MODE_CLOCK) {
    showing = SH_CLOCK;
    updateClock(true);
    return;
  }
  showing = SH_TEXT;
  int n = playlist.size();
  for (int tries = 0; tries < n; tries++) {
    msgIndex = (msgIndex + 1) % n;
    const Message &m = playlist[msgIndex];
    if (!m.en || m.text.isEmpty()) continue;
    showText(m.text,
             m.fi == FX_DEFAULT ? cfg.fxIn  : m.fi,
             m.fo == FX_DEFAULT ? cfg.fxOut : m.fo,
             m.al < 0 ? cfg.align : m.al,
             m.sp < 0 ? cfg.speed : m.sp,
             m.pa < 0 ? cfg.pause : m.pa);
    return;
  }
  showing = SH_NONE;   // nothing enabled
  P->displayClear();
}

void displayLoop() {
  if (restartDisplay) {
    restartDisplay = false;
    P->displayClear();
    nextItem();
    return;
  }
  switch (showing) {
    case SH_NONE:
    case SH_DRAW:
      break;   // static; restartDisplay handles changes
    case SH_CLOCK:
      if (P->displayAnimate()) updateClock(false);
      break;
    default:
      if (P->displayAnimate()) nextItem();
  }
}

void initDisplay() {
  P = new MD_Parola((MD_MAX72XX::moduleType_t)cfg.hwType, CS_PIN, cfg.modules);
  P->begin();

  // Blank glyph the same width as ':' so a blinking colon doesn't shift the digits.
  uint8_t tmp[8];
  uint8_t w = P->getGraphicObject()->getChar(':', sizeof(tmp), tmp);
  memset(blankColonGlyph, 0, sizeof(blankColonGlyph));
  blankColonGlyph[0] = (w > 0 && w < sizeof(blankColonGlyph)) ? w : 2;
  P->addChar(BLANK_COLON, blankColonGlyph);

  applyDisplaySettings();
}

// ---------------------------------------------------------------- web

bool guard() {
  if (apMode || cfg.webPass.isEmpty()) return true;
  if (server.authenticate("admin", cfg.webPass.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "DotMatrix");
  return false;
}

void sendJson(JsonDocument &d) {
  String out;
  serializeJson(d, out);
  server.send(200, "application/json", out);
}

void sendOk(bool reboot = false) {
  server.send(200, "application/json", reboot ? "{\"ok\":true,\"reboot\":true}" : "{\"ok\":true}");
}

void sendError(int code, const char *msg) {
  server.send(code, "application/json", String("{\"ok\":false,\"error\":\"") + msg + "\"}");
}

void fillStatus(JsonObject st) {
  st["fw"]       = FW_VERSION;
  st["ap"]       = apMode;
  st["apName"]   = apName;
  st["ip"]       = ipString();
  st["rssi"]     = apMode ? 0 : WiFi.RSSI();
  st["synced"]   = timeValid();
  st["epoch"]    = (uint32_t)time(nullptr);
  st["utcOffset"] = utcOffsetMinutes();
  st["timeSrc"]  = timeSource;
  st["time"]     = formatTime(true, ':');
  st["date"]     = strftimeNow(cfg.dateFmt.c_str());
  st["uptime"]   = millis() / 1000;
  st["heap"]     = ESP.getFreeHeap();
  st["night"]    = isNight();
  st["showing"]  = (int)showing;
  st["msgIndex"] = msgIndex;
  st["text"]     = (showing == SH_TEXT || showing == SH_ALERT) ? textBuf : "";
}

void handleRoot() {
  if (!guard()) return;
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleState() {
  if (!guard()) return;
  JsonDocument d;
  d["mode"] = cfg.mode;         d["fxIn"] = cfg.fxIn;         d["fxOut"] = cfg.fxOut;
  d["align"] = cfg.align;       d["speed"] = cfg.speed;       d["pause"] = cfg.pause;
  d["spacing"] = cfg.spacing;   d["brightness"] = cfg.brightness;
  d["invert"] = cfg.invert;     d["flipV"] = cfg.flipV;       d["flipH"] = cfg.flipH;
  d["power"] = cfg.power;       d["autoScroll"] = cfg.autoScroll;
  d["clock24"] = cfg.clock24;   d["clockSec"] = cfg.clockSec; d["clockBlink"] = cfg.clockBlink;
  d["tz"] = cfg.tz;             d["dateFmt"] = cfg.dateFmt;
  d["nightOn"] = cfg.nightOn;   d["nightStart"] = cfg.nightStart; d["nightEnd"] = cfg.nightEnd;
  d["nightBright"] = cfg.nightBright; d["nightBlank"] = cfg.nightBlank;
  d["hwType"] = cfg.hwType;     d["modules"] = cfg.modules;
  d["bitmap"] = cfg.bitmap;     d["ssid"] = cfg.ssid;         d["host"] = cfg.host;
  d["webPassSet"] = !cfg.webPass.isEmpty();
  d["cols"] = displayCols();
  playlistToJson(d["playlist"].to<JsonArray>());
  JsonArray fx = d["effects"].to<JsonArray>();
  for (int i = 0; i < EFFECT_COUNT; i++) fx.add(EFFECTS[i].name);
  fillStatus(d["status"].to<JsonObject>());
  sendJson(d);
}

void handleStatus() {
  if (!guard()) return;
  JsonDocument d;
  fillStatus(d.to<JsonObject>());
  sendJson(d);
}

#define SET_INT(key, lo, hi) if (d[#key].is<int>())         cfg.key = constrain(d[#key].as<int>(), lo, hi)
#define SET_BOOL(key)        if (d[#key].is<bool>())        cfg.key = d[#key].as<bool>()
#define SET_STR(key, maxLen) if (d[#key].is<const char*>()) cfg.key = String(d[#key].as<const char*>()).substring(0, maxLen)

void handleConfig() {
  if (!guard()) return;
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain")) || !d.is<JsonObject>()) return sendError(400, "invalid JSON");

  uint8_t oldHw = cfg.hwType, oldModules = cfg.modules;
  String  oldHost = cfg.host;

  SET_INT(mode, 0, 2);
  SET_INT(fxIn, 0, 255);
  SET_INT(fxOut, 0, 255);
  SET_INT(align, 0, 2);
  SET_INT(speed, 0, 1000);
  SET_INT(pause, 0, 60000);
  SET_INT(spacing, 0, 4);
  SET_INT(brightness, 0, 15);
  SET_BOOL(invert);
  SET_BOOL(flipV);
  SET_BOOL(flipH);
  SET_BOOL(power);
  SET_BOOL(autoScroll);
  SET_BOOL(clock24);
  SET_BOOL(clockSec);
  SET_BOOL(clockBlink);
  SET_STR(tz, 64);
  SET_STR(dateFmt, 40);
  SET_BOOL(nightOn);
  SET_INT(nightStart, 0, 23);
  SET_INT(nightEnd, 0, 23);
  SET_INT(nightBright, 0, 15);
  SET_BOOL(nightBlank);
  SET_INT(hwType, 0, 3);
  SET_INT(modules, 1, MAX_MODULES);
  SET_STR(host, 32);
  SET_STR(webPass, 64);
  if (d["playlist"].is<JsonArray>()) playlistFromJson(d["playlist"].as<JsonArrayConst>());

  saveConfig();

  if (cfg.hwType != oldHw || cfg.modules != oldModules || cfg.host != oldHost) {
    rebootAt = millis() + 1000;
    return sendOk(true);
  }
  if (!d["tz"].isNull()) { setenv("TZ", cfg.tz.c_str(), 1); tzset(); }

  applyDisplaySettings();

  static const char *restartKeys[] = {
    "mode", "fxIn", "fxOut", "align", "speed", "pause", "spacing", "invert", "flipV", "flipH",
    "autoScroll", "clock24", "clockSec", "clockBlink", "dateFmt", "tz", "playlist"
  };
  for (const char *k : restartKeys) {
    if (!d[k].isNull()) {
      if (!d["playlist"].isNull() || !d["mode"].isNull()) msgIndex = -1;
      restartDisplay = true;
      break;
    }
  }
  sendOk();
}

void handleDraw() {
  if (!guard()) return;
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) return sendError(400, "invalid JSON");
  if (d["bitmap"].is<const char*>()) {
    String b = d["bitmap"].as<const char*>();
    if ((int)b.length() > displayCols() * 2) b = b.substring(0, displayCols() * 2);
    for (size_t i = 0; i < b.length(); i++)
      if (!isxdigit((unsigned char)b[i])) return sendError(400, "bitmap must be hex");
    cfg.bitmap = b;
    bitmapDirty = true;
    bitmapChangedAt = millis();
  }
  if ((d["show"] | false) && cfg.mode != MODE_DRAW) {
    cfg.mode = MODE_DRAW;
    saveConfig();
    restartDisplay = true;
  }
  if (showing == SH_DRAW && !restartDisplay) renderBitmap();
  sendOk();
}

// Set the clock from the browser: works without internet (setup AP) and fixes the time zone.
void handleTime() {
  if (!guard()) return;
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) return sendError(400, "invalid JSON");

  double ms = d["ms"].is<double>() ? d["ms"].as<double>() : 0;
  if (!d["ms"].isNull() && ms < 1.7e12) return sendError(400, "invalid time");
  bool refresh = !timeValid();

  if (d["tz"].is<const char*>()) {
    cfg.tz = String(d["tz"].as<const char*>()).substring(0, 64);
    setenv("TZ", cfg.tz.c_str(), 1);
    tzset();
    saveConfig();
    refresh = true;
  }
  if (ms > 0) {
    struct timeval tv;
    tv.tv_sec  = (time_t)(ms / 1000.0);
    tv.tv_usec = (suseconds_t)((ms - tv.tv_sec * 1000.0) * 1000.0);
    settimeofday(&tv, nullptr);
    timeSource = "device";
  }
  if (refresh) restartDisplay = true;   // replace any "--:--" currently on screen
  sendOk();
}

void handleSay() {
  if (!guard()) return;
  if (server.hasArg("clear")) {
    alertRepeat = 0;
    restartDisplay = true;
    return sendOk();
  }
  String t = server.arg("text");
  if (t.isEmpty()) return sendError(400, "missing text");
  alertText   = t.substring(0, MAX_MSG_LEN);
  alertRepeat = server.hasArg("repeat") ? constrain(server.arg("repeat").toInt(), 1, 1000) : 1;
  alertFxIn   = server.hasArg("fxIn")  ? server.arg("fxIn").toInt()  : FX_SCROLL_LEFT;
  alertFxOut  = server.hasArg("fxOut") ? server.arg("fxOut").toInt() : FX_SCROLL_LEFT;
  restartDisplay = true;
  sendOk();
}

void handleWifi() {
  if (!guard()) return;
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) return sendError(400, "invalid JSON");
  if (!d["ssid"].is<const char*>()) return sendError(400, "missing ssid");
  cfg.ssid = d["ssid"].as<const char*>();
  if (d["pass"].is<const char*>()) cfg.pass = d["pass"].as<const char*>();
  saveConfig();
  rebootAt = millis() + 1500;
  sendOk(true);
}

void handleScan() {
  if (!guard()) return;
  int n = WiFi.scanNetworks();
  JsonDocument d;
  JsonArray arr = d.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    bool dup = false;
    for (JsonObject o : arr) if (ssid == o["ssid"].as<const char*>()) { dup = true; break; }
    if (dup) continue;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = ssid;
    o["rssi"] = WiFi.RSSI(i);
    o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendJson(d);
}

void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadAuthorized = apMode || cfg.webPass.isEmpty() || server.authenticate("admin", cfg.webPass.c_str());
    if (!uploadAuthorized) return;
    Serial.printf("OTA: %s\n", up.filename.c_str());
    displayStatic("UPDATE");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (!uploadAuthorized) {
    return;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void handleUpdateDone() {
  if (!guard()) return;
  if (Update.hasError() || !Update.isFinished()) {
    restartDisplay = true;
    return sendError(500, "update failed");
  }
  displayStatic("REBOOT");
  rebootAt = millis() + 1000;
  sendOk(true);
}

void handleNotFound() {
  if (apMode) {   // captive portal: send every unknown URL to the setup page
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
    return;
  }
  sendError(404, "not found");
}

void setupWeb() {
  server.enableCORS(true);
  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/api/state",  HTTP_GET,  handleState);
  server.on("/api/status", HTTP_GET,  handleStatus);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/draw",   HTTP_POST, handleDraw);
  server.on("/api/say",    HTTP_ANY,  handleSay);
  server.on("/api/time",   HTTP_POST, handleTime);
  server.on("/api/wifi",   HTTP_POST, handleWifi);
  server.on("/api/scan",   HTTP_GET,  handleScan);
  server.on("/api/reboot", HTTP_POST, []() {
    if (!guard()) return;
    rebootAt = millis() + 1000;
    sendOk(true);
  });
  server.on("/api/reset",  HTTP_POST, []() {
    if (!guard()) return;
    prefs.begin("dm", false);
    prefs.clear();
    prefs.end();
    rebootAt = millis() + 1000;
    sendOk(true);
  });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound(handleNotFound);
  server.begin();
}

// ---------------------------------------------------------------- wifi

void startWiFi() {
  WiFi.persistent(false);
  WiFi.setHostname(cfg.host.c_str());

  if (!cfg.ssid.isEmpty()) {
    displayStatic("WiFi..");
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
    // Most C3 Super Mini boards have a poorly matched antenna and fail to
    // associate at full power; 8.5 dBm is the widely used fix.
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(100);

    if (WiFi.status() == WL_CONNECTED) {
      WiFi.setAutoReconnect(true);
      Serial.printf("Connected: http://%s  (http://%s.local)\n", WiFi.localIP().toString().c_str(), cfg.host.c_str());
      if (MDNS.begin(cfg.host.c_str())) MDNS.addService("http", "tcp", 80);
      sntp_set_time_sync_notification_cb([](struct timeval *) { timeSource = "internet"; });
      configTzTime(cfg.tz.c_str(),"pool.ntp.org", "time.google.com", "time.cloudflare.com");
      alertText = "IP " + WiFi.localIP().toString();
      alertRepeat = 1;
      return;
    }
    Serial.println("WiFi connect failed, starting setup AP");
  }

  apMode = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);   // STA side enabled so the setup page can scan
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  apName = "DotMatrix-" + mac.substring(8);
  WiFi.softAP(apName.c_str());
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  dnsServer.start(53, "*", WiFi.softAPIP());
  setenv("TZ", cfg.tz.c_str(), 1);
  tzset();
  Serial.printf("Setup AP: %s  ->  http://192.168.4.1\n", apName.c_str());
}

// ---------------------------------------------------------------- main

void setup() {
  Serial.begin(115200);
  loadConfig();
  initDisplay();
  startWiFi();
  setupWeb();
  restartDisplay = true;
}

void loop() {
  if (apMode) dnsServer.processNextRequest();
  server.handleClient();
  displayLoop();

  static unsigned long lastTick = 0;
  if (millis() - lastTick >= 1000) {
    lastTick = millis();
    updatePowerAndIntensity(false);
    // Saved network unreachable (e.g. router was off): retry every 5 min while nobody uses the setup AP.
    if (apMode && !cfg.ssid.isEmpty() && WiFi.softAPgetStationNum() == 0 && millis() > 300000) ESP.restart();
  }

  if (bitmapDirty && millis() - bitmapChangedAt > 3000) saveConfig();   // debounce flash writes while drawing
  if (rebootAt && millis() > rebootAt) ESP.restart();
}
