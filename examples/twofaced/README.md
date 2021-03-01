# Dual-Radio Interface RPL (DRiPL) for Zolertia Firefly
This document describes the usage and configuration of the twofaced example for Contiki-NG. It is designed to work with Zolertia Firefly nodes. The purpose of the twofaced application is to evaluate our implementation of a 6TiSCH-compliant dual-interface routing protocol named DRiPL.

## Table of Contents

- [Dual-Radio Interface RPL (DRiPL) for Zolertia Firefly](#dual-radio-interface-rpl-dripl-for-zolertia-firefly)
  - [Table of Contents](#table-of-contents)
  - [Getting Started](#getting-started)
  - [Tips & Tricks](#tips--tricks)
  - [Usage](#usage)
  - [Renode](#renode)
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
$ sudo apt-get install build-essential doxygen git curl wireshark python-serial srecord rlwrap pyserial uncrustify
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

After a while, if you feel like the changes you've made are significant, you can always try and send us a merge request on GitHub (for the `dripl` branch). However, before you do that, make sure the changes you've made don't introduce conflicts. You may check this by pulling from the `dripl` branch on the `origin` remote and compiling + uploading. If the code doesn't compile or the flashed remote shows unexpected behavior, try and solve the underlying issue before sending a merge request. 

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

## Renode

Coming soon.

## Recommended Reads

- [**The Contiki-NG Wiki**](https://github.com/contiki-ng/contiki-ng/wiki): It is generally advisable to start out here. However, the documentation is really basic and leaves much to be desired, especially if you want to do more than just write applications on top of existing functionality.
- [**Practical Contiki-NG**](https://doi.org/10.1007/978-1-4842-3408-2) *by Agus Kurniawan*: After you've gone through the Wiki you'll have a lot of unanswered questions about how stuff actually works. This book can help you with a small part of those questions, but don't expect an in-depth explanation of anything PHY related. If you have a Shibboleth or Athens account via your research institution, you can even download a digital copy for free, legally!
- [**IEEE 802.15.4-2020**](https://standards.ieee.org/standard/802_15_4-2020.html): The currently active PHY + MAC layer standard for 802.15.4 networks. Although this is the official standard, many developers seem to have a total disregard for certain aspects of it. Especially on the Sub-GHz PHY layers, there seems to be a lot of confusion as to what is actually standardised and what is not. The fact that IEEE standards are not publicly available and otherwise very expensive to obtain doesn't help this confusion either.
- [**Jump Start Git**](https://www.sitepoint.com/premium/books/jump-start-git) *by Shaumik Daityari*: As mentioned before, if you don't speak Git, this book is where you might want to start your journey.
- [**C in a Nutshell**](http://shop.oreilly.com/product/0636920033844.do) *by Peter Prinz & Tony Crawford*: Although C is an incredibly forgiving language when it comes to getting what you want out of it (hence the abundance of terribly written but otherwise "functional" code), some parts of our source code contain more advanced features and contstructs from the C99 and later C11 specification. We make no mistake about it, seasoned embedded developers would probably have a heart-attack looking at parts of our code, but the important thing to remember is that we're well on our way to write better source code and this book is what's getting us there.