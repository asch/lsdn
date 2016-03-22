#ifndef _LSDN_NETWORK_H
#define _LSDN_NETWORK_H

/**
 * @brief Logical representation of network.
 *
 * Networks consist of nodes (logical network appliances), their ports
 * and connection between those ports.
 */
struct lsdn_network;

/**
 * @param netname
 *      Network names will be used as a prefix for network interfaces and other
 *      named kernel objects created by LSDN. It should be unique.
 */
struct lsdn_network *lsdn_network_new(const char* netname);
void lsdn_network_create(struct lsdn_network* network);
void lsdn_network_free(struct lsdn_network *network);

#endif
