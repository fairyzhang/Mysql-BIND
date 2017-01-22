/*
 * MySQL BIND Finding IP Information
 *
 * Copyright (C) 2012-2015 Fairy zhang <mysqlbind@fairyzhang.me>.
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

#include <dns/name.h>
#include <dns/result.h>

/*Database Informations for IP locating */
typedef struct dns_mysqlip
{
    MYSQL conn;
    char database[30];
    char host[30];
    char user[30];
    char passwd[30];
} dns_mysqlip_t;


/* locating user IP information
 * args  - qname : Domain Name for resolving
 * 			 ip : IP Address structure of user local DNS
 * 		  isp_id : ISP Info of user local DNS , which is for outputing
 * 	  location_id : LOCATION Info of user local DNS , which is for outputing
 * 	 	  idc_id : IDC Info of user local DNS , which is written by administrator for outputing
 *		  ipinfo : Mysql IP Information of ns_g_server (global)
 * return - result : the value of ISC_R_SUCCESS is meaning successful.
*/
isc_result_t locate_ip_information(const dns_name_t *qname, const isc_sockaddr_t *ip,
									isc_int16_t *isp_id, isc_int16_t *location_id, isc_int16_t *idc_id,
									dns_mysqlip_t *ipinfo);

