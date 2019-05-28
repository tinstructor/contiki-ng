# Raw Log Subdirectory
Put the raw logs obtained from your nodes in this directory and run the following commands from the `contiki-ng-relsas > examples > ranger` parent directory of this subdirectory:

```bash
$ python3 logbuilder.py <receive log>.log <transmit log>.log
$ python3 analyzer.ng <receive log>.log <transmit log>.log -c <csv filename>.csv
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
usage: analyzer-ng.py [-h] [-c CSVFILE] rxlog txlog

positional arguments:
  rxlog                 The logfile containing all received messages.
  txlog                 The logfile containing all transmitted messages.

optional arguments:
  -h, --help            show this help message and exit
  -c CSVFILE, --csvfile CSVFILE
                        The csv file to which log-derived info must be
                        appended.
```