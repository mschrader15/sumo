#!/usr/bin/env python
# Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
# Copyright (C) 2009-2021 German Aerospace Center (DLR) and others.
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License 2.0 which is available at
# https://www.eclipse.org/legal/epl-2.0/
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the Eclipse
# Public License 2.0 are satisfied: GNU General Public License, version 2
# or later which is available at
# https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

# @file    test.py
# @author  Pablo Alvarez Lopez
# @date    2016-11-25

# import common functions for netedit tests
import os
import sys

testRoot = os.path.join(os.environ.get('SUMO_HOME', '.'), 'tests')
neteditTestRoot = os.path.join(
    os.environ.get('TEXTTEST_HOME', testRoot), 'netedit')
sys.path.append(neteditTestRoot)
import neteditTestFunctions as netedit  # noqa

# Open netedit
neteditProcess, referencePosition = netedit.setupAndStart(neteditTestRoot)

# go to shape mode
netedit.shapeMode()

# go to shape mode
netedit.changeElement("poiGeo")

# create poi
netedit.leftClick(referencePosition, 100, 100)

# change color to white (To see icon)
netedit.changeDefaultValue(4, "white")

# Change parameter width with a valid value (To see icon)
netedit.changeDefaultValue(8, "10")

# Change parameter height with a valid value (To see icon)
netedit.changeDefaultValue(9, "10")

# change imgfile (valid)
netedit.changeDefaultValue(10, "berlin_icon.ico")

# create poi
netedit.leftClick(referencePosition, 100, 350)

# go to inspect mode
netedit.inspectMode()

# inspect first POI
netedit.leftClick(referencePosition, 110, 95)

# block POI
netedit.modifyBoolAttribute(13, True)

# inspect second POI
netedit.leftClick(referencePosition, 110, 360)

# block POI
netedit.modifyBoolAttribute(13, True)

# go to move mode
netedit.moveMode()

# try to move first POI to left down
netedit.moveElement(referencePosition, -78, 50, 200, 60)

# try to move second POI to left up
netedit.moveElement(referencePosition, -78, 385, 200, 300)

# go to inspect mode again
netedit.inspectMode()

# inspect first POI
netedit.leftClick(referencePosition, 100, 100)

# unblock POI
netedit.modifyBoolAttribute(13, True)

# inspect first POI
netedit.leftClick(referencePosition, 100, 350)

# unblock POI
netedit.modifyBoolAttribute(13, True)

# go to move mode
netedit.moveMode()

# move first POI to left down
netedit.moveElement(referencePosition, -78, 50, 200, 60)

# move second POI to left up
netedit.moveElement(referencePosition, -78, 385, 200, 300)

# Check undo redo
netedit.undo(referencePosition, 4)
netedit.redo(referencePosition, 4)

# save shapes
netedit.saveAdditionals(referencePosition)

# save network
netedit.saveNetwork(referencePosition)

# quit netedit
netedit.quit(neteditProcess)
