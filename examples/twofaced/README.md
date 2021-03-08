# Dual-Radio Interface RPL (DRiPL) for Zolertia Firefly
This document describes the usage and configuration of the twofaced example for Contiki-NG. It is designed to work with Zolertia Firefly nodes. The purpose of the twofaced application is to evaluate our implementation of a 6TiSCH-compliant dual-interface routing protocol named DRiPL.

## Table of Contents

- [Dual-Radio Interface RPL (DRiPL) for Zolertia Firefly](#dual-radio-interface-rpl-dripl-for-zolertia-firefly)
  - [Table of Contents](#table-of-contents)
  - [Getting Started](#getting-started)
  - [Tips & Tricks](#tips--tricks)
  - [Usage](#usage)
    - [Project Structure](#project-structure)
  - [Cooja](#cooja)
  - [Renode](#renode)
    - [The Robot Framework](#the-robot-framework)
  - [Uncrustify](#uncrustify)
  - [Recommended Reads](#recommended-reads)

## Getting Started
The first step is to clone our remote repository to your local machine, updating all git submodules and checking out the `dripl` branch as follows:

```bash
$ cd ~
$ git clone https://github.com/tinstructor/contiki-ng.git
$ cd ~/contiki-ng/
$ git submodule update --init --recursive
$ git checkout dripl
```

If you'd like to adopt the proper git workflow (i.e., the forking workflow) for this project, according to the [Atlassian][git-workflow] git workflow tutorial, you'd first need to create your own remote repository (often named `upstream`) and local branch before pushing that branch to your remote and setting the newly created local branch to track the corresponding branch of the remote. The following code snippet shows how this is done.

>**Note:** there are shorter ways to achieve this, but the following snippet breaks it down into easy steps, sort off.


[git-workflow]: https://www.atlassian.com/git/tutorials/comparing-workflows/forking-workflow

```bash
$ cd ~/contiki-ng/
$ git remote add upstream <link to your remote>
$ git checkout -b <new branch>
$ git push upstream <new branch>
$ git branch -u upstream/<new branch>
```

The next step is to download the latest version of the ARM gcc compiler as follows:

```bash
$ cd ~
$ mkdir opt
$ cd opt/
$ wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2
$ tar xjf gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2
$ sudo chmod -R -w gcc-arm-none-eabi-10-2020-q4-major
$ rm gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2
```

Now open up `.bashrc`:

```bash
$ cd ~
$ sudo nano .bashrc
```

The compiler must then be added to the `PATH` environment variable. Add the following line to end the of the file:

```bash
export PATH="$PATH:$HOME/opt/gcc-arm-none-eabi-10-2020-q4-major/bin"
```

However, before you can actually call the compiler you need to either restart your shel or execute the following command:

```bash
$ exec bash
```

You can check whether intallation was succesfull by requesting the version of the compiler as follows:

```bash
$ arm-none-eabi-gcc --version
arm-none-eabi-gcc (GNU Arm Embedded Toolchain 10-2020-q4-major) 10.2.1 20201103 (release)
Copyright (C) 2020 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

The last few steps consist of installing some necessary packages:

```bash
$ sudo apt-get update
$ sudo apt-get install python3 python3-pip build-essential doxygen git curl wireshark python-serial srecord rlwrap pyserial uncrustify
```

When the Wireshark installer prompts you to say whether or not you want non-superusers to be able to capture packets select `"yes"` and finally add yourself to the `wireshark` group:

```bash
$ sudo usermod -a -G wireshark <user>
```

To be able to access the USB without using sudo, the user should be part of the groups plugdev and dialout.

```bash
$ sudo usermod -a -G plugdev <user>
$ sudo usermod -a -G dialout <user>
```

FINALLY you can start making changes to the code base. The twofaced application is located in `contiki-ng > examples > twofaced`. You might want to download [VSCode][vs-code] as a convenient IDE for browsing through the code base. It comes with features that are very usefull when coding, especially in terms of code completion.

>**Note:** for more information on the structure of the code base and how to modify it, have a look at the [Contiki-NG Wiki][contiki-ng].

[vs-code]: https://code.visualstudio.com/
[contiki-ng]: https://github.com/contiki-ng/contiki-ng/wiki

After a while, if you feel like the changes you've made are significant, you can always try and send us a merge request on GitHub (for the `dripl` branch). However, before you do that, make sure the changes you've made don't introduce conflicts. You may check this by pulling from the `dripl` branch on the `origin` remote and compiling + uploading. If the code doesn't compile or the flashed remote shows unexpected behavior, try and solve the underlying issue before sending a merge request. Also, it is important that you first [uncrustify your code](#uncrustify) before submitting a merge request! 

>**Note:** a good entry book explaning git (and the tools it has to resolve conflicts) is [Jump Start Git][jump-start-git] by Shaumik Daityari. More advanced topics are covered in the git bible, i.e., [Pro Git][pro-git] which may be downloaded free of charge but can be hard to read for complete beginners.

[jump-start-git]: https://www.sitepoint.com/premium/books/jump-start-git
[pro-git]: https://git-scm.com/book/en/v2

```bash
$ git pull origin dripl
$ cd ~/contiki-ng/examples/twofaced
$ make TARGET=zoul BOARD=firefly twofaced.upload
```

Alternatively, you may test the merged code (rebasing onto our dripl branch is also allowed and even preferred) by simulating the hardware in [Renode](https://renode.readthedocs.io/en/latest/). More about Renode in the [corresponding section](#renode).

## Tips & Tricks

I reckon you'll probably be working with Contiki-NG more often in the future. Hence you might as well make your life easier. For starters, since this application works with the Zolertia Firefly, you should make use of the Makefile savetarget command, which allows you to save the target platform (and optionally a specific board revision) to a Makefile.target file as follows:

```bash
$ make TARGET=zoul BOARD=firefly savetarget
```

From now on, when you perform any make command from within the `contiki-ng > examples > twofaced` directory which requires to you specify the flash target, the configuration stored in the Makefile.target file is used if you don't explicitely specify a target.

```bash
$ make
using saved target 'zoul'
  MKDIR     build/zoul/firefly/obj
  CC        ../../arch/platform/zoul/./platform.c
  CC        ../../arch/platform/zoul/dev/leds-arch.c
  CC        ../../os/dev/leds.c
  ...
```

>**Note:** Technically, the application probably works with other Zoul-based platforms with minimal changes to the code. The main difference will likely be in the pinout etc.

## Usage

Coming soon.

### Project Structure

The following files have been added or adapted outside the twofaced example filestructure and are hence of interest (more info coming soon):

[line 90-91](../../os/net/routing/rpl-classic/rpl.h#L90-L91) of `~/contiki-ng/os/net/routing/rpl-classic/rpl.h`

[line 73](../../os/net/routing/rpl-classic/rpl-dag.c#L73) of `~/contiki-ng/os/net/routing/rpl-classic/rpl-dag.c`

[line 165](../../os/net/routing/rpl-classic/rpl-private.h#L165) of `~/contiki-ng/os/net/routing/rpl-classic/rpl-private.h`

[the PO OF](../../os/net/routing/rpl-classic/rpl-poof.c) implemented in `~/contiki-ng/os/net/routing/rpl-classic/rpl-poof.c`

[the DRiPL OF](../../os/net/routing/rpl-classic/rpl-driplof.c) implemented in `~/contiki-ng/os/net/routing/rpl-classic/rpl-driplof.c`

[a link-table](../../os/net/link-table.c) implemented in `~/contiki-ng/os/net/link-table.c`

[the same link-table](../../os/net/link-table.h) configured in `~/contiki-ng/os/net/link-table.h`

The following files are part of the twofaced example filestructure and are hence of interest (more info coming soon):

[a MAC abstraction for DRiPL and PO](net/mac/twofaced-mac/twofaced-mac.c) implemented in `~/contiki-ng/examples/twofaced/net/mac/twofaced-mac/twofaced-mac.c`

[the same MAC abstraction for DRiPL and PO](net/mac/twofaced-mac/twofaced-mac.h) configured in `~/contiki-ng/examples/twofaced/net/mac/twofaced-mac/twofaced-mac.h`

[a cooja-specific dual-interface radio driver](cooja/dev/twofaced-rf/twofaced-rf.c) implemented in `~/contiki-ng/examples/twofaced/cooja/dev/twofaced-rf/twofaced-rf.c`

[the same dual-interface radio driver](cooja/dev/twofaced-rf/twofaced-rf.h) configured in `~/contiki-ng/examples/twofaced/cooja/dev/twofaced-rf/twofaced-rf.h`

[a zoul-specific dual-interface radio driver](zoul/dev/twofaced-rf/twofaced-rf.c) implemented in `~/contiki-ng/examples/twofaced/zoul/dev/twofaced-rf/twofaced-rf.c`

[the same dual-interface radio driver](zoul/dev/twofaced-rf/twofaced-rf.h) configured in `~/contiki-ng/examples/twofaced/zoul/dev/twofaced-rf/twofaced-rf.h`

The `target-conf.h` files in the BOARD-specific `~/contiki-ng/examples/twofaced/zoul` subdirectories are, as the name implies, configuration files for each supported / corresponding zoul board.

## Cooja

There are generally two ways to simulate experiments using the Cooja network simulator, i.e., by running Cooja in a Docker container or running it natively on Linux (tested for Ubuntu 18.04 LTS and should work on 20.04 LTS). The downside of the Docker approach is that you can't really modify the Cooja source itself (only the Contiki-NG codebase when installed as a bind mount). The downside of running Cooja natively is that not all node types are supported in a 64-bit version of Linux, which is most likely what you'll be running. While future implementations of this example will most likely rely on running Cooja natively, for now, the Docker approach suffices. Before you can run Cooja through Docker however, you must first go through the installation process.

The installation process (for Ubuntu) starts by [making sure there are no older versions of Docker installed](https://docs.docker.com/engine/install/ubuntu/#uninstall-old-versions). Next, it is recommended to install Docker through its repository [as explained here](https://docs.docker.com/engine/install/ubuntu/#install-using-the-repository). Following the installation of Docker, you must first make sure your user is added to the `docker` group and rebooting as follows:

```bash
$ sudo usermod -a -G docker <user>
$ sudo reboot
```

Then, you can pull the latest Docker Contiki-NG image:

```bash
$ docker pull contiker/contiki-ng
```

Finally you must append the following lines to `~/.bashrc`:

```bash
export CNG_PATH="$PATH:$HOME/contiki-ng"
alias contiker="docker run --privileged --sysctl net.ipv6.conf.all.disable_ipv6=0 --mount type=bind,source=$CNG_PATH,destination=$PATH:$HOME/contiki-ng -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v /dev/bus/usb:/dev/bus/usb -ti contiker/contiki-ng"
```

and running the following command:

```bash
$ exec bash
```

You are now ready to start simulating with Cooja! Running Cooja in a Docker container is as easy as typing:

```bash
$ contiker cooja
```

which creates a Docker container and opens Cooja. When exiting Cooja, the docker container will halt too. It will, however, not be removed and can be restarted if you wish. This also means that it takes up disk space though! Since the purpose of the Docker container is merely to provide all dependencies required to simulate and compile examples written for Contiki-NG, who's codebase is attached to a Docker container (that is, if it's created with the `contiker` command) as a bind mount, and nothing more, it makes no sense to leave halted containers in memory. Removing halted Docker containers can be done as follows:

```bash
$ docker container prune
```

>**Note:** A list of useful Docker commands is provided in [this cheatsheet](https://dockerlabs.collabnix.com/docker/cheatsheet/).

Anyhow, when you've opened a Docker container and started Cooja, you can open the provided simulation file by clicking on `file > Open simulation > Browse` (from the tab menus in the top-left corner) as shown in the following figure:

![cooja-tab](https://i.imgur.com/ZnRmFS6.png)

Next, browse to the `twofaced` example directory as follows:

![cooja-browse](https://i.imgur.com/msmEIut.png)

Then select the Cooja simulation (`.csc`) file named `cooja.csc` (which we've named according to the platform we've originally compiled for when writing this guide). Cooja will then ask to (re-)compile the latest binaries of the selected firmware (which you may have altered outside the Docker container in the meantime) for each type of "mote" present in the simulation file. The only thing you need to do right now is press `compile` and `create` (in that order) for every "mote" type pop-up.

![cooja-compile](https://i.imgur.com/tts80n7.png)

Upon pressing the `create` button for the last "mote" type pop-up (of which there'll be 2 in our case), the Cooja simulation window will open. This window contains a number of panels, among which the `Simulation control` and `Mote output` panel. The `Simulation control` panel allows you to start, pauze, stop, and reload the current simulation and gives an indication of the simulated time and the simulation speed (i.e., how much faster or slower the simulation is w.r.t. system speed). Press the `start` button, wait for say 15 simulation seconds, and hit `pauze`. Note that the `Mote output` panel contains the terminal output for each node in the simulation. Different nodes can be distinguished by means of their color in the `Mote output` panel and via their Node ID. The `Mote output` panel also allows you to save the combined terminal output (of all nodes in the simulation) to a separate `.txt` file (or append to an existing `.txt` file) for further (external) processing.

![cooja-simscreen](https://i.imgur.com/lT1WKIS.png)

## Renode

Renode is a pretty amazing embedded development tool that allows you to run unmodified binaries (i.e., those output by running GNU make for a given target) on virtualized hardware. What's more, it allows you to connect these virtualized development boards through a simulated wired or wireless medium. It's **ALMOST** perfect in every way. Unfortunately, even though it does support some rudimentary path loss models when connecting virtual boards over a wireless medium, it does not take collisions into account and should hence not be used to run large-scale experiments with the purpose of approximating real-life performance parameters such as Packet Delivery Ratio (PDR), end-to-end delay, etc., unless maybe for ultra-deterministic TDMA-based networks or something (but then PDR would equal 100% anyway). That said, for cost-saving and practicality reasons alone, Renode can be an invaluable part of your embedded toolkit (especially for unit-testing purposes, through its Robot framework).

>**Note:** Renode works with any binary file as long as a platform specification and script for the corresponding compilation target is provided. Put differently, it is OS-agnostic, meaning that you could just as well use it with binaries generated by compiling code from RIOT, OpenWSN, Zephyr, ARM Mbed, TinyOS, freeRTOS, and more. As long as you got the proper `.repl` and `.resc` files, you're game!

For most people, installing the portable version of Renode will suffice. It can do most things out of the box, except for compiling `.cs` (i.e., C#) files and running the Robot unit-testing framework (the latter of which can be enabled by installing separate dependencies). The following commands should take care of the installation. Also, I've included the installation of some GTK modules which don't really break anything important if you don't install them but you might as well get rid of some pesky warnings while you're at it.

>**Note:** If you're a Windows or MacOS user you can still install Renode following the instructions listed somewhere [in here](https://github.com/renode/renode/blob/master/README.rst). If you came here looking for instructions for how to install Renode on any platform other than Linux, I'm afraid you're SOL :)

```bash
$ cd ~
$ mkdir renode_portable
$ wget https://github.com/renode/renode/releases/download/v1.11.0/renode-1.11.0.linux-portable.tar.gz
$ tar xf renode-*.linux-portable.tar.gz -C renode_portable --strip-components=1
$ rm renode-*.linux-portable.tar.gz
$ sudo apt-get install libcanberra-gtk-module libcanberra-gtk3-module
```

Optionally, you may install the dependencies for the Robot framework. ~~Although we will not elaborate on it,~~ you can read more about this powerful testing framework [over here](https://renode.readthedocs.io/en/latest/introduction/testing.html).

>**Note:** We've added an extra subsection on using the Robot testing framework with Renode because it is just such a powerful testing tool that it will save you an eternity when you're running freshly modified firmware and trying to make sure everything still works prior to pushing to your remote or, better yet, filing a merge request!

```bash
$ cd ~/renode_portable
$ python3 -m pip install -r tests/requirements.txt
```

With that out of the way you can now start validating your code on virtualized hardware. Got ya! You're not ready just yet. First you need to update the `PATH` environment variable with the direction to `~/renode_portable/`. The process is the same as with the ARM compiler. First, open up `.bashrc`:

```bash
$ cd ~
$ sudo nano .bashrc
```

Then `~/renode_portable/` must be added to the `PATH` environment variable. Add the following line to end the of the file:

```bash
export PATH="$PATH:$HOME/renode_portable"
```

However, before you can actually call Renode you need to either restart your shel or execute the following command:

```bash
$ exec bash
```

Since Renode is fully documented [over here](https://renode.readthedocs.io/en/latest/index.html), we'll just get you going with a supplied example script written specifically to work with binaries compiled for the Zolertia Zoul-based Firefly platform. Before running a simulation, you must first compile this example for the approriate platform as follows:

```bash
$ cd ~/contiki-ng/examples/twofaced/
$ make TARGET=zoul BOARD=firefly
```

Next up, start renode from the shell. Running the renode command will start a Renode monitor window in the current working directory.

```bash
$ cd ~/contiki-ng/examples/twofaced/
$ renode
```

Finally the supplied simulation script (`twofaced.resc`) is run by executing the following command in the monitor window (when opened from `~/contiki-ng/examples/twofaced/` that is):

```bash
(monitor) s @twofaced.resc
```

The output should look something like this. You may recognize some of the terminal windows as the terminal output of a Contiki-NG node! That's because they're exactly that, the terminal of a virtualized Firefly node.

![renode](https://i.imgur.com/AbDSHam.png)

Stopping a simulation is as easy as asking the Renode monitor to quit:

```bash
(monitor) q
Renode is quitting
```

### The Robot Framework

If you've not yet installed the dependencies for the Robot testing framework then, in the immortal words of Arnold Schwarzenegger: "C'MON ... DO IT NOOOOW!", as follows:

```bash
$ cd ~/renode_portable
$ python3 -m pip install -r tests/requirements.txt
```

Next up, if you've downloaded Renode as [the Linux portable release](https://github.com/renode/renode/releases/download/v1.11.0/renode-1.11.0.linux-portable.tar.gz) (which you probably have if you've just been mindlessly following my instructions), we're going to modify it just a tiny bit on order to make it easier for you (but more importantly, me) to follow [the Renode docs for working with Robot](https://renode.readthedocs.io/en/latest/introduction/testing.html).

```bash
$ cd ~/renode_portable
$ mv test.sh renode-test
$ sudo chmod 777 renode-test
$ exec bash
```

>**Note:** you could always modify the access rights of files with `sudo chmod 755 <filename>` for safety if you actually care and / or are using a computer with a native Linux install instead of a basically disposable virtual machine like I am currently running

Since you've already added the `~/renode_portable/` directory to the `PATH` variable previously, what you've done is renamed the `test.sh` shell script and made it into a globally available command called `renode-test`. This command would've automatically been available had we installed Renode from a package instead. Anyhow, using the Robot framework with Renode is actually pretty straightforward if you know how to work with vanilla Renode (i.e., how to use `.resc` and `.repl` files) and hence, instead of boring you with details, we'll jump straight into executing the provided `.robot` script:

>**Note:** I realise that I forgot to mention you have to specify the Contiki-NG home directory on [line 11](tests/twofaced.robot#L11) of `twofaced.robot` before running it. Otherwise, the test will automatically fail (unless your username is also robbe and you happend to install Contiki-NG in `/home/robbe/contiki-ng`).

```bash
$ cd ~/contiki-ng/examples/twofaced
$ make distclean
$ make TARGET=zoul BOARD=firefly
$ cd tests/
$ renode-test twofaced.robot
Preparing suites
Started Renode instance on port 9999; pid 15697
Starting suites
Running twofaced.robot
+++++ Starting test 'twofaced.Should Talk Over Wireless Network'
+++++ Finished test 'twofaced.Should Talk Over Wireless Network' in 166.67 seconds with status OK
Cleaning up suites
Closing Renode pid 15697
Aggregating all robot results
Output:  /home/robbe/contiki-ng/examples/twofaced/tests/robot_output.xml
Log:     /home/robbe/contiki-ng/examples/twofaced/tests/log.html
Report:  /home/robbe/contiki-ng/examples/twofaced/tests/report.html
Tests finished successfully :)
```

>**Note:** obviously running the Robot script requires (as do vanilla Renode scripts) the appropriate binaries to be available in the `twofaced/build` folder. Since our test script was written to work with the Zolertia Firefly specifically, we've added the appropriate commands to compile the correct binaries.

If, for some reason, the test fails, opening the `twofaced/tests/log.html` file in your browser of choice can really tell you a lot about the origin of the supposed error. However, in order to understand most errors you probably need a basic understanding of how Robot scripts work. To this extent, [the Renode docs](https://renode.readthedocs.io/en/latest/introduction/testing.html) are a great starting point.

## Uncrustify

Before submitting merge requests of any kind, please make sure your code (and file-naming) adheres to [the code style guidelines of Contiki-NG](https://github.com/contiki-ng/contiki-ng/wiki/Code-style). A good way to rectify slopily written source code is to use `uncrustify` (a tool) with some style scripts provided by Contiki-NG. First off, install Uncrustify as follows:

```bash
$ sudo apt-get install uncrustify
```

Now, before I explain the general usage of the provided scripts I'll do you a favor and explain to you how to create some commands that'll make your life much easier. First, we'll create a folder that you can use to store all self-defined shell scripts (because including custom commands in `/usr/bin/` is just asking for trouble):

```bash
$ cd ~
$ mkdir bin
```

As always, add the `~/bin/` folder to your `PATH` variable by appending the following line to `~/.bashrc`:

```bash
export PATH="$PATH:$HOME/bin"
```

Ok so now that's out of the way we'll create two commands that'll allow you to either retrieve the recommended style changes to a file (`checkcrust`) or automatically apply these changes on disk (`fixcrust`). For safety's sake, I've limited the amount of arguments the `fixcrust` command accepts to just one file (or its location) instead of the space-delimited list of files (or their locations) it normally accepts (that is, the underlying script does). The `checkcrust` command only accepts a single argument (i.e., a code file or its location) as well. However, this is not a safety feature, but merely a limitation of the underlying script provided by Contiki-NG. Anyhow, we first create two files in `~/bin/`:

```bash
$ cd ~/bin/
$ touch checkcrust fixcrust
```

Next, add the following lines to `checkcrust`:

```bash
#!/bin/bash
~/contiki-ng/tools/code-style/uncrustify-check-style.sh $1 2>&1
```

and the following lines to `fixcrust`:

```bash
#!/bin/bash
~/contiki-ng/tools/code-style/uncrustify-fix-style.sh $1 2>&1
```

Then, we give these files all rights imaginable because we're only partially pedantic about good practices and sometimes (read: often) we only pretend to care:

```bash
$ cd ~/bin/
$ sudo chmod 777 checkcrust fixcrust
```

Finally, restart your shell or execute the following convenient command so that the `PATH` variable is updated and now also points to `~/bin/`:

```bash
$ exec bash
```

In the following example we added some innapropriate indentation on line 50 and used the `checkcrust` command to see if the supplied code file was properly formatted (which of course it wasn't). Upon seeying that the only flaw (identified by uncrustify, that is) is said indentation, we used the `fixcrust` command to rectify this error automatically.

![uncrustify](https://i.imgur.com/fuRqqvq.png)

>**Note:** As stated in [the official docs](https://github.com/contiki-ng/contiki-ng/wiki/Code-style), uncrustify is not some magic silver bullet and will sometimes misformat your code! Be aware of this! As such, it's probably better in most cases to only use `checkcrust` instead of `fixcrust`.

## Recommended Reads

- [**The Contiki-NG Wiki**](https://github.com/contiki-ng/contiki-ng/wiki): It is generally advisable to start out here. However, the documentation is really basic and leaves much to be desired, especially if you want to do more than just write applications on top of existing functionality.
- [**Practical Contiki-NG**](https://doi.org/10.1007/978-1-4842-3408-2) *by Agus Kurniawan*: After you've gone through the Wiki you'll have a lot of unanswered questions about how stuff actually works. This book can help you with a small part of those questions, but don't expect an in-depth explanation of anything PHY related. If you have a Shibboleth or Athens account via your research institution, you can even download a digital copy for free, legally!
- [**IEEE 802.15.4-2020**](https://standards.ieee.org/standard/802_15_4-2020.html): The currently active PHY + MAC layer standard for 802.15.4 networks. Although this is the official standard, many developers seem to have a total disregard for certain aspects of it. Especially on the Sub-GHz PHY layers, there seems to be a lot of confusion as to what is actually standardised and what is not. The fact that IEEE standards are not publicly available and otherwise very expensive to obtain doesn't help this confusion either.
- [**Jump Start Git**](https://www.sitepoint.com/premium/books/jump-start-git) *by Shaumik Daityari*: As mentioned before, if you don't speak Git, this book is where you might want to start your journey.
- [**C in a Nutshell**](http://shop.oreilly.com/product/0636920033844.do) *by Peter Prinz & Tony Crawford*: Although C is an incredibly forgiving language when it comes to getting what you want out of it (hence the abundance of terribly written but otherwise "functional" code), some parts of our source code contain more advanced features and contstructs from the C99 and later C11 specification. We make no mistake about it, seasoned embedded developers would probably have a heart-attack looking at parts of our code, but the important thing to remember is that we're well on our way to write better source code and this book is what's getting us there.
- [**The Renode docs**](https://renode.readthedocs.io/en/latest/): pretty elaborate documentation of Renode and the functionality it provides. Is sufficient for most use cases but lacks when it comes to scenarios requiring heavy modification, let alone the creation of, Renode source files.