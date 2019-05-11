//////////////////////////////////////////////////////////////////////////////////////
//                                                                                  //
//  Copyright (c) 2016-2019 Leonardo Consoni <consoni_2519@hotmail.com>             //
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


#include <stdio.h>

#include "interface/ipc.h"

#include "ipc_base_ip.h"
#include "ipc_base_shm.h"

#include <stdlib.h>
  
  
typedef struct _IPCConnectionData
{
  void* baseConnection;
  bool (*ref_ReadMessage)( void*, Byte* message );
  bool (*ref_WriteMessage)( void*, const Byte* );
  void (*ref_Close)( void* );
}
IPCConnectionData;


IPCConnection IPC_OpenConnection( enum IPCMode mode, const char* host, const char* channel )
{  
  fprintf( stderr, "opening connection\n" );
  
  IPCConnectionData* newConnection = (IPCConnectionData*) malloc( sizeof(IPCConnectionData) );
  
  if( IP_IsValidAddress( host ) )
  {
    fprintf( stderr, "ip://%s:%s\n", ( host != NULL ) ? host : "*", ( channel != NULL ) ? channel : "0" );
    uint8_t connectionType = 0;
    if( mode == IPC_REQ ) connectionType = ( IP_TCP | IP_CLIENT );
    else if( mode == IPC_REP ) connectionType = ( IP_TCP | IP_SERVER );
    else if( mode == IPC_SUB || mode == IPC_CLIENT ) connectionType = ( IP_UDP | IP_CLIENT );
    else if( mode == IPC_PUB || mode == IPC_SERVER ) connectionType = ( IP_UDP | IP_SERVER );
    newConnection->baseConnection = IP_OpenConnection( connectionType, host, channel );
    newConnection->ref_ReadMessage = IP_ReceiveMessage;
    newConnection->ref_WriteMessage = IP_SendMessage;
    newConnection->ref_Close = IP_CloseConnection;
  }
  else // SHM host
  {
    fprintf( stderr, "shm://%s/%s\n", host, channel );
    newConnection->baseConnection = NULL;
    if( mode == IPC_REQ ) newConnection->baseConnection = SHM_OpenMapping( host, channel, "rep", "req" );
    else if( mode == IPC_REP ) newConnection->baseConnection = SHM_OpenMapping( host, channel, "req", "rep" );
    else if( mode == IPC_PUB ) newConnection->baseConnection = SHM_OpenMapping( host, channel, "sub", "pub" );
    else if( mode == IPC_SUB ) newConnection->baseConnection = SHM_OpenMapping( host, channel, "pub", "sub" );
    else if( mode == IPC_CLIENT ) newConnection->baseConnection = SHM_OpenMapping( host, channel, "server", "client" );
    else if( mode == IPC_SERVER ) newConnection->baseConnection = SHM_OpenMapping( host, channel, "client", "server" );
    newConnection->ref_ReadMessage = SHM_ReadData;
    newConnection->ref_WriteMessage = SHM_WriteData;
    newConnection->ref_Close = SHM_CloseMapping;    
  }
  
  if( newConnection->baseConnection == NULL )
  {
    free( newConnection );
    return IPC_INVALID_CONNECTION;
  }
  
  return (IPCConnection) newConnection;
}

bool IPC_ReadMessage( IPCConnection ref_connection, Byte* message )
{
  IPCConnectionData* connection = (IPCConnectionData*) ref_connection;
  return connection->ref_ReadMessage( (void*) connection->baseConnection, message );
}

bool IPC_WriteMessage( IPCConnection ref_connection, const Byte* message )
{
  IPCConnectionData* connection = (IPCConnectionData*) ref_connection;
  return connection->ref_WriteMessage( (void*) connection->baseConnection, message );
}

void IPC_CloseConnection( IPCConnection ref_connection )
{
  IPCConnectionData* connection = (IPCConnectionData*) ref_connection;
  connection->ref_Close( (void*) connection->baseConnection );
}
