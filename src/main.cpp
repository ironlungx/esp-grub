#include <Arduino.h>
#include <ESP32-targz.h>
#include <LittleFS.h>
#include <U8g2lib.h>
#include <Update.h>
#include <pixelView.h>

#define FORMAT_LITTLEFS_IF_FAILED true
#define TMP_DIR "/tmp"

namespace pins {
constexpr int UP = 4;
constexpr int DOWN = 18;
constexpr int LEFT = 15;
constexpr int RIGHT = 5;
constexpr int SEL = 19;
} // namespace pins

static const unsigned char image_earth_bits[] = {
    0xe0, 0x83, 0x78, 0x0e, 0xe4, 0x1f, 0x86, 0x27, 0xc2, 0x27, 0xe1,
    0x53, 0xf9, 0x6f, 0xfb, 0x41, 0xfb, 0x41, 0xc7, 0x43, 0x86, 0x2f,
    0x0e, 0x2f, 0x8c, 0x1f, 0xd8, 0x0f, 0xe0, 0x03, 0x01, 0x00};

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
PixelView pv(
    &u8g2,
    []() {
      for (;;) {
        if (!digitalRead(pins::UP)) {
          return (ActionType::UP);

          while (!digitalRead(pins::UP))
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        if (!digitalRead(pins::DOWN)) {
          return (ActionType::DOWN);

          while (!digitalRead(pins::DOWN))
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        if (!digitalRead(pins::LEFT)) {
          return (ActionType::LEFT);

          while (!digitalRead(pins::LEFT))
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        if (!digitalRead(pins::RIGHT)) {
          return (ActionType::RIGHT);

          while (!digitalRead(pins::RIGHT))
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }

        if (!digitalRead(pins::SEL)) {
          return (ActionType::SEL);

          while (!digitalRead(pins::SEL))
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }

        return ActionType::NONE;

        vTaskDelay(20 / portTICK_PERIOD_MS);
      }
    },
    delay);

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }
  Serial.println("- read from file:");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

bool endsWith(const char *str, const char *suffix) {
  size_t len = strlen(str);
  size_t suffixLen = strlen(suffix);

  if (len < suffixLen) {
    return false; // string too short to have .tar ending
  }

  // compare end of str with suffix
  return strcmp(str + len - suffixLen, suffix) == 0;
}

uint64_t myTotalBytesFn() { return LittleFS.totalBytes(); }

uint64_t myFreeBytesFn() {
  return LittleFS.totalBytes() - LittleFS.usedBytes();
}

void reboot(fs::FS &filesys) {
  filesys.remove(TMP_DIR);
  ESP.restart();
}

void updateFirmware(const char *file, const char *dest_folder) {
  if (!LittleFS.exists(file)) {
    Serial.println("TAR file not found");
    return;
  }

  // Extract TAR file
  TarUnpacker *unpacker = new TarUnpacker();
  unpacker->haltOnError(true);
  unpacker->setTarVerify(true);

  // Fixed filesystem callbacks
  unpacker->setupFSCallbacks(myTotalBytesFn, myFreeBytesFn);

  // Progress callbacks
  unpacker->setLoggerCallback(BaseUnpacker::targzPrintLoggerCallback);

  unpacker->setTarProgressCallback([](uint8_t progress) {
    pv.progressBar(progress, "Extracting firmware");
  });

  unpacker->setTarVerify(false);

  unpacker->setTarStatusProgressCallback(
      BaseUnpacker::defaultTarStatusProgressCallback);
  unpacker->setTarMessageCallback(BaseUnpacker::targzPrintLoggerCallback);

  if (!unpacker->tarExpander(LittleFS, file, LittleFS, dest_folder)) {
    ESP_LOGE("TARGZ", "tarExpander failed with return code #%d\n",
             unpacker->tarGzGetError());
    delete unpacker;
    return;
  }
  delete unpacker;

  // Get the base filename without path and extension
  const char *filename = strrchr(file, '/');
  if (filename == nullptr) {
    filename = file; // No path separator found, use the whole string
  } else {
    filename++; // Skip the '/' character
  }

  // Create a copy to modify (remove .tar extension)
  char baseName[64];
  strncpy(baseName, filename, sizeof(baseName) - 1);
  baseName[sizeof(baseName) - 1] = '\0';

  // Remove .tar extension
  char *dot = strrchr(baseName, '.');
  if (dot != nullptr && strcmp(dot, ".tar") == 0) {
    *dot = '\0';
  }

  // Build firmware path with the correct .bin filename
  char firmwarePath[256];
  if (strcmp(dest_folder, "/") == 0) {
    snprintf(firmwarePath, sizeof(firmwarePath), "/%s.bin", baseName);
  } else {
    snprintf(firmwarePath, sizeof(firmwarePath), "%s/%s.bin", dest_folder,
             baseName);
  }

  // Check if firmware exists
  if (!LittleFS.exists(firmwarePath)) {
    Serial.printf("Firmware file not found at: %s\n", firmwarePath);
    return;
  }

  // Open firmware file
  File firmware = LittleFS.open(firmwarePath);
  if (!firmware) {
    Serial.println("Failed to open firmware file");
    return;
  }

  Serial.printf("Found firmware file: %s (%zu bytes)\n", firmwarePath,
                firmware.size());
  Serial.println("Starting firmware update...");

  // Set up progress callback
  Update.onProgress([](size_t currSize, size_t totalSize) {
    Serial.printf("Update progress: %zu/%zu bytes (%.1f%%)\n", currSize,
                  totalSize, (currSize * 100.0) / totalSize);

    pv.progressBar((currSize * 100.0) / totalSize, "Flashing");
  });

  // Begin update
  if (!Update.begin(firmware.size(), U_FLASH)) {
    Serial.printf("Update.begin() failed: %s\n", Update.errorString());
    firmware.close();
    return;
  }

  // Write firmware
  size_t written = Update.writeStream(firmware);
  if (written != firmware.size()) {
    Serial.printf("Write failed: expected %zu bytes, wrote %zu bytes\n",
                  firmware.size(), written);
    firmware.close();
    Update.abort();
    return;
  }

  // Finalize update
  if (Update.end()) {
    Serial.println("Firmware update completed successfully!");
    Serial.println("Restarting");
    firmware.close();

    pv.showMessage(
        "Installed app successfully\nPress & Hold OK to come back here");

    reboot(LittleFS);
  } else {
    Serial.printf("Update.end() failed: %s\n", Update.errorString());
    firmware.close();
  }
}

void chooseFirmware(fs::FS &fs, const char *app_dir) {
  Serial.printf("Listing directory: %s\r\n", app_dir);
  File root = fs.open(app_dir);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  size_t len = 0;
  struct {
    char name[32];
    char path[32];
  } firmwareNames[64];

  const char *firmwares[64];

  while (file) {
    if (!file.isDirectory()) {
      if (endsWith(file.name(), ".tar")) {
        Serial.print("  FILE: ");
        Serial.print(file.name());
        Serial.print("\tSIZE: ");
        Serial.println(file.size());

        // Copy the string to persistent storage
        strncpy(firmwareNames[len].name, file.name(), 31);
        strncpy(firmwareNames[len].path, file.path(), 31);
        firmwareNames[len].name[31] = '\0';       // Ensure null termination
        firmwareNames[len].path[31] = '\0';       // Ensure null termination
        firmwares[len] = firmwareNames[len].name; // Point to the copy
        len++;
      }
    }
    file = root.openNextFile();
  }
  int index = pv.subMenu("Firmwares", firmwares, len);

  updateFirmware(firmwareNames[index].path, TMP_DIR);
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();

  pinMode(pins::UP, INPUT_PULLUP);
  pinMode(pins::DOWN, INPUT_PULLUP);
  pinMode(pins::LEFT, INPUT_PULLUP);
  pinMode(pins::RIGHT, INPUT_PULLUP);
  pinMode(pins::SEL, INPUT_PULLUP);

  pv.showMessage("Hello");

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    ESP_LOGE("FILESYS", "LittleFS failed to initialize");
    return;
  }

  // updateFirmware("/foo.tar", TMP_DIR);
  LittleFS.mkdir(TMP_DIR);
  listDir(LittleFS, "/", 8);
  chooseFirmware(LittleFS, "/");
}

void loop() {}
