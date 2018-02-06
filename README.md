# Weather Station application back end

Written with Python2.7 in mind to deployed to a RaspberryPi (currenty tested on RPi3), this application takes sensory information from [BMP180](https://www.adafruit.com/product/1603) and [DHT22](https://www.adafruit.com/product/385) sensors and pushes them to AWS IoT using the private key, certification, endpoint and rootca path file provided by the AWS service when adding the 'Thing' to the platform. 

## Getting Started

1. Generate the above files (privatekey, certificate, etc..) by following this [LINK](https://docs.aws.amazon.com/iot/latest/developerguide/register-device.html) 
2. Git clone this repository to the `/opt/` directory on your Raspberry Pi 
3. Run ```./WeatherStation.py -e ${ENDPOINT} -k ${/PATH/TO/PRIVATEKEY} -r ${/PATH/TO/ROOTCA} -c ${/PATH/TO/CERTIFICATION}``` to start pushing sensory information to AWS IoT. 

## Prerequisites

All below packages are installed by using 
```
pip install ${package_name}
```

AWSIoTPythonSDK
Adafruit-Python-DHT
Adafruit-BMP

```
pip install -r requirements.txt
```

