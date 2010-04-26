AM_CFLAGS = $(PULSEAUDIO_CFLAGS)

AM_LIBADD = $(PULSEAUDIO_LIBS)

modlibexec_LTLIBRARIES = module-meego-sidetone.la

noinst_HEADERS = module-meego-sidetone-symdef.h

module_meego_sidetone_la_SOURCES = module-meego-sidetone.c

module_meego_sidetone_la_LDFLAGS = -module -avoid-version -Wl,-no-undefined
module_meego_sidetone_la_LIBADD = $(AM_LIBADD)
module_meego_sidetone_la_CFLAGS = $(AM_CFLAGS)
