# Raw Log Subdirectory
Put the raw logs obtained from your nodes in this directory and run the following commands from the `contiki-ng-relsas > examples > ranger` parent directory of this subdirectory:

```bash
$ python3 logbuilder.py <receive log>.log <transmit log>.log
$ python3 analyzer.ng <receive log>.log <transmit log>.log <csv filename>.csv <node info>.txt <los info>.txt
```

For more information on how to use the scripts you may pass the `-h` option when calling the python scripts like so:

```bash
$ python3 logbuilder.py -h
usage: logbuilder.py [-h] rxlog txlog

positional arguments:
  rxlog       The logfile to which all received messages need to be copied.
  txlog       The logfile to which all transmitted messages need to be copied.

optional arguments:
  -h, --help  show this help message and exit
$ python3 analyzer-ng.py -h
usage: analyzer-ng.py [-h] rxlog txlog csvfile nodeinfo losinfo

positional arguments:
  rxlog       The logfile containing all received messages.
  txlog       The logfile containing all transmitted messages.
  csvfile     The csv file to which log-derived info must be appended.
  nodeinfo    Configuration file for nodes.
  losinfo     Configuration file for los conditions.

optional arguments:
  -h, --help  show this help message and exit
```
Each line in the node configuration file to be passed to the analyzer-ng.py script has to be formatted a certain way, for example:

```
node-info: 0012.4b00.09df.540e, A1, 2003, lorem_ipsum, 27.4, 250, 183, 201
node-info: 0012.4b00.09df.540c, A2, 2007, lorem_ipsum, 24.9, 187, 102, 203
node-info: 0012.4b00.09df.8ee8, B1, 1999, lorem_ipsum, 26.1, 301, 267, 198
```

As you can see each line starts with the string `"node-info:"` followed by the link address of said node, its corresponding id, the absolute height of the node in millimeter, the antenna type, the temperature at that node's location in °C and the x, y and z coordinates of the node (again, in millimeter) with respect to the grid origin.

Similarly, each line in the line-of-sight (los) configuration file to be passed to the analyzer-ng.py script has to be formatted a certain way, for example:

```
los-info: A1, A2, yes
los-info: A1, B1, yes
los-info: A2, B1, no
```

Here, each line starts with the string `"los-info:"` followed by the id of two nodes in the grid, followed by a string that indicates whether or not there are line-of-sight conditions between these nodes. If you where to enter a duplicate combination of 2 nodes (regardless of their respective order), all **but** the first combination are discarded.