/*
 * ============================================================
 *  AQUASCAPE IoT Controller — ESP32  v5.0  (Blynk IoT)
 * ============================================================
 *  Hardware:
 *    Relay1  GPIO 2   — Pump / Filter
 *    Relay2  GPIO 15  — Light
 *    Buzzer  GPIO 27
 *    LCD I2C SDA=21, SCL=22  (16×2, addr 0x27)
 *
 *  Library needed (Arduino Library Manager):
 *    • Blynk           by Volodymyr Shymanskyy  (install "Blynk" v1.x)
 *    • LiquidCrystal I2C  by Frank de Brabander
 *
 *  Blynk Virtual Pins:
 *    V0  — Relay1 button      (Button widget, Switch mode)
 *    V1  — Relay2 button      (Button widget, Switch mode)
 *    V2  — Relay1 mode        (Label widget — shows MANUAL/AUTO)
 *    V3  — Relay2 mode        (Label widget — shows MANUAL/AUTO)
 *    V4  — Relay1 AUTO btn    (Button widget, Push mode)
 *    V5  — Relay2 AUTO btn    (Button widget, Push mode)
 *    V6  — Relay1 ON time     (Time Input widget)
 *    V7  — Relay1 OFF time    (Time Input widget)
 *    V8  — Relay2 ON time     (Time Input widget)
 *    V9  — Relay2 OFF time    (Time Input widget)
 *    V10 — Status label       (Label widget — shows time + relay states)
 * ============================================================
 */

// ─── BLYNK CONFIG — fill in your credentials ────────────────

#define BLYNK_TEMPLATE_ID "**********" //YOUR BLYNK TEMPLATE ID 
#define BLYNK_TEMPLATE_NAME "*********" // YOUR BLYNK TEMPLATE NAME
#define BLYNK_AUTH_TOKEN "**********" // YOUR BLYNK TOKEN

// Comment this out to disable serial debug logs
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Preferences.h>
#include <time.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ─── PINS ────────────────────────────────────────────────────
#define RELAY1    2
#define RELAY2   15
#define BUZZER   27
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// ─── LCD ─────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─── WiFi ────────────────────────────────────────────────────
const char* WIFI_SSID = "**********"; //YOUR WIFI SSID
const char* WIFI_PASS = "**********s"; //YOUR WIFI PASSWORD

// ─── NTP ─────────────────────────────────────────────────────
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET = xxx * xxx;   // YOUR GMT
const int   DST_OFFSET = 0;

// ─── STATE ───────────────────────────────────────────────────
struct RelayState {
  bool   state;       // actual physical state
  bool   manualMode;  // true = manual overrides schedule
  bool   manualVal;   // last manual value
  String onTime;      // "HH:MM"
  String offTime;     // "HH:MM"
};

RelayState relay[2];
Preferences prefs;
BlynkTimer  timer;

// ════════════════════════════════════════════════════════════
//  HELPERS
// ════════════════════════════════════════════════════════════
String currentTime() {
  struct tm ti;
  if (!getLocalTime(&ti)) return "--:--:--";
  char buf[12];
  strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
  return String(buf);
}

void beep(int ms = 80) {
  digitalWrite(BUZZER, HIGH);
  delay(ms);
  digitalWrite(BUZZER, LOW);
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print(currentTime());
  lcd.print(Blynk.connected() ? "  B:OK " : "  B:-- ");
  lcd.setCursor(0, 1);
  String line = "R1:" + String(relay[0].state ? "ON " : "OFF") +
                "  R2:" + String(relay[1].state ? "ON " : "OFF");
  lcd.print(line.substring(0, 16));
}

// ════════════════════════════════════════════════════════════
//  RELAY CONTROL
// ════════════════════════════════════════════════════════════
void applyRelay(int idx, bool on) {
  relay[idx].state = on;
  digitalWrite((idx == 0) ? RELAY1 : RELAY2, on ? RELAY_ON : RELAY_OFF);
  updateLCD();
  Serial.printf("[RELAY%d] → %s  (%s)\n",
    idx + 1, on ? "ON" : "OFF",
    relay[idx].manualMode ? "MANUAL" : "AUTO");
}

// Push current states to Blynk app
void syncBlynk() {
  if (!Blynk.connected()) return;

  // Relay button states
  Blynk.virtualWrite(V0, relay[0].state ? 1 : 0);
  Blynk.virtualWrite(V1, relay[1].state ? 1 : 0);

  // Mode labels
  Blynk.virtualWrite(V2, relay[0].manualMode ? "MANUAL" : "AUTO");
  Blynk.virtualWrite(V3, relay[1].manualMode ? "MANUAL" : "AUTO");

  // Status label
  String status = currentTime() +
    "\nR1:" + String(relay[0].state ? "ON" : "OFF") +
    " (" + String(relay[0].manualMode ? "MANUAL" : "AUTO") + ")" +
    "\nR2:" + String(relay[1].state ? "ON" : "OFF") +
    " (" + String(relay[1].manualMode ? "MANUAL" : "AUTO") + ")";
  Blynk.virtualWrite(V10, status);
}

// ════════════════════════════════════════════════════════════
//  SCHEDULE — range-based, midnight-safe
// ════════════════════════════════════════════════════════════
void checkSchedules() {
  struct tm ti;
  if (!getLocalTime(&ti)) return;

  int nowMins = ti.tm_hour * 60 + ti.tm_min;

  for (int i = 0; i < 2; i++) {
    if (relay[i].manualMode) continue;

    int onMins  = relay[i].onTime.substring(0,2).toInt()  * 60
                + relay[i].onTime.substring(3,5).toInt();
    int offMins = relay[i].offTime.substring(0,2).toInt() * 60
                + relay[i].offTime.substring(3,5).toInt();

    bool shouldBeOn;
    if (onMins == offMins) {
      shouldBeOn = false;
    } else if (onMins < offMins) {
      shouldBeOn = (nowMins >= onMins && nowMins < offMins);
    } else {
      // Overnight e.g. 23:00 → 01:00
      shouldBeOn = (nowMins >= onMins || nowMins < offMins);
    }

    if (shouldBeOn != relay[i].state) {
      applyRelay(i, shouldBeOn);
      shouldBeOn ? beep() : (beep(40), beep(40));
      syncBlynk();
    }
  }
}

// ════════════════════════════════════════════════════════════
//  PERSIST
// ════════════════════════════════════════════════════════════
void savePrefs() {
  prefs.begin("aqua", false);
  for (int i = 0; i < 2; i++) {
    prefs.putString(("on"     + String(i)).c_str(), relay[i].onTime);
    prefs.putString(("off"    + String(i)).c_str(), relay[i].offTime);
    prefs.putBool  (("manual" + String(i)).c_str(), relay[i].manualMode);
    prefs.putBool  (("mval"   + String(i)).c_str(), relay[i].manualVal);
  }
  prefs.end();
}

void loadPrefs() {
  prefs.begin("aqua", true);
  relay[0].onTime     = prefs.getString("on0",     "06:00");
  relay[0].offTime    = prefs.getString("off0",    "22:00");
  relay[0].manualMode = prefs.getBool  ("manual0", false);
  relay[0].manualVal  = prefs.getBool  ("mval0",   false);
  relay[1].onTime     = prefs.getString("on1",     "07:00");
  relay[1].offTime    = prefs.getString("off1",    "21:00");
  relay[1].manualMode = prefs.getBool  ("manual1", false);
  relay[1].manualVal  = prefs.getBool  ("mval1",   false);
  prefs.end();
}

// ════════════════════════════════════════════════════════════
//  BLYNK VIRTUAL PIN HANDLERS
// ════════════════════════════════════════════════════════════

// V0 — Relay1 ON/OFF button (Switch mode)
BLYNK_WRITE(V0) {
  int val = param.asInt();
  relay[0].manualMode = true;
  relay[0].manualVal  = val;
  applyRelay(0, val);
  val ? beep(60) : (beep(40), beep(40));
  savePrefs();
  syncBlynk();
}

// V1 — Relay2 ON/OFF button (Switch mode)
BLYNK_WRITE(V1) {
  int val = param.asInt();
  relay[1].manualMode = true;
  relay[1].manualVal  = val;
  applyRelay(1, val);
  val ? beep(60) : (beep(40), beep(40));
  savePrefs();
  syncBlynk();
}

// V4 — Relay1 AUTO button (Push mode) — release manual, resume schedule
BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    relay[0].manualMode = false;
    savePrefs();
    checkSchedules();
    syncBlynk();
  }
}

// V5 — Relay2 AUTO button (Push mode)
BLYNK_WRITE(V5) {
  if (param.asInt() == 1) {
    relay[1].manualMode = false;
    savePrefs();
    checkSchedules();
    syncBlynk();
  }
}

// V6 — Relay1 ON time  (Time Input widget, single time)
BLYNK_WRITE(V6) {
  long t = param[0].asLong();   // seconds since midnight
  int h = t / 3600;
  int m = (t % 3600) / 60;
  char buf[6];
  sprintf(buf, "%02d:%02d", h, m);
  relay[0].onTime = String(buf);
  savePrefs();
  if (!relay[0].manualMode) checkSchedules();
  Serial.printf("[SCHED] R1 ON → %s\n", relay[0].onTime.c_str());
}

// V7 — Relay1 OFF time
BLYNK_WRITE(V7) {
  long t = param[0].asLong();
  int h = t / 3600;
  int m = (t % 3600) / 60;
  char buf[6];
  sprintf(buf, "%02d:%02d", h, m);
  relay[0].offTime = String(buf);
  savePrefs();
  if (!relay[0].manualMode) checkSchedules();
  Serial.printf("[SCHED] R1 OFF → %s\n", relay[0].offTime.c_str());
}

// V8 — Relay2 ON time
BLYNK_WRITE(V8) {
  long t = param[0].asLong();
  int h = t / 3600;
  int m = (t % 3600) / 60;
  char buf[6];
  sprintf(buf, "%02d:%02d", h, m);
  relay[1].onTime = String(buf);
  savePrefs();
  if (!relay[1].manualMode) checkSchedules();
  Serial.printf("[SCHED] R2 ON → %s\n", relay[1].onTime.c_str());
}

// V9 — Relay2 OFF time
BLYNK_WRITE(V9) {
  long t = param[0].asLong();
  int h = t / 3600;
  int m = (t % 3600) / 60;
  char buf[6];
  sprintf(buf, "%02d:%02d", h, m);
  relay[1].offTime = String(buf);
  savePrefs();
  if (!relay[1].manualMode) checkSchedules();
  Serial.printf("[SCHED] R2 OFF → %s\n", relay[1].offTime.c_str());
}

// Called when Blynk app connects — sync all states to app
BLYNK_CONNECTED() {
  Serial.println("[BLYNK] Connected — syncing state");
  Blynk.syncVirtual(V0, V1, V4, V5, V6, V7, V8, V9);
  syncBlynk();
}

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== AQUASCAPE BLYNK CONTROLLER ===");

  pinMode(RELAY1, OUTPUT); digitalWrite(RELAY1, RELAY_OFF);
  pinMode(RELAY2, OUTPUT); digitalWrite(RELAY2, RELAY_OFF);
  pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("  AQUASCAPE IoT ");
  lcd.setCursor(0, 1); lcd.print("   Starting...  ");
  delay(1500); lcd.clear();

  // Default state
  relay[0] = { false, false, false, "06:00", "22:00" };
  relay[1] = { false, false, false, "07:00", "21:00" };
  loadPrefs();

  for (int i = 0; i < 3; i++) { beep(50); delay(80); }

  // WiFi + Blynk
  lcd.setCursor(0, 0); lcd.print("Connecting...   ");
  lcd.setCursor(0, 1); lcd.print(WIFI_SSID);

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);

  // NTP sync after WiFi is up
  configTime(GMT_OFFSET, DST_OFFSET, NTP_SERVER);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Syncing NTP...  ");
  struct tm ti;
  int ntpTries = 0;
  while (!getLocalTime(&ti) && ntpTries < 20) { delay(500); ntpTries++; }
  Serial.println("Time: " + currentTime());
  lcd.setCursor(0, 1); lcd.print(currentTime());
  delay(1000);

  // Restore relay states on boot
  if (relay[0].manualMode) applyRelay(0, relay[0].manualVal);
  if (relay[1].manualMode) applyRelay(1, relay[1].manualVal);
  checkSchedules();
  updateLCD();

  // Timer: schedule check every 10 s
  timer.setInterval(10000L, checkSchedules);

  // Timer: sync Blynk + LCD every 15 s
  timer.setInterval(15000L, []() {
    syncBlynk();
    updateLCD();
  });

  // Timer: serial log every 60 s
  timer.setInterval(60000L, []() {
    Serial.printf("[%s] R1=%s(%s) R2=%s(%s) Blynk=%s\n",
      currentTime().c_str(),
      relay[0].state ? "ON":"OFF", relay[0].manualMode ? "MAN":"AUTO",
      relay[1].state ? "ON":"OFF", relay[1].manualMode ? "MAN":"AUTO",
      Blynk.connected() ? "OK" : "LOST");
  });
}

// ════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  Blynk.run();
  timer.run();
}
