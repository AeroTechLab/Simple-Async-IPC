//////////////////////////////////////////////////////////////////////////////////////
//                                                                                  //
//  Copyright (c) 2016-2017 Leonardo Consoni <consoni_2519@hotmail.com>             //
//                                                                                  //
//  This file is part of Async IP Connections.                                      //
//                                                                                  //
//  Async IP Connections is free software: you can redistribute it and/or modify    //
//  it under the terms of the GNU Lesser General Public License as published        //
//  by the Free Software Foundation, either version 3 of the License, or            //
//  (at your option) any later version.                                             //
//                                                                                  //
//  Async IP Connections is distributed in the hope that it will be useful,         //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of                  //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                    //
//  GNU Lesser General Public License for more details.                             //
//                                                                                  //
//  You should have received a copy of the GNU Lesser General Public License        //
//  along with Async IP Connections. If not, see <http://www.gnu.org/licenses/>.    //
//                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////


/// @file ipc_definitions.h
/// @brief Asynchronous IP network connection abstraction.
///
/// Library that combines threading utilities with the socket connection library 
/// to provide asyncronous and thread-safe network communication methods.

#ifndef IPC_DEFINITIONS_H
#define IPC_DEFINITIONS_H


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define IPC_MAX_MESSAGE_LENGTH 512      ///< Maximum allowed length of messages transmitted through an IP connection

typedef uint8_t Byte;

typedef Byte Message[ IPC_MAX_MESSAGE_LENGTH ];

typedef void* IPCBaseConnection;

#define IPC_INVALID_CONNECTION NULL      ///< Connection identifier to be returned on initialization errors

#define IPC_SERVER 0x01                  ///< IPC server connection creation flag
#define IPC_CLIENT 0x02                  ///< IPC client connection creation flag

#define IPC_TCP 0x10                     ///< IP TCP (stream) connection creation flag
#define IPC_UDP 0x20                     ///< IP UDP (datagram) connection creation flag
#define IPC_SHM 0x30                     ///< Shared Memory connection creation flag

#define IPC_TRANSPORT_MASK 0xF0
#define IPC_ROLE_MASK  0x0F

#endif // IPC_DEFINITIONS_H 
