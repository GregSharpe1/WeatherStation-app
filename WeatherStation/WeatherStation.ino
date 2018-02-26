 /*
    Weather Station

    Sketch for a Aberyswyth University, Computer Science, Major Project
    Retrieves meteorological information from an assorted range of sensors including;
    an anemometer, rain gauge, Air Pressure/Humidity/Temperature sensor (Indoor) BME280 and 
    an OUTER temperature sensor DS18B20.

    The data is outputted in a standard format and then passed onto a Raspberry
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
// DEBUG PRINT
// setup
// Loop

// Included libraries
#include <Adafruit_BME280.h>

// The below are for the outdoor temperature (DS18B20)
#include <OneWire.h>
#include <DallasTemperature.h>

// Debug flag
// When DEBUG is FALSE only the reading output shall be printed to Serial
// in order for the Raspberry Pi to read.
#define DEBUG false
int DEBUGCOUNTER = 0;

// OUTDOOR temperature sensor (http://www.hobbytronics.co.uk/ds18b20-arduino)
// Connected as:
// Green wire -> to pin
// Red wire -> to positive
// yellow wire -> to negative
#define OUTDOOR_TEMPERATURE_PIN D4
// Initalize a one wire object
OneWire oneWire(OUTDOOR_TEMPERATURE_PIN);
DallasTemperature sensors(&oneWire);

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

  if (DEBUG)
  {
    Serial.println("Duration was: " + String(duration));
  }

  
  // Anemometer transmits data every two seconds
  if (duration > 50000) {
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
  else if (DEBUG) {
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
double getIndoorTemperatureReading()
{
  return BME.readTemperature();
}

// Function to return humidity from BME280
double getHumidityReading()
{
  return BME.readHumidity();
}

////////////////////////////////////////// OUTDOOR TEMP (DS18B20) ////////////////////////////////////////////////

double getOutdoorTemperatureReading()
{
  // Incase I choose to later add more of these sensors
  // make sure we return the first instance of the DS18B20 reading.
  return sensors.getTempCByIndex(0);
}


////////////////////////////////////////////// DEBUG MODE ////////////////////////////////////////////////////

// Outputs the most recent meteorological data to the serial monitor
void displayLastReading() {

  // Increase the Debug counter every time I loop through this function
  if (DEBUG)
  {
    DEBUGCOUNTER++;
    Serial.print("Number of loops: ");
    Serial.println(DEBUGCOUNTER);
    Serial.println();
  }
  
  // The following will be printed to the serial monitor.
  // Has tp be in the following format.
  //
  // SENSOR(NUM)[COLON][SPACE][VALUE] 
  // This will ensure the values are read correctly within the python script
  Serial.println("TEMP1: " + String(getIndoorTemperatureReading()));
  Serial.println("TEMP2: " + String(getOutdoorTemperatureReading()));
  Serial.println("HUMD1: " + String(getHumidityReading()));  
  Serial.println("AIRP1: " + String(getAirPressureReading()));
  Serial.println("RAIN1: " + String(rainFall));
  Serial.println("WNDDIR1: "+ String(windDirection));
  Serial.println("WNDSPD1: "+ String(windSpeed));
  Serial.println("]]");
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

  // Set the anemometer pin to begin reading values
  pinMode(ANEMOMETER_PIN, INPUT); // Initialise anemometer pin as 'input'

  // Rain Gauge
  // Set the PIN to pull up mode
  pinMode(RAIN_GAUGE_PIN, INPUT_PULLUP);
  // Use the digitalPinToInterrupt function to convert the D7 pin to interrupt
  // Use this if cannot use that function for some reason: https://github.com/esp8266/Arduino/issues/584
  attachInterrupt(digitalPinToInterrupt(RAIN_GAUGE_PIN), handleRainGaugeInterrupt, FALLING);

  // Begin the dallas temp sensor using the sensor object
  sensors.begin();

  // In debug mode take the readings every 10 seconds instead of 30.
  if (DEBUG)
  {
    // Because the readings get taken every 2 seconds from the
    // Anenomemter I'm not sure how reducing the iterval will
    // effect the wind readings.
    readingInterval = 5000;
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

    // request a readding from the ds18b20 sensor
    sensors.requestTemperatures();

    // After the rain fall variable has been set resest the intterupt counter
    rainGaugeinterruptCounter = 0;
    
    displayLastReading();
  }
  ESP.wdtFeed(); // Feed the WDT
  yield();

}
