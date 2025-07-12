# Makefile
TARGETS := 01_funcPtr 02_file 03_swap
CXXFLAGS += -Iinclude

SRCS_01_funcPtr := 01_funcPtr.cc
SRCS_02_file    := 02_file.cc fileMonitor.cc
SRCS_03_swap    := 03_swap.cc

include config.mk
