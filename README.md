# LED-Strip Light Organ #

A light (or color) organ is an electronic device which converts an audio signal into rhythmic light effects. Invented in the 70's they are still popular in discotheques and party rooms though.
Now that LED strips became fairly cheap they can replace normal light bulbs used in the earlier years and make the color organ look more modern.

![github](https://github.com/yellobyte/LED-Strip-Light-Organ/raw/main/Doc/SequenceNormalMode.jpg)

The electronic circuit of a normal light organ separates the audio signal into several frequency bands and dimms the light bulbs or LEDs according to the average level of each band.
For a decent visual impression at least 3 channels are needed: low frequencies (bass), middle frequencies and high frequencies (treble). For best visual results each channel should controle a light sources of different color, e.g bass = red, middle = yellow and treble = green.

In our case we control 3 LED strips that are specified for 12V DC. The device can deliver a few amperes per channel, in my case the external power supply at hand (Mean Well GS90A12) can only deliver 6.7A DC so I set the limit to about 2.2A per channel using trimpots (trimmer potentiometer). The driver MOSFETs IRF540N operate way from their limits and only get handwarm.

Since different LED strips vary in their electrical parameters, their respective current ratings and the external 12V power supply you have available determine the length of the LED Strips you are able to use in the end.

The LED Strips I have tested with are equipped with 3528 SMD LEDs, have 60 LEDs per meter and are 5m long. They proved no problems at all and never triggered the electric overload circuitry.

The light organ has 3 working modes: [Normal](https://github.com/yellobyte/LED-Strip-Light-Organ/blob/main/Doc/Normal%20Mode.mp4), [Rhythm](https://github.com/yellobyte/LED-Strip-Light-Organ/blob/main/Doc/Rhythm%20Mode.mp4) and [Cyclic](https://github.com/yellobyte/LED-Strip-Light-Organ/blob/main/Doc/Cyclic.mp4), which can be selected via pressing the **mode selection** button.

## Block Diagram ##

For calculating the values of the 3 filter circuits the program **FilterLab** was used. It's easy and very intuitiv. An Arduino Nano 328P samples the respective filter output und transforms the voltage level in digital PWM signals which feed the PAs with their IRF540N Power MOSFETs. Overtemperature & electric overload (OL) protection has been integrated. 

The circuit had to be devided into 2 separate PCBs, a **Filter-PCB** and a **Power-PCB** as I only call the basic version of the Eagle Design Tool my own and therefore PCB size is limited to Euro card size. But this proved to be very fortunate in the end for I tried several different PA designs and didn't have to redo the filter part every time. Please have a look at folder **EagleFiles** for schematic & PCB details.

![github](https://github.com/yellobyte/LED-Strip-Light-Organ/raw/main/Doc/Block%20Diagram.jpg)


## How to calibrate ##

The pre-amplifier stage, the automatic gain control stage and all three filter circuits have to be calibrated in order to produce decent results. This procedure itself is fairly easy to be accomplished. Only obstacle is you need a function generator and ideally an oscilloscope. A cheap multimeter will do if you can´t get hold of the latter.



