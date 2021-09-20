#!/bin/bash

source debian/vars.sh

set -x

# pulled from apr-util
mkdir -p config
cp $ea_apr_config config/apr-1-config
cp $ea_apr_config config/apr-config
cp /usr/share/pkgconfig/ea-apr16-1.pc config/apr-1.pc
cp /usr/share/pkgconfig/ea-apr16-util-1.pc config/apr-util-1.pc
cp /usr/share/pkgconfig/ea-apr16-1.pc config
cp /usr/share/pkgconfig/ea-apr16-util-1.pc config

export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:`pwd`/config"
echo "PKG_CONFIG_PATH :$PKG_CONFIG_PATH:"

mkdir -p $DEB_INSTALL_ROOT

cp mod_evasive{20,24}.c

sed 's/connection->remote_ip/connection->client_ip/' < mod_evasive20.c > mod_evasive24.c
sed -i 's/evasive20_module/evasive24_module/' mod_evasive24.c
/usr/bin/apxs -Wc,"$optflags" -c mod_evasive24.c

echo "END CONFIG/BUILD"

