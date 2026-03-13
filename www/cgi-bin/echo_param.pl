#!/usr/bin/perl
use strict;
use warnings;

print "Content-type: text/html\r\n";
print "\r\n";
my $name = '';
if ($ENV{QUERY_STRING}) {
    ($name) = $ENV{QUERY_STRING} =~ /^name=(.*)$/;
}

print "Hello $name\n";