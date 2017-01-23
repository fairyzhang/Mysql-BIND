/*
 * MySQL BIND for IP information
 *
 * Copyright (C) 2012-2015 Fairy Zhang <fairyling.zhang@gmail.com>.
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
 * $Id: mysqlip.c,v 1.2 2012/10/17 16:16:48 fairy Exp $ 
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <isc/mem.h>
#include <isc/print.h>

#include <isc/util.h>
#include <isc/buffer.h>
#include <isc/hash.h>
#include <isc/sockaddr.h>

#include <dns/log.h>

#include <named/mysqlip.h>

/*
  * Mysql Database connection information
*/
#define DB_NAME	""
#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASSWD ""
#define DB_PORT 3306

/*
 * Initialize Mysql IP environment, connecting mysql database
 */
static isc_result_t
mysqlip_info_init( isc_mem_t *mctx,dns_mysqlip_t **ip_info);

void
ns_mysqlip_log(const isc_sockaddr_t *sip, isc_logcategory_t *category,
	   isc_logmodule_t *module, int level, const char *fmt, ...);

/*Connect to IP databse*/
static isc_result_t db_connect(dns_mysqlip_t *dbi){

    if(!mysql_init(&dbi->conn))
        return (ISC_R_FAILURE);
	
    if (mysql_real_connect(&dbi->conn, dbi->host, dbi->user, dbi->passwd, dbi->database, dbi->port, NULL, 0))
        return (ISC_R_SUCCESS);
    else
        return (ISC_R_FAILURE);
}

isc_result_t mysqlip_set_info( isc_mem_t *mctx,const cfg_obj_t *config, dns_mysqlip_t **ip_info) {
	if(config == NULL)
		return (ISC_R_FAILURE);

	dns_mysqlip_t *ipinfo;
	isc_result_t result;

	result = mysqlip_info_init(mctx, &ipinfo);
	if (result != (ISC_R_SUCCESS)) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
		      "There is an error while allocating ip information structure in setting information.");
		return (ISC_R_FAILURE);
	}

	const cfg_obj_t *catobj = NULL;
	const char *str;

	result = cfg_map_get(config, "host", &catobj);
	if(result == (ISC_R_SUCCESS)){
		str = cfg_obj_asstring(catobj);
		strncpy(ipinfo->host, str, 30);
	}

	catobj = NULL;
	result = cfg_map_get(config, "port", &catobj);
	if(result == (ISC_R_SUCCESS))
		if(cfg_obj_isuint32(catobj))
			ipinfo->port = cfg_obj_asuint32(catobj);

	catobj = NULL;
	result = cfg_map_get(config, "database", &catobj);
	if(result == (ISC_R_SUCCESS)){
		str = cfg_obj_asstring(catobj);
		strncpy(ipinfo->database, str, 30);
	}else{
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "Plesse specify the database of MysqlIpDB");
		isc_mem_put(mctx, ipinfo, sizeof(dns_mysqlip_t));
		return (ISC_R_FAILURE);
	}
	
	catobj = NULL;
	result = cfg_map_get(config, "user", &catobj);
	if(result == (ISC_R_SUCCESS)){
		str = cfg_obj_asstring(catobj);
		strncpy(ipinfo->user, str, 30);
	}

	catobj = NULL;
	result = cfg_map_get(config, "password", &catobj);
	if(result == (ISC_R_SUCCESS)){
		str = cfg_obj_asstring(catobj);
		strncpy(ipinfo->passwd, str, 30);
	}

	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_INFO,
			      "Trying to connect database '%s' of %s:%d using user(%s) and password(%s)",
			      ipinfo->database,ipinfo->host, ipinfo->port, ipinfo->user, ipinfo->passwd);
	
	result = db_connect(ipinfo);

	if (result != (ISC_R_SUCCESS)){
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "Cannot connect to database '%s' when initializing IP DB(%s)",
			      ipinfo->database,ipinfo->host);
	}
	if(&ipinfo->conn == NULL){
		isc_mem_put(mctx, ipinfo, sizeof(dns_mysqlip_t));
		return (ISC_R_FAILURE);
	}

	*ip_info = ipinfo;
	return (ISC_R_SUCCESS);
}

static isc_result_t
mysqlip_info_init( isc_mem_t *mctx,dns_mysqlip_t **ip_info) {
	dns_mysqlip_t *ipinfo;
	
	ipinfo = isc_mem_get(mctx, sizeof(dns_mysqlip_t));
	if (ipinfo == NULL) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
		      "Cannot allocate ip information structure while initializing radix array.");
		return (ISC_R_NOMEMORY);
	}
	
	if(ipinfo == NULL)
		return (ISC_R_FAILURE);

	strncpy(ipinfo->database, DB_NAME,30);
	strncpy(ipinfo->host, DB_HOST,30);
	strncpy(ipinfo->user, DB_USER,30);
	strncpy(ipinfo->passwd, DB_PASSWD,30);
	ipinfo->port = DB_PORT;

	*ip_info = ipinfo;
	return (ISC_R_SUCCESS);
}

static isc_result_t mysqlip_prefix_totext(const mysqlip_prefix_t *prefix, char *ip_str){
	if(prefix){
		struct in_addr ip ;
		ip.s_addr = htonl(prefix->add.sin.s_addr);
		if(inet_ntop(AF_INET, (void *)&ip, ip_str, 25))
			return (ISC_R_SUCCESS);
	}

	return (ISC_R_FAILURE);
}

static void mysqlip_insertip_log( mysqlip_prefix_t*node,  mysqlip_prefix_t *new_node){
	char node_ipstr[30] = "\0";
	char new_node_ipstr[30] = "\0";

	if(node == NULL || new_node == NULL){
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
		      "There is an error while inserting new node(%p)", new_node);
		return;
	}

	mysqlip_prefix_totext(node, node_ipstr);
	mysqlip_prefix_totext(new_node, new_node_ipstr);

	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
		      "The node(%s/%d) is already exist while inserting new node(%s/%d)",
		      node_ipstr,
		      node->bitlen,
		      new_node_ipstr, new_node->bitlen);
}

#define ipv4_threthold 4294967295U

#define NETADDR_TO_PREFIX_T(ip,pt,bits) \
	if(ip <= ipv4_threthold ) { \
		(pt)->family = AF_INET; \
		(pt)->bitlen = (bits); \
		(pt)->add.sin.s_addr = (ip); \
	} else { \
		(pt)->family = AF_INET6; \
		memcpy(&(pt)->add.sin6, &ip, 128);\
	} \

mysqlip_radix_tree_t **m_radix_tree = NULL;
isc_uint32_t m_radix_tree_num = 0;

static isc_result_t convert_ip_into_prefix(isc_mem_t *mctx,mysqlip_prefix_t **target,  isc_uint32_t db_addr, isc_uint16_t mask);
isc_result_t
mysqlip_radix_create(isc_mem_t *mctx, mysqlip_radix_tree_t **target, char *ip_tbl);

isc_result_t
mysqlip_radix_insert(mysqlip_radix_tree_t *radix, unsigned long ip, isc_uint32_t mask,
						isc_uint32_t isp_id, isc_uint32_t location_id, isc_uint32_t idc_id);

static isc_result_t mysqlip_init_radix( MYSQL *conn, const char *ip_tbl, mysqlip_radix_tree_t *radix_tree);

isc_result_t mysqlip_init_radix_array(isc_mem_t *mctx, dns_mysqlip_t *ipinfo){
	isc_result_t result;
	MYSQL_RES *res = 0;
	MYSQL_ROW row ;
	isc_uint32_t num;
	char str[500];
	char *name_str;
	isc_uint32_t i=0;
	mysqlip_radix_tree_t **radix_tree = NULL;
	mysqlip_radix_tree_t **tmp_radix_tree = NULL;
	isc_uint32_t tmp_radix_tree_num = 0;

	if(ipinfo == NULL){
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
		      "Cannot connect to database '%s' while initializing radix array.",
		      ipinfo->database);
		return (ISC_R_FAILURE);
	}
	if(&ipinfo->conn == NULL){
		isc_mem_put(mctx, ipinfo, sizeof(dns_mysqlip_t));
		return (ISC_R_FAILURE);
	}
	memset(str, '\0', sizeof(str));
	strncpy(str,"SHOW TABLES like 'ip_%' ", sizeof(str));
	
	if( mysql_query(&ipinfo->conn, str) != 0 ){
    	memset(str, '\0', sizeof(str));
		isc_mem_put(mctx, ipinfo, sizeof(dns_mysqlip_t));
    	return (ISC_R_FAILURE);
    }
	
    res = mysql_store_result(&ipinfo->conn);
	if(res == NULL){
		memset(str, '\0', sizeof(str));
		isc_mem_put(mctx, ipinfo, sizeof(dns_mysqlip_t));
        return (ISC_R_UNSET);
	}else if ((num=mysql_num_rows(res)) == 0){
		mysql_free_result(res);
		memset(str, '\0', sizeof(str));
		isc_mem_put(mctx, ipinfo, sizeof(dns_mysqlip_t));
        return (ISC_R_UNSET);
    }

	radix_tree = isc_mem_get(mctx, num*sizeof(mysqlip_radix_tree_t *));
	if(radix_tree == NULL){
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
		      "Cannot allocate m_radix_tree point.");
		isc_mem_put(mctx, ipinfo, sizeof(dns_mysqlip_t));
		return (ISC_R_NOMEMORY);
	}
	while ((row = mysql_fetch_row(res))){
       	name_str = row[0];
		
		if(name_str == NULL){
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "IP TABLE name '%s' is NULL while initing radix array.",
			      ipinfo->database);
			name_str = NULL;
			
			continue;
		}

		radix_tree[i] = NULL;
		result = mysqlip_radix_create(mctx, &radix_tree[i], name_str);
		if(result != ISC_R_SUCCESS){
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "There is an error while creating radix tree for IP TABLE '%s' in db %s.",
			      name_str,ipinfo->database);
			name_str = NULL;
			
			continue;
		}

		result = mysqlip_init_radix(&ipinfo->conn, name_str, radix_tree[i]);
		if(result != ISC_R_SUCCESS){
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "There is an error while initializing radix tree for IP TABLE '%s' in db %s.",
			      name_str,ipinfo->database);
			name_str = NULL;
			
			continue;
		}
		
		i++;
		name_str = NULL;
	}
	mysql_free_result(res);
	isc_mem_put(mctx, ipinfo, sizeof(dns_mysqlip_t));
	
	tmp_radix_tree = m_radix_tree;
	tmp_radix_tree_num = m_radix_tree_num;
	
	m_radix_tree = radix_tree;
	m_radix_tree_num = num;
	
	if(tmp_radix_tree != NULL && tmp_radix_tree_num>0) {//free old radix_tree
		result = mysqldb_clear_radix_tree_array(mctx, tmp_radix_tree, tmp_radix_tree_num);
		if(result != ISC_R_SUCCESS)
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "There is an error while freeing old radix tree array.");
	}
	
	return (ISC_R_SUCCESS);
	
}

static isc_result_t mysqlip_init_radix( MYSQL *conn, const char *ip_tbl, mysqlip_radix_tree_t *radix_tree){
	MYSQL_RES *res = 0;
	MYSQL_ROW row ;
	char str[500];
	long long ip;
	isc_uint32_t mask;
	isc_uint32_t isp_id;
	isc_uint32_t location_id;
	isc_uint32_t idc_id;
	isc_result_t result;

	char *ip_str;
	char *mask_str;
	char *isp_str;
	char *location_str;
	char *idc_str;

	if(conn == NULL)
		return (ISC_R_FAILURE);
	if(ip_tbl == NULL)
		return (ISC_R_FAILURE);

	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		DNS_LOGMODULE_MYSQL, ISC_LOG_INFO,
		"Initializing the radix tree of IP table %s.", ip_tbl);

	snprintf(str, sizeof(str), "SELECT sip, mask, isp_id, location_id, idc_id FROM %s ",ip_tbl);

	if( mysql_query(conn, str) != 0 ){
    	memset(str, '\0', sizeof(str));
    	return (ISC_R_FAILURE);
    }
    res = mysql_store_result(conn);

	if(res == NULL){
		memset(str, '\0', sizeof(str));
        return (ISC_R_FAILURE);
	}else if (mysql_num_rows(res) == 0){
		mysql_free_result(res);
		memset(str, '\0', sizeof(str));
        return (ISC_R_FAILURE);
    }

	while ((row = mysql_fetch_row(res))){
		ip_str = row[0];
		mask_str = row[1];
		isp_str = row[2];
		location_str = row[3];
		idc_str = row[4];

		char *endp;

		ip = strtol(ip_str, &endp, 10);
		mask = strtol(mask_str, &endp, 10);
		isp_id = strtol(isp_str, &endp, 10);
		location_id = strtol(location_str, &endp, 10);
		idc_id = strtol(idc_str, &endp, 10);

		result = mysqlip_radix_insert(radix_tree, ip, mask, isp_id, location_id, idc_id);
		if(result != (ISC_R_SUCCESS)){
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "There is an error while inserting into radix tree for IP  '%s/%d' of table %s.",
			      ip_str, mask, ip_tbl);
		}
		
	}

	mysql_free_result(res);
	return result;
}

static isc_result_t convert_ip_into_prefix(isc_mem_t *mctx,mysqlip_prefix_t **target,  isc_uint32_t db_addr, isc_uint16_t mask){
	mysqlip_prefix_t *prefix;
	
	if(mctx == NULL)
		return (ISC_R_FAILURE);
	prefix = isc_mem_get(mctx, sizeof(mysqlip_prefix_t));
	if(prefix == NULL)
		return (ISC_R_FAILURE);
	long l_sip = db_addr & 0x0FFFFFFFF;

	NETADDR_TO_PREFIX_T(l_sip,prefix,mask);
	
	*target = prefix;
	return (ISC_R_SUCCESS);
}

static isc_result_t convert_ipinfo_into_struct (isc_mem_t *mctx, mysqlip_ipinfo_t **target,
										isc_uint16_t idc, isc_uint16_t isp, isc_uint16_t location) {
	if(mctx == NULL)
		return (ISC_R_FAILURE);
	mysqlip_ipinfo_t *ipinfo;
	ipinfo = isc_mem_get(mctx, sizeof(mysqlip_ipinfo_t));
	if(ipinfo == NULL)
		return (ISC_R_FAILURE);

	ipinfo->idc = idc;
	ipinfo->isp = isp;
	ipinfo->location = location;

	*target = ipinfo;
	return (ISC_R_SUCCESS);
}

static unsigned int return_hashval(const char *domain){
	unsigned int keylen = strlen(domain);
	return isc_hash_calc((const unsigned char *)domain, keylen, ISC_FALSE);
}

static isc_result_t convert_str_lower(const char *str, char *target, isc_uint16_t len){
	if(str == NULL)
		return (ISC_R_FAILURE);

	isc_uint16_t i;
	isc_uint16_t l = (len<strlen(str))?len:strlen(str);
	char *t = target;

	for(i=0;i<l;i++){
		*t = tolower(str[i]);
		t ++;
	}

	*t = '\0';

	return (ISC_R_SUCCESS);
}

static int mysqlip_split_tblname(const char *tbl_name, char *domain_name, isc_uint32_t *hash){
	if(tbl_name == NULL || domain_name == NULL)
		return 0;

	int loop = 0;
	char name[255];
	char *name_array;
	char mdot[] = "_";
	char domain[255] = "";

	strncpy(name, tbl_name, 255);

	// split table name into arrays by '_' which are components of domain name
	name_array= strtok(name, mdot);
	if(name_array == NULL) {
	    return 0;
	}

	while(name_array != NULL) {
	    name_array = strtok(NULL,mdot);
		if(name_array == NULL)
	        break;
		if(loop != 0)
			strcat(domain, ".");
		strcat(domain, name_array);
		
		loop++;
	}

	strncpy(domain_name, domain, 255);

	*hash = return_hashval(domain);
	return loop;
}

static int
_comp_with_mask(u_int32_t addr, u_int32_t cmp, u_int32_t bits) {
	/* Mask length of zero matches everything */
	if (bits == 0)
		return 1;

	isc_uint32_t mask = 0x100000000 - (1 << (32-bits));

	if((cmp & mask) == addr)
		return 1;
	return 0;
}

#define DEFAULT_IPTBL "ip_tbl"

isc_result_t
mysqlip_radix_create(isc_mem_t *mctx, mysqlip_radix_tree_t **target, char *ip_tbl) {
	mysqlip_radix_tree_t *radix;

	REQUIRE(target != NULL && *target == NULL);

	radix = isc_mem_get(mctx, sizeof(mysqlip_radix_tree_t));
	if (radix == NULL)
		return (ISC_R_NOMEMORY);

	radix->mctx = mctx;
	radix->head = NULL;
	radix->num_active_node = 0;
	radix->num_added_node = 0;

	char iptbl[255];
	if(convert_str_lower(ip_tbl, iptbl,255) == (ISC_R_FAILURE))
		strncpy(iptbl, ip_tbl, 255);
	
	if(strncmp(iptbl, DEFAULT_IPTBL, 20) == 0){
		strncpy(radix->domain_name, iptbl, 255);
		radix->domain_hash = -1;
	}else{
		mysqlip_split_tblname(iptbl, radix->domain_name, &radix->domain_hash);
	}

	radix->default_node = NULL;
	
	*target = radix;
	return (ISC_R_SUCCESS);
}

static isc_result_t mysqlip_create_node(isc_mem_t *mctx, mysqlip_radix_node_t **target){
	mysqlip_radix_node_t *node;
	
	node = isc_mem_get(mctx, sizeof(mysqlip_radix_node_t));
	if(node == NULL)
		return (ISC_R_NOMEMORY);

	node->bit = 0;
	node->ipinfo = NULL;
	node->parent = NULL;
	node->l = NULL;
	node->r = NULL;
	node->prefix = NULL;

	*target = node;
	return (ISC_R_SUCCESS);
}

static isc_result_t
mysqlip_init_node(isc_mem_t *mctx, unsigned long ip, isc_uint32_t mask,
						isc_uint32_t isp_id, isc_uint32_t location_id, isc_uint32_t idc_id,
						mysqlip_radix_node_t **node){
	mysqlip_prefix_t *prefix;
	mysqlip_radix_node_t *new_node;
	isc_result_t result;

	REQUIRE(mctx != NULL);
	result = convert_ip_into_prefix(mctx, &prefix, ip, mask);
	if(result != (ISC_R_SUCCESS)) {
		char ip_str[30] = "\0";
		mysqlip_prefix_totext(prefix, ip_str);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "There is an error while creating prefix for IP '%s'.",
			      ip_str);

		
		return (ISC_R_FAILURE);
	}
	
	INSIST(prefix != NULL);

	result = mysqlip_create_node(mctx, &new_node);
	if(result != (ISC_R_SUCCESS)){
		isc_mem_put(mctx, prefix, sizeof(mysqlip_prefix_t));
		return result;
	}
	new_node->bit = prefix->bitlen;
	new_node->prefix = prefix;
	if(convert_ipinfo_into_struct(mctx, &new_node->ipinfo, idc_id, isp_id, location_id)!= ISC_R_SUCCESS) {
		char prefix_str[30] = "\0";
		mysqlip_prefix_totext(prefix, prefix_str);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
		      "There is an error while converting ip information into structure for IP '%s/%d' (%d,%d,%d).",
		      prefix_str, new_node->bit, idc_id, isp_id, location_id);
	}
	
	*node = new_node;
	return (ISC_R_SUCCESS);
}

static isc_result_t
mysqlip_radix_insert_node(mysqlip_radix_tree_t *radix, mysqlip_radix_node_t *new_node) {
	mysqlip_prefix_t *prefix;
	mysqlip_radix_node_t *node, *next;
	isc_uint32_t bitlen, bits, bitc, fam;
	isc_uint32_t mask;
	isc_result_t result;
	
	REQUIRE(radix != NULL);
	prefix = new_node->prefix;
	
	INSIST(prefix != NULL);

	bitlen = prefix->bitlen;
	fam = prefix->family;
	
	long long addr;
	if(new_node->prefix->family == AF_INET) {
		bits = 1<<31;
		mask = 0x100000000 - (1<<(32-bitlen));
	} else {
		return (ISC_R_FAILURE);
	}

	addr = prefix->add.sin.s_addr;
	bitc = 1;
	if(radix->head == NULL){
		result = mysqlip_create_node(radix->mctx, &node);
		if(result != (ISC_R_SUCCESS))
			return result;

		radix->head = node;
		radix->num_added_node++;
	}

	node = radix->head;
	next = NULL;
	while(mask & bits) {
		if(addr & bits) {
			next = node->r;
		}else{
			next = node->l;
		}

		if(next == NULL)
			break;

		bitc ++;
		bits>>=1;
		node = next;
	}

	if(next) {
		if(node->prefix){
			radix->num_active_node--;

			mysqlip_insertip_log(node->prefix, new_node->prefix);
			
			isc_mem_put(radix->mctx, new_node->prefix, sizeof(mysqlip_prefix_t));
			if(new_node->ipinfo)
				isc_mem_put(radix->mctx, new_node->ipinfo, sizeof(mysqlip_ipinfo_t));
			
			isc_mem_put(radix->mctx, new_node, sizeof(mysqlip_radix_node_t));
			return (ISC_R_SUCCESS);
		}

		if(node->bit == bitlen) {
			radix->num_added_node--;
			node->prefix = prefix;
			node->ipinfo = new_node->ipinfo;
			isc_mem_put(radix->mctx, new_node, sizeof(mysqlip_radix_node_t));

			return (ISC_R_SUCCESS);
		}

		if(node->bit < bitlen) {
			return (ISC_R_FAILURE);
		}
	}

	while(mask & bits) {
		if(bitlen == bitc){
			next = new_node;
		}else{
			result = mysqlip_create_node(radix->mctx, &next);
			if(result != (ISC_R_SUCCESS))
				return result;

			next->bit = bitc;
			radix->num_added_node++;
		}
		next->parent = node;
		
		if(bitc == 1 && radix->head == NULL){
			radix->head = next;
		}else {
			if (addr & bits) {
				node->r = next;
			}else{
				node->l = next;
			}
		}
		bits >>= 1;
		bitc ++;
		node = next;
		
	}
	
	return (ISC_R_SUCCESS);
}

isc_result_t
mysqlip_radix_insert(mysqlip_radix_tree_t *radix, unsigned long ip, isc_uint32_t mask,
						isc_uint32_t isp_id, isc_uint32_t location_id, isc_uint32_t idc_id) {
	mysqlip_radix_node_t *new_node;
	isc_result_t result;

	REQUIRE(radix != NULL);
	new_node = NULL;
	result = mysqlip_init_node(radix->mctx, ip, mask, isp_id, location_id, idc_id, &new_node);
	if(result != (ISC_R_SUCCESS)){
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "There is an error while creating new node '%ld' mask %d isp %d location %d idc %d.",
			      ip, mask, isp_id, location_id, idc_id);
		return result;
	}
	if(ip == ipv4_threthold){
		radix->default_node = new_node;
		return (ISC_R_SUCCESS);
	}
	radix->num_active_node++;

	result = mysqlip_radix_insert_node(radix, new_node);
	if (result != (ISC_R_SUCCESS)) {
		isc_mem_put(radix->mctx, new_node->prefix, sizeof(mysqlip_prefix_t));
		isc_mem_put(radix->mctx, new_node->ipinfo, sizeof(mysqlip_ipinfo_t));
		isc_mem_put(radix->mctx, new_node, sizeof(mysqlip_radix_node_t));
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "There is an error while inserting new node '%ld' mask %d isp %d location %d idc %d.",
			      ip, mask, isp_id, location_id, idc_id);
		
	}

	return result;
}

static isc_result_t
mysqlip_radix_search_node(mysqlip_radix_tree_t *radix, mysqlip_radix_node_t **target,
		 mysqlip_prefix_t *prefix) {
	mysqlip_radix_node_t *node;
	long long addr;
	isc_uint32_t bits;
	mysqlip_radix_node_t *found = NULL;
	REQUIRE(radix != NULL);
	REQUIRE(prefix != NULL);

	*target = NULL;

	if (radix->head == NULL) {
		return (ISC_R_NOTFOUND);
	}
	
	node = radix->head;
	
	if(prefix->family == AF_INET) {
		bits = 0x80000000;
	}else {
		return (ISC_R_NOTFOUND);
	}

	addr = prefix->add.sin.s_addr;
	while(node){
		if(node->prefix != NULL){
			found = node;
		}

		if(addr & bits){
			node = node->r;
		}else{
			node = node->l;
		}
		bits = bits >> 1;
	}
	if(found) {
		if(_comp_with_mask(found->prefix->add.sin.s_addr,addr,found->bit))
			*target = found;
	}
	if(*target == NULL)
		*target = radix->default_node;
	
	if (*target == NULL)
		return (ISC_R_NOTFOUND);
	else
		return (ISC_R_SUCCESS);
	
}

static mysqlip_radix_tree_t *
mysqlip_radix_tree_search(const char *qname, const char *zone){
	if(m_radix_tree == NULL)
		return NULL;
	if(qname == NULL || zone == NULL)
		return NULL;

	mysqlip_radix_tree_t *zone_radix = NULL;
	mysqlip_radix_tree_t *default_radix = NULL;
	char domain[255];
	
	convert_str_lower(zone, domain, 255);
	isc_uint32_t zone_hash = return_hashval(domain);

	convert_str_lower(qname, domain, 255);
	isc_uint32_t qname_hash = return_hashval(domain);

	isc_uint32_t ret = 0;
	isc_uint16_t i = 0;

	while(m_radix_tree[i] && i<m_radix_tree_num){
		if(qname_hash == m_radix_tree[i]->domain_hash){
			if(strncmp(domain, m_radix_tree[i]->domain_name, 255) == 0){
				isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_MYSQL, ISC_LOG_DEBUG(5),
					      "The ip table is %s", m_radix_tree[i]->domain_name);
				return m_radix_tree[i];
			}
		}else if(zone_hash == m_radix_tree[i]->domain_hash) {
			ret = 1; //zone_flag
			zone_radix = m_radix_tree[i];
		}else if((isc_int32_t)(m_radix_tree[i]->domain_hash) == -1) {
			default_radix = m_radix_tree[i];

			if(ret == 0) {
				ret = 2; //default_falg
			}
		}
		
		i++;
	}

	if (ret == 1) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_MYSQL, ISC_LOG_DEBUG(5),
					      "The ip table is %s", zone_radix->domain_name);
		return zone_radix;
	}

	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_DEBUG(5),
			      "The ip table is %s", default_radix->domain_name);
	return default_radix;
	
}

isc_result_t
mysqlip_radix_search(const char *qname, const char* zone, const isc_sockaddr_t *ip,
									isc_int16_t *isp_id, isc_int16_t *location_id, isc_int16_t *idc_id) {
	mysqlip_radix_tree_t *radix;
	mysqlip_prefix_t *prefix;
	mysqlip_radix_node_t *node;
	isc_result_t result = -1;
	
	if(ip == NULL || qname == NULL || zone == NULL)
		return (ISC_R_FAILURE);
	
	radix = mysqlip_radix_tree_search(qname, zone);
	
	if(radix == NULL)
		return (ISC_R_FAILURE);
	
	long l_sip = htonl(ip->type.sin.sin_addr.s_addr) & 0xFFFFFFFF;

	result = convert_ip_into_prefix(radix->mctx, &prefix, l_sip, 32);
	if(result != (ISC_R_SUCCESS))
		return result;

	result = mysqlip_radix_search_node(radix, &node, prefix);
	if(result != (ISC_R_SUCCESS)) {
		isc_mem_put(radix->mctx, prefix, sizeof(mysqlip_prefix_t));
		return (ISC_R_FAILURE);
	}
	
	if(node->ipinfo == NULL){
		isc_mem_put(radix->mctx, prefix, sizeof(mysqlip_prefix_t));
		return (ISC_R_FAILURE);
	}

	char node_str[25] = "\0";
	mysqlip_prefix_totext(node->prefix, &node_str[0]);
	ns_mysqlip_log(ip, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_MYSQL, ISC_LOG_INFO,
		      "Locate at isp %d location %d idc %d of %s/%d",
		      node->ipinfo->isp, node->ipinfo->location, node->ipinfo->idc,
		      node_str, node->bit);
	
	*isp_id = node->ipinfo->isp;
	*location_id = node->ipinfo->location;
	*idc_id = node->ipinfo->idc;

	isc_mem_put(radix->mctx, prefix, sizeof(mysqlip_prefix_t));
	return (ISC_R_SUCCESS);
	
}

static isc_result_t mysqldb_put_radix_node(isc_mem_t *mctx, mysqlip_radix_node_t *node){
	if(mctx == NULL || node == NULL)
		return (ISC_R_FAILURE);

	if(node->prefix)
		isc_mem_put(mctx, node->prefix, sizeof(mysqlip_prefix_t));
	
	if(node->ipinfo)
		isc_mem_put(mctx, node->ipinfo, sizeof(mysqlip_ipinfo_t));
	
	isc_mem_put(mctx, node, sizeof(mysqlip_radix_node_t));
	return (ISC_R_SUCCESS);
}

static isc_result_t mysqldb_clear_radix_node(isc_mem_t *mctx, mysqlip_radix_node_t *node){
	if(mctx == NULL || node == NULL)
		return (ISC_R_FAILURE);

	isc_result_t result;
	
	if(node->l)
		result = mysqldb_clear_radix_node(mctx, node->l);

	if(node->r)
		result = mysqldb_clear_radix_node(mctx, node->r);

	result = mysqldb_put_radix_node(mctx, node);

	return (ISC_R_SUCCESS);
}

static isc_result_t mysqldb_clear_radix_tree(isc_mem_t *mctx, mysqlip_radix_tree_t *tree){
	if(mctx == NULL || tree == NULL)
		return (ISC_R_FAILURE);

	if(tree->head == NULL)
		return (ISC_R_FAILURE);

	isc_result_t result;
	result = mysqldb_clear_radix_node(mctx, tree->head);

	if(tree->default_node)
		mysqldb_put_radix_node(mctx, tree->default_node);
	
	isc_mem_put(mctx, tree, sizeof(mysqlip_radix_tree_t));
	return result;
}

isc_result_t mysqldb_clear_radix_tree_array(isc_mem_t *mctx, mysqlip_radix_tree_t **tree_array, isc_uint32_t tree_num){
	isc_result_t result;
	isc_uint32_t i;
	
	if(tree_array == NULL || tree_num <= 0){
		tree_array = m_radix_tree;
		tree_num = m_radix_tree_num;
		
		if(tree_array == NULL || tree_num <= 0)
			return (ISC_R_SUCCESS);
	}
	
	isc_uint32_t c = 0;
	for(i = 0; i<tree_num ; i++){
		c += tree_array[i]->num_added_node + tree_array[i]->num_active_node;
		result = mysqldb_clear_radix_tree(mctx, tree_array[i]);
		if(result != (ISC_R_SUCCESS)){
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "There is something wrong while clearing the memory of radix tree.");
		}
	}
	isc_mem_put(mctx, tree_array, tree_num * sizeof(mysqlip_radix_tree_t *));
	return result;
}

static void
ns_mysqlip_name(const isc_sockaddr_t *sip, char *peerbuf, size_t len) {
	if (sip)
		isc_sockaddr_format(sip, peerbuf, len);
	else
		snprintf(peerbuf, len, "@%p", sip);
}

static void ns_mysqlip_logv(const isc_sockaddr_t *sip, isc_logcategory_t *category,
	       isc_logmodule_t *module, int level, const char *fmt, va_list ap)
{
	char msgbuf[2048];
	char peerbuf[ISC_SOCKADDR_FORMATSIZE];

	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);

	ns_mysqlip_name(sip, peerbuf, sizeof(peerbuf));

	isc_log_write(dns_lctx, category, module, level,
		      "mysqldb %s: %s",
		      peerbuf, msgbuf);
}

void ns_mysqlip_log(const isc_sockaddr_t *sip, isc_logcategory_t *category,
	   isc_logmodule_t *module, int level, const char *fmt, ...)
{
	va_list ap;

	if (! isc_log_wouldlog(dns_lctx, level))
		return;

	va_start(ap, fmt);
	ns_mysqlip_logv(sip, category, module, level, fmt, ap);
	va_end(ap);
}
