LIBS = glib-2.0 json fuse

CC = gcc

CFLAGS = -std=gnu99 -pedantic -Wall -Wextra -O3 -Iinclude
LDFLAGS = -lcurl

TARGET  = vkaudiofs
SOURCES = $(shell echo src/*.c)
OBJECTS = $(SOURCES:.c=.o)

PREFIX = $(DESTDIR)/usr/local
BINDIR = $(PREFIX)/bin

CFLAGS += $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS))

all: $(TARGET)
 
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
    
install: all
	install $(TARGET) $(BINDIR)/$(TARGET)
    
uninstall:
	-rm $(BINDIR)/$(TARGET)

clean:
	-rm -f $(OBJECTS)

distclean: clean
	-rm -f $(TARGET)