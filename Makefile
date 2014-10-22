.PHONY: clean distclean

CFLAGS   = -Wall -Wextra -Werror -std=gnu99 -pipe
CPPFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

SHARED	 = -fPIC -shared -Wl,-soname,

DEBUG    = -D__DEBUGG__ -O0 -g3 -ggdb

all: imagine png tiff webp

imagine:
	@$(CC) $(CFLAGS) $(CPPFLAGS) -ldl src/imagine.c -o imagine
	-@echo "built ‘imagine.c’ → ‘imagine’"

png:
	@$(CC) -o imagine-png.so $(CFLAGS) $(CPPFLAGS) $(SHARED)imagine-png.so `pkg-config --cflags --libs libpng` src/png.c
	-@echo "built ‘png.c’ → ‘imagine-png.so’"

tiff:
	@$(CC) -o imagine-tiff.so $(CFLAGS) $(CPPFLAGS) $(SHARED)imagine-tiff.so -ltiff src/tiff.c
	-@echo "built ‘tiff.c’ → ‘imagine-tiff.so’"

webp:
	@$(CC) -o imagine-webp.so $(CFLAGS) $(CPPFLAGS) $(SHARED)imagine-webp.so -lwebp src/webp.c
	-@echo "built ‘webp.c’ → ‘imagine-webp.so’"

debug: debug-imagine debug-png debug-tiff debug-webp

debug-imagine:
	@$(CC) $(CFLAGS) $(CPPFLAGS) -ldl src/imagine.c $(DEBUG) -o imagine
	-@echo "built ‘imagine.c’ → ‘imagine’"

debug-png:
	@$(CC) -o imagine-png.so $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(SHARED)imagine-png.so `pkg-config --cflags --libs libpng` src/png.c
	-@echo "built ‘png.c’ → ‘imagine-png.so’"

debug-tiff:
	@$(CC) -o imagine-tiff.so $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(SHARED)imagine-tiff.so -ltiff src/tiff.c
	-@echo "built ‘tiff.c’ → ‘imagine-tiff.so’"

debug-webp:
	@$(CC) -o imagine-webp.so $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(SHARED)imagine-webp.so -lwebp src/webp.c
	-@echo "built ‘webp.c’ → ‘imagine-webp.so’"

clean:
	@rm -fv imagine

distclean: clean
	@rm -fv imagine-png.so imagine-tiff.so
