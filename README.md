# Display Server
Updates a text table in the display with data from socket connection.
The client must connect to the raspberry IP with a TCP socket, then 
send a formated string with at least one of the measurements to be 
displayed, and its ID, where ID is an integer greater than zero.

```
IDxx
Txxx.xxxx
Hxxx.xxxx
Pxxx.xxxx
```
T (temperature), H (humidity), P (pressure) must use S.I. units, with
up to 6 digits plus the dot. The resulting data will be displayed as
shown in figure 1.

![Figure 1](display_layout_v0.png)

## Setup
Add em `/boot/config.txt`
```
    dtparam=spi=on

    # ST7735R framebuffer driver
    dtoverlay=adafruit-st7735r:rotate=90,cs=0,dc_pin=24,reset_pin=25,speed=16000000
```

| ST7735 pin  | RP. 2W pin header | 
|:------------|:-----------------:| 
| VCC         |         1         | 
| GND         |         6         | 
| CS          |        24         |
| RESET / RST |        22         |
| DC (A0)     |        18         | 
| SCK         |        23         | 
| SDA         |        19         |
| LED         |        12         |

Reboot and look for kernel load of the driver, with
`$ dmesg | grep fb` or `$ dmesg | grep -i st77` on command line.
If the display is working, then it will also appear as
a framebuffer on `/dev/fb*` and therefore, may be used
as terminal display for the device adding in `/boot/config.txt`:
```
    # map a console to TFT display
    fbcon=map:0
    fbcon=font:VGA8x8
    fbcon=rotate:1

    # Define resolution
    dframebuffer_width=160
    framebuffer_height=128
```