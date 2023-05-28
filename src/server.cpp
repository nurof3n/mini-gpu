// ====================================================== //
// ===================== Web Server ===================== //
// ====================================================== //

#include <Arduino.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>

#include "server.h"
#include "filesystem.h"
#include "webpage.h"

// WiFi and WebServer
const char *ssid     = "nurof3n";
const char *password = "nuamchef";
WebServer   server(80);

void handleListDir()
{
    String dirname = "/";
    if (server.hasArg("dir")) {
        dirname = server.arg("dir");
    }

    if (dirname != "/" && dirname.endsWith("/")) {
        dirname.remove(dirname.length() - 1);
    }

    Serial.printf("Listing directory: %s\n", dirname.c_str());

    auto listing = listDir(dirname.c_str());
    if (listing.empty()) {
        server.send(404, "text/json", "{\"message\":\"Directory not found\"}");
        return;
    }

    String output = "[";
    for (const auto &file : listing) {
        if (output != "[") {
            output += ',';
        }
        output += "{\"type\":\"";
        output += (file.isDir) ? "dir" : "file";
        output += "\",\"name\":\"";
        output += file.name;
        output += "\",\"size\":";
        output += sizeInKbytes(file.size);
        output += "}";
    }

    output += "]";
    server.send(200, "text/json", output);
}

void handleCreateDir()
{
    if (!server.hasArg("dir")) {
        server.send(404, "text/json",
                "{\"message\":\"Directory not fully specified\"}");
        return;
    }

    String dirname = server.arg("dir");
    Serial.printf("Creating directory: %s\n", dirname.c_str());

    if (!createDir(dirname.c_str())) {
        server.send(
                500, "text/json", "{\"message\":\"Directory not created\"}");
        return;
    }

    server.send(200, "text/json", "{\"message\":\"Directory created\"}");
}

void handleRename()
{
    if (!server.hasArg("old") || !server.hasArg("new")) {
        server.send(404, "text/json",
                "{\"message\":\"Names not fully specified\"}");
        return;
    }

    String oldName = server.arg("old");
    String newName = server.arg("new");
    Serial.printf(
            "Renaming file: %s to %s\n", oldName.c_str(), newName.c_str());

    if (!renameFile(oldName.c_str(), newName.c_str())) {
        server.send(404, "text/json", "{\"message\":\"File not found\"}");
        return;
    }

    server.send(200, "text/json", "{\"message\":\"File renamed\"}");
}

void handleStorageDetails()
{
    String output = "{";
    output += "\"total\":";
    output += sizeInKbytes(SD.totalBytes());
    output += ",\"used\":";
    output += sizeInKbytes(SD.usedBytes());
    output += "}";
    server.send(200, "text/json", output);
}

void handleUpload()
{
    if (!server.hasArg("dir")) {
        server.send(404, "text/json",
                "{\"message\":\"Directory not fully specified\"}");
        return;
    }

    HTTPUpload &upload   = server.upload();
    String      filepath = server.arg("dir") + upload.filename;

    if (upload.status == UPLOAD_FILE_START) {
        SD.remove(filepath);
        Serial.printf("Upload started: %s\n", filepath.c_str());
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!writeFile(
                    filepath.c_str(), upload.buf, upload.currentSize, true)) {
            Serial.printf("Upload failed: %s\n", upload.filename.c_str());
            server.send(500, "text/json", "{\"message\":\"Upload failed\"}");
        }
        else {
            Serial.printf(
                    "Upload: %s (%u)\n", filepath.c_str(), upload.totalSize);
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.printf("Upload was aborted: %s\n", filepath.c_str());
        server.send(500, "text/json", "{\"message\":\"Upload aborted\"}");
    }
    else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("Upload ended: %s (%d bytes)\n", filepath.c_str(),
                upload.totalSize);
        server.send(200, "text/json", "{\"message\":\"Upload successful\"}");
    }
}

void handleDownload()
{
    if (!server.hasArg("file")) {
        server.send(404, "text/json", "{\"message\":\"No file specified\"}");
        return;
    }

    String filename = server.arg("file");
    Serial.printf("Download: %s\n", filename.c_str());

    File file = SD.open(filename.c_str());
    if (!file) {
        server.send(404, "text/json", "{\"message\":\"File not found\"}");
        return;
    }

    if (file.isDirectory()) {
        server.send(403, "text/json",
                "{\"message\":\"Cannot download a directory\"}");
    }
    else if (server.streamFile(file, "application/octet-stream")
             != file.size()) {
        Serial.println("Sent less data than expected!");
        server.send(500, "text/json", "{\"message\":\"Download incomplete\"}");
    }
    else {
        Serial.println("File sent successfully");
        server.send(200, "text/json", "{\"message\":\"Download successful\"}");
    }

    file.close();
}

void handleDelete()
{
    if (!server.hasArg("file")) {
        server.send(404, "text/json", "{\"message\":\"No file specified\"}");
        return;
    }

    String filename = server.arg("file");
    Serial.printf("Delete: %s\n", filename.c_str());

    if (SD.exists(filename.c_str())) {
        if (deleteItem(filename.c_str())) {
            server.send(200, "text/json", "{\"message\":\"File deleted\"}");
        }
        else {
            server.send(500, "text/plain", "{\"message\":\"Delete failed\"}");
        }
    }
    else {
        server.send(404, "text/plain", "{\"message\":\"File not found\"}");
    }
}

void handleGetMonitorDetails()
{
    server.send(200, "text/plain", "OK");
}

void serverTask(void *args)
{
    // ─── Connect To Wifi Network ─────────────────────────────────────────

    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.print("\nConnected to WiFi. IP address: ");
    Serial.println(WiFi.localIP());

    // ─── Start Server ────────────────────────────────────────────────────

    server.on("/", HTTP_GET, []() { server.send(200, "text/html", webpage); });
    server.on("/listDir", HTTP_GET, handleListDir);
    server.on("/createDir", HTTP_GET, handleCreateDir);
    server.on("/rename", HTTP_GET, handleRename);
    server.on("/storage", HTTP_GET, handleStorageDetails);
    server.on("/monitor", HTTP_GET, handleGetMonitorDetails);
    server.on("/update", HTTP_POST, handleUpload);
    server.on("/download", HTTP_GET, handleDownload);
    server.on("/delete", HTTP_GET, handleDelete);
    server.onNotFound([]() {
        server.send(404, "text/json", "{\"message\":\"Not found\"}");
    });
    server.begin();

    // ─── Server Loop ─────────────────────────────────────────────────────

    for (;;) {
        server.handleClient();
        vTaskDelay(1);  // avoid watchdog reset
    }
}