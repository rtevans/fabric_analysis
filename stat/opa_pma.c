#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include "oib_utils.h"
#include "iba/stl_pa.h"
#include "stl_print.h"

uint64_t g_transactID = 0xffffffff12340000;
#define RESP_WAIT_TIME (1000)   // 1000 milliseconds for receive response
PrintDest_t g_dest;

static int get_numports(uint64 portmask) 
{
  int i;
  int nports = 0;
  for (i = 0; i < MAX_PM_PORTS; i++) {
    if ((portmask >> i) & (uint64)1)
      nports++;
  }
  return nports;
} 

static int get_stats(uint32_t dlid, uint32_t port)
{

  int status = 0;
  struct oib_port *mad_port = NULL;
  STL_SMP smp;
  STL_PERF_MAD *mad = (STL_PERF_MAD*)&smp;
  MemoryClear(mad, sizeof(*mad));

  STL_DATA_PORT_COUNTERS_REQ *pStlDataPortCountersReq = (STL_DATA_PORT_COUNTERS_REQ *)&(mad->PerfData);
  STL_DATA_PORT_COUNTERS_RSP *pStlDataPortCountersRsp;
  
  uint32 attrmod;
  uint64 portmask = (uint64)1 << port;
  pStlDataPortCountersReq->PortSelectMask[3] = portmask;
  pStlDataPortCountersReq->VLSelectMask = 0x1;
  attrmod = get_numports(portmask) << 24;
  
  BSWAP_STL_DATA_PORT_COUNTERS_REQ(pStlDataPortCountersReq);

  mad->common.BaseVersion = STL_BASE_VERSION;
  mad->common.ClassVersion = STL_PM_CLASS_VERSION;
  mad->common.MgmtClass = MCLASS_PERF;
  mad->common.u.NS.Status.AsReg16 = 0;
  mad->common.mr.AsReg8 = 0;
  mad->common.mr.s.Method = MMTHD_GET;
  mad->common.AttributeID = STL_PM_ATTRIB_ID_DATA_PORT_COUNTERS;
  mad->common.TransactionID = (++g_transactID);
  mad->common.AttributeModifier = attrmod;

  oib_open_port_by_num(&mad_port, 0, 1);

  if (oib_get_port_state(mad_port) != IB_PORT_ACTIVE)
    fprintf(stderr, "WARNING port (%s:%d) is not ACTIVE!\n",
	    oib_get_hfi_name(mad_port),
	    oib_get_hfi_port_num(mad_port));

  uint16_t pkey = oib_get_mgmt_pkey(mad_port, 0, 0);
  if (pkey==0) {
    fprintf(stderr, "ERROR: Local port does not have management privileges\n");
    return (FPROTECTION);
  }

  BSWAP_MAD_HEADER((MAD*)mad);
  {
    struct oib_mad_addr addr = {
        lid  : dlid,
        qpn  : 1,
        qkey : QP1_WELL_KNOWN_Q_KEY,
        pkey : pkey,
        sl   : 0
    };
    size_t recv_size = sizeof(*mad);
    status = oib_send_recv_mad_no_alloc(mad_port, (uint8_t *)mad, 
					sizeof(STL_DATA_PORT_COUNTERS_REQ)+sizeof(MAD_COMMON), 
					&addr,
					(uint8_t *)mad, 
					&recv_size, RESP_WAIT_TIME, 0);
  }
  BSWAP_MAD_HEADER((MAD*)mad);

  PrintDestInitFile(&g_dest, stdout);
  if (status == FSUCCESS) {
    PrintFunc(&g_dest, "Received MAD:\n");
    PrintMadHeader(&g_dest, 2, &mad->common);
    PrintSeparator(&g_dest);
  }

  pStlDataPortCountersRsp = (STL_DATA_PORT_COUNTERS_RSP *)pStlDataPortCountersReq;  
  BSWAP_STL_DATA_PORT_COUNTERS_RSP(pStlDataPortCountersRsp);
  PrintStlDataPortCountersRsp(&g_dest, 0, pStlDataPortCountersRsp);
  
  if (FSUCCESS == status && mad->common.u.NS.Status.AsReg16 != MAD_STATUS_SUCCESS) {
    fprintf(stderr, "MAD returned with Bad Status: %s\n",
	    iba_mad_status_msg2(mad->common.u.NS.Status.AsReg16));
    return FERROR;
  }

  if (mad_port != NULL)
    oib_close_port(mad_port);
  return status;
}

static void usage(void)
{
  fprintf(stderr,
          "Usage: -l dlid -p port\n"
          "Collect statistics.\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "  -h, --help         display this help and exit\n"
          );
}


int main(int argc, char** argv) {
  uint32_t dlid = 0;
  uint32_t port = 0;

  struct option opts[] = {
    { "help", 0, 0, 'h' },
    { "lid", required_argument, 0, 'l' },
    { "port", 0, 0, 'p' },
    { NULL, 0, 0, 0 },
  };

  int c;
  while ((c = getopt_long(argc, argv, "h:l:p:", opts, 0)) != -1) {
    switch (c) {
    case 'h':
      usage();
      exit(0);
    case 'l':
      dlid = (uint32_t) strtol(optarg, NULL, 16);
      break;
    case 'p':
      port = (uint32_t) strtol(optarg, NULL, 10);
      break;
    case '?':
      fprintf(stderr, "Try `%s --help' for more information.\n");
      exit(1);
    }
  }
  if (dlid == 0) {
    fprintf(stderr, "missing dlid [-l] argument\n");
    exit(1);
  }
  if (port == 0 ) 
    port = 1;
  
  get_stats(dlid, port);

  return 0;
}
