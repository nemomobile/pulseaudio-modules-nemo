AM_CFLAGS = $(PULSEAUDIO_CFLAGS) $(ASOUNDLIB_CFLAGS) $(CHECK_CFLAGS) -I$(top_builddir)/src/sidetone

AM_LIBADD = $(PULSEAUDIO_LIBS) $(ASOUNDLIB_LIBS) $(CHECK_LIBS)

TESTS = check_sidetone
check_PROGRAMS = check_sidetone

check_sidetone_SOURCES = \
	$(top_srcdir)/src/sidetone/sidetone-args.c \
	$(top_srcdir)/src/sidetone/sidetone-args.h \
	check_sidetone.c

check_sidetone_LDFLAGS = -avoid-version -Wl,-no-undefined
check_sidetone_LDADD = $(AM_LIBADD)
check_sidetone_CFLAGS = $(AM_CFLAGS)

