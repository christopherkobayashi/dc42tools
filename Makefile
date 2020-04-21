CFLAGS=	-O2 -Wall -Werror -ggdb


all: dc42cksm makedc42

install: all
	install -m755 dc42cksm makedc42 /usr/local/bin/

clean:
	rm -f dc42cksm makedc42 *.o
