#pragma once

constexpr int DISPLAY_BRIGHTNESS = 127;         // 0-255

#if defined(PROJECT_WIOTERMINAL_HUMAN_DETECTION_PIR)
constexpr int CAPTURE_INTERVAL = 100;           // [msec.]
constexpr int TELEMETRY_INTERVAL = 60;          // [sec.]
#elif defined(PROJECT_WIOTERMINAL_TEMP_HUMI_SHT31)
constexpr int CAPTURE_INTERVAL = 1000;          // [msec.]
constexpr int TELEMETRY_INTERVAL = 60;          // [sec.]
#elif defined(PROJECT_WIOTERMINAL_ACCUMULATION_COUNTER_ULTRASONIC)
constexpr int CAPTURE_INTERVAL = 100;           // [msec.]
constexpr int TELEMETRY_INTERVAL = 60;          // [sec.]
constexpr int DISTANCE_THRESHOLD = 100;         // [mm]
#elif defined(PROJECT_WIOTERMINAL_WATCHDOG_VL53L0X)
constexpr int CAPTURE_INTERVAL = 100;           // [msec.]
constexpr int PERIOD = 60;                      // [sec.]
constexpr int DISTANCE_THRESHOLD = 100;         // [mm]
constexpr int COUNT_IN_PERIOD_THRESHOLD = 5;
#elif defined(PROJECT_WIOTERMINAL_DUST_HM3301)
constexpr int CAPTURE_INTERVAL = 1000;          // [msec.]
constexpr int TELEMETRY_INTERVAL = 60;          // [sec.]
#else
#error No project macro defined.
#endif

extern const char DPS_GLOBAL_DEVICE_ENDPOINT_HOST[];
extern const char MODEL_ID[];
constexpr int MQTT_PACKET_SIZE = 1024;
constexpr int TOKEN_LIFESPAN = 1 * 60 * 60;     // [sec.]
constexpr float RECONNECT_RATE = 0.85;
constexpr int JSON_MAX_SIZE = 1024;
