#
# Moisture control - Bit indicator widget
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


class BitIndicator(QWidget):
	"""Graphical bit value indicator"""

	def __init__(self, offText, onText, parent):
		"""Class constructor."""
		QWidget.__init__(self, parent)
		self.setLayout(QGridLayout(self))
		self.layout().setContentsMargins(QMargins())

		self.offText = offText
		self.onText = onText

		self.textLabel = QLabel("", self)
		self.layout().addWidget(self.textLabel, 0, 0)

		self.__state = None
		self.setState(False)

	def setState(self, newState):
		newState = bool(newState)
		if newState != self.__state:
			self.__state = newState
			self.__update()

	def __update(self):
		if self.__state:
			self.textLabel.setText(self.onText)
		else:
			self.textLabel.setText(self.offText)
