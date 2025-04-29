//////////////////////////////////////////////////////////////////////////////////////
//                                                                                  //
//  Copyright (c) 2016-2025 Leonardo Consoni <leonardojc@protonmail.com>            //
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
                         

#ifndef IPC_BASE_IP_H
#define IPC_BASE_IP_H

#include <stdint.h>
#include <stdbool.h>

#define IP_SERVER 0x01                  
#define IP_CLIENT 0x02                  

#define IP_TCP 0x10                    
#define IP_UDP 0x20                     


bool IP_IsValidAddress( const char* addressString );

void* IP_OpenConnection( uint8_t connectionType, const char* host, const char* port );

void IP_CloseConnection( void* connection );
 
bool IP_ReceiveMessage( void* connection, uint8_t* message );
                                                                             
bool IP_SendMessage( void* connection, const uint8_t* message );

#endif // IPC_BASE_IP_H
