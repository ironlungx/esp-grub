#include <Arduino.h>
#include <ESP32-targz.h>
#include <LittleFS.h>
#include <Update.h>
#define FORMAT_LITTLEFS_IF_FAILED true

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

uint64_t myTotalBytesFn() { return LittleFS.totalBytes(); }

uint64_t myFreeBytesFn() {
  return LittleFS.totalBytes() - LittleFS.usedBytes();
}

void updateFirmware(const char *file, const char *dest_folder) {
  if (!LittleFS.exists(file)) {
    Serial.println("TAR file not found");
    return;
  }

  // Extract TAR file
  TarUnpacker *unpacker = new TarGzUnpacker();
  unpacker->haltOnError(true);
  unpacker->setTarVerify(true);

  // Fixed filesystem callbacks
  unpacker->setupFSCallbacks(myTotalBytesFn, myFreeBytesFn);

  // Progress callbacks
  unpacker->setLoggerCallback(BaseUnpacker::targzPrintLoggerCallback);
  unpacker->setTarProgressCallback(BaseUnpacker::defaultProgressCallback);
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

  // Build firmware path safely
  char firmwarePath[256];
  if (strcmp(dest_folder, "/") == 0) {
    strcpy(firmwarePath, "/firmware.bin");
  } else {
    snprintf(firmwarePath, sizeof(firmwarePath), "%s/firmware.bin",
             dest_folder);
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
    Serial.println("Restarting in 2 seconds...");
    firmware.close();
    delay(2000);
    ESP.restart();
  } else {
    Serial.printf("Update.end() failed: %s\n", Update.errorString());
    firmware.close();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    ESP_LOGE("FILESYS", "LittleFS failed to initialize");
    return;
  }

  updateFirmware("/foo.tar", "/");
}

void loop() {
  // Serial.println("bar");
}
