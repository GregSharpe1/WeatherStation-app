#!/bin/bash

# This Application will be deploy to the /opt/ directory using git clone and jenkins

# This script is mainly to run the WeatherStation application as a service. 

# As spoken about within the usage() function of my WeatherStation.py file
# There are two ways to run this script. One for testing (./WeatherStation.py -d | ./WeatherStation.py --debug)
# DEBUG will just be used to print the sensory information to the terminal.

# EXPORT THE CURRENT CONFIG IF EXISTS
[[ -r /opt/WeatherStation-app/config ]] && \
    source /opt/WeatherStation-app/config

# And the below option is to push the sensory information to AWS IoT using the private key, root ca endpoint and certificate

# During the deployment I will export the path of the privatekey, rootca, endpoint and certificate. 
/usr/bin/python2 /opt/WeatherStation-app/WeatherStation.py -e ${ENDPOINT} -r ${ROOTCA_KEY_PATH} -c ${CERTIFICATE_KEY_PATH} -k ${PRIVATE_KEY_KEY_PATH}

# Complete
