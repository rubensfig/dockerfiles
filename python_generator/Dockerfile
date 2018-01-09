FROM debian
RUN apt update && apt install python-pip curl -y
RUN pip install prometheus_client pysnmp
ADD traffic_generator.py /root/traffic_generator.py
ADD entrypoint.sh /root/entrypoint.sh
RUN chmod +x /root/entrypoint.sh
ENTRYPOINT /root/entrypoint.sh

