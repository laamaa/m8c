# Audio setup for M8 headless 

Please note that the program includes SDL based audio routing built-in nowadays.

Experimental audio routing support can be enabled by setting the config value `"audio_enabled"` to `"true"`. The audio buffer size can also be tweaked from the config file for possible lower latencies.
If the right audio device is not picked up by default, you can use a specific audio device by using `"audio_output_device"` config parameter.

## Windows

* Right click Sound in Taskbar
* Open Sound Settings
* Select M8 (or whatever your Teensy Interface is named) as Input
* Select Device properties 
* Select Additional Device Properties 
* Select Listen tab 
* Check Listen to this device and then select output of your choice (whatever source plays your speakers/headphone uses)

## Linux / Raspberry Pi

### Pulseaudio

If you have a desktop installation, chances are you're using Pulseaudio which has a module for making an internal loopback.

run the following command in terminal:

```
pacmd load-module module-loopback latency_msec=1
```

after that, you should hear audio through your default input device. If not, check Input devices in Pulseaudio Volume Control
```
pavucontrol
```

If the tools are missing, install pavucontrol (Debian based systems):
```
apt install pavucontrol
```

### JACK

It is possible to route the M8 USB audio to another audio interface in Linux without a DAW, using JACK Audio Connection kit and a few other command line tools.

Please note that this is not an optimal solution for getting audio from the headless M8, but as far as I know the easiest way to use the USB audio feature on Linux. The best parameters for running the applications can vary a lot depending on your configuration, and however you do it, there will be some latency.

It is possible to use the integrated audio of the Pi for this, but it has quite a poor quality and likely will have sound popping issues while running this.

These instructions were written for Raspberry PI OS desktop using a Raspberry Pi 3 B+, but should work for other Debian/Ubuntu flavors as well.

Open Terminal and run the commands below to setup the system.

-----

#### First time setup

#### Install JACK Audio Connection Kit 

The jackd2 package will provide the basic tools that make the audio patching possible. 

```
sudo apt install jackd2
```

#### Add your user to the audio group

This is required to get real time priority for the JACK process.

```
sudo usermod -a -G audio $USER
```

You need to log out completely or reboot for this change to take effect.

-----

#### Find your audio interface ALSA device id

Use the ```aplay -l``` command to list the audio devices present in the system.

```
pi@raspberrypi:~ $ aplay -l
**** List of PLAYBACK Hardware Devices ****
card 0: Headphones [bcm2835 Headphones], device 0: bcm2835 Headphones [bcm2835 Headphones]
  Subdevices: 8/8
  Subdevice #0: subdevice #0
  Subdevice #1: subdevice #1
  Subdevice #2: subdevice #2
  Subdevice #3: subdevice #3
  Subdevice #4: subdevice #4
  Subdevice #5: subdevice #5
  Subdevice #6: subdevice #6
  Subdevice #7: subdevice #7
card 1: M8 [M8], device 0: USB Audio [USB Audio]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 2: vc4hdmi [vc4-hdmi], device 0: MAI PCM vc4-hdmi-hifi-0 [MAI PCM vc4-hdmi-hifi-0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 3: v10 [AudioQuest DragonFly Red v1.0], device 0: USB Audio [USB Audio]
  Subdevices: 1/1
  Subdevice #0: subdevice #0

```

Take note of the card number you wish to use. In my case, I want to use card  3: v10.

After these steps have been taken care of, we can try to route some audio.

#### Start JACK server and route audio
```
jackd -d alsa -d hw:M8 -r44100 -p512 &
```

This will start the JACK server using ALSA backend driver (-d alsa), open the M8 USB audio interface (-d hw:M8) with the sample rate of 44100hz (-r44100) and buffer period size of 512 frames (-p512). The program will run in background (the & character in the end specifies this).
If you don't see your terminal prompt, just press enter and it should appear.

Next, we will create a virtual port for routing audio to another interface. We need to specify the audio interface id where we want to output audio here.

```alsa_out -j m8out -d hw:<AUDIO INTERFACE NUMBER> -r 44100 &```

example:

```alsa_out -j m8out -d hw:3 -r 44100 &```

This creates a JACK client called "m8out" (-j m8out) that performs output to ALSA audio hardware interface number 3 (-d hw:3) with a samplerate of 44100 (-r 44100). The ampersand at the end runs the program in the background.

If you wish to use the Pi integrated sound board, check the number of the "bcm2835 Headphones" device. Usually it is 0.

After the virtual port has been created, we need to connect it to the M8 USB Audio capture device.

```
jack_connect system:capture_1 m8out:playback_1
jack_connect system:capture_2 m8out:playback_2
```

This will connect the audio ports. You should be able to hear sounds from the M8 now.

If you wish to shut down the applications, you can do that by sending a interrupt signal to the processes.

```killall -s SIGINT jackd alsa_out```


#### TL;DR
Run these in Terminal:

```
sudo apt install jackd2
sudo usermod -a -G audio $USER
sudo reboot

```
```
jackd -d alsa -d hw:M8 -r44100 -p512 &
alsa_out -j m8out -d hw:0 -r 44100 &
jack_connect system:capture_1 m8out:playback_1
jack_connect system:capture_2 m8out:playback_2
```
