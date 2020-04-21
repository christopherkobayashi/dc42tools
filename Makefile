CFLAGS=	-O2 -Wall -Werror -ggdb


all: dc42cksm dsk2dc42

install: all
	install -m755 dc42cksm dsk2dc42 /usr/local/bin/

clean:
	rm -f dc42cksm dsk2dc42 *.o
