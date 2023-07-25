# pol.pl

The "Print Oz Log" script parses OZ naming messages along with ZeroMQ monitor events to provide a concise summary of connection/disconnection activity.

It processes messages for the OZ "dataSub" and "namingPub" sockets, as these are the sockets that initiate connections.  (It ignores the "dataPub" and "namingSub" sockets.  See ["Socket Types"](Naming-Service.md#socket-types) for more info).   

## Usage

`pol.pl -g '<grep-string>' <logfile>`

Parameter | Meaning
----------| -------
-g | Specifies a "grep" string that is used to filter the output.  You can specify any valid Perl regex for grep-string (see example below).

The output of the command is in tab-separated format, and can easily be imported into most spreadsheet programs.

### Notes

1. The log file must contain naming messages (`log_level_naming` in mama.properties) to get the process's name and PID.  
2. Only the first CONNECT REQ from a particular endpoint is displayed -- subsequent CONNECT REQ messages are discarded.

## Example

```
$ pol.pl -g 'tpsdaemon|tpsworker' abim3.log
```

Time | Event | Host  | Port | Prog | PID | fd
---- | ----- | ----- | ---- | ---- | --- | ---
6/13 16:07:13.200262|WELCOME|nsb12|40953|nsd|12773|0
6/13 16:07:13.200646|CONNECTED|nsb12|40953|nsd|12773|39
6/13 16:07:13.201104|HANDSHAKE_SUCCEEDED|nsb12|40953|nsd|12773|39
6/13 16:07:13.206905|CONNECT REQ|nsb11|36375|tpsdaemon|16384|0
6/13 16:07:13.207090|CONNECT REQ|nsb12|39195|tpsdaemon|18893|0
6/13 16:07:13.207611|CONNECT REQ|nsb12|43814|tpsdaemon|12334|0
6/13 16:07:13.209573|CONNECTED|nsb12|39195|tpsdaemon|18893|52
6/13 16:07:13.209682|CONNECTED|nsb11|36375|tpsdaemon|16384|51
6/13 16:07:13.232949|HANDSHAKE_SUCCEEDED|nsb11|43735|tpsworker|16623|81
6/13 16:07:13.233025|HANDSHAKE_SUCCEEDED|nsb11|42554|tpsworker|16884|97
6/13 16:07:13.233087|HANDSHAKE_SUCCEEDED|nsb11|37988|tpsworker|20351|93
6/13 16:07:13.236395|HANDSHAKE_SUCCEEDED|nsb11|36375|tpsdaemon|16384|51
