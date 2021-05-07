#include <Arduino.h>
#include "Config.h"

#include "ConfigurationMode.h"
#include <GroveDriverPack.h>

////////////////////////////////////////////////////////////////////////////////
// Storage

#include <ExtFlashLoader.h>
#include "Storage.h"

static ExtFlashLoader::QSPIFlash Flash_;
static Storage Storage_(Flash_);

////////////////////////////////////////////////////////////////////////////////
// Display

#include <LovyanGFX.hpp>
#include "Display.h"

static LGFX Gfx_;
static Display Display_(Gfx_);

////////////////////////////////////////////////////////////////////////////////
// Network

#include <Network/WiFiManager.h>
#include <Network/TimeManager.h>
#include <Aziot/AziotDps.h>
#include <Aziot/AziotHub.h>
#include <ArduinoJson.h>

static TimeManager TimeManager_;
static std::string HubHost_;
static std::string DeviceId_;
static AziotHub AziotHub_;

static void ConnectWiFi()
{
    Display_.Printf("Connecting to SSID: %s\n", Storage_.WiFiSSID.c_str());
	WiFiManager wifiManager;
	wifiManager.Connect(Storage_.WiFiSSID.c_str(), Storage_.WiFiPassword.c_str());
	while (!wifiManager.IsConnected())
	{
		Display_.Printf(".");
		delay(500);
	}
	Display_.Printf("Connected\n");
}

static void SyncTimeServer()
{
	Display_.Printf("Synchronize time\n");
	while (!TimeManager_.Update())
	{
		Display_.Printf(".");
		delay(1000);
	}
	Display_.Printf("Synchronized\n");
}

static bool DeviceProvisioning()
{
	Display_.Printf("Device provisioning:\n");
    Display_.Printf(" Id scope = %s\n", Storage_.IdScope.c_str());
    Display_.Printf(" Registration id = %s\n", Storage_.RegistrationId.c_str());

	AziotDps aziotDps;
	aziotDps.SetMqttPacketSize(MQTT_PACKET_SIZE);

    if (aziotDps.RegisterDevice(DPS_GLOBAL_DEVICE_ENDPOINT_HOST, Storage_.IdScope, Storage_.RegistrationId, Storage_.SymmetricKey, MODEL_ID, TimeManager_.GetEpochTime() + TOKEN_LIFESPAN, &HubHost_, &DeviceId_) != 0)
    {
        Display_.Printf("ERROR: RegisterDevice()\n");
		return false;
    }

    Display_.Printf("Device provisioned:\n");
    Display_.Printf(" Hub host = %s\n", HubHost_.c_str());
    Display_.Printf(" Device id = %s\n", DeviceId_.c_str());

    return true;
}

static bool AziotIsConnected()
{
    return AziotHub_.IsConnected();
}

static void AziotDoWork()
{
    static unsigned long connectTime = 0;
    static unsigned long forceDisconnectTime;

    bool repeat;
    do
    {
        repeat = false;

        const auto now = TimeManager_.GetEpochTime();
        if (!AziotHub_.IsConnected())
        {
            if (now >= connectTime)
            {
                Serial.printf("Connecting to Azure IoT Hub...\n");
                if (AziotHub_.Connect(HubHost_, DeviceId_, Storage_.SymmetricKey, MODEL_ID, now + TOKEN_LIFESPAN) != 0)
                {
                    Serial.printf("ERROR: Try again in 5 seconds\n");
                    connectTime = TimeManager_.GetEpochTime() + 5;
                    return;
                }

                Serial.printf("SUCCESS\n");
                forceDisconnectTime = TimeManager_.GetEpochTime() + static_cast<unsigned long>(TOKEN_LIFESPAN * RECONNECT_RATE);

                AziotHub_.RequestTwinDocument("get_twin");
            }
        }
        else
        {
            if (now >= forceDisconnectTime)
            {
                Serial.printf("Disconnect\n");
                AziotHub_.Disconnect();
                connectTime = 0;

                repeat = true;
            }
            else
            {
                AziotHub_.DoWork();
            }
        }
    }
    while (repeat);
}

template <typename T>
static void AziotSendConfirm(const char* requestId, const char* name, T value, int ackCode, int ackVersion)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	doc[name]["value"] = value;
	doc[name]["ac"] = ackCode;
	doc[name]["av"] = ackVersion;

	char json[JSON_MAX_SIZE];
	serializeJson(doc, json);

	AziotHub_.SendTwinPatch(requestId, json);
}

template <typename T>
static bool AziotUpdateWritableProperty(const char* name, T* value, const JsonVariant& desiredVersion, const JsonVariant& desired, const JsonVariant& reported = JsonVariant())
{
    bool ret = false;

    JsonVariant desiredValue = desired[name];
    JsonVariant reportedProperty = reported[name];

	if (!desiredValue.isNull())
    {
        *value = desiredValue.as<T>();
        ret = true;
    }

    if (desiredValue.isNull())
    {
        if (reportedProperty.isNull())
        {
            AziotSendConfirm<T>("init", name, *value, 200, 1);
        }
    }
    else if (reportedProperty.isNull() || desiredVersion.as<int>() != reportedProperty["av"].as<int>())
    {
        AziotSendConfirm<T>("update", name, *value, 200, desiredVersion.as<int>());
    }

    return ret;
}

template <typename T>
static bool AziotUpdateReadOnlyProperty(const char* name, T* value, const JsonVariant& reported)
{
    bool ret = false;

    JsonVariant reportedProperty = reported[name];

	if (!reportedProperty.isNull())
    {
        *value = reportedProperty.as<T>();
        ret = true;
    }

    return ret;
}

template <size_t capacity>
static void AziotSendReadOnlyProperty(const StaticJsonDocument<capacity>& jsonDoc)
{
	char json[jsonDoc.capacity()];
	serializeJson(jsonDoc, json, sizeof(json));

	AziotHub_.SendTwinPatch("report", json);
}

template <size_t capacity>
static void AziotSendTelemetry(const StaticJsonDocument<capacity>& jsonDoc)
{
	char json[jsonDoc.capacity()];
	serializeJson(jsonDoc, json, sizeof(json));

	AziotHub_.SendTelemetry(json);
}

////////////////////////////////////////////////////////////////////////////////
// Project Specific

#if defined(PROJECT_WIOTERMINAL_HUMAN_DETECTION_PIR)

static GroveBoard Board_;
static GrovePIR Sensor_(&Board_.GroveBCM27);

static unsigned long TelemetryInterval_ = TELEMETRY_INTERVAL;   // [sec.]

static void ReceivedTwinDocument(const char* json, const char* requestId)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["desired"]["$version"].isNull()) return;
    
    if (AziotUpdateWritableProperty("TelemetryInterval", &TelemetryInterval_, doc["desired"]["$version"], doc["desired"], doc["reported"]))
    {
		Serial.printf("TelemetryInterval = %d\n", TelemetryInterval_);
    }
}

static void ReceivedTwinDesiredPatch(const char* json, const char* version)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["$version"].isNull()) return;

    if (AziotUpdateWritableProperty("TelemetryInterval", &TelemetryInterval_, doc["$version"], doc.as<JsonVariant>()))
    {
		Serial.printf("TelemetryInterval = %d\n", TelemetryInterval_);
    }
}

static void ProjectSetup()
{
    Board_.GroveBCM27.Enable();
    Sensor_.Init();

    AziotHub_.ReceivedTwinDocumentCallback = ReceivedTwinDocument;
    AziotHub_.ReceivedTwinDesiredPatchCallback = ReceivedTwinDesiredPatch;
}

static void ProjectLoop()
{
    static unsigned long nextCaptureTime = 0;
    static bool sensorValue = false;
    static unsigned long nextTelemetrySendTime = 0;

    if (millis() >= nextCaptureTime)
    {
        bool preSensorValue = sensorValue;
        sensorValue = Sensor_.IsOn();

        if (!preSensorValue && sensorValue || preSensorValue && !sensorValue)
        {
            const time_t now = TimeManager_.GetEpochTime();
            char nowStr[32];
            strftime(nowStr, sizeof(nowStr), "%H:%M:%S %Z", localtime(&now));
            Display_.Printf("%s - %s\n", nowStr, sensorValue ? "Detected" : "Gone");
        }

        if ((!preSensorValue && sensorValue)                   ||
            (sensorValue && millis() >= nextTelemetrySendTime)   )
        {
            if (AziotIsConnected())
            {
                StaticJsonDocument<JSON_MAX_SIZE> doc;
                doc["detect"] = "exist";
                AziotSendTelemetry<JSON_MAX_SIZE>(doc);

                nextTelemetrySendTime = millis() + TelemetryInterval_ * 1000;
            }
        }

        nextCaptureTime = millis() + CAPTURE_INTERVAL;
    }
}

#elif defined(PROJECT_WIOTERMINAL_TEMP_HUMI_SHT31)

static GroveBoard Board_;
static GroveTempHumiSHT31 Sensor_(&Board_.GroveI2C1);

static unsigned long TelemetryInterval_ = TELEMETRY_INTERVAL;   // [sec.]

static void ReceivedTwinDocument(const char* json, const char* requestId)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["desired"]["$version"].isNull()) return;
    
    if (AziotUpdateWritableProperty("TelemetryInterval", &TelemetryInterval_, doc["desired"]["$version"], doc["desired"], doc["reported"]))
    {
		Serial.printf("TelemetryInterval = %d\n", TelemetryInterval_);
    }
}

static void ReceivedTwinDesiredPatch(const char* json, const char* version)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["$version"].isNull()) return;

    if (AziotUpdateWritableProperty("TelemetryInterval", &TelemetryInterval_, doc["$version"], doc.as<JsonVariant>()))
    {
		Serial.printf("TelemetryInterval = %d\n", TelemetryInterval_);
    }
}

static void ProjectSetup()
{
    Board_.GroveI2C1.Enable();
    Sensor_.Init();

    AziotHub_.ReceivedTwinDocumentCallback = ReceivedTwinDocument;
    AziotHub_.ReceivedTwinDesiredPatchCallback = ReceivedTwinDesiredPatch;
}

static void ProjectLoop()
{
    static unsigned long nextCaptureTime = 0;
    static unsigned long nextTelemetrySendTime = 0;

    if (millis() >= nextCaptureTime)
    {
        Sensor_.Read();
        const float temp = Sensor_.Temperature;
        const float humi = Sensor_.Humidity;

        const time_t now = TimeManager_.GetEpochTime();
        char nowStr[32];
        strftime(nowStr, sizeof(nowStr), "%H:%M:%S %Z", localtime(&now));
        Display_.Printf("%s - %.1f[C]  %.1f[%%]\n", nowStr, temp, humi);

        if (millis() >= nextTelemetrySendTime)
        {
            if (AziotIsConnected())
            {
                StaticJsonDocument<JSON_MAX_SIZE> doc;
                doc["temp"] = temp;
                doc["humi"] = humi;
                AziotSendTelemetry<JSON_MAX_SIZE>(doc);

                nextTelemetrySendTime = millis() + TelemetryInterval_ * 1000;
            }
        }

        nextCaptureTime = millis() + CAPTURE_INTERVAL;
    }
}

#elif defined(PROJECT_WIOTERMINAL_ACCUMULATION_COUNTER_ULTRASONIC)

static GroveBoard Board_;
static GroveUltrasonicRanger Sensor_(&Board_.GroveBCM27);

static unsigned long TelemetryInterval_ = TELEMETRY_INTERVAL;   // [sec.]
static int DistanceThreshold_ = DISTANCE_THRESHOLD;             // [cm]

static void ReceivedTwinDocument(const char* json, const char* requestId)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["desired"]["$version"].isNull()) return;
    
    if (AziotUpdateWritableProperty("TelemetryInterval", &TelemetryInterval_, doc["desired"]["$version"], doc["desired"], doc["reported"]))
    {
		Serial.printf("TelemetryInterval = %d\n", TelemetryInterval_);
    }
    if (AziotUpdateWritableProperty("distanceThreshold", &DistanceThreshold_, doc["desired"]["$version"], doc["desired"], doc["reported"]))
    {
		Serial.printf("distanceThreshold = %d\n", DistanceThreshold_);
    }
}

static void ReceivedTwinDesiredPatch(const char* json, const char* version)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["$version"].isNull()) return;

    if (AziotUpdateWritableProperty("TelemetryInterval", &TelemetryInterval_, doc["$version"], doc.as<JsonVariant>()))
    {
		Serial.printf("TelemetryInterval = %d\n", TelemetryInterval_);
    }
    if (AziotUpdateWritableProperty("distanceThreshold", &DistanceThreshold_, doc["$version"], doc.as<JsonVariant>()))
    {
		Serial.printf("distanceThreshold = %d\n", DistanceThreshold_);
    }
}

static void ProjectSetup()
{
    Board_.GroveBCM27.Enable();
    Sensor_.Init();

    AziotHub_.ReceivedTwinDocumentCallback = ReceivedTwinDocument;
    AziotHub_.ReceivedTwinDesiredPatchCallback = ReceivedTwinDesiredPatch;
}

static void ProjectLoop()
{
    static int accumulationCount = 0;
    static unsigned long nextCaptureTime = 0;
    static int sensorValue = std::numeric_limits<int>::max();
    static unsigned long nextTelemetrySendTime = 0;

    if (millis() >= nextCaptureTime)
    {
        const auto preSensorValue = sensorValue;
        Sensor_.Read();
        sensorValue = static_cast<int>(Sensor_.Distance); // [mm]

        if (preSensorValue > DistanceThreshold_ && sensorValue <= DistanceThreshold_) accumulationCount++;

        const time_t now = TimeManager_.GetEpochTime();
        char nowStr[32];
        strftime(nowStr, sizeof(nowStr), "%H:%M:%S %Z", localtime(&now));
        Display_.Printf("%s - %4d[mm]  %d\n", nowStr, sensorValue, accumulationCount);

        if (millis() >= nextTelemetrySendTime)
        {
            if (AziotIsConnected())
            {
                StaticJsonDocument<JSON_MAX_SIZE> doc;
                doc["accumulationCount"] = accumulationCount;
                AziotSendTelemetry<JSON_MAX_SIZE>(doc);

                nextTelemetrySendTime = millis() + TelemetryInterval_ * 1000;
            }
        }

        nextCaptureTime = millis() + CAPTURE_INTERVAL;
    }
}

#elif defined(PROJECT_WIOTERMINAL_WATCHDOG_VL53L0X)

#include <Seeed_vl53l0x.h>

static Seeed_vl53l0x VL53L0X_;

static unsigned long Period_ = PERIOD;                          // [sec.]
static int DistanceThreshold_ = DISTANCE_THRESHOLD;             // [cm]
static int CountInPeriodThreshold_ = COUNT_IN_PERIOD_THRESHOLD;

static void ReceivedTwinDocument(const char* json, const char* requestId)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["desired"]["$version"].isNull()) return;
    
    if (AziotUpdateWritableProperty("period", &Period_, doc["desired"]["$version"], doc["desired"], doc["reported"]))
    {
		Serial.printf("period = %d\n", Period_);
    }
    if (AziotUpdateWritableProperty("distanceThreshold", &DistanceThreshold_, doc["desired"]["$version"], doc["desired"], doc["reported"]))
    {
		Serial.printf("distanceThreshold = %d\n", DistanceThreshold_);
    }
    if (AziotUpdateWritableProperty("countInPeriodThreshold", &CountInPeriodThreshold_, doc["desired"]["$version"], doc["desired"], doc["reported"]))
    {
		Serial.printf("countInPeriodThreshold = %d\n", CountInPeriodThreshold_);
    }
}

static void ReceivedTwinDesiredPatch(const char* json, const char* version)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["$version"].isNull()) return;

    if (AziotUpdateWritableProperty("period", &Period_, doc["$version"], doc.as<JsonVariant>()))
    {
		Serial.printf("period = %d\n", Period_);
    }
    if (AziotUpdateWritableProperty("distanceThreshold", &DistanceThreshold_, doc["$version"], doc.as<JsonVariant>()))
    {
		Serial.printf("distanceThreshold = %d\n", DistanceThreshold_);
    }
    if (AziotUpdateWritableProperty("countInPeriodThreshold", &CountInPeriodThreshold_, doc["$version"], doc.as<JsonVariant>()))
    {
		Serial.printf("countInPeriodThreshold = %d\n", CountInPeriodThreshold_);
    }
}

static void ProjectSetup()
{
    VL53L0X_Error status = VL53L0X_.VL53L0X_common_init();
    if (status != VL53L0X_ERROR_NONE)
    {
        Display_.Printf("ERROR: VL53L0X_common_init()\n");
        abort();
    }
    status = VL53L0X_.VL53L0X_single_ranging_init();
    if (status != VL53L0X_ERROR_NONE)
    {
        Display_.Printf("ERROR: VL53L0X_single_ranging_init()\n");
        abort();
    }

    AziotHub_.ReceivedTwinDocumentCallback = ReceivedTwinDocument;
    AziotHub_.ReceivedTwinDesiredPatchCallback = ReceivedTwinDesiredPatch;
}

static void ProjectLoop()
{
    static int countInPeriod = 0;
    static unsigned long nextCaptureTime = 0;
    static int sensorValue = std::numeric_limits<int>::max();
    static unsigned long nextPeriodTime = 0;

    if (millis() >= nextCaptureTime)
    {
        const auto preSensorValue = sensorValue;

        VL53L0X_RangingMeasurementData_t rangingMeasurementData;
        VL53L0X_Error status = VL53L0X_.PerformSingleRangingMeasurement(&rangingMeasurementData);
        sensorValue = rangingMeasurementData.RangeMilliMeter;   // [mm]

        if (preSensorValue > DistanceThreshold_ && sensorValue <= DistanceThreshold_) countInPeriod++;

        const time_t now = TimeManager_.GetEpochTime();
        char nowStr[32];
        strftime(nowStr, sizeof(nowStr), "%H:%M:%S %Z", localtime(&now));
        Display_.Printf("%s - %4d[mm]  %d\n", nowStr, sensorValue, countInPeriod);

        if (millis() >= nextPeriodTime)
        {
            if (AziotIsConnected())
            {
                StaticJsonDocument<JSON_MAX_SIZE> doc;
                doc["countInPeriod"] = countInPeriod;
                doc["status"] = countInPeriod >= CountInPeriodThreshold_ ? "run": "stop";
                AziotSendTelemetry<JSON_MAX_SIZE>(doc);

                nextPeriodTime = millis() + Period_ * 1000;
            }

            countInPeriod = 0;
        }

        nextCaptureTime = millis() + CAPTURE_INTERVAL;
    }
}

#elif defined(PROJECT_WIOTERMINAL_DUST_HM3301)

static GroveBoard Board_;
static GrovePM25HM3301 Sensor_(&Board_.GroveI2C1);

static unsigned long TelemetryInterval_ = TELEMETRY_INTERVAL;   // [sec.]

static void ReceivedTwinDocument(const char* json, const char* requestId)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["desired"]["$version"].isNull()) return;
    
    if (AziotUpdateWritableProperty("telemetryInterval", &TelemetryInterval_, doc["desired"]["$version"], doc["desired"], doc["reported"]))
    {
		Serial.printf("telemetryInterval = %d\n", TelemetryInterval_);
    }
}

static void ReceivedTwinDesiredPatch(const char* json, const char* version)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["$version"].isNull()) return;

    if (AziotUpdateWritableProperty("telemetryInterval", &TelemetryInterval_, doc["$version"], doc.as<JsonVariant>()))
    {
		Serial.printf("telemetryInterval = %d\n", TelemetryInterval_);
    }
}

static void ProjectSetup()
{
    Board_.GroveI2C1.Enable();
    if (!Sensor_.Init())
    {
        Display_.Printf("ERROR: Sensor not found.\n");
        abort();
    }

    AziotHub_.ReceivedTwinDocumentCallback = ReceivedTwinDocument;
    AziotHub_.ReceivedTwinDesiredPatchCallback = ReceivedTwinDesiredPatch;
}

static void ProjectLoop()
{
    static unsigned long nextCaptureTime = 0;
    static unsigned long nextTelemetrySendTime = 0;

    if (millis() >= nextCaptureTime)
    {
        Sensor_.Read();
        if (Sensor_.Concentration_1_Standard != INT_MAX)
        {
            const time_t now = TimeManager_.GetEpochTime();
            char nowStr[32];
            strftime(nowStr, sizeof(nowStr), "%H:%M:%S %Z", localtime(&now));
            Display_.Printf("%s - ", nowStr);
            Display_.Printf("%d[ug/m3] ", Sensor_.Concentration_2_5_Standard);
            Display_.Printf("%d[#/l]\n", Sensor_.ParticleNumber_2_5);

            if (millis() >= nextTelemetrySendTime)
            {
                if (AziotIsConnected())
                {
                    StaticJsonDocument<JSON_MAX_SIZE> doc;
                    doc["concentration1"]["standard"] = Sensor_.Concentration_1_Standard;
                    doc["concentration1"]["atmospheric"] = Sensor_.Concentration_1_Atmospheric;
                    doc["concentration2_5"]["standard"] = Sensor_.Concentration_2_5_Standard;
                    doc["concentration2_5"]["atmospheric"] = Sensor_.Concentration_2_5_Atmospheric;
                    doc["concentration10"]["standard"] = Sensor_.Concentration_10_Standard;
                    doc["concentration10"]["atmospheric"] = Sensor_.Concentration_10_Atmospheric;
                    doc["particleNumber0_3"] = Sensor_.ParticleNumber_0_3;
                    doc["particleNumber0_5"] = Sensor_.ParticleNumber_0_5;
                    doc["particleNumber1"] = Sensor_.ParticleNumber_1;
                    doc["particleNumber2_5"] = Sensor_.ParticleNumber_2_5;
                    doc["particleNumber5"] = Sensor_.ParticleNumber_5;
                    doc["particleNumber10"] = Sensor_.ParticleNumber_10;
                    AziotSendTelemetry<JSON_MAX_SIZE>(doc);

                    nextTelemetrySendTime = millis() + TelemetryInterval_ * 1000;
                }
            }
        }

        nextCaptureTime = millis() + CAPTURE_INTERVAL;
    }
}

#elif defined(PROJECT_WIOTERMINAL_DIGITAL_SIGNAGE)

#include <AceButton.h>
using namespace ace_button;

enum class ButtonId
{
    RIGHT = 0,
    CENTER,
    LEFT,
};
static const int BUTTON_NUMBER = 3;
static AceButton Buttons_[BUTTON_NUMBER];
static bool ButtonsClicked_[BUTTON_NUMBER];

static void ButtonEventHandler(AceButton* button, uint8_t eventType, uint8_t buttonState)
{
    const uint8_t id = button->getId();
    if (BUTTON_NUMBER <= id) return;

    switch (eventType)
    {
    case AceButton::kEventClicked:
        switch (static_cast<ButtonId>(id))
        {
        case ButtonId::RIGHT:
            Serial.printf("Right button was clicked\n");
            break;
        case ButtonId::CENTER:
            Serial.printf("Center button was clicked\n");
            break;
        case ButtonId::LEFT:
            Serial.printf("Left button was clicked\n");
            break;
        }
        ButtonsClicked_[id] = true;
        break;
    }
}

static void ButtonInit()
{
    Buttons_[static_cast<int>(ButtonId::RIGHT)].init(WIO_KEY_A, HIGH, static_cast<uint8_t>(ButtonId::RIGHT));
    Buttons_[static_cast<int>(ButtonId::CENTER)].init(WIO_KEY_B, HIGH, static_cast<uint8_t>(ButtonId::CENTER));
    Buttons_[static_cast<int>(ButtonId::LEFT)].init(WIO_KEY_C, HIGH, static_cast<uint8_t>(ButtonId::LEFT));

    ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
    buttonConfig->setEventHandler(ButtonEventHandler);
    buttonConfig->setFeature(ButtonConfig::kFeatureClick);

    for (int i = 0; i < BUTTON_NUMBER; i++) ButtonsClicked_[i] = false;
}

static void ButtonDoWork()
{
    for (int i = 0; static_cast<size_t>(i) < BUTTON_NUMBER; ++i)
    {
        Buttons_[i].check();
    }
}

static void DisplayMessage(const std::string& message)
{
    Display_.Clear();
    Display_.PrintMessage(message.c_str());
}

static std::string Message_ = "";
static int RightButtonClickCount_ = 0;
static int CenterButtonClickCount_ = 0;
static int LeftButtonClickCount_ = 0;

static void ReceivedTwinDocument(const char* json, const char* requestId)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["desired"]["$version"].isNull()) return;
    
    if (AziotUpdateWritableProperty("message", &Message_, doc["desired"]["$version"], doc["desired"], doc["reported"]))
    {
		Serial.printf("message = %s\n", Message_.c_str());
    }

    doc.clear();
    doc["rightButtonClickCount"] = RightButtonClickCount_;
    doc["centerButtonClickCount"] = CenterButtonClickCount_;
    doc["leftButtonClickCount"] = LeftButtonClickCount_;
    AziotSendReadOnlyProperty<JSON_MAX_SIZE>(doc);

    DisplayMessage(Message_);
}

static void ReceivedTwinDesiredPatch(const char* json, const char* version)
{
	StaticJsonDocument<JSON_MAX_SIZE> doc;
	if (deserializeJson(doc, json)) return;
	if (doc["$version"].isNull()) return;

    if (AziotUpdateWritableProperty("message", &Message_, doc["$version"], doc.as<JsonVariant>()))
    {
		Serial.printf("message = %s\n", Message_.c_str());

        RightButtonClickCount_ = 0;
        CenterButtonClickCount_ = 0;
        LeftButtonClickCount_ = 0;

        doc.clear();
        doc["rightButtonClickCount"] = RightButtonClickCount_;
        doc["centerButtonClickCount"] = CenterButtonClickCount_;
        doc["leftButtonClickCount"] = LeftButtonClickCount_;
        AziotSendReadOnlyProperty<JSON_MAX_SIZE>(doc);

        DisplayMessage(Message_);
    }
}

static void ProjectSetup()
{
    AziotHub_.ReceivedTwinDocumentCallback = ReceivedTwinDocument;
    AziotHub_.ReceivedTwinDesiredPatchCallback = ReceivedTwinDesiredPatch;

    ButtonInit();
}

static void ProjectLoop()
{
    ButtonDoWork();

    if (AziotIsConnected())
    {
        bool clicked = false;
        for (int i = 0; i < BUTTON_NUMBER; i++) if (ButtonsClicked_[i]) clicked = true;

        if (clicked)
        {
            StaticJsonDocument<JSON_MAX_SIZE> doc;

            if (ButtonsClicked_[static_cast<int>(ButtonId::RIGHT)])
            {
                doc["rightButtonClickCount"] = ++RightButtonClickCount_;
                ButtonsClicked_[static_cast<int>(ButtonId::RIGHT)] = false;
            }
            if (ButtonsClicked_[static_cast<int>(ButtonId::CENTER)])
            {
                doc["centerButtonClickCount"] = ++CenterButtonClickCount_;
                ButtonsClicked_[static_cast<int>(ButtonId::CENTER)] = false;
            }
            if (ButtonsClicked_[static_cast<int>(ButtonId::LEFT)])
            {
                doc["leftButtonClickCount"] = ++LeftButtonClickCount_;
                ButtonsClicked_[static_cast<int>(ButtonId::LEFT)] = false;
            }

            AziotSendReadOnlyProperty<JSON_MAX_SIZE>(doc);
        }
    }
}

#else
#error No project macro defined.
#endif

////////////////////////////////////////////////////////////////////////////////
// setup and loop

void setup()
{
    ////////////////////
    // Load storage

    Storage_.Load();

    ////////////////////
    // Init base component

    Serial.begin(115200);
    Display_.Init();
    Display_.SetBrightness(DISPLAY_BRIGHTNESS);

    ////////////////////
    // Enter configuration mode

    pinMode(WIO_KEY_A, INPUT_PULLUP);
    pinMode(WIO_KEY_B, INPUT_PULLUP);
    pinMode(WIO_KEY_C, INPUT_PULLUP);
    delay(100);

    if (digitalRead(WIO_KEY_A) == LOW &&
        digitalRead(WIO_KEY_B) == LOW &&
        digitalRead(WIO_KEY_C) == LOW   )
    {
        Display_.Printf("In configuration mode\n");
        ConfigurationMode(Storage_);
    }

    ////////////////////
    // Networking

    ConnectWiFi();
    SyncTimeServer();
    if (!DeviceProvisioning()) abort();

    AziotHub_.SetMqttPacketSize(MQTT_PACKET_SIZE);

    ////////////////////
    // Project Specific

    ProjectSetup();
}

void loop()
{
    ////////////////////
    // Project Specific

    ProjectLoop();

    ////////////////////
    // Networking

    AziotDoWork();
}
