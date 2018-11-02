#!/bin/bash

PORTS="port"

ADDR='10.0.0.1'           
PATH='http://172.0.0.3:2379/v2/keys/basebox/ports'
/usr/bin/curl $PATH/ -s -q -XPUT -d dir=true
                          
for i in {1..4}
do
  for j in {2..4}
  do
    /usr/bin/curl $PATH/port$i/vlan/$j/enabling_token -q -XPUT -d value=""	
  done
  #etcdctl set /basebox/ports/$port/vlan/$i/enable ''
done
