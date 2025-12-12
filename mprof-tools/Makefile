# mprof-tools/Makefile

HOST_ARCH := $(shell uname -m)
BPF_ARCH  := $(shell echo $(HOST_ARCH) | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')

BPFTOOL ?= bpftool
CLANG   ?= clang
CC      ?= cc

CFLAGS  ?= -g -O2 -Wa# mprof-tools/Makefile

HOST_ARCH := $(shell uname -m)
BPF_ARCH  := $(shell echo $(HOST_ARCH) | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')

BPFTOOL ?= bpftool
CLANG   ?= clang
CC      ?= cc

CFLAGS  ?= -g -O2 -Wall -Wextra
LDLIBS  ?= -lbpf -lelf -lz

BPF_INCLUDES := -I. -I/usr/include/$(HOST_ARCH)-linux-gnu

# Tools to build
TOOLS := alloc_hist alloc_stacks

.PHONY: all clean
all: $(TOOLS)

# ------------------------------------------------------------
# vmlinux.h
# ------------------------------------------------------------
vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

# ------------------------------------------------------------
# BPF object
# ------------------------------------------------------------
%.bpf.o: %.bpf.c vmlinux.h
	$(CLANG) -g -O2 -target bpf \
		-D__TARGET_ARCH_$(BPF_ARCH) \
		$(BPF_INCLUDES) \
		-c $< -o $@

# ------------------------------------------------------------
# Skeleton header
# ------------------------------------------------------------
%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

# ------------------------------------------------------------
# User binary
# IMPORTANT: explicitly depend on the skeleton
# ------------------------------------------------------------
alloc_hist: alloc_hist.c alloc_hist.skel.h
	$(CC) $(CFLAGS) -I. -o $@ $< $(LDLIBS)

alloc_stacks: alloc_stacks.c alloc_stacks.skel.h
	$(CC) $(CFLAGS) -I. -o $@ $< $(LDLIBS)

clean:
	rm -f $(TOOLS) *.o *.skel.h vmlinux.hll -Wextra
LDLIBS  ?= -lbpf -lelf -lz

BPF_INCLUDES := -I. -I/usr/include/$(HOST_ARCH)-linux-gnu

# Tools to build
TOOLS := alloc_hist alloc_stacks

.PHONY: all clean
all: $(TOOLS)

# ------------------------------------------------------------
# vmlinux.h
# ------------------------------------------------------------
vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

# ------------------------------------------------------------
# BPF object
# ------------------------------------------------------------
%.bpf.o: %.bpf.c vmlinux.h
	$(CLANG) -g -O2 -target bpf \
		-D__TARGET_ARCH_$(BPF_ARCH) \
		$(BPF_INCLUDES) \
		-c $< -o $@

# ------------------------------------------------------------
# Skeleton header
# ------------------------------------------------------------
%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

# ------------------------------------------------------------
# User binary
# IMPORTANT: explicitly depend on the skeleton
# ------------------------------------------------------------
alloc_hist: alloc_hist.c alloc_hist.skel.h
	$(CC) $(CFLAGS) -I. -o $@ $< $(LDLIBS)

alloc_stacks: alloc_stacks.c alloc_stacks.skel.h
	$(CC) $(CFLAGS) -I. -o $@ $< $(LDLIBS)

clean:
	rm -f $(TOOLS) *.o *.skel.h vmlinux.h