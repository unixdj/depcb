DEBUG		= -g
#DEBUG=-O2
GTK_CFLAGS	:= $(shell pkg-config --cflags gtk+-2.0 goocanvas)
GTK_LDFLAGS	:= $(shell pkg-config --libs gtk+-2.0 goocanvas)
CFLAGS		+= $(DEBUG) -Wall -Werror $(GTK_CFLAGS)
LDFLAGS		+= $(DEBUG) $(GTK_LDFLAGS)
OBJS		= main.o event.o display.o
HDRS		= pcb.h
PROG		= depcb

all: $(PROG)

$(OBJS): $(HDRS)

$(PROG): $(OBJS)
	echo $(GTK_CFLAGS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS)

clean:
	-rm -rf $(PROG) $(OBJS)

scope:
	cscope -bq $(OBJS) $(HDRS)

#####
run: all
	./$(PROG) save.pcb

new: all
	./$(PROG) new.pcb ../degate/examples/c3100/Cect_c3100_copper_layer?.jpg

dbg: all
	gdb --args ./$(PROG) new.pcb ../degate/examples/c3100/Cect_c3100_copper_layer?.jpg
