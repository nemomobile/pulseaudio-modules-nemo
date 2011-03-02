AM_CFLAGS = $(PULSEAUDIO_CFLAGS) -I$(top_srcdir)/inc

AM_LIBADD = $(PULSEAUDIO_LIBS) -lm /usr/lib/pulse-*/modules/libmeego-common.la

modlibexec_LTLIBRARIES = module-meego-test-parameters.la

noinst_HEADERS = module-meego-test-parameters-symdef.h

module_meego_test_parameters_la_SOURCES = module-meego-test-parameters.c

module_meego_test_parameters_la_LDFLAGS = -module -avoid-version -Wl,-no-undefined
module_meego_test_parameters_la_LIBADD = $(AM_LIBADD)
module_meego_test_parameters_la_CFLAGS = $(AM_CFLAGS)
