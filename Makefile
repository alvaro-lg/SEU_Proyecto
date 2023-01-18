EXTRA_CFLAGS := -I$(PWD)/
obj-m := spkr.o
	spkr-y := spkr-main.o spkr-io.o
default:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -o getmute getmute.c
	gcc -o setmute setmute.c
clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	$(RM) getmute setmute
	
