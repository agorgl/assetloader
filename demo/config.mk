PRJTYPE = Executable
LIBS = assetloader macu physfs openal orb vorbis ogg freetype png jpeg tiff zlib gfxwnd glfw glad
ifeq ($(TARGET_OS), Windows)
	LIBS += glu32 opengl32 gdi32 winmm ole32 shell32 user32
else ifeq ($(TARGET_OS), Darwin)
	MLDFLAGS += -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
else
	LIBS += GLU GL X11 Xcursor Xinerama Xrandr Xxf86vm Xi pthread m dl
endif
ADDLIBDIR = ../lib \
			../deps/ZLib/lib \
			../deps/Png/lib \
			../deps/Jpeg/lib \
			../deps/Tiff/lib \
			../deps/Vorbis/lib \
			../deps/Ogg/lib \
			../deps/Freetype/lib \
			../deps/PhysFS/lib
MOREDEPS = ..
EXTDEPS = macu::0.0.2dev orb::dev gfxwnd::0.0.1dev
