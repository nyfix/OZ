# pol.pl

The "Print Oz Log" script parses OZ naming messages along with ZeroMQ monitor events to provide a concise summary of connection/disconnection activity.

It processes messages for the OZ "dataSub" and "namingPub" sockets, as these are the sockets that initiate connections.  (It ignores the "dataPub" and "namingSub" sockets.  See ["Socket Types"](Naming-Service.md#socket-types) for more info).   

## Usage

`pol.pl <logfile>`

Note that the log file must contain naming messages (`log_level_naming` in mama.properties) to get the process's name and PID.

## Example
Below is an example of importing the tab-separated file into a spreadsheet program.

Time | Event | Host  | Port | Prog | PID | fd
---- | ----- | ----- | ---- | ---- | --- | ---
11/17 10:34:08.491951|CONNECTED|bt-brixu|40895|nsd|157751|30
11/17 10:34:08.492716|HANDSHAKE_SUCCEEDED|bt-brixu|40895|nsd|157751|30
11/17 10:34:08.495518|CONNECTED|bt-brixu|34161|tpsdaemon|157776|32
11/17 10:34:08.496640|CONNECTED|bt-brixu|43813|svr_state_mon|157774|34
11/17 10:34:08.496754|CONNECTED|bt-brixu|39039|check_status|157778|28
11/17 10:34:08.496882|HANDSHAKE_SUCCEEDED|bt-brixu|34161|tpsdaemon|157776|32
11/17 10:34:08.497212|HANDSHAKE_SUCCEEDED|bt-brixu|43813|svr_state_mon|157774|34
11/17 10:34:08.497612|HANDSHAKE_SUCCEEDED|bt-brixu|39039|check_status|157778|28
11/17 10:34:10.666831|DISCONNECTED|bt-brixu|39039|check_status|157778|28
