# pol.pl

The "Print Oz Log" script parses OZ naming messages along with ZeroMQ monitor events to provide a concise summary of connection/disconnection activity for a single process.

## Usage

`pol.pl [-e | -p | -h] [-g '<grep-string>'] <logfiles>`

Parameter | Meaning
----------| -------
-e | Prints a list of events seen in the input.  This is the default if none is specified.
-p | Prints a list of peers seen in the input.
-h | Prints a list of hosts seen in the input.
-g | Specifies a "grep" string that is used to filter the output.  You can specify any valid Perl regex for grep-string (see example below).
logfile(s) | One or more filenames containing naming and event log messages. 

By default, the script prints a list of all ZeroMQ protocol events, enhanced with host and program information taken from OZ naming messages.  This can help with troubleshooting connection problems, or as a way to better understand the way OZ processes are connected.

The output of the command is in tab-separated format, and can easily be imported into most spreadsheet programs.  It can be instructive to use pivot tables/categories to view the data in formats.

### Notes

The log file(s) must be from a single process -- the script is not able to process log files from multiple processes.

The log file(s) need not be sorted -- the script will sort events on output.

The log file(s) must contain naming messages (`log_level_naming` in mama.properties) to get host and program information.

## Examples

To see a list of all events from a single process:

```
pol.pl tpsdaemon.log
```

The output will look something like the following:

Timestamp|Event|Socket|LocalPort|RemoteHost|RemoteProg|RemotePID|RemotePort
---|---|---|---|---|---|---|---
01/24 09:27:11.873686|LISTENING|dataPub|44739||||
01/24 09:27:23.169904|CONNECTED|dataSub|44860|bt|tpsworker|1919845|33023
01/24 09:27:23.170504|HANDSHAKE_SUCCEEDED|dataSub|44860|bt|tpsworker|1919845|33023
01/24 09:27:24.289465|ACCEPTED|dataPub|44739|bt|||36420
01/24 09:27:24.290055|HANDSHAKE_SUCCEEDED|dataPub|44739|bt|||36420
01/24 09:27:25.389546|DISCONNECTED|dataPub|44739|bt|||36420

- The "Socket" column shows the name of the correspoing OZ socket (see ["Naming Service"](../doc/Naming-Service.md) for more info).
- "LocalPort" shows the underlying port used by ZeroMQ for the connection.  
- "RemoteHost" shows the remote host name, if known -- otherwise it simply shows the host address.
- "RemoteProg" and "RemotePID" show the name and ID of the remote process endpoint, if known.
- "RemotePort" shows the port number of the remote side of the connection. 

The script can show all *outgoing* connections from the process, usually with the name and PID of the remote endpoint.

Unfortunately there is currently no way to identify *incoming* connections, so these show only the port number, along with the host (e.g., the "ACCEPTED" event, above).

<hr>

For a list of peers (endpoints) seen in the input:

```
$ pol.pl -p tpsdaemon.log
```

will produce output similar to the following:

Endpoint|Host|Port|Prog|PID|First Seen
---|---|---|---|---|--- 
tcp://127.0.0.1:33023|bt|33023|tpsworker|1919845|01/24 09:27:23.169380
tcp://127.0.0.1:34743|bt|34743|snapshot|1919984|01/24 09:27:54.438107
tcp://127.0.0.1:34931|bt|34931|tpsdaemon|1920080|01/24 09:28:04.906442
tcp://127.0.0.1:35493|bt|35493|dropcopy|1920948|01/24 09:30:44.597890
tcp://127.0.0.1:35575|bt|35575|configpub|1919982|01/24 09:27:54.435554
tcp://127.0.0.1:36813|bt|36813|xla|1919985|01/24 09:27:54.437992
tcp://127.0.0.1:38565|bt|38565|java|1920677|01/24 09:29:44.717238
tcp://127.0.0.1:46311|bt|46311|nsd|1919738|01/24 09:27:11.874267
tcp://127.0.0.1:46507|bt|46507|configpub|1919983|01/24 09:27:54.435704

## Running Remotely

In many cases, it can make more sense to run the script on the machine where the log files are located.  This can be done using ssh like so:

```
ssh {hostname} 'perl - {args to perl script}' < {path to perl script}
```

e.g.,

```
ssh bt-brixu 'perl - -p /home/btorpey/hs2/btdev/tpsdaemon.log' < $(which pol.pl)
```


