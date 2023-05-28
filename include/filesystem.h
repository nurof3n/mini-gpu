// ====================================================== //
// ========= API for file system on microSD card ======== //
// ====================================================== //

#include <Arduino.h>
#include <SD.h>
#include <vector>

#define SD_CS   15
#define SD_MOSI 5
#define SD_MISO 4
#define SD_SCK  47

struct DirListingItem
{
    String name;
    bool   isDir;
    size_t size;
};

size_t                      sizeInKbytes(size_t bytes);
size_t                      sizeInMbytes(size_t bytes);
std::vector<DirListingItem> listDir(const char *dirname);
bool                        createDir(const char *path);
bool                        removeDir(const char *path);
uint8_t                    *readFile(const char *path);
bool writeFile(const char *path, const uint8_t *bytes, size_t size,
        bool append = false);
bool renameFile(const char *path1, const char *path2);
bool deleteFile(const char *path);
bool deleteItem(const char *path);
void testFileIO(const char *path);
void testSD();
void setupSD();