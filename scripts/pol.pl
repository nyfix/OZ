#!/usr/bin/perl
#
# parse OZ logs
#

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
};

my %peers;
my %hosts;

sub getPeer {
   my $endpoint = shift;
      my $exists = exists($peers{$endpoint});
      my $peer;
      if (!$exists) {
         $peer = Peer->new();
         $peers{$endpoint} = $peer;   
      }
      else  {
         $peer = $peers{$endpoint};
      }
      
   return $peer, $exists;
}   

# take filename arg on cmd line, or read from stdin
local *INFILE;
if ( defined( $ARGV[0] ) ) {
   open( INFILE, "<:crlf", "$ARGV[0]" ) or die "Cant open $ARGV[0]\n";
}
else {
   *INFILE = *STDIN;
}

print "Time\tEvent\tHost\tPort\tProg\tPID\tfd\n";

while (<INFILE>) {
   chomp;
   my (@parts) = split /\|/;
   my $timestamp = $parts[0];
   my $msg; my $type; my @fields;
   if ($#parts == 1) {
      # old format
      my (@tokens) = split(':', $parts[1], 3);
      $msg = $tokens[0];
      $type = $tokens[1];
      (@fields) = split(' ', $tokens[2]);
   }
   elsif ($#parts == 4) {
      # new format
      $msg = $parts[1];
      next if (! $msg =~ "zmqBridge");
      my (@tokens) = split(' ', $parts[3], 2);
      $type = (split(':', $tokens[1]))[0]; 
      if ($msg eq "zmqBridgeMamaTransportImpl_dispatchNamingMsg") {
         (@fields) = split(' ', (split(':', $tokens[1], 2))[1]);
      }
      elsif ($msg eq "zmqBridgeMamaTransportImpl_monitorEvent") {
         (@fields) = split(' ', $tokens[1]);
      }
      elsif ($msg eq "zmqBridgeMamaTransportImpl_monitorEvent_v2") {
         (@fields) = split(' ', $tokens[1]);
      }
   }
   else {
      # unknown format
      next;
   }
   next if $#fields eq 0;
   if ($msg eq "zmqBridgeMamaTransportImpl_dispatchNamingMsg" ) {
      if ( ($type =~ "Received endpoint msg") || ($type =~ "Published endpoint msg") ) {
         my $endpoint = ((split('=', $fields[6]))[1]);
         my ($peer, $exists) = getPeer($endpoint);
         $peer->prog((split('=', $fields[1]))[1]);
         $peer->host((split('=', $fields[2]))[1]);
         $peer->pid((split('=', $fields[4]))[1]);
         $peer->endpoint($endpoint);
         $peer->proto((split('[:/]+', $peer->endpoint))[0]);
         $peer->addr((split('[:/]+', $peer->endpoint))[1]);
         $peer->port((split('[:/]+', $peer->endpoint))[2]);
         if (!exists($hosts{$peer->addr()})) {
            $hosts{$peer->addr()} = $peer->host();
         }
      }
   }
   elsif ($msg eq "zmqBridgeMamaTransportImpl_monitorEvent" ) {
      my $sockType = (split(':', $fields[1]))[1];
      next if (($sockType eq "dataPub") || ($sockType eq "namingSub"));
      my $event = (split(':', $fields[4]))[1];
      if ($event =~ "LISTENING") {
         my $endpoint = (split(':', $fields[5], 2))[1];
         my ($peer, $exists) = getPeer($endpoint);
         my $fd = (split(':', $fields[2]))[1];
         $peer->fd($fd) if $fd != 0;
      }
      elsif ( ($event =~ "CONNECTED") || 
              ($event =~ "HANDSHAKE_SUCCEEDED") ||  
              ($event =~ "DISCONNECTED") ) {  
         my $endpoint = (split(':', $fields[5], 2))[1];
         my ($peer, $exists) = getPeer($endpoint);
         my $fd = (split(':', $fields[3]))[1];
         $peer->fd($fd) if $fd != 0;
         printf("%s\t%s\t%s\t%d\t%s\t%d\t%d\n", $timestamp, $event, $peer->host(), $peer->port(), $peer->prog(), $peer->pid(), $peer->fd());     
      }
   }
   elsif ($msg eq "zmqBridgeMamaTransportImpl_monitorEvent_v2" ) {
      my $sockType = (split(':', $fields[0]))[1];
      next if (($sockType eq "dataPub") || ($sockType eq "namingSub"));
      my $event = (split(':', $fields[1]))[1];
      if ($event =~ "LISTENING") {
         my $endpoint = (split(':', $fields[3], 2))[1];
         my ($peer, $exists) = getPeer($endpoint);
         my $fd = (split(':', $fields[2]))[1];
         $peer->fd($fd) if $fd != 0;
      }
      elsif ( ($event =~ "CONNECTED") || 
              ($event =~ "HANDSHAKE_SUCCEEDED") ||  
              ($event =~ "DISCONNECTED") ) {  
         my $endpoint = (split(':', $fields[4], 2))[1];
         my ($peer, $exists) = getPeer($endpoint);
         my $fd = (split(':', $fields[2]))[1];
         $peer->fd($fd) if $fd != 0;
         printf("%s\t%s\t%s\t%d\t%s\t%d\t%d\n", $timestamp, $event, $peer->host(), $peer->port(), $peer->prog(), $peer->pid(), $peer->fd());     
      }
   } 
}

close INFILE;
