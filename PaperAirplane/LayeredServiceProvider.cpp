#include "LayeredServiceProvider.h"
#include "Debugger.h"

LayeredServiceProvider LayeredServiceProvider_Current;

LayeredServiceProvider::LayeredServiceProvider()
{
    filterguid = (sizeof(HANDLE) < 8 ? GUID({ 0x70b2b755, 0xa09d, 0x4b5d,{ 0xba, 0xda, 0xdb, 0x70, 0xbb, 0x1a, 0xbb, 0x21 } }) :
        GUID({ 0x51361ede, 0xe7c4, 0x4598,{ 0xa1, 0x77, 0xf4, 0xc5, 0xe9, 0x1, 0x7c, 0x25 } }));
    TotalProtos = 0;
    ProtoInfoSize = 0;
    ProtoInfo = NULL;
    StartProviderCompleted = NULL;
    memset(&cs, 0, sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(&cs);
}

LayeredServiceProvider::~LayeredServiceProvider()
{
    DeleteCriticalSection(&cs);
}

bool LayeredServiceProvider::Load()
{
    int error;
    ProtoInfo = NULL;
    ProtoInfoSize = 0;
    TotalProtos = 0;
    if (WSCEnumProtocols(NULL, ProtoInfo, &ProtoInfoSize, &error) == SOCKET_ERROR)
    {
        if (error != WSAENOBUFS)
        {
            Debugger::Write(L"First WSCEnumProtocols Error!");
            return FALSE;
        }
    }
    if ((ProtoInfo = (LPWSAPROTOCOL_INFOW)GlobalAlloc(GPTR, ProtoInfoSize)) == NULL)
    {
        Debugger::Write(L"GlobalAlloc Error!");
        return FALSE;
    }
    if ((TotalProtos = WSCEnumProtocols(NULL, ProtoInfo, &ProtoInfoSize, &error)) == SOCKET_ERROR)
    {
        Debugger::Write(L"Second WSCEnumProtocols Error!");
        return FALSE;
    }
    return TRUE;
}

void LayeredServiceProvider::Free()
{
    if (ProtoInfo != NULL && GlobalSize(ProtoInfo) > 0)
    {
        GlobalFree(ProtoInfo);
        ProtoInfo = NULL;
    }
}

int LayeredServiceProvider::Start(WORD wversionrequested,
    LPWSPDATA lpwspdata,
    LPWSAPROTOCOL_INFOW lpProtoInfo,
    WSPUPCALLTABLE upcalltable,
    LPWSPPROC_TABLE lpproctable)
{
    LayeredServiceProvider::Free();
    {
        int i;
        int errorcode;
        int filterpathlen;
        DWORD layerid = 0;
        DWORD nextlayerid = 0;
        WCHAR* filterpath;
        HINSTANCE hfilter;
        LPWSPSTARTUP wspstartupfunc = NULL;
        if (lpProtoInfo->ProtocolChain.ChainLen <= 1)
        {
            Debugger::Write(L"ChainLen<=1");
            return FALSE;
        }
        LayeredServiceProvider::Load();
        for (i = 0; i < TotalProtos; i++)
        {
            if (memcmp(&ProtoInfo[i].ProviderId, &filterguid, sizeof(GUID)) == 0)
            {
                layerid = ProtoInfo[i].dwCatalogEntryId;
                break;
            }
        }
        for (i = 0; i < lpProtoInfo->ProtocolChain.ChainLen; i++)
        {
            if (lpProtoInfo->ProtocolChain.ChainEntries[i] == layerid)
            {
                nextlayerid = lpProtoInfo->ProtocolChain.ChainEntries[i + 1];
                break;
            }
        }
        filterpathlen = MAX_PATH;
        filterpath = (WCHAR*)GlobalAlloc(GPTR, filterpathlen);
        for (i = 0; i < TotalProtos; i++)
        {
            if (nextlayerid == ProtoInfo[i].dwCatalogEntryId)
            {
                if (WSCGetProviderPath(&ProtoInfo[i].ProviderId, filterpath, &filterpathlen, &errorcode) == SOCKET_ERROR)
                {
                    Debugger::Write(L"WSCGetProviderPath Error!");
                    return WSAEPROVIDERFAILEDINIT;
                }
                break;
            }
        }
        if (!ExpandEnvironmentStringsW(filterpath, filterpath, MAX_PATH))
        {
            Debugger::Write(L"ExpandEnvironmentStrings Error!");
            return WSAEPROVIDERFAILEDINIT;
        }
        if ((hfilter = LoadLibraryW(filterpath)) == NULL)
        {
            Debugger::Write(L"LoadLibrary Error!");
            return WSAEPROVIDERFAILEDINIT;
        }
        if ((wspstartupfunc = (LPWSPSTARTUP)GetProcAddress(hfilter, "WSPStartup")) == NULL)
        {
            Debugger::Write(L"GetProcessAddress Error!");
            return WSAEPROVIDERFAILEDINIT;
        }
        if ((errorcode = wspstartupfunc(wversionrequested, lpwspdata, lpProtoInfo, upcalltable, lpproctable)) != ERROR_SUCCESS)
        {
            Debugger::Write(L"wspstartupfunc Error!");
            return errorcode;
        }
        NextProcTable = *lpproctable; // ����ԭ������ں�����
        if (StartProviderCompleted != NULL)
        {
            StartProviderCompleted(&NextProcTable, lpproctable);
        }
    }
    LayeredServiceProvider::Free();
    return 0;
};