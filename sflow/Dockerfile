FROM fedora

WORKDIR /opt/sflow

RUN dnf install wget -y
RUN wget https://github.com/sflow/host-sflow/releases/download/v2.0.11-1/hsflowd-centos7-2.0.11-1.x86_64.rpm && dnf install *.rpm -y

COPY hsflowd.conf /etc/hsflowd.conf
