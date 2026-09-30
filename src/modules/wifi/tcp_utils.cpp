// TODO: Be able to read bytes from server in background/task
//       so there is no loss of data when inputing
#ifndef LITE_VERSION
#include "modules/wifi/tcp_utils.h"
#include "core/display.h"
#include "core/wifi/wifi_common.h"

#include "core/ui/compact.h"

#ifdef UI_COMPACT
static void tcpCompactLog(const String &message, const char *title) {
    const int left = cui::PAD;
    const int right = tftWidth - cui::PAD;
    const int bottom = tftHeight - LH - cui::PAD;
    tft.setTextSize(FP);
    if (tft.getCursorX() < left) tft.setCursor(left, tft.getCursorY());

    for (size_t i = 0; i < message.length(); ++i) {
        const char ch = message[i];
        if (ch == '\r') {
            tft.setCursor(left, tft.getCursorY());
            continue;
        }
        if (ch == '\n' || tft.getCursorX() + FP * LW > right) {
            tft.println();
            tft.setCursor(left, tft.getCursorY());
        }
        if (tft.getCursorY() > bottom) {
            drawMainBorderWithTitle(title);
            tft.setTextSize(FP);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.setCursor(left, cui::TOP + FM * LH + 2);
        }
        if (ch != '\n') tft.write((uint8_t)ch);
    }
}
#endif

bool inputMode;

void listenTcpPort() {
    if (!wifiConnected) wifiConnectMenu();

    String portNumber = num_keyboard("", 5, "TCP port to listen");
    if (portNumber.length() == 0 || portNumber == "\x1B") {
        displayError("No port number given, exiting");
        return;
    }
    int portNumberInt = atoi(portNumber.c_str());
    if (portNumberInt == 0) {
        displayError("Invalid port number, exiting");
        return;
    }

    WiFiClient tcpClient;
    drawMainBorderWithTitle("LISTEN TCP");
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);

    WiFiServer server(portNumberInt);
    server.begin();

    padprintln("");
    padprint("Listening on:");
    tft.print(WiFi.localIP().toString().c_str());
    tft.println(":" + portNumber);

    for (;;) {
        WiFiClient client = server.accept();

        if (client) {
            Serial.println("Client connected");
            tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
            tft.println("Client connected");

            while (client.connected()) {
                if (inputMode) {
                    String keyString = keyboard("", 16, "send input data, q=quit");
                    if (keyString == "q" || keyString == "\x1B") {
                        displayError("Exiting Listener");
                        client.stop();
                        server.stop();
                        return;
                    }
                    inputMode = false;
                    drawMainBorderWithTitle("LISTEN TCP");
                    tft.setTextSize(FP);
                    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                    tft.setCursor(UIC(10, cui::PAD), UIC(BORDER_PAD_Y + FM * LH, cui::TOP + FM * LH + 2));
                    if (keyString.length() > 0 && keyString != "\x1B") {
                        if (tft.getCursorY() > tftHeight - 3 * LH * FP) {
                            drawMainBorderWithTitle("LISTEN TCP");
                            padprint(keyString);
                        }
                        client.print(keyString);
                        Serial.print(keyString);
                    }
                } else {
                    if (client.available()) {
                        String incomingData = client.readString();
                        if (tft.getCursorY() > tftHeight - 3 * LH * FP) drawMainBorderWithTitle("LISTEN TCP");
#ifdef UI_COMPACT
                        if (uiCompact()) tcpCompactLog(incomingData, "LISTEN TCP");
                        else
#endif
                            padprint(incomingData);
                        Serial.print(incomingData);
                    }
                    if (check(SelPress)) { inputMode = true; }
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            client.stop();
            Serial.println("Client disconnected");
            displayError("Client disconnected");
        }
        if (check(EscPress)) {
            displayError("Exiting Listener");
            server.stop();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void clientTCP() {
    if (!wifiConnected) wifiConnectMenu();

    String _ip = WiFi.localIP().toString();
    _ip = _ip.substring(0, _ip.lastIndexOf('.')) + ".";

    String serverIP = num_keyboard(_ip, 15, "Enter server IP");
    if (serverIP == "\x1B") return;
    String portString = num_keyboard("", 5, "Enter server Port");
    if (portString == "\x1B") return;
    int portNumber = atoi(portString.c_str());

    if (serverIP.length() == 0 || portNumber == 0) {
        displayError("Invalid IP or Port");
        return;
    }

    WiFiClient client;
    drawMainBorderWithTitle("TCP CLIENT");
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);

    padprintln("Connecting to:");
    tft.println(UIC(serverIP + ":" + portString, uiTruncate(serverIP + ":" + portString, tftWidth - 2 * cui::PAD, FP)));

    if (!client.connect(serverIP.c_str(), portNumber)) {
        displayError("Connection failed");
        return;
    }

    padprintln("Connected!");
    Serial.println("Connected to server");

    while (client.connected()) {
        if (inputMode) {
            String keyString = keyboard("", 16, "send input data");
            inputMode = false;
            drawMainBorderWithTitle("TCP CLIENT");
            tft.setTextSize(FP);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.setCursor(UIC(10, cui::PAD), UIC(BORDER_PAD_Y + FM * LH, cui::TOP + FM * LH + 2));
            if (keyString.length() > 0 && keyString != "\x1B") {
                if (tft.getCursorY() > tftHeight - 3 * LH * FP) drawMainBorderWithTitle("LISTEN TCP");
                padprint(keyString);
                client.print(keyString);
                Serial.print(keyString);
            }
        } else {
            if (client.available()) {
                String incomingData = client.readString();
                if (tft.getCursorY() > tftHeight - 3 * LH * FP) drawMainBorderWithTitle("LISTEN TCP");
#ifdef UI_COMPACT
                if (uiCompact()) tcpCompactLog(incomingData, "TCP CLIENT");
                else
#endif
                    padprint(incomingData);
                Serial.print(incomingData);
            }
            if (check(SelPress)) { inputMode = true; }
        }
        if (check(EscPress)) {
            displayError("Exiting Client");
            client.stop();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    displayError("Connection closed.");
    Serial.println("Connection closed.");
    client.stop();
}
#endif
