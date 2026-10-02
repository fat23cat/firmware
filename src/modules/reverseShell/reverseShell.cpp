#if !defined(LITE_VERSION)
#include "core/display.h"
#include <DNSServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include "core/ui/compact.h"

#ifdef UI_COMPACT
static std::vector<String> reverseShellCompactLines;

static void reverseShellCompactLog(const String &message) {
    const int firstY = cui::TOP + FM * LH + 3;
    const int maxRows = (tftHeight - firstY - cui::PAD) / cui::ROW_FP;
    auto wrapped = uiWrap(message, tftWidth - 2 * cui::PAD, FP);
    for (const String &line : wrapped) reverseShellCompactLines.push_back(line);
    while (reverseShellCompactLines.size() > maxRows) reverseShellCompactLines.erase(reverseShellCompactLines.begin());
    tft.fillRect(cui::PAD, firstY, tftWidth - 2 * cui::PAD, tftHeight - firstY - cui::PAD, bruceConfig.bgColor);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.setTextSize(FP);
    for (size_t i = 0; i < reverseShellCompactLines.size(); ++i)
        uiDrawText(reverseShellCompactLines[i], cui::PAD, firstY + i * cui::ROW_FP, TL_DATUM);
}
#endif

void ReverseShell() {
    AsyncWebServer webServer(80);
    AsyncWebSocket ws("/ws");
    DNSServer dnsServer;
    IPAddress apGateway(192, 168, 4, 1);
    WiFiServer tcpServer(23);
    WiFiClient tcpClient;
    bool shellConnected = false;
    bool wsConnected = false;

    // ── WebSocket Event Handler ────────────────────────────────
    auto onWsEvent = [&](AsyncWebSocket *server, AsyncWebSocketClient *client,
                         AwsEventType type, void *arg, uint8_t *data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                wsConnected = true;
                client->text("Connected to BruceShell!\r\n");
                break;

            case WS_EVT_DISCONNECT:
                wsConnected = false;
                break;

            case WS_EVT_DATA:
                if (shellConnected && tcpClient) {
                    String cmd = String((char*)data);
                    cmd.trim();
                    if (cmd.length() > 0) {
                        tcpClient.println(cmd);

                        // Read output and send back via WebSocket
                        String output = "";
                        unsigned long timeout = millis() + 3000;
                        while (millis() < timeout) {
                            if (tcpClient.available()) {
                                output += tcpClient.readString();
                            }
                            if (output.endsWith("\n> ") || output.endsWith("\n$ ") || output.endsWith("\n# ")) {
                                break;
                            }
                            delay(10);
                        }
                        if (output.length() > 0) {
                            client->text(output);
                        } else {
                            client->text("[Command executed, no output]\r\n");
                        }
                    }
                } else {
                    client->text("Error: No shell connected.\r\n");
                }
                break;

            case WS_EVT_ERROR:
                // Handle error silently
                break;
        }
    };

    // ── Setup ──────────────────────────────────────────────────
#ifdef UI_COMPACT
    if (uiCompact()) reverseShellCompactLines.clear();
#endif
    tft.fillScreen(bruceConfig.bgColor);
#ifdef UI_COMPACT
    if (uiCompact()) drawMainBorder(false);
#endif
    tft.setTextSize(FM);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString("Reverse Shell", tftWidth / 2, UIC(10, cui::TOP), 1);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setCursor(UIC(15, cui::PAD), UIC(33, cui::TOP + FM * LH + 3));
#ifdef UI_COMPACT
    if (uiCompact()) reverseShellCompactLog("Developed by Fourier & Ninja-jr");
    else
#endif
    tft.println("Developed by Fourier & Ninja-jr");
#ifdef UI_COMPACT
    if (uiCompact()) reverseShellCompactLog("Starting reverse shell server...");
    else
#endif
    tft.println("Starting reverse shell server...");

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAPConfig(apGateway, apGateway, IPAddress(255, 255, 255, 0))) {
#ifdef UI_COMPACT
        if (uiCompact()) reverseShellCompactLog("Failed to configure AP");
        else
#endif
        tft.println("Failed to configure AP");
        return;
    }

    // ── AP Password: bruce ─────────────────────────────────────
    if (!WiFi.softAP("BruceShell", "bruce")) {
#ifdef UI_COMPACT
        if (uiCompact()) reverseShellCompactLog("Failed to start AP");
        else
#endif
        tft.println("Failed to start AP");
        return;
    }
#ifdef UI_COMPACT

    if (uiCompact()) reverseShellCompactLog("Wi-Fi AP Started: BruceShell (pass: bruce)");

    else
#endif

    tft.println("Wi-Fi AP Started: BruceShell (pass: bruce)");
#ifdef UI_COMPACT
    if (uiCompact()) reverseShellCompactLog("IP: " + apGateway.toString());
    else
#endif
    tft.println("IP: " + apGateway.toString());

    tcpServer.begin();
#ifdef UI_COMPACT
    if (uiCompact()) reverseShellCompactLog("TCP server started on port 23.");
    else
#endif
    tft.println("TCP server started on port 23.");

    // ── Web Interface ──────────────────────────────────────────
    ws.onEvent(onWsEvent);
    webServer.addHandler(&ws);

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = R"rawliteral(
            <!DOCTYPE html>
            <html>
            <head>
                <title>BruceShell</title>
                <style>
                    body { background: #0a0a1a; color: #00ff41; font-family: 'Courier New', monospace; margin: 0; padding: 20px; }
                    .container { max-width: 800px; margin: auto; }
                    .status { display: inline-block; width: 12px; height: 12px; border-radius: 50%; }
                    .online { background: #00ff41; }
                    .offline { background: #ff0040; }
                    input { width: 70%; padding: 10px; background: #16213e; color: #00ff41; border: 1px solid #00ff41; }
                    button { padding: 10px 20px; background: #16213e; color: #00ff41; border: 1px solid #00ff41; cursor: pointer; }
                    button:hover { background: #1a2a4e; }
                    #output { background: #0a0a1a; padding: 10px; min-height: 300px; max-height: 400px; white-space: pre-wrap; border: 1px solid #00ff41; margin-top: 10px; overflow-y: auto; }
                    .footer { margin-top: 20px; font-size: 12px; color: #666; }
                </style>
            </head>
            <body>
                <div class="container">
                    <h1>🔴 BruceShell <span id="statusDot" class="status offline"></span> <span id="statusText">Offline</span></h1>
                    <p>IP: 192.168.4.1 | Port: 23</p>
                    <div style="display: flex; gap: 10px; flex-wrap: wrap;">
                        <input type="text" id="cmd" placeholder="Enter command..." onkeyup="if(event.keyCode==13) sendCommand();" autofocus>
                        <button onclick="sendCommand();">Execute</button>
                        <button onclick="clearOutput();">Clear</button>
                    </div>
                    <div id="output">~ BruceShell\n~ Connected: Waiting for shell...</div>
                    <div class="footer">Connect via WebSocket ws://192.168.4.1/ws</div>
                </div>
                <script>
                    var ws = new WebSocket('ws://192.168.4.1/ws');
                    ws.onopen = function() {
                        document.getElementById('statusDot').className = 'status online';
                        document.getElementById('statusText').innerText = 'Online';
                    };
                    ws.onclose = function() {
                        document.getElementById('statusDot').className = 'status offline';
                        document.getElementById('statusText').innerText = 'Offline';
                    };
                    ws.onmessage = function(e) {
                        document.getElementById('output').innerText += e.data;
                        document.getElementById('output').scrollTop = document.getElementById('output').scrollHeight;
                    };
                    function sendCommand() {
                        var cmd = document.getElementById('cmd').value;
                        if (cmd) {
                            ws.send(cmd + '\n');
                            document.getElementById('cmd').value = '';
                            document.getElementById('cmd').focus();
                        }
                    }
                    function clearOutput() {
                        document.getElementById('output').innerText = '';
                    }
                </script>
            </body>
            </html>
        )rawliteral";
        request->send(200, "text/html", html);
    });

    webServer.begin();
#ifdef UI_COMPACT
    if (uiCompact()) reverseShellCompactLog("Web server started on port 80!");
    else
#endif
    tft.println("Web server started on port 80!");
#ifdef UI_COMPACT
    if (uiCompact()) reverseShellCompactLog("WebSocket server started on /ws");
    else
#endif
    tft.println("WebSocket server started on /ws");

    dnsServer.start(53, "*", apGateway);

    // ── Main Loop ──────────────────────────────────────────────
    while (true) {
        dnsServer.processNextRequest();
        ws.cleanupClients();

        if (!shellConnected) {
            tcpClient = tcpServer.accept();
            if (tcpClient) {
#ifdef UI_COMPACT
                if (uiCompact()) reverseShellCompactLog("Client connected.");
                else
#endif
                tft.println("Client connected.");
                tcpClient.println("~Welcome to BruceShell.");
                tcpClient.println("~Developed by Fourier & Ninja-jr");
                tcpClient.println("~Type 'help' for available commands");
                shellConnected = true;
            }
        }

        if (shellConnected && !tcpClient.connected()) {
#ifdef UI_COMPACT
            if (uiCompact()) reverseShellCompactLog("Client disconnected.");
            else
#endif
            tft.println("Client disconnected.");
            shellConnected = false;
            tcpClient.stop();
        }

        if (check(EscPress)) {
#ifdef UI_COMPACT
            if (uiCompact()) reverseShellCompactLog("Exiting reverse shell server...");
            else
#endif
            tft.println("Exiting reverse shell server...");
            tcpServer.stop();
            ws.closeAll();
            webServer.end();
            dnsServer.stop();
            break;
        }
        delay(10);
    }
}
#endif
