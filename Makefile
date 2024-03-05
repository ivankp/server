.PHONY: all clean

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean))) #############

CXX = g++
CC = gcc
AS = gcc

CFLAGS := -Wall -O3 -flto
# CFLAGS := -Wall -Og -g
CFLAGS += -fmax-errors=3 -Iinclude
# CFLAGS += -DNDEBUG
CPPSTD := c++20

# generate .d files during compilation
DEPFLAGS = -MT $@ -MMD -MP -MF .build/$*.d

FIND_MAIN := \
  xargs grep -l '^\s*int\s\+main\s*(' \
  | sed -n 's:^src/\(.*\)\.cc\?$$:bin/\1:p'

all:

.build/exe.mk: $(shell find src -type f)
	@mkdir -pv $(dir $@)
	@echo 'all:' $(shell echo $^ | $(FIND_MAIN)) > $@

-include .build/exe.mk

.build/addr_blacklist.o: CFLAGS += -DSERVER_VERBOSE_BLACKLIST

.build/examples/%_server.o \
.build/server.o .build/socket.o \
: CFLAGS += -pthread

bin/examples/%_server: LDFLAGS += -pthread

bin/examples/echo_server: $(patsubst %, .build/%.o, \
  socket error addr_ip4 server \
)

bin/examples/http_server: $(patsubst %, .build/%.o, \
  socket error addr_ip4 \
  server http file mime url request_log file_cache \
)

.PRECIOUS: .build/%.o

bin/%: .build/%.o
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(filter %.o,$^) -o $@ $(LDLIBS)

%.so: .build/%.o
	$(CXX) $(LDFLAGS) -shared $(filter %.o,$^) -o $@ $(LDLIBS)

.build/%.o: src/%.cc
	@mkdir -pv $(dir $@)
	$(CXX) -std=$(CPPSTD) $(CFLAGS) $(DEPFLAGS) -c $(filter %.cc,$^) -o $@

.build/%.o: src/%.c
	@mkdir -pv $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $(filter %.c,$^) -o $@

.build/%.o: src/%.S
	@mkdir -pv $(dir $@)
	$(AS) $(CFLAGS) -c $(filter %.S,$^) -o $@

-include $(shell [ -d '.build' ] && find .build -type f -name '*.d')

endif ###############################################################

clean:
	@rm -frv .build bin

