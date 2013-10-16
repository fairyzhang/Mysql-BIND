/*
 * Copyright (C) 2004, 2005, 2007-2009, 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001, 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id$ */

/*! \file */

#include <config.h>

#include <isc/string.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/nsec.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/result.h>

#include <dst/dst.h>

#define RETERR(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto failure; \
	} while (0)

void
dns_nsec_setbit(unsigned char *array, unsigned int type, unsigned int bit) {
	unsigned int shift, mask;

	shift = 7 - (type % 8);
	mask = 1 << shift;

	if (bit != 0)
		array[type / 8] |= mask;
	else
		array[type / 8] &= (~mask & 0xFF);
}

isc_boolean_t
dns_nsec_isset(const unsigned char *array, unsigned int type) {
	unsigned int byte, shift, mask;

	byte = array[type / 8];
	shift = 7 - (type % 8);
	mask = 1 << shift;

	return (ISC_TF(byte & mask));
}

unsigned int
dns_nsec_compressbitmap(unsigned char *map, const unsigned char *raw,
			unsigned int max_type)
{
	unsigned char *start = map;
	unsigned int window;
	int octet;

	if (raw == NULL)
		return (0);

	for (window = 0; window < 256; window++) {
		if (window * 256 > max_type)
			break;
		for (octet = 31; octet >= 0; octet--)
			if (*(raw + octet) != 0)
				break;
		if (octet < 0) {
			raw += 32;
			continue;
		}
		*map++ = window;
		*map++ = octet + 1;
		/*
		 * Note: potential overlapping move.
		 */
		memmove(map, raw, octet + 1);
		map += octet + 1;
		raw += 32;
	}
	return (map - start);
}

isc_result_t
dns_nsec_buildrdata(dns_db_t *db, dns_dbversion_t *version,
		    dns_dbnode_t *node, dns_name_t *target,
		    unsigned char *buffer, dns_rdata_t *rdata)
{
	isc_result_t result;
	dns_rdataset_t rdataset;
	isc_region_t r;
	unsigned int i;

	unsigned char *nsec_bits, *bm;
	unsigned int max_type;
	dns_rdatasetiter_t *rdsiter;

	memset(buffer, 0, DNS_NSEC_BUFFERSIZE);
	dns_name_toregion(target, &r);
	memcpy(buffer, r.base, r.length);
	r.base = buffer;
	/*
	 * Use the end of the space for a raw bitmap leaving enough
	 * space for the window identifiers and length octets.
	 */
	bm = r.base + r.length + 512;
	nsec_bits = r.base + r.length;
	dns_nsec_setbit(bm, dns_rdatatype_rrsig, 1);
	dns_nsec_setbit(bm, dns_rdatatype_nsec, 1);
	max_type = dns_rdatatype_nsec;
	dns_rdataset_init(&rdataset);
	rdsiter = NULL;
	result = dns_db_allrdatasets(db, node, version, 0, &rdsiter);
	if (result != ISC_R_SUCCESS)
		return (result);
	for (result = dns_rdatasetiter_first(rdsiter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(rdsiter))
	{
		dns_rdatasetiter_current(rdsiter, &rdataset);
		if (rdataset.type != dns_rdatatype_nsec &&
		    rdataset.type != dns_rdatatype_nsec3 &&
		    rdataset.type != dns_rdatatype_rrsig) {
			if (rdataset.type > max_type)
				max_type = rdataset.type;
			dns_nsec_setbit(bm, rdataset.type, 1);
		}
		dns_rdataset_disassociate(&rdataset);
	}

	/*
	 * At zone cuts, deny the existence of glue in the parent zone.
	 */
	if (dns_nsec_isset(bm, dns_rdatatype_ns) &&
	    ! dns_nsec_isset(bm, dns_rdatatype_soa)) {
		for (i = 0; i <= max_type; i++) {
			if (dns_nsec_isset(bm, i) &&
			    ! dns_rdatatype_iszonecutauth((dns_rdatatype_t)i))
				dns_nsec_setbit(bm, i, 0);
		}
	}

	dns_rdatasetiter_destroy(&rdsiter);
	if (result != ISC_R_NOMORE)
		return (result);

	nsec_bits += dns_nsec_compressbitmap(nsec_bits, bm, max_type);

	r.length = nsec_bits - r.base;
	INSIST(r.length <= DNS_NSEC_BUFFERSIZE);
	dns_rdata_fromregion(rdata,
			     dns_db_class(db),
			     dns_rdatatype_nsec,
			     &r);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_nsec_build(dns_db_t *db, dns_dbversion_t *version, dns_dbnode_t *node,
	       dns_name_t *target, dns_ttl_t ttl)
{
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	unsigned char data[DNS_NSEC_BUFFERSIZE];
	dns_rdatalist_t rdatalist;
	dns_rdataset_t rdataset;

	dns_rdataset_init(&rdataset);
	dns_rdata_init(&rdata);

	RETERR(dns_nsec_buildrdata(db, version, node, target, data, &rdata));

	rdatalist.rdclass = dns_db_class(db);
	rdatalist.type = dns_rdatatype_nsec;
	rdatalist.covers = 0;
	rdatalist.ttl = ttl;
	ISC_LIST_INIT(rdatalist.rdata);
	ISC_LIST_APPEND(rdatalist.rdata, &rdata, link);
	RETERR(dns_rdatalist_tordataset(&rdatalist, &rdataset));
	result = dns_db_addrdataset(db, node, version, 0, &rdataset,
				    0, NULL);
	if (result == DNS_R_UNCHANGED)
		result = ISC_R_SUCCESS;

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	return (result);
}

isc_boolean_t
dns_nsec_typepresent(dns_rdata_t *nsec, dns_rdatatype_t type) {
	dns_rdata_nsec_t nsecstruct;
	isc_result_t result;
	isc_boolean_t present;
	unsigned int i, len, window;

	REQUIRE(nsec != NULL);
	REQUIRE(nsec->type == dns_rdatatype_nsec);

	/* This should never fail */
	result = dns_rdata_tostruct(nsec, &nsecstruct, NULL);
	INSIST(result == ISC_R_SUCCESS);

	present = ISC_FALSE;
	for (i = 0; i < nsecstruct.len; i += len) {
		INSIST(i + 2 <= nsecstruct.len);
		window = nsecstruct.typebits[i];
		len = nsecstruct.typebits[i + 1];
		INSIST(len > 0 && len <= 32);
		i += 2;
		INSIST(i + len <= nsecstruct.len);
		if (window * 256 > type)
			break;
		if ((window + 1) * 256 <= type)
			continue;
		if (type < (window * 256) + len * 8)
			present = ISC_TF(dns_nsec_isset(&nsecstruct.typebits[i],
							type % 256));
		break;
	}
	dns_rdata_freestruct(&nsecstruct);
	return (present);
}

isc_result_t
dns_nsec_nseconly(dns_db_t *db, dns_dbversion_t *version,
		  isc_boolean_t *answer)
{
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_dnskey_t dnskey;
	isc_result_t result;

	REQUIRE(answer != NULL);

	dns_rdataset_init(&rdataset);

	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_db_findrdataset(db, node, version, dns_rdatatype_dnskey,
				     0, 0, &rdataset, NULL);
	dns_db_detachnode(db, &node);

	if (result == ISC_R_NOTFOUND)
		*answer = ISC_FALSE;
	if (result != ISC_R_SUCCESS)
		return (result);
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &dnskey, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		if (dnskey.algorithm == DST_ALG_RSAMD5 ||
		    dnskey.algorithm == DST_ALG_RSASHA1 ||
		    dnskey.algorithm == DST_ALG_DSA ||
		    dnskey.algorithm == DST_ALG_ECC)
			break;
	}
	dns_rdataset_disassociate(&rdataset);
	if (result == ISC_R_SUCCESS)
		*answer = ISC_TRUE;
	if (result == ISC_R_NOMORE) {
		*answer = ISC_FALSE;
		result = ISC_R_SUCCESS;
	}
	return (result);
}
