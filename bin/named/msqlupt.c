/* $Id: msqlupt.c,2013-3-12  tbox Exp $ */

#include <config.h>

#include <isc/log.h>
#include <isc/print.h>

#include <dns/message.h>
#include <dns/result.h>
#include <dns/tsig.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/db.h>
#include <dns/zt.h>

#include <named/log.h>
#include <named/msqlupt.h>

#define NS_MSQLUPT_TRACE
#ifdef NS_MSQLUPT_TRACE
#define CTRACE(m)	ns_client_log(client, \
				      DNS_LOGCATEGORY_MSQLUPT, \
				      NS_LOGMODULE_MSQLUPT, \
				      ISC_LOG_DEBUG(3), \
				      "%s", (m))
#else
#define CTRACE(m)	((void)(m))
#endif

#define DNS_GETDB_NOEXACT 0x01U
#define DNS_GETDB_NOLOG 0x02U
#define DNS_GETDB_PARTIAL 0x04U
#define DNS_GETDB_IGNOREACL 0x08U

static void msqlupt_log(ns_client_t *client, int level, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	ns_client_logv(client, DNS_LOGCATEGORY_MSQLUPT, NS_LOGMODULE_MSQLUPT,
		       level, fmt, ap);
	va_end(ap);
}

static void respond(ns_client_t *client, isc_result_t result) {
	dns_rcode_t rcode;
	dns_message_t *message;
	isc_result_t msg_result;

	message = client->message;
	rcode = dns_result_torcode(result);

	msg_result = dns_message_reply(message, ISC_TRUE);
	if (msg_result != ISC_R_SUCCESS)
		msg_result = dns_message_reply(message, ISC_FALSE);
	if (msg_result != ISC_R_SUCCESS) {
		ns_client_next(client, msg_result);
		return;
	}
	message->rcode = rcode;
	if (rcode == dns_rcode_noerror)
		message->flags |= DNS_MESSAGEFLAG_AA;
	else
		message->flags &= ~DNS_MESSAGEFLAG_AA;
	ns_client_send(client);
}

static isc_result_t msqlupt_getdb(ns_client_t *client, dns_name_t *name,
	   						unsigned int options, dns_zone_t **zonep, dns_db_t **dbp)
{
	isc_result_t result;
	unsigned int ztoptions;
	dns_zone_t *zone = NULL;
	dns_db_t *db = NULL;
	isc_boolean_t partial = ISC_FALSE;

	/*%
	 * Find a zone database to answer the query.
	 */
	ztoptions = ((options & DNS_GETDB_NOEXACT) != 0) ?
		DNS_ZTFIND_NOEXACT : 0;

	result = dns_zt_find(client->view->zonetable, name, ztoptions, NULL,
			     &zone);
	if (result == DNS_R_PARTIALMATCH)
		partial = ISC_TRUE;
	if (result == ISC_R_SUCCESS || result == DNS_R_PARTIALMATCH)
		result = dns_zone_getdb(zone, &db);
	if (result != ISC_R_SUCCESS)
		goto fail;
	
	/* Transfer ownership. */
	*zonep = zone;
	*dbp = db;

	if (partial && (options & DNS_GETDB_PARTIAL) != 0)
		return (DNS_R_PARTIALMATCH);
	return (ISC_R_SUCCESS);

 fail:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (db != NULL)
		dns_db_detach(&db);

	return (result);
}

/*
 * This function is for MysqlDB zone which has been modified.
 * params : client - client request
 * ret:	none
 *
 */
void ns_msqlupt_start(ns_client_t *client){
	
	isc_result_t result = ISC_R_FAILURE;
	if(client == NULL)
		goto failure;
	
	dns_message_t *message = client->message;
	dns_db_t *db = NULL;
	dns_zone_t *zone = NULL;
	dns_name_t *domainname = NULL;

	CTRACE("ns_msqlupt_start");

	/*
	 * Get the question name.
	 */
	result = dns_message_firstname(message, DNS_SECTION_QUESTION);
	if (result != ISC_R_SUCCESS) {
		msqlupt_log(client, ISC_LOG_NOTICE, "Get question name failed");
		return;
	}
/*	dns_message_currentname(message, DNS_SECTION_QUESTION,
				&client->query.qname);
	client->query.origqname = client->query.qname;
*/
	dns_message_currentname(message, DNS_SECTION_QUESTION, &domainname);
	
/*	result = msqlupt_getdb(client, client->query.qname,
					 DNS_GETDB_PARTIAL, &zone, &db);*/
/*	result = msqlupt_getdb(client, client->query.qname,
					 0, &zone, &db);
	if(result != ISC_R_SUCCESS){
		msqlupt_log(client, ISC_LOG_NOTICE, "DNS get DB failed");
		goto failure;
	}

	result = dns_db_update(db, client->query.qname);*/
	
	result = msqlupt_getdb(client, domainname, 0, &zone, &db);
	if(result != ISC_R_SUCCESS){
		msqlupt_log(client, ISC_LOG_NOTICE, "DNS get DB failed");
		goto failure;
	}

	result = dns_db_update(db, domainname);
	if(result != ISC_R_SUCCESS){
		msqlupt_log(client, ISC_LOG_NOTICE, "DNS update failed");
		goto failure;
	}

failure:
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);
	respond(client, result);
	
}

