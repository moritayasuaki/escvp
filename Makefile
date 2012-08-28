obj-m := escvp.o

all:
	make -C /lib/modules/`uname -r`/build M=`pwd` modules

clean:
	rm -f escvp.o escvp.ko escvp.mod.* modules.* Module.*
