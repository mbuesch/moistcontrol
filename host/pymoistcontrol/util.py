#
#  Moistcontrol - utility functions
#
#  Copyright (c) 2013 Michael Buesch <m@bues.ch>
#  Licensed under the terms of the GNU General Public License version 2.
#

import sys

# Import the QT toolkit library (PySide).
try:
	from PySide.QtCore import *
	from PySide.QtGui import *
except ImportError as e:
	print("PLEASE INSTALL PySide (http://www.pyside.org/)")
	input("Press enter to exit.")
	sys.exit(1)


# Program version number
VERSION			= "1.0.1"
# Maximum number of flowerpots available.
MAX_NR_FLOWERPOTS	= 6


def clamp(value, minValue, maxValue):
	"""Limit 'value' to the range 'minValue':'maxValue'"""
	return max(min(value, maxValue), minValue)

def boolListToBitMask(boolList):
	"""Convert an iterable of Bools to an integer bit-mask."""
	mask = 0
	for i, b in enumerate(boolList):
		mask |= (1 << i) if b else 0
	return mask

def bitMaskToBoolList(bitMask):
	"""Convert an integer bit-mask to a list of Bools."""
	return list(bool(bitMask & (1 << i))
		    for i in range(bitMask.bit_length()))

class Error(Exception):
	"""Generic exception."""
	pass
