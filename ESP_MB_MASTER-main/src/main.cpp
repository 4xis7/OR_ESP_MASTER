#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_now.h>
#include <LittleFS.h>

// =======================================================
// MASTER NODE
// N1 = GPS NODE
// N2 = CLIENT B
// N3 = CLIENT C
// =======================================================

#define RXD2 16
#define TXD2 17

#define RXD1 18
#define TXD1 19

const char* AP_SSID     = "MASTER_CONFIG";
const char* AP_PASSWORD = "12345678";

// =======================================================
// GLOBAL
// =======================================================

Preferences  prefs;
WebServer    server(80);

uint8_t peerMacs[3][6];
bool    peerReady[3] = {false, false, false};

TaskHandle_t Task1;
TaskHandle_t Task2;

void Task1code(void * pvParameters);
void Task2code(void * pvParameters);

// =======================================================
// MAC STRING -> BYTE
// =======================================================

bool macStringToBytes(String macStr, uint8_t *mac)
{
    if (macStr.length() != 17)
        return false;

    int values[6];

    if (sscanf(macStr.c_str(),
               "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1],
               &values[2], &values[3],
               &values[4], &values[5]) != 6)
    {
        return false;
    }

    for (int i = 0; i < 6; ++i)
        mac[i] = (uint8_t)values[i];

    return true;
}

// =======================================================
// ตรวจสอบ MAC Format
// =======================================================

bool isValidMac(String mac)
{
    if (mac.length() != 17)
        return false;

    for (int i = 0; i < 17; i++)
    {
        if (i == 2 || i == 5 || i == 8 ||
            i == 11 || i == 14)
        {
            if (mac[i] != ':')
                return false;
        }
        else
        {
            if (!isxdigit(mac[i]))
                return false;
        }
    }

    return true;
}

// =======================================================
// WEB HANDLERS
// =======================================================

void handleRoot()
{
    if (!LittleFS.exists("/index.html"))
    {
        server.send(404, "text/plain", "index.html not found");
        return;
    }

    File f = LittleFS.open("/index.html", "r");
    String html = "";
    while (f.available())
        html += (char)f.read();
    f.close();

    // โหลด MAC ที่บันทึกไว้
    prefs.begin("peers", true);
    String m1 = prefs.getString("m1", "NOT SET");
    String m2 = prefs.getString("m2", "NOT SET");
    String m3 = prefs.getString("m3", "NOT SET");
    prefs.end();

    // แทน placeholder
    html.replace("%MY_MAC%", WiFi.macAddress());
    html.replace("%M1_MAC%", m1);
    html.replace("%M2_MAC%", m2);
    html.replace("%M3_MAC%", m3);

    // สถานะการเชื่อมต่อ
    html.replace("%M1_STATUS%", peerReady[0] ? "CONNECTED" : "NOT CONNECTED");
    html.replace("%M2_STATUS%", peerReady[1] ? "CONNECTED" : "NOT CONNECTED");
    html.replace("%M3_STATUS%", peerReady[2] ? "CONNECTED" : "NOT CONNECTED");

    // CSS class สำหรับสี badge
    html.replace("%M1_CLASS%", peerReady[0] ? "on" : "off");
    html.replace("%M2_CLASS%", peerReady[1] ? "on" : "off");
    html.replace("%M3_CLASS%", peerReady[2] ? "on" : "off");

    server.send(200, "text/html", html);
}

void handleSaveN(int n)
{
    String key = "m" + String(n);
    String param = "m" + String(n);

    if (!server.hasArg(param))
    {
        server.send(400, "text/plain", "No MAC param");
        return;
    }

    String mac = server.arg(param);
    mac.trim();
    mac.toUpperCase();

    if (!isValidMac(mac))
    {
        server.sendHeader("Location",
            "/?msg=Invalid+MAC+Format+N" + String(n));
        server.send(302, "text/plain", "");
        return;
    }

    prefs.begin("peers", false);
    prefs.putString(key.c_str(), mac);
    prefs.end();

    Serial.println("SAVED N" + String(n) + " : " + mac);

    server.sendHeader("Location",
        "/?msg=N" + String(n) + "+Saved");
    server.send(302, "text/plain", "");
}

void handleSaveN1()  { handleSaveN(1); }
void handleSaveN2()  { handleSaveN(2); }
void handleSaveN3()  { handleSaveN(3); }

void handleRestart()
{
    server.send(200, "text/plain", "RESTARTING");
    delay(1000);
    ESP.restart();
}

// =======================================================
// LOAD PEERS
// =======================================================

void loadPeers()
{
    prefs.begin("peers", true);
    String mac1 = prefs.getString("m1", "");
    String mac2 = prefs.getString("m2", "");
    String mac3 = prefs.getString("m3", "");
    prefs.end();

    String macs[3] = {mac1, mac2, mac3};

    for (int i = 0; i < 3; i++)
    {
        if (macs[i] == "")
            continue;

        Serial.print("LOAD PEER ");
        Serial.print(i + 1);
        Serial.print(" : ");
        Serial.println(macs[i]);

        if (!macStringToBytes(macs[i], peerMacs[i]))
        {
            Serial.println("INVALID MAC");
            continue;
        }

        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, peerMacs[i], 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;

        if (esp_now_add_peer(&peerInfo) == ESP_OK)
        {
            peerReady[i] = true;
            Serial.println("PEER ADDED");
        }
        else
        {
            Serial.println("ADD PEER FAIL");
        }
    }
}

// =======================================================
// LED FUNCTIONS
// =======================================================

void wink()
{
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(100);
}

void trig_rec()
{
    digitalWrite(4, HIGH);
    delay(100);
    digitalWrite(4, LOW);
    delay(100);
    digitalWrite(4, HIGH);
}

// =======================================================
// SEND CALLBACK
// =======================================================

void OnDataSent(const uint8_t *mac_addr,
                esp_now_send_status_t status)
{
    trig_rec();
}

// =======================================================
// RECEIVE CALLBACK
// =======================================================

void OnDataRecv(const uint8_t *mac,
                const uint8_t *incomingData,
                int len)
{
    String dataIn = "";

    for (int i = 0; i < len; i++)
        dataIn += (char)incomingData[i];

    Serial.print(dataIn);
    Serial.flush();
    delay(10);
}

// =======================================================
// SETUP
// =======================================================

void setup()
{
    Serial.begin(9600);

    Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

    pinMode(2, OUTPUT);
    pinMode(4, OUTPUT);

    if (!LittleFS.begin(true))
        Serial.println("LittleFS ERROR");

    WiFi.mode(WIFI_STA);
    Serial.println(WiFi.macAddress());

    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.println("MASTER CONFIG MODE");
    Serial.println(WiFi.softAPIP());

    server.on("/",         HTTP_GET,  handleRoot);
    server.on("/save1",    HTTP_POST, handleSaveN1);
    server.on("/save2",    HTTP_POST, handleSaveN2);
    server.on("/save3",    HTTP_POST, handleSaveN3);
    server.on("/restart",  HTTP_POST, handleRestart);
    server.begin();

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP NOW ERROR");
        return;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    loadPeers();

    xTaskCreatePinnedToCore(
        Task1code, "Task1",
        10000, NULL, 1, &Task1, 0);
    delay(500);

    xTaskCreatePinnedToCore(
        Task2code, "Task2",
        10000, NULL, 1, &Task2, 1);
    delay(500);

    // Blink test
    for (int i = 0; i < 3; i++)
    {
        digitalWrite(4, HIGH); delay(500);
        digitalWrite(4, LOW);  delay(500);
    }
    digitalWrite(4, HIGH);
}

// =======================================================
// TASK1 - Blink
// =======================================================

void Task1code(void * pvParameters)
{
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {
        wink();
        server.handleClient();
    }
}

// =======================================================
// TASK2 - Serial Command -> ESP-NOW
// =======================================================

void Task2code(void * pvParameters)
{
    Serial.print("Task2 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {
        if (Serial.available() > 0)
        {
            String msg = Serial.readStringUntil('\r');

            // N1
            if (msg.substring(0, 2) == "N1")
            {
                msg.remove(0, 2);
                String data = msg + "\r\n";
                if (peerReady[0])
                    esp_now_send(peerMacs[0],
                                 (uint8_t*)data.c_str(),
                                 data.length());
            }

            // N2
            else if (msg.substring(0, 2) == "N2")
            {
                msg.remove(0, 2);
                String data = msg + "\r\n";
                if (peerReady[1])
                    esp_now_send(peerMacs[1],
                                 (uint8_t*)data.c_str(),
                                 data.length());
            }

            // N3
            else if (msg.substring(0, 2) == "N3")
            {
                msg.remove(0, 2);
                String data = msg + "\r\n";
                if (peerReady[2])
                    esp_now_send(peerMacs[2],
                                 (uint8_t*)data.c_str(),
                                 data.length());
            }

            delay(100);
        }

        delay(10);
    }
}

// =======================================================
// LOOP
// =======================================================

void loop()
{
    delay(100);
}