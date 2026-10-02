/**
 * SOCKS4 / SOCKS4a proxy **server** for Bruce (ESP32 as proxy).
 *
 * Protocol: https://www.openssh.com/txt/socks4.protocol
 *
 * Asio: espressif/asio is archived; Asio lives in esp-protocols (upstream Asio).
 * Official socks4 example is a **client** (connect to proxy). We implement the server.
 * See socks4_proxy.h for links.
 */
#ifndef LITE_VERSION
#include "modules/wifi/socks4_proxy.h"
#include "core/display.h"
#include "core/wifi/wifi_common.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

#include "core/ui/compact.h"

static const uint8_t SOCKS4_VERSION = 4;
static const uint8_t SOCKS4_CMD_CONNECT = 1;
static const uint8_t SOCKS4_REP_GRANTED = 90;
static const uint8_t SOCKS4_REP_REJECTED = 91;
static const size_t RELAY_BUF_SIZE = 512;
static const unsigned long RELAY_POLL_MS = 10;

static void relayLoop(WiFiClient &client, WiFiClient &target) {
    uint8_t buf[RELAY_BUF_SIZE];
    while (client.connected() && target.connected()) {
        if (check(EscPress)) break;
        int n = 0;
        if (client.available()) {
            n = client.read(buf, sizeof(buf));
            if (n > 0) target.write(buf, (size_t)n);
        }
        if (target.available()) {
            n = target.read(buf, sizeof(buf));
            if (n > 0) client.write(buf, (size_t)n);
        }
        if (n <= 0) delay(RELAY_POLL_MS);
    }
}

static bool readSocks4Request(
    WiFiClient &client, uint8_t &cd, uint16_t &dstPort, uint8_t dstIp[4], char *hostname, size_t hostnameSize
) {
    if (hostname && hostnameSize) hostname[0] = '\0';
    uint8_t vn;
    if (client.read(&vn, 1) != 1 || vn != SOCKS4_VERSION) return false;
    if (client.read(&cd, 1) != 1) return false;
    uint8_t portBuf[2];
    if (client.read(portBuf, 2) != 2) return false;
    dstPort = (uint16_t)portBuf[0] << 8 | portBuf[1];
    if (client.read(dstIp, 4) != 4) return false;
    // USERID: read until null
    int c;
    while ((c = client.read()) != -1 && c != 0) {}
    if (c != 0) return false;
    // SOCKS4a: if DSTIP is 0.0.0.x with x != 0, read hostname
    if (hostname && hostnameSize && dstIp[0] == 0 && dstIp[1] == 0 && dstIp[2] == 0 && dstIp[3] != 0) {
        size_t i = 0;
        while (i < hostnameSize - 1) {
            c = client.read();
            if (c == -1) return false;
            if (c == 0) break;
            hostname[i++] = (char)c;
        }
        hostname[i] = '\0';
    }
    return true;
}

static void sendSocks4Reply(WiFiClient &client, uint8_t cd, uint16_t dstPort, const uint8_t *dstIp) {
    uint8_t reply[8] = {
        0, cd, (uint8_t)(dstPort >> 8), (uint8_t)(dstPort & 0xff), dstIp[0], dstIp[1], dstIp[2], dstIp[3]
    };
    client.write(reply, 8);
}

void socks4Proxy(uint16_t port) {
    if (!wifiConnected) {
        wifiConnectMenu();
        if (!wifiConnected) {
            displayError("Connect to WiFi first", true);
            return;
        }
    }

    WiFiServer server(port);
    server.begin();

    drawMainBorderWithTitle("SOCKS4 PROXY");
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setCursor(UIC(10, cui::PAD), UIC(BORDER_PAD_Y + FM * LH, cui::TOP + FM * LH + 2));
    tft.println("Port: " + String(port));
    tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
    tft.println(UIC("IP: " + WiFi.localIP().toString(), uiTruncate("IP: " + WiFi.localIP().toString(), tftWidth - 2 * cui::PAD, FP)));
    tft.println();
    tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
    tft.println("Waiting for clients...");
    tft.println("");
    Serial.println("[SOCKS4] Listening on " + WiFi.localIP().toString() + ":" + String(port));

    char hostname[256];
    uint8_t dstIp[4];
    uint8_t cd;
    uint16_t dstPort;
    int connCount = 0;

    for (;;) {
        WiFiClient client = server.accept();
        if (check(EscPress)) {
            displayInfo("SOCKS4 proxy stopped", true);
            server.stop();
            return;
        }
        if (!client) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

#ifdef UI_COMPACT
        if (uiCompact() && tft.getCursorY() > tftHeight - 5 * cui::ROW_FP) {
            drawMainBorderWithTitle("SOCKS4 PROXY");
            tft.setTextSize(FP);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.setCursor(cui::PAD, cui::TOP + FM * LH + 2);
        }
#endif
        connCount++;
        client.setNoDelay(true);
        String clientIp = client.remoteIP().toString();
        tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
        tft.println("---");
        tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
        tft.println(UIC("CLIENT #" + String(connCount) + " " + clientIp, uiTruncate("CLIENT #" + String(connCount) + " " + clientIp, tftWidth - 2 * cui::PAD, FP)));
        Serial.println("[SOCKS4] Client #" + String(connCount) + " connected from " + clientIp);

        if (!readSocks4Request(client, cd, dstPort, dstIp, hostname, sizeof(hostname))) {
            sendSocks4Reply(client, SOCKS4_REP_REJECTED, 0, dstIp);
            client.stop();
            tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
            tft.println("  Bad request");
            Serial.println("[SOCKS4] Bad request from " + clientIp);
            continue;
        }
        if (cd != SOCKS4_CMD_CONNECT) {
            sendSocks4Reply(client, SOCKS4_REP_REJECTED, 0, dstIp);
            client.stop();
            tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
            tft.println("  Unsupported cmd");
            continue;
        }

        String destStr =
            hostname[0] != '\0'
                ? (String(hostname) + ":" + String(dstPort))
                : (IPAddress(dstIp[0], dstIp[1], dstIp[2], dstIp[3]).toString() + ":" + String(dstPort));
        tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
        tft.println(UIC("  -> " + destStr, uiTruncate("  -> " + destStr, tftWidth - 2 * cui::PAD, FP)));
        Serial.println("[SOCKS4] Connecting to " + destStr);

        WiFiClient target;
        bool ok = false;
        if (hostname[0] != '\0') {
            ok = target.connect(hostname, dstPort, 15000);
        } else {
            IPAddress ip(dstIp[0], dstIp[1], dstIp[2], dstIp[3]);
            ok = target.connect(ip, dstPort, 15000);
        }

        if (!ok) {
            sendSocks4Reply(client, SOCKS4_REP_REJECTED, 0, dstIp);
            client.stop();
            tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
            tft.println("  FAILED");
            Serial.println("[SOCKS4] Connect failed: " + destStr);
            continue;
        }

        tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
        tft.println("  OK - relaying");
        Serial.println("[SOCKS4] Tunnel up: " + clientIp + " <-> " + destStr);

        target.setNoDelay(true);
        sendSocks4Reply(client, SOCKS4_REP_GRANTED, dstPort, dstIp);
        relayLoop(client, target);

        target.stop();
        client.stop();
        tft.setCursor(UIC(10, cui::PAD), tft.getCursorY());
        tft.println("  Closed");
        Serial.println("[SOCKS4] Tunnel closed: " + clientIp + " -> " + destStr);
    }
}
#endif
