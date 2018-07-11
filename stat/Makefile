CC=gcc
CFLAGS=-I/home1/02561/rtevans/opa-ff/builtinclude.OPENIB_FF.release -I/home1/02561/rtevans/opa-ff/IbAccess/Common/Public -I/home1/02561/rtevans/opa-ff/builtinclude.OPENIB_FF.release/iba -I/usr/include/infiniband -I./
LDFLAGS=-L/home1/02561/rtevans/opa-ff/builtlibs.OPENIB_FF.release -lTopology -loib_utils -lpublic -libverbs -lpthread -libumad -lIbPrint -lm

all: opa_trace opa_pma opa_lids

opa_trace: opa_trace.c
	$(CC) opa_trace.c -o opa_trace $(CFLAGS) $(LDFLAGS)

opa_lids: opa_lids.c
	$(CC) opa_lids.c -o opa_lids $(CFLAGS) $(LDFLAGS)

opa_pma: opa_pma.c
	$(CC) opa_pma.c -o opa_pma $(CFLAGS) $(LDFLAGS)

opa_report.o: opareport.c
	$(CC) opareport.c -c $(CFLAGS) $(LDFLAGS)

clean:
	rm opa_trace opa_pma opa_lids opareport.o *~

