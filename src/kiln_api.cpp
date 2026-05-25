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

static void handleCaptive204();

static void handleNotFound() {
    // Respuesta mínima: Android hace muchas peticiones de portal cautivo
    if (s_server.method() == HTTP_GET) {
        const String& uri = s_server.uri();
        if (uri.indexOf("generate") >= 0 || uri.indexOf("connect") >= 0 ||
            uri.indexOf("hotspot") >= 0 || uri.indexOf("redirect") >= 0 ||
            uri == "/favicon.ico") {
            handleCaptive204();
            return;
        }
    }
    s_server.send(404, "text/plain", "not_found");
}

static void handleStatus() {
    Serial.println("[API] GET /api/status");
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

static void handleCaptive204() {
    // Android/iOS comprueban internet; 204 evita que el sistema abandone la red local
    s_server.send(204, "text/plain", "");
}

static void handleRoot() {
    Serial.println("[API] GET /");
    const char* html =
        "<!DOCTYPE html><html><head><meta charset=utf-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>SmartKiln</title></head><body>"
        "<h1>SmartKiln</h1><p>Red local del horno (sin internet).</p>"
        "<ul>"
        "<li><a href=\"/api/status\">Estado (JSON)</a></li>"
        "<li><a href=\"/api/ap-info\">Datos WiFi AP (JSON)</a></li>"
        "</ul></body></html>";
    s_server.send(200, "text/html", html);
}

void kiln_api_begin() {
    s_server.on("/", HTTP_GET, handleRoot);
    s_server.on("/generate_204", HTTP_GET, handleCaptive204);
    s_server.on("/gen_204", HTTP_GET, handleCaptive204);
    s_server.on("/hotspot-detect.html", HTTP_GET, handleCaptive204);
    s_server.on("/connecttest.txt", HTTP_GET, handleCaptive204);
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

static void TaskHttpApi(void* /*pvParameters*/) {
    for (;;) {
        kiln_api_loop();
        vTaskDelay(pdMS_TO_TICKS(10));  // ~100 veces/s
    }
}

void kiln_api_startTask() {
    xTaskCreatePinnedToCore(TaskHttpApi, "HttpApi", 6144, NULL, 5, NULL, 0);
    Serial.println("[API] Tarea HttpApi iniciada (prioridad alta)");
}
