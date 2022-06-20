#!/bin/bash

PCAP_DIR=/vagrant/pcaps
LUA_SCRIPT_DIR=/vagrant/bier-rust/

rm -rf $PCAP_DIR/json
mkdir $PCAP_DIR/json
for pcap_file in $(ls $PCAP_DIR)
do
    tshark -r $PCAP_DIR/$pcap_file -X lua_script:$LUA_SCRIPT_DIR/bier.lua -e bier.bitstring -e data.data -e ipv6.dst -Y "ipv6.dst == ff0:babe:cafe::1" -T json > $PCAP_DIR/json/"${pcap_file%.*}".json
done