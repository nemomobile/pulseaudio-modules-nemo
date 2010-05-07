#!/usr/bin/env python

# Usage: python mainvolume-monitor.py
#
# Copied and adapted from role-volume-monitor.py!

import dbus
from dbus.mainloop.glib import DBusGMainLoop
import gobject

SERVER_ADDRESS = "unix:path=/var/run/pulse/dbus-socket"
CORE_PATH = "/org/pulseaudio/core1"
CORE_IFACE = "org.PulseAudio.Core1"
MEMBER = "StepsUpdated"
MAINVOLUME_PATH = "/com/meego/mainvolume1"
MAINVOLUME_IFACE = "com.Nokia.MainVolume1"
MAINVOLUME_SIGNAL = MAINVOLUME_IFACE + "." + MEMBER

# Since the server address is hardcoded, connecting is very simple, but when
# the run-time address discovery logic gets fixed, this should be changed too.
def connect():
    return dbus.connection.Connection(SERVER_ADDRESS)

# All D-Bus signals are handled here. The SignalMessage object is passed in
# the keyword arguments with key "msg". We expect only VolumeUpdated signals.
def signal_cb(*args, **keywords):

    s = keywords["msg"]

    if s.get_path() == MAINVOLUME_PATH \
            and s.get_interface() == MAINVOLUME_IFACE \
            and s.get_member() == MEMBER:

        # args[0] is current step count as dbus.UInt32
        # args[1] is current active step as dbus.UInt32
        step_count = args[0]
        current_step = args[1]

        # Print the new steps with fancy formatting.
        print "StepsUpdated: Step count %d current step %d" % (step_count, current_step)

    else:
        # This code should not get executed, except when the connection dies
        # (pulseaudio exits or something), in which case we get
        # a org.freedesktop.DBus.Local.Disconnected signal.
        print "Unexpected signal:", s.get_path(), s.get_interface(), s.get_member()



# We integrate with the GLib main loop implementation. That's the easiest way
# to receive D-Bus signals asynchronously in Python.
DBusGMainLoop(set_as_default=True)

# Get the dbus.connection.Connection object.
conn = connect()

# Register the signal callback. By default only the signal arguments are passed
# to the callback, but by setting the message_keyword here, we also get the
# SignalMessage object which is useful for separating different signals from
# each other.
conn.add_signal_receiver(signal_cb, message_keyword="msg")

# Use the Python D-Bus bindings magic to create a proxy object for the central
# core object of Pulseaudio.
core = conn.get_object(object_path=CORE_PATH)

# The server won't send us any signals unless we explicitly tell it to send
# them. Here we tell the server that we'd like to receive the StepsUpdated
# signals.
core.ListenForSignal(MAINVOLUME_SIGNAL, [MAINVOLUME_PATH], dbus_interface=CORE_IFACE)

# Run forever, waiting for the signals to come.
loop = gobject.MainLoop()
loop.run()
