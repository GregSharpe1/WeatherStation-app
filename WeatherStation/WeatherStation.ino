 /*
    Weather Station

    Sketch for a Aberyswyth University, Computer Science, Major Project
    Retrieves meteorological information from an assorted range of sensors including;
    an anemometer, rain gauge, Air Pressure/Humidity/Temperature sensor (Indoor) BME280 and 
    an OUTER temperature sensor DS18B20.

    The data is outputted in a standard json format and then passed onto a Raspberry
    Pi Zero for process and eventually sent to AWS IoT Service.

    Created by Greg Sharpe
*/

// Where yield(); is called is because the ESP8266 module has alot of background
// tasks to compute, the yield() function is called within loop which may pause
// the background tasks. Yield simple tells the nodemcu module to carry on with
// the backgroud tasks, which in turn stops soft resets. 

// CODE BASE LAYOUT
// ALL defines/vars/inclues
// RAIN 
// Anenomemter
// BME280
// JSON
// DEBUG PRINT
// setup
// Loop

// Included libraries
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>

// Debug flag
// When DEBUG is FALSE only the JSON output shall be printed to Serial
// in order for the Raspberry Pi to read.
#define DEBUG false
int DEBUGCOUNTER = 0;

// Anemometer defines
#define ANEMOMETER_PIN D3
#define DATAGRAM_SIZE 41 + 5

// Initialise variables
// Setting the interval timers within the loop() function.
double readingInterval = 30000; // Set the interval to 30 seconds in order to take readings
unsigned long currentTime = 0; // Current Time
unsigned long previousTime = 0; // Previous current time

// Anemometer declare's
unsigned char anemometerDatagram[DATAGRAM_SIZE]; // Array to hold anemometer datagram
unsigned int windDirectionNo = 0; // Wind direction in bit value
String windDirection = ""; // Wind direction as compass direction
double windSpeed = 0; // Wind speed in metres per second

// Initialize Temperature, Humidity and Altitude sensors
// SCL from the BME280 to D1
// SDA from the BME280 to D2
/*
// Important! 
// Modified the below library and changed the I2C address to 0x76 from 0x77.
// As per comment here: https://arduinotronics.blogspot.co.uk/2017/06/esp8266-bme280-weather-station.html (bottom half of the page)
*/
Adafruit_BME280 BME;

// Initalize the JSON Buffer to 512 Characters
DynamicJsonBuffer jsonBuffer(512);

// RAIN GAUGE
#define RAIN_GAUGE_PIN D7

// Counts the amount of times the seesaw 'tips'
// This will get reset at the end of the loop function.
volatile byte rainGaugeinterruptCounter = 0;
// Ddebounce time of the rain gauge
long rainGaugeDebounceTime = 100;
// Declare the global variable to calculate the amount of rainfall
double rainFall = 0;
// Variables to temporary store the amount of rain fall.
double tempRainFallReading = 0;


////////////////////////////////////////////// RAIN GAUGE ////////////////////////////////////////////////////

// This function will increase the interrupt counter every time a interrupt is send to pin
// D7/13 (Interrupt pin). 
void handleRainGaugeInterrupt()
{
  currentTime = millis();
  if (currentTime - previousTime >= rainGaugeDebounceTime) 
  {
    previousTime = currentTime;
    // Increase the counter
    rainGaugeinterruptCounter++;
  }
}

double getRainFallReading()
{
  // This function will return the current rain fall measurement
  tempRainFallReading = rainGaugeinterruptCounter;
  // The measurement for 'tipping' the seesaw is 0.518mm of water
  // (according to the manual)
  // thus I will mulitple the amount of tempRainFallReading by 0.5
  // This isn't the most accurate method of calculating rain fall,
  // Maybe I will later investigate a better method.
  return tempRainFallReading * 0.5;
}

//////////]
//////////////////////////////////// WIND GAUGE ////////////////////////////////////////////////////

// Function that retrieves the datagram from the amemometer and places it into a char array
boolean recieveDatagram() {
 unsigned long duration = 0;

  while (digitalRead(ANEMOMETER_PIN) == HIGH)
  {
    yield();
    delayMicroseconds(1);
  }
  
  while (digitalRead(ANEMOMETER_PIN) == LOW)
  {
    yield();
    delayMicroseconds(1);
    duration++;
  }

  if (DEBUG == true)
  {
    Serial.println("Duration was: " + String(duration));
  }

  
  // Anemometer transmits data every two seconds
  if (duration > 20000) {
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

// Function that decomposes the datagram of an anemometer into its composed varaibles wind direction and wind speed
// **Modified from work by Pete Todd cht35@aber.ac.uk**
void get_anemometer_readings() {
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
  else if (DEBUG == true) {
    Serial.println("Printing Wind Speed values: ");
    Serial.println(windSpeedValue);
    windSpeed = double(windSpeedValue) / 10;
    Serial.println(windSpeed);
  }
}

// Determines the wind direction from the value returned from the Anenometer DATAGRAM
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

////////////////////////////////////////////// BME280 ////////////////////////////////////////////////////

// Function to return air pressure in "hPa" from BME280
double getAirPressureReading()
{
  return BME.readPressure() / 100.0F;
}

// Function to return temperature from BME280
double getTemperatureReading()
{
  return BME.readTemperature();
}

// Function to return humidity from BME280
double getHumidityReading()
{
  return BME.readHumidity();
}


////////////////////////////////////////////// JSON ////////////////////////////////////////////////////
double generate_json()
{
  //     Assuming the JSON Object is laid out like:
  //           { "location":
  //               { "Aberystwyth": { 
  //                   "temperature": "20.0"
  //                   "humidity": "20.0"
  //                   "air pressure": "20.0"
  //                   "altitude": "20.0"
  //                   "wind": {
  //                       "speed": "20.0",
  //                       "direction": "NNW"
  //                       }
  //                   }
  //               }
  //           }
  
  // First we'll create the JSON object with the name 'weather'
  JsonObject& weather = jsonBuffer.createObject();

  // Then we'll add the sensor values to the object
  weather["Temperature"] = String(BME.readTemperature());
  weather["Humidity"] = String(BME.readHumidity());
  weather["Rainfall"] = String(rainFall);
  weather["Pressure"] = String(getAirPressureReading());
  // Create the Nested Array "Wind"
  // Reference: https://arduinojson.org/api/jsonobject/createnestedobject/
  JsonObject& wind = weather.createNestedObject("Wind");
    wind["Direction"] = String(windDirection);
    wind["Speed"] = String(windSpeed);

  // In order to be interruptated by the Raspberry Pi zero, I will have
  // To print the above JSON object to SERIAL.
  return weather.prettyPrintTo(Serial);
}

////////////////////////////////////////////// DEBUG MODE ////////////////////////////////////////////////////

// Outputs the most recent meteorological data to the serial monitor
void displayLastReading() {

  // Increase the Debug counter every time I loop through this function
  DEBUGCOUNTER++;

  // I need to cleanup this functions
  Serial.println("DEBUG MODE!!!");
  Serial.println();
  Serial.print("Number of loops: ");
  Serial.println(DEBUGCOUNTER);
  Serial.println();
  Serial.println("Wind Direction: " + String(windDirection));
  Serial.println("Wind Speed: " + String(windSpeed) + " m/s");
  Serial.println("Temperature: " + String(getTemperatureReading()) + " Celsius");
  Serial.println("Pressure: " + String(getAirPressureReading()) + " hPa");
  Serial.println("Humidity: (BME280) " + String(getHumidityReading()) + " %");
  Serial.println("Rainfall: " + String(rainFall) + "mm");
  Serial.println();
}


////////////////////////////////////////////// SETUP ////////////////////////////////////////////////////

// System set-up
void setup()
{
  Serial.begin(115200);

  // DEBUGING PURPOSES
  if (DEBUG) 
  {
    Serial.println("BEGINNING SETUP");  
  }
  
  if (!BME.begin()) {
    Serial.println("Could not find the BME280, check the wiring!");
    while (true) 
    {
      // Stop the loop until the wiring is 'fixed'
      yield();
    }
  }

  // First Attempt at exporting the values to json
  // Using the arduinojson.org/assistant from the creator of the ArduinoJson library
  JsonObject& weather = jsonBuffer.createObject();

  // Set the anemometer pin to begin reading values
  pinMode(ANEMOMETER_PIN, INPUT); // Initialise anemometer pin as 'input'

  // Rain Gauge
  // Set the PIN to pull up mode
  pinMode(RAIN_GAUGE_PIN, INPUT_PULLUP);
  // Use the digitalPinToInterrupt function to convert the D7 pin to interrupt
  // Use this if cannot use that function for some reason: https://github.com/esp8266/Arduino/issues/584
  attachInterrupt(digitalPinToInterrupt(RAIN_GAUGE_PIN), handleRainGaugeInterrupt, FALLING);

  // In debug mode take the readings every 10 seconds instead of 30.
  if (DEBUG)
  {
    // Because the readings get taken every 2 seconds from the
    // Anenomemter I'm not sure how reducing the iterval will
    // effect the wind readings.
    readingInterval = 5000;
  }

  // DEBUGGING PURPOSES
  if (DEBUG) 
  {
    Serial.println("FINISHED SETUP");
  }
  // Delay for a second to make sure the sensors are active.
  delay(1000);

}

////////////////////////////////////////////// LOOP ////////////////////////////////////////////////////

// Main system loop
void loop()
{
  currentTime = millis(); // Set current time
 
  // If time since last reading is more than set interval, perform next reading
  if (currentTime - previousTime >= readingInterval) {
    previousTime = currentTime;

    if (recieveDatagram() == true)
    {
      get_anemometer_readings(); // Decomposed datagram into its parts: WindDirection and WindSpeed
      translateWindDirection(); // Gets the associated compass direction from its number
    }
    else if (recieveDatagram() == false)
    {
      windSpeed = -50.00; 
      windDirection = "N/A"; 
    }
    yield();
    // Return the amount of rain fall.
    // Setting the global variable here too much sure the result is  the same
    // when printing the in debug mode (displayLastReading() function will return rainFall,
    // Instead of getRainFallReading() function as the may change within that time.)
    rainFall = getRainFallReading();

    // After the rain fall variable has been set resest the intterupt counter
    rainGaugeinterruptCounter = 0;
    
    // Perform when DEBUG flag is set to 'true'
    if (DEBUG == true) {
      displayLastReading();
    }
    generate_json();
    // Because the return of the fucntion about can't add a println,
    Serial.println();
  }
  ESP.wdtFeed(); // Feed the WDT
  yield();
}
