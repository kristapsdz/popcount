PREFIX		 = /var/www/vhosts/kristaps.bsd.lv
DATADIR		 = /vhosts/kristaps.bsd.lv/data

sinclude Makefile.local

OBJS 		 = db.o main.o
CPPFLAGS	+= -I/usr/local/include
CFLAGS	  	+= -g -W -Wall -Wextra -Wmissing-prototypes
CFLAGS	  	+= -Wstrict-prototypes -Wwrite-strings -Wno-unused-parameter
CFLAGS		+= -DDATADIR=\"$(DATADIR)\"
LDFLAGS		+= -L/usr/local/lib
LDADD		 = -lkcgi -lsqlbox -lsqlite3 -lz -lpthread -lm

all: slugcount.cgi

install: all
	mkdir -p $(PREFIX)/cgi-bin
	mkdir -p $(PREFIX)/data
	install -o www -m 0500 slugcount.cgi $(PREFIX)/cgi-bin
	install -o www -m 0600 slugcount.db $(PREFIX)/data

slugcount.cgi: $(OBJS) slugcount.db
	$(CC) -o $@ -static $(OBJS) $(LDFLAGS) $(LDADD)

clean:
	rm -f $(OBJS) slugcount.cgi db.c extern.h slugcount.db db.sql

db.o: extern.h

db.c: db.ort
	ort-c-source -vh extern.h db.ort >$@

extern.h: db.ort
	ort-c-header -v db.ort >$@

db.sql: db.ort
	ort-sql db.ort >$@

slugcount.db: db.sql db.default.sql
	rm -f $@
	sqlite3 $@ < db.sql
	sqlite3 $@ < db.default.sql
