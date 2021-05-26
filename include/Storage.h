#pragma once

#include <string>

class Adafruit_USBD_MSC;

class Storage
{
private:
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

public:
	std::string WiFiSSID;
	std::string WiFiPassword;
	std::string AzureConnection;
	std::string AzureDpsIdScope;
	std::string AzureDpsRegistrationId;
	std::string AzureDpsSymmetricKey;

	Storage(Adafruit_USBD_MSC& msc);
	void Load();
	void Save();
	void Erase();

	void Init();
	void ActivateMsc();

private:
	Adafruit_USBD_MSC& Msc_;
	uint32_t MscReadLba_;
	uint32_t MscReadSize_;
	uint32_t MscWriteLba_;
	uint32_t MscWriteSize_;

	int32_t MscReadHandler(uint32_t lba, void* buffer, uint32_t bufsize);
	int32_t MscWriteHandler(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
	void MscFlushHandler();

	static Storage* Instance_;

	static int32_t MscReadStaticHandler(uint32_t lba, void* buffer, uint32_t bufsize);
	static int32_t MscWriteStaticHandler(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
	static void MscFlushStaticHandler();

};
