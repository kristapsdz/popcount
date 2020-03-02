PREFIX		 = /var/www/vhosts/kristaps.bsd.lv
DATADIR		 = /vhosts/kristaps.bsd.lv/data

sinclude Makefile.local

OBJS 		  = db.o main.o
CFLAGS	  	 += -g -W -Wall -Wextra -Wmissing-prototypes
CFLAGS	  	 += -Wstrict-prototypes -Wwrite-strings -Wno-unused-parameter
CFLAGS		 += -DDATADIR=\"$(DATADIR)\"
CFLAGS		+!= pkg-config --cflags kcgi-json sqlbox
LIBS		 != pkg-config --libs --static kcgi-json sqlbox

all: popcount.cgi

install: all
	mkdir -p $(PREFIX)/cgi-bin
	mkdir -p $(PREFIX)/data
	install -o www -m 0500 popcount.cgi $(PREFIX)/cgi-bin
	install -o www -m 0600 popcount.db $(PREFIX)/data

update: all
	mkdir -p $(PREFIX)/cgi-bin
	install -o www -m 0500 popcount.cgi $(PREFIX)/cgi-bin

popcount.cgi: $(OBJS) popcount.db
	$(CC) -o $@ -static $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) popcount.cgi db.c extern.h popcount.db db.sql

$(OBJS): extern.h

db.c: db.ort
	ort-c-source -vh extern.h db.ort >$@

extern.h: db.ort
	ort-c-header -v db.ort >$@

db.sql: db.ort
	ort-sql db.ort >$@

popcount.db: db.sql db.default.sql
	rm -f $@
	sqlite3 $@ < db.sql
	sqlite3 $@ < db.default.sql
