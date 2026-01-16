#define VERSION "26.1.16-4"

/*
 *     English: Light server based on ESP8266 or ESP32
 *     Français : Serveur d'éclairage à base d'ESP8266 ou d'ESP32
 *
 *  Available URL
 *      /           Root page
 *      /status     Returns status in JSON format
 *      /setup      Display setup page
 *      /command    Supports the following commands:
 *                      enable enter - disable enter
 *                      enable debug - disable debug
 *                      enable verbose - disable verbose
 *                      enable java - disable java
 *                      enable syslog - disable syslog
 *      /upload     Upload file (internal use only)
 *      /languages  Return list of supported languages
 *      /configs    Return list of configuration files
 *      /settings   Returns settings in JSON format
 *      /debug      Display internal variables to debug
 *      /log        Return saved log
 *      /edit       Manage and edit file system
 *      /tables     Display all program tables
 *      /changed    Change a variable value (internal use only)
 *      /rest       Execute API commands
 *          /restart
 *                      Restart ESP
 *
 *  URL disponibles
 *      /           Page d'accueil
 *      /status     Retourne l'état sous forme JSON
 *      /setup      Affiche la page de configuration
 *      /command    Supporte les commandes suivantes :
 *                      enable enter - disable enter
 *                      enable debug - disable debug
 *                      enable verbose - disable verbose
 *                      enable java - disable java
 *                      enable syslog - disable syslog
 *      /upload     Charge un fichier (utilisation interne)
 *      /languages  Retourne la liste des langues supportées
 *      /configs    Retourne la liste des fihciers de configuration
 *      /settings   Retourne la configuration au format JSON
 *      /debug      Affiche les variables internes pour déverminer
 *      /log        Retourne le log mémorisé
 *      /edit       Gère et édite le système de fichier
 *      /tables     Affiche l'ensemble des tables du programme
 *      /changed    Change la valeur d'une variable (utilisation interne)
 *      /rest       Exécute une commande de type API
 *          /restart
 *                      Redémarre l'ESP
 *
 *  Flying Domotic - Novembre 2025
 *
 *  GNU GENERAL PUBLIC LICENSE - Version 3, 29 June 2007
 *
 */

#include <arduino.h>                                                // Arduino
#include <ArduinoJson.h>                                            // JSON documents

#ifdef ESP32
    #include <getChipId.h>                                          // ESP.getChipId emulation
    #if CONFIG_IDF_TARGET_ESP32
        #include "esp32/rom/rtc.h"
    #elif CONFIG_IDF_TARGET_ESP32S2
        #include "esp32s2/rom/rtc.h"
    #elif CONFIG_IDF_TARGET_ESP32C2
        #include "esp32c2/rom/rtc.h"
    #elif CONFIG_IDF_TARGET_ESP32C3
        #include "esp32c3/rom/rtc.h"
    #elif CONFIG_IDF_TARGET_ESP32S3
        #include "esp32s3/rom/rtc.h"
    #elif CONFIG_IDF_TARGET_ESP32C6
        #include "esp32c6/rom/rtc.h"
    #elif CONFIG_IDF_TARGET_ESP32H2
        #include "esp32h2/rom/rtc.h"
    #else
        #error Target CONFIG_IDF_TARGET is not supported
    #endif
#endif

//  ---- Macros ----
#undef __FILENAME__                                                 // Deactivate standard macro, only supporting "/" as separator
#define __FILENAME__ (strrchr(__FILE__, '\\')? strrchr(__FILE__, '\\')+1 : (strrchr(__FILE__, '/')? strrchr(__FILE__, '/')+1 : __FILE__))

//          -------------------
//          ---- Variables ----
//          -------------------

//  ---- WiFi ----
#ifdef ESP32
    #include <WiFi.h>                                               // WiFi
#else
    #include <ESP8266WiFi.h>                                        // WiFi
#endif

//  ---- Log ----
#ifndef LOG_MAX_LINES
    #ifdef ESP32
        #define LOG_MAX_LINES 15
        #define LOG_LINE_LEN 100
    #endif
    #ifdef ESP8266
        #define LOG_MAX_LINES 5
        #define LOG_LINE_LEN 100
    #endif
#endif

char savedLogLines[LOG_MAX_LINES][LOG_LINE_LEN];                    // Buffer to save last log lines
uint16_t savedLogNextSlot = 0;                                      // Address of next slot to be written
uint16_t logRequestNextLog = 0;                                     // Address of next slot to be send for a /log request

//  ---- Syslog ----
#ifdef FF_TRACE_USE_SYSLOG
    #include <Syslog.h>                                             // Syslog client https://github.com/arcao/Syslog
    #include <WiFiUdp.h>
    WiFiUDP udpClient;
    Syslog syslog(udpClient, SYSLOG_PROTO_IETF);
    unsigned long lastSyslogMessageMicro = 0;                       // Date of last syslog message (microseconds)
#endif

//  ---- Asynchronous web server ----
#ifdef ESP32
    #include <AsyncTCP.h>                                           // Asynchronous TCP
#else
    #include <ESPAsyncTCP.h>                                        // Asynchronous TCP
#endif
#include <ESPAsyncWebServer.h>                                      // Asynchronous web server
#include <LittleFS.h>                                               // Flash file system
#include <littleFsEditor.h>                                         // LittleFS file system editor
AsyncWebServer webServer(80);                                       // Web server on port 80
AsyncEventSource events("/events");                                 // Event root
String lastUploadedFile = "";                                       // Name of last download file
int lastUploadStatus = 0;                                           // HTTP last error code

//  ---- Preferences ----
#define SETTINGS_FILE "/settings.json"

String ssid;                                                        // SSID of local network
String pwd;                                                         // Password of local network
String accessPointPwd;                                              // Access point password
#ifdef VERSION_FRANCAISE
    String espName = "Eclairage";                                   // Name of this module
#else
    String espName = "ModelLights";                                 // Name of this module
#endif
String hostName;                                                    // Hosst name
String serverLanguage;                                              // This server language
String syslogServer;                                                // Syslog server name or IP (can be empty)
String fileToStart;                                                 // Configuration file to start
uint16_t syslogPort;                                                // Syslog port (default to 514)
bool traceEnter = true;                                             // Trace routine enter?
bool traceDebug = true;                                             // Trace debug messages?
bool traceVerbose = false;                                          // Trace (super) verbose?
bool traceJava = false;                                             // Trace Java code?
bool traceSyslog = false;                                           // Send traces to syslog?
bool traceTable = false;                                            // Trace tables after loading?
bool enableImmediateSend = false;                                   // Send immediate flag
bool enableClear = false;                                           // Clear LED before sending
bool enableFlash = false;                                           // Use flash settings when testing colors

String ledType = "RGB";                                             // Led type R,G,B in any order
uint16_t ledCount = 100;                                            // Led count
uint16_t ledPin = D2;                                               // Led pin
uint16_t ledFrequency = 800;                                        // Led frequency: 800 or 400 KHz
uint16_t startTimeHour = 0;                                         // Simulation start hour
uint16_t startTimeMinute = 0;                                       //      and minute
uint16_t endTimeHour = 23;                                          // Simulation end hour
uint16_t endTimeMinute = 59;                                        //      and minute
float cycleTime = 10.0;                                             // Cycle duration in minutes
uint8_t globalLuminosity = 100;                                     // Global luminisity (max luminosity)
uint32_t cycleCrc32 = 0;                                            // CRC 32 of last cycle seen

//  ---- Local to this program ----
String resetCause = "";                                             // Used to save reset cause
bool sendAnUpdateFlag = false;                                      // Should we send an update?
String wifiState = "";                                              // Wifi connection state

//  ---- Serial commands ----
#ifdef SERIAL_COMMANDS
    char serialCommand[100];                                        // Buffer to save serial commands
    size_t serialCommandLen = 0;                                    // Buffer used lenght
#endif

//  ---- Lights ----
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel leds(1, D2, NEO_RGB + NEO_KHZ400);                // Values will be overwritten at starup
int simulationTime = 0;                                             // Current simulation time
int simulationStart = 0;                                            // Simulation start time
int simulationStop = 0;                                             // Simulation start time
unsigned long minuteDuration = 0;                                   // Duration of one minute when in simulation
unsigned long lightLastRun = 0;                                     // Time of last light run
bool simulationActive = false;                                      // Simulation active flag
bool ledsStarted = false;                                           // Are LEDs initialized?

//  ---- Agenda ----
const char* separator = ";";                                        // Fields separator

#ifdef VERSION_FRANCAISE
    char agendaName[] = "Agenda";                                   // Agenda text
    char roomName[] = "Pieces";                                     // Room text
    char colorName[] = "Couleurs";                                  // Color tex
    char cycleName[] = "Cycles";                                    // Cycle text
    char groupName[] = "Groupes";                                   // Group text
    char flashName[] = "Flashs";                                    // Flash text
    char agendaName2[] = "Agenda";                                  // Agenda text
    char roomName2[] = "Rooms";                                     // Room text
    char colorName2[] = "Colors";                                   // Color tex
    char cycleName2[] = "Cycles";                                   // Cycle text
    char groupName2[] = "Groups";                                   // Group text
    char flashName2[] = "Flashes";                                  // Flash text
#else
    char agendaName[] = "Agenda";                                   // Agenda text
    char roomName[] = "Rooms";                                      // Room text
    char colorName[] = "Colors";                                    // Color tex
    char cycleName[] = "Cycles";                                    // Cycle text
    char groupName[] = "Groups";                                    // Group text
    char flashName[] = "Flashes";                                   // Flash text
    char agendaName2[] = "Agenda";                                  // Agenda text
    char roomName2[] = "Pieces";                                    // Room text
    char colorName2[] = "Couleurs";                                 // Color tex
    char cycleName2[] = "Cycles";                                   // Cycle text
    char groupName2[] = "Groupes";                                  // Group text
    char flashName2[] = "Flashs";                                   // Flash text
#endif

uint16_t roomCount = 0;                                             // Count of rooms in file
uint16_t colorCount = 0;                                            // Count of colors in file
uint16_t groupCount = 0;                                            // Count of groups in file
uint16_t cycleCount = 0;                                            // Count of cycles in file
uint16_t cyclePtr = 0;                                              // Pointer into cycles
uint16_t cycleDefined = 0;                                          // Count of cycle already defined
uint16_t sequenceCount = 0;                                         // Count of flashes in file
uint16_t flashCount = 0;                                            // Count of cycles in file
uint16_t agendaCount = 0;                                           // Count of agendas in file
uint16_t agendaPreviousTime = 0;                                    // Time of previous line in agenda
uint16_t tablePtr = 0;                                              // Pointer to table currently dumped by /tables
uint16_t tableRow = 0;                                              // Row  of table currently dumped by /tables

#define ROOM_OFFSET 1                                               // Offset to add to index to get room line number
#define GROUP_OFFSET 1                                              // Offset to add to index to get group line number
#define COLOR_OFFSET 1                                              // Offset to add to index to get color line number
#define CYCLE_OFFSET 1                                              // Offset to add to index to get cycle line number
#define FLASH_OFFSET 1                                              // Offset to add to index to get flash line number
#define SEQUENCE_OFFSET 1                                           // Offset to add to index to get sequence#
#define AGENDA_OFFSET 1                                             // Offset to add to index to get agenda line number

typedef enum {
    unknownFileFormat = 0,                                          // File format in unknown
    roomFileFormat,                                                 // We're in a room file
    groupFileFormat,                                                // We're in a group file
    colorFileFormat,                                                // We're in a color file
    cycleFileFormat,                                                // We're in a cycle file
    flashFileFormat,                                                // We're in a flash file
    agendaFileFormat                                                // We're in an agenda file
} fileFormat_t;

typedef enum {                                                      // Agenda structure type
    typeRoom = 0,                                                   // Content is room
    typeGroup,                                                      // Content is group
    typeFlash,                                                      // Content is flash
    typeCycle                                                       // Content is cycle
} tableType_t;

typedef enum {                                                      // Flash Agenda structure flag type
    flashStateInactive = 0,                                         // Unknown state
    flashStarting,                                                  // Flash is starting
    flashIsOn,                                                      // Flash is on
    flashIsOff,                                                     // Flash is off
    flashinPause                                                    // In pause between cycle
} flashState_t;

struct roomTable_s {                                                // Room table
    uint32_t crc;
    uint16_t firstLed;                                              // First LED of rrroom
    uint16_t ledCount;                                              // LED count
    uint8_t intensity;                                              // Luminosity % (0-100)
};

struct groupTable_s {                                               // Group table
    uint32_t crc;
    uint16_t room;                                                  // Room
};

struct colorTable_s {                                               // Color table
    uint32_t crc;
    uint8_t r;                                                      // RGB values
    uint8_t g;
    uint8_t b;
};

struct flashTable_s {                                               // Flash table
    uint32_t crc;
    unsigned long lastRunTime;                                      // Last time this flash was run
    uint16_t waitTime;                                              // Time to wait before next sequence
    uint16_t pendingRepeats;                                        // Repeats still pending
    uint16_t roomOrGroup;                                           // Room or group
    uint16_t color;                                                 // Color
    uint16_t onMin;                                                 // On min time
    uint16_t onMax;                                                 // On max time
    uint16_t offMin;                                                // Off min time
    uint16_t offMax;                                                // Off max time
    uint16_t repeatMin;                                             // Min repeat count
    uint16_t repeatMax;                                             // Max repeat count
    uint16_t pauseMin;                                              // Min pause after one cycle
    uint16_t pauseMax;                                              // Max pause after one cycle
    flashState_t state;                                             // Flash state
    uint8_t intensity;                                              // Flash intensity
};

struct cycleTable_s {                                               // Cycle table
    uint32_t crc;
    unsigned long lastRunTime;                                      // Last time this cycle was run
    uint16_t waitTime;                                              // Time to wait before next sequence
    bool isActive;                                                  // Cycle is active flag
    uint8_t activeSequence;                                         // Active sequence #
    uint8_t sequenceCount;                                          // Sequence count
};

struct sequenceTable_s {                                            // Sequence table
    uint8_t cycle;                                                  // Index in cycle table
    uint8_t sequence;                                               // Sequence in cycle
    uint16_t roomOrGroup;                                           // Room or group index
    uint16_t color;                                                 // Color index
    uint16_t waitTime;                                              // Wait time
    uint16_t maxWaitTime;                                           // Max wait time
};

struct agendaTable_s {                                              // Agenda table
    uint16_t time;                                                  // Time (in minutes since midnight)
    uint16_t tableIndex;                                            // Table (room, flash or cycle) index
    uint16_t otherData;                                             // Color color index or active flag
    uint8_t intensity;                                              // Luminosity % (0-100)
    tableType_t tableType;                                          // tableType
};

struct previousColor_s {                                            // Previous color table
    uint32_t previousColor;                                         // Previous color
};

struct ledColor_s {                                                 // NeoPixel LED color (uint32_t version)
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
};

int agendaError = -1;                                               // Error code of last agenda analysis
char lastErrorMessage[100] = {0};                                   // Last agenda loading error message
uint16_t agendaIndex = 0;                                           // Current position in agenda
fileFormat_t fileFormat = unknownFileFormat;                        // Current header file format
char emptyChar[] = "";                                              // Empty file name
char configurationName[32] = {0};                                   // Configuration file name
int tableLineNumber = 0;                                            // Table line number
int fileLineNumber = 0;                                             // File line number
roomTable_s* roomTable = new roomTable_s[0];                        // Room table
colorTable_s* colorTable = new colorTable_s[0];                     // Color table
flashTable_s* flashTable = new flashTable_s[0+1];                   // Flash table
cycleTable_s* cycleTable = new cycleTable_s[0];                     // Cycle table
groupTable_s* groupTable = new groupTable_s[0];                     // Group table
sequenceTable_s* sequenceTable = new sequenceTable_s[0];            // Sequence table
agendaTable_s* agendaTable = new agendaTable_s[0];                  // Agenda table
previousColor_s* previousColor = new previousColor_s[0];            // Previous color table

//          --------------------------------------
//          ---- Function/routines definition ----
//          --------------------------------------

//  ---- WiFi ----
#ifdef ESP32
    void onWiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info);
    void onWiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
    void onWiFiStationGotIp(WiFiEvent_t event, WiFiEventInfo_t info);
#endif

#ifdef ESP8266
    WiFiEventHandler onStationModeConnectedHandler;                 // Event handler called when WiFi is connected
    WiFiEventHandler onStationModeDisconnectedHandler;              // Event handler called when WiFi is disconnected
    WiFiEventHandler onStationModeGotIPHandler;                     // Event handler called when WiFi got an IP
#endif

//  ---- Log routines ----
void saveLogMessage(const char* message);
String getLogLine(const uint16_t line, bool reversedOrder = false);
void logSetup(void);
void syslogSetup(void);

//  ---- Serial commands ----
#ifdef SERIAL_COMMANDS
    void serialLoop(void);
#endif

//  ---- Trace rountines ----

#include <FF_Trace.h>                                               // Trace module https://github.com/FlyingDomotic/FF_Trace
trace_callback(traceCallback);
void traceSetup(void);
void enterRoutine(const char* routineName);

//  ---- System routines ----

#ifdef ESP32
    String verbosePrintResetReason(int3 reason);
#endif

String getResetCause(void);

//  ---- Preferences routines ----

void dumpSettings(void);
void restartToApply(void);
bool readSettings(void);
void writeSettings(void);

//  ---- Web server routines ----

void percentDecode(char *src);
int parseUrlParams (char *queryString, char *results[][2], const int resultsMaxCt, const boolean decodeUrl);
void setupReceived(AsyncWebServerRequest *request);
void restReceived(AsyncWebServerRequest *request);
void settingsReceived(AsyncWebServerRequest *request);
void debugReceived(AsyncWebServerRequest *request);
void statusReceived(AsyncWebServerRequest *request);
void setChangedReceived(AsyncWebServerRequest *request);
void languagesReceived(AsyncWebServerRequest *request);
void configsReceived(AsyncWebServerRequest *request);
void commandReceived(AsyncWebServerRequest *request);
void logReceived(AsyncWebServerRequest *request);
void tableReceived(AsyncWebServerRequest *request);
void notFound(AsyncWebServerRequest *request);
void updateWebServerData(void);
void sendWebServerUpdate(void);
void startUpload(AsyncWebServerRequest *request);
void handleUpload(AsyncWebServerRequest *request, String fileName, size_t index, uint8_t *data,
    size_t len, bool final);

//  ---- OTA routines ----

#include <ArduinoOTA.h>                                             // OTA (network update)

void onStartOTA(void);
void onEndOTA(void);
void onErrorOTA(const ota_error_t erreur);

// --- User's routines ---

bool inString(const String candidate, const String listOfValues, const String separator = ",");
String extractItem(const String candidate, const uint16_t index, const String separator = ",");
void checkFreeBufferSpace(const char *function, const uint16_t line, const char *bufferName,
    const size_t bufferSize, const size_t bufferLen);
bool isDebugCommand(const String givenCommand);
bool restartMe = false;
void pleaseReboot(void);

//  ---- Light routines ----

bool isGroup(uint16_t roomOrGroup);
uint16_t setGroup(uint16_t group);
uint16_t getGroup(uint16_t group);
void setRoomOrGroup(uint16_t roomOrGroup, uint16_t color, char* legend=nullptr, uint8_t otherItensity=100, bool isFlash=false);
void revertRoomOrGroup(uint16_t roomOrGroup, char* legend=nullptr);
void lightSetup(void);
void lightLoop(void);
void lightParameterChanged(void);
void startLight(void);
void stopLight(void);
void sendLight(void);
void clearAllLights(void);
void formatTime(const int time, char* buffer, const size_t bufferLen, char* prefix = emptyChar);
uint8_t percent(const uint16_t value, const uint16_t percentage);
void uploadLoop(void);
void setLedAgenda(const uint16_t agendaPtr);
void getGroupAgenda(const uint16_t agendaPtr);
void setCycleAgenda(const uint16_t agendaPtr);
void setFlashAgenda(const uint16_t agendaPtr);
void activateCycle (const uint8_t cycle, const uint8_t increment=0);
void activateFlash (const uint8_t flash);
uint8_t decodeHex(const char* hexa);
uint32_t calculateCRC32(const char *data, size_t length);
void setGlobalLuminosity(uint8_t luminosity);
ledColor_s splitNeoPixelColor(uint32_t color);

//  ---- Agenda routines ----

// Parameters for a readallFile callbacks
#define READ_FILE_PARAMETERS const char* fileLineData

int loadAgenda(void);
int loadAgendaDetails(void);
int readFile(const char* readFileName, int (*callback)(READ_FILE_PARAMETERS));
int readAllHeaders(READ_FILE_PARAMETERS);
int readAllTables(READ_FILE_PARAMETERS);
bool startWith(const char* stringToTest, const char* compareWith);
int checkValueRange(const char* stringValue, const int fieldNumber, uint8_t *valueToWrite,
    const uint8_t minValue, const uint8_t maxValue, const uint8_t defaultValue);
int checkValueRange(const char* stringValue, const int fieldNumber, uint16_t *valueToWrite,
    const uint16_t minValue, const uint16_t maxValue, const uint16_t defaultValue);
int signalError(const int errorCode, const int integerValue = 0, const char* stringValue = emptyChar);
bool waitForEventsEmpty(void);
void checkAgenda(void);

//          ----------------------------
//          ---- Functions/routines ----
//          ----------------------------

//  ---- WiFi ----

// Called when wifi is connected
#ifdef ESP32
    void onWiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
#endif
#ifdef ESP8266
    void onWiFiStationConnected(WiFiEventStationModeConnected data) {
#endif
    if (traceEnter) enterRoutine(__func__);
    char buffer[100];
    #ifdef VERSION_FRANCAISE
        snprintf_P(buffer, sizeof(buffer), "Connecté à %s (%s)",
            ssid.c_str(), WiFi.localIP().toString().c_str());
    #else
        snprintf_P(buffer, sizeof(buffer), "Connected to %s (%s)",
            ssid.c_str(), WiFi.localIP().toString().c_str());
    #endif
    checkFreeBufferSpace(__func__, __LINE__, "buffer", sizeof(buffer), strlen(buffer));
    wifiState = String(buffer);
    updateWebServerData();
}

// Called when an IP is given
#ifdef ESP32
    void onWiFiStationGotIp(WiFiEvent_t event, WiFiEventInfo_t info) {
#endif
#ifdef ESP8266
    void onWiFiStationGotIp(WiFiEventStationModeGotIP data) {
#endif
    if (traceEnter) enterRoutine(__func__);
    char buffer[100];
    #ifdef VERSION_FRANCAISE
        snprintf_P(buffer, sizeof(buffer), "Connecté à %s (%s)",
            ssid.c_str(), WiFi.localIP().toString().c_str());
    #else
        snprintf_P(buffer, sizeof(buffer), "Connected to %s (%s)",
            ssid.c_str(), WiFi.localIP().toString().c_str());
    #endif
    checkFreeBufferSpace(__func__, __LINE__, "buffer", sizeof(buffer), strlen(buffer));
    wifiState = String(buffer);
    updateWebServerData();
}

// Called when WiFi is disconnected
#ifdef ESP32
    void onWiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
#endif

#ifdef ESP8266
    void onWiFiStationDisconnected(WiFiEventStationModeDisconnected data) {
#endif
    if (traceEnter) enterRoutine(__func__);
    #ifdef VERSION_FRANCAISE
        trace_info_P("Wifi déconnecté", NULL);
    #else
        trace_info_P("Wifi disconnected", NULL);
    #endif
}

//  ---- Log routines ----

// Save a message to log queue
void saveLogMessage(const char* message) {
    strncpy(savedLogLines[savedLogNextSlot++], message, LOG_LINE_LEN-1);
    if (savedLogNextSlot >= LOG_MAX_LINES) {
        savedLogNextSlot = 0;
    }
}

// Returns a log line number
String getLogLine(const uint16_t line, bool reversedOrder) {
    int16_t ptr = savedLogNextSlot;
    if (reversedOrder) {
        ptr -= line+1;
        if (ptr < 0) {
            ptr += (LOG_MAX_LINES-1);
        }
    } else {
        ptr += line+1;
        if (ptr >= LOG_MAX_LINES) {
            ptr -= (LOG_MAX_LINES-1);
        }
    }
    if (ptr >=0 && ptr < LOG_MAX_LINES) {
        return savedLogLines[ptr];
    }
    return String("");
}

// Setup part for log
void logSetup(void) {
    if (traceEnter) enterRoutine(__func__);
    // Clear all log slots
    for (uint16_t i = 0; i < LOG_MAX_LINES; i++) {
        memset(savedLogLines[i], 0, LOG_LINE_LEN);
    }
}

// Setup part of syslog
void syslogSetup(void) {
    if (traceEnter) enterRoutine(__func__);
    #ifdef FF_TRACE_USE_SYSLOG
        if (syslogServer != "") {
            syslog.server(syslogServer.c_str(), syslogPort);
        }
        syslog.defaultPriority(LOG_LOCAL0 + LOG_DEBUG);
        syslog.appName(__FILENAME__);
    #endif
}

//  ---- Serial commands ----
#ifdef SERIAL_COMMANDS
    // Manage Serial commands
    void serialLoop(void) {
        while(Serial.available()>0) {
            char c = Serial.read();
            // Check for end of line
            if (c == '\n' || c== '\r') {
                // Do we have some command?
                if (serialCommandLen) {
                    #ifdef VERSION_FRANCAISE
                        Serial.printf("Commande : >%s<\n", serialCommand);
                    #else
                        Serial.printf("Command: >%s<\n", serialCommand);
                    #endif
                    String command = serialCommand;
                    if (isDebugCommand(command)) {
                        // Command is known and already executed, do nothing
                    } else {
                        #ifdef VERSION_FRANCAISE
                            Serial.println(PSTR("Utiliser enable/disable trace/debug/enter/syslog"));
                    #else
                            Serial.println(PSTR("Use enable/disable trace/debug/enter/syslog"));
                        #endif
                    }
                }
                // Reset command
                serialCommandLen = 0;
                serialCommand[serialCommandLen] = '\0';
            } else {
                // Do we still have room in buffer?
                if (serialCommandLen < sizeof(serialCommand)) {
                    // Yes, add char
                    serialCommand[serialCommandLen++] = c;
                    serialCommand[serialCommandLen] = '\0';
                } else {
                    // Reset command
                    serialCommandLen = 0;
                    serialCommand[serialCommandLen] = '\0';
                }
            }
        }
    }
#endif

//  ---- Trace rountines ----

trace_declare();                                                    // Declare trace class

// Trace callback routine
//    _level contains severity level
//    _file: calling source file name with extension (unless FF_TRACE_NO_SOURCE_INFO is defined)
//    _line: calling source file line (unless FF_TRACE_NO_SOURCE_INFO is defined)
//    _function: calling calling source function name (unless FF_TRACE_NO_SOURCE_INFO is defined)
//    _message contains message to display/send

trace_callback(traceCallback) {
    //String messageLevel = FF_TRACE.textLevel(_level);
    if (_level != FF_TRACE_LEVEL_DEBUG || traceDebug) {             // Don't trace debug if debug flag not set
        Serial.print(FF_TRACE.textLevel(_level));
        Serial.print(": ");
        Serial.println(_message);                                   // Print message on Serial
        #ifdef SERIAL_FLUSH
            Serial.flush();
        #endif
        if (_level == FF_TRACE_LEVEL_ERROR || _level == FF_TRACE_LEVEL_WARN) {
            events.send(_message, "error");                         // Send message to destination
        } else if (_level != FF_TRACE_LEVEL_NONE) {
            if (events.count() && (events.avgPacketsWaiting() < 5)) {// If any clients connected and less than 5 packets pending
                events.send(_message, "info");                      // Send message to destination
            }
        }
        // Send trace to syslog if needed
        #ifdef FF_TRACE_USE_SYSLOG
            #define MIN_MICROS 1000
            if (syslogServer != "" && WiFi.status() == WL_CONNECTED && traceSyslog) {
                unsigned long currentMicros = micros();             // Get microseconds
                if ((currentMicros - lastSyslogMessageMicro) < MIN_MICROS) {  // Last message less than a ms
                    delayMicroseconds(MIN_MICROS - (currentMicros - lastSyslogMessageMicro)); // Wait remaining ms                            // Delay a ms to avoid overflow
                }
                syslog.deviceHostname(messageLevel.c_str());
                switch(_level) {
                    case FF_TRACE_LEVEL_ERROR:
                        syslog.log(LOG_ERR, _message);
                        break;
                    case FF_TRACE_LEVEL_WARN:
                        syslog.log(LOG_WARNING, _message);
                        break;
                    case FF_TRACE_LEVEL_INFO:
                        syslog.log(LOG_INFO, _message);
                        break;
                    default:
                        syslog.log(LOG_DEBUG, _message);
                        break;
                }
                lastSyslogMessageMicro = micros();                  // Save date of last syslog message in microseconds
            }
        #endif
        saveLogMessage(_message);                                   // Save message into circular log
    }
}

//  Trace setup code
void traceSetup(void) {
    if (traceEnter) enterRoutine(__func__);
    trace_register(&traceCallback);                                 // Register callback
    FF_TRACE.setLevel(FF_TRACE_LEVEL_VERBOSE);                      // Start with verbose trace
}

//  Trace each routine entering
void enterRoutine(const char* routineName) {
    #ifdef VERSION_FRANCAISE
        trace_info_P("Entre dans %s", routineName);
    #else
        trace_info_P("Entering %s", routineName);
    #endif
}

//  ---- System routines ----

// Return ESP32 reset reason text
#ifdef ESP32
    String verbosePrintResetReason(int reason) {
        switch ( reason) {
            case 1  : return PSTR("Vbat power on reset");break;
            case 3  : return PSTR("Software reset digital core");break;
            case 4  : return PSTR("Legacy watch dog reset digital core");break;
            case 5  : return PSTR("Deep Sleep reset digital core");break;
            case 6  : return PSTR("Reset by SLC module, reset digital core");break;
            case 7  : return PSTR("Timer Group0 Watch dog reset digital core");break;
            case 8  : return PSTR("Timer Group1 Watch dog reset digital core");break;
            case 9  : return PSTR("RTC Watch dog Reset digital core");break;
            case 10 : return PSTR("Instrusion tested to reset CPU");break;
            case 11 : return PSTR("Time Group reset CPU");break;
            case 12 : return PSTR("Software reset CPU");break;
            case 13 : return PSTR("RTC Watch dog Reset CPU");break;
            case 14 : return PSTR("for APP CPU, reseted by PRO CPU");break;
            case 15 : return PSTR("Reset when the vdd voltage is not stable");break;
            case 16 : return PSTR("RTC Watch dog reset digital core and rtc module");break;
            default : return PSTR("Can't decode reason ")+String(reason);
        }
    }
#endif

// Return ESP reset/restart cause
String getResetCause(void) {
    if (traceEnter) enterRoutine(__func__);
    #ifdef ESP32
        #ifdef VERSION_FRANCAISE
            String reason = "Raison reset : CPU#0: "+verbosePrintResetReason(rtc_get_reset_reason(0))
                +", CPU#1: "+verbosePrintResetReason(rtc_get_reset_reason(1));
        #else
            String reason = "Reset reasons: CPU#0: "+verbosePrintResetReason(rtc_get_reset_reason(0))
                +", CPU#1: "+verbosePrintResetReason(rtc_get_reset_reason(1));
        #endif
        return reason;
    #else
        struct rst_info *rtc_info = system_get_rst_info();
        // Get reset reason
        #ifdef VERSION_FRANCAISE
            String reason = PSTR("Raison reset : ") + String(rtc_info->reason, HEX)
                + PSTR(" - ") + ESP.getResetReason();
        #else
            String reason = PSTR("Reset reason: ") + String(rtc_info->reason, HEX)
                + PSTR(" - ") + ESP.getResetReason();
        #endif
        // In case of software restart, send additional info
        if (rtc_info->reason == REASON_WDT_RST
                || rtc_info->reason == REASON_EXCEPTION_RST
                || rtc_info->reason == REASON_SOFT_WDT_RST) {
            // If crashed, print exception
            if (rtc_info->reason == REASON_EXCEPTION_RST) {
                reason += PSTR(", exception (") + String(rtc_info->exccause)+PSTR("):");
            }
            reason += PSTR(" epc1=0x") + String(rtc_info->epc1, HEX)
                    + PSTR(", epc2=0x") + String(rtc_info->epc2, HEX)
                    + PSTR(", epc3=0x") + String(rtc_info->epc3, HEX)
                    + PSTR(", excvaddr=0x") + String(rtc_info->excvaddr, HEX)
                    + PSTR(", depc=0x") + String(rtc_info->depc, HEX);
        }
        return reason;
    #endif
}

//  ---- Preferences routines ----

// Dumps all settings on screen
void dumpSettings(void) {
    trace_info_P("ssid = %s", ssid.c_str());
    trace_info_P("pwd = %s", pwd.c_str());
    trace_info_P("accessPointPwd = %s", accessPointPwd.c_str());
    trace_info_P("name = %s", espName.c_str());
    trace_info_P("traceEnter = %s", traceEnter? "true" : "false");
    trace_info_P("traceDebug = %s", traceDebug? "true" : "false");
    trace_info_P("traceVerbose = %s", traceVerbose? "true" : "false");
    trace_info_P("traceJava = %s", traceJava? "true" : "false");
    trace_info_P("traceSyslog = %s", traceSyslog? "true" : "false");
    trace_info_P("traceTable = %s", traceTable? "true" : "false");
    trace_info_P("serverLanguage = %s", serverLanguage.c_str());
    trace_info_P("syslogServer = %s", syslogServer.c_str());
    trace_info_P("syslogPort = %d", syslogPort);
    trace_info_P("ledCount = %d", ledCount);
    trace_info_P("ledPin = %d", ledPin);
    trace_info_P("ledType = %s", ledType.c_str());
    trace_info_P("ledFrequency = %d", ledFrequency);
    trace_info_P("startTime = %02d:%02d", startTimeHour, startTimeMinute);
    trace_info_P("endTime = %02d:%02d", endTimeHour, endTimeMinute);
    trace_info_P("cycleTime = %.1f", cycleTime);
    trace_info_P("enableImmediateSend = %s", enableImmediateSend? "true" : "false");
    trace_info_P("enableClear = %s", enableClear? "true" : "false");
    trace_info_P("enableFlash = %s", enableFlash? "true" : "false");
    trace_info_P("fileToStart = %s", fileToStart.c_str());
}

// Restart to apply message
void restartToApply(void) {
    #ifdef VERSION_FRANCAISE
        trace_info_P("*** Relancer l'ESP pour prise en compte ***", NULL);
    #else
        trace_info_P("*** Restart ESP to apply changes ***", NULL);
    #endif
}
// Read settings
bool readSettings(void) {
    if (traceEnter) enterRoutine(__func__);
    File settingsFile = LittleFS.open(SETTINGS_FILE, "r");          // Open settings file
    if (!settingsFile) {                                            // Error opening?
        #ifdef VERSION_FRANCAISE
            trace_error_P("Ne peut ouvrir %s", SETTINGS_FILE);
        #else
            trace_error_P("Failed to %s", SETTINGS_FILE);
        #endif
        return false;
    }

    JsonDocument settings;
    auto error = deserializeJson(settings, settingsFile);           // Read settings
    settingsFile.close();                                           // Close file
    if (error) {                                                    // Error reading JSON?
        #ifdef VERSION_FRANCAISE
            trace_error_P("Ne peut décoder %s", SETTINGS_FILE);
        #else
            trace_error_P("Failed to parse %s", SETTINGS_FILE);
        #endif
        return false;
    }

    // Load all settings into corresponding variables
    traceEnter = settings["traceEnter"].as<bool>();
    traceDebug = settings["traceDebug"].as<bool>();
    traceVerbose = settings["traceVerbose"].as<bool>();
    traceJava = settings["traceJava"].as<bool>();
    traceSyslog = settings["traceSyslog"].as<bool>();
    traceTable = settings["traceTable"].as<bool>();
    ssid = settings["ssid"].as<String>();
    pwd = settings["pwd"].as<String>();
    accessPointPwd = settings["accessPointPwd"].as<String>();
    espName = settings["name"].as<String>();
    serverLanguage = settings["serverLanguage"].as<String>();
    syslogServer = settings["syslogServer"].as<String>();
    syslogPort = settings["syslogPort"].as<uint16_t>();
    ledCount = settings["ledCount"].as<uint16_t>();
    ledPin = settings["ledPin"].as<uint16_t>();
    ledType = settings["ledType"].as<String>();
    ledFrequency = settings["ledFrequency"].as<uint16_t>();
    startTimeHour = settings["startTimeHour"].as<uint16_t>();
    startTimeMinute = settings["startTimeMinute"].as<uint16_t>();
    endTimeHour = settings["endTimeHour"].as<uint16_t>();
    endTimeMinute = settings["endTimeMinute"].as<uint16_t>();
    cycleTime = settings["cycleTime"].as<float>();
    enableImmediateSend = settings["enableImmediateSend"].as<bool>();
    enableClear = settings["enableClear"].as<bool>();
    globalLuminosity = settings["globalLuminosity"].as<uint8_t>();
    fileToStart = settings["fileToStart"].as<String>();

    // Use syslog port default value if needed
    if (syslogPort == 0) {
        syslogPort = 514;
    }

    // Set default for ledType and ledFrequency if needed
    if (ledType == "null") {
        ledType = "RGB";
    }

    if (!ledFrequency) {
        ledFrequency = 400;
    }

    // Dump settings on screen
    dumpSettings();
    return true;
}

// Write settings
void writeSettings(void) {
    if (traceEnter) enterRoutine(__func__);
    JsonDocument settings;

    // Load settings in JSON
    settings["ssid"] = ssid.c_str();
    settings["pwd"] = pwd.c_str();
    settings["accessPointPwd"] = accessPointPwd.c_str();
    settings["name"] = espName.c_str();
    settings["traceEnter"] = traceEnter;
    settings["traceDebug"] = traceDebug;
    settings["traceVerbose"] = traceVerbose;
    settings["traceJava"] = traceJava;
    settings["traceSyslog"] = traceSyslog;
    settings["traceTable"] = traceTable;
    settings["serverLanguage"] = serverLanguage.c_str();
    settings["ledCount"] = ledCount;
    settings["ledPin"] = ledPin;
    settings["ledType"] = ledType.c_str();
    settings["ledFrequency"] = ledFrequency;
    settings["syslogServer"] = syslogServer.c_str();
    settings["syslogPort"] = syslogPort;
    settings["startTimeHour"] = startTimeHour;
    settings["startTimeMinute"] = startTimeMinute;
    settings["endTimeHour"] = endTimeHour;
    settings["endTimeMinute"] = endTimeMinute;
    settings["cycleTime"] = cycleTime;
    settings["globalLuminosity"] = globalLuminosity;
    settings["enableImmediateSend"] = enableImmediateSend;
    settings["enableClear"] = enableClear;
    settings["fileToStart"] = fileToStart.c_str();

    File settingsFile = LittleFS.open(SETTINGS_FILE, "w");          // Open settings file
    if (!settingsFile) {                                            // Error opening?
        #ifdef VERSION_FRANCAISE
            trace_error_P("Ne peut ouvrir %s en écriture", SETTINGS_FILE);
        #else
            trace_error_P("Can't open %s for write", SETTINGS_FILE);
        #endif
        return;
    }

    uint16_t bytes = serializeJsonPretty(settings, settingsFile);   // Write JSON structure to file
    if (!bytes) {                                                   // Error writting?
        #ifdef VERSION_FRANCAISE
            trace_error_P("Ne peut écrire %s", SETTINGS_FILE);
        #else
            trace_error_P("Can't write %s", SETTINGS_FILE);
        #endif
    }
    settingsFile.flush();                                           // Flush file
    settingsFile.close();                                           // Close it
    #ifdef VERSION_FRANCAISE
        trace_debug_P("Envoi settings", NULL);
    #else
        trace_debug_P("Sending settings event", NULL);
    #endif
    events.send("Ok", "settings");                                  // Send a "settings" (changed) event
}

//  ---- Web server routines ----

//  Perform URL percent decoding
void percentDecode(char *src) {
    char *dst = src;
    while (*src) {
        if (*src == '+') {
            src++;
            *dst++ = ' ';
        } else if (*src == '%') {
            // handle percent escape
            *dst = '\0';
            src++;
            if (*src >= '0' && *src <= '9') {*dst = *src++ - '0';}
            else if (*src >= 'A' && *src <= 'F') {*dst = 10 + *src++ - 'A';}
            else if (*src >= 'a' && *src <= 'f') {*dst = 10 + *src++ - 'a';}
            *dst <<= 4;
            if (*src >= '0' && *src <= '9') {*dst |= *src++ - '0';}
            else if (*src >= 'A' && *src <= 'F') {*dst |= 10 + *src++ - 'A';}
            else if (*src >= 'a' && *src <= 'f') {*dst |= 10 + *src++ - 'a';}
            dst++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

//  Parse an URL parameters list and return each parameter and value in a given table
int parseUrlParams (char *queryString, char *results[][2], const int resultsMaxCt, const boolean decodeUrl) {
    int ct = 0;

    while (queryString && *queryString && ct < resultsMaxCt) {
    results[ct][0] = strsep(&queryString, "&");
    results[ct][1] = strchr(results[ct][0], '=');
    if (*results[ct][1]) *results[ct][1]++ = '\0';
    if (decodeUrl) {
        percentDecode(results[ct][0]);
        percentDecode(results[ct][1]);
    }
    ct++;
    }
    return ct;
}

// Called when /setup is received
void setupReceived(AsyncWebServerRequest *request) {
    if (traceEnter) enterRoutine(__func__);
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "setup.htm", "text/html");
    request->send(response);                                        // Send setup.htm
}

// Called when /rest is received
void restReceived (AsyncWebServerRequest *request) {
    if (traceEnter) enterRoutine(__func__);
    if (request->url() == "/rest/restart") {
        request->send(200, "text/plain", "Restarting...");
        restartMe = true;
        return;
    }
}

// Called when /settings is received
void settingsReceived(AsyncWebServerRequest *request) {
    if (traceEnter) enterRoutine(__func__);
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, SETTINGS_FILE, "application/json");
    request->send(response);                                        // Send settings.json
}

// Called when /debug is received
void debugReceived(AsyncWebServerRequest *request) {
    if (traceEnter) enterRoutine(__func__);
    // Send a json document with interresting variables
    JsonDocument answer;
    answer["version"] = VERSION;
    answer["wifiState"] = wifiState.c_str();
    answer["traceEnter"] = traceEnter;
    answer["traceDebug"] = traceDebug;
    answer["traceVerbose"] = traceVerbose;
    answer["traceJava"] = traceJava;
    answer["startTimeHour"] = startTimeHour;
    answer["startTimeMinute"] = startTimeMinute;
    answer["endTimeHour"] = endTimeHour;
    answer["endTimeMinute"] = endTimeMinute;
    answer["cycleTime"] = cycleTime;
    answer["globalLuminosity"] = globalLuminosity;
    answer["simulationTime"] = simulationTime;
    answer["simulationStart"] = simulationStart;
    answer["simulationStop"] = simulationStop;
    answer["minuteDuration"] = minuteDuration;
    answer["simulationActive"] = simulationActive;
    answer["agendaError"] = agendaError;
    answer["agendaIndex"] = agendaIndex;
    #ifdef ESP32
        answer["freeMemory"] = ESP.getFreeHeap();
        answer["largestChunk"] = ESP.getMaxAllocHeap();
        answer["memoryLowMark"] = ESP.getMinFreeHeap();
    #else
        answer["freeMemory"] = ESP.getFreeHeap();
        answer["largestChunk"] = ESP.getMaxFreeBlockSize();
    #endif
    String  buffer;
    serializeJsonPretty(answer, buffer);
    request->send(200, "application/json", buffer);
}

// Called when /upload data is received
void startUpload(AsyncWebServerRequest *request) {
    // Load file name
    if(request->hasParam("file", true, true)) {
        const AsyncWebParameter* fileParameter = request->getParam("file", true, true);
        if (fileParameter) {
            String fileName = fileParameter->value();
            // Do we have a .txt file?
            if (fileName.endsWith(".txt")) {
                lastUploadStatus = 200;
                request->send(lastUploadStatus, "Starting upload");
                return;
            }
            // File name is not supported
            lastUploadStatus = 412;
            request->send(lastUploadStatus, "Unsupported file name");
            #ifdef VERSION_FRANCAISE
                trace_error_P("Nom de fichier %s illégal", fileParameter->value().c_str());
            #else
                trace_error_P("Illegal file name %s", fileParameter->value().c_str());
            #endif
            return;
        }
    }
    // File parameter not found in POST request
    lastUploadStatus = 400;
    request->send(lastUploadStatus, "No file parameter");
}

// Called when a /upload data
void handleUpload(AsyncWebServerRequest *request, String fileName, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        request->_tempFile = LittleFS.open("/tmpfile.tmp", "w");
    }

    if (len) {
        // stream the incoming chunk to the opened file
        request->_tempFile.write(data, len);
    }

    if (final) {
        // close the file handle as the upload is now done
        request->_tempFile.close();
        Serial.printf("End of upload %d %s\n", lastUploadStatus, fileName.c_str());
        request->send(200, "Upload ok");
        // Delete existing file
        if (LittleFS.exists(fileName)) {
            LittleFS.remove(fileName);
        }
        // Rename file
        LittleFS.rename("/tmpfile.tmp", fileName);
        lastUploadedFile = fileName;
    }
}

// Called when a /status click is received
void statusReceived(AsyncWebServerRequest *request) {
    if (traceEnter) enterRoutine(__func__);
    // Send a json document with data correponding to current status
    JsonDocument answer;
    char buffer[512];
    formatTime(simulationTime, buffer, sizeof(buffer));
    answer["simulationTime"] = buffer;
    formatTime(simulationStart, buffer, sizeof(buffer));
    answer["simulationStart"] = buffer;
    formatTime(simulationStop, buffer, sizeof(buffer));
    answer["simulationStop"] = buffer;
    answer["cycleTime"] = cycleTime;
    answer["globalLuminosity"] = globalLuminosity;
    answer["agendaError"] = agendaError;
    answer["roomCount"] = roomCount;
    answer["groupCount"] = groupCount;
    answer["flashCount"] = cycleCount;
    answer["colorCount"] = colorCount;
    answer["cycleCount"] = cycleCount;
    answer["sequenceCount"] = sequenceCount;
    answer["agendaCount"] = agendaCount;
    answer["dataSize"] = ((roomCount+1) * sizeof(roomTable_s)) +
        ((colorCount+1) * sizeof(colorTable_s)) +
        ((flashCount+1) * sizeof(flashTable_s)) +
        (cycleCount * sizeof(cycleTable_s)) +
        (groupCount * sizeof(groupTable_s)) +
        (sequenceCount * sizeof(sequenceTable_s)) +
        (agendaCount * sizeof(agendaTable_s));
    answer["saveSize"] = ledCount * sizeof(previousColor_s);
    #ifdef ESP32
        answer["freeMemory"] = ESP.getFreeHeap();
        answer["largestChunk"] = ESP.getMaxAllocHeap();
        answer["memoryLowMark"] = ESP.getMinFreeHeap();
    #else
        answer["freeMemory"] = ESP.getFreeHeap();
        answer["largestChunk"] = ESP.getMaxFreeBlockSize();
    #endif
    serializeJsonPretty(answer, buffer, sizeof(buffer));
    checkFreeBufferSpace(__func__, __LINE__, "buffer", sizeof(buffer), strlen(buffer));
    request->send(200, "application/json", buffer);
}

// Called when /changed/<variable name>/<variable value> is received
void setChangedReceived(AsyncWebServerRequest *request) {
    if (traceEnter) enterRoutine(__func__);
    bool dontWriteSettings = false;
    String position = request->url().substring(1);
    #ifdef VERSION_FRANCAISE
        trace_debug_P("Reçu %s", position.c_str());
    #else
        trace_debug_P("Received %s", position.c_str());
    #endif
    int separator1 = position.indexOf("/");                         // Position of first "/"
    if (separator1 >= 0) {
        int separator2 = position.indexOf("/", separator1+1);       // Position of second "/"
        if (separator2 > 0) {
            // Extract field name and value
            String fieldName = position.substring(separator1+1, separator2);
            String fieldValue = position.substring(separator2+1);
            // Check against known field names and set value accordingly
            if (fieldName == "traceEnter") {
                traceEnter = (fieldValue == "true");
            } else if (fieldName == "traceDebug") {
                traceDebug = (fieldValue == "true");
            } else if (fieldName == "traceVerbose") {
                traceVerbose = (fieldValue == "true");
            } else if (fieldName == "traceJava") {
                traceJava = (fieldValue == "true");
            } else if (fieldName == "traceSyslog") {
                traceSyslog = (fieldValue == "true");
            } else if (fieldName == "traceTable") {
                traceTable = (fieldValue == "true");
            } else if (fieldName == "ssid") {
                ssid = fieldValue;
                restartToApply();
            } else if (fieldName == "pwd") {
                restartToApply();
                pwd = fieldValue;
            } else if (fieldName == "accessPointPwd") {
                restartToApply();
                accessPointPwd = fieldValue;
            } else if (fieldName == "name") {
                restartToApply();
                espName = fieldValue;
            } else if (fieldName == "serverLanguage") {
                serverLanguage = fieldValue;
            } else if (fieldName == "syslogServer") {
                #ifdef FF_TRACE_USE_SYSLOG
                    if (fieldValue != "") {
                        if (syslogServer != fieldValue) {
                            syslog.server(fieldValue.c_str(), syslogPort);
                        }
                    }
                #endif
                syslogServer = fieldValue;
            } else if (fieldName == "syslogPort") {
                #ifdef FF_TRACE_USE_SYSLOG
                    if (fieldValue.toInt() > 0 && syslogServer != "") {
                        if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 65535) {
                            if (syslogPort != fieldValue.toInt()) {
                                syslog.server(syslogServer.c_str(), fieldValue.toInt());
                            }
                        }
                    }
                #endif
                syslogPort = fieldValue.toInt();
            } else if (fieldName == "ledCount") {
                if (atol(fieldValue.c_str()) >= 1 && atol(fieldValue.c_str()) <= 2048) {
                    ledCount = atol(fieldValue.c_str());
                    pleaseReboot();
                }
            } else if (fieldName == "ledPin") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 16) {
                    ledPin = atol(fieldValue.c_str());
                    pleaseReboot();
                }
            } else if (fieldName == "ledType") {
                ledType = fieldValue;
                    pleaseReboot();
            } else if (fieldName == "ledFrequency") {
                ledFrequency = atol(fieldValue.c_str());
                    pleaseReboot();
            } else if (fieldName == "startTimeHour") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 23) {
                    startTimeHour = atol(fieldValue.c_str());
                    lightParameterChanged();
                }
            } else if (fieldName == "startTimeMinute") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 59) {
                    startTimeMinute = atol(fieldValue.c_str());
                    lightParameterChanged();
                }
            } else if (fieldName == "endTimeHour") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 23) {
                    endTimeHour = atol(fieldValue.c_str());
                    lightParameterChanged();
                }
            } else if (fieldName == "endTimeMinute") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 59) {
                    endTimeMinute = atol(fieldValue.c_str());
                    lightParameterChanged();
                }
            } else if (fieldName == "cycleTime") {
                cycleTime = atof(fieldValue.c_str());
            } else if (fieldName == "globalLuminosity") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 100) {
                    globalLuminosity = atoi(fieldValue.c_str());
                    if (ledsStarted) setGlobalLuminosity(globalLuminosity);
                }
            } else if (fieldName == "firstLed") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 2048) {
                    roomTable[roomCount].firstLed = atol(fieldValue.c_str());
                    if (enableImmediateSend) sendLight();
                }
            } else if (fieldName == "ledSize") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 2048) {
                    roomTable[roomCount].ledCount = atol(fieldValue.c_str());
                    if (enableImmediateSend) sendLight();
                }
            } else if (fieldName == "redLevel") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 255) {
                    colorTable[colorCount].r = atol(fieldValue.c_str());
                    if (enableImmediateSend) sendLight();
                }
            } else if (fieldName == "greenLevel") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 255) {
                    colorTable[colorCount].g = atol(fieldValue.c_str());
                    if (enableImmediateSend) sendLight();
                }
            } else if (fieldName == "blueLevel") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 255) {
                    colorTable[colorCount].b = atol(fieldValue.c_str());
                    if (enableImmediateSend) sendLight();
                }
            } else if (fieldName == "luminosity") {
                if (atol(fieldValue.c_str()) >= 0 && atol(fieldValue.c_str()) <= 100) {
                    roomTable[roomCount].intensity = atol(fieldValue.c_str());
                    if (enableImmediateSend) sendLight();
                }
            } else if (fieldName == "flashOnMin") {
                flashTable[flashCount].onMin = atol(fieldValue.c_str());
                if (enableImmediateSend) sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "flashOnMax") {
                flashTable[flashCount].onMax = atol(fieldValue.c_str());
                if (enableImmediateSend) sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "flashOffMin") {
                flashTable[flashCount].offMin = atol(fieldValue.c_str());
                if (enableImmediateSend) sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "flashOffMax") {
                flashTable[flashCount].offMax = atol(fieldValue.c_str());
                if (enableImmediateSend) sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "flashRepeatMin") {
                flashTable[flashCount].repeatMin = atol(fieldValue.c_str());
                if (enableImmediateSend) sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "flashRepeatMax") {
                flashTable[flashCount].repeatMax = atol(fieldValue.c_str());
                if (enableImmediateSend) sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "flashPauseMin") {
                flashTable[flashCount].pauseMin = atol(fieldValue.c_str());
                if (enableImmediateSend) sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "flashPauseMax") {
                flashTable[flashCount].pauseMax = atol(fieldValue.c_str());
                if (enableImmediateSend) sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "enableImmediateSend") {
                enableImmediateSend = (fieldValue == "true");
            } else if (fieldName == "enableClear") {
                enableClear = (fieldValue == "true");
            } else if (fieldName == "enableFlash") {
                enableFlash = (fieldValue == "true");
                if (enableImmediateSend) sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "colorPicker") {
                colorTable[colorCount].r = decodeHex(fieldValue.substring(0, 2).c_str());
                colorTable[colorCount].g = decodeHex(fieldValue.substring(2, 4).c_str());
                colorTable[colorCount].b = decodeHex(fieldValue.substring(4, 6).c_str());
                // Change color if requested
                if (enableImmediateSend) sendLight();
                events.send(fieldName, "serialized");
                dontWriteSettings = true;
            } else if (fieldName == "fileToStart") {
                if (fieldValue != fileToStart) {
                    fileToStart = fieldValue;
                    writeSettings();
                    loadAgenda();
                }
            } else if (fieldName == "start") {
                startLight();
                dontWriteSettings = true;
            } else if (fieldName == "stop") {
                stopLight();
                dontWriteSettings = true;
            } else if (fieldName == "send") {
                sendLight();
                dontWriteSettings = true;
            } else if (fieldName == "restart") {
                restartMe = true;
            } else {
                // This is not a known field
                #ifdef VERSION_FRANCAISE
                    trace_error_P("Donnée >%s< inconnue, valeur >%s<", fieldName.c_str(), fieldValue.c_str());
                #else
                    trace_error_P("Can't set field >%s<, value >%s<", fieldName.c_str(), fieldValue.c_str());
                #endif
                char msg[70];                                       // Buffer for message
                snprintf_P(msg, sizeof(msg),PSTR("<status>Bad field name %s</status>"), fieldName.c_str());
                checkFreeBufferSpace(__func__, __LINE__, "msg", sizeof(msg), strlen(msg));
                request->send(400, "", msg);
                return;
            }
            if (!dontWriteSettings) writeSettings();
        } else {
            // This is not a known field
            #ifdef VERSION_FRANCAISE
                trace_error_P("Pas de nom de donnée", NULL);
            #else
                trace_error_P("No field name", NULL);
            #endif
            char msg[70];                                           // Buffer for message
            snprintf_P(msg, sizeof(msg),PSTR("<status>No field name</status>"));
            checkFreeBufferSpace(__func__, __LINE__, "msg", sizeof(msg), strlen(msg));
            request->send(400, "", msg);
            return;
        }
    }
    request->send(200, "", "<status>Ok</status>");
}

// Called when /languages command is received
void languagesReceived(AsyncWebServerRequest *request){
    if (traceEnter) enterRoutine(__func__);
    String path = "/";
#ifdef ESP32
    File dir = LittleFS.open(path);
#else
    Dir dir = LittleFS.openDir(path);
    path = String();
#endif
    String output = "[";
#ifdef ESP32
    File entry = dir.openNextFile();
    while(entry){
#else
    while(dir.next()){
        fs::File entry = dir.openFile("r");
#endif
        String fileName = String(entry.name());
        if (fileName.startsWith("lang_")) {
            #ifdef ESP32
                fileName = path + fileName;
            #endif
            File languageFile = LittleFS.open(fileName, "r");       // Open language file
            if (languageFile) {                                     // Open ok?
                JsonDocument jsonData;
                auto error = deserializeJson(jsonData, languageFile); // Read settings
                languageFile.close();                               // Close file
                if (!error) {                                       // Reading JSON ok?
                    if (output != "[") output += ',';
                    output += "{\"code\":\"";
                    output += jsonData["code"].as<String>();
                    output += "\",\"text\":\"";
                    output += jsonData["text"].as<String>();
                    output += "\"}";
                } else {
                    #ifdef VERSION_FRANCAISE
                        trace_error_P("Ne peut decoder %s", fileName.c_str());
                    #else
                        trace_error_P("Can't decode %s", fileName.c_str());
                    #endif
                }
            } else {
                #ifdef VERSION_FRANCAISE
                    trace_error_P("Ne peut ouvrir %s", fileName.c_str());
                #else
                    trace_error_P("Can't open %s", fileName.c_str());
                #endif
            }
        }
        #ifdef ESP32
            entry = dir.openNextFile();
        #else
            entry.close();
        #endif
        }
#ifdef ESP32
    dir.close();
#endif
    output += "]";
    request->send(200, "application/json", output);
}

// Called when /configs command is received
void configsReceived(AsyncWebServerRequest *request){
    if (traceEnter) enterRoutine(__func__);
    String path = "/";
#ifdef ESP32
    File dir = LittleFS.open(path);
#else
    Dir dir = LittleFS.openDir(path);
    path = String();
#endif
    String output = "[";
#ifdef ESP32
    File entry = dir.openNextFile();
    while(entry){
#else
    while(dir.next()){
        fs::File entry = dir.openFile("r");
#endif
        String fileName = String(entry.name());
        if (fileName.endsWith(".txt")) {
            #ifdef ESP32
                fileName = path + fileName;
            #endif
            if (output != "[") output += ',';
            output += "\"" + fileName + "\"";
        }
        #ifdef ESP32
            entry = dir.openNextFile();
        #else
            entry.close();
        #endif
        }
#ifdef ESP32
    dir.close();
#endif
    output += "]";
    request->send(200, "application/json", output);
}

// Called when /command/<command name>/<commandValue> is received
void commandReceived(AsyncWebServerRequest *request) {
    if (traceEnter) enterRoutine(__func__);
    String position = request->url().substring(1);
    #ifdef VERSION_FRANCAISE
        trace_debug_P("Reçu %s", position.c_str());
    #else
        trace_debug_P("Received %s", position.c_str());
    #endif
    String commandName = "";
    String commandValue = "";
    int separator1 = position.indexOf("/");                         // Position of first "/"
    if (separator1 >= 0) {
        int separator2 = position.indexOf("/", separator1+1);       // Position of second "/"
        if (separator2 > 0) {
            // Extract field name and value
            commandName = position.substring(separator1+1, separator2);
            commandValue = position.substring(separator2+1);
        } else {
            commandName = position.substring(separator1+1);
        }
        // Check against known command names
        if (commandName == "xxx") {
        } else {
            // This is not a known field
            #ifdef VERSION_FRANCAISE
                trace_error_P("Commande >%s< inconnue", commandName.c_str());
            #else
                trace_error_P("Can't execute command >%s<", commandName.c_str());
            #endif
            char msg[70];                                           // Buffer for message
            snprintf_P(msg, sizeof(msg),PSTR("<status>Bad command name %s</status>"), commandName.c_str());
            checkFreeBufferSpace(__func__, __LINE__, "msg", sizeof(msg), strlen(msg));
            request->send(400, "", msg);
            return;
        }
    } else {
        #ifdef VERSION_FRANCAISE
            trace_error_P("Pas de commande", NULL);
        #else
            trace_error_P("No command name", NULL);
        #endif
        char msg[70];                                               // Buffer for message
        snprintf_P(msg, sizeof(msg),PSTR("<status>No command name</status>"));
        checkFreeBufferSpace(__func__, __LINE__, "msg", sizeof(msg), strlen(msg));
        request->send(400, "", msg);
        return;
    }
    request->send(200, "", "<status>Ok</status>");
}

// Called when /log is received - Send saved log, line by line
void logReceived(AsyncWebServerRequest *request) {
    if (traceEnter) enterRoutine(__func__);
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain; charset=utf-8",
            [](uint8_t *logResponseBuffer, size_t maxLen, size_t index) -> size_t {
        // For all log lines
        while (logRequestNextLog < LOG_MAX_LINES) {
            // Get message
            String message = getLogLine(logRequestNextLog++);
            // If not empty
            if (message != "") {
                // Compute message len (adding a "\" at end)
                size_t chunkSize = min(message.length(), maxLen-1)+1;
                // Copy message
                memcpy(logResponseBuffer, message.c_str(), chunkSize-1);
                // Add "\n" at end
                logResponseBuffer[chunkSize-1] = '\n';
                // Return size (and message loaded)
                return chunkSize;
            }
        }
        // That's the end
        return 0;
    });
    logRequestNextLog = 0;
    request->send(response);
}

// Called when /tables is received
void tableReceived(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
            [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        if (index == 0) {                                           // Init pointers if index is null
            tablePtr = 0;
            tableRow = 0;
        }
        memset(buffer, 0, maxLen);                                  // Clear buffer
        char header[100] = {0};                                     // Clear header
        if (tablePtr ==  0) {                                       // Room table
            if (!tableRow)  {                                       // Set header
                snprintf_P(header, sizeof(header), "Row 1st Cnt Int (%s)\n", roomName);
            }
            if (roomCount) {                                        // Any room defined?
                snprintf_P((char*) buffer, maxLen, "%s%3d %3d %3d %3d\n", header, tableRow,
                    roomTable[tableRow].firstLed, roomTable[tableRow].ledCount, roomTable[tableRow].intensity);
            } else {
                snprintf_P((char*) buffer, maxLen, "%s", header);
            }
            tableRow++;                                             // Next table index
            if (tableRow >= roomCount+1) {                          // Are we at end of table?
                tablePtr++;                                         // Next table
                tableRow = 0;                                       // Clear index
            }
        } else if (tablePtr ==  1) {                                // Group table
            if (!tableRow)  {                                       // Set header
                snprintf_P(header, sizeof(header), "Row     Name Rom (%s)\n", groupName);
            }
            if (groupCount) {
                snprintf_P((char*) buffer, maxLen, "%s%3d %8x %3d\n", header, tableRow,
                    groupTable[tableRow].crc, groupTable[tableRow].room);
            } else {
                snprintf_P((char*) buffer, maxLen, "%s", header);

            }
            tableRow++;                                             // Next table index
            if (tableRow >= groupCount) {                           // Are we at end of table?
                tablePtr++;                                         // Next table
                tableRow = 0;                                       // Clear index
            }
        } else if (tablePtr ==  2) {                                // Color table
            if (!tableRow)  {                                       // Set header
                snprintf_P(header, sizeof(header), "Row   R   G   B (%s)\n", colorName);
            }
            if (colorCount) {
                snprintf_P((char*) buffer, maxLen, "%s%3d %3d %3d %3d\n", header, tableRow,
                    colorTable[tableRow].r, colorTable[tableRow].g, colorTable[tableRow].b);
            } else {
                snprintf_P((char*) buffer, maxLen, "%s", header);
            }
            tableRow++;                                             // Next table index
            if (tableRow >= colorCount+1) {                         // Are we at end of table?
                tablePtr++;                                         // Next table
                tableRow = 0;                                       // Clear index
            }
        } else if (tablePtr ==  3) {                                // Flash table
            if (!tableRow)  {                                       // Set header
                snprintf_P(header, sizeof(header), "Row Idex Col Sta  Wait  Rept OnMin OnMax OfMin OfMax RpMin RpMax Psin PsMax (%s)\n", flashName);
            }
            if (flashCount) {
                snprintf_P((char*) buffer, maxLen, "%s%3d %4d %3d %3d %5d %5d %5d %5d %5d %5d %5d %5d %5d %5d\n", header, tableRow,
                    flashTable[tableRow].roomOrGroup, flashTable[tableRow].color, flashTable[tableRow].state,
                    flashTable[tableRow].waitTime, flashTable[tableRow].pendingRepeats,
                    flashTable[tableRow].onMin, flashTable[tableRow].onMax,
                    flashTable[tableRow].offMin, flashTable[tableRow].offMax,
                    flashTable[tableRow].repeatMin, flashTable[tableRow].repeatMax,
                    flashTable[tableRow].pauseMin, flashTable[tableRow].pauseMax);
            } else {
                snprintf_P((char*) buffer, maxLen, "%s", header);
            }
            tableRow++;                                             // Next table index
            if (tableRow >= flashCount+1) {                         // Are we at end of table?
                tablePtr++;                                         // Next table
                tableRow = 0;                                       // Clear index
            }
        } else if (tablePtr ==  4) {                                // Cycle table
            if (!tableRow)  {                                       // Set header
                snprintf_P(header, sizeof(header), "Row Run Act Cnt  Wait (%s)\n", cycleName);
            }
            if (cycleCount) {
                snprintf_P((char*) buffer, maxLen, "%s%3d %3d %3d %3d %5d\n", header, tableRow,
                    cycleTable[tableRow].isActive, cycleTable[tableRow].activeSequence,
                    cycleTable[tableRow].sequenceCount, cycleTable[tableRow].waitTime);
            } else {
                snprintf_P((char*) buffer, maxLen, "%s", header);
            }
            tableRow++;                                             // Next table index
            if (tableRow >= cycleCount) {                           // Are we at end of table?
                tablePtr++;                                         // Next table
                tableRow = 0;                                       // Clear index
            }
        } else if (tablePtr ==  5) {                                // Sequence table
            if (!tableRow)  {                                       // Set header
                snprintf_P(header, sizeof(header), "Row Cyc Seq Idex Clr  Wait MaxWt (Sequence)\n");
            }
            if (sequenceCount) {
                snprintf_P((char*) buffer, maxLen, "%s%3d %3d %3d %4d %3d %5d %5d\n", header, tableRow,
                    sequenceTable[tableRow].cycle, sequenceTable[tableRow].sequence,
                    sequenceTable[tableRow].roomOrGroup, sequenceTable[tableRow].color,
                    sequenceTable[tableRow].waitTime, sequenceTable[tableRow].maxWaitTime);
            } else {
                snprintf_P((char*) buffer, maxLen, "%s", header);
            }
            tableRow++;                                             // Next table index
            if (tableRow >= sequenceCount) {                        // Are we at end of table?
                tablePtr++;                                         // Next table
                tableRow = 0;                                       // Clear index
            }
        } else if (tablePtr ==  6) {                                // Agenda table
            if (!tableRow)  {                                       // Set header
                snprintf_P(header, sizeof(header), "Row Flg Time Ind Oth Int (%s)\n", agendaName);
            }
            if (agendaCount) {
                snprintf_P((char*) buffer, maxLen, "%s%3d %3d %4d %3d %3d %3d\n", header, tableRow,
                    agendaTable[tableRow].tableType, agendaTable[tableRow].time,
                    agendaTable[tableRow].tableIndex, agendaTable[tableRow].otherData,
                    agendaTable[tableRow].intensity);
            } else {
                snprintf_P((char*) buffer, maxLen, "%s", header);
            }
            tableRow++;                                             // Next table index
            if (tableRow >= agendaCount) {                          // Are we at end of table?
                tablePtr++;                                         // Next table
                tableRow = 0;                                       // Clear index
            }
        } else {
            tablePtr = 0;                                           // Clear table
            tableRow = 0;                                           // Clear index
            return 0;                                               // Finished!
        }
      return strlen((char*) buffer);
    });
    request->send(response);
}

// Called when a request can't be mapped to existing ones
void notFound(AsyncWebServerRequest *request) {
    char msg[120];
    #ifdef VERSION_FRANCAISE
        snprintf_P(msg, sizeof(msg), PSTR("Fichier %s inconnu"), request->url().c_str());
    #else
        snprintf_P(msg, sizeof(msg), PSTR("File %s not found"), request->url().c_str());
    #endif
    trace_debug(msg);
    checkFreeBufferSpace(__func__, __LINE__, "msg", sizeof(msg), strlen(msg));
    request->send(404, "text/plain", msg);
    trace_info(msg);
}

//  ---- OTA routines ----

// Called when OTA starts
void onStartOTA(void) {
    if (traceEnter) enterRoutine(__func__);
    if (ArduinoOTA.getCommand() == U_FLASH) {                       // Program update
        #ifdef VERSION_FRANCAISE
            trace_info_P("Début MAJ firmware", NULL);
        #else
            trace_info_P("Starting firmware update", NULL);
        #endif
    } else {                                                        // File system update
        #ifdef VERSION_FRANCAISE
            trace_info_P("Début MAJ fichiers", NULL);
        #else
            trace_info_P("Starting file system update", NULL);
        #endif
        LittleFS.end();
    }
}

// Called when OTA ends
void onEndOTA(void) {
    if (traceEnter) enterRoutine(__func__);
    #ifdef VERSION_FRANCAISE
        trace_info_P("Fin de MAJ", NULL);
    #else
        trace_info_P("End of update", NULL);
    #endif
}

// Called when OTA error occurs
void onErrorOTA(const ota_error_t erreur) {
    if (traceEnter) enterRoutine(__func__);
    #ifdef VERSION_FRANCAISE
        String msg = "Erreur OTA(";
        msg += String(erreur);
        msg += ") : Erreur ";
        if (erreur == OTA_AUTH_ERROR) {
            msg += "authentification";
        } else if (erreur == OTA_BEGIN_ERROR) {
            msg += "lancement";
        } else if (erreur == OTA_CONNECT_ERROR) {
            msg += "connexion";
        } else if (erreur == OTA_RECEIVE_ERROR) {
            msg += "réception";
        } else if (erreur == OTA_END_ERROR) {
            msg += "fin";
        } else {
            msg += "inconnue !";
        }
    #else
        String msg = "OTA error(";
        msg += String(erreur);
        msg += ") : Error ";
        if (erreur == OTA_AUTH_ERROR) {
            msg += "authentication";
        } else if (erreur == OTA_BEGIN_ERROR) {
            msg += "starting";
        } else if (erreur == OTA_CONNECT_ERROR) {
            msg += "connecting";
        } else if (erreur == OTA_RECEIVE_ERROR) {
            msg += "receiving";
        } else if (erreur == OTA_END_ERROR) {
            msg += "terminating";
        } else {
            msg += "unknown !";
        }
    #endif
    trace_error(msg.c_str());
}

// --- User's routines ---

// Looks for string into a list of strings
bool inString(const String candidate, const String listOfValues, const String separator) {
    int endPosition = 0;
    int startPosition = 0;
    String allValues = listOfValues + separator;
    while (endPosition >= 0) {
        endPosition = allValues.indexOf(separator, startPosition);
        // Compare sending number with extracted one
        if (candidate.equalsIgnoreCase(allValues.substring(startPosition, endPosition))) {
            return true;
        }
        startPosition = endPosition+1;
    }
    return false;
}

// Returns part of a string, giving index and delimiter
String extractItem(const String candidate, const uint16_t index, const String separator) {
    int endPosition = 0;
    int startPosition = 0;
    int i = 0;
    String allValues = candidate + separator;
    while (endPosition >= 0) {
        endPosition = allValues.indexOf(separator, startPosition);
        if (i == index) {
            // Return part corresponding to required item
            return allValues.substring(startPosition, endPosition);
        }
        startPosition = endPosition+1;
    }
    return "";
}

// Check for remaining space into a buffer
void checkFreeBufferSpace(const char *function, const uint16_t line, const char *bufferName,
        const size_t bufferSize, const size_t bufferLen) {
    if ((bufferSize - bufferLen) < 0 || bufferSize <= 0) {
        #ifdef VERSION_FRANCAISE
            trace_error_P("Taille %d et longueur %d pour %s dans %s:%d", bufferSize, bufferLen,
                bufferName, function, line);
        #else
            trace_error_P("Invalid size %d and length %d for %s in %s:%d", bufferSize, bufferLen,
                bufferName, function, line);
        #endif
    } else {
        size_t freeSize = bufferSize - bufferLen;
        uint8_t percent = (bufferLen * 100)/ bufferSize;
        if (percent > 90) {
            #ifdef VERSION_FRANCAISE
                trace_debug_P("%s:%d: %s rempli à %d\%%, %d octets libres (taille %d, longueur %d))",
            #else
                trace_debug_P("%s:%d: %s is %d\%% full, %d bytes remaining (size %d, length %d))",
            #endif
                function, line, bufferName, percent, freeSize, bufferSize, bufferLen);
        }
    }
}

// Execute a debug command, received either by Serial or MQTT
bool isDebugCommand(const String givenCommand) {
    if (traceEnter) enterRoutine(__func__);
    String command = String(givenCommand);
    command.toLowerCase();
    // enable/disable trace/debug
    if (command == "enable debug") {
        traceDebug = true;
    } else if (command == "disable debug") {
        traceDebug = false;
    } else if (command == "enable verbose") {
        traceVerbose = true;
    } else if (command == "disable verbose") {
        traceVerbose = false;
    } else if (command == "enable enter") {
        traceEnter = true;
    } else if (command == "disable enter") {
        traceEnter = false;
    } else if (command == "enable java") {
        traceJava = true;
    } else if (command == "disable java") {
        traceJava = false;
    } else if (command == "enable syslog") {
        traceSyslog = true;
    } else if (command == "disable syslog") {
        traceSyslog = false;
    } else if (command == "enable table") {
        traceTable = true;
    } else if (command == "disable table") {
        traceTable = false;
    } else {
        return false;
    }
    return true;
}

// Update state on web server
void updateWebServerData(void) {
    // Flag update needed
    sendAnUpdateFlag = true;
}

// Send web server data to clients
void sendWebServerUpdate(void) {
    if (traceEnter) enterRoutine(__func__);
    char buffer[512];
    // Send new state to connected users
    JsonDocument data;
    data["serverName"] = espName.c_str();
    data["serverVersion"] = VERSION;
    data["wifiState"] = wifiState.c_str();
    char buffer2[20];
    if (simulationActive) {
        formatTime(simulationTime, buffer2, sizeof(buffer2));       // Format time
    } else {
        #ifdef VERSION_FRANCAISE
            formatTime(simulationTime, buffer2, sizeof(buffer2), (char*) "Arrêté à ");
        #else
            formatTime(simulationTime, buffer2, sizeof(buffer2), (char*) "Stopped at ");
        #endif
    }
    data["currentTime"] = buffer2;
    #ifdef ESP32
        data["freeMemory"] = ESP.getFreeHeap();
        data["largestChunk"] = ESP.getMaxAllocHeap();
        data["memoryLowMark"] = ESP.getMinFreeHeap();
    #else
        data["freeMemory"] = ESP.getFreeHeap();
        data["largestChunk"] = ESP.getMaxFreeBlockSize();
    #endif
    if (agendaError == -1) {
        #ifdef VERSION_FRANCAISE
            data["agendaState"] = "*** Agenda pas encore chargé ***";
        #else
            data["agendaState"] = "*** Agenda not yet loaded ***";
        #endif
    } else if (agendaError == 0) {
        #ifdef VERSION_FRANCAISE
            data["agendaState"] = fileToStart + " chargé";
        #else
            data["agendaState"] = fileToStart + " loaded";
        #endif
    } else {
        data["agendaState"] = lastErrorMessage;
    }
    serializeJson(data, buffer, sizeof(buffer));
    checkFreeBufferSpace(__func__, __LINE__, "buffer", sizeof(buffer), strlen(buffer));
    events.send(buffer, "data");
    sendAnUpdateFlag = false;
}

void pleaseReboot(void) {
    #ifdef VERSION_FRANCAISE
        trace_info_P("*** Merci de relancer le module ***", NULL);
    #else
        trace_info_P("*** Please restart module ***", NULL);
    #endif
}

//  ---- Light routines ----

// Determine if a roomOrGroup value is a group
bool isGroup(uint16_t roomOrGroup) {
    return (roomOrGroup >= 9000);
}

// Set a group as roomOrGroup
uint16_t setGroup(uint16_t group) {
    return group + 9000;
}

// Get a group as roomOrGroup
uint16_t getGroup(uint16_t group) {
    return group - 9000;
}

// Set color for room or group, optionally saving previous color
void setRoomOrGroup(uint16_t roomOrGroup, uint16_t color, char* legend, uint8_t otherIntensity, bool isFlash) {
    colorTable_s colorData = colorTable[color];                     // Load color
    if (isGroup(roomOrGroup)) {
        // Scan all groups lines for this group id
        uint16_t groupIndex = getGroup(roomOrGroup);                // Get group index
        for (int j=0; j < groupCount; j++) {
            if (groupTable[j].crc == groupTable[groupIndex].crc) {  // Are we on the same group?
                roomTable_s roomData = roomTable[groupTable[j].room];               // Load room from group
                uint8_t intensity;
                if (!roomData.intensity || !otherIntensity) {       // If one intensity is zero
                    intensity = 0;                                  // Intensity = 0
                } else {                                            // Else compute average
                    intensity = (roomData.intensity + otherIntensity+1) >> 1;
                }
                uint8_t r = percent(colorData.r, intensity);
                uint8_t g = percent(colorData.g, intensity);
                uint8_t b = percent(colorData.b, intensity);
                if (legend != nullptr) {
                    ledColor_s ledColor = splitNeoPixelColor(leds.getPixelColor(roomData.firstLed-1));
                    #ifdef VERSION_FRANCAISE
                        trace_debug_P("LED %d à %d mises à (%d, %d, %d), sauve (%d, %d, %d) %s",
                            roomData.firstLed, roomData.firstLed + roomData.ledCount-1,
                            colorData.r, colorData.g, colorData.b,
                            ledColor.r, ledColor.g, ledColor.b, legend);
                    #else
                        trace_debug_P("LED %d to %d set to (%d, %d, %d), save (%d, %d, %d) %s",
                            roomData.firstLed, roomData.firstLed + roomData.ledCount-1,
                            colorData.r, colorData.g, colorData.b,
                            ledColor.r, ledColor.g, ledColor.b, legend);
                    #endif
                }
                for (int i=roomData.firstLed; i < roomData.firstLed + roomData.ledCount; i++) {
                    if (isFlash) {
                        previousColor[i-1].previousColor = leds.getPixelColor(i-1);
                    }
                    leds.setPixelColor(i-1, leds.Color(r, g, b));
                }
            }
        }
    } else {
        roomTable_s roomData = roomTable[roomOrGroup];              // Load room
        uint8_t intensity;
        if (!roomData.intensity || !otherIntensity) {               // If one intensity is zero
            intensity = 0;                                          // Intensity = 0
        } else {                                                    // Else compute average
            intensity = (roomData.intensity + otherIntensity+1) >> 1;
        }
        uint8_t r = percent(colorData.r, intensity);
        uint8_t g = percent(colorData.g, intensity);
        uint8_t b = percent(colorData.b, intensity);
        if (legend != nullptr) {
            ledColor_s ledColor = splitNeoPixelColor(leds.getPixelColor(roomData.firstLed-1));
            #ifdef VERSION_FRANCAISE
                trace_debug_P("LED %d à %d mises à (%d, %d, %d), sauve (%d, %d, %d) %s",
                    roomData.firstLed, roomData.firstLed + roomData.ledCount-1,
                    colorData.r, colorData.g, colorData.b,
                    ledColor.r, ledColor.g, ledColor.b,
                    legend);
            #else
                trace_debug_P("LED %d to %d set to (%d, %d, %d), save (%d, %d, %d)  %s",
                    roomData.firstLed, roomData.firstLed + roomData.ledCount-1,
                    colorData.r, colorData.g, colorData.b,
                    ledColor.r, ledColor.g, ledColor.b,
                    legend);
            #endif
        }
        for (int i = roomData.firstLed; i < roomData.firstLed + roomData.ledCount; i++) {
            if (isFlash) {
                previousColor[i-1].previousColor = leds.getPixelColor(i-1);
            }
            leds.setPixelColor(i-1, leds.Color(r, g, b));
        }
    }
}

// Reset color for room or group
void revertRoomOrGroup(uint16_t roomOrGroup, char* legend) {
    if (isGroup(roomOrGroup)) {
        // Scan all groups lines for this group id
        uint16_t groupIndex = getGroup(roomOrGroup);                // Get group index
        for (int j=0; j < groupCount; j++) {
            if (groupTable[j].crc == groupTable[groupIndex].crc) {  // Are we on the same group?
                roomTable_s roomData = roomTable[groupTable[j].room];// Load room from group
                if (legend != nullptr) {
                    ledColor_s ledColor = splitNeoPixelColor(previousColor[roomData.firstLed-1].previousColor);
                    #ifdef VERSION_FRANCAISE
                        trace_debug_P("LED %d à %d remises à (%d, %d, %d) (G%d) %s",
                            roomData.firstLed, roomData.firstLed + roomData.ledCount-1,
                            ledColor.r, ledColor.g, ledColor.b, j + GROUP_OFFSET, legend);
                    #else
                        trace_debug_P("LED %d to %d restored to (%d, %d, %d) (G%d) %s",
                            roomData.firstLed, roomData.firstLed + roomData.ledCount-1,
                            ledColor.r, ledColor.g, ledColor.b, j + GROUP_OFFSET, legend);
                    #endif
                }
                for (int i=roomData.firstLed; i < roomData.firstLed + roomData.ledCount; i++) {
                    leds.setPixelColor(i-1, previousColor[i-1].previousColor);
                }
            }
        }
    } else {
        roomTable_s roomData = roomTable[roomOrGroup];              // Load room
        if (legend != nullptr) {
            ledColor_s ledColor = splitNeoPixelColor(previousColor[roomData.firstLed-1].previousColor);
            #ifdef VERSION_FRANCAISE
                trace_debug_P("LED %d à %d remises à (%d, %d, %d) %s",
                    roomData.firstLed, roomData.firstLed + roomData.ledCount-1,
                    ledColor.r, ledColor.g, ledColor.b, legend);
            #else
                trace_debug_P("LED %d to %d restored to (%d, %d, %d) %s",
                    roomData.firstLed, roomData.firstLed + roomData.ledCount-1,
                    ledColor.r, ledColor.g, ledColor.b, legend);
            #endif
        }
        for (int i = roomData.firstLed; i < roomData.firstLed + roomData.ledCount; i++) {
            leds.setPixelColor(i-1, previousColor[i-1].previousColor);
        }
    }
}

// Setup for lights
void lightSetup(void) {
    loadAgenda();
    uint16_t ribbonType = 0;
    if (ledType == "RGB") {
        ribbonType = NEO_RGB;
    } else if (ledType == "RBG") {
        ribbonType = NEO_RBG;
    } else if (ledType == "GRB") {
        ribbonType = NEO_GRB;
    } else if (ledType == "GBR") {
        ribbonType = NEO_GBR;
    } else if (ledType == "BRG") {
        ribbonType = NEO_BRG;
    } else if (ledType == "BGR") {
        ribbonType = NEO_BGR;
    } else {
        trace_error_P("Unknown led type >%s<", ledType.c_str());
        return;
    }
    if (ledFrequency == 400) {
        ribbonType += NEO_KHZ400;
    } else if (ledFrequency == 800) {
        ribbonType += NEO_KHZ800;
    } else {
        trace_error_P("Unknown led frequency '%d'", ledFrequency);
        return;
    }
    trace_debug_P("%d LED, type %s %d kHz, pin %d", ledCount, ledType.c_str(), ledFrequency, ledPin);
    leds.updateLength(ledCount);                                    // Set LED count
    leds.updateType(ribbonType);                                    // Set LED type
    leds.setPin(ledPin);                                            // Set LED pin
    leds.begin();
    ledsStarted = true;
    setGlobalLuminosity(globalLuminosity);
    startLight();                                                   // Start lights when booting
}

// Loop for lights
void lightLoop(void) {
    bool ledUpdateNeeded = false;                                   // LED update flag
    if (simulationActive && !agendaError) {
        if ((millis() - lightLastRun) >= minuteDuration) {          // Last run more than a simulated minute?
            if (events.count() && !events.avgPacketsWaiting()) {    // If any clients connected and no packets pending
                char buffer[128];
                // Send new state to connected users
                JsonDocument data;
                char buffer2[6];
                formatTime(simulationTime, buffer2, sizeof(buffer2)); // Format time
                data["currentTime"] = buffer2;
                #ifdef ESP32
                    data["freeMemory"] = ESP.getFreeHeap();
                    data["largestChunk"] = ESP.getMaxAllocHeap();
                    data["memoryLowMark"] = ESP.getMinFreeHeap();
                #else
                    data["freeMemory"] = ESP.getFreeHeap();
                    data["largestChunk"] = ESP.getMaxFreeBlockSize();
                #endif
                serializeJson(data, buffer, sizeof(buffer));
                checkFreeBufferSpace(__func__, __LINE__, "buffer", sizeof(buffer), strlen(buffer));
                events.send(buffer, "data");
            }
            // Execute actions from current position in simulation up to current time
            if (agendaIndex < agendaCount && agendaTable[agendaIndex].time <= simulationTime) {
                char buffer[6];
                formatTime(simulationTime, buffer, sizeof(buffer));
                #ifdef VERSION_FRANCAISE
                    trace_info_P("Agenda %d, maintenant %s", agendaIndex+AGENDA_OFFSET, buffer);
                #else
                    trace_info_P("Agenda %d, now %s", agendaIndex+AGENDA_OFFSET, buffer);
                #endif
                while (agendaIndex < agendaCount && agendaTable[agendaIndex].time <= simulationTime) {
                    if (agendaTable[agendaIndex].tableType == typeRoom) {
                        setLedAgenda(agendaIndex);
                    } else if (agendaTable[agendaIndex].tableType == typeGroup) {
                        setGroupAgenda(agendaIndex);
                    } else if (agendaTable[agendaIndex].tableType == typeFlash) {
                        setFlashAgenda(agendaIndex);
                    } else if (agendaTable[agendaIndex].tableType == typeCycle) {
                        setCycleAgenda(agendaIndex);
                    } else {
                        #ifdef VERSION_FRANCAISE
                            trace_error_P("Flag %d ligne %d de l'agenda inconnu", agendaTable[agendaIndex].tableType, agendaIndex + AGENDA_OFFSET);
                        #else
                            trace_error_P("Agenda flag %d line %d is unknown", agendaTable[agendaIndex].tableType, agendaIndex + AGENDA_OFFSET);
                        #endif
                    }
                    agendaIndex++;
                }
                ledUpdateNeeded = true;
            }
            simulationTime++;                                       // Simulation is one minute later
            if (simulationTime > simulationStop) {                  // After end of simulation time?
                char buffer[6], buffer2[6];
                simulationTime = simulationStart;                   // Reinit time
                formatTime(simulationStop, buffer, sizeof(buffer));
                formatTime(simulationTime, buffer2, sizeof(buffer2));
                #ifdef VERSION_FRANCAISE
                    trace_info_P("On passe de %s à %s", buffer, buffer2);
                #else
                    trace_info_P("Going from %s to %s", buffer, buffer2);
                #endif
                agendaIndex = 0;                                    // Reset agendaIndex
                // Position index to start time
                while (agendaIndex < agendaCount && agendaTable[agendaIndex].time < simulationStart) {
                    if (agendaTable[agendaIndex].tableType == typeRoom) {
                        setLedAgenda(agendaIndex);
                    } else if (agendaTable[agendaIndex].tableType == typeGroup) {
                        setGroupAgenda(agendaIndex);
                    } else if (agendaTable[agendaIndex].tableType == typeFlash) {
                        setFlashAgenda(agendaIndex);
                    } else if (agendaTable[agendaIndex].tableType == typeCycle) {
                        setCycleAgenda(agendaIndex);
                    } else {
                        #ifdef VERSION_FRANCAISE
                            trace_error_P("Flag %d ligne %d de l'agenda inconnu", agendaTable[agendaIndex].tableType, agendaIndex + AGENDA_OFFSET);
                        #else
                            trace_error_P("Agenda flag %d line %d is unknown", agendaTable[agendaIndex].tableType, agendaIndex + AGENDA_OFFSET);
                        #endif
                    }
                    agendaIndex++;
                }
                ledUpdateNeeded = true;
            }
            lightLastRun = millis();                                // Set last run time
        }
        // Scan all expired cycle events
        cycleTable_s cycleData;
        unsigned long now = millis();                               // Current time
        for (int i=0; i < cycleCount; i++) {                        // Scan all cycles
            cycleData = cycleTable[i];                              // Load this cycle
            if (cycleData.isActive && (now - cycleData.lastRunTime) > cycleData.waitTime) {
                // Current cycle expired, load next
                activateCycle(i, 1);                                // Activate cycle i after adding one to sequence
                ledUpdateNeeded = true;
            }
        }
        // Scan all expired flash events
        flashTable_s flashData;
        for (int i=0; i < flashCount; i++) {                        // Scan all flashes
            flashData = flashTable[i];                              // Load this flash
            if (flashData.state != flashStateInactive
                    && (now - flashData.lastRunTime) > flashData.waitTime) {
                // Current flash expired, load next flash step
                activateFlash(i);                                   // Activate flash i
                ledUpdateNeeded = true;
            }
        }
    }
    if (!simulationActive && enableFlash) {                         // Outside simulation, but with flash enabled ?
        // Scan all expired flash events
        flashTable_s flashData;
        int i = flashCount;
        flashData = flashTable[i];                                  // Load last flash (LED test)
        if (flashData.state != flashStateInactive
                && (millis() - flashData.lastRunTime) > flashData.waitTime) {
            // Current flash expired, load next flash step
            activateFlash(i);                                       // Activate flash i
            ledUpdateNeeded = true;
        }
    }
    if (ledUpdateNeeded) leds.show();                               // Update LED if needed
}

void activateSequence(const uint16_t sequence) {
    sequenceTable_s sequenceData = sequenceTable[sequence];

    char legend[50];
    if (traceVerbose) {
        #ifdef VERSION_FRANCAISE
            snprintf_P(legend, sizeof(legend), "pour %d ms (S%dP%dC%d)",
                sequenceData.waitTime, sequence+SEQUENCE_OFFSET,
                sequenceData.roomOrGroup+ROOM_OFFSET, sequenceData.color+COLOR_OFFSET);
        #else
            snprintf_P(legend, sizeof(legend), "for %d ms (S%dR%dC%d)",
                sequenceData.waitTime, sequence+SEQUENCE_OFFSET,
                sequenceData.roomOrGroup+ROOM_OFFSET, sequenceData.color+COLOR_OFFSET);
        #endif
    }

    setRoomOrGroup(sequenceData.roomOrGroup, sequenceData.color,
        traceVerbose? legend : nullptr, 100);
    if (sequenceData.maxWaitTime) {
        cycleTable[sequenceData.cycle].waitTime =
            random (sequenceData.waitTime, sequenceData.maxWaitTime+1); // Random wait time
    } else {
        cycleTable[sequenceData.cycle].waitTime = sequenceData.waitTime; // Load wait time for this sequence
    }
    cycleTable[sequenceData.cycle].lastRunTime = millis();          // Set start of wait
}

void activateCycle (const uint8_t cycle, const uint8_t increment) {
    cycleTable_s cycleData = cycleTable[cycle];
    uint8_t seq = cycleData.activeSequence + increment;             // Next sequence
    if (seq > cycleData.sequenceCount) seq = 0;                     // Reset pointer if needed
    cycleTable[cycle].activeSequence = seq;                         // Save new sequence
    bool sequenceFound = false;
    for (int i=0; i < sequenceCount; i++) {                         // Scan all seqeunces
        if (sequenceTable[i].cycle == cycle && sequenceTable[i].sequence == seq) {
            activateSequence(i);
            sequenceFound = true;
            break;
        }
    }
    if (!sequenceFound) {
    #ifdef VERSION_FRANCAISE
        trace_error_P("Cycle %d séquence %d inconnue, ignorée", cycle+CYCLE_OFFSET, seq+SEQUENCE_OFFSET);
    #else
        trace_error_P("Can't find cycle %d sequence %d, ignored", cycle+CYCLE_OFFSET, seq+SEQUENCE_OFFSET);
    #endif
    }
}

void activateFlash (const uint8_t flash) {
    flashTable_s flashData = flashTable[flash];                     // Load flash table
    if (flashData.state == flashStarting                            // Flash sequence starting
            || flashData.state == flashinPause) {                   // Flash ends pause
        if (flashData.repeatMax > flashData.repeatMin) {            // Max > min?
            flashData.pendingRepeats = random(flashData.repeatMin, flashData.repeatMax+1);
        } else {
            flashData.pendingRepeats = flashData.repeatMin;
        }
    }

    if (flashData.state == flashStarting                            // Flash is starting
            || flashData.state == flashinPause                      // ... ends pause
            || flashData.state == flashIsOff) {                     // ... or off
        flashData.state = flashIsOn;
        if (flashData.onMax > flashData.onMin) {                    // Max > min?
            flashData.waitTime = random(flashData.onMin, flashData.onMax+1);
        } else {
            flashData.waitTime = flashData.onMin;
        }
        char legend[50] = {0};
        if (traceDebug) {
            #ifdef VERSION_FRANCAISE
                snprintf_P(legend, sizeof(legend), "(F%dP%d)",
                    flash+FLASH_OFFSET, flashData.roomOrGroup+ROOM_OFFSET);
            #else
                snprintf_P(legend, sizeof(legend), "(F%dR%d)",
                    flash+FLASH_OFFSET, flashData.roomOrGroup+ROOM_OFFSET);
            #endif
        }
        setRoomOrGroup(flashData.roomOrGroup, flashData.color,
            traceDebug? legend : nullptr,
            flashData.intensity, true);                             // Set color for room or group, saving previous color
    } else if (flashData.state == flashIsOn) {                      // Flash is on
        if (!flashData.pendingRepeats) {                            // No more repeat pending
            flashData.state = flashinPause;
            // We should wait in off state for off time + pause time
            if (flashData.pauseMax + flashData.offMax> flashData.pauseMin + flashData.offMax) {          // Max > min?
                flashData.waitTime = random(flashData.pauseMin + flashData.offMax, flashData.pauseMax + flashData.offMax+1);
            } else {
                flashData.waitTime = flashData.pauseMin;
            }
        } else {
            if (flashData.pendingRepeats) flashData.pendingRepeats--;
            flashData.state = flashIsOff;
            if (flashData.offMax > flashData.offMin) {              // Max > min?
                flashData.waitTime = random(flashData.offMin, flashData.offMax+1);
            } else {
                flashData.waitTime = flashData.offMin;
            }
        }
        // For all LEDs in this room
        char legend[50];
        if (traceDebug) {
            #ifdef VERSION_FRANCAISE
                snprintf_P(legend, sizeof(legend), "(F%dP%d)",
                    flash+FLASH_OFFSET, flashData.roomOrGroup+ROOM_OFFSET);
            #else
                snprintf_P(legend, sizeof(legend), "(F%dR%d)",
                    flash+FLASH_OFFSET, flashData.roomOrGroup+ROOM_OFFSET);
            #endif
        }
        revertRoomOrGroup(flashData.roomOrGroup,
            traceDebug? legend : nullptr);
    }
    flashData.lastRunTime = millis();                               // Save last change date
    flashTable[flash] = flashData;                                  // Save modified data
}

// Set global luminosity
void setGlobalLuminosity(uint8_t luminosity) {
    #ifdef VERSION_FRANCAISE
        trace_debug_P("Luminosité %d", luminosity);
    #else
        trace_debug_P("Luminosity %d", luminosity);
    #endif
    leds.setBrightness(percent(255, luminosity));
}

// Split NeoPixel color in r/g/b/w
ledColor_s splitNeoPixelColor(uint32_t color) {
    ledColor_s splitColor;
    memmove(&splitColor, &color, sizeof(splitColor));
    return splitColor;
}


// Called when light parameters changed
void lightParameterChanged(void) {
    simulationStart = (startTimeHour * 60) + startTimeMinute;
    simulationStop = (endTimeHour * 60) + endTimeMinute;
    minuteDuration = cycleTime * 60000.0 / (float) (simulationStop + 1 - simulationStart);
}

// Clear (turn off) all lights
void clearAllLights(void) {
    #ifdef VERSION_FRANCAISE
        trace_info("Tout éteint");
    #else
        trace_info("Clearing all lights");
    #endif
    colorTable_s color = colorTable[0];
    for (uint16_t i = 0; i < ledCount; i++) {
        leds.setPixelColor(i, color.r, color.g, color.b);
        previousColor[i].previousColor = 0;
    }
    setGlobalLuminosity(globalLuminosity);
    leds.show();
}

// Format a time in minutes as hh:mm
void formatTime(const int time, char* buffer, const size_t bufferLen, char* prefix){
    snprintf_P(buffer, bufferLen, "%s%02d:%02d", prefix, time / 60, time - ((time / 60) *60));
}

// Activate light simulation
void startLight(void) {
    if (traceEnter) enterRoutine(__func__);
    clearAllLights();                                               // Clear all lights
    lightParameterChanged();                                        // Compute parameters
    if (agendaError) {
        #ifdef VERSION_FRANCAISE
            trace_error_P("Erreur %d chargeant %s, stoppé", agendaError, fileToStart.c_str());
        #else
            trace_error_P("Error %d loading %s, not starting", agendaError, fileToStart.c_str());
        #endif
    } else {
        simulationActive = true;                                    // Simulation is active
        #ifdef VERSION_FRANCAISE
            trace_info("Début simulation");
        #else
            trace_info("Starting simulation");
        #endif
        // Initialize all LEDS
        agendaIndex = 0;
        // Turn everything off
        clearAllLights();
        // Play one time all events
        while (agendaIndex < agendaCount) {
            if (agendaTable[agendaIndex].tableType == typeRoom) {
                setLedAgenda(agendaIndex);
            } else if (agendaTable[agendaIndex].tableType == typeGroup) {
                setGroupAgenda(agendaIndex);
            } else if (agendaTable[agendaIndex].tableType == typeFlash) {
                setFlashAgenda(agendaIndex);
            } else {
                setCycleAgenda(agendaIndex);
            }
            agendaIndex++;
        }
        agendaIndex = 0;
        // Position index to start time
        while (agendaIndex < agendaCount && agendaTable[agendaIndex].time < simulationStart) {
            if (agendaTable[agendaIndex].tableType == typeRoom) {
                setLedAgenda(agendaIndex);
            } else if (agendaTable[agendaIndex].tableType == typeGroup) {
                setGroupAgenda(agendaIndex);
            } else if (agendaTable[agendaIndex].tableType == typeFlash) {
                setFlashAgenda(agendaIndex);
            } else {
                setCycleAgenda(agendaIndex);
            }
            agendaIndex++;
        }
        leds.show();
    }
    sendAnUpdateFlag = true;
}

// Set light as specified in a given agenda line number
void setLedAgenda(const uint16_t agendaPtr) {
    agendaTable_s agendaData = agendaTable[agendaIndex];            // Get agenda data

    char legend[50];
    if (traceDebug) {
        #ifdef VERSION_FRANCAISE
            snprintf_P(legend, sizeof(legend), "(A%dP%dC%d)",
                agendaPtr+AGENDA_OFFSET, agendaData.tableIndex+ROOM_OFFSET, agendaData.otherData+COLOR_OFFSET);
        #else
            snprintf_P(legend, sizeof(legend), "(A%R%dC%d)",
                agendaPtr+AGENDA_OFFSET, agendaData.tableIndex+ROOM_OFFSET, agendaData.otherData+COLOR_OFFSET);
        #endif
    }
    setRoomOrGroup(agendaData.tableIndex, agendaData.otherData,
        traceDebug? legend : nullptr, agendaData.intensity);
}

// Set group of light as specified in a given agenda line number
void setGroupAgenda(const uint16_t agendaPtr) {
    agendaTable_s agendaData = agendaTable[agendaIndex];            // Get agenda data

    char legend[50];
    if (traceDebug) {
        #ifdef VERSION_FRANCAISE
            snprintf_P(legend, sizeof(legend), "(A%dG%dC%d)",
                agendaPtr+AGENDA_OFFSET, agendaData.tableIndex+ROOM_OFFSET, agendaData.otherData+COLOR_OFFSET);
        #else
            snprintf_P(legend, sizeof(legend), "(A%G%dC%d)",
                agendaPtr+AGENDA_OFFSET, agendaData.tableIndex+ROOM_OFFSET, agendaData.otherData+COLOR_OFFSET);
        #endif
    }
    setRoomOrGroup(agendaData.tableIndex, agendaData.otherData, 
        traceDebug? legend : nullptr, agendaData.intensity);
}

// Set cycle as specified in a given agenda line number
void setCycleAgenda(const uint16_t agendaPtr) {
    agendaTable_s agendaData = agendaTable[agendaPtr];              // Get agenda data
    cycleTable[agendaData.tableIndex].isActive = (agendaData.otherData != 0); // Set active flag
    #ifdef VERSION_FRANCAISE
        trace_info_P("Agenda %d, cycle %d %sactif",
            agendaPtr+AGENDA_OFFSET, agendaData.tableIndex+CYCLE_OFFSET,
            cycleTable[agendaData.tableIndex].isActive? "" : "in");
    #else
        trace_info_P("Agenda %d, cycle %d %sactive",
            agendaPtr+AGENDA_OFFSET, agendaData.tableIndex+CYCLE_OFFSET,
            cycleTable[agendaData.tableIndex].isActive? "" : "in");
    #endif
    if (cycleTable[agendaData.tableIndex].isActive) {
        // Activate cycle
        cycleTable[agendaData.tableIndex].activeSequence = 0;       // Reset sequence
        activateCycle(agendaData.tableIndex);                       // Resume cycle
    } else {
        // Stop cycle, turn all associated lights off
        sequenceTable_s sequenceData;
        for (int i=0; i < sequenceCount; i++) {                     // Scan all sequences
            sequenceData = sequenceTable[i];                        // Load sequence data
            if (sequenceData.color == agendaData.tableIndex) {      // Are we on the corresponding cycle?
                setRoomOrGroup(sequenceData.roomOrGroup, 0);
            }
        }
    }
}

// Set flash as specified in a given agenda line number
void setFlashAgenda(const uint16_t agendaPtr) {
    agendaTable_s agendaData = agendaTable[agendaPtr];              // Get agenda data
    uint16_t flashPtr = agendaData.tableIndex;                      // Get pointer into flash table
    flashTable_s flashData = flashTable[flashPtr];                  // Get flash data
    flashData.state = agendaData.otherData != 0? flashStarting : flashStateInactive; // Set state
    #ifdef VERSION_FRANCAISE
        trace_info_P("Agenda %d, flash %d %sactif",
            agendaPtr+AGENDA_OFFSET, flashPtr+FLASH_OFFSET, flashData.state != flashStateInactive? "" : "in");
    #else
        trace_info_P("Agenda %d, flash %d %sactive",
            agendaPtr+AGENDA_OFFSET, flashPtr+FLASH_OFFSET, flashData.state != flashStateInactive? "" : "in");
    #endif
    if (flashData.state != flashStateInactive) {
        // Activate flash
        flashData.intensity = agendaData.intensity;
        flashData.state = flashStarting;
        flashTable[flashPtr] = flashData;                           // Save modified data
        activateFlash(flashPtr);                                    // Activate flash sequence
    } else {
        flashTable[flashPtr] = flashData;                           // Save modified data
        // Stop flash, turn all associated lights off
        revertRoomOrGroup(flashData.roomOrGroup);                   // Restore led data
    }
}

// Stop light simulation
void stopLight(void) {
    simulationActive = false;                                       // Simulation is not active
    #ifdef VERSION_FRANCAISE
        trace_info("Fin simulation");
    #else
        trace_info("Stoping simulation");
    #endif
    sendAnUpdateFlag = true;
}

// Set some LED as specified on screen
void sendLight(void) {
    stopLight();
    if (enableClear) clearAllLights();
    roomTable_s roomData = roomTable[roomCount];
    colorTable_s colorData = colorTable[colorCount];
    uint8_t r = percent(colorData.r, roomData.intensity);
    uint8_t g = percent(colorData.g, roomData.intensity);
    uint8_t b = percent(colorData.b, roomData.intensity);
    #ifdef VERSION_FRANCAISE
        trace_info_P("LED %d à %d mises à (%d, %d, %d), flash %d (Utilisateur)",
            roomData.firstLed, roomData.firstLed + roomData.ledCount-1, r, g, b, enableFlash);
    #else
        trace_info_P("LED %d to %d set to (%d, %d, %d), flash %d (User)",
            roomData.firstLed, roomData.firstLed + roomData.ledCount-1, r, g, b, enableFlash);
    #endif
    if (enableFlash) {                                              // Is flash enabled?
        uint16_t flashPtr = flashCount;                             // Get pointer into flash table
        flashTable_s flashData = flashTable[flashPtr];              // Get flash data
        // Activate flash
        flashData.state = flashStarting;
        flashData.roomOrGroup = roomCount;
        flashData.color = colorCount;
        flashTable[flashPtr] = flashData;                           // Save modified data
        activateFlash(flashPtr);                                    // Activate flash sequence
    } else {                                                        // No flash, just turn lights on with right color
        for (int i = roomData.firstLed; i < roomData.firstLed + roomData.ledCount; i++) {
            leds.setPixelColor(i-1, leds.Color(r, g, b));
        }
    }
    leds.show();
}

// Compute percentage
uint8_t percent(const uint16_t value, const uint16_t percentage) {
    return (uint8_t) round(value * percentage / 100.0);
}

// Work with files just after they're uploaded
void uploadLoop(void) {
    if (lastUploadedFile != "") {
        #ifdef VERSION_FRANCAISE
            trace_info_P("Reçu %s", lastUploadedFile.c_str());
        #else
            trace_info_P("Just received %s", lastUploadedFile.c_str());
        #endif
        if (lastUploadedFile != fileToStart) {
            fileToStart = lastUploadedFile;
            writeSettings();
        }
        lastUploadedFile = "";
        loadAgenda();
    }
}

// Convert 2 hexadecimal characeters to 8 bits value
uint8_t decodeHex(const char* hexa) {
    return (uint8_t) strtol(hexa, 0, 16);
}

//  ---- Agenda routines ----
// Signal an error()
int signalError(const int errorCode, const int integerValue,  const char* stringValue) {
    memset(lastErrorMessage, 0, sizeof(lastErrorMessage));
    switch (errorCode) {
        case 100:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** Ne peut ouvrir %s ***", stringValue);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** Can't open file %s ***", stringValue);
            #endif
            break;
        case 101:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** Entête >%s< incorrecte dans %s ***",
                    stringValue, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** Bad file header >%s< in %s ***",
                    stringValue, configurationName);
            #endif
            break;
        case 102:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** Partie %s manquante ***", stringValue);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** Part missing for %s ***", stringValue);
            #endif
            break;
        case 103:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** Entête >%s< déjà définie dans %s, avant la ligne %d ***",
                    stringValue, configurationName, fileLineNumber);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** >%s< header already defined in %s, before line %d ***",
                    stringValue, configurationName, fileLineNumber);
            #endif
            break;
        case 104:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** Nombre de zones (%d) incorrect ligne %d de %s ***",
                    integerValue, fileLineNumber, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage), "*** Bad field count (%d) line %d of %s ***",
                    integerValue, fileLineNumber, configurationName);
            #endif
            break;
        case 105:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Valeur >%s< incorrecte, zone %d, ligne %d de %s ***",
                    stringValue, integerValue, fileLineNumber, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Illegal number >%s< field %d line %d of %s ***",
                    stringValue, integerValue, fileLineNumber, configurationName);
            #endif
            break;
        case 106:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Valeur >%s< hors limite, zone %d, ligne %d de %s ***",
                    stringValue, integerValue, fileLineNumber, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Out of range >%s< field %d line %d of %s ***",
                    stringValue, integerValue, fileLineNumber, configurationName);
            #endif
            break;
        case 107:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Pièce >%s< inconnue, ligne %d de %s ***", stringValue, fileLineNumber, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Room >%s< not found line %d of %s ***", stringValue, fileLineNumber, configurationName);
            #endif
            break;
        case 108:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Couleur >%s< inconnue, ligne %d de %s ***", stringValue, fileLineNumber, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Color >%s< not found line %d of %s ***", stringValue, fileLineNumber, configurationName);
            #endif
            break;
        case 109:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Heure >%s< incorrecte, zone %d, ligne %d de %s ***",
                    stringValue, integerValue, fileLineNumber, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Illegal time >%s<, field %d line %d of %s ***",
                    stringValue, integerValue, fileLineNumber, configurationName);
            #endif
            break;
        case 110:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** LED de fin %d incorrecte, ligne %d de %s ***",
                    integerValue, fileLineNumber, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "** Incorrect ending LED %d, line %d of %s ***",
                    integerValue, fileLineNumber, configurationName);
            #endif
            break;
        case 111:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Pièce ou groupe >%s< inconnu, ligne %d de %s ***", stringValue, fileLineNumber, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Room or group >%s< not found line %d of %s ***", stringValue, fileLineNumber, configurationName);
            #endif
            break;
        case 112:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Pièce, groupe, cycle ou flash >%s< inconnu, ligne %d de %s ***", stringValue, fileLineNumber, configurationName);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Room, group, cycle or flash >%s< not found line %d of %s ***", stringValue, fileLineNumber, configurationName);
            #endif
            break;
        default:
            #ifdef VERSION_FRANCAISE
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Erreur %d inconnue, fichier %s, ligne %d, entier %d, chaîne >%s< ***",
                    errorCode, configurationName, fileLineNumber, integerValue, stringValue);
            #else
                snprintf_P(lastErrorMessage, sizeof(lastErrorMessage),
                    "*** Unkown error %d, file %s, line %d, integer %d, string >%s< ***",
                    errorCode, configurationName, fileLineNumber, integerValue, stringValue);
            #endif
    }
    trace_error(lastErrorMessage);
    return errorCode;
}

// Calculate CRC32 on data
uint32_t calculateCRC32(const char *data, size_t length) {
    uint32_t crc = 0xffffffff;
    while (length--) {
        uint8_t c = *data++;
        for (uint32_t i = 0x80; i > 0; i >>= 1) {
            bool bit = crc & 0x80000000;
            if (c & i) {
                bit = !bit;
            }
            crc <<= 1;
            if (bit) {
                crc ^= 0x04c11db7;
            }
        }
    }
    return crc;
}
// Check if a string starts with another
bool startWith(const char* stringToTest, const char* compareWith) {
    return strncmp(stringToTest, compareWith, strlen(compareWith)) == 0;
}

// Convert string to uint8_t, check value range or set default
int checkValueRange(const char* stringValue, const int fieldNumber, uint8_t *valueToWrite,
        const uint8_t minValue, const uint8_t maxValue, const uint8_t defaultValue) {
    size_t stringLen = strlen(stringValue);                         // Get value length
    if (stringLen) {                                                // Not null string
        long value = 0;                                             // Extracted value
        for (int i=0; i < (int) stringLen; i++) {                   // Check each character
            if (stringValue[i] >= '0' && stringValue[i] <= '9') {   // Numeric value?
                value = (value * 10) + (stringValue[i] - '0');      // Compute new value
            } else {
                return signalError(105, fieldNumber, stringValue);
            }
        }
        if (value < minValue || value > maxValue) {
            return signalError(106, fieldNumber, stringValue);
        }
        *valueToWrite = value;
    } else {
        *valueToWrite = defaultValue;
    }
    return 0;
}

// Convert string to uint16_t, check value range or set default
int checkValueRange(const char* stringValue, const int fieldNumber, uint16_t *valueToWrite,
        const uint16_t minValue, const uint16_t maxValue, const uint16_t defaultValue) {
    size_t stringLen = strlen(stringValue);                         // Get value length
    if (stringLen) {                                                // Not null string
        long value = 0;                                             // Extracted value
        for (int i=0; i < (int) stringLen; i++) {                   // Check each character
            if (stringValue[i] >= '0' && stringValue[i] <= '9') {   // Numeric value?
                value = (value * 10) + (stringValue[i] - '0');      // Compute new value
            } else {
                return signalError(105, fieldNumber, stringValue);
            }
        }
        if (value < minValue || value > maxValue) {
            return signalError(106, fieldNumber, stringValue);
        }
        *valueToWrite = value;
    } else {
        *valueToWrite = defaultValue;
    }
    return 0;
}

// Read a file, calling a callback with each line
int readFile(const char* readFileName, int (*callback)(READ_FILE_PARAMETERS)) {
    fileLineNumber = 0;
    fileFormat = unknownFileFormat;
    strncpy(configurationName, readFileName, sizeof(configurationName));
    File fileStream = LittleFS.open(configurationName, "r");
    if (!fileStream) {
        return signalError(100, 0, configurationName);
    }
    char lineContent[128];
    int errorCode = 0;
    while (fileStream.available()) {
        memset(lineContent, 0, sizeof(lineContent));
        int lengthRead = fileStream.readBytesUntil('\n', lineContent, sizeof(lineContent));
        if (lengthRead) {
            // Ends line on <CR>, <LF> or "["
            for (int i = 0; i < lengthRead; i++) {
                if (lineContent[i] == '\n' || lineContent[i] == '\r' || lineContent[i] == '#') {
                    lineContent[i] = 0;
                    break;
                }
            }
            // Remove trailing spaces and ";"
            for(int i=strlen(lineContent)-1; i > 0; i--) {
                if (lineContent[i] != ' ' && lineContent[i] != ';') break;
                lineContent[i] = 0;
            }
            fileLineNumber++;
            // Ignore empty lines
            if (lineContent[0]) {
                errorCode = callback(lineContent);
                if (errorCode) break;
            } else {
                fileFormat = unknownFileFormat;
            }
        }
    }
    fileStream.close();
    return errorCode;
}

// Read file header callback
int readAllHeaders(READ_FILE_PARAMETERS) {
    if (fileFormat == unknownFileFormat) {                          // We're not in a table description
        if (startWith(fileLineData, roomName) || startWith(fileLineData, roomName2)) {
            if (roomCount) {
                return signalError(103, 0, roomName);
            }
            fileFormat = roomFileFormat;
        } else if (startWith(fileLineData, groupName) || startWith(fileLineData, groupName2)) {
            if (groupCount) {
                return signalError(103, 0, groupName);
            }
            fileFormat = groupFileFormat;
        } else if (startWith(fileLineData, flashName) || startWith(fileLineData, flashName2)) {
            if (flashCount) {
                return signalError(103, 0, flashName);
            }
            fileFormat = flashFileFormat;
        } else if (startWith(fileLineData, colorName) || startWith(fileLineData, colorName2)) {
            if (colorCount) {
                return signalError(103, 0, colorName);
            }
            fileFormat = colorFileFormat;
        } else if (startWith(fileLineData, cycleName) || startWith(fileLineData, cycleName2)) {
            if (cycleCount) {
                return signalError(103, 0, cycleName);
            }
            fileFormat = cycleFileFormat;
        } else if (startWith(fileLineData, "Time;") || startWith(fileLineData, "Heure;")) {
            if (agendaCount) {
                return signalError(103, 0, agendaName);
            }
            fileFormat = agendaFileFormat;
        } else {
            return signalError(101, 0, (char*) fileLineData);
        }
    } else {
        // File format is known
        if (fileFormat == roomFileFormat) roomCount++;
        else if (fileFormat == groupFileFormat) groupCount++;
        else if (fileFormat == colorFileFormat) colorCount++;
        else if (fileFormat == flashFileFormat) flashCount++;
        else if (fileFormat == agendaFileFormat) agendaCount++;
        else if (fileFormat == cycleFileFormat) {
            // Special case for cycles
            sequenceCount++;                                        // Increment sequece count
            // Copy line and split it on ";"
            char lineCopy[strlen(fileLineData)+1];                  // Copy of line
            memset(lineCopy, 0, sizeof(lineCopy));                  // Init line
            strncpy(lineCopy, fileLineData, strlen(fileLineData));  // Do copy line
            char* field = strtok(lineCopy, separator);              // Get first field
            uint32_t thisCrc = calculateCRC32(field, strlen(field));// Compute CRC32
            if (thisCrc != cycleCrc32) {                            // This is a new cycle
                cycleCount += 1;                                    // Increment cycle count
                cycleCrc32 = thisCrc;                               // Save CRC
            }
        }
    }
    return 0;
}

// Read all tables callback
int readAllTables(READ_FILE_PARAMETERS) {
    int errorCode = 0;
    if (fileFormat == unknownFileFormat) {                          // We're not in a table description
        if (startWith(fileLineData, roomName) || startWith(fileLineData, roomName2)) {
            fileFormat = roomFileFormat;
        } else if (startWith(fileLineData, groupName) || startWith(fileLineData, groupName2)) {
            fileFormat = groupFileFormat;
        } else if (startWith(fileLineData, flashName) || startWith(fileLineData, flashName2)) {
            fileFormat = flashFileFormat;
        } else if (startWith(fileLineData, colorName) || startWith(fileLineData, colorName2)) {
            fileFormat = colorFileFormat;
        } else if (startWith(fileLineData, cycleName) || startWith(fileLineData, cycleName2)) {
            fileFormat = cycleFileFormat;
        } else if (startWith(fileLineData, "Heure;") || startWith(fileLineData, "Time;")) {
            fileFormat = agendaFileFormat;
        }
        tableLineNumber = 0;                                        // Reset table line number
    } else {                                                        // We're in a table definition
        tableLineNumber++;                                          // Increment table line  number
        // Copy line and split it on ";"
        char lineCopy[strlen(fileLineData)+1];                      // Copy of line
        memset(lineCopy, 0, sizeof(lineCopy));                      // Init line
        strncpy(lineCopy, fileLineData, strlen(fileLineData));      // Do copy line
        char* field = strtok(lineCopy, separator);                  // Get first field
        int fieldCount = 0;                                         // Contain field number
        // Loop on separator
        int index = tableLineNumber -1;                             // Compute index
        int i;                                                      // Poiter into structures
        while (field != NULL) {                                     // Still to do?
            fieldCount++;                                           // Bump field number
            if (fileFormat == roomFileFormat) {
                if (fieldCount == 1) {                              // Room name
                    roomTable[index].crc = calculateCRC32(field, strlen(field));
                } else if (fieldCount == 2) {                       // First LED
                    errorCode = checkValueRange(field, fieldCount, &roomTable[index].firstLed,
                        (uint16_t) 1, (uint16_t) 2048, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 3) {                       // LED count
                    errorCode = checkValueRange(field, fieldCount, &roomTable[index].ledCount,
                        (uint16_t) 1, (uint16_t) 2048, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 4) {                       // Intensity
                    errorCode = checkValueRange(field, fieldCount, &roomTable[index].intensity,
                        (uint8_t) 0, (uint8_t) 100, (uint8_t) 100);
                    if (errorCode) return errorCode;
                }
            } else if (fileFormat == groupFileFormat) {
                if (fieldCount == 1) {                              // Group name
                    groupTable[index].crc = calculateCRC32(field, strlen(field));
                } else if (fieldCount == 2) {                       // Red
                    uint32 crc = calculateCRC32(field, strlen(field));
                    for (i = 0; i < roomCount; i++) {               // Scan all room for name
                        if (roomTable[i].crc == crc) break;         // We found it
                    }
                    if (i < roomCount) {
                        groupTable[index].room = i;
                    } else {
                        return signalError(107, 0, field);
                    }
                }
            } else if (fileFormat == colorFileFormat) {
                if (fieldCount == 1) {                              // Room name
                    colorTable[index].crc = calculateCRC32(field, strlen(field));
                } else if (fieldCount == 2) {                       // Red
                    errorCode = checkValueRange(field, fieldCount, &colorTable[index].r,
                        (uint8_t) 0, (uint8_t) 255, (uint8_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 3) {                       // Green
                    errorCode = checkValueRange(field, fieldCount, &colorTable[index].g,
                        (uint8_t) 0, (uint8_t) 255, (uint8_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 4) {                       // Blue
                    errorCode = checkValueRange(field, fieldCount, &colorTable[index].b,
                        (uint8_t) 0, (uint8_t) 255, (uint8_t) 0);
                    if (errorCode) return errorCode;
                }
            } else if (fileFormat == cycleFileFormat) {
                if (fieldCount == 1) {                              // Cycle name
                    cycleCrc32 = calculateCRC32(field, strlen(field)); // Compute CRC
                    bool cycleFound = false;
                    for (cyclePtr = 0; cyclePtr < cycleDefined; cyclePtr++) { // Scan all defined cycle for name
                        if (cycleTable[cyclePtr].crc == cycleCrc32) { // We found it
                            cycleFound = true;
                            break;
                        }
                    }
                    if (cycleFound) {                               // Cycle found?
                        cycleTable[cyclePtr].sequenceCount++;       // Increment sequence of existing cycle
                    } else {
                        cyclePtr = cycleDefined;                    // Set cycle
                        cycleTable[cyclePtr].crc = cycleCrc32;
                        cycleTable[cyclePtr].isActive = false;
                        cycleTable[cyclePtr].lastRunTime = 0;
                        cycleTable[cyclePtr].sequenceCount = 0;
                        cycleTable[cyclePtr].activeSequence = 0;
                        cycleTable[cyclePtr].waitTime = 0;
                        cycleDefined++;                             // Increment number of defined cycles
                    }
                    sequenceTable[index].cycle = cyclePtr;          // Load cycle
                    sequenceTable[index].sequence = cycleTable[cyclePtr].sequenceCount; // Extract sequence number for cycle
                } else if (fieldCount == 2) {                       // Room
                    uint32 crc = calculateCRC32(field, strlen(field));
                    for (i = 0; i < roomCount; i++) {               // Scan all room for name
                        if (roomTable[i].crc == crc) break;         // We found it
                    }
                    if (i < roomCount) {
                        sequenceTable[index].roomOrGroup = i;
                    } else {
                        for (i = 0; i <groupCount; i++) {           // Scan all groups for name
                            if (groupTable[i].crc == crc) break;    // We found it
                        }
                        if (i < groupCount) {
                            sequenceTable[index].roomOrGroup = setGroup(i);
                        } else {
                            return signalError(111, 0, field);
                        }
                    }
                } else if (fieldCount == 3) {                       // Color
                    uint32 crc = calculateCRC32(field, strlen(field));
                    for (i = 0; i < colorCount; i++) {              // Scan all color for name
                        if (colorTable[i].crc == crc) break;        // We found it
                    }
                    if (i < colorCount) {
                        sequenceTable[index].color = i;
                    } else {
                        return signalError(108, 0, field);
                    }
                } else if (fieldCount == 4) {                       // Wait time before next step
                    errorCode = checkValueRange(field, fieldCount, &sequenceTable[index].waitTime,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 5) {                       // Random part
                    errorCode = checkValueRange(field, fieldCount, &sequenceTable[index].maxWaitTime,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                }
            } else if (fileFormat == flashFileFormat) {
                if (fieldCount == 1) {                              // Room name
                    flashTable[index].crc = calculateCRC32(field, strlen(field));
                    flashTable[index].lastRunTime = 0;
                    flashTable[index].waitTime = 0;
                    flashTable[index].pendingRepeats = 0;
                    flashTable[index].state = flashStateInactive;
                } else if (fieldCount == 2) {                       // Room
                    uint32 crc = calculateCRC32(field, strlen(field));
                    for (i = 0; i < roomCount; i++) {               // Scan all rooms for name
                        if (roomTable[i].crc == crc) break;         // We found it
                    }
                    if (i < roomCount) {
                        flashTable[index].roomOrGroup = i;          // Room or group  index
                    } else {
                        for (i = 0; i <groupCount; i++) {           // Scan all groups for name
                            if (groupTable[i].crc == crc) break;    // We found it
                        }
                        if (i < groupCount) {
                            flashTable[index].roomOrGroup = setGroup(i);
                        } else {
                            return signalError(111, 0, field);
                        }
                    }
                } else if (fieldCount == 3) {                       // Color
                    uint32 crc = calculateCRC32(field, strlen(field));
                    for (i = 0; i < colorCount; i++) {              // Scan all room for name
                        if (colorTable[i].crc == crc) break;        // We found it
                    }
                    if (i < colorCount) {
                        flashTable[index].color = i;
                    } else {
                        return signalError(108, 0, field);
                    }
                } else if (fieldCount == 4) {                       // On min time
                    errorCode = checkValueRange(field, fieldCount, &flashTable[index].onMin,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 5) {                       // On max time
                    errorCode = checkValueRange(field, fieldCount, &flashTable[index].onMax,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 6) {                       // Off min time
                    errorCode = checkValueRange(field, fieldCount, &flashTable[index].offMin,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 7) {                       // Off max time
                    errorCode = checkValueRange(field, fieldCount, &flashTable[index].offMax,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 8) {                       // Repeat count min
                    errorCode = checkValueRange(field, fieldCount, &flashTable[index].repeatMin,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 9) {                       // Repeat count max
                    errorCode = checkValueRange(field, fieldCount, &flashTable[index].repeatMax,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 10) {                      // Pause between 2 cycle min
                    errorCode = checkValueRange(field, fieldCount, &flashTable[index].pauseMin,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                } else if (fieldCount == 11) {                      // Pause between 2 cycle max
                    errorCode = checkValueRange(field, fieldCount, &flashTable[index].pauseMax,
                        (uint16_t) 0, (uint16_t) 0xffff, (uint16_t) 0);
                    if (errorCode) return errorCode;
                }
            } else if (fileFormat == agendaFileFormat) {
                if (fieldCount == 1) {                              // Room name
                    char timeCopy[7];                               // Copy of time string
                    uint8_t hoursValue;                             // Hours
                    uint8_t minutesValue;                           // Minutes
                    strncpy(timeCopy, field, sizeof(timeCopy)-1);   // Copy time value
                    if (timeCopy[2] != ':') return signalError(106, fieldCount, field);
                    timeCopy[2] = 0;                                // Force null after hours
                    errorCode = checkValueRange (timeCopy, fieldCount, &hoursValue, 0, 23, 0);
                    if (errorCode) return errorCode;
                    errorCode = checkValueRange (&timeCopy[3], fieldCount, &minutesValue, 0, 59, 0);
                    if (errorCode) return errorCode;
                    agendaTable[index].time = (hoursValue * 60) + minutesValue;
                    // Check if this time is less thant previous one (time not sorted)
                    if (agendaTable[index].time < agendaPreviousTime) {
                        return signalError(109, fieldCount, field);
                    }
                } else if (fieldCount == 2) {                       // Room
                    uint32 crc = calculateCRC32(field, strlen(field));
                    for (i = 0; i < roomCount; i++) {               // Scan all rooms for name
                        if (roomTable[i].crc == crc) break;         // We found it
                    }
                    if (i < roomCount) {
                        agendaTable[index].tableType = typeRoom;    // Agenda type = room
                        agendaTable[index].tableIndex = i;
                    } else {
                        for (i = 0; i <groupCount; i++) {           // Scan all groups for name
                            if (groupTable[i].crc == crc) break;    // We found it
                        }
                        if (i < groupCount) {
                            agendaTable[index].tableType = typeGroup;   // Agenda type = group
                            agendaTable[index].tableIndex = i;
                        } else {
                            for (i = 0; i <cycleCount; i++) {       // Scan all cycles for name
                                if (cycleTable[i].crc == crc) break;// We found it
                            }
                            if (i < cycleCount) {
                                agendaTable[index].tableType = typeCycle;   // Agenda type = cycle
                                agendaTable[index].tableIndex = i;
                            } else {
                                for (i = 0; i <flashCount; i++) {   // Scan all flashes for name
                                    if (flashTable[i].crc == crc) break; // We found it
                                }
                                if (i < flashCount) {
                                    agendaTable[index].tableType = typeFlash; // Agenda type = flash
                                    agendaTable[index].tableIndex = i;
                                } else {
                                    return signalError(107, 0, field);
                                }
                            }
                        }
                    }
                } else if (fieldCount == 3) {                       // Color
                    if (agendaTable[index].tableType == typeRoom) { // On a room?
                        uint32 crc = calculateCRC32(field, strlen(field));
                        for (i = 0; i < colorCount; i++) {          // Scan all room for name
                            if (colorTable[i].crc == crc) break;    // We found it
                        }
                        if (i < colorCount) {
                            agendaTable[index].otherData = i;
                        } else {
                            return signalError(108, 0, field);
                        }
                    } else if (agendaTable[index].tableType == typeCycle || agendaTable[index].tableType == typeFlash) {
                        agendaTable[index].otherData = atol(field);
                    }
                } else if (fieldCount == 4) {                       // Intensity
                    errorCode = checkValueRange(field, fieldCount, &agendaTable[index].intensity,
                        (uint8_t) 0, (uint8_t) 100, (uint8_t) 100);
                    if (errorCode) return errorCode;
                }
            }
            field = strtok(NULL, separator);                        // Get next field
        }
        // Check for field count
        if (fileFormat == roomFileFormat) {
            if (fieldCount == 3) {                                  // Intensity missing
                roomTable[index].intensity = 100;
            } else if (fieldCount != 4) {
                return signalError(104, fieldCount);
            }
            // Check for last LED
            uint16_t lastLed = roomTable[index].firstLed + roomTable[index].ledCount -1;
            if (lastLed > ledCount) {
                return signalError(110, lastLed);
            }
        } else if (fileFormat == groupFileFormat) {
            if (fieldCount != 2) {
                return signalError(104, fieldCount);
            }
        } else if (fileFormat == colorFileFormat) {
            // Check for field count
            if (fieldCount != 4) {
                return signalError(104, fieldCount);
            }
        } else if (fileFormat == cycleFileFormat) {
            if (fieldCount == 2) {                                  // Color, wait time and random missing
                sequenceTable[index].color = 0;
                sequenceTable[index].waitTime = 0;
                sequenceTable[index].maxWaitTime = 0;
            } else if (fieldCount == 3) {                           // Wait time and random missing
                sequenceTable[index].waitTime = 0;
                sequenceTable[index].maxWaitTime = 0;
            } else if (fieldCount == 4) {                           // Random missing
                sequenceTable[index].maxWaitTime = 0;
            } else if (fieldCount != 5) {
                return signalError(104, fieldCount);
            }
        } else if (fileFormat == flashFileFormat) {
            if (fieldCount != 11) {
                return signalError(104, fieldCount);
            }
        } else if (fileFormat == agendaFileFormat) {
            if (fieldCount == 2){                                   // Color and intensity missing
                agendaTable[index].otherData = 0;
                agendaTable[index].intensity = 100;
            } else if (fieldCount == 3){                            // Intensity missing
                agendaTable[index].intensity = 100;
            } else if (fieldCount != 4) {
                return signalError(104, fieldCount);
            }
        }
    }
    return 0;
}

// Wait until event queue empty for 100 ms max (return true if timeout occured)
bool waitForEventsEmpty(void) {
    int loopCount = 0;
    // Wait for enpty event queue for 100 ms
    while (events.avgPacketsWaiting() && loopCount < 100) {
        delay(1);
        loopCount++;
    }
    return (events.avgPacketsWaiting());
}

//  Load all data from configuration file (Error management)
int loadAgenda(void) {
    if (traceEnter) enterRoutine(__func__);
    unsigned long startTime = millis();
    agendaError = loadAgendaDetails();
    if (agendaError) {
        #ifdef VERSION_FRANCAISE
            trace_error_P("Erreur %d dans %s - Simulation impossible", agendaError, fileToStart.c_str());
        #else
            trace_error_P("Error %d loading %s - No actions will be done!", agendaError, fileToStart.c_str());
        #endif
    } else {
        #ifdef VERSION_FRANCAISE
            trace_info_P("%s chargé en %d ms", fileToStart.c_str(), millis() - startTime);
        #else
            trace_info_P("%s loaded in %d ms", fileToStart.c_str(),millis() - startTime);
        #endif
    }
    updateWebServerData();
    return agendaError;
}

//  Load all data from configuration file (Do the job)
int loadAgendaDetails(void) {
    int errorCode = 0;

    roomCount = 0;
    groupCount = 0;
    colorCount = 0;
    cycleCount = 0;
    flashCount = 0;
    sequenceCount = 0;
    agendaCount = 0;
    agendaError = 0;
    cyclePtr = 0;
    cycleDefined = 0;
    agendaPreviousTime = 0;

    errorCode = readFile(fileToStart.c_str(), &readAllHeaders);     // Load all headers
    if (errorCode) return errorCode;

    // Check if all headers have been found
    if (!roomCount) {
        return signalError(102, 0,roomName);
    }
    if (!colorCount) {
        return signalError(102, 0, colorName);
    }
    if (!agendaCount) {
        return signalError(102, 0, agendaName);
    }

    delete[] agendaTable;                                           // Clear tables
    delete[] sequenceTable;
    delete[] cycleTable;
    delete[] flashTable;
    delete[] colorTable;
    delete[] groupTable;
    delete[] roomTable;
    delete[] previousColor;
    previousColor = new previousColor_s[ledCount];                  // Create previous color table
    memset(previousColor, 0, ledCount * sizeof(previousColor_s));
    roomTable = new roomTable_s[roomCount+1];                       // Create room table
    memset(roomTable, 0, (roomCount+1) * sizeof(roomTable_s));
    groupTable = new groupTable_s[groupCount];                      // Create group table
    memset(groupTable, 0, groupCount * sizeof(groupTable_s));
    colorTable = new colorTable_s[colorCount+1];                    // Create color table
    memset(colorTable, 0, (colorCount+1) * sizeof(colorTable_s));
    flashTable = new flashTable_s[flashCount+1];                    // Create flash table
    memset(flashTable, 0, (flashCount+1) * sizeof(flashTable_s));
    cycleTable = new cycleTable_s[cycleCount];                      // Create cycle table
    memset(cycleTable, 0, cycleCount * sizeof(cycleTable_s));
    sequenceTable = new sequenceTable_s[sequenceCount];             // Create sequence table
    memset(sequenceTable, 0, sequenceCount * sizeof(sequenceTable_s));
    agendaTable = new agendaTable_s[agendaCount];                   // Create agenda table
    memset(agendaTable, 0, agendaCount * sizeof(agendaTable_s));

    roomTable[roomCount].firstLed = 1;                              // First LED to light
    roomTable[roomCount].ledCount = 1;                              // LED count to light
    roomTable[roomCount].intensity = 100;                           // Luminosity percentage (0-100)
    colorTable[colorCount].r = 255;                                 // LED Red level (0-255)
    colorTable[colorCount].g = 255;                                 // LED Green level (0-255)
    colorTable[colorCount].b = 255;                                 // LED Blue level (0-255)
    flashTable[flashCount].roomOrGroup = roomCount;                 // Pointer to room
    flashTable[flashCount].color = colorCount;                      // Pointer to color
    flashTable[flashCount].intensity = 100;                         // Flash intensity
    flashTable[flashCount].onMin = 1;                               // Minimum on time
    flashTable[flashCount].onMax = 0;                               // Maximum on time
    flashTable[flashCount].offMin = 1;                              // Minimum off time
    flashTable[flashCount].offMax = 0;                              // Maximum off time
    flashTable[flashCount].repeatMin = 0;                           // Minimum repeat count
    flashTable[flashCount].repeatMax = 0;                           // Maximum repeat count
    flashTable[flashCount].pauseMin = 1;                            // Minimum pause time
    flashTable[flashCount].pauseMax = 0;                            // Maximum pause time

    errorCode = readFile(fileToStart.c_str(), &readAllTables);      // Load all tables
    if (errorCode) return errorCode;

    checkAgenda();

    if (traceTable) {
        trace_debug_P("Row 1st Cnt Int (%s)", roomName);
        for (int i = 0; i < roomCount; i++) {
            trace_debug_P("%3d %3d %3d %3d", i,
                roomTable[i].firstLed, roomTable[i].ledCount,roomTable[i].intensity);
            waitForEventsEmpty();                                   // Wait for 100 ms or event queue empty
        }
        trace_debug_P("Row     Name Rom (%s)", groupName);
        for (int i = 0; i < groupCount; i++) {
            trace_debug_P("%3d %8x %3d", i,
                groupTable[i].crc, groupTable[i].room);
            waitForEventsEmpty();                                   // Wait for 100 ms or event queue empty
        }
        trace_debug_P("Row   R   G   B (%s)", colorName);
        for (int i = 0; i < colorCount; i++) {
            trace_debug_P("%3d %3d %3d %3d", i,
                colorTable[i].r, colorTable[i].g, colorTable[i].b);
            waitForEventsEmpty();                                   // Wait for 100 ms or event queue empty
        }
        trace_debug_P("Row Idex Flg OnMin OnMax OfMin OfMax RpMin RpMax PsMin PsMax (%s)", flashName);
        for (int i = 0; i < flashCount; i++) {
            trace_debug_P("%3d %4d %3d %5d %5d %5d %5d %5d %5d %5d %5d", i,
                flashTable[i].roomOrGroup, flashTable[i].color,
                flashTable[i].onMin, flashTable[i].onMax,
                flashTable[i].offMin, flashTable[i].offMax,
                flashTable[i].repeatMin, flashTable[i].repeatMax,
                flashTable[i].pauseMin, flashTable[i].pauseMax);
            waitForEventsEmpty();                                   // Wait for 100 ms or event queue empty
        }
        trace_debug_P("Row Run Act Cnt  Wait (%s)", cycleName);
        for (int i = 0; i < cycleCount; i++) {
            trace_debug_P("%3d %3d %3d %3d %5d", i,
                cycleTable[i].isActive, cycleTable[i].activeSequence,
                cycleTable[i].sequenceCount, cycleTable[i].waitTime);
            waitForEventsEmpty();                                   // Wait for 100 ms or event queue empty
        }
        trace_debug_P("Row Cyc Seq Idex Clr  Wait MaxWt (Sequence)", NULL);
        for (int i = 0; i < sequenceCount; i++) {
            trace_debug_P("%3d %3d %3d %4d %3d %5d %5d", i,
                sequenceTable[i].cycle, sequenceTable[i].sequence,
                sequenceTable[i].roomOrGroup, sequenceTable[i].color,
                sequenceTable[i].waitTime, sequenceTable[i].maxWaitTime);
            waitForEventsEmpty();                                   // Wait for 100 ms or event queue empty
        }
        trace_debug_P("Row Flg Time Ind Oth Int (%s)", agendaName);
        for (int i = 0; i < agendaCount; i++) {
            trace_debug_P("%3d %3d %4d %3d %3d %3d", i,
                agendaTable[i].tableType, agendaTable[i].time,
                agendaTable[i].tableIndex, agendaTable[i].otherData,
                agendaTable[i].intensity);
            waitForEventsEmpty();                                   // Wait for 100 ms or event queue empty
        }
    }
    return 0;
}

// Check agenda for warnings/errors/inconsistencies
void checkAgenda(void){
    int startLed;
    int endLed;
    
    // Scan all rooms but last
    for (int i=0; i < roomCount-1; i++) {
        // Load start and end LEDs
        startLed = roomTable[i].firstLed;
        endLed = startLed + roomTable[i].ledCount -1;
        // Scan all remaining  rooms
        for (int j=i+1; j < roomCount; j++) {
            if (roomTable[j].firstLed <= endLed && (roomTable[j].firstLed + roomTable[j].ledCount-1) >= startLed) {
            #ifdef VERSION_FRANCAISE
                trace_info_P("Chevauchement des LED entre les pièces %d et %d", i, j);
            #else
                trace_info_P("LED overlapping between rooms %d and %d", i, j);
            #endif
            }
        }
    }
}

//          -------------------------------------
//          ---- Program initialization code ----
//          -------------------------------------

// Setup routine
void setup(void) {
    traceEnter = true;
    logSetup();                                                     // Init log
    traceSetup();                                                   // Register trace
    #ifdef ESP8266
        Serial.begin(74880);
    #else
        Serial.begin(115200);
    #endif
    Serial.setDebugOutput(false);                                   // To allow Serial.swap() to work properly

    Serial.println("");
    #ifdef VERSION_FRANCAISE
        trace_info_P("Initialise %s V%s", __FILENAME__, VERSION);
    #else
        trace_info_P("Initializing %s V%s", __FILENAME__, VERSION);
    #endif
    resetCause = getResetCause();                                   // Get reset cause
    trace_info_P("Cause : %s", resetCause.c_str());

    #ifdef ESP32
        // Stop Blutooth
        btStop();
    #endif

    // Starts flash file system
    LittleFS.begin();

    #define DUMP_FILE_SYSTEM
    #ifdef DUMP_FILE_SYSTEM
        String path = "/";
        #ifdef ESP32
            File dir = LittleFS.open(path);
            #ifdef VERSION_FRANCAISE
                trace_info_P("Contenu flash", NULL);
            #else
                trace_info_P("FS content", NULL);
            #endif
            File entry = dir.openNextFile();
            while(entry){
        #else
            Dir dir = LittleFS.openDir(path);
            path = String();
            #ifdef VERSION_FRANCAISE
                trace_info_P("Contenu flash", NULL);
            #else
                trace_info_P("FS content", NULL);
            #endif
            while(dir.next()){
                fs::File entry = dir.openFile("r");
        #endif
                String fileName = String(entry.name());
                #ifdef ESP32
                    fileName = path + fileName;
                #endif
                trace_info_P("%s", fileName.c_str());
                #ifdef ESP32
                    entry = dir.openNextFile();
                #else
                    entry.close();
                #endif
                }
        #ifdef ESP32
            dir.close();
        #endif
    #endif

    // Load preferences
    if (!readSettings()) {
        #ifdef VERSION_FRANCAISE
            trace_error_P("Pas de configuration, stop !", NULL);
        #else
            trace_error_P("No settings, stopping!", NULL);
        #endif
        while (true) {
            yield();
        }
    };

    hostName = espName;                                             // Set host name to ESP name
    hostName.replace(" ", "-");                                     // Replace spaces by dashes
    WiFi.hostname(hostName.c_str());                                // Define this module name for client network
    WiFi.setAutoReconnect(true);                                    // Reconnect automatically

    #ifdef ESP32
        WiFi.IPv6(false);                                           // Disable IP V6 for AP
    #endif

    #ifdef ESP32
        WiFi.onEvent(onWiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
        WiFi.onEvent(onWiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        WiFi.onEvent(onWiFiStationGotIp, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    #endif
    #ifdef  ESP8266
        onStationModeConnectedHandler = WiFi.onStationModeConnected(&onWiFiStationConnected); // Declare connection callback
        onStationModeDisconnectedHandler = WiFi.onStationModeDisconnected(&onWiFiStationDisconnected); // Declare disconnection callback
        onStationModeGotIPHandler = WiFi.onStationModeGotIP(&onWiFiStationGotIp); // Declare got IP callback
    #endif

    ssid.trim();
    if (ssid != "") {                                               // If SSID is given, try to connect to
        #ifdef ESP32
            WiFi.mode(WIFI_MODE_STA);                               // Set station mode
        #endif
        #ifdef ESP8266
            WiFi.mode(WIFI_STA);                                    // Set station mode
        #endif
        #ifdef VERSION_FRANCAISE
            trace_info_P("Recherche %s", ssid.c_str());
        #else
            trace_info_P("Searching %s", ssid.c_str());
        #endif
        WiFi.begin(ssid.c_str(), pwd.c_str());                      // Start to connect to existing SSID
        uint16_t loopCount = 0;
        while (WiFi.status() != WL_CONNECTED && loopCount < 10) {   // Wait for connection for 10 seconds
            delay(1000);                                            // Wait for 1 s
            loopCount++;
        }                                                           // Loop
        if (WiFi.status() == WL_CONNECTED) {                        // If we're not connected
            #ifdef VERSION_FRANCAISE
                trace_info_P("Connexion à %s par http://%s/ ou http://%s/ ",
                    ssid.c_str(), WiFi.localIP().toString().c_str(), hostName.c_str());
            #else
                trace_info_P("Connect to %s with http://%s/ or http://%s/ ",
                    ssid.c_str(), WiFi.localIP().toString().c_str(), hostName.c_str());
            #endif
        } else {
            #ifdef VERSION_FRANCAISE
                trace_info_P("Pas connecté, passe en mode point d'accès ...", NULL);
            #else
                trace_info_P("Not connected, starting access point...", NULL);
            #endif
        }
    }

    if (WiFi.status() != WL_CONNECTED) {                            // If not connected, start access point
        #ifdef ESP32
            WiFi.mode(WIFI_MODE_AP);                                // Set access point mode
        #endif
        #ifdef ESP8266
            WiFi.mode(WIFI_AP);                                     // Set access point mode
        #endif
        char buffer[80];
        // Load this Wifi access point name as ESP name plus ESP chip Id
        #ifdef ESP32
            snprintf_P(buffer, sizeof(buffer),PSTR("%s_%X"), hostName.c_str(), getChipId());
        #else
            snprintf_P(buffer, sizeof(buffer),PSTR("%s_%X"), hostName.c_str(), ESP.getChipId());
        #endif
        checkFreeBufferSpace(__func__, __LINE__, "buffer", sizeof(buffer), strlen(buffer));
        #ifdef VERSION_FRANCAISE
            trace_info_P("Creation du point d'accès %s (%s)", buffer, accessPointPwd.c_str());
        #else
            trace_info_P("Creating %s access point (%s)", buffer, accessPointPwd.c_str());
        #endif
        WiFi.softAP(buffer, accessPointPwd.c_str());                // Starts Wifi access point
        #ifdef VERSION_FRANCAISE
            trace_info_P("Connexion à %s par http://%s/", buffer, WiFi.softAPIP().toString().c_str());
            snprintf_P(buffer, sizeof(buffer), "Point d'accès %s actif (%s)",
                ssid.c_str(), WiFi.softAPIP().toString().c_str());
        #else
            trace_info_P("Connect to %s with http://%s/", buffer, WiFi.softAPIP().toString().c_str());
            snprintf_P(buffer, sizeof(buffer), "WiFi access point %s active (%s)",
                ssid.c_str(), WiFi.softAPIP().toString().c_str());
        #endif
        checkFreeBufferSpace(__func__, __LINE__, "buffer", sizeof(buffer), strlen(buffer));
        wifiState = String(buffer);
    }

    updateWebServerData();

    // Start syslog
    syslogSetup();                                                  // Init log

    // OTA trace
    ArduinoOTA.onStart(onStartOTA);
    ArduinoOTA.onEnd(onEndOTA);
    ArduinoOTA.onError(onErrorOTA);

    //ArduinoOTA.setPassword("my OTA password");                    // Uncomment to set an OTA password
    ArduinoOTA.begin();                                             // Initialize OTA

    #ifdef VERSION_FRANCAISE
        trace_info_P("%s V%s lancé", __FILENAME__, VERSION);
        trace_info_P("Cause : %s", resetCause.c_str());
    #else
        trace_info_P("Starting %s V%s", __FILENAME__, VERSION);
        trace_info_P("Reset cause: %s", resetCause.c_str());
    #endif

    // List of URL to be intercepted and treated locally before a standard treatment
    //  These URL can be used as API
    webServer.on("/status", HTTP_GET, statusReceived);              // /status request
    webServer.on("/setup", HTTP_GET, setupReceived);                // /setup request
    webServer.on("/command", HTTP_GET, commandReceived);            // /command request
    webServer.on("/languages", HTTP_GET, languagesReceived);        // /languages request
    webServer.on("/configs", HTTP_GET, configsReceived);            // /configs received
    webServer.on("/settings", HTTP_GET, settingsReceived);          // /settings request
    webServer.on("/debug", HTTP_GET, debugReceived);                // /debug request
    webServer.on("/rest", HTTP_GET, restReceived);                  // /rest request
    webServer.on("/log", HTTP_GET, logReceived);                    // /log request
    webServer.on("/upload", HTTP_POST, startUpload, handleUpload);  // /download received
    webServer.on("/tables", HTTP_GET, tableReceived);               // /tables received
    // These URL are used internally by setup.htm - Use them at your own risk!
    webServer.on("/changed", HTTP_GET, setChangedReceived);         // /changed request

    // Other webserver stuff
    webServer.addHandler(&events);                                  // Define web events
    webServer.addHandler(new LittleFSEditor());                     // Define file system editor
    webServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.htm"); // Serve "/", default page = index.htm
    webServer.onNotFound (notFound);                                // To be called when URL is not known

    events.onConnect([](AsyncEventSourceClient *client){            // Routine called when a client connects
        #ifdef VERSION_FRANCAISE
            trace_debug_P("Client connecté", NULL);
        #else
            trace_debug_P("Event client connected", NULL);
        #endif
        // Set send an update flag
        sendAnUpdateFlag = true;
        // Send last log lines
        for (uint16_t i=0; i < LOG_MAX_LINES; i++) {
            String logLine = getLogLine(i, true);
            if (logLine != "") {
                client->send(logLine, "info");
            }
        }
        char msg[20];
        snprintf_P(msg, sizeof(msg),"%016lx", millis());
        client->send(msg, "time");
    });

    events.onDisconnect([](AsyncEventSourceClient *client){         // Routine called when a client connects
        #ifdef VERSION_FRANCAISE
            trace_debug_P("Client déconnecté", NULL);
        #else
            trace_debug_P("Event client disconnected", NULL);
        #endif
    });

    webServer.begin();                                              // Start Web server
    lightSetup();                                                   // Start simulation

    #ifdef VERSION_FRANCAISE
        trace_info_P("Fin lancement", NULL);
    #else
        trace_info_P("Init done", NULL);
    #endif
}

//      ------------------------
//      ---- Permanent loop ----
//      ------------------------

// Main loop
void loop(void) {
    lightLoop();                                                    // Work with LEDs
    uploadLoop();                                                   // Check for an uploaded file
    ArduinoOTA.handle();                                            // Give hand to OTA
    #ifdef SERIAL_COMMANDS
        serialLoop();                                               // Scan for serial commands
    #endif
    // Send an update to clients if needed
    if (sendAnUpdateFlag) {
        sendWebServerUpdate();                                      // Send updated data to clients
    }
    #ifdef FF_TRACE_USE_SYSLOG
        if ((micros() - lastSyslogMessageMicro) > 600000000) {      // Last syslog older than 10 minutes?
            #ifdef VERSION_FRANCAISE
               trace_info_P("Toujours vivant ...", NULL);
            #else
               trace_info_P("I'm still alive...", NULL);
            #endif
        }
    #endif
    if (restartMe) {
        #ifdef VERSION_FRANCAISE
            trace_info_P("Relance l'ESP ...", NULL);
        #else
            trace_info_P("Restarting ESP ...", NULL);
        #endif
        delay(1000);
        ESP.restart();
    }
    delay(1);
}