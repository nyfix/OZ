# pol.pl

The "Print Oz Log" script parses OZ naming messages along with ZeroMQ monitor events to provide a concise summary of connection/disconnection activity.

It processes messages for the OZ "dataSub" and "namingPub" sockets, as these are the sockets that initiate connections.  (It ignores the "dataPub" and "namingSub" sockets.  See ["Socket Types"](Naming-Service.md#socket-types) for more info).   

## Usage

`pol.pl -g '<grep-string>' <logfile>`

Parameter | Meaning
----------| -------
-g | Specifies a "grep" string that is used to filter the output.  You can specify any valid Perl regex for grep-string (see example below).

> Note that the log file must contain naming messages (`log_level_naming` in mama.properties) to get the process's name and PID.

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
6/13 16:07:13.209904|CONNECTED|nsb12|43814|tpsdaemon|12334|55
6/13 16:07:13.210329|CONNECT REQ|nsb13|38855|tpsworker|7412|0
6/13 16:07:13.210723|CONNECT REQ|nsb12|33745|tpsdaemon|7679|0
6/13 16:07:13.210904|CONNECT REQ|nsb13|44681|tpsworker|7576|0
