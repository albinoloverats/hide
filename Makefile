.PHONY: clean distclean

CFLAGS   = -Wall -Wextra -Werror -std=gnu99  -pipe
CPPFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

SHARED	 = -fPIC -shared -Wl,-soname,

DEBUG    = -O0 -g3 -ggdb

#all:
#	@$(CC) $(CFLAGS) $(CPPFLAGS) $(SOURCE) $(LIBS) -O2 -o $(APP)
#	-@echo "built ‘`echo $(SOURCE) $(COMMON) | sed 's/ /’\n      ‘/g'`’ → ‘$(APP)’"

debug:
	@$(CC) $(CFLAGS) $(CPPFLAGS) -ldl src/main.c $(DEBUG) -o imagine
	-@echo "built ‘main.c’ → ‘imagine’"

png:
	@$(CC) -o imagine-png.so $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(SHARED)imagine-png.so `pkg-config --cflags --libs libpng` src/png.c
	-@echo "built ‘png.c’ → ‘imagine-png.so’"

tiff:
	@$(CC) -o imagine-tiff.so $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(SHARED)imagine-tiff.so -ltiff src/tiff.c
	-@echo "built ‘tiff.c’ → ‘imagine-tiff.so’"

clean:
	@rm -fv imagine

distclean: clean
	@rm -fv imagine-png.so imagine-tiff.so
