#include <stdio.h>
#include <iba/public/iquickmap.h>
#include <topology.h>

int                     g_exitstatus  = 0;
int                     g_quiet       = 1;    // omit progress output
EUI64                   g_portGuid    = -1;   // local port to use to access fabric
IB_PORT_ATTRIBUTES      *g_portAttrib = NULL;// attributes for our local port
FabricData_t            g_Fabric;

int main() {

  FSTATUS fstatus;
  FabricFlags_t sweepFlags = FF_NONE;
  uint64 mkey;
  cl_map_item_t *pmap;
  cl_pfn_qmap_compare_key_t key_compare;
  PortData *portp;
  NodeData *nodep;  
  
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

  printf( "%u LID(s) in Fabric\n", (unsigned)cl_qmap_count(&g_Fabric.u.AllLids));
  for (pmap = cl_qmap_head(&g_Fabric.u.AllLids); 
       pmap != cl_qmap_end(&g_Fabric.u.AllLids); 
       pmap = cl_qmap_next(pmap)) {

    portp = PARENT_STRUCT(pmap, PortData, AllLidsEntry);
    nodep = portp->nodep;
    printf( "0x%04x",
	    portp->PortInfo.LID);
    printf( " 0x%016"PRIx64" %3u %s %s\n",
	    nodep->NodeInfo.NodeGUID, portp->PortNum,
	    StlNodeTypeToText(nodep->NodeInfo.NodeType),
	    (char*)nodep->NodeDesc.NodeString );
  }

 done:
  DestroyMad();
 return 0;
}
