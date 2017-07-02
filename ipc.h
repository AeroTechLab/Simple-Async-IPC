////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (c) 2016-2017 Leonardo Consoni <consoni_2519@hotmail.com>       //
//                                                                            //
//  This file is part of RobRehabSystem.                                      //
//                                                                            //
//  RobRehabSystem is free software: you can redistribute it and/or modify    //
//  it under the terms of the GNU Lesser General Public License as published  //
//  by the Free Software Foundation, either version 3 of the License, or      //
//  (at your option) any later version.                                       //
//                                                                            //
//  RobRehabSystem is distributed in the hope that it will be useful,         //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of            //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              //
//  GNU Lesser General Public License for more details.                       //
//                                                                            //
//  You should have received a copy of the GNU Lesser General Public License  //
//  along with RobRehabSystem. If not, see <http://www.gnu.org/licenses/>.    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////


/// @file ipc.h
/// @brief Asynchronous IP network connection abstraction.
///
/// Library that combines threading utilities with the socket connection library 
/// to provide asyncronous and thread-safe network communication methods.           

#ifndef IPC_H
#define IPC_H


#include "ipc_definitions.h"

/// Structure that stores data of a single IP connection
typedef struct _IPCConnectionData IPCConnectionData;
/// Opaque type to reference encapsulated IP connection structure
typedef IPCConnectionData* IPCConnection;


/// @brief Creates a new IP connection structure (with defined properties) and add it to the asynchronous connections list                              
/// @param[in] flags configuration variable defining connection as client or server, TCP or UDP                                   
/// @param[in] host IPv4 or IPv6 host string (NULL for server listening on any local address)    
/// @param[in] channel IPv4 or IPv6 host string (NULL for server listening on any local address)     
/// @return unique generic identifier for newly created connection (IPC_INVALID_CONNECTION on error) 
IPCConnection IPC_OpenConnection( Byte flags, const char* host, uint16_t channel );

/// @brief Handle termination of connection corresponding to given identifier                             
/// @param[in] connection connection identifier
void IPC_CloseConnection( IPCConnection connection );

/// @brief Pops first (oldest) queued message from read queue of client connection corresponding to given identifier                      
/// @param[in] connection client connection identifier  
/// @param[in] message message byte array pointer 
/// @return pointer to message string, overwritten on next call to ReadMessage() (NULL on error or no message available)  
bool IPC_ReadMessage( IPCConnection connection, Byte* message );
                                                                           
/// @brief Pushes given message string to write queue of connection corresponding to given identifier                                                
/// @param[in] connection connection identifier   
/// @param[in] message message byte array pointer  
/// @return true on success, false on error  
bool IPC_WriteMessage( IPCConnection connection, const Byte* message );


#endif // IPC_H 
