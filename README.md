# Simple-Aquascape-Controller-esp32
Well, it's just a fun project because I have an aquascape at home. But you can try it, because it's simple, just using local wifi and Blynk, it can be used for your practice and development!

Cauntion!!
============================================================
 *  AQUASCAPE IoT Controller — ESP32  v5.0  (Blynk IoT)
 * ============================================================
 *  Hardware: ESP32 Dev Module
 *    Relay1  GPIO 2   — Pump / Filter
 *    Relay2  GPIO 15  — Light
 *    Buzzer  GPIO 27
 *    LCD I2C SDA=21, SCL=22  (16×2, addr 0x27)
 *    //Adjust according to your GPIO pin
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
