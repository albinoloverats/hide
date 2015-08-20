.PHONY: hide clean distclean

SOURCE   = src/hide.c src/common/error.c src/common/cli.c

CFLAGS  += -Wall -Wextra -Werror -std=gnu99 -pipe -O2
CPPFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

LIBS     = -ldl -lpthread
SHARED   = -fPIC -shared -Wl,-soname,

DEBUG    = -D__DEBUG__ -O0 -g3 -ggdb

all: hide jpeg png tiff webp

hide:
	 @$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBS) $(SOURCE) -o hide
	-@echo "built ‘$(SOURCE)’ → ‘hide’"

#hide-gui:
#	 @$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBS) $(SOURCE) src/gui-gtk.c -o hide
#	-@echo "built ‘$(SOURCE) src/gui-gtk.c’ → ‘hide’"

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

debug: debug-hide debug-jpeg debug-png debug-tiff debug-webp

debug-hide:
	 @$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBS) $(SOURCE) $(DEBUG) -o hide
	-@echo "built ‘$(SOURCE)’ → ‘hide’"

debug-profile-jpeg:
	 @$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBS) $(SOURCE) -lm src/jpeg.c src/jpeg-load.c src/jpeg-save.c $(DEBUG) -pg -lc -o hide
	-@echo "built ‘$(SOURCE)’ → ‘hide’"

#debug-hide-gui:
#	 @$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBS) $(SOURCE) src/gui-gtk.c $(DEBUG) -o hide
#	-@echo "built ‘$(SOURCE) src/gui-gtk.c’ → ‘hide’"

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
	 @rm -fv hide-jpeg.so hide-png.so hide-tiff.so hide-webp.so
