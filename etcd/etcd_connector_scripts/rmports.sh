#!/bin/bash

PORTS=("port1" "port2" "port3" "port4")
ADDR='10.0.0.1'
PATH='http://172.0.0.3:2379/v2/keys/basebox/ports'

for i in {1..4}
do
  /usr/bin/curl $PATH/port$i/vlan/2/enabling_token?recursive=true -XDELETE 
done
