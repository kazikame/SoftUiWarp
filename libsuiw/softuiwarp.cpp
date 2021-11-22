
#include <stdlib.h>
#include <stdio.h>

#include "softuiwarp.h"

/* Internal state. */

static ibv_device *suiw_ibv_device;

/* Internal functions. */

static const char *kernel_dev_name = "enp94s0f0"; // just hard-code this for now...
static const char *dev_name = "softuiwarp0";
static const char *dev_path = "/dev/zero";
static const char *ibdev_path = "/dev/zero";

static ibv_device *create_fake_device() {
    ibv_device *fake_device = (ibv_device*) malloc(sizeof(struct ibv_device));
    memset(fake_device, 0, sizeof(struct ibv_device));
    fake_device->transport_type = IBV_TRANSPORT_IWARP;
    strcpy(fake_device->name, kernel_dev_name);
    strcpy(fake_device->dev_name, dev_name);
    strcpy(fake_device->dev_path, dev_path);
    strcpy(fake_device->ibdev_path, ibdev_path);
    return fake_device;
}

/* Implementation of public API. */

ibv_device *suiw_get_ibv_device() {
    ibv_device *dev = (ibv_device*) malloc(sizeof(struct ibv_device));
    memcpy(dev, suiw_ibv_device, sizeof(struct ibv_device));
    return dev;
}

ibv_context *suiw_open_device(ibv_device *device) {
    if (device == nullptr)
        return nullptr;
    ibv_context *context = (ibv_context*) malloc(sizeof(ibv_context));
    memset(context, 0, sizeof(ibv_context));
    context->device = device;
    // TODO there may be some more initialization to do here
    return context;
}

int suiw_close_device(ibv_context *context) {
    if (context == nullptr)
        return 0;
    free(context);
    return 0;
}

bool suiw_is_device_softuiwarp(ibv_device* device) {
    return device != nullptr && strcmp(device->dev_name, dev_name) == 0;
}

/* Runs when the library is loaded. */
__attribute__((constructor)) static void init(void) {
    printf("Loading libsuiw ...\n");
    suiw_ibv_device = create_fake_device();
}

/* Runs when the library is unloaded. */
__attribute__((destructor)) static void fini(void) {
    printf("Unloading libsuiw ...\n");
    free(suiw_ibv_device);
}
