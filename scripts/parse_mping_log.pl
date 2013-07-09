#!/usr/bin/perl -w

use strict;
use Scalar::Util qw/openhandle/;

my $last_pktsize = 0;
my $cur_pktsize;
my $cur_winsize;

while(<>) {
  my ($line) = $_;

  if ($line =~ m/^\[/) {
    if ($line =~ m/^\[I\]/) {
      if ($line =~ m/packet size ([^\,]+)\, window size (\d+)/) {
        $cur_pktsize = $1;
        $cur_winsize = $2;

        next if ($cur_winsize == 0);

        if ($cur_pktsize != $last_pktsize) {
          if (defined(openhandle(*TEMP))) {
            close TEMP;
          }

          open TEMP, ">size_$cur_pktsize.temp" or 
              die "cannot write to file $cur_pktsize.log.txt";
          
          $last_pktsize = $cur_pktsize;
        }
      }
    }
  } else {
    if ($line =~ m/^Sent (\d+) received (\d+)/) {
      my $sent = $1;
      my $recv = $2;
      next if ($sent == 0);
      print TEMP "$cur_winsize $sent $recv\n";
    }
  }
}
