OBJS = orta_data.o orta_ctrl_udp.o orta_ctrl_tcp.o fifo_queue.o	\
ordered_queue.o linked_list.o routing_table.o links.o members.o	\
neighbours.o orta.o # netTCP.o # orta_debug.o

OBJS = fifo_queue.o links.o neighbours.o ordered_queue.o		\
orta_ctrl_tcp.o orta_data.o routing_table.o linked_list.o members.o	\
netTCP.o orta.o orta_ctrl_udp.o orta_routing.o orta_debug.o dijkstra.o

INCLUDE = 

# Debug options are:
#  -DORTA_DEBUG
#  -DPACKET_DEBUG
#  -DDEBUG_PRINT_STATE
# Testing options are:
#  -DCONTROL_EVAL
#  -DEVAL_RDP
#  
# Set these options by uncommenting the following line, and adding the
# debug options you want, separated by spaces.

# DEBUG   = -DORTA_DEBUG -DDEBUG_PRINT_STATE

LIBNAME = orta


all: lib$(LIBNAME).a

lib$(LIBNAME).a: $(OBJS)
	rm -f $@
	ar rc $@ $(OBJS)
	ranlib $@

.c.o:
	gcc $(DEBUG) $(INCLUDE) -o $*.o -c $<

clean:
	rm -f *.o *.a
