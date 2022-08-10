""" Petit script, pour monitor l'arduino !
"""

import serial

from time import sleep
from datetime import datetime

from influxdb_client import InfluxDBClient, client, domain, Point

# /dev/ttyACM1
PORT = '/dev/serial/by-id/usb-Arduino__www.arduino.org__Arduino_Due_Prog._Port_7543134333435181C0F1-if00'
BRATE = 115200

URL = "http://127.0.0.1:8086/"
TOKEN = "***"
ORG = "Strontium"

if __name__ == "__main__":
    client_db = InfluxDBClient(url=URL, token=TOKEN, org=ORG)
    
    query_api = client_db.query_api()
    db_write_api = client_db.write_api(write_options=client.write_api.SYNCHRONOUS)

    def db_write_point(p):
        p.time(datetime.utcnow(), write_precision=domain.write_precision.WritePrecision.MS)
        db_write_api.write(bucket="lab", record=p)

    s = serial.Serial(PORT, BRATE)

    while True:
        #TSOCK:X-TTABLE:X-CMD:X-CONS:X
        res = s.readline()        
        if(len(res) != 0): 
            values = str(res[:-2])[:-1].split('-')
            print(values)
            for i in range(0, len(values)):
                values[i] = values[i].split(':')
            
            tsock = values[0][1]
            ttable = values[1][1]
            cmd = values[2][1]
            cons = values[3][1]

            db_write_point(Point("asservissement").field("vanne", float(cmd)))
            db_write_point(Point("roomtemp").tag("where", "sciencetable_arduino").field("tempC", float(ttable)))
            db_write_point(Point("roomtemp").tag("where", "airsock").field("tempC", float(tsock)))
            db_write_point(Point("roomtemp").tag("where","consigne airsock").field("tempC", float(cons)))




        