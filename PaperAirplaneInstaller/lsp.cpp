#include "lsp.h"

#include <Winsock2.h> 
#include <Windows.h> 
#include <Ws2spi.h> 
#include <tchar.h> 
#include <iostream>
#include "Iphlpapi.h"
#include <Sporder.h>      // ������WSCWriteProviderOrder���� 

#include <stdio.h> 

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Rpcrt4.lib")  // ʵ����UuidCreate

// Ҫ��װ��LSP��Ӳ���룬���Ƴ���ʱ��Ҫʹ���� 
GUID  ProviderGuid = (sizeof(HANDLE) < 8 ? GUID({ 0x70b2b755, 0xa09d, 0x4b5d,{ 0xba, 0xda, 0xdb, 0x70, 0xbb, 0x1a, 0xbb, 0x21 } }) :
    GUID({ 0x51361ede, 0xe7c4, 0x4598,{ 0xa1, 0x77, 0xf4, 0xc5, 0xe9, 0x1, 0x7c, 0x25 } }));

LPWSAPROTOCOL_INFOW GetProvider(LPINT lpnTotalProtocols)
{
    DWORD dwSize = 0;
    int nError;
    LPWSAPROTOCOL_INFOW pProtoInfo = NULL;

    // ȡ����Ҫ�ĳ��� 
    if (::WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError) == SOCKET_ERROR)
    {
        if (nError != WSAENOBUFS)
            return NULL;
    }

    pProtoInfo = (LPWSAPROTOCOL_INFOW)::GlobalAlloc(GPTR, dwSize);
    *lpnTotalProtocols = ::WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError);
    return pProtoInfo;
}

void FreeProvider(LPWSAPROTOCOL_INFOW pProtoInfo)
{
    ::GlobalFree(pProtoInfo);
}

BOOL InstallLayeredServiceProvider(WCHAR* pwszPathName)
{
    UninstallLayeredServiceProvider();

    WCHAR wszLSPName[] = L"PaperAirplane";
    LPWSAPROTOCOL_INFOW pProtoInfo;
    int nProtocols;
    WSAPROTOCOL_INFOW OriginalProtocolInfo[3];
    DWORD            dwOrigCatalogId[3];
    int nArrayCount = 0;

    DWORD dwLayeredCatalogId;       // ���Ƿֲ�Э���Ŀ¼ID�� 

    int nError;
    BOOL bFindTcp = FALSE;

    // �ҵ����ǵ��²�Э�飬����Ϣ���������� 
    // ö�����з�������ṩ�� 
    pProtoInfo = GetProvider(&nProtocols);
    for (int i = 0; i < nProtocols; i++)
    {
        if (pProtoInfo[i].iAddressFamily == AF_INET)
        {
            if (!bFindTcp && pProtoInfo[i].iProtocol == IPPROTO_TCP)
            {
                bFindTcp = TRUE;
                {
                    memcpy(&OriginalProtocolInfo[nArrayCount], &pProtoInfo[i], sizeof(WSAPROTOCOL_INFOW));
                    OriginalProtocolInfo[nArrayCount].dwServiceFlags1 =
                        OriginalProtocolInfo[nArrayCount].dwServiceFlags1 & (~XP1_IFS_HANDLES);
                }
                dwOrigCatalogId[nArrayCount++] = pProtoInfo[i].dwCatalogEntryId;
            }
        }
    }

    // ��װ���ǵķֲ�Э�飬��ȡһ��dwLayeredCatalogId 
    // �����һ���²�Э��Ľṹ���ƹ������� 
    WSAPROTOCOL_INFOW LayeredProtocolInfo;
    memcpy(&LayeredProtocolInfo, &OriginalProtocolInfo[0], sizeof(WSAPROTOCOL_INFOW));

    // �޸�Э�����ƣ����ͣ�����PFL_HIDDEN��־ 
    wcscpy(LayeredProtocolInfo.szProtocol, wszLSPName);
    LayeredProtocolInfo.ProtocolChain.ChainLen = LAYERED_PROTOCOL; // 0; 
    LayeredProtocolInfo.dwProviderFlags |= PFL_HIDDEN;

    // ��װ 
    if (::WSCInstallProvider(&ProviderGuid,
        pwszPathName, &LayeredProtocolInfo, 1, &nError) == SOCKET_ERROR)
    {
        return FALSE;
    }

    // ����ö��Э�飬��ȡ�ֲ�Э���Ŀ¼ID�� 
    FreeProvider(pProtoInfo);
    pProtoInfo = GetProvider(&nProtocols);
    for (int i = 0; i < nProtocols; i++)
    {
        if (memcmp(&pProtoInfo[i].ProviderId, &ProviderGuid, sizeof(ProviderGuid)) == 0)
        {
            dwLayeredCatalogId = pProtoInfo[i].dwCatalogEntryId;
            break;
        }
    }

    // ��װЭ���� 
    // �޸�Э�����ƣ����� 
    WCHAR wszChainName[WSAPROTOCOL_LEN + 1];
    for (int i = 0; i < nArrayCount; i++)
    {
        if (OriginalProtocolInfo[i].iProtocol == IPPROTO_TCP) {
            swprintf(wszChainName, L"%ws %ws", wszLSPName, L"Tcpip [TCP/IP]");
        }

        wcscpy(OriginalProtocolInfo[i].szProtocol, wszChainName);
        if (OriginalProtocolInfo[i].ProtocolChain.ChainLen == 1)
        {
            OriginalProtocolInfo[i].ProtocolChain.ChainEntries[1] = dwOrigCatalogId[i];
        }
        else
        {
            for (int j = OriginalProtocolInfo[i].ProtocolChain.ChainLen; j > 0; j--)
            {
                OriginalProtocolInfo[i].ProtocolChain.ChainEntries[j]
                    = OriginalProtocolInfo[i].ProtocolChain.ChainEntries[j - 1];
            }
        }
        OriginalProtocolInfo[i].ProtocolChain.ChainLen++;
        OriginalProtocolInfo[i].ProtocolChain.ChainEntries[0] = dwLayeredCatalogId;
    }

    // ��ȡһ��Guid����װ֮ 
    GUID ProviderChainGuid;
    if (::UuidCreate(&ProviderChainGuid) == RPC_S_OK)
    {
        if (::WSCInstallProvider(&ProviderChainGuid,
            pwszPathName, OriginalProtocolInfo, nArrayCount, &nError) == SOCKET_ERROR)
        {
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }

    // ��������WinsockĿ¼�������ǵ�Э������ǰ 
    // ����ö�ٰ�װ��Э�� 
    FreeProvider(pProtoInfo);
    pProtoInfo = GetProvider(&nProtocols);

    DWORD dwIds[100];
    int nIndex = 0;

    // ������ǵ�Э���� 
    for (int i = 0; i < nProtocols; i++)
    {
        if ((pProtoInfo[i].ProtocolChain.ChainLen > 1) &&
            (pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId))
            dwIds[nIndex++] = pProtoInfo[i].dwCatalogEntryId;
    }

    // �������Э�� 
    for (int i = 0; i < nProtocols; i++)
    {
        if ((pProtoInfo[i].ProtocolChain.ChainLen <= 1) ||
            (pProtoInfo[i].ProtocolChain.ChainEntries[0] != dwLayeredCatalogId))
            dwIds[nIndex++] = pProtoInfo[i].dwCatalogEntryId;
    }

    // ��������WinsockĿ¼ 
    if ((nError = ::WSCWriteProviderOrder(dwIds, nIndex)) != ERROR_SUCCESS)
    {
        return FALSE;
    }
    FreeProvider(pProtoInfo);
    return TRUE;
}

BOOL UninstallLayeredServiceProvider()
{
    LPWSAPROTOCOL_INFOW pProtoInfo;
    int nProtocols;
    DWORD dwLayeredCatalogId;

    // ����Guidȡ�÷ֲ�Э���Ŀ¼ID�� 
    pProtoInfo = GetProvider(&nProtocols);
    int nError, i;
    for (i = 0; i < nProtocols; i++)
    {
        if (memcmp(&ProviderGuid, &pProtoInfo[i].ProviderId, sizeof(ProviderGuid)) == 0)
        {
            dwLayeredCatalogId = pProtoInfo[i].dwCatalogEntryId;
            break;
        }
    }

    if (i < nProtocols)
    {
        // �Ƴ�Э���� 
        for (int i = 0; i < nProtocols; i++)
        {
            if ((pProtoInfo[i].ProtocolChain.ChainLen > 1) &&
                (pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId))
            {
                ::WSCDeinstallProvider(&pProtoInfo[i].ProviderId, &nError);
            }
        }
        // �Ƴ��ֲ�Э�� 
        ::WSCDeinstallProvider(&ProviderGuid, &nError);
    }

    return TRUE;
}