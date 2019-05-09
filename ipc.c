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
    fprintf( stderr, "ip://%s:%s", ( host != NULL ) ? host : "*", channel );
    uint8_t connectionType = ( mode == IPC_REQ || mode == IPC_REP ) ? IP_TCP : IP_UDP;
    if( mode == IPC_PUB || mode == IPC_REP || mode == IPC_SERVER ) connectionType |= IP_SERVER;
    if( mode == IPC_SUB || mode == IPC_REQ || mode == IPC_CLIENT ) connectionType |= IP_CLIENT;
    newConnection->baseConnection = IP_OpenConnection( mode, host, channel );
    newConnection->ref_ReadMessage = IP_ReceiveMessage;
    newConnection->ref_WriteMessage = IP_SendMessage;
    newConnection->ref_Close = IP_CloseConnection;
  }
  else // SHM host
  {
    fprintf( stderr, "shm://%s/%s", host, channel );
    enum MapType mappingType = SHM_CLIENT;
    if( mode == IPC_PUB || mode == IPC_REP || mode == IPC_SERVER ) mappingType = SHM_SERVER;
    newConnection->baseConnection = SHM_OpenMapping( mappingType, host, channel );
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
