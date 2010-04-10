AM_CFLAGS = $(PULSEAUDIO_CFLAGS) $(DBUS_CFLAGS) $(CHECK_CFLAGS) -I$(top_builddir)/src/mainvolume

AM_LIBADD = $(PULSEAUDIO_LIBS) $(DBUS_LIBS) $(CHECK_LIBS)

TESTS = check_mainvolume
check_PROGRAMS = check_mainvolume

check_mainvolume_SOURCES = $(top_builddir)/src/mainvolume/mainvolume.c $(top_builddir)/src/mainvolume/mainvolume.h check_mainvolume.c

check_mainvolume_LDFLAGS = -avoid-version -Wl,-no-undefined
check_mainvolume_LDADD = $(AM_LIBADD)
check_mainvolume_CFLAGS = $(AM_CFLAGS)

