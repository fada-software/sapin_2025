#==============================================================================
#Projet : sapin de noel 2025 avec 77 LED WS2812B
#Date: 28/11/2025
#Author: fada-software
#IDE : Thonny
#OS : Micropython 1.26.1 for ESP32C3
#Upload with Thonny IDE : menu tools / options / interpreter / install micropython
#requirement : need python & esptool (pip install esptool)
#Upload main.py file file with Thonny
#==============================================================================
from machine import Pin #pour GPIO
import neopixel #pour LED WS2812, https://docs.micropython.org/en/latest/library/neopixel.html
import time #pour tempo time.sleep()

NUM_LEDS = 77 # 77 LED strip
BRIGHTNESS = 50
p = Pin(7, Pin.OUT)    # create output pin 7
n = neopixel.NeoPixel(pin=p, n=NUM_LEDS, bpp=3, timing=1)

while(True):
    for br in range(BRIGHTNESS):
        for i in range(NUM_LEDS):
            n[i] = (br, 0, 0)
        n.write()
        time.sleep(0.01)
    for br in range(BRIGHTNESS):
        for i in range(NUM_LEDS):
            n[i] = (0, br, 0)
        n.write()
        time.sleep(0.01)
    for br in range(BRIGHTNESS):
        for i in range(NUM_LEDS):
            n[i] = (0, 0, br)
        n.write()
        time.sleep(0.01)
    for br in range(BRIGHTNESS):
        for i in range(NUM_LEDS):
            n[i] = (br, br, br)
        n.write()
        time.sleep(0.01)
