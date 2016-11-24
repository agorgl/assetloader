PRJTYPE = Executable
ADDINCS = ../include ../deps/Macu/include
LIBS = assetloader macu openal vorbis ogg freetype png jpeg tiff zlib gfxwnd glfw glad
ifeq ($(TARGET_OS), Windows_NT)
	LIBS += glu32 opengl32 gdi32 winmm ole32 shell32 user32
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
			../deps/Macu/lib \
			../deps/Freetype/lib
MOREDEPS = ..
EXTDEPS = macu::0.0.2dev gfxwnd::0.0.0dev
