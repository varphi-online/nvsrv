CC=gcc
CFLAGS=-I./lib -fsanitize=address
DEPS=./lib/cxl.h ./lib/http.h ./lib/json.h
VPATH=./lib

TARGET_EXEC=nvrchserver

OBJS = main.o ./lib/cxl.o ./lib/http.o ./lib/json.o

# Declare object files as intermediate targets
.INTERMEDIATE: $(OBJS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

asan: $(OBJS)
	$(CC) -o $(TARGET_EXEC) $(OBJS) ./lib/libsqlite3ASAN.a $(CFLAGS)

.PHONY: all clean

all: asan

clean:
	rm -f $(OBJS) $(TARGET_EXEC)
