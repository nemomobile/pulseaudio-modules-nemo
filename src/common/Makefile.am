AM_CFLAGS = \
	$(PULSEAUDIO_CFLAGS)

modlibexec_LTLIBRARIES = libmeego-common.la

libmeego_common_la_SOURCES = \
	optimized.c optimized.h \
	pa-optimized.c pa-optimized.h \
	memory.c memory.h \
	parameter-hook.c parameter-hook.h \
	algorithm-hook.c algorithm-hook.h \
	proplist-meego.h

libmeego_common_la_LDFLAGS = -avoid-version
libmeego_common_la_LIBADD = $(PULSEAUDIO_LIBS)

if X86
libmeego_common_la_SOURCES += \
	src-8-to-48.c \
	src-48-to-8.c
else
libmeego_common_la_LIBADD += \
	src-8-to-48-neon.o \
	src-48-to-8-neon.o
%-neon.o: src_8_48_neon.a
	ar x src_8_48_neon.a
endif

TESTS = check_common
check_PROGRAMS = check_common
check_common_SOURCES = tests.c
check_common_LDADD = libmeego-common.la $(CHECK_LIBS)
check_common_CFLAGS = $(CHECK_CFLAGS)

