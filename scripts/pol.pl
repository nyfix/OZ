#!/usr/bin/perl
#
# parse OZ logs
#

use strict;

# get params
use Getopt::Long qw(:config pass_through);
my $grep;
GetOptions( 'g|grep=s' => \$grep );
my $hosts=0;
GetOptions( 'h|hosts!' => \$hosts );
my $peers=0;
GetOptions( 'p|peers!' => \$peers );
my $events=0;
GetOptions( 'e|events!' => \$events );
# default to listing events
if (($hosts == 0) && ($peers == 0) && ($events == 0) ) {
   $events = 1;
}

use Class::Struct;
struct Peer => {
   prog => '$',
   host => '$',
   pid => '$',
   endpoint => '$',
   proto => '$',
   addr => '$',
   port => '$',
   fd => '$',
   firstSeen => '$',
};

struct Event => {
   name => '$',
   sockType => '$',
   local => '$',
   remote => '$',
};


my %peers;
my %hosts;
my %events;

sub parseProto {
   my $endpoint = shift;
   return (split('[:/]+', $endpoint))[0];
}

sub parseAddr {
   my $endpoint = shift;
   return (split('[:/]+', $endpoint))[1];
}

sub parsePort {
   my $endpoint = shift;
   return (split('[:/]+', $endpoint))[2];
}

sub shortName {
   my $temp = shift;
   # strip off domain, if any
   $temp = (split('\.', $temp))[0];
   # strip off suffix, if any
   $temp = (split('\-', $temp))[0];
   return $temp;
}

sub parseEndpoint {
   my $endpoint = shift;
   my $proto = parseProto($endpoint);
   my $addr = parseAddr($endpoint);
   my $port = parsePort($endpoint);

   return ($proto, $addr, $port);
}

sub getField {
   my $name = shift;
   my $string = shift;

   my $index = index($string, $name);
   return '' if ($index < 0);

   my $temp = substr($string, $index+length($name));
   my (@value) = split(' ', $temp);
   return $value[0];
}

sub findPeer {
   my $endpoint = shift;
   if (!exists($peers{$endpoint})) {
      return (Peer->new(), 0);
   }
   else {
      return ($peers{$endpoint}, 1);
   }
}


sub findHost {
   my $endpoint = shift;
   my ($proto, $addr, $port) = parseEndpoint($endpoint);
   if (substr($addr, 0, 1) =~ /[0-9]/ ) {
      return $hosts{$addr};
   }
   else {
      return shortName($addr);
   }
}

sub printg {
   my $format = shift;
   my $line = sprintf $format, @_;
   print $line if (!defined $grep || ($line =~ $grep));
}


while (<>) {
   chomp;
   my (@parts) = split /\|/;

   my $timestamp = $parts[0];
   # hack for date
   my ($date, $time) = split(' ', $timestamp);
   if (length($date) < 5) {
      $timestamp = "0$timestamp"
   }

   # get msgType, subType
   my $msgType; my $subType; my $msgString; my $sockType;
   if ($#parts == 4) {
      # new format
      $msgType = $parts[1];
      next if (! $msgType =~ "zmqBridge");
      my (@tokens) = split(' ', $parts[3], 2);
      $subType = (split(':', $tokens[1]))[0];
      if ( ($msgType eq "zmqBridgeMamaTransportImpl_dispatchNamingMsg") || ($msgType eq "zmqBridgeMamaTransportImpl_sendEndpointsMsg") ) {
        $msgString = (split(':', $tokens[1], 2))[1];
      }
      elsif ($msgType eq "zmqBridgeMamaTransportImpl_monitorEvent_v2") {
         $msgString = $tokens[1];
      }
      $sockType = getField("name:", $msgString);
   }
   else {
      # unknown format
      next;
   }

   # "naming" msg: save info about the endpoint
   if ( ($msgType eq "zmqBridgeMamaTransportImpl_dispatchNamingMsg") || ($msgType eq "zmqBridgeMamaTransportImpl_sendEndpointsMsg") ) {
      if ( ($subType =~ "Received endpoint msg") || ($subType =~ "Published endpoint msg") ) {
         my $event;
         my $endpoint = getField("pub=", $msgString);
         my ($peer, $exists) = findPeer($endpoint);
         my ($proto, $temp, $port) = parseEndpoint($endpoint);
         $peer->proto($proto);
         $peer->port($port);
         $peer->host(shortName(getField("host=", $msgString)));
         # endpoint usually specifies tcp addr, but sometimes host name
         if (substr($temp, 0, 1) =~ /[0-9]/ ) {
            $peer->addr($temp);
         }
         $peer->prog(getField("prog=", $msgString));
         $peer->pid(getField("pid=", $msgString));
         $peer->endpoint($endpoint);
         if (!$exists) {
            $peer->firstSeen($timestamp);
            $peers{$peer->endpoint()} = $peer;
         }
         if (!exists($hosts{$peer->addr()}) && defined $peer->addr()) {
            $hosts{$peer->addr()} = $peer->host();
         }
         if (!exists($hosts{$peer->host()}) && defined $peer->host()) {
            $hosts{$peer->host()} = $peer->addr();
         }
      }
   }

   # monitor event
   elsif ($msgType eq "zmqBridgeMamaTransportImpl_monitorEvent_v2" ) {
      my $event = Event->new();
      $event->name(getField("event:", $msgString));
      $event->sockType($sockType);
      $event->local(getField("local:", $msgString));
      $event->remote(getField("remote:", $msgString));
      # may be mult. events per timestamp
      push @{$events{$timestamp}}, $event;
   }
}


if ($hosts) {
	printf("Addr\tHost\n");
	for my $h (sort (keys %hosts)) {
	   printg("%s\t%s\n", $h, $hosts{$h});
	}
}

if ($peers) {
	printf("Endpoint\tHost\tPort\tProg\tPID\tFirst Seen\n");
	for my $p (sort (keys %peers)) {
	   printg("%s\t%s\t%d\t%s\t%d\t%s\n", $p, $peers{$p}->host(), $peers{$p}->port(), $peers{$p}->prog(), $peers{$p}->pid(), $peers{$p}->firstSeen());
	}
}

if ($events) {
   printf("Timestamp\tEvent\tSocket\tLocalPort\tRemoteHost\tRemoteProg\tRemotePID\tRemotePort\n");
   for my $timestamp (sort (keys %events)) {
       for my $e ( @{ $events{$timestamp} } ) {
         # get local info
         my $localHost; my $localProg; my $localPort;
         if ($e->name() ne "CLOSED") {
            my ($local, $localExists) = findPeer($e->local());
            if (!$localExists) {
               $localHost = findHost($e->local());
               $localPort = parsePort($e->local());
            }
            else {
               $localHost = $local->host();
               $localProg = $local->prog();
               $localPort = $local->port();
            }
         }
         # get remote info
         my ($remote, $remoteExists) = findPeer($e->remote());
         my $remoteHost; my $remoteProg; my $remotePort;
         if (!$remoteExists) {
            $remoteHost = findHost($e->remote());
            $remotePort = parsePort($e->remote());
         }
         else {
            $remoteHost = $remote->host();
            $remoteProg = $remote->prog();
            $remotePort = $remote->port();
         }

         printg("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", $timestamp, $e->name(), $e->sockType(), $localPort, $remoteHost, $remoteProg, $remote->pid(), $remotePort);
       }
   }
}
