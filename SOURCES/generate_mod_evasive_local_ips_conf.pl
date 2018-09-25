#!/usr/local/cpanel/3rdparty/bin/perl
# cpanel - update_mod_evasive_conf.pl                Copyright 2018 cPanel, Inc.
#                                                           All rights Reserved.
# copyright@cpanel.net                                         http://cpanel.net
# This code is subject to the cPanel license. Unauthorized copying is prohibited

use strict;
use warnings;

use Cpanel::Fcntl::Constants       ();
use Cpanel::Linux::RtNetlink       ();
use Cpanel::Transaction::File::Raw ();

if ( scalar @ARGV < 1 ) {
    print "Usage:  $0 <name of file to generate>\n";
    exit 1;
}

my $mod_evaseive_local_ips_conf_file = $ARGV[0];

# First get our list of local IPs (this will include 127.0.0.1)
# Get them as a hash so it will be easy to remove any already present
my $addr_info = Cpanel::Linux::RtNetlink::get_interface_addresses('AF_INET');
my %addr_list = map { $_->{ip} => 1 } @{$addr_info};

if ( scalar keys %addr_list == 0 ) {

    # This should never really happen
    print "\nNo local IPs to add\n\n";
    exit;
}

print "\nAdding local IPs to mod_evasive DOSWhitelist\n\n";

print "Local IPs to whitelist\n";
foreach my $ip ( sort keys %addr_list ) {
    print "\t$ip\n";
}
print "\n";

my $file_trans = Cpanel::Transaction::File::Raw->new(
    'path'        => $mod_evaseive_local_ips_conf_file,
    'permissions' => 0644,
);

my @new_lines;
push @new_lines, "<IfModule mod_evasive24.c>\n\n";

push @new_lines, "    # Automatically generated file\n";
push @new_lines, "    # If you modify your changes may be lost\n\n";

foreach my $ip ( sort keys %addr_list ) {

    push @new_lines, "    DOSWhitelist    $ip\n";
}
push @new_lines, "</IfModule>\n";

print "Saving $mod_evaseive_local_ips_conf_file\n\n";

my $contents = join( '', @new_lines );
$file_trans->set_data( \$contents );
$file_trans->save_and_close_or_die();
