/*
 * MySQL BIND Finding IP Information
 *
 * Copyright (C) 2012-2015 Fairy zhang <fairyling.zhang@gmail.com>.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 *
 * $Id: mysqlip.h,v 1.1.1.1 2012/10/17 16:15:40 fairy Exp $ 
 */

#include <mysql.h>

#include <isc/types.h>
#include <isc/net.h>
#include <isc/netaddr.h>
#include <isc/sockaddr.h>
#include <isc/refcount.h>
#include <isccfg/cfg.h>

#include <dns/name.h>
#include <dns/result.h>

/*Database Informations for IP locating */
typedef struct dns_mysqlip
{
    MYSQL conn;
    isc_uint32_t port;
    char database[30];
    char host[30];
    char user[30];
    char passwd[30];
} dns_mysqlip_t;

typedef struct mysqlip_prefix {
    unsigned int family;	/* AF_INET | AF_INET6, or AF_UNSPEC for "any" */
    unsigned int bitlen;	/* 0 for "any" */
    isc_refcount_t refcount;
    union {
		struct in_addr sin;
		struct in6_addr sin6;
    } add;
} mysqlip_prefix_t;
#define RADIX_MAXBITS 128

#define mysqlip_prefix_tochar(prefix) ((char *)&(prefix)->add.sin)
#define mysqlip_prefix_touchar(prefix) ((u_char *)&(prefix)->add.sin)

#define BIT_TEST(f, b)  ((f) & (b))

/*structures for IP searching*/
#define MYSQLIP_IS6(family) ((family) == AF_INET6 ? 1 : 0)

typedef struct mysqlip_ipinfo {
	isc_int16_t isp;			/* isp information */
	isc_int16_t location;		/* location information */
	isc_int16_t idc;			/* IDC information */
} mysqlip_ipinfo_t;

struct mysqlip_radix_node;

typedef struct mysqlip_radix_node {
	isc_uint32_t bit;			/* bit length of the prefix */
	mysqlip_prefix_t *prefix;		/* who we are in radix tree */
	struct mysqlip_radix_node *l, *r;	/* left and right children */
	struct mysqlip_radix_node *parent;	/* may be used */
	mysqlip_ipinfo_t *ipinfo;	/* IP information structure */
} mysqlip_radix_node_t;

typedef struct mysqlip_radix_tree {
	isc_mem_t		*mctx;
	mysqlip_radix_node_t 	*head;
	isc_uint32_t		maxbits;	/* for IP, 32 bit addresses */
	int num_active_node;			/* for debugging purposes */
	int num_added_node;			/* total number of nodes */
	char				domain_name[255]; /*the name of ip table for this tree*/
	isc_uint32_t		domain_hash;
	mysqlip_radix_node_t *default_node;
} mysqlip_radix_tree_t;

isc_result_t mysqlip_set_info(isc_mem_t *mctx, const cfg_obj_t *config, dns_mysqlip_t **ip_info) ;

isc_result_t mysqlip_init_radix_array(isc_mem_t *mctx, dns_mysqlip_t *ipinfo);

isc_result_t mysqldb_clear_radix_tree_array(isc_mem_t *mctx, mysqlip_radix_tree_t **tree_array, isc_uint32_t tree_num);

/* locating user IP information
 * args  - qname : Domain Name for resolving
 * 			 ip : IP Address structure of user local DNS
 * 		  isp_id : ISP Info of user local DNS , which is for outputing
 * 	  location_id : LOCATION Info of user local DNS , which is for outputing
 * 	 	  idc_id : IDC Info of user local DNS , which is written by administrator for outputing
 * return - result : the value of ISC_R_SUCCESS is meaning successful.
*/
isc_result_t
mysqlip_radix_search(const char *qname, const char* zone, const isc_sockaddr_t *ip,
									isc_int16_t *isp_id, isc_int16_t *location_id, isc_int16_t *idc_id);


void ns_mysqlip_log(const isc_sockaddr_t *sip, isc_logcategory_t *category,
	   					isc_logmodule_t *module, int level, const char *fmt, ...);

