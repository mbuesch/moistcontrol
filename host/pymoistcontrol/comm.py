#
#  Serial communication
#
#  Copyright (c) 2013 Michael Buesch <m@bues.ch>
#  Licensed under the terms of the GNU General Public License version 2.
#

import sys
import struct
import time

try:
	import serial
except ImportError as e:
	print("PLEASE INSTALL pyserial (https://pypi.python.org/pypi/pyserial)")
	input("Press enter to exit.")
	sys.exit(1)


class SerialError(Exception):
	pass

class SerialMessage(object):
	SER_HDR_LEN		= 4
	SER_FCS_LEN		= 2

	# Frame control
	COMM_FC_RESET		= 0x01
	COMM_FC_REQ_ACK		= 0x02
	COMM_FC_ACK		= 0x04

	COMM_FC_ERRCODE		= 0xC0
	COMM_FC_ERRCODE_SHIFT	= 6

	# Frame error codes
	COMM_ERR_OK		= 0
	COMM_ERR_FAIL		= 1
	COMM_ERR_FCS		= 2
	COMM_ERR_Q		= 3

	@classmethod
	def crc16Update(cls, crc, data):
		crc ^= data
		for i in range(8):
			if crc & 1:
				crc = (crc >> 1) ^ 0xA001
			else:
				crc = crc >> 1
		return crc

	@classmethod
	def crc16(cls, data):
		crc = 0xFFFF
		for d in data:
			crc = cls.crc16Update(crc, d)
		return crc ^ 0xFFFF

	def __init__(self, fc=0, payload=b'', serialComm=None):
		self.fc = fc
		self.seq = 0
		self.sa = 0
		self.da = 0
		self.payload = payload
		self.fcs = 0
		self.serialComm = serialComm

	def copyHeaderFrom(self, fromMsg):
		self.fc = fromMsg.fc
		self.seq = fromMsg.seq
		self.sa = fromMsg.sa
		self.da = fromMsg.da
		self.serialComm = fromMsg.serialComm

	def calcFrameDuration(self):
		"""Returns the duration of this frame, in seconds (float)."""
		nrBytes = self.SER_HDR_LEN +\
			  self.serialComm.payloadLen +\
			  self.SER_FCS_LEN
		symbolsPerByte = 1 + self.serialComm.serial.getByteSize() +\
				 (0 if (self.serialComm.serial.getParity() == serial.PARITY_NONE) else 1) +\
				 self.serialComm.serial.getStopbits()
		return (nrBytes * symbolsPerByte) / self.serialComm.serial.getBaudrate()

	def getErrorCode(self):
		return (self.fc & self.COMM_FC_ERRCODE) >> self.COMM_FC_ERRCODE_SHIFT

	def setSerialComm(self, serialComm):
		self.serialComm = serialComm

	def __calcFcs(self):
		return self.crc16(self.__getBytes()[0 : -self.SER_FCS_LEN])

	def getPayload(self):
		return self.payload

	def __getBytes(self):
		payload = self.getPayload()
		assert(len(payload) <= self.serialComm.payloadLen)
		if len(payload) < self.serialComm.payloadLen:
			payload += b'\x00' * (self.serialComm.payloadLen - len(payload))

		fmtString = "%dB" % (self.SER_HDR_LEN +
				     self.serialComm.payloadLen +
				     self.SER_FCS_LEN)
		arguments = [ self.fc & 0xFF,
			      self.seq & 0xFF,
			      (self.sa & 0xF) | ((self.da & 0xF) << 4),
			      0, ]
		arguments.extend(list(payload))
		arguments.extend([ self.fcs & 0xFF,
				   (self.fcs >> 8) & 0xFF, ])
		return struct.pack(fmtString, *arguments)

	def getBytes(self):
		self.fcs = self.__calcFcs()
		return self.__getBytes()

	def __setBytes(self, data):
		if len(data) != self.SER_HDR_LEN + self.serialComm.payloadLen + self.SER_FCS_LEN:
			raise SerialError("Msg: Invalid number of bytes")
		fmtString = "%dB" % (self.SER_HDR_LEN +
				     self.serialComm.payloadLen +
				     self.SER_FCS_LEN)
		fields = struct.unpack(fmtString, data)
		self.fc = fields[0]
		self.seq = fields[1]
		self.sa = fields[2] & 0xF
		self.da = (fields[2] >> 4) & 0xF
		self.payload = bytes(fields[self.SER_HDR_LEN :
					    self.SER_HDR_LEN + self.serialComm.payloadLen])
		fcsLow = fields[self.SER_HDR_LEN +
				self.serialComm.payloadLen]
		fcsHigh = fields[self.SER_HDR_LEN +
				 self.serialComm.payloadLen + 1]
		self.fcs = (fcsLow & 0xFF) | ((fcsHigh & 0xFF) << 8)

	def setBytes(self, data):
		self.__setBytes(data)
		if self.__calcFcs() != self.fcs:
			raise SerialError("Msg: FCS mismatch")

	def __repr__(self):
		ret = "SerialMessage: "
		for b in self.getBytes():
			ret += "%02X" % b
		return ret

	def send(self, destinationAddress=0, serialComm=None):
		if not serialComm:
			serialComm = self.serialComm
		assert(serialComm)
		serialComm.send(self, destinationAddress)

class SerialComm(object):
	def __init__(self, device, baudrate=9600, nrbits=8,
		     parity=serial.PARITY_NONE, nrstop=1,
		     localAddress=1,
		     payloadLen=8,
		     debug=False):
		try:
			self.serial = serial.Serial(device, baudrate, nrbits,
						    parity, nrstop)
		except (serial.SerialException, OSError) as e:
			raise SerialError(str(e))
		self.localAddress = localAddress
		self.payloadLen = payloadLen
		self.sendDelay = 0
		self.seq = 0
		self.debug = debug

	def close(self):
		try:
			self.serial.close()
		except (serial.SerialException, OSError) as e:
			raise SerialError("Failed to close serial line: %s" % str(e))

	def setSendDelay(self, seconds):
		self.sendDelay = seconds

	def poll(self):
		try:
			messageLength = SerialMessage.SER_HDR_LEN +\
					self.payloadLen +\
					SerialMessage.SER_FCS_LEN
			if self.serial.inWaiting() < messageLength:
				return None
			while 1:
				b = self.serial.read(messageLength)
				msg = SerialMessage(serialComm = self)
				msg.setBytes(b)
				if msg.da == self.localAddress:
					if self.debug:
						print("Received raw message: %s" %\
						      str(msg))
					return msg
				if self.serial.inWaiting() < messageLength:
					return None
		except (serial.SerialException, OSError) as e:
			raise SerialError("Serial receive failed: %s" % str(e))

	def send(self, msg, destinationAddress=0):
		try:
			msg.sa = self.localAddress & 0xF
			msg.da = destinationAddress & 0xF
			msg.seq = self.seq
			self.seq = (self.seq + 1) & 0xFF
			msg.setSerialComm(self)
			if self.debug:
				print("Sending raw message: %s" % str(msg))
			data = msg.getBytes()
			if self.sendDelay:
				for b in data:
					self.serial.write(bytes((b,)))
					self.serial.flush()
					time.sleep(self.sendDelay)
			else:
				self.serial.write(data)
			self.serial.flush()
		except (serial.SerialException, OSError) as e:
			raise SerialError("Serial send failed: %s" % str(e))

	def sendSync(self, msg, destinationAddress=0, timeout=1.0):
		self.send(msg, destinationAddress)
		if msg.fc & msg.COMM_FC_REQ_ACK:
			timeoutCount = timeout
			while timeoutCount > 0.0:
				res = self.poll()
				if res is not None:
					return res
				time.sleep(0.01)
				timeoutCount -= 0.01
			raise SerialError("Serial send-sync failed: "
					  "Timeout of %.01f seconds exceed." %\
					  timeout)
		return None
