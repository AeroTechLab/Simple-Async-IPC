//////////////////////////////////////////////////////////////////////////////////////
//                                                                                  //
//  Copyright (c) 2016-2017 Leonardo Consoni <consoni_2519@hotmail.com>             //
//                                                                                  //
//  This file is part of Simple Async IPC.                                          //
//                                                                                  //
//  Simple Async IPC is free software: you can redistribute it and/or modify        //
//  it under the terms of the GNU Lesser General Public License as published        //
//  by the Free Software Foundation, either version 3 of the License, or            //
//  (at your option) any later version.                                             //
//                                                                                  //
//  Simple Async IPC is distributed in the hope that it will be useful,             //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of                  //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                    //
//  GNU Lesser General Public License for more details.                             //
//                                                                                  //
//  You should have received a copy of the GNU Lesser General Public License        //
//  along with Simple Async IPC. If not, see <http://www.gnu.org/licenses/>.        //
//                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////
///// Multiplatform library for creation and handling of IP sockets connections /////
///// as server or client, using TCP or UDP protocols                           /////
/////////////////////////////////////////////////////////////////////////////////////

#include "ipc_base_ip.h"

#include "threads/threads.h"
#include "threads/thread_safe_queues.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
  
#ifdef __unix__
  #define _XOPEN_SOURCE 700
#endif
  
#ifdef WIN32
  //#include <winsock2.h>
  #include <ws2tcpip.h>
  #include <stdint.h>
  #include <time.h>
  
  #pragma comment(lib, "Ws2_32.lib")
  
  #define SHUT_RDWR SD_BOTH
  #define close( i ) closesocket( i )
  #define poll WSAPoll
  
  typedef SOCKET Socket;
#else
  #include <fcntl.h>
  #include <unistd.h>
  #include <errno.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <stropts.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>

  const int SOCKET_ERROR = -1;
  const int INVALID_SOCKET = -1;

  typedef int Socket;
#endif

#define PORT_LENGTH 6                                           // Maximum length of short integer string representation
  
#ifndef IP_NETWORK_LEGACY
  #include <poll.h>
  typedef struct pollfd SocketPoller;
  #define ADDRESS_LENGTH INET6_ADDRSTRLEN                       // Maximum length of IPv6 address (host+port) string
  typedef struct sockaddr_in6 IPAddressData;                    // IPv6 structure can store both IPv4 and IPv6 data
  #define IS_IPV6_MULTICAST_ADDRESS( address ) ( ((struct sockaddr_in6*) address)->sin6_addr.s6_addr[ 0 ] == 0xFF )
  #define ARE_EQUAL_IPV6_ADDRESSES( address_1, address_2 ) ( ((struct sockaddr*) address_1)->sa_family == AF_INET6 && \
                                                             ((IPAddressData*) address_1)->sin6_port == ((IPAddressData*) address_2)->sin6_port && \
                                                             memcmp( ((IPAddressData*) address_1)->sin6_addr.s6_addr, ((IPAddressData*) address_2)->sin6_addr.s6_addr, 16 ) == 0 )
#else
  typedef struct { Socket fd; } SocketPoller;
  #define ADDRESS_LENGTH INET_ADDRSTRLEN + PORT_LENGTH          // Maximum length of IPv4 address (host+port) string
  typedef struct sockaddr_in IPAddressData;                     // Legacy mode only works with IPv4 addresses
  #define IS_IPV6_MULTICAST_ADDRESS( address ) false
  #define ARE_EQUAL_IPV6_ADDRESSES( address_1, address_2 ) false
#endif
typedef struct sockaddr* IPAddress;                             // Opaque IP address type
#define IS_IPV4_MULTICAST_ADDRESS( address ) ( (ntohl( ((struct sockaddr_in*) address)->sin_addr.s_addr ) & 0xF0000000) == 0xE0000000 )
#define ARE_EQUAL_IPV4_ADDRESSES( address_1, address_2 ) ( ((struct sockaddr*) address_1)->sa_family == AF_INET && \
                                                           ((struct sockaddr_in*) address_1)->sin_port == ((struct sockaddr_in*) address_2)->sin_port )

#define IS_IP_MULTICAST_ADDRESS( address ) ( IS_IPV4_MULTICAST_ADDRESS( address ) || IS_IPV6_MULTICAST_ADDRESS( address ) )
#define ARE_EQUAL_IP_ADDRESSES( address_1, address_2 ) ( ARE_EQUAL_IPV4_ADDRESSES( address_1, address_2 ) || ARE_EQUAL_IPV6_ADDRESSES( address_1, address_2 ) )


///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                                      INTERFACE DEFINITION                                       /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

const size_t QUEUE_MAX_ITEMS = 10;
const unsigned long EVENT_WAIT_TIME_MS = 5000;

typedef struct _IPConnectionData IPConnectionData;
typedef IPConnectionData* IPConnection;

// Generic structure to store methods and data of any connection type handled by the library
struct _IPConnectionData
{
  SocketPoller* socket;
  void (*ref_ReceiveMessage)( IPConnection );
  void (*ref_SendMessage)( IPConnection, const Byte* );
  void (*ref_Close)( IPConnection );
  IPAddressData addressData;
  union {
    SocketPoller** clientsList;
    IPAddressData* addressesList;
  };
  size_t remotesCount;
  TSQueue readQueue;
  TSQueue writeQueue;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                                        GLOBAL VARIABLES                                         /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

// Thread for asyncronous connections update
static Thread globalReadThread = THREAD_INVALID_HANDLE;
static Thread globalWriteThread = THREAD_INVALID_HANDLE;
static volatile bool isNetworkRunning = false;

static IPConnection* globalConnectionsList = NULL;
static int activeConnectionsCount = 0;

#ifdef IP_NETWORK_LEGACY
static fd_set polledSocketsSet = { 0 };
static fd_set activeSocketsSet = { 0 };
#else
static SocketPoller polledSocketsList[ 1024 ] = { 0 };
#endif
static size_t polledSocketsNumber = 0;

/////////////////////////////////////////////////////////////////////////////
/////                        FORWARD DECLARATIONS                       /////
/////////////////////////////////////////////////////////////////////////////


static void ReceiveTCPClientMessage( IPConnection );
static void ReceiveUDPClientMessage( IPConnection );
static void ReceiveTCPServerMessages( IPConnection );
static void ReceiveUDPServerMessages( IPConnection );
static void SendTCPClientMessage( IPConnection, const Byte* );
static void SendUDPClientMessage( IPConnection, const Byte* );
static void SendTCPServerMessages( IPConnection, const Byte* );
static void SendUDPServerMessages( IPConnection, const Byte* );
static void CloseTCPServer( IPConnection );
static void CloseUDPServer( IPConnection );
static void CloseTCPClient( IPConnection );
static void CloseUDPClient( IPConnection );

static void* AsyncReadQueues( void* );
static void* AsyncWriteQueues( void* );


//////////////////////////////////////////////////////////////////////////////////
/////                             INITIALIZATION                             /////
//////////////////////////////////////////////////////////////////////////////////

#ifndef IP_NETWORK_LEGACY
static int CompareSockets( const void* ref_socket_1, const void* ref_socket_2 )
{
  return ( ((SocketPoller*) ref_socket_1)->fd - ((SocketPoller*) ref_socket_2)->fd );
}
#endif

static SocketPoller* AddSocketPoller( Socket socketFD )
{
  #ifndef IP_NETWORK_LEGACY
  SocketPoller cmpPoller = { .fd = socketFD };
  SocketPoller* socketPoller = (SocketPoller*) bsearch( &cmpPoller, polledSocketsList, polledSocketsNumber, sizeof(SocketPoller), CompareSockets );
  if( socketPoller == NULL )
  {
    socketPoller = &(polledSocketsList[ polledSocketsNumber ]);
    socketPoller->fd = socketFD;
    socketPoller->events = POLLIN | POLLHUP;
    polledSocketsNumber++;
    qsort( polledSocketsList, polledSocketsNumber, sizeof(SocketPoller), CompareSockets );
  }
  #else
  SocketPoller* socketPoller = (SocketPoller*) malloc( sizeof(SocketPoller) );
  FD_SET( socketFD, &polledSocketsSet );
  if( socketFD >= polledSocketsNumber ) polledSocketsNumber = socketFD + 1;
  socketPoller->fd = socketFD;
  #endif
  return socketPoller;
}

// Handle construction of a IPConnection structure with the defined properties
static IPConnection AddConnection( Socket socketFD, IPAddress address, Byte transportProtocol, Byte networkRole )
{
  IPConnection connection = (IPConnection) malloc( sizeof(IPConnectionData) );
  memset( connection, 0, sizeof(IPConnectionData) );
  
  connection->socket = AddSocketPoller( socketFD );
  
  memcpy( &(connection->addressData), address, sizeof(IPAddressData) );
  
  connection->clientsList = NULL;
  connection->remotesCount = 0;
  
  connection->readQueue = TSQ_Create( QUEUE_MAX_ITEMS, IPC_MAX_MESSAGE_LENGTH );
  connection->writeQueue = TSQ_Create( QUEUE_MAX_ITEMS, IPC_MAX_MESSAGE_LENGTH );
  
  if( networkRole == IPC_SERVER ) // Server role connection
  {
    connection->ref_ReceiveMessage = ( transportProtocol == IPC_TCP ) ? ReceiveTCPServerMessages : ReceiveUDPServerMessages;
    connection->ref_SendMessage = ( transportProtocol == IPC_TCP ) ? SendTCPServerMessages : SendUDPServerMessages;
    if( transportProtocol == IPC_UDP && IS_IP_MULTICAST_ADDRESS( address ) ) connection->ref_SendMessage = SendUDPClientMessage;
    connection->ref_Close = ( transportProtocol == IPC_TCP ) ? CloseTCPServer : CloseUDPServer;
  }
  else
  { 
    //connection->address->sin6_family = AF_INET6;
    connection->ref_ReceiveMessage = ( transportProtocol == IPC_TCP ) ? ReceiveTCPClientMessage : ReceiveUDPClientMessage;
    connection->ref_SendMessage = ( transportProtocol == IPC_TCP ) ? SendTCPClientMessage : SendUDPClientMessage;
    connection->ref_Close = ( transportProtocol == IPC_TCP ) ? CloseTCPClient : CloseUDPClient;
  }
  
  return connection;
}

IPAddress LoadAddressInfo( const char* host, const char* port, Byte networkRole )
{
  static IPAddressData addressData;
  
  #ifdef WIN32
  static WSADATA wsa;
  if( wsa.wVersion == 0 )
  {
    if( WSAStartup( MAKEWORD( 2, 2 ), &wsa ) != 0 )
    {
      fprintf( stderr, "%s: error initialiasing windows sockets: code: %d\n", __func__, WSAGetLastError() );
      return NULL;
    }
    
    fprintf( stderr, "%s: initialiasing windows sockets version: %d\n", __func__, wsa.wVersion );
  }
  #endif
  
  #ifndef IP_NETWORK_LEGACY
  struct addrinfo hints = { .ai_family = AF_UNSPEC }; // IPv6 (AF_INET6) or IPv4 (AF_INET) address
  struct addrinfo* hostsInfoList;
  struct addrinfo* hostInfo = NULL;
                        
  if( host == NULL )
  {
    if( networkRole == IPC_SERVER )
    {
      hints.ai_flags |= AI_PASSIVE;                   // Set address for me
      hints.ai_family = AF_INET6;                     // IPv6 address
    }
    else // if( networkRole == IPC_CLIENT )
      return NULL;
  }
  
  int errorCode = 0;
  if( (errorCode = getaddrinfo( host, port, &hints, &hostsInfoList )) != 0 )
  {
    fprintf( stderr, "getaddrinfo: error reading host info: %s", gai_strerror( errorCode ) );
    return NULL;
  }
  
  memcpy( &addressData, hostInfo->ai_addr, hostInfo->ai_addrlen );
  
  freeaddrinfo( hostsInfoList ); // Don't need this struct anymore
  
  if( hostInfo == NULL ) return (IPAddress) NULL;
  #else
  addressData.sin_family = AF_INET;   // IPv4 address
  uint16_t portNumber = (uint16_t) strtoul( port, NULL, 0 );
  addressData.sin_port = htons( portNumber );
  if( host == NULL ) addressData.sin_addr.s_addr = INADDR_ANY;
  else if( strcmp( host, "255.255.255.255" ) == 0 ) addressData.sin_addr.s_addr = INADDR_BROADCAST; 
  else if ( (addressData.sin_addr.s_addr = inet_addr( host )) == INADDR_NONE ) return NULL;
  #endif
  
  return (IPAddress) &addressData;
}

int CreateSocket( uint8_t protocol, IPAddress address )
{
  int socketType, transportProtocol;
  
  if( protocol == IPC_TCP ) 
  {
    socketType = SOCK_STREAM;
    transportProtocol = IPPROTO_TCP;
  }
  else if( protocol == IPC_UDP ) 
  {
    socketType = SOCK_DGRAM;
    transportProtocol = IPPROTO_UDP;
  }
  else
  {
    return INVALID_SOCKET;
  }
  
  // Create IP socket
  int socketFD = socket( address->sa_family, socketType, transportProtocol );
  if( socketFD == INVALID_SOCKET )
    fprintf( stderr, "socket: failed opening %s %s socket", ( protocol == IPC_TCP ) ? "TCP" : "UDP", ( address->sa_family == AF_INET6 ) ? "IPv6" : "IPv4" );                                                              
  
  return socketFD;
}

bool SetSocketConfig( int socketFD )
{
  #ifdef WIN32
  u_long nonBlocking = 1;
  if( ioctlsocket( socketFD, FIONBIO, &nonBlocking ) == SOCKET_ERROR )
  #else
  if( fcntl( socketFD, F_SETFL, O_NONBLOCK ) == SOCKET_ERROR )
  #endif
  {
    fprintf( stderr, "failure setting socket %d to non-blocking state", socketFD );
    close( socketFD );
    return false;
  }
  
  int reuseAddress = 1; // Allow sockets to be binded to the same local port
  if( setsockopt( socketFD, SOL_SOCKET, SO_REUSEADDR, (const char*) &reuseAddress, sizeof(reuseAddress) ) == SOCKET_ERROR ) 
  {
    fprintf( stderr, "setsockopt: failed setting socket %d option SO_REUSEADDR", socketFD );
    close( socketFD );
    return NULL;
  }
  
  return true;
}

bool BindServerSocket( int socketFD, IPAddress address )
{
  if( address->sa_family == AF_INET6 )
  {
    int ipv6Only = 0; // Let IPV6 servers accept IPV4 clients
    if( setsockopt( socketFD, IPPROTO_IPV6, IPV6_V6ONLY, (const char*) &ipv6Only, sizeof(ipv6Only) ) == SOCKET_ERROR )
    {
      fprintf( stderr, "setsockopt: failed setting socket %d option IPV6_V6ONLY", socketFD );
      close( socketFD );
      return false;
    }
  }
  
  // Bind server socket to the given local address
  size_t addressLength = ( address->sa_family == AF_INET6 ) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
  if( bind( socketFD, address, addressLength ) == SOCKET_ERROR )
  {
    fprintf( stderr, "bind: failed on binding socket %d", socketFD );
    close( socketFD );
    return false;
  }
  
  return true;
}

bool BindTCPServerSocket( int socketFD, IPAddress address )
{
  const size_t QUEUE_SIZE = 20;
  
  if( !BindServerSocket( socketFD, address ) ) return false;
  
  // Set server socket to listen to remote connections
  if( listen( socketFD, QUEUE_SIZE ) == SOCKET_ERROR )
  {
    fprintf( stderr, "listen: failed listening on socket %d", socketFD );
    close( socketFD );
    return false;
  }
  
  return true;
}

bool BindUDPServerSocket( int socketFD, IPAddress address )
{
  if( !BindServerSocket( socketFD, address ) ) return false;
  
  #ifndef IP_NETWORK_LEGACY
  int multicastTTL = 255; // Set TTL of multicast packet
  if( address->sa_family == AF_INET6 )
  {
    if( setsockopt( socketFD, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (const char*) &multicastTTL, sizeof(multicastTTL)) != 0 ) 
    {
      fprintf( stderr, "setsockopt: failed setting socket %d option IPV6_MULTICAST_HOPS", socketFD );
      close( socketFD );
      return false;
    }
    unsigned int interfaceIndex = 0; // 0 means default interface
    if( setsockopt( socketFD, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char*) &interfaceIndex, sizeof(interfaceIndex)) != 0 ) 
    {
      fprintf( stderr, "setsockopt: failed setting socket %d option IPV6_MULTICAST_IF", socketFD );
      close( socketFD );
      return false;
    }
  }
  else //if( address->sa_family == AF_INET )
  {
    if( setsockopt( socketFD, IPPROTO_IP, IP_MULTICAST_TTL, (const char*) &multicastTTL, sizeof(multicastTTL)) != 0 ) 
    {
      fprintf( stderr, "setsockopt: failed setting socket %d option IP_MULTICAST_TTL", socketFD );
      close( socketFD );
      return false;
    }
    in_addr_t interface = htonl( INADDR_ANY );
	  if( setsockopt( socketFD, IPPROTO_IP, IP_MULTICAST_IF, (const char*) &interface, sizeof(interface)) != 0 ) 
    {
      fprintf( stderr, "setsockopt: failed setting socket %d option IP_MULTICAST_IF", socketFD );
      close( socketFD );
      return false;
    }
  }
  #else
  int broadcast = 1; // Enable broadcast for IPv4 connections
  if( setsockopt( socketFD, SOL_SOCKET, SO_BROADCAST, (const char*) &broadcast, sizeof(broadcast) ) == SOCKET_ERROR )
  {
    fprintf( stderr, "setsockopt: failed setting socket %d option SO_BROADCAST", socketFD );
    close( socketFD );
    return false;
  }
  #endif
  
  return true;
}

bool ConnectTCPClientSocket( int socketFD, IPAddress address )
{
  // Connect TCP client socket to given remote address
  size_t addressLength = ( address->sa_family == AF_INET6 ) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
  if( connect( socketFD, address, addressLength ) == SOCKET_ERROR )
  {
    fprintf( stderr, "connect: failed on connecting socket %d to remote address", socketFD );
    close( socketFD );
    return false;
  }
  
  return true;
}

bool ConnectUDPClientSocket( int socketFD, IPAddress address )
{
  // Bind UDP client socket to available local address
  static struct sockaddr_storage localAddress;
  localAddress.ss_family = address->sa_family;
  if( bind( socketFD, (struct sockaddr*) &localAddress, sizeof(localAddress) ) == SOCKET_ERROR )
  {
    fprintf( stderr, "bind: failed on binding socket %d to arbitrary local port", socketFD );
    close( socketFD );
    return false;
  }
  
  #ifndef IP_NETWORK_LEGACY
  // Joint the multicast group
  if( address->sa_family == AF_INET6 )
  {
    if( IS_IPV6_MULTICAST_ADDRESS( address ) )
    {
      struct ipv6_mreq multicastRequest;  // Specify the multicast group
      memcpy( &multicastRequest.ipv6mr_multiaddr, &(((struct sockaddr_in6*) address)->sin6_addr), sizeof(multicastRequest.ipv6mr_multiaddr) );

      multicastRequest.ipv6mr_interface = 0; // Accept multicast from any interface

      // Join the multicast address
      if ( setsockopt( socketFD, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*) &multicastRequest, sizeof(multicastRequest) ) != 0 ) 
      {
        fprintf( stderr, "setsockopt: failed setting socket %d option IPV6_ADD_MEMBERSHIP", socketFD );
        close( socketFD );
        return false;
      }
    }
  }
  else //if( address->sa_family == AF_INET )
  {
    if( IS_IPV4_MULTICAST_ADDRESS( address ) )
    {
      struct ip_mreq multicastRequest;  // Specify the multicast group
      memcpy( &(multicastRequest.imr_multiaddr), &(((struct sockaddr_in*) address)->sin_addr), sizeof(multicastRequest.imr_multiaddr) );

      multicastRequest.imr_interface.s_addr = htonl( INADDR_ANY ); // Accept multicast from any interface

      // Join the multicast address
      if( setsockopt( socketFD, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &multicastRequest, sizeof(multicastRequest)) != 0 ) 
      {
        fprintf( stderr, "setsockopt: failed setting socket %d option IP_ADD_MEMBERSHIP", socketFD );
        close( socketFD );
        return false;
      }
    }
  }
  #endif
  
  return true;
}

// Generic method for opening a new socket and providing a corresponding IPConnection structure for use
IPCBaseConnection IP_OpenConnection( Byte connectionType, const char* host, uint16_t port )
{
  const Byte TRANSPORT_MASK = 0xF0, ROLE_MASK = 0x0F;
  static char portString[ PORT_LENGTH ];
  
  // Assure that the port number is in the Dynamic/Private range (49152-65535)
  if( port < 49152 /*|| port > 65535*/ )
  {
    fprintf( stderr, "invalid port number value: %u", port );
    return NULL;
  }
  
  sprintf( portString, "%u", port );
  IPAddress address = LoadAddressInfo( host, portString, (connectionType & ROLE_MASK) );
  if( address == NULL ) return NULL;
  
  Socket socketFD = CreateSocket( (connectionType & TRANSPORT_MASK), address );
  if( socketFD == INVALID_SOCKET ) return NULL;
  
  if( !SetSocketConfig( socketFD ) ) return NULL;
  
  switch( connectionType )
  {
    case( IPC_TCP | IPC_SERVER ): if( !BindTCPServerSocket( socketFD, address ) ) return NULL;
      break;
    case( IPC_UDP | IPC_SERVER ): if( !BindUDPServerSocket( socketFD, address ) ) return NULL;
      break;
    case( IPC_TCP | IPC_CLIENT ): if( !ConnectTCPClientSocket( socketFD, address ) ) return NULL;
      break;
    case( IPC_UDP | IPC_CLIENT ): if( !ConnectUDPClientSocket( socketFD, address ) ) return NULL;
      break;
    default: fprintf( stderr, "invalid connection type: %x", connectionType );
      return NULL;
  } 
  
  // Build the IPConnection structure
  IPConnection newConnection =  AddConnection( socketFD, address, (connectionType & TRANSPORT_MASK), (connectionType & ROLE_MASK) );
  
  if( newConnection != NULL )
  {
    globalConnectionsList = (IPConnection*) realloc( globalConnectionsList, (activeConnectionsCount + 1 ) * sizeof(IPConnection) );
    globalConnectionsList[ activeConnectionsCount ] = newConnection;
    if( activeConnectionsCount == 0 )
    {
      globalReadThread = Thread_Start( AsyncReadQueues, NULL, THREAD_JOINABLE );
      globalWriteThread = Thread_Start( AsyncWriteQueues, NULL, THREAD_JOINABLE );
      activeConnectionsCount++;
    }
  }
  
  return (IPCBaseConnection) newConnection;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                                     ASYNCRONOUS UPDATE                                          /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

// Loop of message reading (storing in queue) to be called asyncronously for client/server connections
static void* AsyncReadQueues( void* args )
{
  isNetworkRunning = true;
  
  while( isNetworkRunning )
  { 
    // Blocking call
    #ifndef IP_NETWORK_LEGACY
    int eventsNumber = poll( polledSocketsList, polledSocketsNumber, EVENT_WAIT_TIME_MS );
    #else
    struct timeval waitTime = { .tv_sec = EVENT_WAIT_TIME_MS / 1000, .tv_usec = ( EVENT_WAIT_TIME_MS % 1000 ) * 1000 };
    activeSocketsSet = polledSocketsSet;
    int eventsNumber = select( polledSocketsNumber, &activeSocketsSet, NULL, NULL, &waitTime );
    #endif
    if( eventsNumber == SOCKET_ERROR ) fprintf( stderr, "select: error waiting for events on %lu FDs", polledSocketsNumber );
    
    if( eventsNumber > 0 ) 
    {
      for( size_t connectionIndex = 0; connectionIndex < activeConnectionsCount; connectionIndex++ )
      {
        IPConnection connection = globalConnectionsList[ connectionIndex ];
        if( connection == NULL ) continue;
        
        connection->ref_ReceiveMessage( connection );
      }
    }
  }
  
  return NULL;
}

// Loop of message writing (removing in order from queue) to be called asyncronously for client connections
static void* AsyncWriteQueues( void* args )
{
  Message messageOut;
  
  isNetworkRunning = true;
  
  while( isNetworkRunning )
  {
    for( size_t connectionIndex = 0; connectionIndex < activeConnectionsCount; connectionIndex++ )
    {
      IPConnection connection = globalConnectionsList[ connectionIndex ];
      if( connection == NULL ) continue;
      
      // Do not proceed if queue is empty
      if( TSQ_GetItemsCount( connection->writeQueue ) == 0 ) continue;
      
      memset( &(messageOut), 0, sizeof(Message) );
      
      TSQ_Dequeue( connection->writeQueue, (void*) &messageOut, TSQUEUE_WAIT );
  
      connection->ref_SendMessage( connection, (const Byte*) messageOut );
    }
    
#ifdef _WIN32
    Sleep( 1000 );
#else
    usleep( 1000*1000 );  /* sleep for 1000 milliSeconds */
#endif
  }
  
  return NULL;//(void*) 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                                      SYNCRONOUS UPDATE                                          /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

// Get (and remove) message from the beginning (oldest) of the given index corresponding read queue
// Method to be called from the main thread
bool IP_ReceiveMessage( IPCBaseConnection ref_connection, Byte* message )
{  
  if( ref_connection == NULL ) return false;
  IPConnection connection = (IPConnection) ref_connection;
  //if( bsearch( connection, globalConnectionsList, activeConnectionsCount, sizeof(IPConnection), CompareConnections ) == NULL ) return false;
    
  if( TSQ_GetItemsCount( connection->readQueue ) == 0 ) return false;

  TSQ_Dequeue( connection->readQueue, (void*) message, TSQUEUE_WAIT );
  
  return true;
}

bool IP_SendMessage( IPCBaseConnection ref_connection, const Byte* message )
{  
  if( ref_connection == NULL ) return false;
  IPConnection connection = (IPConnection) ref_connection;
  //if( bsearch( connection, globalConnectionsList, activeConnectionsCount, sizeof(IPConnection), CompareConnections ) == NULL ) return false;
  
  if( TSQ_GetItemsCount( connection->writeQueue ) >= QUEUE_MAX_ITEMS )
    fprintf( stderr, "connection index %p write queue is full", connection );
  
  TSQ_Enqueue( connection->writeQueue, (void*) message, TSQUEUE_NOWAIT );
  
  return true;
}

// Verify available incoming messages for the given connection, preventing unnecessary blocking calls (for syncronous networking)
int WaitEvents( unsigned int milliseconds )
{
  #ifndef IP_NETWORK_LEGACY
  int eventsNumber = poll( polledSocketsList, polledSocketsNumber, milliseconds );
  #else
  struct timeval waitTime = { .tv_sec = milliseconds / 1000, .tv_usec = ( milliseconds % 1000 ) * 1000 };
  activeSocketsSet = polledSocketsSet;
  int eventsNumber = select( polledSocketsNumber, &activeSocketsSet, NULL, NULL, &waitTime );
  #endif
  if( eventsNumber == SOCKET_ERROR ) fprintf( stderr, "select: error waiting for events on %lu FDs", polledSocketsNumber );
  
  return eventsNumber;
}

bool IsDataAvailable( SocketPoller* socket )
{ 
  #ifndef IP_NETWORK_LEGACY
  if( socket->revents & POLLRDNORM ) return true;
  else if( socket->revents & POLLRDBAND ) return true;
  #else
  if( FD_ISSET( socket->fd, &activeSocketsSet ) ) return true;
  #endif
  
  return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////                      SPECIFIC TRANSPORT/ROLE COMMUNICATION                    /////
/////////////////////////////////////////////////////////////////////////////////////////

static void RemoveSocket( Socket );

// Try to receive incoming message from the given TCP client connection and store it on its buffer
static void ReceiveTCPClientMessage( IPConnection connection )
{
  static Message messageIn;
  
  if( connection->socket->fd == INVALID_SOCKET ) return;

  //if( TSQ_GetItemsCount( connection->readQueue ) >= QUEUE_MAX_ITEMS ) return;
  
  if( IsDataAvailable( connection->socket ) == false ) return;
  
  // Blocks until there is something to be read in the socket
  int bytesReceived = recv( connection->socket->fd, (void*) messageIn, IPC_MAX_MESSAGE_LENGTH, 0 );

  if( bytesReceived == SOCKET_ERROR )
  {
    fprintf( stderr, "recv: error reading from socket %d", connection->socket->fd );
    //connection->socket->fd = INVALID_SOCKET;
    //RemoveSocket( connection->socket->fd );
    return;
  }
  else if( bytesReceived == 0 )
  {
    fprintf( stderr, "recv: remote connection with socket %d closed", connection->socket->fd );
    //connection->socket->fd = INVALID_SOCKET;
    RemoveSocket( connection->socket->fd );
    return;
  }
  
  TSQ_Enqueue( connection->readQueue, &(messageIn), TSQUEUE_WAIT );
}

// Send given message through the given TCP connection
static void SendTCPClientMessage( IPConnection connection, const Byte* message )
{
  if( send( connection->socket->fd, (void*) message, IPC_MAX_MESSAGE_LENGTH, 0 ) == SOCKET_ERROR )
    fprintf( stderr, "send: error writing to socket %d", connection->socket->fd );
}

// Try to receive incoming message from the given UDP client connection and store it on its buffer
static void ReceiveUDPClientMessage( IPConnection connection )
{
  static Message messageIn;
  
  if( IsDataAvailable( connection->socket ) == false ) return;
  
  // Blocks until there is something to be read in the socket
  IPAddressData address;
  socklen_t addressLength = sizeof(IPAddressData);
  if( recvfrom( connection->socket->fd, (void*) messageIn, IPC_MAX_MESSAGE_LENGTH, 0, (IPAddress) &(address), &addressLength ) == SOCKET_ERROR )
  {
    //fprintf( stderr, "recvfrom: error reading from socket %d", connection->socket->fd );
    return;
  }
    
  TSQ_Enqueue( connection->readQueue, &(messageIn), TSQUEUE_WAIT );
}

// Send given message through the given UDP connection
static void SendUDPClientMessage( IPConnection connection, const Byte* message )
{
  if( sendto( connection->socket->fd, message, IPC_MAX_MESSAGE_LENGTH, 0, (IPAddress) &(connection->addressData), sizeof(IPAddressData) ) == SOCKET_ERROR )
    fprintf( stderr, "sendto: error writing to socket %d", connection->socket->fd );
}

// Send given message to all the clients of the given TCP server connection
static void SendTCPServerMessages( IPConnection connection, const Byte* message )
{
  for( size_t clientIndex = 0; clientIndex < connection->remotesCount; clientIndex++ )
  {
    SocketPoller* clientSocket = (SocketPoller*) connection->clientsList[ clientIndex ];
    if( send( clientSocket->fd, message, IPC_MAX_MESSAGE_LENGTH, 0 ) == SOCKET_ERROR )
      fprintf( stderr, "send: error writing to socket %d", clientSocket->fd );
  }
}

// Send given message to all the clients of the given server connection
static void SendUDPServerMessages( IPConnection connection, const Byte* message )
{
  for( size_t clientIndex = 0; clientIndex < connection->remotesCount; clientIndex++ )
  {
    socklen_t addressLength = sizeof(IPAddressData);
    IPAddress clientAddress = (IPAddress) &(connection->addressesList[ clientIndex ]);
    if( sendto( connection->socket->fd, (void*) message, IPC_MAX_MESSAGE_LENGTH, 0, clientAddress, addressLength ) == SOCKET_ERROR )
      fprintf( stderr, "send: error writing to socket %d", connection->socket->fd );
  }
}

// Waits for a remote connection to be added to the client list of the given TCP server connection
static void ReceiveTCPServerMessages( IPConnection server )
{ 
  static Message messageIn;
  
  if( IsDataAvailable( server->socket ) )
  {
    Socket clientSocketFD = accept( server->socket->fd, NULL, NULL );
    if( clientSocketFD == INVALID_SOCKET )
      fprintf( stderr, "accept: failed accepting connection on socket %d\n", server->socket->fd );
    else
    {
      server->clientsList = (SocketPoller**) realloc( server->clientsList, ++server->remotesCount * sizeof(SocketPoller*) );
      server->clientsList[ server->remotesCount - 1 ] = AddSocketPoller( clientSocketFD );
    }
  }
  
  for( size_t clientIndex = 0; clientIndex < server->remotesCount; clientIndex++ )
  {
    SocketPoller* clientSocket = (SocketPoller*) server->clientsList[ clientIndex ];
    if( IsDataAvailable( clientSocket ) )
    {
      int bytesReceived = recv( clientSocket->fd, (void*) messageIn, IPC_MAX_MESSAGE_LENGTH, 0 );
      if( bytesReceived == SOCKET_ERROR ) 
      {
        fprintf( stderr, "recv: error reading from socket %d\n", clientSocket->fd );
        continue;
      }
      else if( bytesReceived == 0 )
      {
        fprintf( stderr, "recv: remote connection with socket %d closed\n", clientSocket->fd );
        clientSocket->fd = INVALID_SOCKET;
        continue;
      }

      TSQ_Enqueue( server->readQueue, &(messageIn), TSQUEUE_WAIT );
    }
  }
}

// Waits for a remote connection to be added to the client list of the given UDP server connection
static void ReceiveUDPServerMessages( IPConnection server )
{
  static Message messageIn;
  
  if( IsDataAvailable( server->socket ) == false ) return;
  
  IPAddressData addressData;
  socklen_t addressLength = sizeof(IPAddressData);
  if( recvfrom( server->socket->fd, (void*) messageIn, IPC_MAX_MESSAGE_LENGTH, 0, (IPAddress) &(addressData), &addressLength ) == SOCKET_ERROR )
  {
    fprintf( stderr, "recvfrom: error reading from socket %d", server->socket->fd );
    return;
  }
  
  TSQ_Enqueue( server->readQueue, &(messageIn), TSQUEUE_WAIT );
  
  // Verify if incoming message belongs to unregistered client (returns default value if not)
  for( size_t clientIndex = 0; clientIndex < server->remotesCount; clientIndex++ )
  {
    if( ARE_EQUAL_IP_ADDRESSES( &(server->addressesList[ clientIndex ]), &(addressData) ) )
      return;
  }
  
  server->addressesList = (IPAddressData*) realloc( server->addressesList, ++server->remotesCount * sizeof(IPAddressData) );
  memcpy( &(server->addressesList[ server->remotesCount - 1 ]), &(addressData), sizeof(IPAddressData) );
}


//////////////////////////////////////////////////////////////////////////////////
/////                               FINALIZING                               /////
//////////////////////////////////////////////////////////////////////////////////

// Handle proper destruction of any given connection type

void RemoveSocket( Socket socketFD )
{
  #ifndef IP_NETWORK_LEGACY
  SocketPoller cmpPoller = { .fd = socketFD };
  SocketPoller* poller = bsearch( &cmpPoller, polledSocketsList, polledSocketsNumber, sizeof(SocketPoller), CompareSockets );
  if( poller != NULL )
  {
    poller->fd = (Socket) 0xFFFF;
    qsort( polledSocketsList, polledSocketsNumber, sizeof(SocketPoller), CompareSockets );
    polledSocketsNumber--;
  }
  #else
  FD_CLR( socketFD, &polledSocketsSet );
  if( socketFD + 1 >= polledSocketsNumber ) polledSocketsNumber = socketFD - 1;
  #endif
  close( socketFD );
}

void CloseTCPServer( IPConnection server )
{
  for( size_t clientIndex = 0; clientIndex < server->remotesCount; clientIndex++ )
    RemoveSocket( server->clientsList[ clientIndex ]->fd );
  shutdown( server->socket->fd, SHUT_RDWR );
  RemoveSocket( server->socket->fd );
  if( server->clientsList != NULL ) free( server->clientsList );
}

void CloseUDPServer( IPConnection server )
{
  // Check number of client connections of a server (also of sharers of a socket for UDP connections)
  RemoveSocket( server->socket->fd );
  if( server->addressesList != NULL ) free( server->addressesList );
}

void CloseTCPClient( IPConnection client )
{
  shutdown( client->socket->fd, SHUT_RDWR );
  RemoveSocket( client->socket->fd );
}

void CloseUDPClient( IPConnection client )
{
  RemoveSocket( client->socket->fd );
}

static int CompareConnections( const void* connection_1, const void* connection_2 )
{
  return ( ((IPConnection) connection_1)->socket - ((IPConnection) connection_2)->socket );
}

void IP_CloseConnection( IPCBaseConnection ref_connection )
{
  if( ref_connection == NULL ) return;
  IPConnection connection = (IPConnection) ref_connection;
  //if( bsearch( connection, globalConnectionsList, activeConnectionsCount, sizeof(IPConnection), CompareConnections ) == NULL ) return;
  
  // Each TCP connection has its own socket, so we can close it without problem. But UDP connections
  // from the same server share the socket, so we need to wait for all of them to be stopped to close the socket
  connection->ref_Close( connection );
  
  connection->socket = (SocketPoller*) 0xFFFF;
  qsort( globalConnectionsList, activeConnectionsCount, sizeof(IPConnection), CompareConnections );
  
  TSQ_Discard( connection->readQueue );
  TSQ_Discard( connection->writeQueue );
  free( connection );
  
  globalConnectionsList = (IPConnection*) realloc( globalConnectionsList, --activeConnectionsCount * sizeof(IPConnection) );
  if( activeConnectionsCount <= 0 )
  {
    Thread_WaitExit( globalReadThread, 5000 );
    Thread_WaitExit( globalWriteThread, 5000 );
    free( globalConnectionsList );
    globalConnectionsList = NULL;
    activeConnectionsCount = 0;
  }
}
