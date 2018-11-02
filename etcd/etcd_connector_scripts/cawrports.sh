#!/bin/bash

for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/portba611628/vlan/$vid/enabling_token -XPUT ; done && \
for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/portba611580/vlan/$vid/enabling_token -XPUT ; done && \
for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/portba611650/vlan/$vid/enabling_token -XPUT ; done && \
for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/port7abd41d0/vlan/$vid/enabling_token -XPUT ; done && \
for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/port7abd41d2/vlan/$vid/enabling_token -XPUT ; done && \
for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/portba611654/vlan/$vid/enabling_token -XPUT ; done && \
for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/portba611510/vlan/$vid/enabling_token -XPUT ; done && \
for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/portba6113a0/vlan/$vid/enabling_token -XPUT ; done && \
for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/portba61160c/vlan/$vid/enabling_token -XPUT ; done && \
for vid in {2..10}; do curl http://172.0.0.3:2379/v2/keys/basebox/ports/portba61151c/vlan/$vid/enabling_token -XPUT ; done
