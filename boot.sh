#!/bin/bash

#todo Connect to internet through 4G modem

sleep 30
./pins_setup.sh
sh -c '/home/debian/Gokart_CAN_API/Onboard_CAN_MQTT_Client/CANsniff.py & \
	/home/debian/Gokart_CAN_API/Power_off/Pocket_shutdown \
	/home/debian/Gokart_CAN_API/GPS_USB_NMEA_PARCER/Debug/GPS_USB_NMEA_PARCER'
