import dbus
import os
from dbus.types import *
import unittest
import traceback

DEFAULT_ADDRESS = "unix:path=/var/run/pulse/dbus-socket"
CORE_PATH = "/org/pulseaudio/core1"
CORE_IFACE = "org.PulseAudio.Core1"
DEVICE_IFACE = "org.PulseAudio.Core1.Device"
MAINVOLUME_NAME = "module-nokia-mainvolume"
MAINVOLUME_PATH = "/com/nokia/mainvolume1"
MAINVOLUME_IFACE = "com.Nokia.MainVolume1"
MIN_VOL = 0
MAX_VOL = 65535

class MainVolumeConnection:
    def __init__(self):
        self.connect()

    def connect(self):
        self.address = os.environ.get('PULSE_DBUS_SERVER', DEFAULT_ADDRESS)
        self.connection = dbus.connection.Connection(self.address)
        proxy = self.connection.get_object(object_path=MAINVOLUME_PATH)
        self.iface = dbus.Interface(proxy, dbus_interface=MAINVOLUME_IFACE)
        self.prop = dbus.Interface(proxy, dbus_interface=dbus.PROPERTIES_IFACE)
        self.introspect = dbus.Interface(proxy, dbus_interface="org.freedesktop.DBus.Introspectable")

    def interface(self):
        return self.iface

    def properties(self):
        return self.prop

    def introspect(self):
        return self.introspect

    def close(self):
        self.connection.close()
        self.connection = None

class MainVolumeTestSet(unittest.TestCase):
    def setUp(self):
        self.connection = MainVolumeConnection()

    def tearDown(self):
        self.connection.close()

class TestMainVolume(MainVolumeTestSet):
    def testProtocolRevision(self):
        p = self.connection.properties()
        p.Get(MAINVOLUME_IFACE, "InterfaceRevision")

    def testGetStepCount(self):
        p = self.connection.properties()
        p.Get(MAINVOLUME_IFACE, "StepCount")

# currently crashes pulseaudio
#    def testSetStepCount(self):
#        p = self.connection.properties()
#        p.Set(MAINVOLUME_IFACE, "StepCount", UInt32(5))

    def testGetCurrentStep(self):
        p = self.connection.properties()
        p.Get(MAINVOLUME_IFACE, "CurrentStep")

    def testSetCurrentStep(self):
        p = self.connection.properties()
        p.Set(MAINVOLUME_IFACE, "CurrentStep", UInt32(1))

    def testGetAndSetStep(self):
        p = self.connection.properties()
        step_count = p.Get(MAINVOLUME_IFACE, "StepCount")
        current_step = p.Get(MAINVOLUME_IFACE, "CurrentStep")
        new_step = UInt32(step_count-1)
        if new_step == current_step:
            new_step = UInt32(new_step - 1)
        p.Set(MAINVOLUME_IFACE, "CurrentStep", new_step)
        check_step = p.Get(MAINVOLUME_IFACE, "CurrentStep")
        self.assertTrue(check_step == new_step)

    def testGetAllProperties(self):
        p = self.connection.properties()
        props = p.GetAll(MAINVOLUME_IFACE)


if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestMainVolume)
    unittest.TextTestRunner(verbosity=2).run(suite)

