# BLEFloor

BLEFloor is an Arduino-powered WIFI/BLE connected thermostat 

## Warning

This hardware device is designed to switch/engage household voltages (100-230VAC). It has not been formally tested in any lab, nor does it carry any safety certification. While many precautions have been taken, always be careful around dangerous electricity as it is deadly and can kill you or others. In many jurisdictions, only electricians are allowed to connect to household voltages. Your mileage may vary, be careful!

## Hardware Description

The main controller is an Arduino Nano 33 IoT, which has an Atmel ARM Microcontroller attached to a uBlox Nina Wifi/BLE module (itself an ESP of some sort). A SI7020-A20 temperature/humidity sensor from Silicon Labs is attached, as well as a TPL5010 watchdog. The powersupply is a monolithic block from RECOM, the relay is from Omron, and the power path is fused and tranzorbed. 

### Theory of operation

A heated floor is controlled by a wax valve which melts over five minutes in the presence of heat. A heated floor thermostat is a bimetallic strip which can engage or disengage at a specific temperature. This device replaces the bimetallic strip with connected logic, such that it can be configured on a schedule, controlled remotely, make reports, etc. 

### Pictures

![Board Backside](pictures/board%20backside.jpg?raw=true)
![Board Front, No Arduino](pictures/board%20front%20side,%20without%20arduino.jpg?raw=true)


### Errata

The temperature sensor is not sufficiently isolated from the operating conditions of the board and as such when the relay is engaged it senses a 2-3 degree self-warming. This is currently compensated for in the firmware.

## Firmware description

The firmware is written using a recent Arduino IDE. 

The setup() function initializes the GPIO pins, sets up an interrupt handler for the watchdog, sets up I2C for the temperature sensor, and initializes communications with the NINA module.

Configuration settings are saved in a file on the NINA module, this requires the firmware on the NINA module to be updated to a recent release (tested with 1.4.3). The contents of the configuration are in the settings_t struct. The first-run defaults are defined in the defaults() function.

The main loop() function checks to see if the watchdog chip activated, and if so reads the current temperature and humity. If configured, these values are reported to an InfluxDB UDP collector. The remote firmware update query is serviced, the HTTP handler is serviced, and the heartbeat LED is serviced.

If rip and rport are configured, the device will report the current temperature, relative humidity, if the relay is engaged, and when it has rebooted over UDP.

The HTTP handler listens for GET and POST requests. GET requests bring up a simple page with the current state of the device, whereas POST accepts the following commands in a query string:

| command | argument | comment |
| ------- | -------- | ------- |
| ssid | wifi ssid to connect to | max length = 32 characters | 
| pass | password for wifi | max length = 32 characters |
| title | device name | max length = 32 characters |
| rip | reporting ip | address of influxdb in dotted format, max length 16 characters (though it should always fit in 13) | 
| rport | reporting port | port for influxdb udp |
| on | temperature in celcius below which the relay will engage | accepts numbers down to 5.0, anything beneath 5.0 will be raised to 5.0 |
| off | temperature in celcius above which the relay will disenage | accepts up to 50.0, anything above will be dropped to 50.0 |
| to | temperature offset | in case the reading differs from the actual room temperature, this will be applied before any other logic |
| save | n/a | instruct the thermostat to save the current configuration (must be last) |
| reset | n/a | instruct the thermostat to reboot | 
| relay | off/on | force the relay to (dis)engage |

Example Setting Config:

`   
  curl --data "ssid=My Access Point&pass=password!&rip=192.168.178.72&save=yes" 192.168.178.45
`  

### Pictures

![Web Interface](pictures/web%20interface.jpg?raw=true)

### Errata

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## License
Firmware:
[MPLv2](https://choosealicense.com/licenses/mpl-2.0/)

Hardware design files:
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)

