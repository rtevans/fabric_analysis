#include <stdio.h>
#include <iba/public/iquickmap.h>
#include <oib_utils.h>
#include <oib_utils_sa.h>
#include <topology.h>
#include <getopt.h>
#include "stl_print.h"

int                     g_exitstatus  = 0;
int                     g_quiet       = 1;    // omit progress output
EUI64                   g_portGuid    = -1;   // local port to use to access fabric
IB_PORT_ATTRIBUTES      *g_portAttrib = NULL;// attributes for our local port
FabricData_t            g_Fabric;

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

  /*
  PrintDestInitFile(&g_dest, stdout);
  if (status == FSUCCESS) {
    PrintFunc(&g_dest, "Received MAD:\n");
    PrintMadHeader(&g_dest, 2, &mad->common);
    PrintSeparator(&g_dest);
  }
  */

  pStlDataPortCountersRsp = (STL_DATA_PORT_COUNTERS_RSP *)pStlDataPortCountersReq;
  BSWAP_STL_DATA_PORT_COUNTERS_RSP(pStlDataPortCountersRsp);
  //PrintStlDataPortCountersRsp(&g_dest, 0, pStlDataPortCountersRsp);

  struct _port_dpctrs *dPort;
  dPort = (struct _port_dpctrs *)&(pStlDataPortCountersRsp->Port[0]);

  printf("xmit data %20"PRIu64" MB\n", dPort->PortXmitData/FLITS_PER_MB);
  printf("rcv  data %20"PRIu64" MB\n", dPort->PortRcvData/FLITS_PER_MB);
  printf("xmit flits %20"PRIu64"  \n", dPort->PortXmitData);
  printf("rcv  flits %20"PRIu64"  \n", dPort->PortRcvData);

  printf("xmit pkts %20"PRIu64"   \n", dPort->PortXmitPkts);
  printf("rcv  pkts %20"PRIu64"   \n", dPort->PortRcvPkts);
  printf("MC Xmit Pkts %20"PRIu64"\n", dPort->PortMulticastXmitPkts);
  printf("MC Rcv Pkts  %20"PRIu64"\n", dPort->PortMulticastRcvPkts);

  printf("cong disc %20"PRIu64"   \n", dPort->SwPortCongestion);
  printf("rcv  FECN %20"PRIu64"   \n", dPort->PortRcvFECN);
  printf("rcv  BECN %20"PRIu64"   \n", dPort->PortRcvBECN);
  printf("mark FECN %20"PRIu64"   \n", dPort->PortMarkFECN);
  printf("Xmit Time Cong %20"PRIu64"\n", dPort->PortXmitTimeCong);
  printf( "Xmit Wait %20"PRIu64"\n", dPort->PortXmitWait);

  printf( "Xmit Wasted BW %20"PRIu64"\n", dPort->PortXmitWastedBW);
  printf( "Xmit Wait Data %20"PRIu64"\n", dPort->PortXmitWaitData);
  printf( "Rcv Bubble %20"PRIu64"\n", dPort->PortRcvBubble);
  printf("\n");
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
          "Usage: -s route_src -p route_dst\n"
          "Collect statistics.\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "  -h, --help         display this help and exit\n"
          );
}

int main(int argc, char** argv) {

  FSTATUS fstatus;
  FabricFlags_t sweepFlags = FF_NONE;
  uint64 mkey;

  char *src = NULL;
  char *dst = NULL;

  struct option opts[] = {
    { "help", 0, 0, 'h' },
    { "source", required_argument, 0, 's' },
    { "destination", required_argument, 0, 'd' },
    { NULL, 0, 0, 0 },
  };
  
  int c;
  while ((c = getopt_long(argc, argv, "h:s:d:", opts, 0)) != -1) {
    switch (c) {
    case 'h':
      usage();
      exit(0);
    case 's':
      src = optarg;
      break;
    case 'd':
      dst = optarg;
      break;
    case '?':
      fprintf(stderr, "Try `%s --help' for more information.\n");
      exit(1);
    }
  }

  if (!src) {
    fprintf(stderr, "missing route src\n");
    usage(); 
    goto done;
  }
  if (!dst) {
    fprintf(stderr, "missing route dst\n");
    usage(); 
    goto done;
  }

  Point point1, point2;


  ///////////////////////
  uint32 caCount, portCount;
  uint8  hfi  = 0;
  uint8  port = 1;

  fstatus = iba_get_portguid(hfi, port, NULL, &g_portGuid, NULL, &g_portAttrib,
			     &caCount, &portCount);
  if (FNOT_FOUND == fstatus) {
    fprintf(stderr, "opareport: %s\n",
	    iba_format_get_portguid_error(hfi, port, caCount, portCount));
    g_exitstatus = 1;
    goto done;
  } else if (FSUCCESS != fstatus) {
    fprintf(stderr, "opareport: iba_get_portguid Failed: %s\n",
	    iba_fstatus_msg(fstatus));
    g_exitstatus = 1;
    goto done;
  }
  ////////////////////////

  if (FSUCCESS != InitMad(g_portGuid, NULL)) {
    g_exitstatus = 1;
    goto done;
  }
  if (sweepFlags & FF_SMADIRECT) {
    if (FSUCCESS != InitSmaMkey(mkey)) {
      g_exitstatus = 1;
      goto done;
    }
  }
  
  if (FSUCCESS != Sweep(g_portGuid, &g_Fabric, sweepFlags, SWEEP_ALL, g_quiet)) {
    g_exitstatus = 1;
    goto done;
  }

  struct oib_port *oib_port_session = NULL;
  PQUERY_RESULT_VALUES pQueryResults = NULL;
  PQUERY_RESULT_VALUES ptQueryResults = NULL;
  uint32 NumPathRecords;
  IB_PATH_RECORD *pPathRecords = NULL;
  uint32 NumTraceRecords;
  STL_TRACE_RECORD	*pTraceRecords = NULL;

  PointInit(&point1);
  PointInit(&point2);

  char *p;

  if (FSUCCESS != (fstatus = ParsePoint(&g_Fabric, src, &point1, FIND_FLAG_FABRIC, &p)) || *p != '\0') {
    goto done;
  }

  if (FSUCCESS != (fstatus = ParsePoint(&g_Fabric, dst, &point2, FIND_FLAG_FABRIC, &p)) || *p != '\0') {
    goto done;
  }

  PortData *portp1 = point1.u.portp;
  PortData *portp2 = point2.u.portp;

  fstatus = oib_open_port_by_guid(&oib_port_session, g_portGuid);
  if (FSUCCESS != fstatus)
    goto done;

  fstatus = GetPaths(oib_port_session, portp1, portp2, &pQueryResults);
  if (FSUCCESS != fstatus)
    goto done;
  NumPathRecords = ((PATH_RESULTS*)pQueryResults->QueryResult)->NumPathRecords;
  pPathRecords = ((PATH_RESULTS*)pQueryResults->QueryResult)->PathRecords;
  printf("path recs %d\n",NumPathRecords);

  int i,j;
  for (i = 0; i < NumPathRecords; i++) {    
    fstatus = GetTraceRoute(oib_port_session, &pPathRecords[i], &ptQueryResults);
    if (FSUCCESS != fstatus) {
      goto done;
    }
    NumTraceRecords = ((STL_TRACE_RECORD_RESULTS*)ptQueryResults->QueryResult)->NumTraceRecords;
    pTraceRecords = ((STL_TRACE_RECORD_RESULTS*)ptQueryResults->QueryResult)->TraceRecords;

    PortData *tport = portp1;    
    printf("%s %s %u\n", (char*)tport->nodep->NodeDesc.NodeString, 
	   StlNodeTypeToText(tport->nodep->NodeInfo.NodeType), tport->PortNum);    
    get_stats(tport->EndPortLID, tport->PortNum);
    for (j = 1; j < NumTraceRecords; j++) {        
      if (pTraceRecords[j].NodeType == STL_NODE_FI)
	break;
      tport = tport->neighbor;
      printf("%s %s %u\n", (char*)tport->nodep->NodeDesc.NodeString, 
	     StlNodeTypeToText(tport->nodep->NodeInfo.NodeType), tport->PortNum);
      get_stats(tport->EndPortLID, tport->PortNum);
      tport = FindNodePort(tport->nodep, pTraceRecords[j].ExitPort);          
      printf("%s %s %u\n", (char*)tport->nodep->NodeDesc.NodeString, 
	     StlNodeTypeToText(tport->nodep->NodeInfo.NodeType), tport->PortNum);
      get_stats(tport->EndPortLID, tport->PortNum);
    }
    printf("%s %s %u\n", (char*)portp2->nodep->NodeDesc.NodeString, StlNodeTypeToText(portp2->nodep->NodeInfo.NodeType), portp2->PortNum);
    get_stats(tport->EndPortLID, tport->PortNum);
    break;
  }    

 done:
  PointDestroy(&point1);
  PointDestroy(&point2);
  DestroyMad();
  if (pQueryResults)
    oib_free_query_result_buffer(pQueryResults);
  if (oib_port_session != NULL)
    oib_close_port(oib_port_session);
  //if (pPathRecords)
  //MemoryDeallocate(pPathRecords);

  return 0;
}
