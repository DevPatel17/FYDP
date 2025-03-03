
#define BROWN_VENT 1
#define WHITE_VENT 2

// ONLY SET THIS DEFINE TO BROWN_VENT OR WHITE_VENT
#define VENT_COLOR BROWN_VENT

#if VENT_COLOR == BROWN_VENT
    #define VENT_NAME "BLE-Server1"
    #define VOLTAGE_PADDING 0.2f
#elif VENT_COLOR == WHITE_VENT
    #define VENT_NAME "BLE-Server2"
    #define VOLTAGE_PADDING 0.15f
#endif