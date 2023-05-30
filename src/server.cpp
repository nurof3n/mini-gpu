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
#include "vga.h"

// WiFi and WebServer
const char *ssid     = "nurof3n";
const char *password = "nuamchef";
WebServer   server(80);
const int   maxUploadIter = 5;  // used to slow down the upload speed to
                                // avoid watchdog reset

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

void handleDraw()
{
    if (!server.hasArg("file")) {
        server.send(
                404, "text/json", "{\"message\":\"File not fully specified\"}");
        return;
    }

    String filename = server.arg("file");
    Serial.printf("Drawing file: %s\n", filename.c_str());

    // Load png
    if (!decodePng(filename.c_str())) {
        Serial.println("Failed to load image");
        server.send(500, "text/json", "{\"message\":\"Failed to load image\"}");
        return;
    }
    else {
        Serial.println("Image loaded");

        // Start VGA emulator
        vga.setup();
    }

    server.send(200, "text/json", "{\"message\":\"Image loaded\"}");
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
    static int  iteration = 0;  // used to slow down for watchdog
    static File uploadFile;
    HTTPUpload &upload = server.upload();

    if (!server.hasArg("dir")) {
        server.send(404, "text/json",
                "{\"message\":\"Directory not fully specified\"}");
        return;
    }
    String filepath = server.arg("dir") + upload.filename;

    if (upload.status == UPLOAD_FILE_START) {
        iteration = 0;

        if (SD.exists(filepath.c_str())) {
            SD.remove(filepath.c_str());
        }

        uploadFile = SD.open(filepath.c_str(), FILE_WRITE);
        if (!uploadFile) {
            server.send(
                    500, "text/json", "{\"message\":\"File creation failed\"}");
            return;
        }

        Serial.println("Upload started for: " + filepath);
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }

        if (++iteration % maxUploadIter == 0) {
            Serial.printf("Uploaded %d Kbytes so far\n",
                    sizeInKbytes(upload.totalSize));
            vTaskDelay(1);
        }

        iteration %= maxUploadIter;
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Serial.println(
                    "Upload finished: " + String(upload.totalSize) + " bytes");
        }
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
        if (deleteItem(filename)) {
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

void handleWipe()
{
    // Wipe everything from the SD card
    Serial.println("Wipe SD card");

    auto listing = listDir("/");
    for (auto &item : listing)
        if (item.name != "System Volume Information") {
            deleteItem("/" + item.name);
        }

    server.send(200, "text/json", "{\"message\":\"SD card wiped\"}");
}

void handleGetMonitorDetails()
{
    int width = 0, height = 0;
    vga.getMonitorResolution(&width, &height);
    server.send(200, "text/json",
            "{\"width\":" + String(width) + ",\"height\":" + String(height)
                    + "}");
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
    server.on("/draw", HTTP_GET, handleDraw);
    server.on("/rename", HTTP_GET, handleRename);
    server.on("/storage", HTTP_GET, handleStorageDetails);
    server.on("/monitor", HTTP_GET, handleGetMonitorDetails);
    server.on(
            "/update", HTTP_POST,
            []() {
                server.send(200, "text/json",
                        "{\"message\":\"Upload successful\"}");
            },
            handleUpload);
    server.on("/download", HTTP_GET, handleDownload);
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/wipe", HTTP_GET, handleWipe);
    server.onNotFound([]() {
        server.send(404, "text/json", "{\"message\":\"Not found\"}");
    });
    server.begin();

    // ─── Server Loop ─────────────────────────────────────────────────────

    for (;;) {
        server.handleClient();
        vTaskDelay(1);
    }
}