#!/usr/bin/env python

import dbus
from dbus.types import *
import os
import sys

DEFAULT_ADDRESS = "unix:path=/var/run/pulse/dbus-socket"
CORE_PATH = "/org/pulseaudio/core1"
CORE_IFACE = "org.PulseAudio.Core1"
DEVICE_IFACE = "org.PulseAudio.Core1.Device"
MAINVOLUME_NAME = "module-nokia-mainvolume"
MAINVOLUME_PATH = "/com/nokia/mainvolume1"
MAINVOLUME_IFACE = "com.Nokia.MainVolume1"
MIN_VOL = 0
MAX_VOL = 65535

def print_help():
    print "Usage %s set/get <volume step>" % sys.argv[0]

def main():
    if len(sys.argv) < 2:
        print_help()
        return

    address = os.environ.get('PULSE_DBUS_SERVER', DEFAULT_ADDRESS)
    connection = dbus.connection.Connection(address)
    proxy = connection.get_object(object_path=MAINVOLUME_PATH)
    prop = dbus.Interface(proxy, dbus_interface=dbus.PROPERTIES_IFACE)

    if sys.argv[1] == "get":
        print "StepCount", prop.Get(MAINVOLUME_IFACE, "StepCount")
        print "CurrentStep", prop.Get(MAINVOLUME_IFACE, "CurrentStep")
    elif sys.argv[1] == "set":
        if len(sys.argv) < 3:
            print_help()
            return
        prop.Set(MAINVOLUME_IFACE, "CurrentStep", UInt32(sys.argv[2]))

if __name__ == "__main__":
    main()
