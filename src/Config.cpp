#include "Config.h"

const char DPS_GLOBAL_DEVICE_ENDPOINT_HOST[] = "global.azure-devices-provisioning.net";

#if defined(PROJECT_WIOTERMINAL_HUMAN_DETECTION_PIR)
const char MODEL_ID[] = "dtmi:seeedkk:wioterminal:wioterminal_human_detection;1";
#elif defined(PROJECT_WIOTERMINAL_TEMP_HUMI_SHT31)
const char MODEL_ID[] = "dtmi:seeedkk:wioterminal:wioterminal_temp_humi;1";
#elif defined(PROJECT_WIOTERMINAL_ACCUMULATION_COUNTER_ULTRASONIC)
const char MODEL_ID[] = "dtmi:seeedkk:wioterminal:wioterminal_accumulation_counter;1";
#else
#error No project macro defined.
#endif
