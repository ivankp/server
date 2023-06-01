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
  find src -type f -regex '.*\.cc?$$' \
  | xargs grep -l '^\s*int\s\+main\s*(' \
  | sed 's:^src/\(.*\)\.c\+$$:bin/\1:'
EXE := $(shell $(FIND_MAIN))

all: $(EXE)

.build/addr_blacklist.o: CFLAGS += -DSERVER_VERBOSE_BLACKLIST

.build/server.o: CFLAGS += -pthread
.build/socket.o: CFLAGS += -pthread
.build/examples/echo_server.o: CFLAGS += -pthread

bin/examples/echo_server: $(patsubst %, .build/%.o, \
  socket error addr_ip4 \
  server addr_blacklist \
)
bin/examples/echo_server: LDFLAGS += -pthread

bin/examples/http_server: $(patsubst %, .build/%.o, \
  socket error addr_ip4 \
  server http file mime \
)
bin/examples/http_server: LDFLAGS += -pthread

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

