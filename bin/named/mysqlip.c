/*
 * MySQL BIND for IP information
 *
 * Copyright (C) 2012-2015 Fairy Zhang <mysqlbind@fairyzhang.me>.
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


#include <isc/mem.h>
#include <isc/print.h>

#include <isc/util.h>
#include <isc/buffer.h>

#include <dns/log.h>
#include <dns/types.h>

#include <named/mysqlip.h>

#include <mysql.h>

/*
  * Mysql Database connection information
*/
#define DB_NAME	"mysql_bind"
#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASSWD ""

/*
 * Initialize Mysql IP environment, connecting mysql database
 */
static isc_result_t mysqlip_init(dns_mysqlip_t *ipinfo);

/*
 * Destroy Mysql IP environment
 */

static void mysqlip_destroy(dns_mysqlip_t *ipinfo);


/*Connect to IP databse*/
static isc_result_t db_connect(dns_mysqlip_t *dbi){

    if(!mysql_init(&dbi->conn))
        return (ISC_R_FAILURE);
	
    if (mysql_real_connect(&dbi->conn, dbi->host, dbi->user, dbi->passwd, dbi->database, 0, NULL, 0))
        return (ISC_R_SUCCESS);
    else
        return (ISC_R_FAILURE);
}


/*
 * Check to see if the connection is still valid.  If not, attempt to
 * reconnect.
 */
static isc_result_t maybe_reconnect(dns_mysqlip_t *dbi){
    if (!mysql_ping(&dbi->conn))
        return (ISC_R_SUCCESS);

    return (db_connect(dbi));
}

/* find IP information from Database
 *
 * args  -    conn : connection of IP Database
 *		   ip_tbl : IP Table Name for query name
 * 	    source_ip : IP Address structure of user local DNS
 * 		  isp_id : ISP Info of user local DNS , which is for outputing
 * 	  location_id : LOCATION Info of user local DNS , which is for outputing
 * 	 	  idc_id : IDC Info of user local DNS , which is written by administrator for outputing
 *
 * return - result : the value of 1 is meaning finding successfully.
 *			      the value of 0 is meaning error or table non-existing.
*/
static isc_result_t mysqlip_find_from_db(MYSQL *conn,const char *ip_tbl,isc_sockaddr_t *source_ip, 
											isc_int16_t *isp_id, isc_int16_t *location_id, isc_int16_t *idc_id){
	if(ip_tbl == NULL||source_ip == NULL || conn == NULL || isp_id == NULL || location_id == NULL || idc_id == NULL)
		return 0;
	
	MYSQL_RES *res = 0;
	MYSQL_ROW row ;
	char str[1500];
	char *isp_str;
	char *location_str;
	char *idc_str;
	u_char *c_ip;
	long l_sip = htonl(source_ip->type.sin.sin_addr.s_addr) & 0xFFFFFF00;
	snprintf(str, sizeof(str),"SELECT isp_id,location_id,idc_id FROM `%s` WHERE (sip <= %ld and eip>= %ld) or sip=4294967295 order by id limit 1", ip_tbl,l_sip,l_sip);

	if( mysql_query(conn, str) != 0 ){
    	memset(str, '\0', sizeof(str));
    	return 0;
    }
	
    res = mysql_store_result(conn);

	if(res == NULL){
		memset(str, '\0', sizeof(str));
		*isp_id = 0;
		*location_id = 0;
		*idc_id = 0;
        return 1;
	}else if (mysql_num_rows(res) == 0){
		mysql_free_result(res);
		memset(str, '\0', sizeof(str));
		*isp_id = 0;
		*location_id = 0;
		*idc_id = 0;
        return 1;
   	}

	while ((row = mysql_fetch_row(res))) {
        isp_str = row[0];
		location_str = row[1];
		idc_str = row[2];
	    break;
	}

	mysql_free_result(res);
	memset(str, '\0', sizeof(str));
	*isp_id = atoi(isp_str);
	*location_id = atoi(location_str);
	*idc_id = atoi(idc_str);

	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_DEBUG(5),
			      "The info of source ip '%s' are isp %d location %d idc %d in %s table",
			      inet_ntoa(source_ip->type.sin.sin_addr),isp_id,location_id,idc_id,ip_tbl);
			
	if(*isp_id< 0 || *location_id<0 || *idc_id<0){
		return 0;
	}

	return 1;
}

/* split query name into arrays by '.' which are components of DB table name
 *
 * args  - qname : Domain Name to be splited
 *      split_name : name array which has been splited for returning
 *
 * return - spliting num : the value of non-zero is meaning spliting successfully.
 *					 the value of zero is meaning error or invalid query name.
*/
static int mysqlip_split_qname_by_dot(const char *qname, char **split_name){
	if(qname == NULL || split_name == NULL)
		return 0;

	int loop = 0;
	char name[255];
	char mdot[] = ".";

	strncpy(name, qname, 255);

	// split query name into arrays by '.' which are components of DB table name
	split_name[0]=strtok(name, mdot);

	if(split_name[0]==NULL)
	    return 0;

	for(loop=1;loop<142;loop++){
	    split_name[loop]=strtok(NULL,mdot);
	    if(split_name[loop]==NULL)
	        break;
	}

	if(loop < 2)
		return 0;
	return loop;
}

/* tranlate query name into DB table name and find IP information from Database
 *
 * args  -    conn : connection of IP Database
 *		 qname : Domain Name for resolving
 * 	    source_ip : IP Address structure of user local DNS
 * 		  isp_id : ISP Info of user local DNS , which is for outputing
 * 	  location_id : LOCATION Info of user local DNS , which is for outputing
 * 	 	  idc_id : IDC Info of user local DNS , which is written by administrator for outputing
 *
 * return - result : the value of ISC_R_SUCCESS is meaning successful.
*/
static isc_result_t mysqlip_transname_and_locateip(MYSQL *conn, const char *qname,
										isc_sockaddr_t *source_ip, 
										isc_int16_t *isp_id, isc_int16_t *location_id, isc_int16_t *idc_id){
	if(conn == NULL || qname == NULL)
		return 0;
	if(source_ip== NULL || isp_id == NULL ||location_id == NULL || idc_id == NULL)
		return 0;

	char tbl_name[255];
	char *array[142];

	int loop = 0;
	int i = 0;
	int j = 0;
	int start = 0;

	char *tmp_tbl = tbl_name;

	loop = mysqlip_split_qname_by_dot(qname,array);
	if(loop>4)
		start = loop - 4;

	//Merge arrays of DB table name components with '_' which are being used to  find IP information 
	for(j = start;j<=loop-2;j++){
		tmp_tbl = tbl_name;
		memset(tmp_tbl,'\0',255);
		memcpy(tmp_tbl,"ip",2);
		tmp_tbl += 2;
		
		for(i=j;i<loop;i++){
			snprintf(tmp_tbl,255,"_%s",array[i]);
			tmp_tbl += strlen(array[i])+1;
		}
		
		if(mysqlip_find_from_db(conn, tbl_name, source_ip, isp_id, location_id, idc_id) == 1){
			return 1;
		}
	}

	// Find IP information from default IP table (ip_tbl) when it could not be found from its own IP table or its zone IP table
	if(mysqlip_find_from_db(conn,"ip_tbl",source_ip,isp_id,location_id,idc_id) == 1){
		return 1;
	}

	return 0;
}

/* locating user IP information
 * args  - qname : Domain Name for resolving
 * 			 ip : IP Address structure of user local DNS
 * 		  isp_id : ISP Info of user local DNS , which is for outputing
 * 	  location_id : LOCATION Info of user local DNS , which is for outputing
 * 	 	  idc_id : IDC Info of user local DNS , which is written by administrator for outputing
 *		  ipinfo : Mysql IP Info of ns_g_server (global)
 * return - result : the value of ISC_R_SUCCESS is meaning successful.
*/
isc_result_t mysqlip_locate_information(const char *qname, const isc_sockaddr_t *ip,
									isc_int16_t *isp_id, isc_int16_t *location_id, isc_int16_t *idc_id,
									dns_mysqlip_t *ipinfo){
	if(isp_id == NULL || location_id == NULL || idc_id == NULL)
		return (ISC_R_FAILURE);
	
	if(qname == NULL || ip == NULL || ipinfo == NULL){
		*isp_id = -1;
		*location_id = -1;
		*idc_id = -1;
		return (ISC_R_SUCCESS);
	}

	isc_result_t result;
	
	result = mysqlip_init(ipinfo);
	if (result != ISC_R_SUCCESS){
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "Cannot connect to database '%s' when finding IP (%s) location.",
			      ipinfo->database,
			      inet_ntoa(ip->type.sin.sin_addr));

		*isp_id = 0;
		*location_id = 0;
		*idc_id = 0;
		return result;
	}
	
	if(qname != 0)
		if(mysqlip_transname_and_locateip(&ipinfo->conn,qname,ip,isp_id,location_id,idc_id)){
			mysqlip_destroy(ipinfo);
			return (ISC_R_SUCCESS);
		}
		
	mysqlip_destroy(ipinfo);
	return (ISC_R_FAILURE);
}

static isc_result_t
mysqlip_info_init(dns_mysqlip_t *ipinfo) {
	if(ipinfo == NULL)
		return (ISC_R_FAILURE);

	strncpy(ipinfo->database, DB_NAME,30);
	strncpy(ipinfo->host, DB_HOST,30);
	strncpy(ipinfo->user, DB_USER,30);
	strncpy(ipinfo->passwd, DB_PASSWD,30);
	return (ISC_R_SUCCESS);
}

/*
 * Initialize Mysql IP environment, connecting to mysql database
 */
static isc_result_t
mysqlip_init(dns_mysqlip_t *ipinfo) {
	isc_result_t result;
	
	if(ipinfo == NULL)
		return (ISC_R_FAILURE);
	
	result = mysqlip_info_init(ipinfo);
	if(result != ISC_R_SUCCESS)
		return result;
	
	result = db_connect(ipinfo);
	if (result != ISC_R_SUCCESS){
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_MYSQL, ISC_LOG_ERROR,
			      "Cannot connect to database '%s' when initializing IP DB(%s)",
			      ipinfo->database,ipinfo->host);
	}
	
	return result;
}

/*
 * Destroy Mysql IP environment
 */
static void
mysqlip_destroy(dns_mysqlip_t *ipinfo) {
	mysql_close(&ipinfo->conn);
}

