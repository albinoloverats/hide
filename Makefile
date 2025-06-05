.PHONY: hide clean distclean

SOURCE   = src/hide.c
COMMON   = common/src/error.c common/src/cli.c common/src/mem.c

CFLAGS  += -Wall -Wextra -Werror -std=gnu99 -pipe -O2
CPPFLAGS = -Icommon/src -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

LIBS     = -ldl -lpthread
SHARED   = -fPIC -shared -Wl,-soname,

DEBUG    = -D__DEBUG__ -O0 -g3 -ggdb

all: hide bmp jpeg png tiff webp

hide:
	 @$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBS) $(SOURCE) $(COMMON) -o hide
	-@echo "built ‘$(SOURCE) $(COMMON)’ → ‘hide’"

#hide-gui:
#	 @$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBS) $(SOURCE) $(COMMON) src/gui-gtk.c -o hide
#	-@echo "built ‘$(SOURCE) $(COMMON) src/gui-gtk.c’ → ‘hide’"

bmp:
	 @$(CC) -o hide-bmp.so $(CFLAGS) $(CPPFLAGS) $(SHARED)hide-bmp.so src/bmp.c
	-@echo "built ‘bmp.c’ → ‘hide-bmp.so’"

jpeg:
	 @$(CC) -o hide-jpeg.so $(CFLAGS) $(CPPFLAGS) -lm $(SHARED)hide-jpeg.so src/jpeg.c src/jpeg-load.c src/jpeg-save.c
	-@echo "built ‘jpeg.c jpeg-load.c jpeg-save.c’ → ‘hide-jpeg.so’"

png:
	 @$(CC) -o hide-png.so $(CFLAGS) $(CPPFLAGS) $(SHARED)hide-png.so `pkg-config --cflags --libs libpng` src/png.c
	-@echo "built ‘png.c’ → ‘hide-png.so’"

tiff:
	 @$(CC) -o hide-tiff.so $(CFLAGS) $(CPPFLAGS) $(SHARED)hide-tiff.so -ltiff src/tiff.c
	-@echo "built ‘tiff.c’ → ‘hide-tiff.so’"

webp:
	 @$(CC) -o hide-webp.so $(CFLAGS) $(CPPFLAGS) $(SHARED)hide-webp.so -lwebp src/webp.c
	-@echo "built ‘webp.c’ → ‘hide-webp.so’"

debug: debug-hide debug-bmp debug-jpeg debug-png debug-tiff debug-webp

debug-hide:
	 @$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBS) $(SOURCE) $(DEBUG) -o hide
	-@echo "built ‘$(SOURCE)’ → ‘hide’"

debug-profile-jpeg:
	 @$(CC) $(CFLAGS) $(CPPFLAGS) -D__DEBUG_JPEG__ $(LIBS) $(SOURCE) -lm src/jpeg.c src/jpeg-load.c src/jpeg-save.c $(DEBUG) -pg -lc -o hide
	-@echo "built ‘$(SOURCE)’ → ‘hide’"

#debug-hide-gui:
#	 @$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBS) $(SOURCE) src/gui-gtk.c $(DEBUG) -o hide
#	-@echo "built ‘$(SOURCE) src/gui-gtk.c’ → ‘hide’"

debug-bmp:
	  @$(CC) -o hide-bmp.so $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(SHARED)hide-bmp.so src/bmp.c
	-@echo "built ‘bmp.c’ → ‘hide-bmp.so’"

debug-jpeg:
	 @$(CC) -o hide-jpeg.so $(CFLAGS) $(CPPFLAGS) -lm $(DEBUG) $(SHARED)hide-jpeg.so src/jpeg.c src/jpeg-load.c src/jpeg-save.c
	-@echo "built ‘jpeg.c jpeg-load.c jpeg-save.c’ → ‘hide-jpeg.so’"

debug-png:
	 @$(CC) -o hide-png.so $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(SHARED)hide-png.so `pkg-config --cflags --libs libpng` src/png.c
	-@echo "built ‘png.c’ → ‘hide-png.so’"

debug-tiff:
	  @$(CC) -o hide-tiff.so $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(SHARED)hide-tiff.so -ltiff src/tiff.c
	-@echo "built ‘tiff.c’ → ‘hide-tiff.so’"

debug-webp:
	 @$(CC) -o hide-webp.so $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(SHARED)hide-webp.so -lwebp src/webp.c
	-@echo "built ‘webp.c’ → ‘hide-webp.so’"

clean:
	 @rm -fv hide

distclean: clean
	 @rm -fv hide-bmp.so hide-jpeg.so hide-png.so hide-tiff.so hide-webp.so
