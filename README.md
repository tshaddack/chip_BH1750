Autogenerated from [https://www.improwis.com/projects/sw_chip_BH1750/](https://www.improwis.com/projects/sw_chip_BH1750/)






BH1750-read







BH1750-read
===========



---

[Why](#Why "#Why")  
[How](#How "How")  
      [chip](#chip "How.chip")  
      [code](#code "How.code")  
[Usage](#Usage "Usage")  
      [options](#options "Usage.options")  
      [output](#output "Usage.output")  
      [example use](#exampleuse "Usage.example use")  
            [examples](#examples "Usage.example use.examples")  
[Compiling](#Compiling "Compiling")  
[Files](#Files "Files")  
[TODO](#TODO "TODO")  


---

Why
---



The [BH1750](https://www.improwis.com/datasheets/BH1750 - I2C light sensor.pdf "local file - datasheet") is a fairly inexpensive [I2C](https://en.wikipedia.org/wiki/I2C "Wikipedia link: I2C") sensor of illumination. Its uses include simple automation tasks,
switching on or off lights (or infrared lamps for cameras) based on ambient light.




It often comes on breakout boards, eg. the [Adafruit one](https://learn.adafruit.com/adafruit-bh1750-ambient-light-sensor "remote link: https://learn.adafruit.com/adafruit-bh1750-ambient-light-sensor"). Adafruit also provides software, including python
libraries.




For reading from shell scripts, especially on underpowered boards like Raspberry Pi 1, python is less suitable due to the
overheads at process startup. Insane amount of files has to be read and interpreted. A frequently executed task will
bring a significant load on the machine.




It was also desired to get hands on the raw I2C device interface, and write a "scaffolding" code to modify for other sensors.





---

How
---



A simple C code was written based on fragments off the Net.




As the sensor doesn't do conventional registers, and only bare reads and writes are used (so eg. i2cdump will show just
all zeroes and confuse the register settings), the barest of interfacing will do. File and IOCTL access to /dev/i2c-something
was therefore chosen.



### chip



The chip operates in a simple way. Send a one-byte command. Or, read two-byte measured data.




The chip can operate in continuous reading mode, when the result register is periodically updated and always ready.
Or it can work in single-shot mode, where after command it measures (and it's needed to wait few 100s milliseconds), puts
data to register, and sleeps. Single-shot mode was chosen here.




There are three measurement modes. Normal, high-res, and high-res2 (twice the sensitivity). As the main applications will be
for indoor device control, high-res2 mode was chosen.




The measurement is done by integration of current through the photodiode, for a specified time. The time is set by a pair
of writes to a 8-bit value (69 by default). Higher value increases sensitivity, lower decreases acquisition time. Highest
value is 254, or 0xFE, achieving resolution down to 0.11 lx. Two writes are needed, as the chip accepts only one-byte writes;
the 8bit value is split to 3-bit and 5-bit parts, sent with fixed prefixes (0x40, 0x60) with the bits added (value&0x1F for
low 5 bits, value>>5 for high 3 bits).




Then there is a wait. 600 milliseconds were chosen for a compromise.




To calculate the 



### code



The code first opens the I2C device (or fails when device doesn't exist or is inaccessible).
IOCTL I2C\_SLAVE is executed on the resulting file handle, with the chip's address as parameter.




The integration time value is sent (code fails here if the chip is not on the given address).




The single-shot-read is sent.




Then couple hundred milliseconds wait for the result.




Then two bytes are read.




If the value is 0xFFFF, the measurement overflowed. In that case, the process is repeated with lower integration
value, up to 4 times. (Unless told to run in low-light mode, when the additional measurements are skipped;
we know we have more than 7417 lux, which is usually enough to know a lamp doesn't have to be switched.)

```
 #define BH1750_RES_MAX 254        // max output in mode 2 =   7417 lux
 #define BH1750_RES_DEFAULT 69     // max output in mode 2 =  27306 lux
 #define BH1750_RES_LOW 20         // max output in mode 2 =  94206 lux
 #define BH1750_RES_MIN 5          // max output in mode 2 = 376826 lux

```




In case of overflow even on the highest range, the value 999999.9 (or 1000000 in integer mode) is used.





---

Usage
-----


### options



If executed without options, it queries first BH1750 sensor on the /dev/i2c-1 bus. This is a default
configuration on raspberry pi boards.




```
BH1750 sensor read
Usage: bh1750 [-l] [-a0] [-a1] [-d <dev>] [-O] [-v] 
measure:
  -l        low light mode (just overflow to 999999 if over 7417 lx)
  -i        integer mode (no float)
  -i10      integer mode (no float), 10 times higher value
I2C:
  -a0       address ADDR=L (0x23, default)
  -a1       address ADDR=H (0x5c)
  -d <dev>  specify I2C device, default /dev/i2c-1
general:
  -O        force light register overflow (for test, use with -v)
  -v        verbose mode
  -h,--help this help


```
* measuring
+ -l - lowlight mode, do not iterate through higher values on overflow; "bright enough" mode
+ -i - integer mode, output an integer value, round off the decimals; for shell scripts that don't cope well with floats
+ -i10 - integer mode, output an integer value multiplied by 10 and round off the decimals; for shell scripts that don't cope well with floats and need low values

* I2C
+ -a0 - select the first chip on the bus (at 0x23), default
+ -a1 - select the second chip on the bus (at 0x5C)
+ -d /dev/i2c-something - specify the I2C device to access

* general
+ -O - force overflow on every read iteration; for tests/debugging
+ -v - verbose mode, report every read and device open; suitable for debugging
+ -h - help
+ --help - help; seriously, why every software has one or the other and never both?!?

### output



If all is okay, result code is 0. Otherwise, if something crashes, result is 1.




If not verbose or help, only the measured lux value is spit to stdout. Float with two decimal points, or a rounded-off integer.



### example use



```
lx=`bh1750 -i`
if [ $? == 0 ]; then
  if [ $lx -gt $limit ]; then switch_lights_off; else switch_lights_on; fi
else
  handle_sensor_error
fi

```

or, simpler, without sensor error handling:




```
if [ `bh1750 -i` -gt $limit ]; then switch_lights_off; else switch_lights_on; fi

```
#### examples


./bh17501.5
 
./bh1750 -i1




---

Compiling
---------



The code doesn't need any special compilation. A simple call will do:
* cc -o bh1750 bh1750.c

or, for statically linked version,
* cc -o bh1750 -static bh1750.c





---

Files
-----


* [bh1750.c](bh1750.c "local file") - source code
* [Makefile](Makefile "local file") - for easier compiling
* [bh1750-raspi](bh1750-raspi "local file") - binary for raspberry pi
* [bh1750-raspi-static](bh1750-raspi-static "local file") - binary for raspberry pi, statically linked



---

TODO
----


* support for [TCA9548A](https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout "remote link: https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout") I2C multiplexer (for up to 16 sensors, select by writing single byte to the mux address)
* derive more such utilities for other sensors






