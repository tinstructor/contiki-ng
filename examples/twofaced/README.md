# Dual-Radio Interface RPL (DRiPL) for Zolertia Firefly
This document describes the usage and configuration of the twofaced example for Contiki-NG. It is designed to work with Zolertia Firefly nodes. The purpose of the twofaced application is to evaluate our implementation of a 6TiSCH-compliant dual-interface routing protocol named DRiPL.

## Table of Contents

- [Dual-Radio Interface RPL (DRiPL) for Zolertia Firefly](#dual-radio-interface-rpl-dripl-for-zolertia-firefly)
  - [Table of Contents](#table-of-contents)
  - [Getting Started](#getting-started)
  - [Tips & Tricks](#tips--tricks)
    - [Custom Commands on Linux](#custom-commands-on-linux)
  - [Usage](#usage)
    - [Project Structure & Configuration](#project-structure--configuration)
      - [Physical Layer](#physical-layer)
      - [Link Layer](#link-layer)
      - [Network Layer](#network-layer)
      - [Transport Layer](#transport-layer)
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

### Custom Commands on Linux

I'll do you a favor and briefly explain how to create Linux commands in order to make your life much easier. First, create a folder that you can use to store all self-defined shell scripts (because including custom commands in `/usr/bin/` is just asking for trouble):

```bash
$ cd ~
$ mkdir bin
```

Add the `~/bin/` folder to your `PATH` variable by appending the following line to `~/.bashrc`:

```bash
export PATH="$PATH:$HOME/bin"
```

Ok so now that's out of the way we'll create a command that lists all connected (via USB) boards or "motes" supported by Contiki-NG. More specifically, the command we'll create leverages some funtionality provided by the `~/contiki-ng/tools/motelist/motelist.py` script but makes it a bit more user-friendly. First, create a file in `~/bin/`:

```bash
$ cd ~/bin/
$ touch motelist
```

Next, add the following lines to `motelist`:

```bash
#!/bin/bash
python3 ~/contiki-ng/tools/motelist/motelist.py 2>&1
```

Then, we give the file all rights imaginable because we're only partially pedantic about good practices and sometimes (read: often) we only pretend to care:

```bash
$ cd ~/bin/
$ sudo chmod 777 motelist
```

Finally, restart your shell or execute the following convenient command so that the `PATH` variable is updated and now also points to `~/bin/`:

```bash
$ exec bash
```

Now, if you plugin (via USB) a board supported by Contiki-NG and run the `motelist` command, it'll list all sorts of useful information:

```bash
$ motelist
Port          Serial          VID     PID     Product                    Vendor      
------------  --------------  ------  ------  -------------------------  ------------
/dev/ttyUSB0  ZOL-RM01-A0208  0x10C4  0xEA60  Zolertia RE-Mote platform  Silicon Labs
```

## Usage

This example may be used to achieve several goals, each of which implies a slightly different way in which to use it. Nonetheless, in any case, compiling this example for a certain (supported) platform results in the creation of binaries for two different kinds of RPL nodes: non-root nodes and root nodes. When compiling for the Zolertia Firefly (as follows), the make command output lists all files that are output in this process:

```bash
$ cd ~/contiki-ng/examples/twofaced
$ make distclean
$ make TARGET=zoul BOARD=firefly
...
LD        build/zoul/firefly/twofaced-node.elf
OBJCOPY   build/zoul/firefly/twofaced-node.elf --> build/zoul/firefly/twofaced-node.i16hex
SREC_CAT  build/zoul/firefly/twofaced-node.i16hex --> build/zoul/firefly/twofaced-node.hex
OBJCOPY   build/zoul/firefly/twofaced-node.elf --> build/zoul/firefly/twofaced-node.bin
CP        build/zoul/firefly/twofaced-node.elf --> build/zoul/firefly/twofaced-node.zoul
CP        build/zoul/firefly/twofaced-node.zoul --> twofaced-node.zoul
CC        twofaced-root.c
LD        build/zoul/firefly/twofaced-root.elf
OBJCOPY   build/zoul/firefly/twofaced-root.elf --> build/zoul/firefly/twofaced-root.i16hex
SREC_CAT  build/zoul/firefly/twofaced-root.i16hex --> build/zoul/firefly/twofaced-root.hex
OBJCOPY   build/zoul/firefly/twofaced-root.elf --> build/zoul/firefly/twofaced-root.bin
CP        build/zoul/firefly/twofaced-root.elf --> build/zoul/firefly/twofaced-root.zoul
CP        build/zoul/firefly/twofaced-root.zoul --> twofaced-root.zoul
rm twofaced-node.o twofaced-root.o build/zoul/firefly/twofaced-root.i16hex build/zoul/firefly/obj/startup-gcc.o build/zoul/firefly/twofaced-node.i16hex
```

If, for some reason, you need direct access to the appropriate binaries for (in this case) the Zolertia Firefly (e.g., because you want to use them in conjunction with simulated hardware through [Renode](#renode)), you can thus find them in the `build/zoul/firefly/` directory. More specifically, `twofaced-node.elf` is the binary file for a non-root RPL node (as implemented in `twofaced-node.c`), while `twofaced-root.elf` is the binary file for a RPL root node (as implemented in `twofaced-root.c`). A similar output build directory is created if you compile for another (supported) platform.

> More coming soon.

### Project Structure & Configuration

This project was written to enable trully multi-interfaced operation (specifically for RPL nodes but you could re-use the radio driver principles for other purposes just as well). In order for that to work, several additions / changes needed to be made to the Contiki-NG code base. Many of these changes closely align with the 3 bottom layers of the TCP / IP stack.

#### Physical Layer

Notice how `examples/twofaced/Makefile` contains the line `MODULES_REL += $(TARGET)/dev/twofaced-rf`. This means that when you execute the make command (for example) as `make TARGET=zoul BOARD=firefly`, the files contained in the directory `./zoul/dev/twofaced-rf` (notice this directory is relative w.r.t. the Makefile's directory) will be included during the build process. The purpose of this is to allow anybody to write a platform-specific radio driver for multi-interfaced operation.

>**Note:** make sure `NETSTACK_CONF_RADIO` is set to `twofaced_rf_driver` in `examples/twofaced/project-conf.h`

A twofaced-rf radio driver module generally consists of a single source file (`twofaced-rf.c`), wherein the driver is actually implemented, and a single header file (`twofaced-rf.h`), mainly serving to declare radio driver functions such that they can be called from anywhere within the driver module's source file. Now, the purpose of a twofaced-rf driver is merely to serve as an abstraction on top of multiple existing radio drivers such that they may be used with minimal modifications. For example, each board in the Zoul platform family (such as the Firefly or RE-Mote) possesses both a cc2538 and a cc1200 radio. As such, since identifiers that are not names of variables or functions (e.g., structure tags) have no linkage by default, we declared all the possible underlying radio drivers in the beginning of `examples/twofaced/zoul/dev/twofaced-rf/twofaced-rf.c` with the storage class specifier `extern` (such that they have external linkage and hence represent the same objects as the drivers declared in `arch/cpu/cc2538/dev/cc2538-rf.h` and `arch/dev/radio/cc1200/cc1200.c`, which are also presented to the linker by virtue of them being declared with the storage class specifier `extern`).

```c
/* The supported interface drivers */
extern const struct radio_driver cc2538_rf_driver;
extern const struct radio_driver cc1200_driver;
static const struct radio_driver *const available_interfaces[] = TWOFACED_RF_AVAILABLE_IFS;
```

The drivers / interfaces that are actually used by the twofaced-rf abstraction driver are then configurable by means of a macro called `TWOFACED_RF_CONF_AVAILABLE_IFS`. If you don't define it (for example in `examples/twofaced/project-conf.h`), the array of available interfaces will be initialized to `{ &cc2538_rf_driver, &cc1200_driver }`, meaning that both the cc2538 and cc1200 radio will be operational and the cc2538 radio (which is the first one in the list) shall start out as the preferred interface for all outgoing transmissions (more on that later).

>**Note:** since `available_interfaces[]` is an array of const-qualified pointers to const-qualified `struct radio_driver` objects, nothing about the array nor its contents is modifiable after initialization (by means of list-initialization in this case)

```c
#ifdef TWOFACED_RF_CONF_AVAILABLE_IFS
#define TWOFACED_RF_AVAILABLE_IFS TWOFACED_RF_CONF_AVAILABLE_IFS
#else /* TWOFACED_RF_CONF_AVAILABLE_IFS */
#define TWOFACED_RF_AVAILABLE_IFS { &cc2538_rf_driver, &cc1200_driver }
#endif /* TWOFACED_RF_CONF_AVAILABLE_IFS */
```

The actual abstraction provided by a `twofaced_rf_driver` is predominantly centered auround setting the pointer `selected_interface` to the radio driver / interface to be used for outgoing transmissions, after which the driver passes most (not all) non-abstraction specific function calls to the selected underlying driver, takes the return value obtained from the underlying function call, and returns that same value. For example, the `transmit` function of the Zoul platform's `twofaced_rf_driver` looks like this:

```c
static int
transmit(unsigned short transmit_len)
{
  return selected_interface->transmit(transmit_len);
}
```

Setting the selected interface of a twofaced-rf radio driver module can be achieved in two ways. Firstly, one could call the `twofaced_rf_driver`'s `set_value()` function and pass the `RADIO_PARAM_SEL_IF_ID` radio parameter with a value equal to the identifier (this is NOT the MAC address, which uniquely identifies the whole node and not its interfaces!) associated with the radio driver / interface you want to select. For example, `examples/twofaced/project-conf.h` sets the following interface identifiers (make sure they're unique and not 0):

```c
#define CC2538_CONF_INTERFACE_ID 1
#define CC1200_CONF_INTERFACE_ID 2
```

Secondly, one could also call the `twofaced_rf_driver`'s `set_object()` function, while passing the `RADIO_PARAM_SEL_IF_DESC` radio parameter and a string literal equal to the `driver_descriptor` of a certain `struct radio_driver` object. However, since this parameter was newly added to `struct radio_driver` in `os/dev/radio.h`, it is not neccessarily initialized in every existing definition of a `const struct radio_driver` object (since this must be explicitly added to the initialization list). Note that this is also the case for several other newly added `struct radio_driver` parameters which are typically only used for twofaced-rf abstraction drivers. As such, the cc2538's radio driver now looks like this:

```c
const struct radio_driver cc2538_rf_driver = {
  init,
  prepare,
  transmit,
  send,
  read,
  channel_clear,
  receiving_packet,
  pending_packet,
  on,
  off,
  get_value,
  set_value,
  get_object,
  set_object,
// The following are all newly added parameters:
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  "cc2538_rf_driver"
};
```

>**Note:** because of how list-initialization works, adding new parameters at the end of a struct declaration does not break existing functionality of other radio drivers of the type `struct radio_driver`. It does however mean that you need to put something (in our case all NULL pointers) at all positions in the initialization list prior to the last parameter you actually wish to initialize (or use member initialization instead).

To make sure you can't change the selected interface in between two radio driver operations that MUST be performed on the same underlying radio driver (such as, e.g., consecutive calls to `prepare()` and `transmit()`), we've introduced a mutex lock (have a look at `mutex_try_lock()` and `mutex_unlock()` in `os/sys/mutex.h`) that may be (try-) locked and unlocked by means of calling the `twofaced_rf_driver`'s `lock_interface()` and `unlock_interface()` functions respectively whenever you want to prevent the selected interface from changing in a certain region.

```c
const struct radio_driver twofaced_rf_driver = {
  init,
  prepare,
  transmit,
  send,
  read,
  channel_clear,
  receiving_packet,
  pending_packet,
  on,
  off,
  get_value,
  set_value,
  get_object,
  set_object,
  lock_interface,
  unlock_interface,
  channel_clear_all,
  receiving_packet_all,
  pending_packet_all,
  "twofaced_rf_driver"
};
```

> More coming soon.

#### Link Layer

Contrary to the generally accepted process for defining a new MAC layer in Contiki-NG (as defined [here](https://github.com/contiki-ng/contiki-ng/wiki/The-Contiki%E2%80%90NG-configuration-system#network-stack-mac-layer)), we do not set the `MAKE_MAC` variable to `MAKE_MAC_OTHER` in `examples/twofaced/Makefile`. Instead, we do the following:

```makefile
MAKE_MAC = MAKE_MAC_TWOFACED

ifeq ($(MAKE_MAC),MAKE_MAC_TWOFACED)
  MODULES_REL += net/mac/twofaced-mac
  CFLAGS += -DMAC_CONF_WITH_TWOFACED=1
endif
```

However, we still put the following in `examples/twofaced/project-conf.h`:

```c
#define NETSTACK_CONF_MAC twofaced_mac_driver
```

This allows us to perform preprocessor checks on `MAC_CONF_WITH_TWOFACED` rather than `MAC_CONF_WITH_OTHER` and includes the module required to run the twofaced mac protocol only when necessary. This MAC protocol is an adaptation of the CSMA MAC protocol with some multi-interface specific changes / additions.

> More coming soon.

This brings us to the way in which Contiki-NG keeps statistics about links to neighbors. More specifically, it keeps track of per-neighbor link-statistics by storing value entries of type `struct link_stats` in a Contiki-NG `NBR_TABLE` called `link_stats` (see `os/net/link-stats.c`). Upon successfully transmitting a (unicast) packet, the function `link_stats_packet_sent()` is called (defined in `os/net/link-stats.c` and declared in `os/net/link-stats.h`). Here, the link_stats table is checked for an existing entry keyed on the link-layer address of the neighbor to which the packet was sent. If no existing entry is found, a new entry in the link_stats neighbor table is added. If adding the new entry was succesful, a linked-list (consisting of structs of a certain format, see `os/lib/list.h`) named `interface_list`, which is a member of a `link_stats` struct object (and thus of each link-stats neighbor table entry), can be initialized. Next, whether or not the entry was newly added doesn't really matter, the `interface_list` of the given link-stats table entry (i.e., the one we found or otherwise newly added just now) is checked for an entry (of the type `struct interface_list_entry`) containing the identifier (this is NOT a MAC address) of the interface that was put in the packetbuffer attribute `PACKETBUF_ATTR_INTERFACE_ID` by a higher stack layer before transmitting (see [this wiki article](https://github.com/contiki-ng/contiki-ng/wiki/Documentation:-Packet-buffers) for more information on Contiki-NG buffers and how to use them). If such entry was found (i.e., `interface_list_entry_from_id()` didn't return `NULL`), the existing interface list entry is updated. Otherwise, when no interface list entry is found for the given interface identifier, a new entry is added if and only if the current number of entries in the link-stats table entry's interface list is less than `LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR`, i.e., the amount of interfaces each node in the network is expected to have (this constant is also used for normalized metric calculation etcetera).

>**Note:** a Contiki-NG `NBR_TABLE` is pretty straightforward once you get the hang of it. Have a look at `os/net/nbr-table.h`. If you don't speak fluent preprocessor hocus-pocus, don't worry. What it boils down to is that each entry in a neighbor table is keyed on the link-layer address of a neighbor and stores entries of a given type.

```c
void
link_stats_packet_sent(const linkaddr_t *lladdr, int status, int numtx)
{
  struct interface_list_entry *ile;
  struct link_stats *stats;
  ...
  stats = nbr_table_get_from_lladdr(link_stats, lladdr);
  if(stats == NULL) {
    /* If transmission failed, do not add the neighbor */
    if(status != MAC_TX_OK) {
      return;
    }
    /* Add the neighbor */
    stats = nbr_table_add_lladdr(link_stats, lladdr, NBR_TABLE_REASON_LINK_STATS, NULL);
    if(stats != NULL) {
      /* Init node-scope metric (irrelevant for twofaced) */
      ...
    } else {
      return; /* No space left, return */
    }
    LIST_STRUCT_INIT(stats, interface_list);
  }

  uint8_t if_id = packetbuf_attr(PACKETBUF_ATTR_INTERFACE_ID);
  ile = interface_list_entry_from_id(stats, if_id);
  uint16_t bad_metric = ...
  ...
  if(ile != NULL) {
    /* Update the existing ile */
    ...
    uint16_t old_metric = ile->inferred_metric;
    /* Set inferred metric to worse than threshold if no ACK was received */
    ...
    ile->inferred_metric = (status == MAC_TX_OK ? LINK_STATS_INFERRED_METRIC_FUNC() : bad_metric);
    ...
    /* When an inferred metric is not updated, or when it is but it doesn't
       cross the metric threshold in any direction, the link-layer may not
       update the corresponding defer flag */
    if(old_metric != ile->inferred_metric) {
      if(LINK_STATS_WORSE_THAN_THRESH(old_metric) &&
        !LINK_STATS_WORSE_THAN_THRESH(ile->inferred_metric)) {
        ile->defer_flag = LINK_STATS_DEFER_FLAG_FALSE;
        ...
      } else if(!LINK_STATS_WORSE_THAN_THRESH(old_metric) &&
                LINK_STATS_WORSE_THAN_THRESH(ile->inferred_metric)) {
        ile->defer_flag = LINK_STATS_DEFER_FLAG_TRUE;
        ...
      }
      /* It makes no sense to re-select the preferred interface if there's
         no change in inferred metric for the given interface (represented
         by the ile) */
      link_stats_select_pref_interface(lladdr);
    }
  } else {
    if(list_length(stats->interface_list) < LINK_STATS_NUM_INTERFACES_PER_NEIGHBOR) {
      /* Create new ile and add to interface list */
      ile = memb_alloc(&interface_memb);
      if(ile != NULL) {
        ile->if_id = if_id;
        /* Set inferred metric to worse than threshold if no ACK was received */
        ...
        ile->inferred_metric = (status == MAC_TX_OK ? LINK_STATS_INFERRED_METRIC_FUNC() : bad_metric);
        ile->weight = LINK_STATS_DEFAULT_WEIGHT;
        list_add(stats->interface_list, ile);
        ...
        link_stats_update_norm_metric(lladdr);
        link_stats_select_pref_interface(lladdr);
      } else {
        ...
        return;
      }
    }
  }

  /* Update last timestamp and freshness */
  ...
  /* Other stuff */
  ...
}
```

> More coming soon.

#### Network Layer

While technically redundant for the `MAKE_NET` variable (because it is configured like that by default in `Makefile.include`), we set the following make variables in `examples/twofaced/Makefile` (partially for the sake of clarity):

```makefile
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC
```

The choice for RPL-classic over RPL-lite has a lot to do with the feature-completeness of RPL-classic (which, frankly, is abominable, but RPL-lite is even worse **\*deep sigh to signify I'm deeply disappointed\***) and the fact that it supports all MOPs (instead of just non-storing mode). As you undoubtedly know, much of the functionality provided by RPL is governed by the objective function. Hence, a large part of our efforts towards truly multi-interfaced RPL nodes consisted of developing new objective functions. Much like with our multi-interface radio driver abstraction (well let's say we borrowed those principles from here) `os/net/routing/rpl-classic/rpl-dag.c` declares all possible objective functions a RPL node could run with the storage class specifier `extern`:

>**Note:** a RPL objective function that is declared without a storage class specifier automatically has external linkage because it is of type `rpl_of_t` and not `struct rpl_of`. Hence, `os/net/routing/rpl-classic/rpl-of0.c` for example, simply declares (and simultaneously defines) OF0 as `rpl_of_t rpl_of0 = {<init list>}`. However, in `os/net/routing/rpl-classic/rpl-dag.c` we must still use the `extern` keyword for reasons that currently escape me.

```c
extern rpl_of_t rpl_of0, rpl_mrhof, rpl_poof, rpl_driplof;
static rpl_of_t * const objective_functions[] = RPL_SUPPORTED_OFS;
```

The objective functions that may actually be used by a RPL node are then configurable by means of a macro called `RPL_CONF_SUPPORTED_OFS` (see `os/net/routing/rpl-classic/rpl-conf.h`). Put differently, the `RPL_CONF_SUPPORTED_OFS` macro (which should be defined as an array initialization list) configures the OFs supported by any node (root or non-root) at runtime. A node may only join a RPL instance (advertised in a DIO) which is based on one of the OFs in this list as indicated by the OCP in the DODAG Configuration option attached to a DIO. If you don't define it (for example in `examples/twofaced/project-conf.h`), the array of supported objective functions will be initialized to `{ &rpl_mrhof }`, meaning that a node will only understand MRHOF.

```c
#ifdef RPL_CONF_SUPPORTED_OFS
#define RPL_SUPPORTED_OFS RPL_CONF_SUPPORTED_OFS
#else /* RPL_CONF_SUPPORTED_OFS */
#define RPL_SUPPORTED_OFS {&rpl_mrhof}
#endif /* RPL_CONF_SUPPORTED_OFS */
```

Similarly, the OF that is disseminated through the network by root nodes (i.e., through the OCP field of DODAG Configuration options attached to DIOs) is configurable by means of a macro called `RPL_CONF_OF_OCP` (see `os/net/routing/rpl-classic/rpl-conf.h`). If you don't define it (for example, in `examples/twofaced/project-conf.h`), a node configured as root will set the OCP of DODAG Configuration options to `RPL_OCP_MRHOF` (i.e., 0x01). The OCP setting has no meaning to non-root nodes, as they'll run any OF advertised by the root as long as they recognize the corresponding OCP value and thus support a given OF.

```c
#ifdef RPL_CONF_OF_OCP
#define RPL_OF_OCP RPL_CONF_OF_OCP
#else /* RPL_CONF_OF_OCP */
#define RPL_OF_OCP RPL_OCP_MRHOF
#endif /* RPL_CONF_OF_OCP */
```

> More coming soon.

#### Transport Layer

When the execution flow (when transmitting a packet) inevitably ends up in `tcpip_ipv6_output()` (see `os/net/ipv6/tcpip.c`) and encounters an unconditional jump in the form of a `goto send_packet;` statement, the `uip_buf` flags are checked to see if the `UIPBUF_ATTR_FLAGS_ALL_INTERFACES` flag is set (see `os/net/routing/rpl-classic/rpl-icmp6.c` and `os/net/routing/rpl-classic/rpl-timers.c` for examples of when that happens). If it is not set, meaning the packet to be transmitted must be sent to the given `nbr` via the preferred interface for that neighbor, the link-stats table entry corresponding to said neighbor is retrieved and the therein contained preferred interface identifier (which is NOT a MAC address) is passed to the `UIPBUF_ATTR_INTERFACE_ID` attribute. As you undoubtedly know by now, the 6LoWPAN adaptation layer (see `os/net/ipv6/sicslowpan.c`) shall copy this attribute to the corresponding `packetbuf` attribute (i.e., `PACKETBUF_ATTR_INTERFACE_ID`) further down the line, after which the twofaced MAC protocol eventually calls a twofaced-rf radio driver abstraction's `set_value()` function passing the `RADIO_PARAM_SEL_IF_ID` radio parameter and a value copied from the `PACKETBUF_ATTR_INTERFACE_ID` attribute value (see `send()` in `examples/twofaced/net/mac/twofaced-mac/twofaced-mac.c` and `send_one_packet()` in `examples/twofaced/net/mac/twofaced-mac/twofaced-mac-output.c`) in order to change the currently selected interface.

>**Note:** the [access rules for Contiki-NG packet buffers](https://github.com/contiki-ng/contiki-ng/wiki/Documentation:-Packet-buffers) (amongst which `uip_buf` and `packetbuf`) state that `uip_buf` may only be accessed from the 6LoWPAN adaptation layer (see `os/net/ipv6/sicslowpan.c`) and above, while the `packetbuf` may only be accessed from the 6LoWPAN adaptation layer and below.

```c
send_packet:
  if(nbr) {
    linkaddr = uip_ds6_nbr_get_ll(nbr);
    if(!uipbuf_is_attr_flag(UIPBUF_ATTR_FLAGS_ALL_INTERFACES)) {
      const struct link_stats *stats = link_stats_from_lladdr((linkaddr_t *)linkaddr);
      if(stats != NULL) {
        ...
        uipbuf_set_attr(UIPBUF_ATTR_INTERFACE_ID, stats->pref_if_id);
      }
    }
  } else {
    linkaddr = NULL;
  }
  ...
  tcpip_output(linkaddr);
```

## Cooja

There are generally two ways to simulate experiments using the Cooja network simulator, i.e., by running Cooja in a Docker container or running it natively on Linux (tested for Ubuntu 18.04 LTS and should work on 20.04 LTS). The downside of the Docker approach is that you can't really modify the Cooja source itself (only the Contiki-NG codebase when installed as a bind mount). The downside of running Cooja natively is that not all node types are supported in a 64-bit version of Linux, which is most likely what you'll be running. While future implementations of this example will most likely rely on running Cooja natively, for now, the Docker approach suffices. Before you can run Cooja through Docker however, yo_u must first go through the installation process.

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

Now, before I explain the general usage of the provided scripts, you're going to define some custom Linux commands. More specifically, let's create two commands that'll allow you to either retrieve the recommended style changes to a file (`checkcrust`) or automatically apply these changes on disk (`fixcrust`). For safety's sake, I've limited the amount of arguments the `fixcrust` command accepts to just one file (or its location) instead of the space-delimited list of files (or their locations) it normally accepts (that is, the underlying script does). The `checkcrust` command only accepts a single argument (i.e., a code file or its location) as well. However, this is not a safety feature, but merely a limitation of the underlying script provided by Contiki-NG. Anyhow, we first create two files in `~/bin/`:

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

Then, we give these files all rights imaginable because, once again, we're only partially pedantic about good practices and sometimes (read: often) we only pretend to care:

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