# Add additional .c files here if you added any yourself.
ADDITIONAL_SOURCES =

# Add additional .h files here if you added any yourself.
ADDITIONAL_HEADERS =

# -- Do not modify below this point - will get replaced during testing --
CURDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

LIB = libmyalloc.so
SOURCES_LIB = alloc.c $(ADDITIONAL_SOURCES)
HEADERS_LIB = alloc.h $(ADDITIONAL_HEADERS)
TEST = test
SOURCES_TEST = test_framework/main.c test_framework/tests.c \
			   test_framework/memlist.c test_framework/checked_alloc.c \
			   test_framework/intercept.c
HEADERS_TEST = test_framework/common.h test_framework/tests.h \
			   test_framework/memlist.h test_framework/checked_alloc.h
META = Makefile README.rst check.py

DOCKERIMG = vusec/vu-os-alloc-check:latest

CFLAGS = -Wall -Wextra -std=gnu99 -MD -g3 -O1
CFLAGS_LIB = -fPIC
CFLAGS_TEST = -DALLOC_TEST_FRAMEWORK -fvisibility=hidden
LDFLAGS =
LDFLAGS_LIB = -fPIC -shared
LDFLAGS_TEST = -fvisibility=hidden -ldl -L. -Wl,-rpath="$(CURDIR)" -lmyalloc

CC = gcc

.SUFFIXES: # Disable built-in rules
.PHONY: all tarball clean check

all: $(LIB) $(TEST)

tarball: alloc.tar.gz

alloc.tar.gz: $(SOURCES_LIB) $(HEADERS_LIB) $(SOURCES_TEST) $(HEADERS_TEST) $(META)
	tar -czf $@ $^

check: all
	@./check.py

docker-update:
	docker pull --platform=linux/amd64 $(DOCKERIMG)

docker-check: alloc.tar.gz
	mkdir -p docker_mnt
	cp $^ docker_mnt/
	docker run --platform=linux/amd64 -i -t --rm -v `pwd`/docker_mnt:/submission $(DOCKERIMG) /test

docker-run:
	docker run --platform=linux/amd64 -it --rm --read-only -v "`pwd`:/code" -w /code \
	--name os_$(TARGET) $(DOCKERIMG) /bin/bash

docker-connect:
# 	If the docker is not running, throw an error
	@if ! docker container inspect os_$(TARGET) > /dev/null 2>&1; then \
		echo "\033[0;31mDocker container os_$(TARGET) is not running! Run \"make docker-run\" first in another terminal.\033[0m" && false; \
	fi

# 	Connect to the docker container
	docker container exec -it os_$(TARGET) /bin/bash

clean:
	rm -f $(LIB)
	rm -f $(TEST)
	rm -f *.o test_framework/*.o
	rm -f *.d test_framework/*.d
	rm -rf docker_mnt
	rm -f alloc.tar.gz

$(LIB): $(SOURCES_LIB:.c=.o)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDFLAGS_LIB)

$(TEST): $(SOURCES_TEST:.c=.o) | $(LIB)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDFLAGS_TEST)

test_framework/%.o: test_framework/%.c
	$(CC) $(CFLAGS) $(CFLAGS_TEST) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LIB) -c -o $@ $<


-include *.d
