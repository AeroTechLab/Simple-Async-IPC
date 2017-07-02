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

#include "ipc.h"

#include "ipc_base_ip.h"
#include "ipc_base_shm.h"

#include <stdlib.h>
  
  
struct _IPCConnectionData
{
  IPCBaseConnection baseConnection;
  bool (*ref_ReadMessage)( IPCBaseConnection, Byte* message );
  bool (*ref_WriteMessage)( IPCBaseConnection, const Byte* );
  void (*ref_Close)( IPCBaseConnection );
};


IPCConnection IPC_OpenConnection( Byte connectionType, const char* host, uint16_t channel )
{  
  fprintf( stderr, "opening connection\n" );
  
  IPCConnection newConnection = (IPCConnection) malloc( sizeof(IPCConnectionData) );
  
  Byte transport = (connectionType & IPC_TRANSPORT_MASK);
  
  if( transport == IPC_TCP || transport == IPC_UDP )
  {
    fprintf( stderr, "%s://%s:%u", ( transport == IPC_TCP ) ? "tcp" : "udp", ( host != NULL ) ? host : "*", channel );
    newConnection->baseConnection = IP_OpenConnection( connectionType, host, channel );
    newConnection->ref_ReadMessage = IP_ReceiveMessage;
    newConnection->ref_WriteMessage = IP_SendMessage;
    newConnection->ref_Close = IP_CloseConnection;
  }
  else if( transport == IPC_SHM )
  {
    fprintf( stderr, "shm:/dev/shm/%s_%u", ( host != NULL ) ? host : "data", channel );
    newConnection->baseConnection = SHM_OpenMapping( connectionType, host, channel );
    newConnection->ref_ReadMessage = SHM_ReadData;
    newConnection->ref_WriteMessage = SHM_WriteData;
    newConnection->ref_Close = SHM_CloseMapping;    
  }
  
  if( newConnection->baseConnection == NULL )
  {
    free( newConnection );
    return NULL;
  }
  
  return newConnection;
}

bool IPC_ReadMessage( IPCConnection connection, Byte* message )
{
  return connection->ref_ReadMessage( (IPCBaseConnection) connection->baseConnection, message );
}

bool IPC_WriteMessage( IPCConnection connection, const Byte* message )
{
  return connection->ref_WriteMessage( (IPCBaseConnection) connection->baseConnection, message );
}

void IPC_CloseConnection( IPCConnection connection )
{
  connection->ref_Close( (IPCBaseConnection) connection->baseConnection );
}
