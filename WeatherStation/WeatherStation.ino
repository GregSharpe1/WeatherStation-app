/*
    Weather Station

    Sketch for a Aberyswyth University, Computer Science, Major Project
    Retrieves meteorological information from an assorted range of sensors including;
    an anemometer, rain gauge and two DHT22 temperature/humidity sensors and BMP180.

    The data is outputted in a standard json format and then passed onto a Raspberry
    Pi Zero for process and eventually sent to AWS IoT Service.

    Created by Greg Sharpe
*/

// Included libraries
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <ArduinoJson.h>

// Debug flag
#define DEBUG true

// Anemometer defines
#define ANEMOMETER_PIN D3
#define DATAGRAM_SIZE 41 + 5

// Initialise variables
const long interval = 30000; // Time interval between each reading
unsigned long currentTime = 0; // Current Time
unsigned long previousTime = 0; // Previous current time

// Anemometer declare's
unsigned char anemometerDatagram[DATAGRAM_SIZE]; // Array to hold anemometer datagram
unsigned int windDirectionNo = 0; // Wind direction in bit value
String windDirection = ""; // Wind direction as compass direction
double windSpeed = 0; // Wind speed in metres per second

// Initialize Temperature, Humidity and Altitude sensors
// SCL from the BMP180 to D1
// SDA from the BMP180 to D2
Adafruit_BMP085 BMP;

// DHT22 Humidity sensor defines
#define DHT_TYPE DHT22
// INWARD (inside the case)
#define DHT22_INWARD_PIN D4
// OUTWARD (outside the case)
#define DHT22_OUTWARD_PIN D5

// Initalize the DHT Sensor
DHT INWARD_DHT_22(DHT22_INWARD_PIN, DHT_TYPE, 11);
DHT OUTWARD_DHT_22(DHT22_OUTWARD_PIN, DHT_TYPE, 11);

// Initalize the JSON Buffer to 200 Characters
StaticJsonBuffer<200> jsonBuffer;

enum DHTReadingType {
  TEMPERATURE,
  HUMIDITY
};

// Function that retrieves the datagram from the amemometer and places it into a char array
boolean recieveDatagram() {
  unsigned long duration = 0;

  while (digitalRead(ANEMOMETER_PIN) == HIGH) {
    delayMicroseconds(1);
  }
  while (digitalRead(ANEMOMETER_PIN) == LOW) {
    delayMicroseconds(1);
    duration++;
  }
  // Anemometer transmits data every two seconds
  if (duration > 200000) {
    delayMicroseconds(600); // Go to middle of first bit
    for (int i = 0; i < DATAGRAM_SIZE; i++) {
      anemometerDatagram[i] = digitalRead(ANEMOMETER_PIN);
      delayMicroseconds(1200);
    }
    return true;
  }
  else {
    return false;
  }
}

// Function to return the value from a DHT22 sensor depending on which sensor (there will be multiple)
// And which reading you would like to take (Humidity or Temperature)
double getDHTValue(DHT sensor, DHTReadingType type) {
  double dhtreading;
  
  // Take input when calling the Function
  // Depending on what is called return
  // the given reading.
  switch (type) {
    case TEMPERATURE:
      dhtreading = sensor.readTemperature();
      break;
    case HUMIDITY:
      dhtreading = sensor.readHumidity();
      break;
    default:
      Serial.println("Please choose one of the following: TEMPERATURE, HUMIDITY");
      break;
  }
  return dhtreading;
}
  

// Function that decomposes the datagram of an anemometer into its composed varaibles wind direction and wind speed
// **Modified from work by Pete Todd cht35@aber.ac.uk**
void decomposeWindReading() {
  // Determines wind direction from the supplied 4 bit value. Requires inverting and needs its endinness swapped
  windDirectionNo = (anemometerDatagram[8] << 3 ) + (anemometerDatagram[7] << 2 ) + (anemometerDatagram[6] << 1 ) + anemometerDatagram[5];
  windDirectionNo = (~windDirectionNo) & 0x0f;

  // Determines wind speed from the supplied 12 bit value. Requires inverting and needs its endinness swapped
  unsigned int windSpeedValue = (anemometerDatagram[20] << 11) + (anemometerDatagram[19] << 10) + (anemometerDatagram[18] << 9) + (anemometerDatagram[17] << 8) +
                                (anemometerDatagram[16] << 7) + (anemometerDatagram[15] << 6) + (anemometerDatagram[14] << 5) + (anemometerDatagram[13] << 4) +
                                (anemometerDatagram[12] << 3) + (anemometerDatagram[11] << 2) + (anemometerDatagram[10] << 1) + anemometerDatagram[9];

  windSpeedValue = (~windSpeedValue) & 0x1ff;

  // Calculates the checksum from the wind direction and three nibbles of the 12 bit wind speed by doing a sum calculation
  unsigned char windSpeedNibble1to4 = (anemometerDatagram[12] << 3) + (anemometerDatagram[11] << 2) + (anemometerDatagram[10] << 1) + anemometerDatagram[9];
  windSpeedNibble1to4 = (~windSpeedNibble1to4) & 0x0f;
  unsigned char windSpeedNibble5to8 = (anemometerDatagram[16] << 3) + (anemometerDatagram[15] << 2) + (anemometerDatagram[14] << 1) + anemometerDatagram[13];
  windSpeedNibble5to8 = (~windSpeedNibble5to8) & 0x0f;
  unsigned char windSpeedNibble9to12 = (anemometerDatagram[20] << 3) + (anemometerDatagram[19] << 2) + (anemometerDatagram[18] << 1) + anemometerDatagram[17];
  windSpeedNibble9to12 = (~windSpeedNibble9to12) & 0x0f;

  // SUM Calculation of nibbles
  unsigned int windSpeedNibleSum = windSpeedNibble1to4 + windSpeedNibble5to8 + windSpeedNibble9to12;

  unsigned char calculatedChecksum = (windDirectionNo + windSpeedNibleSum) & 0x0f;

  // Determines the checksum given by the anemometer
  unsigned int checksum = (anemometerDatagram[24] << 3) + (anemometerDatagram[23] << 2) + (anemometerDatagram[22] << 1) + anemometerDatagram[21];
  checksum = (~checksum) & 0x0f;

  // Checks to see if both the checksum and the calculated checksum match
  if (checksum != calculatedChecksum) {
    Serial.println("\nFailed to read from anemometer |Checksum Error|");
    Serial.println("Checksum: " + String(checksum) + " != " + String(calculatedChecksum));
  }
}

// Determines the wind direction as a compass measurement from the corresponding number
void translateWindDirection() {
  switch (windDirectionNo) {
    case 0:
      windDirection = "N";
      break;
    case 1:
      windDirection = "NNE";
      break;
    case 2:
      windDirection = "NE";
      break;
    case 3:
      windDirection = "ENE";
      break;
    case 4:
      windDirection = "E";
      break;
    case 5:
      windDirection = "ESE";
      break;
    case 6:
      windDirection = "SE";
      break;
    case 7:
      windDirection = "SSE";
      break;
    case 8:
      windDirection = "S";
      break;
    case 9:
      windDirection = "SSW";
      break;
    case 10:
      windDirection = "SW";
      break;
    case 11:
      windDirection = "WSW";
      break;
    case 12:
      windDirection = "W";
      break;
    case 13:
      windDirection = "WNW";
      break;
    case 14:
      windDirection = "NW";
      break;
    case 15:
      windDirection = "NNW";
      break;
  }
}


double json_builder() 
{
  JsonObject& weather = jsonBuffer.createObject();

  weather["Temperature"] = String(BMP.readTemperature());
  weather["Wind Direction"] = String(windDirection);
  weather["Wind Speed"] = String(windSpeed);
  weather["Altitude"] = String(BMP.readAltitude());
  weather["Pressure"] = String(BMP.readPressure());
  weather["Humidity"] = String(getDHTValue(INWARD_DHT_22, HUMIDITY));
  
  return weather.prettyPrintTo(Serial);
  
}

// Outputs the most recent meteorological data to the serial monitor
void displayLastReading() {
  Serial.println("DEBUG MODE!!!");
  Serial.println("");
  Serial.println("Wind Direction: " + String(windDirection));
  Serial.println("Wind Speed: " + String(windSpeed) + " m/s");
  Serial.println("Altitude: " + String(BMP.readAltitude()) + "");
  Serial.println("Temperature: " + String(BMP.readTemperature()) + " Celsius");
  Serial.println("Pressure: " + String(BMP.readPressure()) + " Pascal");
  Serial.println("Humidity (inside): " + String(getDHTValue(INWARD_DHT_22, HUMIDITY)));
  Serial.println("Humidity (outside): " + String(getDHTValue(OUTWARD_DHT_22, HUMIDITY)));
  Serial.println();
}

// System set-up
void setup() {
  Serial.begin(115200);
  if (!BMP.begin()) {
    Serial.println("Could not find the BMP180, check the wiring!");
    while (1) {}
  }

  // Start the DHT Modules
  INWARD_DHT_22.begin();
  OUTWARD_DHT_22.begin();

  // First Attempt at exporting the values to json
  // Using the arduinojson.org/assistant from the creator of the ArduinoJson library
  JsonObject& weather = jsonBuffer.createObject();
  
  pinMode(ANEMOMETER_PIN, INPUT); // Initialise anemometer pin as 'input'
  delay(1000);
}

// Main system loop
void loop() {
  currentTime = millis(); // Set current time (from when the board began)

  // If time since last reading is more than set interval, perform next reading
  if (currentTime - previousTime >= interval) {
    previousTime = currentTime;

    if (recieveDatagram() == true) { // Retrieves anemometer datagram
      yield();
      decomposeWindReading(); // Decomposed datagram into its parts: WindDirection and WindSpeed
      translateWindDirection(); // Gets the associated compass direction from its number
    }
    else if (recieveDatagram() == false){
      windSpeed = NULL; // Sets windSpeed to -50 aka NULL if no datagram recieved
      windDirection = "N/A"; // Sets windDirection to "N/A"
    }
    yield();
    
    
    // Perform when DEBUG flag is set to 'true'
    if (DEBUG == true) {
      displayLastReading();
      Serial.println("Testing the JSON library: ");
      json_builder();
    }
  }
  ESP.wdtFeed(); // Feed the watchdog timer to keep it content
  yield();
}
