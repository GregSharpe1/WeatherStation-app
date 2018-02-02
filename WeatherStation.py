#!/usr/bin/env python2
import sys
import getopt
import datetime
import json
# RaspberryPi specific
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient
import Adafruit_DHT as DHT
import Adafruit_BMP as BMP

def usage():
    """Return the help/usage output for the WeatherStation application."""
    print "WeatherStation application - Greg Sharpe"
    print
    print "Usage: ./WeatherStation.py -t (output the sensor information to the current window)"
    print "       ./WeatherStation.py -e {ENDPOINT} -r {ROOTCA} -c {CERTIFICATION} -k {PRIVATE KEY}"
    print
    print
    print "Examples: "
    print "./WeatherStation.py -t"

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
        # use the dummy variable because pylint will throw and error.abs
        opts, dummy = getopt.getopt(sys.argv[1:], "te:r:c:k:",
                                    ["test",
                                    "endpoint",
                                    "rootCA",
                                    "certificate",
                                    "privateKey"])
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
        elif opt in ("-t", "--test"):
            # Run the test function (Print all readings to terminal)
            # TODO: Add test function here
            # TODO: Consider the use of a debug function??
            print "TEST"
        elif opt in ("-e", "--endpoint"):
            # I only want the global variable to be set if using this option
            global endpoint
            endpoint = arg
        elif opt in ("-r", "-rootca"):
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
        else:
            assert False, "ERROR: Unhandled option."

main()                                    