#pragma once

#include <string>
#include <Adafruit_TinyUSB.h>

class Storage
{
private:
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

public:
	std::string WiFiSSID;
	std::string WiFiPassword;
	std::string IdScope;
	std::string RegistrationId;
	std::string SymmetricKey;

	Storage();
	void Load();
	void Save();
	void Erase();

	void Init();
	void Begin();

	int32_t MscReadCB(uint32_t lba, void* buffer, uint32_t bufsize);
	int32_t MscWriteCB(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
	void MscFlushCB();

protected:
	Adafruit_USBD_MSC UsbMsc_;
	uint32_t ReadLba_;
	uint32_t ReadSize_;
	uint32_t WriteLba_;
	uint32_t WriteSize_;

};
