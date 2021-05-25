#include <Arduino.h>
#include "Config.h"
#include "Storage.h"

#include <Adafruit_TinyUSB.h>
#include <ArduinoJson.h>
#include <SFUD/Seeed_SFUD.h>

#define DISK_BLOCK_NUM  (SPIFLASH.flashSize() / DISK_BLOCK_SIZE)
#define DISK_BLOCK_SIZE (SECTORSIZE)

Storage* Storage::Instance_ = nullptr;

Storage::Storage(Adafruit_USBD_MSC& msc) :
	Msc_(msc)
{
	Instance_ = this;

	MscReadLba_ = -1;
	MscReadSize_ = 0;
	MscWriteLba_ = -1;
	MscWriteSize_ = 0;
}

void Storage::Init()
{
	SPIFLASH.begin(50000000UL);
	Msc_.setID("Seeed", "MSC", "0.1");
	Msc_.setCapacity(DISK_BLOCK_NUM, DISK_BLOCK_SIZE);
	Msc_.setReadWriteCallback(MscReadStaticHandler, MscWriteStaticHandler, MscFlushStaticHandler);
	Msc_.setUnitReady(false);
	Msc_.begin();
}

void Storage::ActivateMsc()
{
	Msc_.setUnitReady(true);
}

void Storage::Load()
{
	// Remount the FAT just in case
	SPIFLASH.end();
	SPIFLASH.begin(50000000UL);

	bool loaded = false;
	File file = SPIFLASH.open(CONFIG_FILE, "r");
	if (file)
	{
		StaticJsonDocument<JSON_MAX_SIZE> doc;
		DeserializationError error = deserializeJson(doc, file);
		if (!error)
		{
			WiFiSSID = doc["wifi"]["ssid"].as<std::string>();
			WiFiPassword = doc["wifi"]["password"].as<std::string>();
			IdScope = doc["azure"]["dps"]["idScope"].as<std::string>();
			RegistrationId = doc["azure"]["dps"]["registrationId"].as<std::string>();
			SymmetricKey = doc["azure"]["dps"]["symmetricKey"].as<std::string>();

			loaded = true;
		}
		else
		{
			Serial.println("Failed to read file, using default configuration");
		}

		file.close();
	}

	if (!loaded)
	{
		WiFiSSID.clear();
		WiFiPassword.clear();
		IdScope.clear();
		RegistrationId.clear();
		SymmetricKey.clear();
	}
}

void Storage::Save()
{
	File file = SPIFLASH.open(CONFIG_FILE, "w");
	if (!file) return;

	StaticJsonDocument<JSON_MAX_SIZE> doc;
	doc["wifi"]["ssid"] = WiFiSSID;
	doc["wifi"]["password"] = WiFiPassword;
	doc["azure"]["dps"]["idScope"] = IdScope;
	doc["azure"]["dps"]["registrationId"] = RegistrationId;
	doc["azure"]["dps"]["symmetricKey"] = SymmetricKey;

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

int32_t Storage::MscReadHandler(uint32_t lba, void* buffer, uint32_t bufsize)
{
	const sfud_flash* flash = sfud_get_device_table();
	int addr = DISK_BLOCK_SIZE * lba + (512 * 0x01f8);
	// 8blocks
	if (MscReadLba_ != lba)
	{
		MscReadLba_ = lba;
		MscReadSize_ = bufsize;
	}
	else
	{
		addr += MscReadSize_;
		MscReadSize_ += bufsize;
		if (MscReadSize_ >= DISK_BLOCK_SIZE) MscReadSize_ = 0;
	}

	if (sfud_read(flash, addr, bufsize, (uint8_t*)buffer) != SFUD_SUCCESS) return -1;

	return bufsize;
}

int32_t Storage::MscWriteHandler(uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
	const sfud_flash* flash = sfud_get_device_table();
	int addr = DISK_BLOCK_SIZE * lba + (512 * 0x01f8);
	// 8blocks
	if (MscWriteLba_ != lba)
	{
		MscWriteLba_ = lba;
		MscWriteSize_ = bufsize;
	}
	else
	{
		addr += MscWriteSize_;
		MscWriteSize_ += bufsize;
		if (MscWriteSize_ >= DISK_BLOCK_SIZE) MscWriteSize_ = 0;
	}

	if (flash->chip.write_mode & SFUD_WM_PAGE_256B)
	{
		if (MscWriteSize_ == bufsize)
		{
			// erase 4KB block
			if (sfud_erase(flash, addr, DISK_BLOCK_SIZE) != SFUD_SUCCESS) return -1;
		}
		// write 2 x 256bytes
		if (sfud_write(flash, addr, 256, buffer) != SFUD_SUCCESS) return -1;
		if (sfud_write(flash, addr + 256, 256, buffer + 256) != SFUD_SUCCESS) return -1;
	}
	else if (flash->chip.write_mode & SFUD_WM_AAI)
	{
		if (sfud_erase_write(flash, addr, bufsize, buffer) != SFUD_SUCCESS) return -1;
	}

	return bufsize;
}

void Storage::MscFlushHandler()
{
	MscReadLba_ = -1;
	MscReadSize_ = 0;
	MscWriteLba_ = -1;
	MscWriteSize_ = 0;
}

int32_t Storage::MscReadStaticHandler(uint32_t lba, void* buffer, uint32_t bufsize)
{
	return Instance_->MscReadHandler(lba, buffer, bufsize);
}

int32_t Storage::MscWriteStaticHandler(uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
	return Instance_->MscWriteHandler(lba, buffer, bufsize);
}

void Storage::MscFlushStaticHandler()
{
	Instance_->MscFlushHandler();
}
