AM_LIBADD =                                                     \
	$(PULSEAUDIO_LIBS)					\
	$(CMTSPEECH_LIB)					\
	$(DBUS_LIBS)						\
	$(top_builddir)/src/common/libmeego-common.la

AM_CFLAGS =						\
	$(PULSEAUDIO_CFLAGS)				\
	$(DBUS_CFLAGS)                                  \
	-I$(top_srcdir)/src/cmtspeech			\
	-I$(top_srcdir)/src/voice                       \
	-I$(top_srcdir)/src/common

CMTSPEECH_LIB = -lcmtspeechdata

###################################
#            Cmtspeech            #
###################################
noinst_HEADERS = module-meego-cmtspeech-symdef.h

modlibexec_LTLIBRARIES = module-meego-cmtspeech.la

module_meego_cmtspeech_la_SOURCES = \
      cmtspeech-connection.c \
      cmtspeech-dbus.c \
      cmtspeech-mainloop-handler.c \
      cmtspeech-sink-input.c \
      cmtspeech-source-output.c \
      module-meego-cmtspeech.c

module_meego_cmtspeech_la_LDFLAGS = -module -avoid-version -Wl,-no-undefined -Wl,-z,noexecstack
module_meego_cmtspeech_la_LIBADD = $(AM_LIBADD)
module_meego_cmtspeech_la_CFLAGS = $(AM_CFLAGS)
