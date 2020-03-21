// Default Arduino includes
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <nvs.h>
#include <nvs_flash.h>

// Includes for JSON object handling
// Requires ArduinoJson library
// https://arduinojson.org
// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>

// Includes for BLE Arduino-NimBLE
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include <NimBLEDevice.h>
#include <NimBLEAdvertising.h>

/** Build time */
const char compileDate[] = __DATE__ " " __TIME__;

/** Unique device name */
char apName[] = "ESP32-xxxxxxxxxxxx";
/** Selected network 
    true = use primary network
		false = use secondary network
*/
bool usePrimAP = true;
/** Flag if stored AP credentials are available */
bool hasCredentials = false;
/** Connection status */
volatile bool isConnected = false;
/** Connection change status */
bool connStatusChanged = false;

/**
 * Create unique device name from MAC address
 **/
void createName()
{
	uint8_t baseMac[6];
	// Get MAC address for WiFi station
	esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
	// Write unique name into apName
	sprintf(apName, "ESP32-%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
}

// List of Service and Characteristic UUIDs
// #define SERVICE_UUID "0000aaaa-ead2-11e7-80c1-9a214cf093ae"
// #define WIFI_UUID "00005555-ead2-11e7-80c1-9a214cf093ae"
#define SERVICE_UUID "0000AAAA-EAD2-11E7-80C1-9A214CF093AE"
#define WIFI_UUID "00005555-EAD2-11E7-80C1-9A214CF093AE"

/** SSIDs of local WiFi networks */
String ssidPrim;
String ssidSec;
/** Password for local WiFi network */
String pwPrim;
String pwSec;

/** Characteristic for digital output */
BLECharacteristic *pCharacteristicWiFi;
/** BLE Advertiser */
BLEAdvertising *pAdvertising;
/** BLE Service */
BLEService *pService;
/** BLE Server */
BLEServer *pServer;

/** Buffer for JSON string */
// Max size is 51 bytes for frame:
// {"ssidPrim":"","pwPrim":"","ssidSec":"","pwSec":""}
// + 4 x 32 bytes for 2 SSID's and 2 passwords
StaticJsonDocument<200> jsonBuffer;

/**
 * MyServerCallbacks
 * Callbacks for client connection and disconnection
 */
class MyServerCallbacks : public BLEServerCallbacks
{
	// TODO this doesn't take into account several clients being connected
	void onConnect(BLEServer *pServer)
	{
		Serial.println("BLE client connected");
	};

	void onDisconnect(BLEServer *pServer)
	{
		Serial.println("BLE client disconnected");
		pAdvertising->start();
	}
};

/**
 * MyCallbackHandler
 * Callbacks for BLE client read/write requests
 */
class MyCallbackHandler : public BLECharacteristicCallbacks
{
	void onWrite(BLECharacteristic *pCharacteristic)
	{
		std::string value = pCharacteristic->getValue();
		if (value.length() == 0)
		{
			return;
		}
		Serial.println("Received over BLE: " + String((char *)&value[0]));

		// Decode data
		int keyIndex = 0;
		for (int index = 0; index < value.length(); index++)
		{
			value[index] = (char)value[index] ^ (char)apName[keyIndex];
			keyIndex++;
			if (keyIndex >= strlen(apName))
				keyIndex = 0;
		}

		/** Json object for incoming data */
		auto error = deserializeJson(jsonBuffer, (char *)&value[0]);
		// JsonObject& jsonIn = jsonBuffer.parseObject((char *)&value[0]);
		if (error)
		{
			Serial.println("Received invalid JSON");
		}
		else
		{

			if (jsonBuffer.containsKey("ssidPrim") &&
				jsonBuffer.containsKey("pwPrim") &&
				jsonBuffer.containsKey("ssidSec") &&
				jsonBuffer.containsKey("pwSec"))
			{
				ssidPrim = jsonBuffer["ssidPrim"].as<String>();
				pwPrim = jsonBuffer["pwPrim"].as<String>();
				ssidSec = jsonBuffer["ssidSec"].as<String>();
				pwSec = jsonBuffer["pwSec"].as<String>();

				Preferences preferences;
				preferences.begin("WiFiCred", false);
				preferences.putString("ssidPrim", ssidPrim);
				preferences.putString("ssidSec", ssidSec);
				preferences.putString("pwPrim", pwPrim);
				preferences.putString("pwSec", pwSec);
				preferences.putBool("valid", true);
				preferences.end();

				Serial.println("Received over bluetooth:");
				Serial.println("primary SSID: " + ssidPrim + " password: " + pwPrim);
				Serial.println("secondary SSID: " + ssidSec + " password: " + pwSec);
				connStatusChanged = true;
				hasCredentials = true;
			}
			else if (jsonBuffer.containsKey("erase"))
			{
				Serial.println("Received erase command");
				Preferences preferences;
				preferences.begin("WiFiCred", false);
				preferences.clear();
				preferences.end();
				connStatusChanged = true;
				hasCredentials = false;
				ssidPrim = "";
				pwPrim = "";
				ssidSec = "";
				pwSec = "";

				int err;
				err = nvs_flash_init();
				Serial.println("nvs_flash_init: " + err);
				err = nvs_flash_erase();
				Serial.println("nvs_flash_erase: " + err);
			}
			else if (jsonBuffer.containsKey("reset"))
			{
				WiFi.disconnect();
				esp_restart();
			}
		}
		jsonBuffer.clear();
	};

	void onRead(BLECharacteristic *pCharacteristic)
	{
		Serial.println("BLE onRead request");
		String wifiCredentials;

		/** Json object for outgoing data */
		// JsonObject &jsonOut = jsonBuffer.createObject();
		jsonBuffer.clear();
		jsonBuffer["ssidPrim"] = ssidPrim;
		jsonBuffer["pwPrim"] = pwPrim;
		jsonBuffer["ssidSec"] = ssidSec;
		jsonBuffer["pwSec"] = pwSec;
		// Convert JSON object into a string
		serializeJson(jsonBuffer, wifiCredentials);
		// jsonBuffer.printTo(wifiCredentials);

		// encode the data
		int keyIndex = 0;
		Serial.println("Stored settings: " + wifiCredentials);
		for (int index = 0; index < wifiCredentials.length(); index++)
		{
			wifiCredentials[index] = (char)wifiCredentials[index] ^ (char)apName[keyIndex];
			keyIndex++;
			if (keyIndex >= strlen(apName))
				keyIndex = 0;
		}
		pCharacteristicWiFi->setValue((uint8_t *)&wifiCredentials[0], wifiCredentials.length());
		jsonBuffer.clear();
	}
};

/**
 * initBLE
 * Initialize BLE service and characteristic
 * Start BLE server and service advertising
 */
void initBLE()
{
	// Initialize BLE and set output power
	BLEDevice::init(apName);
	BLEDevice::setPower(ESP_PWR_LVL_P7);

	Serial.printf("BLE advertising using %s\n", apName);

	// Create BLE Server
	pServer = BLEDevice::createServer();

	// Set server callbacks
	pServer->setCallbacks(new MyServerCallbacks());

	// Create BLE Service
	// pService = pServer->createService(BLEUUID(SERVICE_UUID), 20);
	pService = pServer->createService(SERVICE_UUID);

	// Create BLE Characteristic for WiFi settings
	pCharacteristicWiFi = pService->createCharacteristic(
		// BLEUUID(WIFI_UUID),
		WIFI_UUID,
		PROPERTY_READ |
			PROPERTY_WRITE);
	pCharacteristicWiFi->setCallbacks(new MyCallbackHandler());

	// Start the service
	pService->start();

	// Start advertising
	pAdvertising = pServer->getAdvertising();
	pAdvertising->addServiceUUID(WIFI_UUID);
	pAdvertising->start();
}

/** Callback for receiving IP address from AP */
void gotIP(system_event_id_t event)
{
	isConnected = true;
	connStatusChanged = true;
}

/** Callback for connection loss */
void lostCon(system_event_id_t event)
{
	isConnected = false;
	connStatusChanged = true;
}

/**
	 scanWiFi
	 Scans for available networks 
	 and decides if a switch between
	 allowed networks makes sense

	 @return <code>bool</code>
	        True if at least one allowed network was found
*/
bool scanWiFi()
{
	/** RSSI for primary network */
	int8_t rssiPrim = -100;
	/** RSSI for secondary network */
	int8_t rssiSec = -100;
	/** Result of this function */
	bool result = false;

	Serial.println("Start scanning for networks");

	WiFi.disconnect(true);
	WiFi.enableSTA(true);
	WiFi.mode(WIFI_STA);

	// Scan for AP
	int apNum = WiFi.scanNetworks(false, true, false, 1000);
	if (apNum == 0)
	{
		Serial.println("Found no networks?????");
		return false;
	}

	byte foundAP = 0;
	bool foundPrim = false;

	for (int index = 0; index < apNum; index++)
	{
		String ssid = WiFi.SSID(index);
		Serial.println("Found AP: " + ssid + " RSSI: " + WiFi.RSSI(index));
		if (!strcmp((const char *)&ssid[0], (const char *)&ssidPrim[0]))
		{
			Serial.println("Found primary AP");
			foundAP++;
			foundPrim = true;
			rssiPrim = WiFi.RSSI(index);
		}
		if (!strcmp((const char *)&ssid[0], (const char *)&ssidSec[0]))
		{
			Serial.println("Found secondary AP");
			foundAP++;
			rssiSec = WiFi.RSSI(index);
		}
	}

	switch (foundAP)
	{
	case 0:
		result = false;
		break;
	case 1:
		if (foundPrim)
		{
			usePrimAP = true;
		}
		else
		{
			usePrimAP = false;
		}
		result = true;
		break;
	default:
		Serial.printf("RSSI Prim: %d Sec: %d\n", rssiPrim, rssiSec);
		if (rssiPrim > rssiSec)
		{
			usePrimAP = true; // RSSI of primary network is better
		}
		else
		{
			usePrimAP = false; // RSSI of secondary network is better
		}
		result = true;
		break;
	}
	return result;
}

/**
 * Start connection to AP
 */
void connectWiFi()
{
	// Setup callback function for successful connection
	WiFi.onEvent(gotIP, SYSTEM_EVENT_STA_GOT_IP);
	// Setup callback function for lost connection
	WiFi.onEvent(lostCon, SYSTEM_EVENT_STA_DISCONNECTED);

	WiFi.disconnect(true);
	WiFi.enableSTA(true);
	WiFi.mode(WIFI_STA);

	Serial.println();
	Serial.print("Start connection to ");
	if (usePrimAP)
	{
		Serial.println(ssidPrim);
		WiFi.begin(ssidPrim.c_str(), pwPrim.c_str());
	}
	else
	{
		Serial.println(ssidSec);
		WiFi.begin(ssidSec.c_str(), pwSec.c_str());
	}
}

void setup()
{
	// Create unique device name
	createName();

	// Initialize Serial port
	Serial.begin(115200);
	// Send some device info
	Serial.print("Build: ");
	Serial.println(compileDate);

	Preferences preferences;
	preferences.begin("WiFiCred", false);
	bool hasPref = preferences.getBool("valid", false);
	if (hasPref)
	{
		ssidPrim = preferences.getString("ssidPrim", "");
		ssidSec = preferences.getString("ssidSec", "");
		pwPrim = preferences.getString("pwPrim", "");
		pwSec = preferences.getString("pwSec", "");

		if (ssidPrim.equals("") || pwPrim.equals("") || ssidSec.equals("") || pwPrim.equals(""))
		{
			Serial.println("Found preferences but credentials are invalid");
		}
		else
		{
			Serial.println("Read from preferences:");
			Serial.println("primary SSID: " + ssidPrim + " password: " + pwPrim);
			Serial.println("secondary SSID: " + ssidSec + " password: " + pwSec);
			hasCredentials = true;
		}
	}
	else
	{
		Serial.println("Could not find preferences, need send data over BLE");
	}
	preferences.end();

	// Start BLE server
	initBLE();

	if (hasCredentials)
	{
		// Check for available AP's
		if (!scanWiFi())
		{
			Serial.println("Could not find any AP");
		}
		else
		{
			// If AP was found, start connection
			connectWiFi();
		}
	}
	Serial.println("\n\n##################################");
	Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
	Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
	Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
	Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
	Serial.println("##################################\n\n");
}

void loop()
{
	if (connStatusChanged)
	{
		if (isConnected)
		{
			Serial.print("Connected to AP: ");
			Serial.print(WiFi.SSID());
			Serial.print(" with IP: ");
			Serial.print(WiFi.localIP());
			Serial.print(" RSSI: ");
			Serial.println(WiFi.RSSI());
		}
		else
		{
			if (hasCredentials)
			{
				Serial.println("Lost WiFi connection");
				// Received WiFi credentials
				if (!scanWiFi())
				{ // Check for available AP's
					Serial.println("Could not find any AP");
				}
				else
				{ // If AP was found, start connection
					connectWiFi();
				}
			}
		}
		connStatusChanged = false;
	}
}