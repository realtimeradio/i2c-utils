#!/bin/bash

echo spidev > /sys/bus/spi/devices/spi0.0/driver_override
echo spi0.0 > /sys/bus/spi/drivers/spidev/bind 

echo spidev > /sys/bus/spi/devices/spi0.1/driver_override
echo spi0.1 > /sys/bus/spi/drivers/spidev/bind 

echo spidev > /sys/bus/spi/devices/spi0.2/driver_override
echo spi0.2 > /sys/bus/spi/drivers/spidev/bind 
