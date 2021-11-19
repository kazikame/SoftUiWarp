
#include "softuiwarp.h"

/* Internal state. */

static ibv_device *suiw_ibv_device;

/* Internal functions. */

static ibv_device *create_fake_device() {
    const char *kernel_dev_name = "enp94s0f0"; // just hard-code this for now...
    const char *dev_name = "softuiwarp0";
    const char *dev_path = "";
    const char *ibdev_path = "";

    ibv_device *fake_device = (ibv_device*) malloc(sizeof(struct ibv_device));
    memset(fake_device, 0, sizeof(struct ibv_device));
    fake_device->transport_type = IBV_TRANSPORT_IWARP;
    strcpy(fake_device->name, kernel_dev_name);
    strcpy(fake_device->dev_name, dev_name);
    strcpy(fake_device->dev_path, dev_path);
    strcpy(fake_device->ibdev_path, ibdev_path);
}

/* Implementation of public API. */

ibv_device *suiw_get_ibv_device() {
    return suiw_ibv_device;
}

/* Runs when the library is loaded. */
__attribute__((constructor)) static void init(void) {
    suiw_ibv_device = create_fake_device();
}

/* Runs when the library is unloaded. */
__attribute__((destructor)) static void fini(void) {
    free(suiw_ibv_device);
}
