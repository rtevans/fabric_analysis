#include <stdio.h>
#include <iba/public/iquickmap.h>
#include <oib_utils.h>
#include <oib_utils_sa.h>
#include <topology.h>
#include <opareport.h>
#include <getopt.h>
/*
int                     g_exitstatus  = 0;
int                     g_quiet       = 1;    // omit progress output
EUI64                   g_portGuid    = -1;   // local port to use to access fabric
IB_PORT_ATTRIBUTES      *g_portAttrib = NULL;// attributes for our local port
FabricData_t            g_Fabric;
*/
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

  printf("%s %s %u\n", (char*)portp1->nodep->NodeDesc.NodeString, StlNodeTypeToText(portp1->nodep->NodeInfo.NodeType), portp1->PortNum);

  fstatus = oib_open_port_by_guid(&oib_port_session, g_portGuid);
  if (FSUCCESS != fstatus)
    goto done;

  fstatus = GetPaths(oib_port_session, portp1, portp2, &pQueryResults);
  if (FSUCCESS != fstatus)
    goto done;
  NumPathRecords = ((PATH_RESULTS*)pQueryResults->QueryResult)->NumPathRecords;
  pPathRecords = ((PATH_RESULTS*)pQueryResults->QueryResult)->PathRecords;


  int i,j;
  for (i = 0; i < NumPathRecords; i++) {    

    fstatus = GetTraceRoute(oib_port_session, &pPathRecords[i], &ptQueryResults);
    if (FSUCCESS != fstatus) {
      goto done;
    }
    NumTraceRecords = ((STL_TRACE_RECORD_RESULTS*)ptQueryResults->QueryResult)->NumTraceRecords;
    pTraceRecords = ((STL_TRACE_RECORD_RESULTS*)ptQueryResults->QueryResult)->TraceRecords;
	
    for (j=0; j < NumTraceRecords; j++) {        
      portp1 = portp1->neighbor;
      printf("%s %s %u\n", (char*)portp1->nodep->NodeDesc.NodeString, StlNodeTypeToText(portp1->nodep->NodeInfo.NodeType), portp1->PortNum);

      portp1 = FindNodePort(portp1->nodep, pTraceRecords[i].ExitPort);
      
      printf("%s %s %u\n", (char*)portp1->nodep->NodeDesc.NodeString, StlNodeTypeToText(portp1->nodep->NodeInfo.NodeType), portp1->PortNum);
    }    
  }
  printf("%s %s %u\n", (char*)portp2->nodep->NodeDesc.NodeString, StlNodeTypeToText(portp2->nodep->NodeInfo.NodeType), portp2->PortNum);

  ShowRoutesReport(g_portGuid, &point1, &point2, FORMAT_TEXT, 0, 2);

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
