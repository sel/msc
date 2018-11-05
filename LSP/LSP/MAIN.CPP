// Module Name: main.cpp
//
// Description:
//
//    This sample illustrates how to develop a layered service provider that is
//    capable of counting all bytes transmitted through an IP socket. The application
//    reports when sockets are created and reports how many bytes were sent and
//    received when a socket closes. The results are reported using the OutputDebugString
//    API which will allow you to intercept the I/O by using a debugger such as cdb.exe
//    or you can monitor the I/O using dbmon.exe.
//
//    This file contains the 30 SPI functions you are required to implement in a
//    service provider. It also contains the two functions that must be exported
//    from the DLL module DllMain and WSPStartup.
//    
//
// Compile:
//
//    This project is managed through the LSP.DSW project file.
//
// Execute:
//
//    This project produces a DLL named lsp.dll. This dll should be copied to the
//    %SystemRoot%\System32 directory. Once the file is in place you should execute
//    the application instlsp.exe to insert this provider in the Winsock 2 catalog
//    of service providers.
//

#include "provider.h"

WSPUPCALLTABLE MainUpCallTable;
DWORD gLayerCatId = 0;
DWORD gChainId = 0;
DWORD gEntryCount = 0;
CRITICAL_SECTION gCriticalSection;
LPWSPDATA gWSPData = NULL;
WSPPROC_TABLE NextProcTable;
LPWSPPROC_TABLE gProcTable = NULL;
LPWSAPROTOCOL_INFOW gBaseInfo = NULL;
HINSTANCE HDllInstance = NULL;
HINSTANCE hProvider = NULL;
INT gLayerCount=0;                    // Number of base providers we're layered over

static TCHAR Msg[512];

/*
*
*			DllMain Function
*
*/

BOOL WINAPI DllMain(IN HINSTANCE hinstDll, IN DWORD dwReason, LPVOID lpvReserved)
{
    switch (dwReason)
    {

        case DLL_PROCESS_ATTACH:
            HDllInstance = hinstDll;
            InitializeCriticalSection(&gCriticalSection);
//            InitAsyncSelectCS();
            InitOverlappedCS();
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
} // end of DllMain Function






// The next four functions come from the IpRoute.cpp example in the MS Network Programming book

//----------------------------------------------------------------------------
// If returned status is NO_ERROR, then pIpRouteTab points to a routing
// table.
//----------------------------------------------------------------------------
DWORD MyGetIpForwardTable(PMIB_IPFORWARDTABLE& pIpRouteTab, BOOL fOrder)
{
    DWORD status = NO_ERROR;
    DWORD statusRetry = NO_ERROR;
    DWORD dwActualSize = 0;
    
    // query for buffer size needed
    status = GetIpForwardTable(pIpRouteTab, &dwActualSize, fOrder);

    if (status == NO_ERROR)
        return status;
    else if (status == ERROR_INSUFFICIENT_BUFFER)
    {
        // need more space

        pIpRouteTab = (PMIB_IPFORWARDTABLE) malloc(dwActualSize);
        assert(pIpRouteTab);
        
        statusRetry = GetIpForwardTable(pIpRouteTab, &dwActualSize, fOrder);
        return statusRetry;
    }
    else
    {
        return status;
    }

}

void DoDeleteIpForwardEntry(char* pszDest)
{
    DWORD dwStatus, dwDelStatus, i;
    PMIB_IPFORWARDTABLE pIpRouteTab = NULL; // Ip routing table
    MIB_IPFORWARDROW routeEntry;            // Ip routing table row entry
    DWORD dwForwardDest = 0;
    bool fDeleted = FALSE;
    //TCHAR error_buffer[128];

    memset(&routeEntry, 0, sizeof(MIB_IPFORWARDROW));
    dwForwardDest = inet_addr(pszDest); // convert dotted ip addr. to ip addr.
    if (dwForwardDest == INADDR_NONE)
    {
        //wsprintf(error_buffer, L"IpRoute: Bad Destination %s\n", pszDest);
		printf("IpRoute: Bad Destination %s\n", pszDest);
        return;
    }


    if ( (dwStatus = MyGetIpForwardTable(pIpRouteTab, TRUE)) == NO_ERROR)
    {
        for (i = 0; i < pIpRouteTab->dwNumEntries; i++)
        {
            if (dwForwardDest == pIpRouteTab->table[i].dwForwardDest)
            {
                memcpy(&routeEntry, &(pIpRouteTab->table[i]), sizeof(MIB_IPFORWARDROW));
                dwDelStatus = DeleteIpForwardEntry(&routeEntry); 
                if (dwDelStatus != NO_ERROR)
                {
                    //OutputDebugString(L"\nIpRoute: couldn't delete ");
					//PrintIPString(pszDest);
                    //wsprintf(error_buffer, L"dwDelStatus = %lu.\n", dwDelStatus);
					//OutputDebugString(error_buffer);
					printf("\nIpRoute: couldn't delete %s - dwDelStatus = %lu.\n", pszDest, dwDelStatus);
					return;
                }
                else
                    fDeleted = TRUE;
            }
        }
        free(pIpRouteTab);
        if (! fDeleted)
            //OutputDebugString(L"\nIpRoute: The route specified was not found.\n");
        return;
    }
    else if ( dwStatus == ERROR_NO_DATA)
    {
        //OutputDebugString(L"\nIpRoute: No entries in route table.\n");
		printf("\nIpRoute: No entries in route table.\n");
        if (pIpRouteTab)
            free (pIpRouteTab);
        return;
    }
    else
    {
        if (pIpRouteTab)
            free (pIpRouteTab);
        //wsprintf(error_buffer, L"\nIpRoute returned 0x%x\n", dwStatus);
		//OutputDebugString(error_buffer);
		printf("\nIpRoute returned 0x%x\n", dwStatus);
        return;
    }

}

//----------------------------------------------------------------------------
// Inputs: pIpAddrTable is the IP address table
//         str is the Interface Ip address in dotted decimal format
// Output: If it returns TRUE, dwIndex contains the interface index number
//         and dwMask contains the corresponding subnet mask.
//----------------------------------------------------------------------------
bool InterfaceIpToIdxAndMask(PMIB_IPADDRTABLE pIpAddrTable, char str[], DWORD& dwIndex, DWORD& dwMask)
{
    DWORD dwIfIpAddr; // Interface Ip Address

    if (pIpAddrTable == NULL)
        return FALSE;

    dwIfIpAddr = inet_addr(str);
    if (dwIfIpAddr == INADDR_NONE)
        return FALSE;

    for (DWORD dwIdx = 0; dwIdx < pIpAddrTable->dwNumEntries; dwIdx++)
    {
        if (dwIfIpAddr == pIpAddrTable->table[dwIdx].dwAddr)
        {
            dwIndex = pIpAddrTable->table[dwIdx].dwIndex;
            dwMask = pIpAddrTable->table[dwIdx].dwMask;
            return TRUE;
        }
    }
    return FALSE;
}

//----------------------------------------------------------------------------
// If returned status is NO_ERROR, then pIpAddrTable points to a Ip Address
// table.
//----------------------------------------------------------------------------
DWORD MyGetIpAddrTable(PMIB_IPADDRTABLE& pIpAddrTable, BOOL fOrder)
{
    DWORD status = NO_ERROR;
    DWORD statusRetry = NO_ERROR;
    DWORD dwActualSize = 0;
    
    // query for buffer size needed
    status = GetIpAddrTable(pIpAddrTable, &dwActualSize, fOrder);

    if (status == NO_ERROR)
        return status;
    else if (status == ERROR_INSUFFICIENT_BUFFER)
    {
        // need more space

        pIpAddrTable = (PMIB_IPADDRTABLE) malloc(dwActualSize);
        assert(pIpAddrTable);
        
        statusRetry = GetIpAddrTable(pIpAddrTable, &dwActualSize, fOrder);
        return statusRetry;
    }
    else
    {
        return status;
    }
}



static DWORD LastAdapterUsed = 0; // Which adapter did we use last time?

DWORD GetAdapterToUse(IP_ADDRESS_STRING &adapterAddrStr, IP_ADDRESS_STRING &gwAddrStr)  // pass by reference
{
	DWORD numAdapters = 0;
	DWORD AdapterToUse = 0;			// Which adapter will we use for this session?

	PIP_ADAPTER_INFO pAdapt;
	// GetAdaptersInfo variables
    PIP_ADAPTER_INFO pAdapterInfo;
    DWORD AdapterInfoSize;

	PIP_ADDR_STRING pAddrStr;

	// For displaying error messages to the debug window
    DWORD Err;
    //TCHAR error_buffer[128];

    /*
       Enumerate all of the adapter specific information using the IP_ADAPTER_INFO structure.
       Note:  IP_ADAPTER_INFO contains a linked list of adapter entries.
        
	   This first statement finds the size of the structure, so that we know how much memory
	   to allocate.
    */


#ifdef DEBUGLSP
	//printf("MHLSP.DLL\\GetAdapterToUse(): GetAdapterToUse() started\n");
#endif

	//printf("Calling GetAdaptersInfo\n");
	AdapterInfoSize = 0;
    if ((Err = GetAdaptersInfo(NULL, &AdapterInfoSize)) != 0)
    {
        if (Err != ERROR_BUFFER_OVERFLOW)
        {
            //wsprintf(error_buffer, L"GetAdaptersInfo sizing failed with error %d\n", Err);
            //OutputDebugString(error_buffer);
			printf("MHLSP.DLL\\GetAdapterToUse(): GetAdaptersInfo sizing failed with error %d\n", Err);
            return NULL;
        }
    }

    // Allocate memory from sizing information
	//printf("Allocating Memory\n");
    if ((pAdapterInfo = (PIP_ADAPTER_INFO) GlobalAlloc(GPTR, AdapterInfoSize)) == NULL)
    {
        //wsprintf(error_buffer, L"Memory allocation error\n");
		//OutputDebugString(error_buffer);
		printf("MHLSP.DLL\\GetAdapterToUse(): Memory allocation error\n");
        return NULL;
    }

	//printf("Calling GetAdaptersInfo again\n");
    // Get actual adapter information
    if ((Err = GetAdaptersInfo(pAdapterInfo, &AdapterInfoSize)) != 0)
    {
        //wsprintf(error_buffer, L"GetAdaptersInfo failed with error %d\n", Err);
		//OutputDebugString(error_buffer);
		printf("MHLSP.DLL\\GetAdapterToUse(): GetAdaptersInfo failed with error %d\n", Err);
        return NULL;
    }

	//printf("Calling GetNumberOfInterfaces\n");
	if (GetNumberOfInterfaces(&numAdapters) != NO_ERROR)
	{
		//wsprintf(error_buffer, L"Oh dear, GetNumberOfInterfaces() failed!");
		//OutputDebugString(error_buffer);
		printf("MHLSP.DLL\\GetAdapterToUse(): Oh dear, GetNumberOfInterfaces() failed!");
	}
	
	numAdapters--; // GetNumberOfInterfaces() returns one more - the loopback device

#ifdef DEBUGLSP
	//printf("MHLSP.DLL\\GetAdapterToUse(): LastAdapterUsed: %d\n", LastAdapterUsed);
#endif


	// Interface Selection Algorithm here:

	AdapterToUse = LastAdapterUsed + 1;

	if (AdapterToUse > numAdapters)
		AdapterToUse = 1;


	// end of Interface Selection Algorithm

	pAdapt = pAdapterInfo;

#ifdef DEBUGLSP
	//printf("MHLSP.DLL\\GetAdapterToUse(): Finding ApdapterInfo\n\tnumAdapters = %d\n\tAdapterToUse = %d\n", numAdapters, AdapterToUse);
#endif

	for (DWORD i=1; i < AdapterToUse; i++)  // Find the AdapterInfo for AdapterToUse
		pAdapt = pAdapt->Next;
    
	LastAdapterUsed = AdapterToUse;

    pAddrStr = &(pAdapt->IpAddressList);
	adapterAddrStr = pAddrStr->IpAddress;

	gwAddrStr = pAdapt->GatewayList.IpAddress;



#ifdef DEBUGLSP
	printf("MHLSP.DLL\\GetAdapterToUse(): IP Address of adapter: %s\n", adapterAddrStr.String);
	printf("MHLSP.DLL\\GetAdapterToUse(): INT IP: %s\n\tGW  IP: %s\n", adapterAddrStr.String, gwAddrStr.String);
	printf("MHLSP.DLL\\GetAdapterToUse(): GetAdapterToUse() done\n");
#endif
 

	GlobalFree(pAdapterInfo);

	return 1;

} // End of GetAdapterToUse Function




BOOL UpdateRoutingTable(IP_ADDRESS_STRING adapterAddrStr, IP_ADDRESS_STRING gwAddrStr, sockaddr_in DestinationAddress)
{

	// Add a routing table entry to force us to use this adapter for the destination IP

	//TCHAR error_buffer[128];

    DWORD dwStatus;

    MIB_IPFORWARDROW routeEntry;            // Ip routing table row entry
    PMIB_IPADDRTABLE pIpAddrTable = NULL;   // Ip Addr Table
    DWORD dwIfIndex;                        // Interface index number  
    DWORD dwIfMask;                         // Interface Subnet Mask
    DWORD dwIfIpAddr;                       // Interface Ip Address
	char szDestIPAddr[16];
	LPSTR lpszIPAddr;

	// set routeEntry to all zeros
    memset(&routeEntry, 0, sizeof(MIB_IPFORWARDROW));

	lpszIPAddr = inet_ntoa(DestinationAddress.sin_addr);

	if (lpszIPAddr)
		strcpy((LPSTR) szDestIPAddr, lpszIPAddr);

#ifdef DEBUGLSP
	printf("MHLSP.DLL\\UpdateRoutingTable(): Destination Address: %s\n", szDestIPAddr);
#endif	


	// If this is the loopback address, don't update.
	if (strcmp(szDestIPAddr, "127.0.0.1") == 0)
		return 1;

	// If this is the default destination address, don't update.
	if (strcmp(szDestIPAddr, "0.0.0.0") == 0)
		return 1;

	// First, delete any previous entry for this destination
    DoDeleteIpForwardEntry(szDestIPAddr);

	// Add destination IP address to routing table entry
	routeEntry.dwForwardDest = inet_addr(szDestIPAddr);

	if (routeEntry.dwForwardDest == INADDR_NONE)
    {
        //wsprintf(error_buffer, L"\nUpdateRoutingTable: Bad Destination IP Address\n");
        //OutputDebugString(error_buffer);

		printf("\nMHLSP.DLL\\UpdateRoutingTable(): UpdateRoutingTable: Bad Destination IP Address\n");
		return 0;
    }

	// add default subnet mask of 255.255.255.255 to route to an individual host rather than a network
	routeEntry.dwForwardMask = inet_addr("255.255.255.255");
		
	// Set gateway as interface gw on network
	routeEntry.dwForwardNextHop = inet_addr(gwAddrStr.String);
 
    // Check if we have the given interface
    if ( (dwStatus = MyGetIpAddrTable(pIpAddrTable, 1)) != NO_ERROR)
    {
        //wsprintf(error_buffer, L"\nGetIpAddrTable returned 0x%x\n", dwStatus);
		//OutputDebugString(error_buffer);
		printf("\nMHLSP.DLL\\UpdateRoutingTable(): GetIpAddrTable returned 0x%x\n", dwStatus);
        if (pIpAddrTable)
            free(pIpAddrTable);
        return 0;
    }

    assert(pIpAddrTable);


	if ( InterfaceIpToIdxAndMask(pIpAddrTable, adapterAddrStr.String, dwIfIndex, dwIfMask) == FALSE)
    {
        //wsprintf(error_buffer,L"IpRoute: Bad Argument %s\n", szIPAddr);
		//OutputDebugString(error_buffer);
		printf("MHLSP.DLL\\UpdateRoutingTable(): IpRoute: Bad Argument %s\n", adapterAddrStr.String);
        return 0;
    }
    free(pIpAddrTable);

	dwIfIpAddr = inet_addr(adapterAddrStr.String);
	//dwIfMask = inet_addr(pAddrStr->IpMask.String);


    routeEntry.dwForwardIfIndex = dwIfIndex;
    routeEntry.dwForwardMetric1 = 1;

    // some default values
    routeEntry.dwForwardProto = MIB_IPPROTO_LOCAL;
    routeEntry.dwForwardMetric2 = -1;
    routeEntry.dwForwardMetric3 = -1;
    routeEntry.dwForwardMetric4 = -1;

	/*
		See if source and destination are on the same subnetwork and if so,
		do not change the routing table - use the default route for that network.

		if (src && srcMask) == (dest && destMask) then return
	*/

#ifdef DEBUGLSP
	//printf("MHLSP.DLL\\UpdateRoutingTable(): src IP [%x] & src mask [%x] = %x\n", dwIfIpAddr, dwIfMask, (dwIfIpAddr & dwIfMask));
	//printf("MHLSP.DLL\\UpdateRoutingTable(): dest IP [%x] & src mask [%x] = %x\n", routeEntry.dwForwardDest, dwIfMask, (routeEntry.dwForwardDest & dwIfMask));
#endif

	if ((dwIfIpAddr & dwIfMask) == (routeEntry.dwForwardDest & dwIfMask))
	{
#ifdef DEBUGLSP
		//printf("MHLSP.DLL\\UpdateRoutingTable(): Host and Destination on same subnet - using default subnet interface.\n");
#endif
		return 1;
	}
	else
#ifdef DEBUGLSP
		//printf("MHLSP.DLL\\UpdateRoutingTable(): Host and Destination *not* on same subnetwork - modifying routing table.\n");
#endif

	//wsprintf(error_buffer, L"Changing routing table to use interface index number: %d\n", routeEntry.dwForwardIfIndex);
	//OutputDebugString(error_buffer);
#ifdef DEBUGLSP
	printf("MHLSP.DLL\\UpdateRoutingTable(): Changing routing table to use interface index number: %d, IP: %s\n", routeEntry.dwForwardIfIndex, adapterAddrStr.String);
#endif

    dwStatus = SetIpForwardEntry(&routeEntry); 
    if (dwStatus != NO_ERROR)
    {
        //wsprintf(error_buffer, L"\nWARNING: SetIpForwardEntry couldn't add destination, dwStatus = %lu.\nCalling parameters:\n",
        //             dwStatus);
		//OutputDebugString(error_buffer);
		printf("\nMHLSP.DLL\\UpdateRoutingTable(): WARNING: SetIpForwardEntry couldn't add destination, dwStatus = %lu.\nCalling parameters:\n",
                     dwStatus);

		return 0;
    }
	else
		return 1; // success!

} // End of UpdateRoutingTable Function











/*
*
*			WSPAccept Function
*
*/

SOCKET WSPAPI WSPAccept (
    SOCKET s,                      
    struct sockaddr FAR * addr,  
    LPINT addrlen,                 
    LPCONDITIONPROC lpfnCondition,  
    DWORD dwCallbackData,          
    LPINT lpErrno)
{
    SOCKET NewProviderSocket;
    SOCKET NewSocket;
    SOCK_INFO *NewSocketContext;

    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return INVALID_SOCKET;

    NewProviderSocket = NextProcTable.lpWSPAccept(SocketContext->ProviderSocket, addr, addrlen,
                                                    lpfnCondition, dwCallbackData, lpErrno);
    
    
    if (NewProviderSocket != INVALID_SOCKET)
    {
        if ((NewSocketContext = (SOCK_INFO *) GlobalAlloc(GPTR, sizeof SOCK_INFO)) == NULL)
        {
            *lpErrno = WSAENOBUFS;
            return INVALID_SOCKET;
        }
        NewSocketContext->ProviderSocket = NewProviderSocket;
        NewSocketContext->bClosing  = FALSE;
        NewSocketContext->dwOutstandingAsync = 0;
        NewSocketContext->BytesRecv = 0;
        NewSocketContext->BytesSent = 0;

        if ((NewSocket = MainUpCallTable.lpWPUCreateSocketHandle(gChainId, (DWORD) NewSocketContext, lpErrno)) != INVALID_SOCKET)
            DuplicateAsyncSocket(SocketContext->ProviderSocket, NewProviderSocket, NewSocket);

        {
            TCHAR buffer[128];
            wsprintf(buffer, L"Creating socket %d\n", NewSocket);
            OutputDebugString(buffer);
        }

        return NewSocket;
    }

    return INVALID_SOCKET;

} // end of WSPAccept Function


/*
*
*			WSPAddressToString Function
*
*/

int WSPAPI WSPAddressToString(
    LPSOCKADDR lpsaAddress,            
    DWORD dwAddressLength,               
    LPWSAPROTOCOL_INFOW lpProtocolInfo,   
    LPWSTR lpszAddressString,            
    LPDWORD lpdwAddressStringLength,   
    LPINT lpErrno)
{
    return NextProcTable.lpWSPAddressToString(lpsaAddress, dwAddressLength,               
        &gBaseInfo[0], lpszAddressString, lpdwAddressStringLength, lpErrno);

}  // End of WSPAddressToString Function


/*
*
*			WSPAsyncSelect Function
*
*/

int WSPAPI WSPAsyncSelect (
    SOCKET s,              
    HWND hWnd,           
    unsigned int wMsg,     
    long lEvent,           
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;
    HWND hWorkerWindow;


    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    if ((hWorkerWindow = SetWorkerWindow(SocketContext->ProviderSocket, s, hWnd, wMsg)) == NULL)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPAsyncSelect(SocketContext->ProviderSocket, hWorkerWindow, WM_SOCKET, lEvent, lpErrno);

} // End of WSPAsyncSelect Function


/*
*
*			WSPBind Function
*
*/

int WSPAPI WSPBind(
    SOCKET s,                           
    const struct sockaddr FAR * name,
    int namelen,                        
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPBind(SocketContext->ProviderSocket, name, namelen, lpErrno);

} // End of WSPBind Function


/*
*
*			WSPCancelBlockingCall Function
*
*/

int WSPAPI WSPCancelBlockingCall(
    LPINT lpErrno)
{
    return NextProcTable.lpWSPCancelBlockingCall(lpErrno);

} // End of WSPCancelBlockingCall Function


/*
*
*			WSPCleanup Function
*
*/

int WSPAPI WSPCleanup (
    LPINT lpErrno  
    )
{
    int Ret;

    if (!gEntryCount)
    {
        *lpErrno = WSANOTINITIALISED;
        return SOCKET_ERROR;
    }

    Ret = NextProcTable.lpWSPCleanup(lpErrno);

    EnterCriticalSection(&gCriticalSection);

    gEntryCount--;

    if (gEntryCount == 0)
    {
        FreeLibrary(hProvider);
        hProvider = NULL;
    }

    LeaveCriticalSection(&gCriticalSection);

    return Ret;

} // End of WSPCleanup Function


/*
*
*			WSPCloseSocket Function
*
*/

int WSPAPI WSPCloseSocket (  
    SOCKET s,        
    LPINT lpErrno
)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

	// Delete routing table entry for this socket, if socket is named - ie not using sendto()
	if (SocketContext->bNamed)
	{
		char szDestIPAddr[16];
		LPSTR lpszIPAddr;

		//lpszIPAddr = inet_ntoa(DestinationAddress.sin_addr);

		lpszIPAddr = NULL;
		if (lpszIPAddr)
			strcpy((LPSTR) szDestIPAddr, lpszIPAddr);

		// If this is the loopback address or default route: don't delete.
		if (strcmp(szDestIPAddr, "127.0.0.1") != 0)
			if (strcmp(szDestIPAddr, "0.0.0.0") != 0)
			{
#ifdef DEBUGLSP
				//printf("MHLSP.DLL\\WSPCloseSocket(): Deleting destination [%s] from routing table.\n", szDestIPAddr);
#endif
				DoDeleteIpForwardEntry(szDestIPAddr);
			}
	}
	// end of delete routing table entry code

    if (SocketContext->dwOutstandingAsync != 0)
    {
        SocketContext->bClosing = TRUE;

        if (NextProcTable.lpWSPCloseSocket(SocketContext->ProviderSocket, lpErrno) == SOCKET_ERROR) 
            return SOCKET_ERROR;

        return 0;
    }

    if (NextProcTable.lpWSPCloseSocket(SocketContext->ProviderSocket, lpErrno) == SOCKET_ERROR) 
        return SOCKET_ERROR;

    RemoveSockInfo(SocketContext->ProviderSocket);

    if (MainUpCallTable.lpWPUCloseSocketHandle(s, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    {
        TCHAR buffer[128];
        wsprintf(buffer, L"Closing socket %d Bytes Sent [%lu] Bytes Recv [%lu]\n", s,
            SocketContext->BytesSent, SocketContext->BytesRecv);
        OutputDebugString(buffer);
    }

    GlobalFree(SocketContext);

    return 0;

} // End of WSPCloseSocket Function


/*
*
*			WSPConnect Function
*
*/

int WSPAPI WSPConnect (
    SOCKET s,                           
    const struct sockaddr FAR * name,
    int namelen,                        
    LPWSABUF lpCallerData,
    LPWSABUF lpCalleeData,              
    LPQOS lpSQOS,
    LPQOS lpGQOS,                       
    LPINT lpErrno
)
{
    SOCK_INFO *SocketContext;
    INT ret;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
    {
        return SOCKET_ERROR;
    }

    ret =  NextProcTable.lpWSPConnect(SocketContext->ProviderSocket, name, namelen, lpCallerData, lpCalleeData,
        lpSQOS, lpGQOS, lpErrno);

    return ret;

} // End of WSPConnect Function


/*
*
*			WSPDuplicate Function
*
*/

int WSPAPI WSPDuplicateSocket(
    SOCKET s,                             
    DWORD dwProcessId,                      
    LPWSAPROTOCOL_INFOW lpProtocolInfo,   
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPDuplicateSocket(SocketContext->ProviderSocket,                             
        dwProcessId, lpProtocolInfo, lpErrno);

} // End of WSPDuplicate Function


/*
*
*			WSPEnumNetworkEvents Function
*
*/

int WSPAPI WSPEnumNetworkEvents(  
    SOCKET s,                             
    WSAEVENT hEventObject,                  
    LPWSANETWORKEVENTS lpNetworkEvents,   
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPEnumNetworkEvents(SocketContext->ProviderSocket,                             
        hEventObject, lpNetworkEvents, lpErrno);

} // End of WSPEnumNetworkEvents Function


/*
*
*			WSPEventSelect Function
*
*/

int WSPAPI WSPEventSelect(
    SOCKET s,                
    WSAEVENT hEventObject,   
    long lNetworkEvents,     
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;
    
    return NextProcTable.lpWSPEventSelect(SocketContext->ProviderSocket, hEventObject,
        lNetworkEvents, lpErrno);

} // End of WSPEventSelect Function


/*
*
*			WSPGetOverlappedResult Function
*
*/

BOOL WSPAPI WSPGetOverlappedResult (
    SOCKET s,
    LPWSAOVERLAPPED lpOverlapped,
    LPDWORD lpcbTransfer,
    BOOL fWait,
    LPDWORD lpdwFlags,
    LPINT lpErrno)
{
    DWORD Ret;

    if (lpOverlapped->Internal!=WSS_OPERATION_IN_PROGRESS) 
    {
        *lpcbTransfer = lpOverlapped->InternalHigh;
        *lpdwFlags = lpOverlapped->Offset;
        *lpErrno = lpOverlapped->OffsetHigh;

        return(lpOverlapped->OffsetHigh == 0 ? TRUE : FALSE);
    }
    else
        if (fWait) 
        {
            Ret = WaitForSingleObject(lpOverlapped->hEvent, INFINITE);
            if ((Ret == WAIT_OBJECT_0)
                && (lpOverlapped->Internal != WSS_OPERATION_IN_PROGRESS))
            {
                *lpcbTransfer = lpOverlapped->InternalHigh;
                *lpdwFlags = lpOverlapped->Offset;
                *lpErrno = lpOverlapped->OffsetHigh;
                    
                return(lpOverlapped->OffsetHigh == 0 ? TRUE : FALSE);
            }
            else 
                *lpErrno = WSASYSCALLFAILURE;
        }
        else 
            *lpErrno = WSA_IO_INCOMPLETE;

    return FALSE;

} // End of WSPGetOverlappedResult Function


/*
*
*			WSPGetPeerName Function
*
*/

int WSPAPI WSPGetPeerName(  
    SOCKET s,                     
    struct sockaddr FAR * name,     
    LPINT namelen,                
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPGetPeerName(SocketContext->ProviderSocket, name,
        namelen, lpErrno);
}


/*
*
*			WSPGetSockName Function
*
*/

int WSPAPI WSPGetSockName(
    SOCKET s,                     
    struct sockaddr FAR * name,
    LPINT namelen,                
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPGetSockName(SocketContext->ProviderSocket, name,
        namelen, lpErrno);

} // End of WSPGetPeerName Function


/*
*
*			WSPGetSockOpt Function
*
*/

int WSPAPI WSPGetSockOpt(
    SOCKET s,              
    int level,           
    int optname,           
    char FAR * optval,     
    LPINT optlen,        
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPGetSockOpt(SocketContext->ProviderSocket, level,           
        optname, optval, optlen, lpErrno);

} // End of WSPGetSockOpt Function


/*
*
*			WSPGetQOSByName Function
*
*/

BOOL WSPAPI WSPGetQOSByName(
    SOCKET s,               
    LPWSABUF lpQOSName,   
    LPQOS lpQOS,            
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPGetQOSByName(SocketContext->ProviderSocket, lpQOSName,
        lpQOS, lpErrno);

} // End of WSPGetQOSByName Function


/*
*
*			WSPIoctl Function
*
*/

int WSPAPI WSPIoctl(
    SOCKET s,
    DWORD dwIoControlCode,
    LPVOID lpvInBuffer,
    DWORD cbInBuffer,
    LPVOID lpvOutBuffer,
    DWORD cbOutBuffer,
    LPDWORD lpcbBytesReturned,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
    LPWSATHREADID lpThreadId,
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;
    int Ret;
    LPWSAOVERLAPPED ProviderOverlapped;

    GUID AcceptExGuid = WSAID_ACCEPTEX;
    GUID TransmitFileGuid = WSAID_TRANSMITFILE;
    GUID GetAcceptExSockAddrsGuid = WSAID_GETACCEPTEXSOCKADDRS;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext,
        lpErrno) == SOCKET_ERROR)
    {
//        MessageBox(NULL, L"wham", L"wham", MB_OK);
        return SOCKET_ERROR;
    }

    if (dwIoControlCode == SIO_GET_EXTENSION_FUNCTION_POINTER)
    {
        if (memcmp (lpvInBuffer, &TransmitFileGuid, sizeof (GUID)) == 0)
        {
            *((LPFN_TRANSMITFILE *)lpvOutBuffer) = ExtTransmitFile;
            *lpErrno = 0;
            return 0;
        }
        else
            if (memcmp(lpvInBuffer, &AcceptExGuid, sizeof(GUID)) == 0)
            {
                *((LPFN_ACCEPTEX *)lpvOutBuffer) = ExtAcceptEx;
                *lpErrno = 0;
                return 0;
            }
            else 
                if (memcmp (lpvInBuffer, &GetAcceptExSockAddrsGuid, sizeof (GUID)) == 0)
                {
                    // No socket handle translation needed, let the call pass through below
                }
                else 
                {
                    *lpErrno = WSAEOPNOTSUPP;
                    return SOCKET_ERROR;
                }

    }


    // Check for overlapped I/O
    
    if (lpOverlapped)
    {
        ProviderOverlapped = GetOverlappedStructure(s, SocketContext->ProviderSocket, lpOverlapped, lpCompletionRoutine,
            lpThreadId, NULL);

        Ret = NextProcTable.lpWSPIoctl(SocketContext->ProviderSocket, dwIoControlCode, lpvInBuffer,
            cbInBuffer, lpvOutBuffer, cbOutBuffer, lpcbBytesReturned, ProviderOverlapped, NULL, NULL, lpErrno);
    }
    else
    {
        Ret = NextProcTable.lpWSPIoctl(SocketContext->ProviderSocket, dwIoControlCode, lpvInBuffer,
            cbInBuffer, lpvOutBuffer, cbOutBuffer, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
    }

    return Ret;

} // End of WSPIoctl Function


/*
*
*			WSPJoinLeaf Function
*
*/

SOCKET WSPAPI WSPJoinLeaf(
    SOCKET s,                           
    const struct sockaddr FAR * name,     
    int namelen,                        
    LPWSABUF lpCallerData,                
    LPWSABUF lpCalleeData,              
    LPQOS lpSQOS,                         
    LPQOS lpGQOS,                       
    DWORD dwFlags,                        
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;
    SOCKET NextProviderSocket;
    SOCKET NewSocket;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    NextProviderSocket = NextProcTable.lpWSPJoinLeaf(SocketContext->ProviderSocket,                           
        name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS, dwFlags,                        
        lpErrno);
        
    if (NextProviderSocket != INVALID_SOCKET)
    {
        if ((SocketContext = (SOCK_INFO *) GlobalAlloc(GPTR, sizeof SOCK_INFO)) == NULL)
        {
            *lpErrno = WSAENOBUFS;
            return INVALID_SOCKET;
        }
        SocketContext->ProviderSocket = NextProviderSocket;
        SocketContext->bClosing  = FALSE;
        SocketContext->dwOutstandingAsync = 0;
        SocketContext->BytesRecv = 0;
        SocketContext->BytesSent = 0;

        NewSocket = MainUpCallTable.lpWPUCreateSocketHandle(gChainId, (DWORD) SocketContext, lpErrno);

        {
            TCHAR buffer[128];
            wsprintf(buffer, L"Creating socket %d\n", NewSocket);
            OutputDebugString(buffer);
        }

        return NewSocket;
    }

    return INVALID_SOCKET;

} // End of WSPJoinLeaf Function


/*
*
*			WSPListen Function
*
*/

int WSPAPI WSPListen(
    SOCKET s,        
    int backlog,     
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPListen (SocketContext->ProviderSocket, backlog, lpErrno);

} // End of WSPListen Function


/*
*
*			WSPRecv Function
*
*/

int WSPAPI WSPRecv(
    SOCKET s,                                                 
    LPWSABUF lpBuffers,                                       
    DWORD dwBufferCount,                                      
    LPDWORD lpNumberOfBytesRecvd,                             
    LPDWORD lpFlags,                                          
    LPWSAOVERLAPPED lpOverlapped,                             
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,   
    LPWSATHREADID lpThreadId,                                 
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;
    int Ret;
    LPWSAOVERLAPPED ProviderOverlapped;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;


    // Check for overlapped I/O
    
    if (lpOverlapped)
    {
        ProviderOverlapped = GetOverlappedStructure(s, SocketContext->ProviderSocket, lpOverlapped, lpCompletionRoutine,
            lpThreadId, &SocketContext->BytesRecv);

        Ret = NextProcTable.lpWSPRecv(SocketContext->ProviderSocket, lpBuffers, dwBufferCount,
            lpNumberOfBytesRecvd, lpFlags, ProviderOverlapped, NULL, NULL, lpErrno);

    }
    else
    {
        // Process ordinary blocking call

        Ret = NextProcTable.lpWSPRecv(SocketContext->ProviderSocket, lpBuffers, dwBufferCount,
            lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine, lpThreadId,
            lpErrno);

		
		/*
		* This is where the number of bytes received is calculated
		*/

        if (Ret != SOCKET_ERROR)
        {
            SocketContext->BytesRecv += *lpNumberOfBytesRecvd;
        }
    }

    return Ret;

} // End of WSPRecv Function


/*
*
*			WSPRecvDisconnect Function
*
*/

int WSPAPI WSPRecvDisconnect(
    SOCKET s,                           
    LPWSABUF lpInboundDisconnectData,     
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPRecvDisconnect(SocketContext->ProviderSocket,                           
        lpInboundDisconnectData, lpErrno);

} // End of WSPRecvDisconnect Function


/*
*
*			WSPRecvFrom Function
*
*/

int WSPAPI WSPRecvFrom(
    SOCKET s,
    LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd,
    LPDWORD lpFlags,
    struct sockaddr FAR * lpFrom,
    LPINT lpFromlen,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
    LPWSATHREADID lpThreadId,
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;
    int Ret;
    LPWSAOVERLAPPED ProviderOverlapped;

	WSABUF myBuffer;
	LPWSABUF lpmyBuffer;
	char *myData;
	DWORD myDataLen;

	DWORD userDataLen;

	SessionLayerHeader stHeader;
	char *ptr;
	DWORD lpBuffersLen;

	SessionLayerBindingUpdate updatePDU;
	bool bindingUpdate = false;


    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

#ifdef DEBUGLSP
	printf("MHLSP.DLL\\WSPRecvFrom(): SocketContext->HostSID = %d\n", SocketContext->HostSID);
#endif


    // Check for overlapped I/O
    
    if (lpOverlapped)
    {
        ProviderOverlapped = GetOverlappedStructure(s, SocketContext->ProviderSocket, lpOverlapped, lpCompletionRoutine,
            lpThreadId, &SocketContext->BytesRecv);

        if (ProviderOverlapped == NULL)
        {
            OutputDebugString(L"WSPRecvFrom got a NULL overlapp structure\n");
        }

        Ret = NextProcTable.lpWSPRecvFrom(SocketContext->ProviderSocket, lpBuffers, dwBufferCount,
            lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, ProviderOverlapped, NULL, NULL, lpErrno);

    }
    else
    {

		lpBuffersLen = lpBuffers->len; 
#ifdef DEBUGLSP
		//printf("MHLSP.DLL\\WSPRecvFrom(): Calling NextProcTable.lpWSPRecvFrom()\n");
		//printf("MHLSP.DLL\\WSPRecvFrom(): lpBuffers.len = %d\n", lpBuffersLen);
#endif
		
		userDataLen = lpBuffers->len;
		myDataLen = userDataLen + sizeof(stHeader);
		myData = (char *) GlobalAlloc(GMEM_FIXED, myDataLen);

		myBuffer.buf = myData;
		myBuffer.len = myDataLen;

		lpmyBuffer = (LPWSABUF) &myBuffer;

		Ret = NextProcTable.lpWSPRecvFrom(SocketContext->ProviderSocket, lpmyBuffer, dwBufferCount,
            lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);

#ifdef DEBUGLSP
		//printf("MHLSP.DLL\\WSPRecvFrom(): lpmyBuffer.len = %d\n", lpmyBuffer->len);
		//printf("MHLSP.DLL\\WSPRecvFrom(): dwBufferCount = %d\n", dwBufferCount);
		//printf("MHLSP.DLL\\WSPRecvFrom(): NextProcTable.lpWSPRecvFrom() returned %d\n", Ret);
#endif

		if (Ret == -1)
			return Ret;

		// remove header from data
#ifdef DEBUGLSP
		//printf("MHLSP.DLL\\WSPRecvFrom(): myBuffer.len = %d\n", myBuffer.len);
#endif

		ptr = myBuffer.buf;

		memcpy(&stHeader, ptr, sizeof(stHeader));
		ptr += sizeof(stHeader);

#ifdef DEBUGLSP
		printf("MHLSP.DLL\\WSPRecvFrom(): stHeader.fromSID = %d\n", stHeader.fromSID);
		printf("MHLSP.DLL\\WSPRecvFrom(): stHeader.toSID = %d\n", stHeader.toSID);
		printf("MHLSP.DLL\\WSPRecvFrom(): stHeader.length = %d\n", stHeader.length);
		printf("MHLSP.DLL\\WSPRecvFrom(): stHeader.type = %d\n", stHeader.type);
#endif

		// Update Socket Context
		if (SocketContext->PeerSID == -1)
		{
			SocketContext->PeerSID = stHeader.fromSID;
		}

		if ((DWORD) stHeader.length > (myBuffer.len - sizeof(stHeader)))
		{
			printf("MHLSP.DLL\\WSPRecvFrom(): length contained in header > actual length\n");
			printf("MHLSP.DLL\\WSPRecvFrom(): stHeader.length = %d\n", stHeader.length);
			printf("MHLSP.DLL\\WSPRecvFrom(): myBuffer.len - sizeof(stHeader) = %d\n", myBuffer.len - sizeof(stHeader));
			return -1;
		}

		if (stHeader.type == MHLSP_UPDATE_PDU)
		{
			// Binding Update received
			bindingUpdate = true;

			memcpy(&updatePDU, ptr, sizeof(SessionLayerBindingUpdate));
			ptr += sizeof(SessionLayerBindingUpdate);

		}
		//else
		{

			userDataLen = stHeader.length;

			memcpy(lpBuffers->buf, ptr, userDataLen);
			lpBuffers->len = userDataLen;
		}


#ifdef DEBUGLSP
		//printf("MHLSP.DLL\\WSPRecvFrom(): userDataLen = %d\n", userDataLen);
		//printf("MHLSP.DLL\\WSPRecvFrom(): lpBuffers->buf = [%s]\n", lpBuffers->buf);
		//printf("MHLSP.DLL\\WSPRecvFrom(): lpBuffers->len = %d\n", lpBuffers->len);
		//printf("MHLSP.DLL\\WSPRecvFrom(): lpNumerOfBytesRecvd = %d\n", *lpNumberOfBytesRecvd);
#endif


		/*
		* This is where the number of bytes received is calculated
		*/

        if (Ret != SOCKET_ERROR)
        {
#ifdef DEBUGLSP
			//printf("MHLSP.DLL\\WSPRecvFrom(): sizeof(stHeader) = %d; *lpNumberOfBytesRecvd = %d\n", sizeof(stHeader), *lpNumberOfBytesRecvd);
#endif
            SocketContext->BytesRecv += *lpNumberOfBytesRecvd;
			*lpNumberOfBytesRecvd -= sizeof(stHeader);
			if (bindingUpdate)
				*lpNumberOfBytesRecvd -= sizeof(SessionLayerBindingUpdate);
        }
    }


	GlobalFree(myData);

    return Ret;

} // End of WSPRecvFrom Function


/*
*
*			WSPSelect Function
*
*/

int WSPAPI WSPSelect(
    int nfds,
    fd_set FAR * readfds,
    fd_set FAR * writefds,
    fd_set FAR * exceptfds,
    const struct timeval FAR * timeout,
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;
    u_int i;
    u_int count;
    int Ret;
    int HandleCount;

    // Convert handles
    struct
    {
        SOCKET ClientSocket;
        SOCKET ProvSocket;

    } Read[FD_SETSIZE], Write[FD_SETSIZE], Except[FD_SETSIZE];

    fd_set ReadFds, WriteFds, ExceptFds;

    if (readfds)
    {
        FD_ZERO(&ReadFds);

        for (i = 0; i < readfds->fd_count; i++)
        {
            if (MainUpCallTable.lpWPUQuerySocketHandleContext(
                (Read[i].ClientSocket = readfds->fd_array[i]), 
                (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
                return SOCKET_ERROR;
            FD_SET((Read[i].ProvSocket = SocketContext->ProviderSocket), &ReadFds);
        }
    }

    if (writefds)
    {
        FD_ZERO(&WriteFds);

        for (i = 0; i < writefds->fd_count; i++)
        {
            if (MainUpCallTable.lpWPUQuerySocketHandleContext(
                (Write[i].ClientSocket = writefds->fd_array[i]), 
                (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
                return SOCKET_ERROR;
            FD_SET((Write[i].ProvSocket = SocketContext->ProviderSocket), &WriteFds);
        }
    }

    if (exceptfds)
    {
        FD_ZERO(&ExceptFds);

        for (i = 0; i < exceptfds->fd_count; i++)
        {
            if (MainUpCallTable.lpWPUQuerySocketHandleContext(
                (Except[i].ClientSocket = exceptfds->fd_array[i]), 
                (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
                return SOCKET_ERROR;
            FD_SET((Except[i].ProvSocket = SocketContext->ProviderSocket), &ExceptFds);
        }
    }

    Ret = NextProcTable.lpWSPSelect(nfds, 
        (readfds ? &ReadFds : NULL), (writefds ? &WriteFds : NULL), 
        (exceptfds ? &ExceptFds : NULL), timeout, lpErrno);

    if (Ret != SOCKET_ERROR)
    {
        HandleCount = Ret;

        if (readfds)
        {
            count = readfds->fd_count;
            FD_ZERO(readfds);

            for(i = 0; (i < count) && HandleCount; i++)
            {
                if (MainUpCallTable.lpWPUFDIsSet(Read[i].ProvSocket, &ReadFds))
                {
                    FD_SET(Read[i].ClientSocket, readfds);
                    HandleCount--;
                }
            }
        }

        if (writefds)
        {
            count = writefds->fd_count;
            FD_ZERO(writefds);

            for(i = 0; (i < count) && HandleCount; i++)
            {
                if (MainUpCallTable.lpWPUFDIsSet(Write[i].ProvSocket, &WriteFds))
                {
                    FD_SET(Write[i].ClientSocket, writefds);
                    HandleCount--;
                }
            }
        }

        if (exceptfds)
        {
            count = exceptfds->fd_count;
            FD_ZERO(exceptfds);

            for(i = 0; (i < count) && HandleCount; i++)
            {
                if (MainUpCallTable.lpWPUFDIsSet(Except[i].ProvSocket, &ExceptFds))
                {
                    FD_SET(Except[i].ClientSocket, exceptfds);
                    HandleCount--;
                }
            }
        }
    }

    return Ret;

} // End of WSPSelect Function


/*
*
*			WSPSend Function
*
*/

int WSPAPI WSPSend (
     SOCKET s,                                                 
    LPWSABUF lpBuffers,                                       
    DWORD dwBufferCount,                                      
    LPDWORD lpNumberOfBytesSent,                              
    DWORD dwFlags,                                            
    LPWSAOVERLAPPED lpOverlapped,                             
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,   
    LPWSATHREADID lpThreadId,                                 
    LPINT lpErrno                                             
    )
{
    INT Ret;
    SOCK_INFO *SocketContext;
    LPWSAOVERLAPPED ProviderOverlapped;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    // Check for overlapped I/O
    
    if (lpOverlapped)
    {
        ProviderOverlapped = GetOverlappedStructure(s, SocketContext->ProviderSocket, lpOverlapped, lpCompletionRoutine,
            lpThreadId, &SocketContext->BytesSent);

        Ret = NextProcTable.lpWSPSend(SocketContext->ProviderSocket, lpBuffers, dwBufferCount,
            lpNumberOfBytesSent, dwFlags, ProviderOverlapped, NULL, NULL, lpErrno);

    }
    else
    {
        Ret = NextProcTable.lpWSPSend(SocketContext->ProviderSocket, lpBuffers, dwBufferCount,
            lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);


		/*
		* This is where the number of bytes sent is calculated
		*/

        if (Ret != SOCKET_ERROR)
        {
            SocketContext->BytesSent += *lpNumberOfBytesSent;
        }
    }


    return Ret;

} // End of WSPSend Function


/*
*
*			WSPSendDisconnect Function
*
*/

int WSPAPI WSPSendDisconnect(
    SOCKET s,                            
    LPWSABUF lpOutboundDisconnectData,
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPSendDisconnect(SocketContext->ProviderSocket,
        lpOutboundDisconnectData, lpErrno);

} // End of WSPSendDisconnect Function


/*
*
*			WSPSendTo Function
*
*/

int WSPAPI WSPSendTo(
    SOCKET s,
    LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent,
    DWORD dwFlags,
    const struct sockaddr FAR * lpTo,
    int iTolen,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
    LPWSATHREADID lpThreadId,
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;
    int Ret;
    LPWSAOVERLAPPED ProviderOverlapped;
	sockaddr_in recipient;
	sockaddr_in *pRecpt;

	WSABUF userBuffer;

	WSABUF myBuffer;
	char *myData;
	char *dataptr;
	DWORD myDataLen;

	bool bindingUpdate = true;
	SessionLayerBindingUpdate updatePDU;

    // Check for overlapped I/O
    
    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

#ifdef DEBUGLSP
	printf("MHLSP.DLL\\WSPSendTo(): SocketContext->HostSID = %d\n", SocketContext->HostSID);
#endif

    if (lpOverlapped)
    {
        ProviderOverlapped = GetOverlappedStructure(s, SocketContext->ProviderSocket, lpOverlapped, lpCompletionRoutine,
            lpThreadId, &SocketContext->BytesSent);

        Ret = NextProcTable.lpWSPSendTo(SocketContext->ProviderSocket, lpBuffers, dwBufferCount,
            lpNumberOfBytesSent, dwFlags, lpTo, iTolen, ProviderOverlapped, NULL, NULL, lpErrno);

    }
    else
    {

		// Send data normally - update routing table to force datagram out of appropriate adapter

		// note that we have to get a pointer to recipient - pRecpt and then cast lpTo to a sockaddr_in pointer
		// which we then dereference: Visual C++ will not allow us to simply do "recipient = (sockaddr_in) *lpTo;"
		pRecpt = (sockaddr_in *) lpTo;
		recipient = *pRecpt;
		UpdateRoutingTable(SocketContext->AdapterAddrString, SocketContext->GatewayAddrString, recipient);

		// add header to data
		userBuffer = *lpBuffers;

#ifdef DEBUGLSP
		//printf("MHLSP.DLL\\WSPSendTo(): userBuffer.len = %d\n", userBuffer.len);
		//printf("MHLSP.DLL\\WSPSendTo(): dwBufferCount = %d\n", dwBufferCount);
#endif
		
		SessionLayerHeader stHeader;
		stHeader.fromSID = SocketContext->HostSID;
		stHeader.toSID = SocketContext->PeerSID;
		stHeader.length = (short) userBuffer.len;
		if (bindingUpdate)
			stHeader.type = MHLSP_UPDATE_PDU;
		else
			stHeader.type = MHLSP_DATA_PDU;

#ifdef DEBUGLSP
		printf("MHLSP.DLL\\WSPSendTo(): stHeader.fromSID = %d\n", stHeader.fromSID);
		printf("MHLSP.DLL\\WSPSendTo(): stHeader.toSID = %d\n", stHeader.toSID);
		printf("MHLSP.DLL\\WSPSendTo(): stHeader.length = %d\n", stHeader.length);
		printf("MHLSP.DLL\\WSPSendTo(): stHeader.type = %d\n", stHeader.type);
#endif


		myDataLen = userBuffer.len + sizeof(stHeader);
		myData = (char *) GlobalAlloc(GMEM_FIXED, myDataLen);

		dataptr = myData;

		memcpy(dataptr, &stHeader, sizeof(stHeader));
		dataptr += sizeof(stHeader);

		if (bindingUpdate)
		{
			// Get new adapter to use
			IP_ADDRESS_STRING newAdapterAddrStr;
			IP_ADDRESS_STRING newGWAddrStr;

			GetAdapterToUse(newAdapterAddrStr, newGWAddrStr);

			// copy binding update data into PDU
			updatePDU.OldIP = SocketContext->AdapterAddrString;
			updatePDU.NewIP = newAdapterAddrStr;

			memcpy(dataptr, &updatePDU, sizeof(updatePDU));
			dataptr += sizeof(updatePDU);

			// update socket context info
			SocketContext->AdapterAddrString = newAdapterAddrStr;
			SocketContext->GatewayAddrString = newGWAddrStr;
		}
		//else
		{
			// copy user data into PDU

			memcpy(dataptr, userBuffer.buf, userBuffer.len);

			myBuffer.buf = myData;
			myBuffer.len = myDataLen;
		}

#ifdef DEBUGLSP
		//printf("MHLSP.DLL\\WSPSendTo(): sizeof(stHeader) = %d\n", sizeof(stHeader));
		//printf("MHLSP.DLL\\WSPSendTo(): userBuffer.len = %d\n", userBuffer.len);
		//printf("MHLSP.DLL\\WSPSendTo(): userBuffer.buf = [%s]\n", userBuffer.buf);
		//printf("MHLSP.DLL\\WSPSendTo(): myBuffer.len = %d\n", myBuffer.len);
#endif

        Ret = NextProcTable.lpWSPSendTo(SocketContext->ProviderSocket, (LPWSABUF) &myBuffer, dwBufferCount,
            lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);


		/*/ decode the header - used to debug code to check that header is ok
		
		SessionLayerHeader stNewHeader;
		char *nhptr;
		nhptr = myData;
		memcpy(&stNewHeader, nhptr, sizeof(stNewHeader));

#ifdef DEBUGLSP
		printf("MHLSP.DLL\\WSPSendTo(): stNewHeader.fromSID = %d\n", stNewHeader.fromSID);
		printf("MHLSP.DLL\\WSPSendTo(): stNewHeader.toSID = %d\n", stNewHeader.toSID);
		printf("MHLSP.DLL\\WSPSendTo(): stNewHeader.length = %d\n", stNewHeader.length);
		printf("MHLSP.DLL\\WSPSendTo(): stNewHeader.type = %d\n", stNewHeader.type);
#endif

		/*/

		/*
		* This is where the number of bytes sent is calculated
		*/

        if (Ret != SOCKET_ERROR)
        {
#ifdef DEBUGLSP
			printf("MHLSP.DLL\\WSPSendTo(): sizeof(stHeader) = %d; *lpNumberOfBytesSent = %d\n", sizeof(stHeader), *lpNumberOfBytesSent);
#endif
           SocketContext->BytesSent += *lpNumberOfBytesSent;
			*lpNumberOfBytesSent -= sizeof(stHeader);
			if (bindingUpdate)
				*lpNumberOfBytesSent -= sizeof(SessionLayerBindingUpdate);
	    }
		else
		{
			printf("MHLSP.DLL\\WSPSendTo(): NextProcTable.WSPSendTo failed, returncode [%d]\n", Ret);
		}


		// Delete routing table entry for this destination
		char szDestIPAddr[16];
		LPSTR lpszIPAddr;

		lpszIPAddr = inet_ntoa(recipient.sin_addr);

		if (lpszIPAddr)
			strcpy((LPSTR) szDestIPAddr, lpszIPAddr);

		// If this is the loopback address or default route: don't delete.
		if (strcmp(szDestIPAddr, "127.0.0.1") != 0)
			if (strcmp(szDestIPAddr, "0.0.0.0") != 0)
			{
#ifdef DEBUGLSP
				printf("MHLSP.DLL\\WSPSendTo(): Deleting destination [%s] from routing table.\n", szDestIPAddr);
#endif
				DoDeleteIpForwardEntry(szDestIPAddr);
			}
		

		// free buffer memory
		GlobalFree(myData);

    }
	
#ifdef DEBUGLSP
	printf("MHLSP.DLL\\WSPSendTo(): returning [%d]\n", Ret);
#endif
   return Ret;

} // End of WSPSendTo Function


/*
*
*			WSPSetSockOpt Function
*
*/

int WSPAPI WSPSetSockOpt(
    SOCKET s,
    int level,                 
    int optname,                 
    const char FAR * optval,   
    int optlen,                  
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPSetSockOpt(SocketContext->ProviderSocket, level,                 
        optname, optval, optlen, lpErrno);

} // End of WSPSetSockOpt Function


/*
*
*			WSPShutdown Function
*
*/

int WSPAPI WSPShutdown (
    SOCKET s,        
    int how,         
    LPINT lpErrno)
{
    SOCK_INFO *SocketContext;

    if (MainUpCallTable.lpWPUQuerySocketHandleContext(s, (LPDWORD) &SocketContext, lpErrno) == SOCKET_ERROR)
        return SOCKET_ERROR;

    return NextProcTable.lpWSPShutdown(SocketContext->ProviderSocket, how, lpErrno);

} // End of WSPShutdown Function


/*
*
*			WSPStringToAddress Function
*
*/

int WSPAPI WSPStringToAddress(
    LPWSTR AddressString,                 
    INT AddressFamily,                      
    LPWSAPROTOCOL_INFOW lpProtocolInfo,   
    LPSOCKADDR lpAddress,                   
    LPINT lpAddressLength,                
    LPINT lpErrno)
{
    return NextProcTable.lpWSPStringToAddress (AddressString, AddressFamily,
        &gBaseInfo[0], lpAddress, lpAddressLength, lpErrno);

} // End of WSPStringToAddress Function


/*
*
*			WSPSocket Function
*
*/

SOCKET WSPAPI WSPSocket(
    int af,                               
    int type,
    int protocol,                         
    LPWSAPROTOCOL_INFOW lpProtocolInfo,
    GROUP g,                              
    DWORD dwFlags,
    LPINT lpErrno                         
    )
{
    SOCKET NextProviderSocket;
    SOCKET NewSocket;
    SOCK_INFO *SocketContext;
    LPWSAPROTOCOL_INFOW pInfo=NULL;
    INT        iProtocol, iSockType, i;
	IP_ADDRESS_STRING adapterAddrStr, gwAddrStr;

	// Determine which adapter to use for this session
	if (GetAdapterToUse(adapterAddrStr, gwAddrStr) == NULL)
		return INVALID_SOCKET;

#ifdef DEBUGLSP
    //printf("MHLSP.DLL\\WSPSocket(): Adapter to use: %s\n", adapterAddrStr.String);
	//printf("MHLSP.DLL\\WSPSocket(): Gateway to use: %s\n", gwAddrStr.String);
#endif

	// SOCKET creation stage
    iProtocol = (!lpProtocolInfo ? lpProtocolInfo->iProtocol   : protocol);
    iSockType = (!lpProtocolInfo ? lpProtocolInfo->iSocketType : type);
    for(i=0; i < gLayerCount ;i++)
    {
        if ((gBaseInfo[i].iSocketType == iSockType) && 
            (gBaseInfo[i].iProtocol   == iProtocol))
        {
            pInfo = &gBaseInfo[i];
            break;
        }
    }

    NextProviderSocket = NextProcTable.lpWSPSocket(af, type, protocol, (pInfo ? pInfo : lpProtocolInfo),
                                g, dwFlags, lpErrno);

    if (NextProviderSocket != INVALID_SOCKET)
    {
        if ((SocketContext = (SOCK_INFO *) GlobalAlloc(GPTR, sizeof SOCK_INFO)) == NULL)
        {
            *lpErrno = WSAENOBUFS;
            return INVALID_SOCKET;
        }
        SocketContext->ProviderSocket = NextProviderSocket;
        SocketContext->bClosing  = FALSE;
        SocketContext->dwOutstandingAsync = 0;
        SocketContext->BytesRecv = 0;
        SocketContext->BytesSent = 0;
		// for now lets say that the socket is not named
		SocketContext->bNamed = FALSE;
		// and store adapter to use details
		SocketContext->AdapterAddrString = adapterAddrStr;
		SocketContext->GatewayAddrString = gwAddrStr;
		// and session details
		SocketContext->HostSID = NextProviderSocket;		// the reference to the next socket down
		SocketContext->PeerSID = -1;						// undefined at this point

#ifdef DEBUGLSP
		printf("MHLSP.DLL\\WSPSocket(): NewSocket (HostSID) = %d\n", NextProviderSocket);
#endif

		// register new socket with next layer up
		NewSocket = MainUpCallTable.lpWPUCreateSocketHandle(gChainId, (DWORD) SocketContext, lpErrno);

        {
            TCHAR buffer[128];
            wsprintf(buffer, L"Creating socket %d\n", NewSocket);
            OutputDebugString(buffer);
        }

        return NewSocket;
    }

    return INVALID_SOCKET;

} // End of WSPSocket Function



/*
*
*			WSPStartup Function
*
*/

int WSPAPI WSPStartup(
    WORD wVersion,
    LPWSPDATA lpWSPData,
    LPWSAPROTOCOL_INFOW lpProtocolInfo,
    WSPUPCALLTABLE UpCallTable,
    LPWSPPROC_TABLE lpProcTable)
{

    INT      ReturnCode = 0;
    WCHAR    ProviderPath[MAX_PATH];
    INT      ProviderPathLen = MAX_PATH;
    WCHAR    LibraryPath[MAX_PATH];
    INT      i, j, x, y, z;
    INT      TotalProtocols, idx;
    INT      Error;
    DWORD    NextProviderCatId;
    UINT     iBaseId;

    LPWSAPROTOCOL_INFOW ProtocolInfo;
    LPWSAPROTOCOL_INFOW ProtoInfo = lpProtocolInfo;
    LPWSPSTARTUP    WSPStartupFunc = NULL;

    EnterCriticalSection(&gCriticalSection);

    MainUpCallTable = UpCallTable;

    // Load Next Provider in chain if this is the first time called
    if (!gEntryCount)
    {
        OutputDebugString(L"Multihomed Layered Service Provider\n");

        //  Get all protocol information in database
        if ((ProtocolInfo = GetProviders(&TotalProtocols)) == NULL)
        {
            return  WSAEPROVIDERFAILEDINIT;
        }

        // Find out what our layered protocol catalog ID entry is
        for (i = 0; i < TotalProtocols; i++)
            if (memcmp (&ProtocolInfo[i].ProviderId, &ProviderGuid, sizeof (GUID))==0)
            {
                gLayerCatId = ProtocolInfo[i].dwCatalogEntryId;
                break;
            }

        // Save our protocol chains catalog ID entry
        gChainId = lpProtocolInfo->dwCatalogEntryId;

        gLayerCount=0;
        for(x=0; x < TotalProtocols ;x++)
        {
            for(y=0; y < ProtocolInfo[x].ProtocolChain.ChainLen ;y++)
            {
                if (gLayerCatId == ProtocolInfo[x].ProtocolChain.ChainEntries[y])
                {
                    gLayerCount++;
                    break;
                }
            }
        }
        gBaseInfo = (LPWSAPROTOCOL_INFOW)GlobalAlloc(GPTR, sizeof(WSAPROTOCOL_INFOW)*gLayerCount);
        if (!gBaseInfo)
        {
            return WSAENOBUFS;
        }
        idx=0;
        for(x=0; x < TotalProtocols ;x++)
        {
            for(y=0; y < ProtocolInfo[x].ProtocolChain.ChainLen ;y++)
            {
                if (gLayerCatId == ProtocolInfo[x].ProtocolChain.ChainEntries[y])
                {
                    // Our LSP exists in this entries chain
                    //
                    iBaseId = ProtocolInfo[x].ProtocolChain.ChainEntries[ProtocolInfo[x].ProtocolChain.ChainLen-1];
                    for(z=0; z < TotalProtocols ;z++)
                    {
                        if (ProtocolInfo[z].dwCatalogEntryId == iBaseId)
                        {
                            memcpy(&gBaseInfo[idx++], &ProtocolInfo[z], sizeof(WSAPROTOCOL_INFOW));
                            OutputDebugString(gBaseInfo[idx-1].szProtocol);
                            OutputDebugString(L"\n");
                        }
                    }
                }
            }
        }

        // Find our layered catalog ID entry in the protocol chain
        for(j = 0; j < lpProtocolInfo->ProtocolChain.ChainLen; j++)
        {
            if (lpProtocolInfo->ProtocolChain.ChainEntries[j] == gLayerCatId)
            {

                NextProviderCatId = lpProtocolInfo->ProtocolChain.ChainEntries[j + 1];
                break;
            }
        }


        // Find next provider path to load
        for (i = 0; i < TotalProtocols; i++)
            if (NextProviderCatId == ProtocolInfo[i].dwCatalogEntryId)
            {
                if (WSCGetProviderPath(&ProtocolInfo[i].ProviderId, ProviderPath, &ProviderPathLen, &Error) == SOCKET_ERROR)
                {
                    return  WSAEPROVIDERFAILEDINIT;
                }
                break;
            }

        if (!ExpandEnvironmentStrings(ProviderPath, LibraryPath, MAX_PATH))
        {
            return  WSAEPROVIDERFAILEDINIT;
        }

        if ((hProvider = LoadLibrary(LibraryPath)) == NULL)
        {
            return  WSAEPROVIDERFAILEDINIT;
        }


        if((WSPStartupFunc = (LPWSPSTARTUP) GetProcAddress(hProvider, "WSPStartup")) == NULL)
        {
            return  WSAEPROVIDERFAILEDINIT;
        }

        ReturnCode = (*WSPStartupFunc)(wVersion, lpWSPData, ProtoInfo, UpCallTable, lpProcTable);

        // Save the next providers procedure table
        memcpy(&NextProcTable, lpProcTable, sizeof WSPPROC_TABLE);

        // Remap service provider functions here

        lpProcTable->lpWSPAccept = WSPAccept;
        lpProcTable->lpWSPAddressToString = WSPAddressToString;
        lpProcTable->lpWSPAsyncSelect = WSPAsyncSelect;
        lpProcTable->lpWSPBind = WSPBind;
        lpProcTable->lpWSPCancelBlockingCall = WSPCancelBlockingCall;
        lpProcTable->lpWSPCleanup = WSPCleanup;
        lpProcTable->lpWSPCloseSocket = WSPCloseSocket;
        lpProcTable->lpWSPConnect = WSPConnect;
        lpProcTable->lpWSPDuplicateSocket = WSPDuplicateSocket;
        lpProcTable->lpWSPEnumNetworkEvents = WSPEnumNetworkEvents;
        lpProcTable->lpWSPEventSelect = WSPEventSelect;
        lpProcTable->lpWSPGetOverlappedResult = WSPGetOverlappedResult;
        lpProcTable->lpWSPGetPeerName = WSPGetPeerName;
        lpProcTable->lpWSPGetSockOpt = WSPGetSockOpt;
        lpProcTable->lpWSPGetSockName = WSPGetSockName;
        lpProcTable->lpWSPGetQOSByName = WSPGetQOSByName;
        lpProcTable->lpWSPIoctl = WSPIoctl;
        lpProcTable->lpWSPJoinLeaf = WSPJoinLeaf;
        lpProcTable->lpWSPListen = WSPListen;
        lpProcTable->lpWSPRecv = WSPRecv;
        lpProcTable->lpWSPRecvDisconnect = WSPRecvDisconnect;
        lpProcTable->lpWSPRecvFrom = WSPRecvFrom;
        lpProcTable->lpWSPSelect = WSPSelect;
        lpProcTable->lpWSPSend = WSPSend;
        lpProcTable->lpWSPSendDisconnect = WSPSendDisconnect;
        lpProcTable->lpWSPSendTo = WSPSendTo;
        lpProcTable->lpWSPSetSockOpt = WSPSetSockOpt;
        lpProcTable->lpWSPShutdown = WSPShutdown;
        lpProcTable->lpWSPSocket = WSPSocket;
        lpProcTable->lpWSPStringToAddress = WSPStringToAddress;

        gWSPData = lpWSPData;
        gProcTable = lpProcTable;
    } else
    {
        lpWSPData = gWSPData;
        lpProcTable = gProcTable;
        ReturnCode = 0;
    }

    gEntryCount++;

    LeaveCriticalSection(&gCriticalSection);

    return(ReturnCode);

} // End of WSPStartup Function


/*
*
*		 End of file main.cpp
*
*/


