#
# Moisture control - Pot state/configuration widgets
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
from pymoistcontrol.dayofweek import *
from pymoistcontrol.bitindicator import *
from pymoistcontrol.messages import *


class PotShortStatusWidget(QWidget):
	"""The flower-pot status display widget."""

	def __init__(self, potNumber, parent):
		"""Class constructor."""
		QWidget.__init__(self, parent)
		self.potNumber = potNumber
		self.setLayout(QGridLayout(self))
		self.layout().setContentsMargins(QMargins())

		self.progressBar = QProgressBar(self)
		self.progressBar.setTextVisible(False)
		self.progressBar.setRange(0, 0xFF)
		self.progressBar.setTextVisible(False)
		self.layout().addWidget(self.progressBar, 0, 0)

		self.reset()
		self.enableMessageHandling(False)

	def handlePotStateMessage(self, msg):
		if self.messageHandlingEnabled:
			self.progressBar.setValue(msg.last_measured_value)

	def reset(self):
		self.progressBar.setValue(0)

	def enableMessageHandling(self, enabled):
		self.messageHandlingEnabled = enabled
		if not enabled:
			self.reset()

class PotWidget(QWidget):
	"""The flower-pot tab widget."""

	# Signal: Emitted, if a configuration item changed.
	#         The first parameter (int) is the pot number.
	configChanged = Signal(int)
	# Signal: Emitted, if a 'manual-mode' setting changed.
	manModeChanged = Signal()
	# Signal: Emitted, if a watchdog restart was requested.
	watchdogRestartReq = Signal(int)

	def __init__(self, potNumber, parent):
		"""Class constructor."""
		QWidget.__init__(self, parent)
		self.potNumber = potNumber
		self.setLayout(QGridLayout(self))

		y = 0

		self.enableCheckBox = QCheckBox("Enable pot %d regulator" %\
						(self.potNumber + 1),
						self)
		self.layout().addWidget(self.enableCheckBox, y, 0, 1, 2)
		y += 1

		self.watchdogGroup = QGroupBox("WARNING", self)
		self.watchdogGroup.setLayout(QGridLayout())
		label = QLabel("The watering watchdog was triggered.\n"
			       "Watering is DISABLED on this pot.",
			       self)
		palette = label.palette()
		palette.setColor(QPalette.WindowText, QColor(255, 0, 0))
		label.setPalette(palette)
		self.watchdogGroup.layout().addWidget(label, 0, 0)
		self.watchdogResetButton = QPushButton("Reset watchdog and restart controller",
						       self)
		self.watchdogGroup.layout().addWidget(self.watchdogResetButton, 1, 0)
		self.layout().addWidget(self.watchdogGroup, y, 0, 1, 2)
		y += 1

		label = QLabel("On weekday:")
		self.layout().addWidget(label, y, 0)
		self.dowEnable = DayOfWeekSelectWidget(self)
		self.layout().addWidget(self.dowEnable, y, 1)
		y += 1

		label = QLabel("Current moisture:", self)
		self.layout().addWidget(label, y, 0)
		self.statWidget = PotShortStatusWidget(potNumber, self)
		self.layout().addWidget(self.statWidget, y, 1)
		y += 1

		label = QLabel("Threshold:", self)
		self.layout().addWidget(label, y, 0)
		self.minThreshold = QSlider(Qt.Horizontal, self)
		self.minThreshold.setRange(0, 0xFF)
		self.minThreshold.setValue(0)
		self.layout().addWidget(self.minThreshold, y, 1)
		y += 1

		label = QLabel("Hysteresis:", self)
		self.layout().addWidget(label, y, 0)
		self.hyst = QSlider(Qt.Horizontal, self)
		self.hyst.setRange(0, 0xFF)
		self.hyst.setValue(0)
		self.layout().addWidget(self.hyst, y, 1)
		y += 1

		label = QLabel("Not before time:", self)
		self.layout().addWidget(label, y, 0)
		hbox = QHBoxLayout()
		self.startTimeCheckBox = QCheckBox("Enabled", self)
		hbox.addWidget(self.startTimeCheckBox)
		self.startTime = QTimeEdit(self)
		self.startTime.setDisplayFormat("hh:mm:ss")
		self.startTime.setEnabled(False)
		self.startTime.setTime(QTime(0, 0, 0))
		hbox.addWidget(self.startTime)
		hbox.addStretch()
		self.layout().addLayout(hbox, y, 1)
		y += 1

		label = QLabel("Not after time:", self)
		self.layout().addWidget(label, y, 0)
		hbox = QHBoxLayout()
		self.endTimeCheckBox = QCheckBox("Enabled", self)
		hbox.addWidget(self.endTimeCheckBox)
		self.endTime = QTimeEdit(self)
		self.endTime.setDisplayFormat("hh:mm:ss")
		self.endTime.setEnabled(False)
		self.endTime.setTime(QTime(23, 59, 59))
		hbox.addWidget(self.endTime)
		hbox.addStretch()
		self.layout().addLayout(hbox, y, 1)
		y += 1

		label = QLabel("Force:", self)
		self.layout().addWidget(label, y, 0)
		hbox = QHBoxLayout()
		self.forceOpenButton = QPushButton("&Open valve", self)
		hbox.addWidget(self.forceOpenButton)
		self.forceStartMeasurement = QPushButton("&Trigger measurement", self)
		hbox.addWidget(self.forceStartMeasurement)
		self.forceStopWateringButton = QPushButton("&Stop watering", self)
		self.forceStopWateringButton.setEnabled(False)
		hbox.addWidget(self.forceStopWateringButton)
		self.layout().addLayout(hbox, y, 1)
		y += 1


		self.advancedCheckBox = QCheckBox("Advanced", self)
		self.layout().addWidget(self.advancedCheckBox, y, 0, 1, 2)
		y += 1

		self.advancedGroup = QGroupBox(self)
		self.advancedGroup.setLayout(QGridLayout())
		self.layout().addWidget(self.advancedGroup, y, 0, 1, 2)
		y += 1

		yAdv = 0
		label = QLabel("Raw ADC value:", self)
		self.advancedGroup.layout().addWidget(label, yAdv, 0)
		self.rawAdc = QLabel(self)
		self.advancedGroup.layout().addWidget(self.rawAdc, yAdv, 1)
		yAdv += 1
		label = QLabel("Watering state:", self)
		self.advancedGroup.layout().addWidget(label, yAdv, 0)
		self.wateringIndi = BitIndicator(offText = "Not watering",
						 onText = "Watering",
						 parent = self)
		self.advancedGroup.layout().addWidget(self.wateringIndi, yAdv, 1)
		yAdv += 1
		label = QLabel("State machine:", self)
		self.advancedGroup.layout().addWidget(label, yAdv, 0)
		self.stateMachineText = QLabel(self)
		self.advancedGroup.layout().addWidget(self.stateMachineText, yAdv, 1)
		yAdv += 1
		self.logCheckBox = QCheckBox("Enable logging", self)
		self.advancedGroup.layout().addWidget(self.logCheckBox, yAdv, 0, 1, 2)
		yAdv += 1
		self.verboseLogCheckBox = QCheckBox("Enable verbose logging", self)
		self.advancedGroup.layout().addWidget(self.verboseLogCheckBox, yAdv, 0, 1, 2)

		self.layout().setRowStretch(y, 1)

		self.ignoreChanges = 0
		self.enableCheckBox.stateChanged.connect(self.__enableChanged)
		self.watchdogResetButton.released.connect(self.__watchdogResetPressed)
		self.dowEnable.changed.connect(self.__dowEnableChanged)
		self.logCheckBox.stateChanged.connect(self.__logChanged)
		self.verboseLogCheckBox.stateChanged.connect(self.__logVerboseChanged)
		self.minThreshold.valueChanged.connect(self.__minChanged)
		self.hyst.valueChanged.connect(self.__hystChanged)
		self.startTimeCheckBox.stateChanged.connect(self.__startTimeChanged)
		self.startTime.timeChanged.connect(self.__startTimeChanged)
		self.endTimeCheckBox.stateChanged.connect(self.__endTimeChanged)
		self.endTime.timeChanged.connect(self.__endTimeChanged)
		self.forceOpenButton.pressed.connect(self.__forceOpenPressed)
		self.forceOpenButton.released.connect(self.__forceOpenReleased)
		self.forceStartMeasurement.pressed.connect(self.__forceStartMeasPressed)
		self.forceStopWateringButton.pressed.connect(self.__forceStopWaterPressed)
		self.advancedCheckBox.stateChanged.connect(self.__advancedChanged)

		self.__advancedChanged(self.advancedCheckBox.checkState())
		self.resetState()

	def loggingEnabled(self):
		return self.logCheckBox.checkState() == Qt.Checked

	def verboseLoggingEnabled(self):
		return self.verboseLogCheckBox.checkState() == Qt.Checked

	def forceOpenValveActive(self):
		return self.forceOpenButton.isDown()

	def forceStartMeasActive(self):
		return self.forceStartMeasurement.isDown()

	def forceStopWateringActive(self):
		return self.forceStopWateringButton.isDown()

	def isEnabled(self):
		return self.enableCheckBox.checkState() == Qt.Checked

	def getDowEnableMask(self):
		return boolListToBitMask(self.dowEnable.getStates())

	def getStartTime(self):
		if self.startTimeCheckBox.checkState() == Qt.Checked:
			t = self.startTime.time()
			return MsgContrPotConf.toTimeOfDay(t.hour(),
							   t.minute(),
							   t.second())
		else:
			# Start time disabled.
			# Return the lowest possible time.
			return MsgContrPotConf.toTimeOfDay(0, 0, 0)

	def getEndTime(self):
		if self.endTimeCheckBox.checkState() == Qt.Checked:
			t = self.endTime.time()
			return MsgContrPotConf.toTimeOfDay(t.hour(),
							   t.minute(),
							   t.second())
		else:
			# End time disabled.
			# Return the highest possible time.
			return MsgContrPotConf.toTimeOfDay(23, 59, 59)

	def getMinThreshold(self):
		return self.minThreshold.value()

	def getMaxThreshold(self):
		return min(0xFF, self.minThreshold.value() + self.hyst.value())

	def __advancedChanged(self, newState):
		if newState == Qt.Checked:
			self.advancedGroup.show()
		else:
			self.advancedGroup.hide()

	def __enableChanged(self, newState):
		self.resetState()
		if self.ignoreChanges:
			return
		if newState != Qt.Checked:
			res = QMessageBox.question(self,
				"Disable pot?",
				"Do you really want to disable the "
				"controller on pot %d?" %\
				(self.potNumber + 1),
				QMessageBox.Yes | QMessageBox.No)
			if res != QMessageBox.Yes:
				self.enableCheckBox.setCheckState(Qt.Checked)
				return
		self.configChanged.emit(self.potNumber)

	def __watchdogResetPressed(self):
		self.watchdogRestartReq.emit(self.potNumber)

	def __dowEnableChanged(self):
		if not self.ignoreChanges:
			self.configChanged.emit(self.potNumber)

	def __logChanged(self, newState):
		if not self.ignoreChanges:
			self.configChanged.emit(self.potNumber)
		self.verboseLogCheckBox.setEnabled(newState == Qt.Checked)

	def __logVerboseChanged(self, newState):
		if not self.ignoreChanges:
			self.configChanged.emit(self.potNumber)

	def __minChanged(self, newValue):
		if not self.ignoreChanges:
			self.configChanged.emit(self.potNumber)

	def __hystChanged(self, newValue):
		if not self.ignoreChanges:
			self.configChanged.emit(self.potNumber)

	def __startTimeChanged(self):
		self.startTime.setEnabled(self.startTimeCheckBox.checkState() == Qt.Checked)
		if not self.ignoreChanges:
			self.configChanged.emit(self.potNumber)

	def __endTimeChanged(self):
		self.endTime.setEnabled(self.endTimeCheckBox.checkState() == Qt.Checked)
		if not self.ignoreChanges:
			if self.endTimeCheckBox.checkState() != Qt.Checked:
				self.ignoreChanges += 1
				self.endTime.setTime(QTime(23, 59, 59))
				self.ignoreChanges -= 1
			self.configChanged.emit(self.potNumber)

	def __forceOpenPressed(self):
		if not self.ignoreChanges:
			self.manModeChanged.emit()

	def __forceOpenReleased(self):
		if not self.ignoreChanges:
			self.manModeChanged.emit()

	def __forceStartMeasPressed(self):
		if not self.ignoreChanges:
			self.manModeChanged.emit()

	def __forceStopWaterPressed(self):
		if not self.ignoreChanges:
			self.manModeChanged.emit()

	def handlePotConfMessage(self, msg):
		self.ignoreChanges += 1
		assert(msg.pot_number == self.potNumber)
		if msg.flags & msg.POT_FLG_ENABLED:
			self.enableCheckBox.setCheckState(Qt.Checked)
		else:
			self.enableCheckBox.setCheckState(Qt.Unchecked)
		if msg.flags & msg.POT_FLG_LOG:
			self.logCheckBox.setCheckState(Qt.Checked)
		else:
			self.logCheckBox.setCheckState(Qt.Unchecked)
		if msg.flags & msg.POT_FLG_LOGVERBOSE:
			self.verboseLogCheckBox.setCheckState(Qt.Checked)
		else:
			self.verboseLogCheckBox.setCheckState(Qt.Unchecked)
		self.minThreshold.setValue(msg.min_threshold)
		self.hyst.setValue(msg.max_threshold - msg.min_threshold)
		h, m, s = MsgContrPotConf.fromTimeOfDay(msg.start_time)
		if MsgContrPotConf.toTimeOfDay(h, m, s) == MsgContrPotConf.toTimeOfDay(0, 0, 0):
			self.startTimeCheckBox.setCheckState(Qt.Unchecked)
		else:
			self.startTimeCheckBox.setCheckState(Qt.Checked)
			self.startTime.setTime(QTime(h, m, s))
		h, m, s = MsgContrPotConf.fromTimeOfDay(msg.end_time)
		if MsgContrPotConf.toTimeOfDay(h, m, s) == MsgContrPotConf.toTimeOfDay(23, 59, 59):
			self.endTimeCheckBox.setCheckState(Qt.Unchecked)
		else:
			self.endTimeCheckBox.setCheckState(Qt.Checked)
			self.endTime.setTime(QTime(h, m, s))
		self.dowEnable.setStates(bitMaskToBoolList(msg.dow_on_mask))
		self.ignoreChanges -= 1

	def handlePotStateMessage(self, msg):
		if not self.isEnabled():
			return
		self.ignoreChanges += 1
		self.statWidget.handlePotStateMessage(msg)
		self.wateringIndi.setState(msg.is_watering)
		self.forceStopWateringButton.setEnabled(msg.is_watering)
		self.rawAdc.setText("%d" % msg.last_measured_raw_value)
		self.stateMachineText.setText(controllerStateName(msg.state_id))
		self.ignoreChanges -= 1

	def handlePotRemStateMessage(self, msg):
		if not self.isEnabled():
			return
		self.ignoreChanges += 1
		if msg.flags & msg.POT_REMFLG_WDTRIGGER:
			self.watchdogGroup.show()
		else:
			self.watchdogGroup.hide()
		self.ignoreChanges -= 1

	def resetState(self):
		self.ignoreChanges += 1
		self.statWidget.enableMessageHandling(self.isEnabled())
		self.wateringIndi.setState(False)
		self.rawAdc.setText("None")
		self.stateMachineText.setText("Disabled")
		self.watchdogGroup.hide()
		self.ignoreChanges -= 1
