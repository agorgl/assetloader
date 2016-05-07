PRJTYPE = StaticLib
SRC = \
	src/common/atomic.c \
	src/common/rwlock.c \
	src/common/threads.c \
	src/common/uintmap.c \
	src/OpenAL32/alAuxEffectSlot.c \
	src/OpenAL32/alBuffer.c \
	src/OpenAL32/alEffect.c \
	src/OpenAL32/alError.c \
	src/OpenAL32/alExtension.c \
	src/OpenAL32/alFilter.c \
	src/OpenAL32/alFontsound.c \
	src/OpenAL32/alListener.c \
	src/OpenAL32/alMidi.c \
	src/OpenAL32/alPreset.c \
	src/OpenAL32/alSoundfont.c \
	src/OpenAL32/alSource.c \
	src/OpenAL32/alState.c \
	src/OpenAL32/alThunk.c \
	src/OpenAL32/sample_cvt.c \
	src/Alc/ALc.c \
	src/Alc/ALu.c \
	src/Alc/alcConfig.c \
	src/Alc/alcRing.c \
	src/Alc/bs2b.c \
	src/Alc/effects/autowah.c \
	src/Alc/effects/chorus.c \
	src/Alc/effects/compressor.c \
	src/Alc/effects/dedicated.c \
	src/Alc/effects/distortion.c \
	src/Alc/effects/echo.c \
	src/Alc/effects/equalizer.c \
	src/Alc/effects/flanger.c \
	src/Alc/effects/modulator.c \
	src/Alc/effects/null.c \
	src/Alc/effects/reverb.c \
	src/Alc/helpers.c \
	src/Alc/hrtf.c \
	src/Alc/panning.c \
	src/Alc/mixer.c \
	src/Alc/mixer_c.c \
	src/Alc/midi/base.c \
	src/Alc/midi/sf2load.c \
	src/Alc/midi/dummy.c \
	src/Alc/midi/fluidsynth.c \
	src/Alc/midi/soft.c \
	src/Alc/backends/base.c \
	src/Alc/backends/loopback.c \
	src/Alc/backends/null.c \
	src/Alc/backends/wave.c
ifeq ($(OS), Windows_NT)
	SRC += \
		src/Alc/backends/winmm.c \
		src/Alc/backends/mmdevapi.c
else
	SRC += \
		src/Alc/backends/alsa.c
endif

DEFINES = AL_ALEXT_PROTOTYPES AL_BUILD_LIBRARY AL_LIBTYPE_STATIC
ADDINCS = src/OpenAL32/Include src/Alc