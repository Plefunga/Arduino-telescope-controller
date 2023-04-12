from flask import Flask, request
from flask_cors import CORS

LST = 0
longitude = 150.944799
latitude = -31.08
import time
import json
import math

def jdify(Y, M, D, h, m, s):
	if M <= 2:
		M = M + 12
		Y = Y - 1
	A = math.floor(Y / 100)
	B = math.floor(A / 4)
	C = math.floor(2 - A + B)
	E = math.floor(365.25 * (Y + 4716))
	F = math.floor(30.6001 * (M + 1))
	JD = C + D + E + F - 1524.5 + ih / 24 + im / (24 * 60) + iS / (24 * 60 * 60)
	return JD


def bigt(jd):
  return ((jd - 2451545.0) / 36525.0)


def gmst(t, jd):
	thetazero = 280.46061837 + 360.98564736629 * (
		jd - 2451545.0) + 0.000387933 * (t * t) - (t * t * t) / 38710000.0
	while thetazero > 360:
		thetazero = thetazero - 360
	
	while thetazero < 0:
		thetazero = thetazero + 360
	
	return thetazero

def lst(Y, M, D, h, m, s, long):
	gmstdeg = ih * 15 + im * 15 / 60 + iS * 15 / 3600
	g = gmst(bigt(jdify(iY, iM, iD, ih, im, iS)), jdify(iY, iM, iD, ih, im, iS))
	d = gmstdeg - g
	#g = gmst(bigt(JD), JD)
	L = g + long
	return L, d


def abs_sort(dict):
	keys = list(dict)
	values = []
	init_values = []
	init_values_absed = []
	for i in keys:
		init_values.append(dict[i])
		init_values_absed.append(abs(dict[i]))
		values.append(abs(dict[i]))

	values.sort()
	result = {}
	for i in values:
		result[keys[init_values_absed.index(i)]] = init_values[
			init_values_absed.index(i)]
	return result


def hmsify(r, d):
	h = (r / 360) * 24
	rh = math.floor(h)
	m = (h - rh) * 60
	rm = math.floor(m)
	rs = (m - rm) * 60

	dd = math.floor(d)
	m = (d - dd) * 60
	dm = math.floor(m)
	ds = math.floor((m - dm) * 60)
	return rh, rm, rs, dd, dm, ds


def altaz(h, d, l):
	h = math.radians(h)
	d = math.radians(d)
	l = math.radians(l)
	azimuth = math.pi + math.atan2(
		math.sin(h),
		math.cos(h) * math.sin(l) - math.tan(d) * math.cos(l))
	altitude = math.asin(
		math.sin(l) * math.sin(d) + math.cos(h) * math.cos(d) * math.cos(l))
	azimuth = math.degrees(azimuth)
	altitude = math.degrees(altitude)
	return altitude, azimuth


def precess(r, d, y, rs, ds):
	t = y - 2000
	m = (3.0749 + 0.0000186 * t) * t
	n = (20.043 - 0.000085 * t) * t
	ns = (1.3362 - 0.0000056 * t) * t
	rr = m + ns * math.sin(math.radians(r)) * math.tan(math.radians(d))
	dd = n * math.cos(math.radians(r))
	return rs + rr, ds + dd


def refract(z):
	#1.02cot(h+10.3/(h+5.11))
	R = 1.02 * (1 / math.tan(math.radians(z + 10.3 / (z + 5.11))))
	return z + (R / 3600)

def radec(altitude, azimuth, l, t):
	altitude = math.radians(altitude)
	azimuth = math.radians(azimuth)
	l = math.radians(l)
	t = math.radians(t)
	d = math.asin(
		math.sin(altitude) * math.sin(l) +
		math.cos(altitude) * math.cos(azimuth) * math.cos(l))
	h = math.asin(math.sin(azimuth) * math.cos(altitude) / math.cos(d))
	d = math.degrees(d)
	r = math.degrees(t - (math.pi*2 - h))
	return r, d


t = time.gmtime()
"""iY = int(inp[0:4])
iM = int(inp[4:6])
iD = int(inp[6:8])
ih = int(inp[8:10])
im = int(inp[10:12])
iS = int(inp[12:14])"""

iY = t.tm_year
iM = t.tm_mon
iD = t.tm_mday
ih = t.tm_hour
im = t.tm_min
iS = t.tm_sec
LST, diff = lst(iY, iM, iD, ih, im, iS, longitude)

app = Flask(__name__)
CORS(app)

@app.route('/', methods=['GET'])
def index():
	if request.method == 'GET':
		with open("stars.json", "r") as f:
			stars = json.loads(f.read())
			f.close()
		
		stars_list = list(stars)
		
		stars_dict = {}
		
		for i in stars_list:
			stars_dict[i] = stars[i]["NDEC"]
		
		stars_dict = abs_sort(stars_dict)
		stars_list = list(stars_dict)
		
		alt = 0
		i = -1
		while alt < 30:
			i += 1
			rastr = stars[stars_list[i]]['RA']
			decstr = stars[stars_list[i]]['DEC']
		
			rah = int(rastr[0:2])
			ram = int(rastr[2:4])
			ras = float(rastr[4:])
		
			decd = int(decstr[0:3])
			decm = int(decstr[3:5])
			decs = int(decstr[5:])
		
			raJ200 = rah * 15 + ram * 15 / 60 + ras * 15 / 3600
			decJ200 = decd + decm / 60 + decs / 3600
		
			ras, decs = precess(raJ200, decJ200, iY, ras, decs)
		
			ra = rah * 15 + ram * 15 / 60 + ras * 15 / 3600
			dec = decd + decm / 60 + decs / 3600
		
			ha = LST - ra
			alt, az = altaz(ha, dec, latitude)
		return str(stars_list[i])+"|"+str(ra)+"|"+str(dec)


app.run(host='0.0.0.0', port=80)