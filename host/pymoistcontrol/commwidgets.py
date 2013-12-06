#
# Moisture control - Serial communication widgets
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

import os


class SerialOpenDialog(QDialog):
	"""Serial port connection dialog."""

	def __init__(self, parent):
		"""Class constructor."""

		QDialog.__init__(self, parent)
		self.setLayout(QGridLayout(self))

		self.setWindowTitle("Select serial port")

		self.portCombo = QComboBox(self)
		if os.name.lower() == "posix":
			# Unix operating system
			# Add all serial devices from /dev to
			# the combo box.
			devNodes = QDir("/dev").entryInfoList(QDir.System,
							      QDir.Name)
			select = None
			for node in devNodes:
				name = node.fileName()
				if not name.startswith("ttyS") and\
				   not name.startswith("ttyUSB"):
					continue
				path = node.filePath()
				self.portCombo.addItem(path, path)
				if select is None and\
				   name.startswith("ttyUSB"):
					# Select the first ttyUSB by default.
					select = self.portCombo.count() - 1
			if select is not None:
				self.portCombo.setCurrentIndex(select)
		elif os.name.lower() in ("nt", "ce"):
			# Windows operating system
			# Add 8 COM ports to the combo box.
			for i in range(8):
				port = "COM%d" % (i + 1)
				self.portCombo.addItem(port, port)
		else:
			raise Error("Operating system not supported")
		self.layout().addWidget(self.portCombo, 0, 0, 1, 2)

		self.okButton = QPushButton("&Ok", self)
		self.layout().addWidget(self.okButton, 1, 0)

		self.cancelButton = QPushButton("&Cancel", self)
		self.layout().addWidget(self.cancelButton, 1, 1)

		self.okButton.released.connect(self.accept)
		self.cancelButton.released.connect(self.reject)

	def getSelectedPort(self):
		"""Get the selected port name."""

		index = self.portCombo.currentIndex()
		if index < 0:
			return None
		return self.portCombo.itemData(index)
