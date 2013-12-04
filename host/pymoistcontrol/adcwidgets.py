#
# Moisture control - ADC related widgets
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


class ADCSpinBox(QSpinBox):
	"""Spin box widget for ADC values."""

	def __init__(self, parent):
		"""Class constructor."""
		QSpinBox.__init__(self, parent)

		self.setSuffix(" LSB")
		self.setRange(0, 0x3FF)
