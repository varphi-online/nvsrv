CC=gcc
CFLAGS=-fsanitize=address -D _LINUX_
DEPS=cxl.h

TARGET_EXEC=nvrchserver

OBJS = main.o cxl.o http.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

asan: $(OBJS)
	$(CC) -o $(TARGET_EXEC) $(OBJS) libsqlite3ASAN.a $(CFLAGS)

.PHONY: all clean

all: asan

clean:
	rm -f $(OBJS) $(TARGET_EXEC)
