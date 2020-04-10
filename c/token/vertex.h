//
//  vertex.h
//  ttt
//
//  Created by Kha Do on 12/26/19.
//  Copyright © 2019 Kha Do. All rights reserved.
//

#ifndef vertex_h
#define vertex_h

#include <stdint.h>
#include <stdlib.h>
#define WASI_EXPORT extern "C"
#define ADDRESS_SIZE 35
typedef uint8_t address[ADDRESS_SIZE];
typedef int Event;
extern size_t chain_storage_size_get(const void *, size_t);
extern int chain_storage_get(const void *, size_t, void *);
extern int chain_storage_set(const void *, size_t, const void *, size_t);
extern void chain_get_caller(address);
extern void chain_get_creator(address);

#endif /* vertex_h */
