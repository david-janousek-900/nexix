#ifndef NEXIX_NET_CONFIG_H
#define NEXIX_NET_CONFIG_H
#include "net/route.h"
#include <nexix/socket.h>

/* Interface and route configuration behind the NX_SIOC* socket ioctls (task
 * 427). Worker context; records use the user ABI of <nexix/socket.h> and
 * results are 0 or a negative native errno. Privilege is the caller's check.
 * An interface change is applied whole: if bringing the interface up fails,
 * its previous address is restored before the error is returned. */
int nx_net_config_ifinfo(struct nx_ifinfo *);
int nx_net_config_apply(const struct nx_ifconfig *);
int nx_net_config_get_route(struct nx_route *);
int nx_net_config_set_route(const struct nx_route *);
int nx_net_config_lookup(struct nx_route_lookup *);
#endif
