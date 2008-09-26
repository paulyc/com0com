/*
 * $Id$
 *
 * Copyright (c) 2006-2008 Vyacheslav Frolov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * $Log$
 * Revision 1.14  2008/09/26 14:29:13  vfrolov
 * Added substitution <PRM0> by <file> for --load=<file>
 *
 * Revision 1.13  2008/08/28 15:53:13  vfrolov
 * Added ability to load arguments from standard input and
 * to select fragment for loading
 *
 * Revision 1.12  2008/08/25 08:15:02  vfrolov
 * Itilized TimerAPCProc()
 *
 * Revision 1.11  2008/04/16 14:13:59  vfrolov
 * Added ability to specify source posts for OUT method
 *
 * Revision 1.10  2008/04/14 07:32:03  vfrolov
 * Renamed option --use-port-module to --use-driver
 *
 * Revision 1.9  2008/03/28 15:53:48  vfrolov
 * Fixed help
 *
 * Revision 1.8  2008/03/26 08:48:18  vfrolov
 * Initial revision
 *
 * Revision 1.7  2008/02/04 10:08:49  vfrolov
 * Fixed <LstR>:<LstL> parsing bug
 *
 * Revision 1.6  2007/12/19 13:46:36  vfrolov
 * Added ability to send data received from port to the same port
 *
 * Revision 1.5  2007/05/14 12:06:37  vfrolov
 * Added read interval timeout option
 *
 * Revision 1.4  2007/02/06 11:53:33  vfrolov
 * Added options --odsr, --ox, --ix and --idsr
 * Added communications error reporting
 *
 * Revision 1.3  2007/02/05 09:33:20  vfrolov
 * Implemented internal flow control
 *
 * Revision 1.2  2007/02/01 12:14:59  vfrolov
 * Redesigned COM port params
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "comhub.h"
#include "filters.h"
#include "utils.h"
#include "plugins.h"
#include "route.h"

///////////////////////////////////////////////////////////////
static void Usage(const char *pProgPath, Plugins &plugins)
{
  cerr
  << "Usage:" << endl
  << "  " << pProgPath << " [options] <port0> [options] [<port1> ...]" << endl
  << endl
  << "Common options:" << endl
  << "  --load=[<file>][,<begin>[,<end>]][:<prms>]" << endl
  << "                           - load arguments (one argument per line) between" << endl
  << "                             <begin> and <end> lines from a file <file> (use" << endl
  << "                             standard input if empty) and insert them into the" << endl
  << "                             command line. The syntax of <prms> is" << endl
  << "                             <PRM1>[,<PRM2>...], where <PRMn> will replace" << endl
  << "                             %%n%% in the arguments. Do loading since begining" << endl
  << "                             if <begin> is empty. Do loading till end-of-file" << endl
  << "                             if <end> is empty. Ignore arguments begining with" << endl
  << "                             '#'. <file> will replace %%0%% in the arguments." << endl
  << "  --help                   - show this help." << endl
  << "  --help=*                 - show help for all modules." << endl
  << "  --help=<LstM>            - show help for modules listed in <LstM>." << endl
  << endl
  << "  The syntax of <LstM> above is <MID0>[,<MID1>...], where <MIDn> is a module" << endl
  << "  name." << endl
  << endl
  << "Route options:" << endl
  << "  --route=<LstR>:<LstL>    - send data received from any port listed in <LstR>" << endl
  << "                             to all ports (except itself) listed in <LstL>." << endl
  << "  --bi-route=<LstR>:<LstL> - send data received from any port listed in <LstR>" << endl
  << "                             to all ports (except itself) listed in <LstL> and" << endl
  << "                             vice versa." << endl
  << "  --echo-route=<Lst>       - send data received from any port listed in <Lst>" << endl
  << "                             back to itself via all attached filters." << endl
  << "  --no-route=<LstR>:<LstL> - do not send data received from any port listed in" << endl
  << "                             <LstR> to the ports listed in <LstL>." << endl
  << endl
  << "  If no any route option specified, then the options --route=0:All --route=1:0" << endl
  << "  used by default (route data from first port to all ports and from second" << endl
  << "  port to first port)." << endl
  << endl
  << "Filter options:" << endl
  << "  --create-filter=<MID>[,<FID>][:<Args>]" << endl
  << "                           - by using filter module with name <MID> create a" << endl
  << "                             filter with name <FID> (<FID> is <MID> by default)" << endl
  << "                             and put arguments <Args> (if any) to the filter." << endl
  << "  --add-filters=<Lst>:<LstF>" << endl
  << "                           - attach the filters listed in <LstF> to the ports" << endl
  << "                             listed in <Lst>. These filters will handle the" << endl
  << "                             data by IN method just after receiving from ports" << endl
  << "                             listed in <Lst> or by OUT method just before" << endl
  << "                             sending to ports listed in <Lst>." << endl
  << endl
  << "  The syntax of <LstF> above is <F1>[,<F2>...], where the syntax of <Fn> is" << endl
  << "  <FID>[.<Method>][(<Lst>)], where <FID> is a filter name, <Method> is IN or" << endl
  << "  OUT and <Lst> lists the source ports (the data only from them will be handled" << endl
  << "  by OUT method). The <FID> w/o <Method> is equivalent to adding IN and OUT for" << endl
  << "  each filter with name <FID>. If the list of the source ports is not specified" << endl
  << "  then the data routed from any port will be handled by OUT method." << endl
  << endl
  << "Port options:" << endl
  << "  --use-driver=<MID>       - use driver module with name <MID> to create the" << endl
  << "                             following ports (<MID> is serial by default)." << endl
  << endl
  << "The syntax of <LstR>, <LstL> and <Lst> above is <P1>[,<P2>...], where <Pn> is a" << endl
  << "zero based position number of port or All." << endl
  ;
  plugins.List(cerr);
  cerr
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --route=All:All \\\\.\\CNCB0 \\\\.\\CNCB1 \\\\.\\CNCB2" << endl
  << "    - receive data from CNCB0 and send it to CNCB1 and CNCB2," << endl
  << "      receive data from CNCB1 and send it to CNCB0 and CNCB2," << endl
  << "      receive data from CNCB2 and send it to CNCB0 and CNCB1." << endl
  << "  " << pProgPath << " --echo-route=0 COM2" << endl
  << "    - receive data from COM2 and send it back to COM2." << endl
  << "  " << pProgPath << " --load=" << endl
  << "      --echo-route=0" << endl
  << "      COM2" << endl
  << "      ^Z" << endl
  << "    - the same as above." << endl
  << "  " << pProgPath << " --load=,_BEGIN_,_END_" << endl
  << "      blah blah blah" << endl
  << "      _BEGIN_" << endl
  << "      --echo-route=0" << endl
  << "      COM2" << endl
  << "      _END_" << endl
  << "    - the same as above." << endl
  ;
}
///////////////////////////////////////////////////////////////
static BOOL EnumPortList(
    ComHub &hub,
    const char *pList,
    BOOL (*pFunc)(ComHub &hub, int iPort, PVOID p0, PVOID p1, PVOID p2),
    PVOID p0 = NULL,
    PVOID p1 = NULL,
    PVOID p2 = NULL)
{
  char *pTmpList = _strdup(pList);

  if (!pTmpList) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  BOOL res = TRUE;
  char *pSave;

  for (char *p = STRTOK_R(pTmpList, ",", &pSave) ; p ; p = STRTOK_R(NULL, ",", &pSave)) {
    int i;

    if (_stricmp(p, "All") == 0) {
      for (i = 0 ; i < hub.NumPorts() ; i++) {
        if (!pFunc(hub, i, p0, p1, p2))
          res = FALSE;
      }
    } else if (StrToInt(p, &i) && i >= 0 && i < hub.NumPorts()) {
      if (!pFunc(hub, i, p0, p1, p2))
        res = FALSE;
    } else {
      cerr << "Invalid port " << p << endl;
      res = FALSE;
    }
  }

  free(pTmpList);

  return res;
}
///////////////////////////////////////////////////////////////
static BOOL EchoRoute(ComHub &/*hub*/, int iPort, PVOID pMap, PVOID /*p1*/, PVOID /*p2*/)
{
  AddRoute(*(PortNumMap *)pMap, iPort, iPort, FALSE, FALSE);
  return TRUE;
}

static void EchoRoute(ComHub &hub, const char *pList, PortNumMap &map)
{
  if (!EnumPortList(hub, pList, EchoRoute, &map)) {
    cerr << "Invalid echo route " << pList << endl;
    exit(1);
  }
}
///////////////////////////////////////////////////////////////
static BOOL Route(ComHub &/*hub*/, int iTo, PVOID pIFrom, PVOID pNoRoute, PVOID pMap)
{
  AddRoute(*(PortNumMap *)pMap, *(int *)pIFrom, iTo, *(BOOL *)pNoRoute, TRUE);
  return TRUE;
}

static BOOL RouteList(ComHub &hub, int iFrom, PVOID pListTo, PVOID pNoRoute, PVOID pMap)
{
  return EnumPortList(hub, (const char *)pListTo, Route, &iFrom, pNoRoute, pMap);
}

static BOOL Route(
    ComHub &hub,
    const char *pListFrom,
    const char *pListTo,
    BOOL noRoute,
    PortNumMap &map)
{
  return EnumPortList(hub, pListFrom, RouteList, (PVOID)pListTo, &noRoute, &map);
}
///////////////////////////////////////////////////////////////
static void Route(
    ComHub &hub,
    const char *pParam,
    BOOL biDirection,
    BOOL noRoute,
    PortNumMap &map)
{
  char *pTmp = _strdup(pParam);

  if (!pTmp) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  char *pSave;
  const char *pListR = STRTOK_R(pTmp, ":", &pSave);
  const char *pListL = STRTOK_R(NULL, ":", &pSave);

  if (!pListR || !pListL ||
      !Route(hub, pListR, pListL, noRoute, map) ||
      (biDirection && !Route(hub, pListL, pListR, noRoute, map)))
  {
    cerr << "Invalid route " << pParam << endl;
    exit(1);
  }

  free(pTmp);
}
///////////////////////////////////////////////////////////////
static void CreateFilter(
    const Plugins &plugins,
    Filters &filter,
    const char *pParam)
{
  char *pTmp = _strdup(pParam);

  if (!pTmp) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  char *pSave;

  char *pPlugin = STRTOK_R(pTmp, ":", &pSave);
  char *pArgs = STRTOK_R(NULL, "", &pSave);

  if (!pPlugin || !*pPlugin) {
    cerr << "No module name." << endl;
    exit(1);
  }

  const char *pPluginName = STRTOK_R(pPlugin, ",", &pSave);

  if (!pPluginName || !*pPluginName) {
    cerr << "No module name." << endl;
    exit(1);
  }

  HCONFIG hConfig;

  const FILTER_ROUTINES_A *pFltRoutines =
      (const FILTER_ROUTINES_A *)plugins.GetRoutines(PLUGIN_TYPE_FILTER, pPluginName, &hConfig);

  if (!pFltRoutines) {
    cerr << "No filter module " << pPluginName << endl;
    exit(1);
  }

  const char *pFilterName = STRTOK_R(NULL, "", &pSave);

  if (!pFilterName || !*pFilterName)
    pFilterName = pPluginName;

  if (!filter.CreateFilter(pFltRoutines, pFilterName, hConfig, pArgs)) {
    cerr << "Invalid filter " << pParam << endl;
    exit(1);
  }

  free(pTmp);
}
///////////////////////////////////////////////////////////////
static BOOL AddFilters(ComHub &hub, int iPort, PVOID pFilters, PVOID pListFlt, PVOID /*p2*/)
{
  char *pTmpList = _strdup((const char *)pListFlt);

  if (!pTmpList) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  char *pSave;

  for (char *pFilter = STRQTOK_R(pTmpList, ",", &pSave, "()", FALSE) ;
       pFilter ;
       pFilter = STRQTOK_R(NULL, ",", &pSave, "()", FALSE))
  {
    char *pSave2;

    string filter(STRTOK_R(pFilter, "(", &pSave2));
    char *pList = STRTOK_R(NULL, ")", &pSave2);

    set<int> *pSrcPorts = NULL;

    if (pList) {
      for (char *p = STRTOK_R(pList, ",", &pSave2) ; p ; p = STRTOK_R(NULL, ",", &pSave2)) {
        int i;

        if (_stricmp(p, "All") == 0) {
          if (pSrcPorts) {
            delete pSrcPorts;
            pSrcPorts = NULL;
          }
          break;
        } else if (StrToInt(p, &i) && i >= 0 && i < hub.NumPorts()) {
          if (!pSrcPorts) {
            pSrcPorts = new set<int>;

            if (!pSrcPorts) {
              cerr << "No enough memory." << endl;
              exit(2);
            }
          }

          pSrcPorts->insert(i);
        } else {
          cerr << "Invalid port " << p << endl;
          exit(1);
        }
      }
    }

    string::size_type dot = filter.rfind('.');
    string method(dot != filter.npos ? filter.substr(dot) : "");

    if (method == ".IN") {
      if (!((Filters *)pFilters)->AddFilter(iPort, filter.substr(0, dot).c_str(), TRUE, FALSE, NULL))
        exit(1);
    }
    else
    if (method == ".OUT") {
      if (!((Filters *)pFilters)->AddFilter(iPort, filter.substr(0, dot).c_str(), FALSE, TRUE, pSrcPorts))
        exit(1);
    }
    else {
      if (!((Filters *)pFilters)->AddFilter(iPort, filter.c_str(), TRUE, TRUE, pSrcPorts))
        exit(1);
    }

    if (pSrcPorts)
      delete pSrcPorts;
  }

  free(pTmpList);

  return TRUE;
}

static void AddFilters(ComHub &hub, Filters &filters, const char *pParam)
{
  char *pTmp = _strdup(pParam);

  if (!pTmp) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  char *pSave;
  const char *pList = STRTOK_R(pTmp, ":", &pSave);
  const char *pListFlt = STRTOK_R(NULL, "", &pSave);

  if (!pList || !*pList || !pListFlt || !*pListFlt) {
    cerr << "Invalid filter parameters " << pParam << endl;
    exit(1);
  }

  if (!EnumPortList(hub, pList, AddFilters, &filters, (PVOID)pListFlt)) {
    cerr << "Can't add filters " << pListFlt << " to ports " << pList << endl;
    exit(1);
  }

  free(pTmp);
}
///////////////////////////////////////////////////////////////
static void Init(ComHub &hub, int argc, const char *const argv[])
{
  Args args(argc - 1, argv + 1);

  for (vector<string>::const_iterator i = args.begin() ; i != args.end() ; i++) {
    if (!GetParam(i->c_str(), "--"))
      hub.Add();
  }

  BOOL defaultRouteData = TRUE;
  BOOL defaultRouteFlowControl = TRUE;
  int plugged = 0;
  Plugins *pPlugins = new Plugins();

  if (!pPlugins) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  Filters *pFilters = NULL;

  PortNumMap routeDataMap;
  PortNumMap routeFlowControlMap;

  const char *pUseDriver = "serial";

  for (vector<string>::const_iterator i = args.begin() ; i != args.end() ; i++) {
    BOOL ok = pPlugins->Config(i->c_str());
    const char *pArg = GetParam(i->c_str(), "--");

    if (!pArg) {
      HCONFIG hConfig;

      const PORT_ROUTINES_A *pPortRoutines =
          (const PORT_ROUTINES_A *)pPlugins->GetRoutines(PLUGIN_TYPE_DRIVER, pUseDriver, &hConfig);

      if (!pPortRoutines) {
        cerr << "No driver " << pUseDriver << endl;
        exit(1);
      }

      if (!hub.CreatePort(pPortRoutines, plugged++, hConfig, i->c_str()))
        exit(1);

      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "help")) != NULL && *pParam == 0) {
      Usage(argv[0], *pPlugins);
      exit(0);
    } else
    if ((pParam = GetParam(pArg, "help=")) != NULL) {
      char *pTmpList = _strdup(pParam);

      if (!pTmpList) {
        cerr << "No enough memory." << endl;
        exit(2);
      }

      char *pSave;

      for (char *p = STRTOK_R(pTmpList, ",", &pSave) ; p ; p = STRTOK_R(NULL, ",", &pSave))
        pPlugins->Help(argv[0], p);

      free(pTmpList);
      exit(0);
    } else
    if ((pParam = GetParam(pArg, "route=")) != NULL) {
      defaultRouteData = FALSE;
      Route(hub, pParam, FALSE, FALSE, routeDataMap);
    } else
    if ((pParam = GetParam(pArg, "bi-route=")) != NULL) {
      defaultRouteData = FALSE;
      Route(hub, pParam, TRUE, FALSE, routeDataMap);
    } else
    if ((pParam = GetParam(pArg, "no-route=")) != NULL) {
      defaultRouteData = FALSE;
      Route(hub, pParam, FALSE, TRUE, routeDataMap);
    } else
    if ((pParam = GetParam(pArg, "echo-route=")) != NULL) {
      defaultRouteData = FALSE;
      EchoRoute(hub, pParam, routeDataMap);
    } else
    if ((pParam = GetParam(pArg, "create-filter=")) != NULL) {
      if (!pFilters)
        pFilters = new Filters(hub);

      if (!pFilters) {
        cerr << "No enough memory." << endl;
        exit(2);
      }

      CreateFilter(*pPlugins, *pFilters, pParam);
    } else
    if ((pParam = GetParam(pArg, "add-filters=")) != NULL) {
      if (!pFilters) {
        cerr << "No create-filter option before " << i->c_str() << endl;
        exit(1);
      }

      AddFilters(hub, *pFilters, pParam);
    } else
    if ((pParam = GetParam(pArg, "use-driver=")) != NULL) {
      pUseDriver = pParam;
    } else {
      if (!ok) {
        cerr << "Unknown option " << i->c_str() << endl;
        exit(1);
      }
    }
  }

  if (plugged < 1) {
    Usage(argv[0], *pPlugins);
    exit(1);
  }

  delete pPlugins;

  if (plugged > 1 && defaultRouteData) {
    Route(hub, "0:All", FALSE, FALSE, routeDataMap);
    Route(hub, "1:0", FALSE, FALSE, routeDataMap);
  }

  if (defaultRouteFlowControl) {
    SetFlowControlRoute(routeFlowControlMap, routeDataMap, FALSE);
  } else {
  }

  hub.SetFlowControlRoute(routeFlowControlMap);
  hub.SetDataRoute(routeDataMap);

  hub.SetFilters(pFilters);
  hub.RouteReport();

  if (pFilters)
    pFilters->Report();
}
///////////////////////////////////////////////////////////////
static VOID CALLBACK TimerAPCProc(
  LPVOID pArg,
  DWORD /*dwTimerLowValue*/,
  DWORD /*dwTimerHighValue*/)
{
  ((ComHub *)pArg)->LostReport();
}
///////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
  ComHub hub;

  Init(hub, argc, argv);

  if (hub.StartAll()) {
    HANDLE hTimer = ::CreateWaitableTimer(NULL, FALSE, NULL);

    if (hTimer) {
      LARGE_INTEGER firstReportTime;

      firstReportTime.QuadPart = -100000000;

      if (!::SetWaitableTimer(hTimer, &firstReportTime, 10000, TimerAPCProc, &hub, FALSE)) {
        DWORD err = GetLastError();

        cerr << "WARNING: SetWaitableTimer() - error=" << err << endl;

        ::CloseHandle(hTimer);
      }
    } else {
      DWORD err = GetLastError();

      cerr << "WARNING: CreateWaitableTimer() - error=" << err << endl;
    }

    for (;;)
      ::SleepEx(INFINITE, TRUE);
  }

  return 1;
}
///////////////////////////////////////////////////////////////
