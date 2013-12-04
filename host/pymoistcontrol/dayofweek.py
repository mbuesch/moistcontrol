#
# Moisture control - Day of week selection widget
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


class DayOfWeekSelectWidget(QFrame):
	"""Weekday selection widget"""

	changed = Signal()

	def __init__(self, parent):
		"""Class constructor."""
		QFrame.__init__(self, parent)
		self.setLayout(QGridLayout())
		self.layout().setContentsMargins(QMargins(5, 5, 3, 3))

		self.setFrameShape(QFrame.Box)
		self.setFrameShadow(QFrame.Sunken)

		self.checkBoxes = []
		for i, name in enumerate(("Mon", "Tue", "Wed",
					  "Thu", "Fri", "Sat", "Sun")):
			cb = QCheckBox(self)
			self.checkBoxes.append(cb)
			self.layout().addWidget(cb, 0, i)
			label = QLabel(name, self)
			self.layout().addWidget(label, 1, i)
			cb.stateChanged.connect(self.__cbStateChanged)

	def __cbStateChanged(self):
		self.changed.emit()

	def getStates(self):
		return ( cb.checkState() == Qt.Checked
			 for cb in self.checkBoxes )

	def setStates(self, newStates):
		newStates = tuple(newStates)
		for i, cb in enumerate(self.checkBoxes):
			try:
				s = Qt.Checked if newStates[i] else Qt.Unchecked
			except IndexError:
				s = Qt.Unchecked
			cb.setCheckState(s)
