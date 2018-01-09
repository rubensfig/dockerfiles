FROM fedora

RUN dnf update -y && dnf install python python-pip -y
RUN pip install ncclient

ADD ncclient /ncclient

ENTRYPOINT /bin/bash
