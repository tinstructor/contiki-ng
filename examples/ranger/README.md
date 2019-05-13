# Advanced Ranger for Zolertia Remote (rev-b)
This document describes the usage and configuration of the ranger example for Contiki-NG. It is designed to work with Zolertia remote-revb nodes. The purpose of the ranger application is to gather as much information about upstream sub-GHz 802.15.4 transmissions in a location within a certain area as possible in a convenient fashion. In other words, a node placed at a predetermined location broadcasts a number of messages that may or may not be received by a number of fixed anchor nodes. From the received packets, the anchor nodes can infer a number of properties about the transmission channel, such as: RSSI, LQI, packet error rate, etc. 

## Table of Contents
- [Advanced Ranger for Zolertia Remote (rev-b)](#advanced-ranger-for-zolertia-remote-rev-b)
  - [Table of Contents](#table-of-contents)
  - [Getting Started](#getting-started)
  - [Tips & Tricks](#tips--tricks)
  - [Usage](#usage)

## Getting Started
The first step is to clone our remote repository to your local machine, updating all git submodules and checking out the `ranger-ng` branch as follows:

```bash
$ git clone https://gitlab.ilabt.imec.be/relsas/contiki-ng-relsas.git
$ cd contiki-ng-relsas/
$ git submodule update --init --recursive
$ git checkout ranger-ng
```

If you'd like to adopt the proper git workflow (i.e., the forking workflow) for this project, according to the [Atlassian][git-workflow] git workflow tutorial, you'd first need to create your own remote repository (often named `upstream`) and local branch before pushing that branch to your remote and setting the newly created local branch to track the corresponding branch of the remote. The following code snippet shows how this is done.

>**Note:** there are shorter ways to achieve this, but the following snippet breaks it down into easy steps, sort off.


[git-workflow]: https://www.atlassian.com/git/tutorials/comparing-workflows/forking-workflow

```bash
$ git remote add upstream <link to your remote>
$ git checkout -b <new branch>
$ git push upstream <new branch>
$ git branch -u upstream/<new branch>
```

The next step is to download the latest version of the ARM gcc compiler as follows:

```bash
$ wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/8-2018q4/gcc-arm-none-eabi-8-2018-q4-major-linux.tar.bz2
$ tar xjf gcc-arm-none-eabi-8-2018-q4-major-linux.tar.bz2
$ sudo chmod -R -w gcc-arm-none-eabi-8-2018-q4-major
```

Now open up `.bashrc`:

```bash
$ sudo nano /.bashrc
```

The compiler must then be added to the `PATH` environment variable. Add the following line to end the of the file:

```bash
export PATH="$PATH:$HOME/gcc-arm-none-eabi-8-2018-q4-major/bin"
```

However, before you can actually call the compiler you need to either restart your shel or execute the following command:

```bash
$ exec bash
```

You can check whether intallation was succesfull by requesting the version of the compiler as follows:

```bash
$ arm-none-eabi-gcc --version
arm-none-eabi-gcc (GNU Tools for Arm Embedded Processors 7-2018-q2-update) 7.3.1 20180622 (release) [ARM/embedded-7-branch revision 261907]
Copyright (C) 2017 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

The last few steps consist of installing some necessary packages:

```bash
$ sudo apt-get update
$ sudo apt-get install build-essential doxygen git curl wireshark python-serial srecord rlwrap
```

When the Wireshark installer prompts you to say whether or not you want non-superusers to be able to capture packets select `"yes"` and finally add yourself to the `wireshark` group:

```bash
$ sudo usermod -a -G <user>
```

To be able to access the USB without using sudo, the user should be part of the groups plugdev and dialout.

```bash
$ sudo usermod -a -G plugdev <user>
$ sudo usermod -a -G dialout <user>
```

FINALLY you can start making changes to the code base. The ranger application is located in `contiki-ng-relsas > examples > ranger`. You might wan't to download [VSCode][vs-code] as a convenient IDE for browsing through the code base. It comes with features that are very usefull when coding, especially in terms of code completion.

>**Note:** for more information on the structure of the code base and how to modify it, have a look at the [Contiki-NG Wiki][contiki-ng].

[vs-code]: https://code.visualstudio.com/
[contiki-ng]: https://github.com/contiki-ng/contiki-ng/wiki

After a while, if you feel like the changes you've made are substantial and usefull, you can always try and send us a merge request on GitLab (for the `ranger-ng` branch). However, before you do that, make sure the changes you've made don't introduce conflicts. You may check this by pulling from the `ranger-ng` branch on the `origin` remote and compiling + uploading. If the code doesn't compile or the flashed remote shows unexpected behavior, try and solve the underlying issue before sending a merge request. 

>**Note:** a good entry book explaning git (and the tools it has to resolve conflicts) is [Jump Start Git][jump-start-git] by Shaumik Daityari. More advanced topics are covered in the git bible, i.e., [Pro Git][pro-git] which may be downloaded free of charge but can be hard to read for complete beginners.

[jump-start-git]: https://www.sitepoint.com/premium/books/jump-start-git
[pro-git]: https://git-scm.com/book/en/v2

```bash
$ git pull origin ranger-ng
$ cd contiki-ng-relsas/examples/ranger
$ make TARGET=zoul BOARD=remote-revb ranger.upload
```

## Tips & Tricks

I reckon you'll probably be working with Contiki-NG more often in the future. Hence you might as well make your life easier. For starters, since this application works with but the Zolertia Remote rev-b, you should make use of the Makefile savetarget command, which allows you to save the target platform (and optionally a specific board revision) to a Makefile.target file as follows:

```bash
$ make TARGET=zoul BOARD=remote-revb savetarget
```

From now on, when you perform any make command from within the `contiki-ng-relsas > examples > ranger` directory which requires to you specify the flash target, the configuration stored in the Makefile.target file is used if you don't explicitely specify a target.

```bash
$ make
using saved target 'zoul'
  MKDIR     build/zoul/remote-revb/obj
  CC        ../../arch/platform/zoul/dev/tmp102.c
  CC        ../../arch/platform/zoul/./platform.c
  CC        ../../arch/platform/zoul/dev/leds-arch.c
  CC        ../../os/dev/leds.c
  ...
```

>**Note:** Technically, the application works with the Remote rev-a as well, with exception of the `"send pin"`. If you want to use pin interrupts on the rev-a to trigger a transmission you should provide the propper pin number assignment in a directory called `contiki-ng-relsas > examples > ranger > zoul > remote-reva` (you need to make this directory yourself).

## Usage

The basic usage of this example application is pretty straighforward. The file `contiki-ng-relsas > examples > ranger > ranger-constants.h` contains a number of macro definitions of integer constant expressions used to configure the way in which the application works. The `MAIN_INTERVAL` constitutes the duration (in seconds) between consecutive DATA mesage transmissions when a node is in TX mode. The `TX_DURATION` is the amount of time (in seconds) for which a node remains in TX mode (and hence transmits every `MAIN_INTERVAL` seconds) after a change of mode occured (from RX -> TX). The `CONTENT_SIZE` equals the payload length of a DATA message (in bytes) minus its UID, the message type and packet number. The `TX_POWER_DBM` and `CHANNEL` constant expressions identify the default TX power and channel respectively.

```c
#define MAIN_INTERVAL           6*(CLOCK_SECOND/10)
#define TX_DURATION             MAIN_INTERVAL*100
#define CONTENT_SIZE            28
#define TX_POWER_DBM            14
#define CHANNEL                 0
```

In normal operation, you press the user button of a node (in RX mode) for at least 5 seconds, which triggers a mode change to TX mode. During a time interval specified by `TX_DURATION` said node will then broadcast a DATA message each `MAIN_INTERVAL` seconds after which it changes back to RX mode once again and transmission terminates. A short press of the user button causes a node to change to a different radio configuration, indicated by a specific color of the RGB led.

>**Note:** the index of the pointer to the currently active radio configuration in the `rf_cfg_ptrs[]` array found in `ranger-constants.h` corresponds to the index of the color used to indicate said configuration in the `rf_cfg_leds[]` array (also found in `ranger-constants.h`).

You could then change the radio configurations of all nodes when no node is transmitting before starting transmission once again. Although it is not recommended, setting `ENABLE_CFG_REQ` to anything but `0` and short-pressing the user button, triggers a node to broadcast a configuration request (CFG_REQ) asking all receiving nodes to change to the radio configuration corresponding to the next index in the `rf_cfg_ptrs[]` array or the config with index zero if the index would exceed the amount of available radio configurations (`RF_CFG_AMOUNT`) minus 1. This is problematic however when some nodes don't receive the request, causing these nodes (with a different radio configuration) to become unreachable. In order to somewhat mitigate this issue, you can set the amount of CFG_REQ messages that are broadcasted at once with `BURST_AMOUNT`.

```c
#define ENABLE_CFG_REQ          0
#define ENABLE_SEND_PIN         0
#define ENABLE_UART_INPUT       0
#define BURST_AMOUNT            3
```

A feature that might prove more useful, if you'd want to use the WiLab testbed, is the option to enable serial input by setting `ENABLE_UART_INPUT` to anything but `0`. The following code snippet displays the available inputs and the resulting actions:

```c
static int uart_byte_input_callback(unsigned char input)
{
    switch (input)
    {
        case 'l':
            // equivalent to pressing the user button for > 5 seconds
            break;
        case 's':
            // equivalent to pressing the user button for < 5 seconds
            break;
        case 'r':
            // equivalent to pressing the reset button
        case 't':
            // trigger a readout of a connected tmp102 sensor
            break;
        default:
            break;
    }
    return 1;
}
```

Finally, you could also trigger the transmission of a DATA message by means of providing a rising edge on pin PA7 of the Remote (rev-b only), which triggers an interrupt and causes an ordinary DATA message to be broadcasted. This feature needs further testing and should not be considered reliable by any means.

For your convenience a python script (see `contiki-ng-relsas > examples > ranger > timestamper.py`) is provided that creates a logfile from the serial output passed to it (via a pipe). Creating a logfile with a name of your choice is done as follows:

```bash
$ make login | python3 timestamper.py -f <name of logfile>
Created logfile "<name of logfile>.log"
2019-05-13T10:58:28.600257 | 0:00:00.000046 | using saved target 'zoul'
2019-05-13T10:58:29.075912 | 0:00:00.475701 | rlwrap ../../tools/serial-io/serialdump -b115200 /dev/ttyUSB0
connecting to /dev/ttyUSB0 [OK]
2019-05-13T10:58:36.392570 | 0:00:07.792359 | Current RF config descriptor: 868MHz 2-FSK 1.2 kbps
2019-05-13T10:58:36.392751 | 0:00:07.792540 | csv-log: 0, 40, -111
2019-05-13T10:58:36.939890 | 0:00:08.339679 | Current RF config descriptor: 868MHz 2-FSK 1.2 kbps
2019-05-13T10:58:36.940415 | 0:00:08.340204 | csv-log: 1, 40, -114
2019-05-13T10:58:37.549070 | 0:00:08.948859 | Current RF config descriptor: 868MHz 2-FSK 1.2 kbps
2019-05-13T10:58:37.549526 | 0:00:08.949315 | csv-log: 2, 40, -113
```

>**Note:** a basic python script is also provided to analyze the logfile. The script may be called by providing the name of the logfile to be analyzed: `$ python3 analyzer.py <name of logfile>.log`. However, this script will undergo significant changes in the future to incorporate more advanced analysis of additional metrics etc., so don't expect it to work flawlessly.
