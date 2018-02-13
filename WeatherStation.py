#!/usr/bin/env python2
import sys
import getopt
import datetime
import json
import time
import serial
import glob

from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient

# Set the default DEBUG_MODE to false
DEBUG_MODE = False
# TODO: Shall I base the topic on the location of the device???
# Maybe: WeatherStation-{location}??
DEFAULT_TOPIC = "WeatherStation"
# ESP8266 Attached
ESP8266_BRAD_RATE="115200"

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
        s.close()
    except (OSError, serial.SerialException):
        pass
    return port


def read_from_serial():
    """This function will take input from the ESP8266 device attached via USB"""
    # Let's first initialise the connect with Serial library
    serial_connection_to_esp8266 = serial.Serial(get_esp8266_serial_port(), ESP8266_BRAD_RATE)
    # return the serial output line by line
    return serial_connection_to_esp8266.readlines()


def get_sensor_value(attribute):
    """A function to return values based on function input."""
    # Assuming the JSON Object is laid out like:
    # { 
    #     "temperature": "20.0"
    #     "humidity": "20.0"
    #     "air pressure": "20.0"
    #     "altitude": "20.0"
    #     "wind": {
    #         "speed": "20.0",
    #         "direction": "NNW"
    #         }
    # }
    # Read from the above returning a value.
    with open("weather_example.json") as json_data_file:
        data = json.load(json_data_file)
        attribute_val = data.get(attribute)
    return attribute_val


def configure_aws_iot_connection():
    """Configure the connection to AWS IoT service"""
    # See documentation: https://github.com/aws/aws-iot-device-sdk-python#id3
    # Connect to the AWS IoT service

    # TODO: TESTING_CLIENT_ID Change me!
    IoTClient = AWSIoTMQTTClient("TESTING_CLIENT_ID")
    IoTClient.configureEndpoint(endpoint, 8883)
    IoTClient.configureCredentials(rootCA, privateKey, certificate)

    # AWSIoTMQTTClient connection configuration
    IoTClient.configureAutoReconnectBackoffTime(1, 32, 20)
    IoTClient.configureOfflinePublishQueueing(-1)  # Infinite offline Publish queueing
    IoTClient.configureDrainingFrequency(2)  # Draining: 2 Hz
    IoTClient.configureConnectDisconnectTimeout(10)  # 10 sec
    IoTClient.configureMQTTOperationTimeout(5) # 5 sec

    # Using the AWSIoTMQTTClient's connect method
    IoTClient.connect()

while True:
    print read_from_serial()