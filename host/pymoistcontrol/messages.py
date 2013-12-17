#
#  Moistcontrol - serial messages
#
#  Copyright (c) 2013 Michael Buesch <m@bues.ch>
#  Licensed under the terms of the GNU General Public License version 2.
#

import time
import datetime
import configparser

from pymoistcontrol.comm import *
from pymoistcontrol.logging import *
from pymoistcontrol.util import *


class Message(SerialMessage):
	# Types
	MSG_LOG				= 0
	MSG_LOG_FETCH			= 1
	MSG_RTC				= 2
	MSG_RTC_FETCH			= 3
	MSG_CONTR_CONF			= 4
	MSG_CONTR_CONF_FETCH		= 5
	MSG_CONTR_POT_CONF		= 6
	MSG_CONTR_POT_CONF_FETCH	= 7
	MSG_CONTR_POT_STATE		= 8
	MSG_CONTR_POT_STATE_FETCH	= 9
	MSG_MAN_MODE			= 10
	MSG_MAN_MODE_FETCH		= 11

	@classmethod
	def fromRawMessage(cls, rawMsg):
		if not rawMsg:
			return None
		if rawMsg.getErrorCode() != SerialMessage.COMM_ERR_OK:
			msg = Message()
			msg.copyHeaderFrom(rawMsg)
			return msg
		try:
			msgId = rawMsg.payload[0]
			if msgId == cls.MSG_LOG:
				msg = MsgLog(logItem = LogItem.fromBytes(rawMsg.payload[1:]))
			elif msgId == cls.MSG_LOG_FETCH:
				msg = MsgLogFetch()
			elif msgId == cls.MSG_RTC:
				msg = MsgRtc(
					second = clamp(rawMsg.payload[1], 0, 59),
					minute = clamp(rawMsg.payload[2], 0, 59),
					hour = clamp(rawMsg.payload[3], 0, 23),
					day = clamp(rawMsg.payload[4], 0, 30),
					month = clamp(rawMsg.payload[5], 0, 11),
					year = clamp(rawMsg.payload[6], 0, 99),
					day_of_week = clamp(rawMsg.payload[7], 0, 6))
			elif msgId == cls.MSG_RTC_FETCH:
				msg = MsgRtcFetch()
			elif msgId == cls.MSG_CONTR_CONF:
				msg = MsgContrConf(
					flags = rawMsg.payload[1],
					sensor_lowest_value = rawMsg.payload[2] |
							      (rawMsg.payload[3] << 8),
					sensor_highest_value = rawMsg.payload[4] |
							       (rawMsg.payload[5] << 8))
			elif msgId == cls.MSG_CONTR_CONF_FETCH:
				msg = MsgContrConfFetch()
			elif msgId == cls.MSG_CONTR_POT_CONF:
				msg = MsgContrPotConf(
					pot_number = rawMsg.payload[1],
					flags = rawMsg.payload[2],
					min_threshold = rawMsg.payload[3],
					max_threshold = rawMsg.payload[4],
					start_time = rawMsg.payload[5] |
						     (rawMsg.payload[6] << 8),
					end_time = rawMsg.payload[7] |
						   (rawMsg.payload[8] << 8),
					dow_on_mask = rawMsg.payload[9])
			elif msgId == cls.MSG_CONTR_POT_CONF_FETCH:
				msg = MsgContrPotConfFetch(pot_number = rawMsg.payload[1])
			elif msgId == cls.MSG_CONTR_POT_STATE:
				msg = MsgContrPotState(
					pot_number = rawMsg.payload[1],
					state_id = rawMsg.payload[2],
					is_watering = rawMsg.payload[3],
					last_measured_raw_value = rawMsg.payload[4] |
								  (rawMsg.payload[5] << 8),
					last_measured_value = rawMsg.payload[6])
			elif msgId == cls.MSG_CONTR_POT_STATE_FETCH:
				msg = MsgContrPotStateFetch(pot_number = rawMsg.payload[1])
			else:
				raise Error("Unknown message ID: %d" % msgId)
			msg.copyHeaderFrom(rawMsg)
			return msg
		except IndexError as e:
			raise Error("Message length error")

	def __init__(self, fc=0, serialComm=None):
		SerialMessage.__init__(self, fc = fc,
				       serialComm = serialComm)

	def getType(self):
		return None

	def getPayload(self):
		return b""

	def toText(self):
		raise NotImplementedError

	def fromText(self):
		raise NotImplementedError

class MsgLog(Message):
	def __init__(self, logItem):
		self.logItem = logItem
		Message.__init__(self)

	def getType(self):
		return self.MSG_LOG

	def getPayload(self):
		payload = bytes([ self.getType() ])
		payload += self.logItem.getBytes()
		return payload

class MsgLogFetch(Message):
	def __init__(self):
		Message.__init__(self, fc = Message.COMM_FC_REQ_ACK)

	def getType(self):
		return self.MSG_LOG_FETCH

	def getPayload(self):
		return bytes([ self.getType(), ])

class MsgRtc(Message):
	def __init__(self,
		     second = 0, minute = 0, hour = 0, day = 0,
		     month = 0, year = 0, day_of_week = 0):
		self.second = second
		self.minute = minute
		self.hour = hour
		self.day = day
		self.month = month
		self.year = year
		self.day_of_week = day_of_week
		Message.__init__(self)

	def getType(self):
		return self.MSG_RTC

	def getPayload(self):
		return bytes([ self.getType(),
			       clamp(self.second, 0, 59),
			       clamp(self.minute, 0, 59),
			       clamp(self.hour, 0, 23),
			       clamp(self.day, 0, 30),
			       clamp(self.month, 0, 11),
			       clamp(self.year, 0, 99),
			       clamp(self.day_of_week, 0, 6), ])

	def toText(self):
		return "[RTC]\n" \
		       "second=%d\n" \
		       "minute=%d\n" \
		       "hour=%d\n" \
		       "day=%d\n" \
		       "month=%d\n" \
		       "year=%d\n" \
		       "day_of_week=%d\n" % \
		       (self.second,
			self.minute,
			self.hour,
			self.day,
			self.month,
			self.year,
			self.day_of_week)

	def fromText(self, text):
		try:
			p = configparser.ConfigParser()
			p.read_string(text)
			self.second = clamp(p.getint("RTC", "second"),
					    0, 59)
			self.minute = clamp(p.getint("RTC", "minute"),
					    0, 59)
			self.hour = clamp(p.getint("RTC", "hour"),
					  0, 23)
			self.day = clamp(p.getint("RTC", "day"),
					 0, 30)
			self.month = clamp(p.getint("RTC", "month"),
					   0, 11)
			self.year = clamp(p.getint("RTC", "year"),
					  0, 99)
			self.day_of_week = clamp(p.getint("RTC", "day_of_week"),
						 0, 6)
		except configparser.Error as e:
			raise Error(str(e))

class MsgRtcFetch(Message):
	def __init__(self):
		Message.__init__(self, fc = Message.COMM_FC_REQ_ACK)

	def getType(self):
		return self.MSG_RTC_FETCH

	def getPayload(self):
		return bytes([ self.getType(), ])

class MsgContrConf(Message):
	CONTR_FLG_ENABLE	= 0x01

	def __init__(self,
		     flags = 0,
		     sensor_lowest_value = 0,
		     sensor_highest_value = 0):
		self.flags = flags
		self.sensor_lowest_value = sensor_lowest_value
		self.sensor_highest_value = sensor_highest_value
		Message.__init__(self)

	def getType(self):
		return self.MSG_CONTR_CONF

	def getPayload(self):
		return bytes([ self.getType(),
			       self.flags & 0xFF,
			       self.sensor_lowest_value & 0xFF,
			       (self.sensor_lowest_value >> 8) & 0xFF,
			       self.sensor_highest_value & 0xFF,
			       (self.sensor_highest_value >> 8) & 0xFF, ])

	def toText(self):
		return "[GLOBAL_CONFIG]\n" \
		       "flags=%d\n" \
		       "sensor_lowest_value=%d\n" \
		       "sensor_highest_value=%d\n" % \
		       (self.flags,
			self.sensor_lowest_value,
			self.sensor_highest_value)

	def fromText(self, text):
		try:
			p = configparser.ConfigParser()
			p.read_string(text)
			self.flags = p.getint("GLOBAL_CONFIG", "flags")
			self.sensor_lowest_value = p.getint("GLOBAL_CONFIG",
							    "sensor_lowest_value")
			self.sensor_highest_value = p.getint("GLOBAL_CONFIG",
							     "sensor_highest_value")
		except configparser.Error as e:
			raise Error(str(e))

class MsgContrConfFetch(Message):
	def __init__(self):
		Message.__init__(self, fc = Message.COMM_FC_REQ_ACK)

	def getType(self):
		return self.MSG_CONTR_CONF_FETCH

	def getPayload(self):
		return bytes([ self.getType(), ])

class MsgContrPotConf(Message):
	POT_FLG_ENABLED		= 0x01
	POT_FLG_LOG		= 0x02
	POT_FLG_LOGVERBOSE	= 0x04

	@classmethod
	def toTimeOfDay(cls, hours, minutes, seconds):
		return (seconds + (minutes * 60) + (hours * 60 * 60)) // 2

	@classmethod
	def fromTimeOfDay(cls, value):
		if value > cls.toTimeOfDay(23, 59, 59):
			value = cls.toTimeOfDay(23, 59, 59)
		value *= 2
		hours = clamp(value // (60 * 60), 0, 23)
		value -= hours * 60 * 60
		minutes = clamp(value // 60, 0, 59)
		value -= minutes * 60
		seconds = clamp(value, 0, 59)
		return (hours, minutes, seconds)

	def __init__(self,
		     pot_number,
		     flags = 0,
		     min_threshold = 0,
		     max_threshold = 0,
		     start_time = 0,
		     end_time = 0,
		     dow_on_mask = 0):
		self.pot_number = pot_number
		self.flags = flags
		self.min_threshold = min_threshold
		self.max_threshold = max_threshold
		self.start_time = start_time
		self.end_time = end_time
		self.dow_on_mask = dow_on_mask
		Message.__init__(self)

	def getType(self):
		return self.MSG_CONTR_POT_CONF

	def getPayload(self):
		return bytes([ self.getType(),
			       self.pot_number & 0xFF,
			       self.flags & 0xFF,
			       self.min_threshold & 0xFF,
			       self.max_threshold & 0xFF,
			       self.start_time & 0xFF,
			       (self.start_time >> 8) & 0xFF,
			       self.end_time & 0xFF,
			       (self.end_time >> 8) & 0xFF,
			       self.dow_on_mask & 0xFF, ])

	def toText(self):
		return "[POT_%d_CONFIG]\n" \
		       "flags=%d\n" \
		       "min_threshold=%d\n" \
		       "max_threshold=%d\n" \
		       "start_time=%d\n" \
		       "end_time=%d\n" \
		       "dow_on_mask=%d\n" % \
		       (self.pot_number,
			self.flags,
			self.min_threshold,
			self.max_threshold,
			self.start_time,
			self.end_time,
			self.dow_on_mask)

	def fromText(self, text):
		try:
			p = configparser.ConfigParser()
			p.read_string(text)
			self.flags = p.getint("POT_%d_CONFIG" % self.pot_number,
					      "flags")
			self.min_threshold = p.getint("POT_%d_CONFIG" % self.pot_number,
						      "min_threshold")
			self.max_threshold = p.getint("POT_%d_CONFIG" % self.pot_number,
						      "max_threshold")
			self.start_time = p.getint("POT_%d_CONFIG" % self.pot_number,
						   "start_time")
			self.end_time = p.getint("POT_%d_CONFIG" % self.pot_number,
						 "end_time")
			self.dow_on_mask = p.getint("POT_%d_CONFIG" % self.pot_number,
						    "dow_on_mask")
		except configparser.Error as e:
			raise Error(str(e))

class MsgContrPotConfFetch(Message):
	def __init__(self, pot_number):
		self.pot_number = pot_number
		Message.__init__(self, fc = Message.COMM_FC_REQ_ACK)

	def getType(self):
		return self.MSG_CONTR_POT_CONF_FETCH

	def getPayload(self):
		return bytes([ self.getType(),
			       self.pot_number & 0xFF, ])

class MsgContrPotState(Message):
	def __init__(self,
		     pot_number,
		     state_id = 0,
		     is_watering = 0,
		     last_measured_raw_value = 0,
		     last_measured_value = 0):
		self.pot_number = pot_number
		self.state_id = state_id
		self.is_watering = is_watering
		self.last_measured_raw_value = last_measured_raw_value
		self.last_measured_value = last_measured_value
		Message.__init__(self)

	def getType(self):
		return self.MSG_CONTR_POT_STATE

	def getPayload(self):
		return bytes([ self.getType(),
			       self.pot_number & 0xFF,
			       self.state_id & 0xFF,
			       self.is_watering & 0xFF,
			       self.last_measured_raw_value & 0xFF,
			       (self.last_measured_raw_value >> 8) & 0xFF,
			       self.last_measured_value & 0xFF, ])

class MsgContrPotStateFetch(Message):
	def __init__(self, pot_number):
		self.pot_number = pot_number
		Message.__init__(self, fc = Message.COMM_FC_REQ_ACK)

	def getType(self):
		return self.MSG_CONTR_POT_STATE_FETCH

	def getPayload(self):
		return bytes([ self.getType(),
			       self.pot_number & 0xFF, ])

class MsgManMode(Message):
	def __init__(self,
		     force_stop_watering_mask = 0,
		     valve_manual_mask = 0,
		     valve_manual_state = 0):
		self.force_stop_watering_mask = force_stop_watering_mask
		self.valve_manual_mask = valve_manual_mask
		self.valve_manual_state = valve_manual_state
		Message.__init__(self)

	def getType(self):
		return self.MSG_MAN_MODE

	def getPayload(self):
		return bytes([ self.getType(),
			       self.force_stop_watering_mask & 0xFF,
			       self.valve_manual_mask & 0xFF,
			       self.valve_manual_state & 0xFF, ])

class MsgManModeFetch(Message):
	def __init__(self):
		Message.__init__(self, fc = Message.COMM_FC_REQ_ACK)

	def getType(self):
		return self.MSG_MAN_MODE_FETCH

	def getPayload(self):
		return bytes([ self.getType(), ])
