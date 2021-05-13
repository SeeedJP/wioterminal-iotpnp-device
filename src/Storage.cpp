#include <Arduino.h>
#include "Storage.h"

#include <MsgPack.h>

#include <ArduinoJson.h>
#define CONFIG_FILE "/settings.json"

#include <SFUD/Seeed_SFUD.h>
#define DISK_BLOCK_NUM  (SPIFLASH.flashSize() / DISK_BLOCK_SIZE)
#define DISK_BLOCK_SIZE SECTORSIZE

static Storage* Storage_;

static int32_t mscReadCB(uint32_t lba, void* buffer, uint32_t bufsize)
{
	return Storage_->MscReadCB(lba, buffer, bufsize);
}

static int32_t mscWriteCB(uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
	return Storage_->MscWriteCB(lba, buffer, bufsize);
}

static void mscFlushCB()
{
	Storage_->MscFlushCB();
}

Storage::Storage()
{
	Storage_ = this;

	ReadLba_ = -1;
	ReadSize_ = 0;
	WriteLba_ = -1;
	WriteSize_ = 0;
}

void Storage::Init()
{
	SPIFLASH.begin(50000000UL);
	UsbMsc_.setID("Seeed", "MSC", "0.1");
	UsbMsc_.setCapacity(DISK_BLOCK_NUM, DISK_BLOCK_SIZE);
	UsbMsc_.setReadWriteCallback(mscReadCB, mscWriteCB, mscFlushCB);
	UsbMsc_.setUnitReady(false);
	UsbMsc_.begin();
}

void Storage::Begin()
{
	UsbMsc_.setUnitReady(true);
}

void Storage::Load()
{
	// Remount the FAT just in case
	SPIFLASH.end();
	SPIFLASH.begin(50000000UL);

	File file = SPIFLASH.open(CONFIG_FILE, "r");
	if (!file) {
		return;
	}

	StaticJsonDocument<1024> doc;
	DeserializationError error = deserializeJson(doc, file);
	if (error)
		Serial.println(F("Failed to read file, using default configuration"));

	WiFiSSID = doc["wifi"]["ssid"].as<std::string>();
	WiFiPassword = doc["wifi"]["password"].as<std::string>();
	IdScope = doc["azure"]["idscope"].as<std::string>();
	RegistrationId = doc["azure"]["regid"].as<std::string>();
	SymmetricKey = doc["azure"]["symkey"].as<std::string>();

	file.close();
}

void Storage::Save()
{
	File file = SPIFLASH.open(CONFIG_FILE, "w");
	if (!file) {
		return;
	}

	StaticJsonDocument<1024> doc;
	doc["wifi"]["ssid"] = WiFiSSID;
	doc["wifi"]["password"] = WiFiPassword;
	doc["azure"]["idscope"] = IdScope;
	doc["azure"]["regid"] = RegistrationId;
	doc["azure"]["symkey"] = SymmetricKey;

	serializeJsonPretty(doc, file);
	file.close();
}

void Storage::Erase()
{
	WiFiSSID.clear();
	WiFiPassword.clear();
	IdScope.clear();
	RegistrationId.clear();
	SymmetricKey.clear();

	Save();
}

int32_t Storage::MscReadCB(uint32_t lba, void* buffer, uint32_t bufsize)
{
	const sfud_flash *flash = sfud_get_device_table();
	int addr = DISK_BLOCK_SIZE * lba + (512 * 0x01f8);
	// 8blocks
	if (ReadLba_ != lba) {
		ReadLba_ = lba;
		ReadSize_ = bufsize;
	} else {
		addr += ReadSize_;
		ReadSize_ += bufsize;
		if (ReadSize_ >= DISK_BLOCK_SIZE) ReadSize_ = 0;
	}

	if (sfud_read(flash, addr, bufsize, (uint8_t*)buffer) != SFUD_SUCCESS) {
		return -1;
	}

	return bufsize;
}

int32_t Storage::MscWriteCB(uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
	const sfud_flash *flash = sfud_get_device_table();
	int addr = DISK_BLOCK_SIZE * lba + (512 * 0x01f8);
	// 8blocks
	if (WriteLba_ != lba) {
		WriteLba_ = lba;
		WriteSize_ = bufsize;
	} else {
		addr += WriteSize_;
		WriteSize_ += bufsize;
		if (WriteSize_ >= DISK_BLOCK_SIZE) WriteSize_ = 0;
	}

	if (flash->chip.write_mode & SFUD_WM_PAGE_256B) {
		if (WriteSize_ == bufsize) {
			// erase 4KB block
			if (sfud_erase(flash, addr, DISK_BLOCK_SIZE) != SFUD_SUCCESS) {
				return -1;
			}
		}
		// write 2 x 256bytes
		if (sfud_write(flash, addr, 256, buffer) != SFUD_SUCCESS) {
			return -1;
		}
		if (sfud_write(flash, addr + 256, 256, buffer + 256) != SFUD_SUCCESS) {
			return -1;
		}
	} else if (flash->chip.write_mode & SFUD_WM_AAI) {
		if (sfud_erase_write(flash, addr, bufsize, buffer) != SFUD_SUCCESS) {
			return -1;
		}
	}

	return bufsize;
}

void Storage::MscFlushCB(void)
{
	ReadLba_ = -1;
	ReadSize_ = 0;
	WriteLba_ = -1;
	WriteSize_ = 0;
}
