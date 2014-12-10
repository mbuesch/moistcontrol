#
# Moisture control - Global status/configuration widgets
#
# Copyright (c) 2013 Michael Buesch <m@bues.ch>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

from pymoistcontrol.util import *
from pymoistcontrol.potwidgets import *
from pymoistcontrol.adcwidgets import *
from pymoistcontrol.dayofweek import *
from pymoistcontrol.bitindicator import *


class GlobalConfigWidget(QWidget):
	"""Global configuration widget."""

	# Signal: Emitted, if a configuration item changed.
	configChanged = Signal()
	# Signal: Emitted, if the RTC date or time was edited.
	rtcEdited = Signal()

	def __init__(self, parent):
		"""Class constructor."""

		QWidget.__init__(self, parent)
		self.setLayout(QGridLayout(self))
		y = 0

		self.__shouldCheckRtc = True

		self.enableCheckBox = QCheckBox("Enable the regulator globally",
						self)
		self.layout().addWidget(self.enableCheckBox, y, 0, 1, 2)
		y += 1

		self.watchdogWarning = QLabel()
		palette = self.watchdogWarning.palette()
		palette.setColor(QPalette.WindowText, QColor(255, 0, 0))
		self.watchdogWarning.setPalette(palette)
		self.watchdogWarning.hide()
		self.watchdogWarningPotNumbers = []
		self.layout().addWidget(self.watchdogWarning, y, 0, 1, 2)
		y += 1

		label = QLabel("RTC time:", self)
		self.layout().addWidget(label, y, 0)
		hbox = QHBoxLayout()
		self.rtcEdit = QDateTimeEdit(self)
		self.rtcEdit.setDisplayFormat("yyyy.MM.dd hh:mm:ss")
		self.rtcEdit.setReadOnly(True)
		hbox.addWidget(self.rtcEdit)
		self.rtcEditCheckBox = QCheckBox("edit", self)
		hbox.addWidget(self.rtcEditCheckBox)
		self.layout().addLayout(hbox, y, 1)
		y += 1

		self.statWidgets = []
		for i in range(MAX_NR_FLOWERPOTS):
			label = QLabel("Pot %d:" % (i + 1), self)
			self.layout().addWidget(label, y, 0)
			potStatWidget = PotShortStatusWidget(i, self)
			self.statWidgets.append(potStatWidget)
			self.layout().addWidget(potStatWidget, y, 1)
			y += 1

		self.advancedCheckBox = QCheckBox("Advanced", self)
		self.layout().addWidget(self.advancedCheckBox, y, 0, 1, 2)
		y += 1

		self.advancedGroup = QGroupBox(self)
		self.advancedGroup.setLayout(QGridLayout())
		self.layout().addWidget(self.advancedGroup, y, 0, 1, 2)
		y += 1

		label = QLabel("Lowest possible ADC value:", self)
		self.advancedGroup.layout().addWidget(label, 0, 0)
		self.lowestSensorSpin = ADCSpinBox(self)
		self.advancedGroup.layout().addWidget(self.lowestSensorSpin, 0, 1)

		label = QLabel("Highest possible ADC value:", self)
		self.advancedGroup.layout().addWidget(label, 1, 0)
		self.highestSensorSpin = ADCSpinBox(self)
		self.advancedGroup.layout().addWidget(self.highestSensorSpin, 1, 1)

		self.ignoreChanges = 0
		self.enableCheckBox.stateChanged.connect(self.__enableChanged)
		self.rtcEditCheckBox.stateChanged.connect(self.__rtcEditChanged)
		self.advancedCheckBox.stateChanged.connect(self.__advancedChanged)
		self.lowestSensorSpin.valueChanged.connect(self.__lowestSensorChanged)
		self.highestSensorSpin.valueChanged.connect(self.__highestSensorChanged)

		self.__advancedChanged(self.advancedCheckBox.checkState())

	def globalEnableActive(self):
		return self.enableCheckBox.checkState() == Qt.Checked

	def getRtcDateTime(self):
		return self.rtcEdit.dateTime()

	def lowestRawSensorVal(self):
		return self.lowestSensorSpin.value()

	def highestRawSensorVal(self):
		return self.highestSensorSpin.value()

	def __lowestSensorChanged(self, newValue):
		if not self.ignoreChanges:
			self.configChanged.emit()

	def __highestSensorChanged(self, newValue):
		if not self.ignoreChanges:
			self.configChanged.emit()

	def __advancedChanged(self, newState):
		if newState == Qt.Checked:
			self.advancedGroup.show()
		else:
			self.advancedGroup.hide()

	def __enableChanged(self, newState):
		if self.ignoreChanges:
			return
		if newState != Qt.Checked:
			res = QMessageBox.question(self,
				"Disable globally?",
				"Do you really want to disable the "
				"controller globally?\n"
				"Watering of all pots will be stopped.",
				QMessageBox.Yes | QMessageBox.No)
			if res != QMessageBox.Yes:
				self.enableCheckBox.setCheckState(Qt.Checked)
				return
		self.configChanged.emit()

	def __rtcEditChanged(self, newState):
		if newState == Qt.Checked:
			self.rtcEdit.setReadOnly(False)
		else:
			self.rtcEdited.emit()
			self.rtcEdit.setReadOnly(True)

	def handleGlobalConfMessage(self, msg):
		self.ignoreChanges += 1
		if msg.flags & msg.CONTR_FLG_ENABLE:
			self.enableCheckBox.setCheckState(Qt.Checked)
		else:
			self.enableCheckBox.setCheckState(Qt.Unchecked)
		self.lowestSensorSpin.setValue(msg.sensor_lowest_value)
		self.highestSensorSpin.setValue(msg.sensor_highest_value)
		self.__shouldCheckRtc = True
		self.ignoreChanges -= 1

	def handlePotStateMessage(self, msg):
		self.ignoreChanges += 1
		self.statWidgets[msg.pot_number].handlePotStateMessage(msg)
		self.ignoreChanges -= 1

	def __updateWatchdogWarning(self):
		if self.watchdogWarningPotNumbers:
			self.watchdogWarning.setText(
				"WARNING: The watering watchdog was triggered.\n"\
				"Watering is disabled on: %s" %\
				", ".join("pot %d" % (n + 1) for n in self.watchdogWarningPotNumbers))
			self.watchdogWarning.show()
		else:
			self.watchdogWarning.setText("")
			self.watchdogWarning.hide()

	def handlePotRemStateMessage(self, msg):
		self.ignoreChanges += 1
		if msg.flags & msg.POT_REMFLG_WDTRIGGER:
			if msg.pot_number not in self.watchdogWarningPotNumbers:
				self.watchdogWarningPotNumbers.append(msg.pot_number)
				self.__updateWatchdogWarning()
		else:
			if msg.pot_number in self.watchdogWarningPotNumbers:
				self.watchdogWarningPotNumbers.remove(msg.pot_number)
				self.__updateWatchdogWarning()
		self.ignoreChanges -= 1

	def handlePotConfMessage(self, msg):
		self.ignoreChanges += 1
		self.statWidgets[msg.pot_number].enableMessageHandling(msg.flags & msg.POT_FLG_ENABLED)
		self.ignoreChanges -= 1

	def handlePotEnableChange(self, potNumber, enabled):
		self.statWidgets[potNumber].enableMessageHandling(enabled)

	def __syncRtc(self):
		# Sync RTC with PC clock.
		self.rtcEditCheckBox.setCheckState(Qt.Unchecked)
		self.rtcEdit.setDateTime(QDateTime.currentDateTime())
		self.rtcEdited.emit()

	def handleRtcMessage(self, msg):
		self.ignoreChanges += 1
		date = QDate(2000 + msg.year, msg.month + 1, msg.day + 1)
		time = QTime(msg.hour, msg.minute, msg.second)
		dateTime = QDateTime(date, time)
		if self.rtcEdit.isReadOnly():
			self.rtcEdit.setDateTime(dateTime)
			if self.__shouldCheckRtc:
				error = QDateTime.currentDateTime().secsTo(dateTime)
				if abs(error) > 30:
					res = QMessageBox.question(self,
						"Fix RTC time drift?",
						"The RTC is off by %d seconds.\n"
						"Do you want to synchronize the RTC "
						"with the PC's clock?" % error,
						QMessageBox.Yes | QMessageBox.No,
						QMessageBox.Yes)
					if res == QMessageBox.Yes:
						self.__syncRtc()
				self.__shouldCheckRtc = False
		self.ignoreChanges -= 1
