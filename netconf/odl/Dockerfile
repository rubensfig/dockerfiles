FROM  opendaylight/odl

RUN yum install tmux -y

EXPOSE 1830 6633 6653 8383 8181

WORKDIR /opt/opendaylight

CMD ["tmux", "new", "-s", "karaf","-d"]
ENTRYPOINT sh
