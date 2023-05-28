// ====================================================== //
// ========= API for file system on microSD card ======== //
// ====================================================== //

#include "filesystem.h"

SPIClass sdSPI;

size_t sizeInKbytes(size_t bytes)
{
    return (bytes + 1023) / 1024;
}

size_t sizeInMbytes(size_t bytes)
{
    return (bytes + 1048575) / 1048576;
}

std::vector<DirListingItem> listDir(const char *dirname)
{
    File root = SD.open(dirname);
    if (!root) {
        Serial.printf("Failed to open directory %s\n", dirname);
        return std::vector<DirListingItem>();
    }
    if (!root.isDirectory()) {
        Serial.printf("%s is not a directory\n", dirname);
        return std::vector<DirListingItem>();
    }

    std::vector<DirListingItem> listing;

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            listing.push_back({ file.name(), true, 0 });
        }
        else {
            listing.push_back({ file.name(), false, file.size() });
        }

        file = root.openNextFile();
    }

    Serial.printf("Found %u items in directory %s\n", listing.size(), dirname);
    return listing;
}

bool createDir(const char *path)
{
    if (SD.mkdir(path)) {
        Serial.printf("Dir %s created\n", path);
        return true;
    }
    Serial.printf("mkdir failed for %s\n", path);
    return false;
}

bool removeDir(const char *path)
{
    if (SD.rmdir(path)) {
        Serial.printf("Dir %s removed\n", path);
        return true;
    }
    Serial.printf("rmdir failed for %s\n", path);
    return false;
}

uint8_t *readFile(const char *path)
{
    File file = SD.open(path);
    if (!file) {
        Serial.printf("Failed to open file %s for reading\n", path);
        return nullptr;
    }

    if (file.isDirectory()) {
        Serial.printf("File %s is a directory\n", path);
        return nullptr;
    }

    uint8_t *buf = new uint8_t[file.size()];
    size_t   len = 0;
    while (file.available()) {
        len += file.read(buf + len, file.size());
    }
    file.close();

    Serial.printf("Read %u bytes from file %s\n", sizeInKbytes(len), path);
    return buf;
}

bool writeFile(const char *path, const uint8_t *bytes, size_t size, bool append)
{
    File file = SD.open(path, append ? FILE_APPEND : FILE_WRITE);
    if (!file) {
        Serial.printf("Failed to open file %s for writing\n", path);
        return false;
    }

    bool success = file.write(bytes, size) == size;
    file.close();
    if (success) {
        Serial.printf("Wrote %u bytes to file %s\n", sizeInKbytes(size), path);
        return true;
    }
    Serial.printf("Failed to write to file %s\n", path);
    return false;
}

bool renameFile(const char *path1, const char *path2)
{
    if (SD.rename(path1, path2)) {
        Serial.printf("File renamed to %s\n", path2);
        return true;
    }
    Serial.printf("Rename failed for %s\n", path1);
    return false;
}

bool deleteFile(const char *path)
{
    if (SD.remove(path)) {
        Serial.printf("File %s deleted\n", path);
        return true;
    }
    Serial.printf("Delete failed for %s\n", path);
    return false;
}

bool deleteItem(const char *path)
{
    if (SD.remove(path)) {
        Serial.printf("File %s deleted\n", path);
        return true;
    }
    if (SD.rmdir(path)) {
        Serial.printf("Directory %s deleted\n", path);
        return true;
    }

    Serial.printf("Delete failed for %s\n", path);
    return false;
}

void testFileIO(const char *path)
{
    File           file = SD.open(path);
    static uint8_t buf[512];
    size_t         len   = 0;
    uint32_t       start = millis();
    uint32_t       end   = start;
    if (file) {
        len         = file.size();
        size_t flen = len;
        start       = millis();
        while (len) {
            size_t toRead = len;
            if (toRead > 512) {
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    }
    else {
        Serial.println("Failed to open file for reading");
    }


    file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for (i = 0; i < 2048; i++) {
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

void testSD()
{
    listDir("/");
    createDir("/mydir");
    listDir("/");
    removeDir("/mydir");
    listDir("/");
    writeFile("/hello.txt", (const uint8_t *) "Hello ", 6);
    writeFile("/hello.txt", (const uint8_t *) "World!\n", 7, true);
    readFile("/hello.txt");
    deleteFile("/foo.txt");
    renameFile("/hello.txt", "/foo.txt");
    readFile("/foo.txt");
    testFileIO("/test.txt");
    Serial.printf("Total space: %uMB\n", sizeInMbytes(SD.totalBytes()));
    Serial.printf("Used space: %uMB\n", sizeInMbytes(SD.usedBytes()));
}

void setupSD()
{
    Serial.println("Initializing SD card...");

    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(1000);
    if (!SD.begin(SD_CS, sdSPI)) {
        Serial.println("Card Mount Failed");
        return;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    }
    else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    }
    else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    }
    else {
        Serial.println("UNKNOWN");
    }

    Serial.printf("SD Card Size: %u KB\n", sizeInKbytes(SD.cardSize()));
    Serial.printf(
            "SD Card Total Space: %u KB\n", sizeInKbytes(SD.totalBytes()));
    Serial.printf("SD Card Used Space: %u KB\n", sizeInKbytes(SD.usedBytes()));
}