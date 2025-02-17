#!/usr/bin/env python3
# -*- coding: utf-8 -*-

SERVER = "status.botox.bz"
PORT = 35601
USER = "s01"
PASSWORD = "some-hard-to-guess-copy-paste-password"
INTERVAL = 1 # Update interval
SSLENABLED = 0
SSLCERTPATH = "ssl cert file path"
TRAFFIC_RST_DAY = 1

import socket
import time
import string
import math
import re
import os
import json
import subprocess
import collections
import ssl
from datetime import date
import calendar
import syslog

g_vnstat_dumpdb = True
g_default_NIC = "eth0"

def get_uptime():
	f = open('/proc/uptime', 'r')
	uptime = f.readline()
	f.close()
	uptime = uptime.split('.', 2)
	time = int(uptime[0])
	return int(time)

def get_memory():
	re_parser = re.compile(r'^(?P<key>\S*):\s*(?P<value>\d*)\s*kB')
	result = dict()
	for line in open('/proc/meminfo'):
		match = re_parser.match(line)
		if not match:
			continue;
		key, value = match.groups(['key', 'value'])
		result[key] = int(value)

	MemTotal = float(result['MemTotal'])
	MemFree = float(result['MemFree'])
	Cached = float(result['Cached'])
	MemUsed = MemTotal - (Cached + MemFree)
	SwapTotal = float(result['SwapTotal'])
	SwapFree = float(result['SwapFree'])
	return int(MemTotal), int(MemUsed), int(SwapTotal), int(SwapFree)

def get_hdd():
	p = subprocess.check_output(['df', '-Tlm', '--total', '-t', 'ext4', '-t', 'ext3', '-t', 'ext2', '-t', 'reiserfs', '-t', 'jfs', '-t', 'ntfs', '-t', 'fat32', '-t', 'btrfs', '-t', 'fuseblk', '-t', 'zfs', '-t', 'simfs', '-t', 'xfs']).decode("Utf-8")
	total = p.splitlines()[-1]
	used = total.split()[3]
	size = total.split()[2]
	return int(size), int(used)

def get_load():
	return os.getloadavg()[0]

def get_time():
	stat_file = open("/proc/stat", "r")
	time_list = stat_file.readline().split(' ')[2:6]
	stat_file.close()
	for i in range(len(time_list))  :
		time_list[i] = int(time_list[i])
	return time_list

def delta_time():
	x = get_time()
	time.sleep(INTERVAL)
	y = get_time()
	for i in range(len(x)):
		y[i]-=x[i]
	return y

def get_cpu():
	t = delta_time()
	st = sum(t)
	if st == 0:
		st = 1
	result = 100-(t[len(t)-1]*100.00/st)
	return round(result, 0)

class Traffic:
	def __init__(self):
		self.rx = collections.deque(maxlen=10)
		self.tx = collections.deque(maxlen=10)

	def get(self):
		f = open('/proc/net/dev', 'r')
		net_dev = f.readlines()
		f.close()
		avgrx = 0; avgtx = 0

		for dev in net_dev[2:]:
			dev = dev.split(':')
			if dev[0].strip() == "lo" or dev[0].find("tun") > -1 or dev[0].find("veth") > -1 or dev[0].find("docker") > -1 or dev[0].find("br-") > -1:
				continue
			dev = dev[1].split()
			avgrx += int(dev[0])
			avgtx += int(dev[8])

		self.rx.append(avgrx)
		self.tx.append(avgtx)
		avgrx = 0; avgtx = 0

		l = len(self.rx)
		for x in range(l - 1):
			avgrx += self.rx[x+1] - self.rx[x]
			avgtx += self.tx[x+1] - self.tx[x]

		avgrx = int(avgrx / l / INTERVAL)
		avgtx = int(avgtx / l / INTERVAL)

		return avgrx, avgtx

def testVnstatDumpDb():
	vnstat=os.popen('vnstat --dumpdb').readlines()
	for line in vnstat:
		if "Unknown parameter" in line:
			global g_vnstat_dumpdb
			g_vnstat_dumpdb = False
			global g_default_NIC
			g_default_NIC = os.popen('ip -o -4 route show to default | head -1 | rg -o "dev ([[:alnum:]]+)" -r \'$1\'').read().strip()

def vnstatDumpdb():
	NET_IN = 0
	NET_OUT = 0
	vnstat=os.popen('vnstat --dumpdb').readlines()
	for line in vnstat:
		if line[0:4] == "m;0;":
			mdata=line.split(";")
			NET_IN=int(mdata[3])*1024*1024
			NET_OUT=int(mdata[4])*1024*1024
			break
	return NET_IN, NET_OUT

def vnstatJson():
	NET_IN = 0
	NET_OUT = 0
	vnstat=os.popen('vnstat --json|jq \'.interfaces[] | select(.name=="' + g_default_NIC + '")| .traffic.day\'').read()
	vss=json.loads(vnstat)
	today = date.today().day
	tomonth = date.today().month
	tomonth_last_day = calendar.monthrange(date.today().year, tomonth)[1]
	#If the traffic reset day is beyond the month range, it should be the last day of the month.
	tomonth_rst_day = TRAFFIC_RST_DAY if TRAFFIC_RST_DAY <= tomonth_last_day else tomonth_last_day
	lamonth = tomonth -1 if tomonth -1 > 0 else 12
	#If previous month is 12, the year of previous month should be datetime.year - 1.
	lamyear = date.today().year if tomonth -1 > 0 else date.today().year -1
	lamonth_last_day = calendar.monthrange(lamyear, lamonth)[1]
	lamonth_rst_day = TRAFFIC_RST_DAY if TRAFFIC_RST_DAY <= lamonth_last_day else lamonth_last_day
	for vs in vss:
		if 'date' in vs and 'month' in vs['date'] and 'day' in vs['date']:
			month = int(vs['date']['month'])
			day = int(vs['date']['day']) 
			#If today is less than the reset day of this month, all the traffic in this month should be calculated;
			#last month traffic should be calculated if the day is more than or equal to the reset day of last month.
			if today < tomonth_rst_day and (month == tomonth or (lamonth == month and day >= lamonth_rst_day)):
				NET_IN += int(vs['rx'])
				NET_OUT += int(vs['tx'])
			#If today is more than or equal to the reset day of this month, traffic in this month should be calculted
			#only if the day is more than or euqal to the reset day of this month.
			elif today >= tomonth_rst_day and month == tomonth and day >= tomonth_rst_day:
				NET_IN += int(vs['rx'])
				NET_OUT += int(vs['tx'])
	return NET_IN, NET_OUT

def liuliang():
	if g_vnstat_dumpdb:
		return vnstatDumpdb()
	else:
		return vnstatJson()

def get_network(ip_version):
	if(ip_version == 4):
		HOST = "ipv4.google.com"
	elif(ip_version == 6):
		HOST = "ipv6.google.com"
	try:
		socket.create_connection((HOST, 80), 2).close()
		return True
	except Exception as e:
		try:
			syslog.syslog(syslog.LOG_WARNING, f"ServerStatus Network test 80 exception {e}")
			socket.create_connection((HOST, 443), 2).close()
			return True
		except Exception as e1:
			syslog.syslog(syslog.LOG_WARNING, f"ServerStatus Network test 443 exception {e1}")
	return False

if __name__ == '__main__':
	socket.setdefaulttimeout(30)
	testVnstatDumpDb()
	while 1:
		try:
			syslog.syslog("ServerStatus Connecting...")
			if SSLENABLED:
				st = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
				contextInstance = ssl.SSLContext(protocol=ssl.PROTOCOL_TLS_CLIENT)
				contextInstance.verify_mode = ssl.CERT_REQUIRED
				contextInstance.check_hostname = False
				contextInstance.load_verify_locations(cafile=SSLCERTPATH)
				s = contextInstance.wrap_socket(st)
				#s = ssl.wrap_socket(st, cert_reqs=ssl.CERT_REQUIRED, ca_certs=SSLCERTPATH)
			else:
				s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			s.connect((SERVER, PORT))
			data = s.recv(1024).decode()
			data = data.replace(chr(0), "")
			if data.find("Authentication required") > -1:
				auth = USER + ':' + PASSWORD + '\n'
				s.send(auth.encode())
				data = s.recv(1024).decode()
				data = data.replace(chr(0), "")
				if data.find("Authentication successful") < 0:
					syslog.syslog(syslog.LOG_ERR, f"ServerStatus Auth Error: {data}")
					raise socket.error
			else:
				syslog.syslog(syslog.LOG_ERR, f"ServerStatus Server Response Error: {data}")
				raise socket.error

			syslog.syslog(f"ServerStatus connected. {data}")
			if data.find("IPv4") == -1 and data.find("IPv6") == -1:
				data = s.recv(1024).decode()
				data = data.replace(chr(0), "")
				syslog.syslog(f"ServerStatus Server Response: {data}")

			timer = 0
			check_ip = 0
			if data.find("IPv4") > -1:
				check_ip = 6
			elif data.find("IPv6") > -1:
				check_ip = 4
			else:
				syslog.syslog(syslog.LOG_ERR, f"ServerStatus check ip info error: {data}")
				raise socket.error

			traffic = Traffic()
			traffic.get()
			while 1:
				CPU = get_cpu()
				NetRx, NetTx = traffic.get()
				NET_IN, NET_OUT = liuliang()
				Uptime = get_uptime()
				Load = get_load()
				MemoryTotal, MemoryUsed, SwapTotal, SwapFree = get_memory()
				HDDTotal, HDDUsed = get_hdd()

				array = {}
				if not timer:
					array['online' + str(check_ip)] = get_network(check_ip)
					timer = 120
				else:
					timer -= 1*INTERVAL

				array['uptime'] = Uptime
				array['load'] = Load
				array['memory_total'] = MemoryTotal
				array['memory_used'] = MemoryUsed
				array['swap_total'] = SwapTotal
				array['swap_used'] = SwapTotal - SwapFree
				array['hdd_total'] = HDDTotal
				array['hdd_used'] = HDDUsed
				array['cpu'] = CPU
				array['network_rx'] = NetRx
				array['network_tx'] = NetTx
				array['network_in'] = NET_IN
				array['network_out'] = NET_OUT
				array['traffic_rst_day'] = TRAFFIC_RST_DAY

				updateContent = "update " + json.dumps(array) + "\n"
				s.send(updateContent.encode())
		except KeyboardInterrupt:
			raise
		except socket.error:
			syslog.syslog(syslog.LOG_ERR, f"ServerStatus Disconnected...{socket.error}")
			# keep on trying after a disconnect
			s.close()
			time.sleep(3)
		except Exception as e:
			syslog.syslog(syslog.LOG_ERR, f"ServerStatus Caught Exception: {e}")
			s.close()
			time.sleep(3)
