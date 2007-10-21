#!/bin/sh
cd `dirname "$0"`
MYDIR=`pwd`
cd ../..
echo sudo ./install.rb
sudo ./install.rb
cd "$MYDIR"
echo lighttpd -Df lighttpd.conf
lighttpd -Df lighttpd.conf
