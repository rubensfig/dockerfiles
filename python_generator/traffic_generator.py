from prometheus_client import start_http_server, Summary
import random
import time
import pysnmp
from pysnmp.hlapi import *

# Create a metric to track time spent and requests made.
REQUEST_TIME = Summary('request_processing_seconds', 'Time spent processing request')
REQUEST_SNMP = Summary('request_snmp', 'Time')

# Decorate function with metric.
@REQUEST_TIME.time()
def process_request(t):
    """A dummy function that takes some time."""
    time.sleep(t)

@REQUEST_SNMP.time()
def snmp_request():

	errorIndication, errorStatus, errorIndex, varBinds = next(
	    getCmd(SnmpEngine(),
		   CommunityData('public', mpModel=0),
		   UdpTransportTarget(('snmpd', 6161)),
		   ContextData(),
	           ObjectType(ObjectIdentity('SNMPv2-MIB', 'sysDescr', 0)))
	)

	if errorIndication:
	    print(errorIndication)
	elif errorStatus:
	    print('%s at %s' % (errorStatus.prettyPrint(),
				errorIndex and varBinds[int(errorIndex) - 1][0] or '?'))
	else:
	    for varBind in varBinds:
		print(' = '.join([x.prettyPrint() for x in varBind]))


if __name__ == '__main__':
    start_http_server(8000)
    while True:
	snmp_request()
        process_request(random.random())
