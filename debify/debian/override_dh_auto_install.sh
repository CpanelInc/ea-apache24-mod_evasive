#!/bin/bash

source debian/vars.sh

set -x

mkdir -p $DEB_INSTALL_ROOT/var/log/apache2/mod_evasive
mkdir -p $DEB_INSTALL_ROOT$_httpd_confdir
mkdir -p $DEB_INSTALL_ROOT$_httpd_modconfdir
mkdir -p $DEB_INSTALL_ROOT$_httpd_moddir
mkdir -p $DEB_INSTALL_ROOT/usr/local/cpanel/scripts
install -D $SOURCE1 $DEB_INSTALL_ROOT$_httpd_confdir/300-mod_evasive.conf
install -D $SOURCE2 $DEB_INSTALL_ROOT$_httpd_modconfdir/300-mod_evasive.conf
install -pm 755 .libs/mod_evasive24.so $DEB_INSTALL_ROOT$_httpd_moddir/mod_evasive24.so
mkdir -p $DEB_INSTALL_ROOT/usr/local/cpanel/scripts
install -pm 755 $SOURCE3 $DEB_INSTALL_ROOT/usr/local/cpanel/scripts/generate_mod_evasive_local_ips_conf.pl

ls -ld debian/tmp/usr/local/cpanel/scripts/generate_mod_evasive_local_ips_conf.pl
head debian/tmp/usr/local/cpanel/scripts/generate_mod_evasive_local_ips_conf.pl

echo "END INSTALLL"

