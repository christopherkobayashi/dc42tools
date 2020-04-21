CFLAGS=	-O2 -Wall -Werror


all: dc42cksm

install: all
	install -m755 dc42cksm /usr/local/bin/

clean:
	rm -f dc42cksm *.o
