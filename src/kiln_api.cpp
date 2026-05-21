#include "kiln_api.h"
#include "kiln_commands.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

static WebServer s_server(80);

static void sendJson(int code, const String& body) {
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    s_server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    s_server.send(code, "application/json", body);
}

static void handleOptions() {
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    s_server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    s_server.send(204);
}

static void handleNotFound() {
    StaticJsonDocument<128> doc;
    doc["error"] = "not_found";
    doc["path"] = s_server.uri();
    String body;
    serializeJson(doc, body);
    sendJson(404, body);
}

static void handleStatus() {
    sendJson(200, kiln_buildStatusJson());
}

static void handleInfo() {
    sendJson(200, kiln_buildInfoJson());
}

static void handleApInfo() {
    sendJson(200, kiln_buildApInfoJson());
}

static void handleCommand() {
    if (s_server.method() != HTTP_POST) {
        sendJson(405, "{\"error\":\"method_not_allowed\"}");
        return;
    }
    String body = s_server.arg("plain");
    if (body.length() == 0) {
        sendJson(400, "{\"error\":\"empty_body\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body)) {
        sendJson(400, "{\"error\":\"invalid_json\"}");
        return;
    }

    const char* cmd = doc["cmd"] | "";
    if (strlen(cmd) == 0) {
        sendJson(400, "{\"error\":\"missing_cmd\"}");
        return;
    }

    sendJson(200, kiln_processCommand(String(cmd)));
}

static void handleProfile() {
    if (s_server.method() != HTTP_POST) {
        sendJson(405, "{\"error\":\"method_not_allowed\"}");
        return;
    }
    String body = s_server.arg("plain");
    String err;
    if (kiln_loadProfileJson(body, err)) {
        StaticJsonDocument<128> doc;
        doc["ok"] = true;
        doc["message"] = "PROFILE_OK";
        String out;
        serializeJson(doc, out);
        sendJson(200, out);
    } else {
        StaticJsonDocument<128> doc;
        doc["ok"] = false;
        doc["error"] = err;
        String out;
        serializeJson(doc, out);
        sendJson(400, out);
    }
}

static void handleKilnInfo() {
    if (s_server.method() != HTTP_POST) {
        sendJson(405, "{\"error\":\"method_not_allowed\"}");
        return;
    }
    String body = s_server.arg("plain");
    String err;
    if (kiln_applyKilnInfoJson(body, err)) {
        sendJson(200, "{\"ok\":true,\"message\":\"KILN_INFO_OK\"}");
    } else {
        StaticJsonDocument<128> doc;
        doc["ok"] = false;
        doc["error"] = err;
        String out;
        serializeJson(doc, out);
        sendJson(400, out);
    }
}

static void handleRoot() {
    s_server.sendHeader("Location", "/api/status");
    s_server.send(302, "text/plain", "");
}

void kiln_api_begin() {
    s_server.on("/", HTTP_GET, handleRoot);
    s_server.on("/api/status", HTTP_GET, handleStatus);
    s_server.on("/api/info", HTTP_GET, handleInfo);
    s_server.on("/api/ap-info", HTTP_GET, handleApInfo);
    s_server.on("/api/command", HTTP_POST, handleCommand);
    s_server.on("/api/command", HTTP_OPTIONS, handleOptions);
    s_server.on("/api/profile", HTTP_POST, handleProfile);
    s_server.on("/api/profile", HTTP_OPTIONS, handleOptions);
    s_server.on("/api/kiln-info", HTTP_POST, handleKilnInfo);
    s_server.on("/api/kiln-info", HTTP_OPTIONS, handleOptions);
    s_server.onNotFound(handleNotFound);
    s_server.begin();
    Serial.println("[API] Servidor HTTP en puerto 80");
}

void kiln_api_loop() {
    s_server.handleClient();
}
