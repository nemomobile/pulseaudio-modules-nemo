AM_CFLAGS = $(PULSEAUDIO_CFLAGS) $(DBUS_CFLAGS)

AM_LIBADD = $(PULSEAUDIO_LIBS) $(DBUS_LIBS)

modlibexec_LTLIBRARIES = module-meego-mainvolume.la

noinst_HEADERS = module-meego-mainvolume-symdef.h

module_meego_mainvolume_la_SOURCES = mainvolume.c mainvolume.h module-meego-mainvolume.c

module_meego_mainvolume_la_LDFLAGS = -module -avoid-version -Wl,-no-undefined
module_meego_mainvolume_la_LIBADD = $(AM_LIBADD) $(top_builddir)/src/common/libmeego-common.la
module_meego_mainvolume_la_CFLAGS = $(AM_CFLAGS)

SUBDIRS = tests

