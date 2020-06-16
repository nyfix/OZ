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
   my (@fields) = split /\|/;
   my $timestamp = $fields[0];
   my (@tokens) = split(' ', $fields[3]);
   if ($fields[1] =~ "zmqBridgeMamaTransportImpl_dispatchNamingMsg" ) {
      my $type = $fields[3];
      if ( ($type =~ "Received endpoint msg:") || 
           ($type =~ "Published endpoint msg:") ) {
         my $endpoint = ((split('=', $tokens[10]))[1]);
         my ($peer, $exists) = getPeer($endpoint);
         $peer->prog((split('=', $tokens[5]))[1]);
         $peer->host((split('=', $tokens[6]))[1]);
         $peer->pid((split('=', $tokens[8]))[1]);
         $peer->endpoint($endpoint);
         $peer->proto((split('[:/]+', $peer->endpoint))[0]);
         $peer->addr((split('[:/]+', $peer->endpoint))[1]);
         $peer->port((split('[:/]+', $peer->endpoint))[2]);
         if (!exists($hosts{$peer->addr()})) {
            $hosts{$peer->addr()} = $peer->host();
         }
      }
   }
   elsif ($fields[1] eq "zmqBridgeMamaTransportImpl_monitorEvent" ) {
      my $sockType = (split(':', $tokens[2]))[1];
      next if (($sockType eq "dataPub") || ($sockType eq "namingSub"));
      my $event = (split(':', $tokens[5]))[1];
      if ($event =~ "LISTENING") {
         my $endpoint = (split(':', $tokens[6], 2))[1];
         my ($peer, $exists) = getPeer($endpoint);
         my $fd = (split(':', $tokens[3]))[1];
         $peer->fd($fd) if $fd != 0;
      }
      elsif ( ($event =~ "CONNECTED") || 
              ($event =~ "HANDSHAKE_SUCCEEDED") ||  
              ($event =~ "DISCONNECTED") ) {  
         my $endpoint = (split(':', $tokens[6], 2))[1];
         my ($peer, $exists) = getPeer($endpoint);
         my $fd = (split(':', $tokens[3]))[1];
         $peer->fd($fd) if $fd != 0;
         printf("%s\t%s\t%s\t%d\t%s\t%d\t%d\n", $timestamp, $event, $peer->host(), $peer->port(), $peer->prog(), $peer->pid(), $peer->fd());     
      }
   }
   elsif ($fields[1] eq "zmqBridgeMamaTransportImpl_monitorEvent_v2" ) {
      my $sockType = (split(':', $tokens[1]))[1];
      next if (($sockType eq "dataPub") || ($sockType eq "namingSub"));
      my $event = (split(':', $tokens[2]))[1];
      if ($event =~ "LISTENING") {
         my $endpoint = (split(':', $tokens[4], 2))[1];
         my ($peer, $exists) = getPeer($endpoint);
         my $fd = (split(':', $tokens[3]))[1];
         $peer->fd($fd) if $fd != 0;
      }
      elsif ( ($event =~ "CONNECTED") || 
              ($event =~ "HANDSHAKE_SUCCEEDED") ||  
              ($event =~ "DISCONNECTED") ) {  
         my $endpoint = (split(':', $tokens[5], 2))[1];
         my ($peer, $exists) = getPeer($endpoint);
         my $fd = (split(':', $tokens[3]))[1];
         $peer->fd($fd) if $fd != 0;
         printf("%s\t%s\t%s\t%d\t%s\t%d\t%d\n", $timestamp, $event, $peer->host(), $peer->port(), $peer->prog(), $peer->pid(), $peer->fd());     
      }
   } 
}

close INFILE;
