/* Authors: perry peng
 *
 * Date: September 11, 2011
 *
 * Project Name: 
 * Project Version:
 *
 * Module: main.c
 *
 */

#include <stdio.h>
#include <malloc.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <Mstcpip.h>
#include <Iphlpapi.h>

#pragma comment(lib, "Ws2_32")
#pragma comment(lib, "Iphlpapi")

#define MAX_PACKET_LENGTH 65535

#define ERR_MSG(m)                            \
  fprintf(stderr, "%s [%d] ", __FILE__, __LINE__ - 2); \
  if (!print_err_msg(GetLastError(), NULL))    \
    fprintf(stderr, m);

#define WSA_ERR_MSG(m)                        \
  fprintf(stderr, "%s [%d] ", __FILE__, __LINE__ - 2); \
  if (!print_err_msg(WSAGetLastError(), NULL)) \
    fprintf(stderr, m);

#define FATAL_ERROR(m)                        \
  {                                           \
    ERR_MSG(m);                               \
    goto Exception;                           \
  }

#define WSA_FATAL_ERROR(m)                    \
  {                                           \
    WSA_ERR_MSG(m);                           \
    goto Exception;                           \
  }

#define PRINT_IP(h, t)                        \
  fprintf(stdout, (h));                       \
  //if (!print_addr_info((PIP_ADDR_LIST)adp_info->First##t##Address)) \
    //fprintf(stdout, "No %s Addresses\n", #t);

#define HALLOC(x)       HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (x))
#define REALLOC(p, x)   HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (p), (x))
#define HFREE(p)        HeapFree(GetProcessHeap(), 0, (p))

#define AALLOC(x)       _aligned_malloc((x), MEMORY_ALLOCATION_ALIGNMENT)
#define AFREE(x)        _aligned_free((x))

typedef PIP_ADAPTER_UNICAST_ADDRESS PIP_ADDR_LIST;

typedef union _ADDR_IN
{
  struct
  {
    short family;
    u_short port;
    u_short Words[20];
  };
  struct sockaddr_in ipv4;
  struct sockaddr_in6 ipv6;
} ADDR_IN, *LPADDR_IN;

typedef struct _tagPacketNode
{
  SLIST_ENTRY entry;
  WSAOVERLAPPED overlap;
  WSABUF wsa;
  SOCKET socket;
  DWORD index;
  DWORD flags;
  DWORD length;
  struct _tagPacketNode *forward;
} PACKET, *LPPACKET;

// WARNING !!!  All list items must be aligned on a MEMORY_ALLOCATION_ALIGNMENT boundary;
// WARNING !!!  otherwise, this function will behave unpredictably.
PSLIST_HEADER
  packet_list = NULL;

LPPACKET
  packet_free_list = NULL;

CRITICAL_SECTION
  crit_sec;

volatile int
  bLoop;

DWORD
  packet_index = 0,
  packet_count = 0;

int print_addr_info(PIP_ADDR_LIST);
int print_err_msg(long, char *);

BOOL WINAPI ctrl_callback(DWORD);
DWORD WINAPI work_thread(LPVOID);
void begin_recv_packet(SOCKET);

int get_key_input(LPDWORD, DWORD, int);
int get_socket_addr(LPADDR_IN, short, unsigned short);

int
main()
{
  DWORD
    i,
    sck_ioctl,
    threads_num;

  HANDLE
    *threads = NULL,
    iocp = NULL;

  SOCKET
    socket_raw = INVALID_SOCKET;

  SYSTEM_INFO
    sys_info;

  LPPACKET
    packet;

  ADDR_IN
    addr;

  WSADATA
    wsa_init;

  if (!SetConsoleCtrlHandler(ctrl_callback, TRUE))
    FATAL_ERROR("SetConsoleCtrlHandler() failed to install console handler.\n");

  // ��ʼ���̴߳�ӡͬ������
  InitializeCriticalSection(&crit_sec);

  // ��ʼ��SOCKET��
  if (WSAStartup(MAKEWORD(2, 2), &wsa_init) ||
    LOBYTE(wsa_init.wVersion) != 2 ||
    HIBYTE(wsa_init.wVersion) != 2)
    WSA_FATAL_ERROR("Failed to initialize socket.\n");

  // ��ʾ��ǰ�����������������б����û�ѡ��
  if (!get_socket_addr(&addr, AF_UNSPEC, 0))
    goto Exception;

  // ������SOCKET��
  socket_raw = WSASocket(addr.family, SOCK_RAW, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (socket_raw == INVALID_SOCKET)
    WSA_FATAL_ERROR("Failed to create socket.\n");

  // �󶨵�����ĳ��������
  if (bind(socket_raw, (struct sockaddr *)&addr, sizeof(ADDR_IN)) == SOCKET_ERROR)
    WSA_FATAL_ERROR("Failed to call bind.\n");

  // ����SOCKET I/O CODE����RCVALL��������������ģʽ���������е���������ϵ����ݰ���
  sck_ioctl = RCVALL_ON;
  if (WSAIoctl(socket_raw, SIO_RCVALL, &sck_ioctl, sizeof(sck_ioctl), NULL, 0, &i, NULL, NULL) == SOCKET_ERROR)
    WSA_FATAL_ERROR("Failed to call WSAIoctl.\n");

  // �����µ���ɶ˿ڲ���SOCKET�������ɶ˿ڡ�
  iocp = CreateIoCompletionPort((HANDLE)socket_raw, NULL, socket_raw, 0);
  if (iocp == NULL)
    FATAL_ERROR("Failed to call CreateIoCompletionPort.\n");

  // ��õ�ǰϵͳʶ�𵽵Ĵ�����������
  GetSystemInfo(&sys_info);
  threads_num = sys_info.dwNumberOfProcessors * 2;

  // Ϊ����ͷ�����ڴ档
  packet_list = (PSLIST_HEADER)AALLOC(sizeof(SLIST_HEADER));
  if (packet_list == NULL)
    FATAL_ERROR("Failed to allocated memory.");

  // ��ʼ������
  InitializeSListHead(packet_list);

  // �����߳���������ڴ档
  threads = (HANDLE*)HALLOC(threads_num * sizeof(HANDLE));
  if (!threads)
    FATAL_ERROR("Failed to allocate memory for thread array.\n");

  bLoop = 1;
  // �����߳��顣
  for (i = 0; i < threads_num; i++)
  {
    threads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)work_thread, (LPVOID)iocp, 0, NULL);
    if (threads[i] == NULL)
    {
      ERR_MSG("Failed to create thread array.");
      for (; i < threads_num; i++)
        threads[i] = NULL;
      goto InitThreadsFailed;
    }
  }

  // �ȴ��û������˳����رճ���
  while (get_key_input(&i, 0, TRUE))
  {
    switch (i)
    {
    case VK_RETURN:
      // ��ʼ�˻��������
      packet_index = 0;
      begin_recv_packet(socket_raw);
      break;
    }
  }

InitThreadsFailed:

  bLoop = 0;
  WaitForMultipleObjects(threads_num, threads, TRUE, INFINITE);

Exception:
  if (threads)
  {
    for (i = 0; i < threads_num; i++)
      CloseHandle(threads[i]);

    // �ͷŵ��߳������ڴ档
    HFREE(threads);
    threads = NULL;
  }

  if (socket_raw != INVALID_SOCKET)
    closesocket(socket_raw);

  if (iocp != NULL)
    CloseHandle(iocp);

  if (packet_list)
  {
    // ������������нڵ㡣
    InterlockedFlushSList(packet_list);

    // �ͷŵ�����ͷռ�õ��ڴ档
    AFREE(packet_list);
    packet_list = NULL;
  }

  while (packet_free_list != NULL)
  {
    packet = packet_free_list;
    if (packet->wsa.buf)
      HFREE(packet->wsa.buf);

    // ָ��ǰһ������ڵ㡣
    packet_free_list = packet->forward;
    // �ͷŵ�����Ԫռ�õ��ڴ档
    AFREE(packet);
  }

  // ɾ���̴߳�ӡͬ������
  DeleteCriticalSection(&crit_sec);

  WSACleanup();

  // ȡ��CtrlHandler�����г̡�
  SetConsoleCtrlHandler(ctrl_callback, FALSE);

  return 0;
}

void
begin_recv_packet(socket_raw)
  SOCKET socket_raw;
{
  LPPACKET
    packet;

  DWORD
    error;

  // �ȴӶ����в鿴�Ƿ��п��е��ڴ浥Ԫ��
  packet = (LPPACKET)InterlockedPopEntrySList(packet_list);
  if (packet == NULL)
  {
    // ����ڶ�����û���ҵ����������½�һ���ڴ浥Ԫ��WSARecvʹ�á�
    packet = (LPPACKET)AALLOC(sizeof(PACKET));
    if (packet == NULL)
    {
      ERR_MSG("Memory allocation failed.\n");
      return;
    }

    ZeroMemory(packet, sizeof(PACKET));

    // �µ��ڴ浥Ԫ�����ݻ������ڴ����롣
    packet->wsa.len = MAX_PACKET_LENGTH;
    packet->wsa.buf = HALLOC(MAX_PACKET_LENGTH);
    if (packet->wsa.buf == NULL)
    {
      ERR_MSG("Memory allocation failed.\n");
      AFREE(packet);
      return;
    }

    InterlockedIncrement(&packet_count);

    // ����������ķ��ʡ�
    EnterCriticalSection(&crit_sec);

    // �κ��½��������ݻ���������������һ���б��У�ȷ���ͷ�ʱ������©��
    packet->forward = packet_free_list;
    packet_free_list = packet;

    LeaveCriticalSection(&crit_sec);
  }

  // ���հ�������
  InterlockedIncrement(&packet_index);

  // �����Ϣ��
  packet->index = packet_index;   // ȫ�ֵİ�������
  packet->socket = socket_raw;    // ʹ�ô˰���SOCKET�����
  packet->flags = 0;

  // ��ʼ�첽�������ݰ���
  if (WSARecv(socket_raw, &packet->wsa, 1, &packet->length, &packet->flags, &packet->overlap, NULL) == SOCKET_ERROR)
  {
    if ((error = WSAGetLastError()) != WSA_IO_PENDING)
    {
      WSASetLastError(error);
      WSA_ERR_MSG("WSARecv was failed.\n");
    }
  }
}

BOOL WINAPI
ctrl_callback(event)
  DWORD event;
{
  DWORD
    length;

  INPUT_RECORD
    record;

  switch (event)
  {
  case CTRL_C_EVENT:
  case CTRL_CLOSE_EVENT:
  case CTRL_BREAK_EVENT:
  case CTRL_LOGOFF_EVENT:
  case CTRL_SHUTDOWN_EVENT:
    fprintf(stdout, "System is exiting...\n");

    ZeroMemory(&record, sizeof(INPUT_RECORD));
    record.EventType = KEY_EVENT;
    record.Event.KeyEvent.bKeyDown = 1;
    record.Event.KeyEvent.wRepeatCount = 1;
    record.Event.KeyEvent.wVirtualKeyCode = VK_ESCAPE;

    // ģ���û�����ESC������
    WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &record, 1, &length);

    Sleep(10);

    // ָʾ���й����߳�ֹͣ������
    if (bLoop)
      bLoop = 0;

    break;

  default:
    // unknown type--better pass it on.
    return FALSE;
  }

  return TRUE;
}

DWORD WINAPI
work_thread(data)
  LPVOID data;
{
  HANDLE
    iocp = (HANDLE)data;

  DWORD
    i,
    error,
    transferred;

  ULONG_PTR
    key;

  LPWSAOVERLAPPED
    overlap;

  LPPACKET
    packet;

  CHAR
    display[128];

  while (bLoop)
  {
    overlap = NULL;
    // ����ɶ˿ڶ�����ȡ��һ���Ѿ���ɵ�I/O������
    if (!GetQueuedCompletionStatus(iocp, &transferred, &key, &overlap, 500))  //INFINITE, in milliseconds
    {
      error = GetLastError();
      if (error == WAIT_TIMEOUT)
        continue;

      // û�����ݣ�������ԭ���˳��̡߳�
      if (overlap == NULL)
        break;
    }

    // ȡ������Ԫָ�롣
    packet = (LPPACKET)(((LPBYTE)overlap) - sizeof(SLIST_ENTRY));

    // ���һ�����պ��ڴ�������ǰ�Ϳ�������������һ������
    begin_recv_packet(packet->socket);

    // �ӵ�����WSARev�����ݣ���ʼ�������Ϣ��
    sprintf(display, "RX[%04X], I: %04d L: %04d, ", GetCurrentThreadId(), packet->index, packet->length);

    for (i = 0; i < 16; i++)
      sprintf(&display[strlen(display)], "%02X ", (BYTE)packet->wsa.buf[i]);
    sprintf(&display[strlen(display)], "\n");

    fprintf(stdout, display);

    // ���������ݡ�
    ZeroMemory(&packet->overlap, sizeof(WSAOVERLAPPED));

    // ���ݰ�ʹ�������������ӣ��Ա��������߳�ʹ�á�
    InterlockedPushEntrySList(packet_list, (PSLIST_ENTRY)packet);
  }

  fprintf(stdout, "Thread %04X was exited.\n", GetCurrentThreadId());

  return 0;
}

int
print_err_msg(errorId, format)
  long errorId;
  char *format;
{
  LPSTR
    text = NULL;

  DWORD
    length;

  // Gets the message string by an error code.
  length = FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, errorId, 0,
    (LPSTR)&text,
    0, NULL);

  if (length == 0)
  {
    fprintf(stderr, "FormatMessage was failed, error:0x%08x, format:0x%08x\n", GetLastError(), errorId);
    return 0;
  }

  fprintf(stderr, "%08X:", errorId);

  if (format != NULL)
    fprintf(stderr, format, text);
  else
    fprintf(stderr, text);

  // Free the buffer allocate by the FormatMessage function.
  LocalFree(text);

  return 1;
}

int
get_key_input(key, exitkey, keydown)
  LPDWORD key;
  DWORD exitkey;
  BOOL keydown;
{
  DWORD
    length;

  INPUT_RECORD
    record;

  *key = -1;
  // ��ȡһ�����̨�¼���¼��
  if (!ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &record, 1, &length) ||
      !length)
  {
    ERR_MSG("Failed to get user key events.");
    Sleep(100);
    // ��ȡ��������ʧ�ܣ�����TRUE������һ����ȡ������
    return 1;
  }

  // �ж��Ƿ�Ϊ�����¼���
  if (record.EventType != KEY_EVENT ||
     !(keydown ? record.Event.KeyEvent.bKeyDown: !record.Event.KeyEvent.bKeyDown))
    return 1;

  // ���ص�ǰKEYֵ��
  *key = record.Event.KeyEvent.wVirtualKeyCode;

  // �����ָ�����˳�������ESC�򷵻�FALSE��
  return !(*key == exitkey || *key == VK_ESCAPE);
}

int
print_addr_info(addrinfo)
  PIP_ADDR_LIST addrinfo;
{
  CHAR
    /*
      IPv6�汾IP��ַ��Ҫ����Ĵ���ռ䡣
      For an IPv4 address, this buffer should be large enough to hold at least 16 characters.
      For an IPv6 address, this buffer should be large enough to hold at least 46 characters.
     */
    ip_addr[64];

  DWORD
    bytes = 0;

  while (addrinfo != NULL)
  {
    if (bytes > 0)
      fprintf(stdout, "\n                ");

    // ��IP��ַ��Ϣת��Ϊ�ַ�����
    bytes = sizeof(ip_addr);
    if (WSAAddressToString(addrinfo->Address.lpSockaddr, addrinfo->Address.iSockaddrLength, NULL, ip_addr, &bytes) == ERROR_SUCCESS)
      fprintf(stdout, ip_addr);
    else
    {
      switch (WSAGetLastError())
      {
      case ERROR_SUCCESS:
        fprintf(stdout, ip_addr);
        break;
      case WSAEFAULT:
        fprintf(stdout, "Invalid buffer space.");
        break;
      case WSAEINVAL:
        fprintf(stdout, "Incorrect parameter.");
        break;
      case WSANOTINITIALISED:
        fprintf(stdout, "Winsock 2 DLL has not been initialized.");
        return FALSE;
      case WSAENOBUFS:
        fprintf(stdout, "No buffer space is available.");
        break;
      }
    }

    addrinfo = addrinfo->Next;
  }

  if (bytes == 0)
    return 0;

  fprintf(stdout, "\n");
  return 1;
}

int
get_socket_addr(addr, family, port)
  LPADDR_IN addr;
  short family;
  unsigned short port;
{
  PIP_ADAPTER_ADDRESSES
    adp_info_list = NULL,
    adp_info;

  CONSOLE_SCREEN_BUFFER_INFO
    con_info;

  PIP_ADAPTER_UNICAST_ADDRESS
    uni_addr;

  LPADDR_IN
    addr_in = NULL;

  int
    succeed = 9,
    vista = (GetVersion() & 0xff) >= 6;

  DWORD
    i,
    bytes,
    index,
    result;

  // Ϊ�������һ���Ĵ���ռ䡣
  bytes = sizeof(IP_ADAPTER_ADDRESSES);
  adp_info_list = (PIP_ADAPTER_ADDRESSES)HALLOC(bytes);
  if (adp_info_list == NULL)
    FATAL_ERROR("Failed to allocate heap memory.\n");

  i = GAA_FLAG_INCLUDE_PREFIX;
  if (vista)
    i |= GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_WINS_INFO;

  // ��ô�����������Ϣ������
  while ((result = GetAdaptersAddresses(AF_UNSPEC, i, NULL, adp_info_list, &bytes)) != ERROR_SUCCESS)
  {
    if (result != ERROR_BUFFER_OVERFLOW)
      FATAL_ERROR("Failed to get address information.\n");

    // ������ڴ治�㣬�����������ڴ棬ֱ���㹻Ϊֹ��
    adp_info_list = (PIP_ADAPTER_ADDRESSES)REALLOC(adp_info_list, bytes);
    if (adp_info_list == NULL)
      FATAL_ERROR("Failed to allocate heap memory.\n");
  }

  // ��ÿ���̨���ڳߴ磬��ֹ����̫��Ӱ�������ʽ��
  if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &con_info))
    con_info.dwSize.X = 1024;
  con_info.dwSize.X -= 22;

  index = 0;

  // ��ӡ��������������Ϣ��
  adp_info = adp_info_list;
  while (adp_info)
  {
    // ֻ��Ҫ�г�Ethernet������
    if (adp_info->IfType != IF_TYPE_ETHERNET_CSMACD &&
        adp_info->IfType != IF_TYPE_IEEE80211)
    {
      adp_info = adp_info->Next;
      continue;
    }
    adp_info->IfIndex = ++index;
    fprintf(stdout, "%d   Name:       ", adp_info->IfIndex);
    if (wcslen(adp_info->Description) > con_info.dwSize.X)
    {
      for (i = 0; i < con_info.dwSize.X; i++)
        fprintf(stdout, "%wC", adp_info->Description[i]);
      fprintf(stdout, " ...\n");
    }
    else
      fprintf(stdout, "%wS\n", adp_info->Description);

    fprintf(stdout, "    MAC:        ");
    if (adp_info->PhysicalAddressLength > 0)
    {
      for (i = 0; i < adp_info->PhysicalAddressLength; i++)
      {
        if (i > 0)
          fprintf(stdout, "-");
        fprintf(stdout, "%02X", adp_info->PhysicalAddress[i]);
      }
      fprintf(stdout, "\n");
    }
    else
      fprintf(stdout, "00-00-00-00-00-00\n");

    PRINT_IP("    A-IP:       ", Anycast);
    PRINT_IP("    M-IP:       ", Multicast);
    PRINT_IP("    U-IP:       ", Unicast);
    PRINT_IP("    DNS:        ", DnsServer);

    //fprintf(stdout, "    DNS Suffix: %wS\n", adp_info->DnsSuffix);

    if (vista)
    {
      PRINT_IP("    GATEWAY:    ", Gateway);
      PRINT_IP("    WINS:       ", WinsServer);

      fprintf(stdout, "    SPEED:      ");
      if (adp_info->OperStatus == IfOperStatusUp ||
          adp_info->OperStatus == IfOperStatusTesting)
      {
        //fprintf(stdout, "TX=%I64dMBps, ", adp_info->TransmitLinkSpeed / 1024 / 1024);
        //fprintf(stdout, "RX=%I64dMBps\n", adp_info->ReceiveLinkSpeed / 1024 / 1024);
      }
      else
        fprintf(stdout, "TX=0MBps, RX=0MBps\n");
    }

    fprintf(stdout, "    STATUS:     ");
    switch (adp_info->OperStatus)
    {
    case IfOperStatusUp:              fprintf(stdout, "Up\n"); break;
    case IfOperStatusDown:            fprintf(stdout, "Down\n"); break;
    case IfOperStatusTesting:         fprintf(stdout, "Testing\n"); break;
    case IfOperStatusUnknown:         fprintf(stdout, "Unknown\n"); break;
    case IfOperStatusDormant:         fprintf(stdout, "Dormant\n"); break;
    case IfOperStatusNotPresent:      fprintf(stdout, "Not Present\n"); break;
    case IfOperStatusLowerLayerDown:  fprintf(stdout, "Lower Layer Down\n"); break;
    }

    fprintf(stdout, "\n");
    adp_info = adp_info->Next;
  }

  fprintf(stdout, "Please select a network adapter:");

  adp_info = NULL;
  // �ȴ��û�����һ����ֵ��ʾѡ��ĳ��������
  while (get_key_input(&i, 0, TRUE))
    if (i >= 0x30 && i <= 0x39)
      break;

  adp_info = adp_info_list;
  while (adp_info != NULL)
  {
    // �ж��Ƿ�Ϊ��ǰ�û���ѡ���������������š�
    if ((adp_info->IfIndex + 0x30) != i)
    {
      adp_info = adp_info->Next;
      continue;
    }

    // ������������û��һ����Ч��IP��ַ��
    if (adp_info->FirstUnicastAddress == NULL)
      break;

    // ָ��IP�����ɲ���family���塣
    index = family;

    // ���familyδ�����Ĭ��Ϊ��IPV4��ַ��
    if (index == AF_UNSPEC)
      index = AF_INET;

    while (TRUE)
    {
      uni_addr = adp_info->FirstUnicastAddress;
      while (uni_addr != NULL)
      {
        addr_in = (LPADDR_IN)uni_addr->Address.lpSockaddr;
        if (addr_in->family == index)
          break;
        uni_addr = uni_addr->Next;
      }

      // �ҵ�һ��ָ���ĵ�ַ��
      if (addr_in->family == index)
        break;

      // ���ָ����IP���͵�δƥ�䵽��ȷ��IP��
      if (family != AF_UNSPEC)
      {
        index = 0;
        break;
      }

      // ���û���ҵ��κ�IPV4��ַ������IPV6��ַ��
      switch (index)
      {
      case AF_INET:
        // IPV4û�о���IPV6.
        index = AF_INET6;
        break;
      case AF_INET6:
        // IPV6û�о���...��
        break;
      }

      // ������ʱ�˳��������IPV6Ҳû���ҵ���
      if (index == AF_INET6)
      {
        // ʲôIP��ַҲû���ҵ���
        index = 0;
        break;
      }
    }

    // δ���ҵ�IPV4��IPV6��ַ��
    if (index == 0)
      break;

    // �����ַ�ṹ���ȡ�
    bytes = uni_addr->Address.iSockaddrLength;
    if (bytes == 0)
      break;

    i = sizeof(ADDR_IN);
    // ��ӡ��ǰѡ���IP��ַ������IPV4��IPV6��
    if (WSAAddressToString((LPSOCKADDR)addr_in, bytes, NULL, (char*)addr, &i) == ERROR_SUCCESS)
      fprintf(stdout, "\nOK, the IP address %s was selected.\n", (char*)addr);
    else
      fprintf(stdout, "\nOK, the adapter %c was selected.\n", i);

    // ��ѡ���������IP��ַ����socket������
    if (bytes > sizeof(ADDR_IN))
      bytes = sizeof(ADDR_IN);
    memcpy(addr, addr_in, bytes);

    addr->family = addr_in->family;
    addr->port = port;

    succeed = 1;

    break;
  }

  if (!succeed)
  {
    if (adp_info == NULL)
      fprintf(stdout, "\nNo adapter found which match your selection.\n");
    else
    {
      if (index == 0)
      {
        fprintf(stdout, "\nNo IP address found which match");
        switch (family)
        {
        case AF_INET:
          fprintf(stdout, " IPV4.\n");
          break;
        case AF_INET6:
          fprintf(stdout, " IPV6.\n");
          break;
        }
      }
      else
        fprintf(stdout, "\nThe adapter %c doesn't have a valid IP address.\n", i);
    }
  }

Exception:
  if (adp_info_list != NULL)
    HFREE(adp_info_list);

  return succeed;
}