#
#  Moistcontrol - Log messages
#
#  Copyright (c) 2013 Michael Buesch <m@bues.ch>
#  Licensed under the terms of the GNU General Public License version 2.
#

from pymoistcontrol.util import *

import cgi


def controllerStateName(stateNum):
	"""Get the name string for a controller state number."""

	try:
		stateName = {
			0 : "POT_IDLE",
			1 : "POT_START_MEASUREMENT",
			2 : "POT_MEASURING",
			3 : "POT_WAITING_FOR_VALVE",
		}[stateNum]
	except KeyError:
		stateName = "%d" % stateNum
	return stateName

class LogItem(object):
	"""Base class for log data."""

	# Types
	LOG_TYPE_MASK		= 0x7F
	LOG_ERROR		= 0
	LOG_INFO		= 1
	LOG_SENSOR_DATA		= 2

	# Flags
	LOG_FLAGS_MASK		= 0x80
	LOG_OVERFLOW		= 0x80

	def __init__(self, logType, flags, timestamp, payload=b'\x00'*6):
		"""Class constructor."""

		self.logType = logType
		self.flags = flags
		self.timestamp = timestamp
		self.payload = payload

	@classmethod
	def fromBytes(cls, b):
		"""Create a log item from raw bytes."""

		try:
			logType = b[0] & cls.LOG_TYPE_MASK
			flags = b[0] & cls.LOG_FLAGS_MASK
			timestamp = b[1] |\
				    (b[2] << 8) |\
				    (b[3] << 16) |\
				    (b[4] << 24)
			if logType == cls.LOG_ERROR:
				return LogItemError(flags = flags,
						    timestamp = timestamp,
						    errorCode = b[5],
						    errorData = b[6])
			if logType == cls.LOG_INFO:
				return LogItemInfo(flags = flags,
						   timestamp = timestamp,
						   infoCode = b[5],
						   infoData = b[6])
			elif logType == cls.LOG_SENSOR_DATA:
				sv = b[5] | (b[6] << 8)
				sensorValue = sv & 0x3FF
				sensorNr = (sv >> 10) & 0x3F
				return LogItemSensorData(flags = flags,
							 timestamp = timestamp,
							 sensorNr = sensorNr,
							 sensorValue = sensorValue)
			else:
				raise Error("Unknown log item type: %d" % logType)
		except IndexError as e:
			raise Error("Log item length error")

	def getBytes(self):
		"""Get the raw bytes from this log item."""

		b = bytes([ (self.logType & self.LOG_TYPE_MASK) |\
			    (self.flags & self.LOG_FLAGS_MASK),
			    self.timestamp & 0xFF,
			    (self.timestamp >> 8) & 0xFF,
			    (self.timestamp >> 16) & 0xFF,
			    (self.timestamp >> 24) & 0xFF, ])
		b += self.payload
		return b

	def getText(self):
		"""Get a string representation of this log item."""

		# Must be overridden in the subclass.
		raise NotImplementedError

	def getDateTime(self):
		"""Get the timestamp of this log item as QDateTime."""

		second = clamp((self.timestamp >> 0) & 0x3F, 0, 59)
		minute = clamp((self.timestamp >> 6) & 0x3F, 0, 59)
		hour = clamp((self.timestamp >> 12) & 0x1F, 0, 23)
		day = clamp((self.timestamp >> 17) & 0x1F, 0, 30)
		month = clamp((self.timestamp >> 22) & 0x0F, 0, 11)
		year = clamp((self.timestamp >> 26) & 0x3F, 0, 99)
		return QDateTime(QDate(year + 2000, month + 1, day + 1),
				 QTime(hour, minute, second))

	@property
	def overflow(self):
		"""Returns True, if the overflow flag is set."""

		return bool(self.flags & self.LOG_OVERFLOW)

class LogItemError(LogItem):
	LOG_ERR_SENSOR			= 0

	def __init__(self, flags, timestamp, errorCode, errorData):
		"""Class constructor."""

		LogItem.__init__(self, LogItem.LOG_ERROR, flags, timestamp)
		self.errorCode = errorCode
		self.errorData = errorData

	def getBytes(self):
		"""Get the raw bytes from this log item."""

		self.payload = bytes([ self.errorCode,
				       self.errorData ])
		return LogItem.getBytes(self)

	def getText(self):
		"""Get a string representation of this log item."""

		if self.errorCode == self.LOG_ERR_SENSOR:
			potNumber = self.errorData & 0xF
			return "Error: Measurement at pot %d returned "\
				"an implausible result." %\
				(potNumber + 1)
		else:
			return "Error %d (%d) occurred" %\
				(self.errorCode, self.errorData)

class LogItemInfo(LogItem):
	LOG_INFO_DEBUG			= 0
	LOG_INFO_CONTSTATCHG		= 1
	LOG_INFO_WATERINGCHG		= 2

	def __init__(self, flags, timestamp, infoCode, infoData):
		"""Class constructor."""

		LogItem.__init__(self, LogItem.LOG_ERROR, flags, timestamp)
		self.infoCode = infoCode
		self.infoData = infoData

	def getBytes(self):
		"""Get the raw bytes from this log item."""

		self.payload = bytes([ self.infoCode,
				       self.infoData ])
		return LogItem.getBytes(self)

	def getText(self):
		"""Get a string representation of this log item."""

		if self.infoCode == self.LOG_INFO_DEBUG:
			return "Debug message: %d" % self.infoData
		elif self.infoCode == self.LOG_INFO_CONTSTATCHG:
			potNumber = self.infoData & 0xF
			state = (self.infoData >> 4) & 0xF
			stateName = controllerStateName(state)
			return "Pot %d state machine transition to %s" %\
				(potNumber + 1, stateName)
		elif self.infoCode == self.LOG_INFO_WATERINGCHG:
			potNumber = self.infoData & 0xF
			state = bool(self.infoData & 0x80)
			return "Pot %d watering %s" % \
				(potNumber + 1,
				 "started" if state else "stopped")
		else:
			return "Info message %d (%d)" %\
				(self.infoCode, self.infoData)

class LogItemSensorData(LogItem):
	def __init__(self, flags, timestamp, sensorNr, sensorValue):
		"""Class constructor."""

		LogItem.__init__(self, LogItem.LOG_SENSOR_DATA, flags, timestamp)
		self.sensorNr = sensorNr
		self.sensorValue = sensorValue

	def getBytes(self):
		"""Get the raw bytes from this log item."""

		sv = (self.sensorValue & 0x3FF) | ((self.sensorNr & 0x3F) << 10)
		self.payload = bytes([ sv & 0xFF,
				       (sv >> 8) & 0xFF, ])
		return LogItem.getBytes(self)

	def getText(self):
		"""Get a string representation of this log item."""

		return "Pot %d sensor ADC value measured: %d" %\
			(self.sensorNr + 1, self.sensorValue)

class LogWidget(QWidget):
	"""Message log widget"""

	@staticmethod
	def htmlEscape(plaintext):
		return cgi.escape(plaintext)

	def __init__(self, parent):
		"""Class constructor."""

		QWidget.__init__(self, parent)
		self.setLayout(QGridLayout(self))
		self.layout().setContentsMargins(QMargins())

		self.text = QTextEdit(self)
		self.text.setReadOnly(True)
		self.layout().addWidget(self.text, 0, 0)

		self.clear()

	def clear(self):
		"""Remove all log messages."""

		self.messages = []
		self.text.clear()

	def __commitText(self):
		"""Write all messages to the text box."""

		limit = 100
		if len(self.messages) > limit:
			self.messages.pop(0)
			assert(len(self.messages) == limit)
		html = "<html><body>" + "".join(self.messages) + "</body></html>"
		self.text.setHtml(html)
		# Scroll to end
		scroll = self.text.verticalScrollBar()
		scroll.setTracking(True)
		scroll.setSliderPosition(scroll.maximum())
		scroll.setValue(scroll.maximum())

	def handleLogMessage(self, msg):
		"""Add a message to the log.
		'msg' is an instance of a LogItem subclass."""

		text = msg.logItem.getText()
		if not text:
			return
		if text.endswith("\n"):
			text = text[:-1]
		text = self.htmlEscape(text)
		time = msg.logItem.getDateTime().toString("yyyy.MM.dd hh:mm:ss")
		ovr = "&nbsp;QUEUE OVERFLOW" if msg.logItem.overflow else ""
		text = "<i>[%s%s]</i>&nbsp;&nbsp;%s<br />" % (time, ovr, text)
		self.messages.append(text)
		self.__commitText()
