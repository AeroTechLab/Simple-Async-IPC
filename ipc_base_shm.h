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
                        

#ifndef IPC_BASE_SHM_H
#define IPC_BASE_SHM_H


#include <stdint.h>
#include <stdbool.h>


void* SHM_OpenMapping( const char* dirPath, const char* baseName, const char* inSuffix, const char* outSuffix );

void SHM_CloseMapping( void* mapping );
 
bool SHM_ReadData( void* mapping, uint8_t* message );
                                                                              
bool SHM_WriteData( void* mapping, const uint8_t* message );


#endif // IPC_BASE_SHM_H
