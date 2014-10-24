.PHONY: clean distclean

CFLAGS   = -Wall -Wextra -Werror -std=gnu99 -pipe
CPPFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

SHARED	 = -fPIC -shared -Wl,-soname,

DEBUG    = -D__DEBUGG__ -O0 -g3 -ggdb

all: hide png tiff webp

hide:
	@$(CC) $(CFLAGS) $(CPPFLAGS) -ldl src/hide.c -o hide
	-@echo "built ‘hide.c’ → ‘hide’"

png:
	@$(CC) -o hide-png.so $(CFLAGS) $(CPPFLAGS) $(SHARED)hide-png.so `pkg-config --cflags --libs libpng` src/png.c
	-@echo "built ‘png.c’ → ‘hide-png.so’"

tiff:
	@$(CC) -o hide-tiff.so $(CFLAGS) $(CPPFLAGS) $(SHARED)hide-tiff.so -ltiff src/tiff.c
	-@echo "built ‘tiff.c’ → ‘hide-tiff.so’"

webp:
	@$(CC) -o hide-webp.so $(CFLAGS) $(CPPFLAGS) $(SHARED)hide-webp.so -lwebp src/webp.c
	-@echo "built ‘webp.c’ → ‘hide-webp.so’"

debug: debug-hide debug-png debug-tiff debug-webp

debug-hide:
	@$(CC) $(CFLAGS) $(CPPFLAGS) -ldl src/hide.c $(DEBUG) -o hide
	-@echo "built ‘hide.c’ → ‘hide’"

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
	@rm -fv hide-png.so hide-tiff.so hide-webp.so
