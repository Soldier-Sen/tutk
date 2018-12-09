UNAME_P=$(shell uname -p)

ifeq ("$(ARCH)", "")
    ifeq ("$(UNAME_P)", "i386")
        ARCH=x86
    else ifeq ("$(UNAME_P)", "x86_64")
        ARCH=x64
    else
        $(info Failed to check the OS architecture with the shell command `uname`. The ARCH will be set to x64.)
        $(info Please run the command `ARCH=[your input] make` to manually set the system ARCH.)
        ARCH=x64
    endif
endif