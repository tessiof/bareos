# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2019-2020 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
# Author: Tobias Plum
#
from bareosdir import *


def load_bareos_plugin(plugindef):
    events = []
    events.append(bDirEventJobStart)
    events.append(bDirEventJobEnd)
    events.append(bDirEventJobInit)
    events.append(bDirEventJobRun)
    RegisterEvents(events)
    return bRC_OK


def parse_plugin_definition(plugindef):
    plugin_options = plugindef.split(":")
    for current_option in plugin_options:
        key, sep, val = current_option.partition("=")
        if val == "":
            continue
        elif key == "output":
            global outputfile
            outputfile = val
            continue
        elif key == "instance":
            continue
        elif key == "module_path":
            continue
        elif key == "module_name":
            continue
        else:
            return bRCs["bRC_Error"]
        toFile(outputfile)

    return bRCs["bRC_OK"]


def handle_plugin_event(event):
    if event == bDirEventJobStart:
        toFile("bDirEventJobStart\n")

    elif event == bDirEventJobEnd:
        toFile("bDirEventJobEnd\n")

    elif event == bDirEventJobInit:
        toFile("bDirEventJobInit\n")

    elif event == bDirEventJobRun:
        toFile("bDirEventJobRun\n")

    return bRCs["bRC_OK"]


def toFile(text):
    doc = open(outputfile, "a")
    doc.write(text)
    doc.close()
