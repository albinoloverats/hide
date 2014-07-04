.PHONY: clean

APP      = imagine

SOURCE   = main.c png.c tiff.c

CFLAGS   = -Wall -Wextra -Werror -std=gnu99 `pkg-config --cflags libpng` -pipe -O2
CPPFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

LIBS     = `pkg-config --libs libpng` -ltiff

DEBUG    = -O0 -ggdb

#all:
#	@$(CC) $(CFLAGS) $(CPPFLAGS) $(SOURCE) $(LIBS) -o $(APP)
#	-@echo "built ‘`echo $(SOURCE) $(COMMON) | sed 's/ /’\n      ‘/g'`’ → ‘$(APP)’"

debug:
	@$(CC) $(CFLAGS) $(CPPFLAGS) $(SOURCE) $(LIBS) $(DEBUG) -o $(APP)
	-@echo "built ‘`echo $(SOURCE) | sed 's/ /’\n      ‘/g'`’ → ‘$(APP)’"

clean:
	@rm -fv $(APP)
