# Advanced Ranger for Zolertia Remote (rev-b)
This document describes the usage and configuration of the ranger example for Contiki-NG. It is designed to work with Zolertia remote-revb nodes. The purpose of the ranger application is to gather as much information about upstream sub-GHz 802.15.4 transmissions in a location within a certain area as possible in a convenient fashion. In other words, a node placed at a predetermined location broadcasts a number of messages that may or may not be received by a number of fixed anchor nodes. From the received packets, the anchor nodes can infer a number of properties about the transmission channel, such as: RSSI, LQI, packet error rate, etc. 

## Table of Contents
- [Advanced Ranger for Zolertia Remote (rev-b)](#advanced-ranger-for-zolertia-remote-rev-b)
  - [Table of Contents](#table-of-contents)
  - [Getting Started](#getting-started)
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

## Usage
Coming soon