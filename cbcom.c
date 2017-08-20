#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <linux/termios.h>
#include <unistd.h>

#define DEFAULT_DEVICE "/dev/ttyUSB0"
#define DEFAULT_BAUD 76800


#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

int ioctl(int d, int request, ...);

void usage(char* name) {
	fprintf(stderr, "Usage: %s [-a] [-x] [-X] [-d " DEFAULT_DEVICE "] [-b " STR(DEFAULT_BAUD) "]\n", name);
}

const char indicator_chars[] = {'\\', '|', '/', '-'};
short indicator_idx = 0;
void indicator(void) {
	fprintf(stderr, "\b%c", indicator_chars[indicator_idx]);
	indicator_idx = (indicator_idx + 1) % 4;
}

int main(int argc, char *argv[]) {
	struct termios2 t;
	
	char *deviceFile = DEFAULT_DEVICE;
	int baud = DEFAULT_BAUD;
	int argsOk = 0;

	int fd;
	int opt;
	int auto_reconnect=0,hexinput=0,hexoutput=0;

	while ((opt = getopt(argc, argv, "ad:b:xX")) != -1) {
		switch (opt) {
		case 'd':
			deviceFile = optarg;
			break;
		case 'b':
			baud = atoi(optarg);
			if (baud<1) baud=76800;
			break;
		case 'a':
			auto_reconnect=1;
			break;
		case 'x':
			hexinput=1;
			break;
		case 'X':
			hexoutput=1;
			break;
		default: /* '?' */
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
/*
	if (optind >= argc) {
		 fprintf(stderr, "Expected argument after options\n");
		 usage(argv[0]);
		 exit(EXIT_FAILURE);
	}*/
reconnect:
	fd = open(deviceFile, O_RDWR | O_NOCTTY | O_NDELAY);

	if (fd == -1) {
		if (auto_reconnect) {
			indicator();
			usleep(30000);
			goto reconnect;
		}
		fprintf(stderr, "error opening %s: %s\n", deviceFile, strerror(errno));
		 usage(argv[0]);
		return 2;
	}

	if (ioctl(fd, TCGETS2, &t)) {
		perror("TCGETS2");
		return 3;
	}

	t.c_cflag &= ~CBAUD;
	t.c_cflag |= BOTHER;
	t.c_ispeed = baud;
	t.c_ospeed = baud;

	if (ioctl(fd, TCSETS2, &t)) {
		perror("TCSETS2");
		return 4;
	}

	if (ioctl(fd, TCGETS2, &t)) {
		perror("TCGETS2");
		return 5;
	}

	printf("connected to %s, actual speed reported %d\n", deviceFile, t.c_ospeed);

	fd_set fds;
	int maxfd = (fd>STDIN_FILENO)?fd:STDIN_FILENO;
	unsigned char stdinbuf[255], serbuf[255];
	while(1) {
		FD_ZERO(&fds); FD_SET(fd, &fds); FD_SET(STDIN_FILENO, &fds);
		select(maxfd+1, &fds, NULL, NULL, NULL);
		if (FD_ISSET(STDIN_FILENO, &fds)) {
			int size=read(STDIN_FILENO, &stdinbuf, sizeof(stdinbuf));
			if (size<1) {
				perror("fgets failed");
				return 2;
			} else {
				if (hexinput) {
					unsigned char *parsepos = stdinbuf, *writepos = stdinbuf;
					stdinbuf[size] = 0;
					while(parsepos) {
						unsigned char* nextpos = NULL;
						int byte = strtol(parsepos, (char**)&nextpos, 16);
						//printf("(%02x)",byte); fflush(stdout);
						if (parsepos == nextpos) break;
						*(writepos++) = byte;
						parsepos = nextpos;
					}
					size=write(fd, &stdinbuf, writepos-stdinbuf);
				} else {
					size=write(fd, &stdinbuf, size);
				}
			}
		}
		if (FD_ISSET(fd, &fds)) {
			int size = read(fd, &serbuf, sizeof(serbuf));
			if (size<1) {
				perror("read serial port");
				if (auto_reconnect) {
					fprintf(stderr, "\nauto-reconnect ...  ");
					usleep(1000);
					goto reconnect;
				}
				return 1;
			} else {
				int i;
				for(i=0;i<size;i++) {
					if (hexoutput) {
						if (i%16==0) printf("\n");
						printf(" %02X", serbuf[i]);
					} else if ((serbuf[i]<32&&serbuf[i]!='\n') || serbuf[i]>126) {
						printf("\\x%02X", serbuf[i]);
					} else {
						printf("%c", serbuf[i]);
					}
				}
				fflush(stdout);
			}
		}
	}

	return 0;
}

