#ifndef SOFTUIWARP_H
#define SOFTUIWARP_H

#include <stdlib.h>

#include <infiniband/verbs.h>

ibv_device *suiw_get_ibv_device();

bool suiw_is_device_softuiwarp(ibv_device*);

ibv_context *suiw_open_device(ibv_device*);

int suiw_close_device(ibv_context*);

#endif // SOFTUIWARP_H
