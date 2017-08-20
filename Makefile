cbcom: cbcom.c
	gcc -o cbcom cbcom.c
install: cbcom
	install cbcom /usr/local/bin

