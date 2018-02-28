#!/usr/bin/env python2
import sys
import getopt
import datetime
import json
import time
import serial
import glob
import logging

from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient

# Set the default DEBUG_MODE to false
DEBUG_MODE = False
# TODO: Shall I base the topic on the location of the device???
# Maybe: WeatherStation-{location}??
DEFAULT_TOPIC = "WeatherStation"
# ESP8266 Attached
ESP8266_BRAD_RATE="115200"

# Create and configure logger
LOG_FILE = "/home/pi/weather_station_test.log"
LOG_FORMAT = "%(levelname)s %(asctime)s - %(message)s"
# Setup a basic configuration logger with a DEBUG level.
# This means that messages will get written with a level 20 or above.
# Examplained well here: https://www.youtube.com/watch?v=g8nQ90Hk328
logging.basicConfig(filename = LOG_FILE, level = logging.DEBUG, format = LOG_FORMAT)
# Root logger  
logger = logging.getLogger()

def usage():
    """Return the help/usage output for the WeatherStation application."""
    print "WeatherStation application - Greg Sharpe (gds2)"
    print
    print "Usage: ./WeatherStation.py -d (output the sensor information to the current window)"
    print "       ./WeatherStation.py -e {ENDPOINT} -r {ROOTCA} -c {CERTIFICATION} -k {PRIVATE KEY}"
    print "       ./WeatherStation.py -e {ENDPOINT} -r {ROOTCA} -c {CERTIFICATION} -k {PRIVATE KEY} -t {TOPIC_EXAMPLE}"
    print
    print
    print "Examples: "
    print "./WeatherStation.py -d | --debug"

    # Sys exit 0, meaning exit in a clean manor without and errors.
    sys.exit(0)

def main():
    """Take in the arguments and run the correct functions"""

    # The only allowed options will be to run ./WeatherStation.py --test or
    # ./WeatherStation.py -e ENDPOINT -r ROOTCA -c CERTIFICACTION -k PRIVATEKEY

    # All above options (endpoint, rootca, etc..) will be generated when creating a
    # device within AWS IoT. As this is the main part of the functionailty of the program
    # I will only have these messages.

    # If no arguements are used, print the usage function and exit
    argument_check = len(sys.argv[1:])
    if not argument_check:
        print "ERROR: No arguments selected, please see below."
        usage()
        sys.exit(0)

    # Attempt to locate the correct arguments 
    try:
        # use the dummy variable because pylint will throw and error.
        opts, dummy = getopt.getopt(sys.argv[1:], "de:r:c:k:t:",
                                    ["debug",
                                    "endpoint",
                                    "rootCA",
                                    "certificate",
                                    "privateKey",
                                    "topic"])
    # If the argument used within the command do not match one of the above 
    # (test, endpoint, etc.. or -t, -e, etc..) then print the error and the
    # usage function to the terminal
    except getopt.GetoptError as err:
        print str(err)
        usage()

    for opt, arg in opts:
        # Below would be a perfect example to use a switch case statement,
        # as python does not have a switch case statement (https://stackoverflow.com/questions/11479816/what-is-the-python-equivalent-for-a-case-switch-statement)
        # I have 'opted' to use a if/elif/else set of rules.

        if opt in ("-h", "--help"):
            # Output the usage function to act as a helper.
            usage()
        elif opt in ("-d", "--debug"):
            # Run the debug function (Print all readings to terminal)
            # And do not send any data to AWS.
            print_to_display()
        elif opt in ("-e", "--endpoint"):
            # I only want the global variable to be set if using this option
            global endpoint
            endpoint = arg
        elif opt in ("-r", "--rootca"):
            # I only want the global variable to be set if using this option
            global rootCA
            rootCA = arg
        elif opt in ("-c", "--certificate"):
            # I only want the global variable to be set if using this option
            global certificate
            certificate = arg
        elif opt in ("-k", "--privatekey"):
            # I only want the global variable to be set if using this option
            global privateKey 
            privateKey = arg
        elif opt in ("-t", "--topic"):
            # I only want the global variable to be set if using this option
            global topic
            topic = arg
        else:
            assert False, "ERROR: Unhandled option."

    # If the global variables have been set publish the results to AWS
    if 'rootCA' and 'endpoint' and 'privateKey' and 'certificate' in globals():
        # Log that the application has been run to send info to AWS.
        print "Publishing results to AWS."
        configure_and_publish_to_aws_iot()
        logger.info("Beginning to publish to AWS IoT.")
        logger.debug("Using: {} endpoint, {} privateKey path, {} certificate path and {} root CA path.".format(endpoint, privateKey, certificate, rootCA))
    else:
        print_to_display()

# Slightly modified from: https://stackoverflow.com/questions/12090503/listing-available-com-ports-with-python
def get_esp8266_serial_port():
    """ Find, Test and Return the correct port
        (Can only be a port connected with USB)

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        # As the device will only be connected via USB
        # List the USB ports
        port = glob.glob('/dev/ttyUSB*')
    else:
        raise EnvironmentError('Unsupported platform')

    # the glob function returns the port variable as a string
    # as we are only going to have one value EVER!
    # convert the 'list' it returns into a string by striping the 'leftovers'
    port = str(port).strip("'[]'")
    # Attempt to connect to the port
    try:
        s = serial.Serial(port)
        logger.info("Connected on port: {}".format(port))    
        s.close()
    except (OSError, serial.SerialException):
        pass
    return port

# The function below needs to read from a serial port the entire block of JSON
# Up until an new line.
def read_from_serial():
    """This function will take input from the ESP8266 device attached via USB"""
    # Let's first initialise the connect with Serial library
    serial_connection_to_esp8266 = serial.Serial(get_esp8266_serial_port(), ESP8266_BRAD_RATE)

    weather_readings_array = []
    # This function must run a while loop and break in the '\n' character is found
    while True:
        # Read the value
        value = serial_connection_to_esp8266.readline().strip('\n')
        # Append the value to an array if not '/n'
        # Begin reading if the value is '[['
        if "]]" in value:
            break
        else:
            weather_readings_array.append(value)
        
    # return the serial output line by line
    return weather_readings_array

def parse_serial_output():
    """This function will break down the input from the serial port
       and return a completed JSON object ready to be sent to the
       AWS IoT service
    """

    # Define global varaibles to store the readings
    # call them in here because I don't want these
    # variables declared without using this function.
    global temp_reading1
    global temp_reading2
    global humd_reading1
    global airp_reading1
    global rain_reading1
    global wind_dir_reading1
    global wind_spd_reading1

    weather_readings = read_from_serial()

    for i in weather_readings:
        # A messy if statement placing the readings into vars
        if "TEMP1" in i:
            temp_reading1 = strip_serial_output(i)
        elif "TEMP2" in i:
            temp_reading2 = strip_serial_output(i)
        elif "HUMD1"  in i:
            humd_reading1 = strip_serial_output(i)
        elif "AIRP1" in i:
            airp_reading1 = strip_serial_output(i)
        elif "RAIN1" in i:
            rain_reading1 = strip_serial_output(i)
        elif "WNDDIR1" in i:
            wind_dir_reading1 = strip_serial_output(i)
        elif "WNDSPD1" in i:
            wind_spd_reading1 = strip_serial_output(i)

def strip_serial_output(value):
    """This function will be used to strip the serial output
       into a usable format
       
       E.g TEMP1: 22.0 -> 22.0"""
    
    # First of all let's strip all white space and return characters
    temp_result = value.strip()
    # After removing whitre space and return characters
    # Remove everything before the ":" character.
    temp_result = temp_result.split(": ")[1]
    return temp_result

def build_json_object():
    """This function will initalize a JSON object holding weather information"""
    
    # Make sure the global vars are initalized
    parse_serial_output()
    
    readings = {}
    readings['Temperature (Indoor)'] = temp_reading1
    readings['Temperature (Outdoor)'] = temp_reading2
    readings['Humidity'] = humd_reading1
    readings['Air Pressure'] = airp_reading1
    readings['Rain Fall'] = rain_reading1
    readings['Wind Dir'] = wind_dir_reading1
    readings['Wind Speed'] = wind_spd_reading1

    # Return the readings in JSON format
    return json.dumps(readings)

def print_to_display():
    """A function for debugging purposes, will output all debug to screen."""

    print "ESP Port: " + get_esp8266_serial_port()
    print ""
    print "Reading from serial function return: " 
    print read_from_serial()
    print ""
    print "Parse serial output: "
    print parse_serial_output()
    print "Printing (and stripped) variables generated from above!!"
    print temp_reading1
    print temp_reading2
    print humd_reading1
    print airp_reading1
    print rain_reading1
    print wind_dir_reading1
    print wind_spd_reading1
    print ""
    print ""
    print "JSON Output: " 
    print build_json_object()

def configure_and_publish_to_aws_iot():
    """Configure the connection to AWS IoT service"""
    # See documentation: https://github.com/aws/aws-iot-device-sdk-python#id3
    # Connect to the AWS IoT service

    # After being a little confused as to why I need a client_id
    # found this: https://forums.aws.amazon.com/thread.jspa?threadID=219513
    # Therefore the client id is just used to uniquely identify the MQTT connection.
    IoTClient = AWSIoTMQTTClient("WeatherStation-aberystwyth")
    IoTClient.configureEndpoint(endpoint, 8883)
    IoTClient.configureCredentials(rootCA, privateKey, certificate)
    # log the files used
    logger.info("Using {} root path, {} certificate path and {} private key path.".format(rootCA, certificate, privateKey))
    
    # AWSIoTMQTTClient connection configuration
    IoTClient.configureAutoReconnectBackoffTime(1, 32, 20)
    IoTClient.configureOfflinePublishQueueing(-1)  # Infinite offline Publish queueing
    IoTClient.configureDrainingFrequency(2)  # Draining: 2 Hz
    IoTClient.configureConnectDisconnectTimeout(10)  # 10 sec
    IoTClient.configureMQTTOperationTimeout(5) # 5 sec

    # Using the AWSIoTMQTTClient's connect method
    logger.info("Attempting to connect to the AWS IoT Service.")
    IoTClient.connect()
    logger.info("Connected to the AWS IoT service.")

    while True:
        # Now convert the message to json in order to send it too AWS
        messageJson = build_json_object()
        # If the topic is not set use the DEFAULT_TOPIC vars
        if 'topic' in globals():
            IoTClient.publish(topic, messageJson, 1)
            # Call the logger
            logger.info("Published message: {} to AWS IoT with: {}".format(messageJson, topic))  
        else:
            IoTClient.publish(DEFAULT_TOPIC, messageJson, 1)
            # call the logger.
            logger.info("Published message: {} to AWS IoT with: {}".format(messageJson, DEFAULT_TOPIC))            

        # Wait for oen second
        logger.info("Waiting for one second")
        time.sleep(1)


main()
