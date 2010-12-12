DEBUG		= -g
#DEBUG		= -O2
GTK_CFLAGS	:= $(shell pkg-config --cflags gtk+-2.0 goocanvas)
GTK_LDFLAGS	:= $(shell pkg-config --libs gtk+-2.0 goocanvas)
CFLAGS		+= $(DEBUG) -Wall -Werror $(GTK_CFLAGS)
LDFLAGS		+= $(DEBUG) $(GTK_LDFLAGS)
SRCS		= main.c event.c display.c
OBJS		= $(SRCS:.c=.o)
HDRS		= pcb.h
PROG		= depcb

all: $(PROG)

$(OBJS): $(HDRS)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS)

clean:
	-rm -rf $(PROG) $(OBJS)

scope:
	cscope -bq $(SRCS) $(HDRS)
