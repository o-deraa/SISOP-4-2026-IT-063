CFLAGS = -D_FILE_OFFSET_BITS=64 $(shell pkg-config --cflags fuse)
LDFLAGS = $(shell pkg-config --libs fuse)

all: fuse

fuse: fuse.c
	gcc $(CFLAGS) -o fuse fuse.c $(LDFLAGS)

clean:
	rm -f fuse

mount: fuse
	./fuse ./encrypted_storage ./fuse_mount -f

.PHONY: all clean mount