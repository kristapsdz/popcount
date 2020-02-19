/*	$Id$ */
/*
 * Copyright (c) 2020 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <kcgi.h>
#include <kcgijson.h>

#include "extern.h"

enum	page {
	PAGE_DELETE,
	PAGE_NSUBMIT,
	PAGE_SUBMIT,
	PAGE__MAX
};

static const char *const pages[PAGE__MAX] = {
	"delete", /* PAGE_DELETE */
	"nsubmit", /* PAGE_NSUBMIT */
	"submit", /* PAGE_SUBMIT */
};

static void
http_open(struct kreq *r, enum khttp code)
{

	khttp_head(r, kresps[KRESP_STATUS], 
		"%s", khttps[code]);
	khttp_head(r, kresps[KRESP_CACHE_CONTROL], 
		"%s", "no-cache, no-store, must-revalidate");
	khttp_head(r, kresps[KRESP_PRAGMA], 
		"%s", "no-cache");
	khttp_head(r, kresps[KRESP_EXPIRES], 
		"%s", "0");
	khttp_head(r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[KMIME_APP_JSON]);
	khttp_body(r);
}

static void
send_delete(struct kreq *r)
{
	struct kpair	*kpi, *kpc;

	if ((kpi = r->fieldmap[VALID_SIGHTING_ID]) == NULL ||
	    (kpc = r->fieldmap[VALID_SIGHTING_COOKIE]) == NULL) {
		http_open(r, KHTTP_403);
		return;
	}

	/* Delete only within the last hour. */

	db_sighting_delete_recent(r->arg,
		kpi->parsed.i, /* id */
		kpc->parsed.i, /* cookie */
		time(NULL) - (60 * 60)); /* ctime (gt) */

	kutil_info(r, NULL,
		"sighting (maybe) deleted: sighting-%" PRId64 ,
		kpi->parsed.i);

	http_open(r, KHTTP_201);
}

static void
send_nsubmit(struct kreq *r)
{
	struct kpair	*kpp, *kps, *kpn, *kpe, *kpa;

	if ((kpp = r->fieldmap[VALID_SIGHTING_PLACEID]) == NULL ||
	    (kps = r->fieldmap[VALID_SIGHTING_SPECIESID]) == NULL ||
	    (kpn = r->fieldmap[VALID_SIGHTING_NAME]) == NULL ||
	    (kpe = r->fieldmap[VALID_SIGHTING_EMAIL]) == NULL ||
	    (kpa = r->fieldmap[VALID_SIGHTING_AGE]) == NULL) {
		http_open(r, KHTTP_403);
		return;
	}

	while (kps != NULL && kpa != NULL) {
		db_sighting_insert(r->arg,
			kpp->parsed.i, /* placeid */
			kps->parsed.i, /* speciesid */
			kpn->parsed.s, /* name */
			kpe->parsed.s, /* email */
			arc4random(), /* cookie */
			time(NULL), /* ctime */
			(enum age)kpa->parsed.i, /* age */
			0); /* count */
		kps = kps->next;
		kpa = kpa->next;
	}

	kutil_info(r, kpe->parsed.s,
		"slug non-sighting: place-%" PRId64,
		kpp->parsed.i);

	http_open(r, KHTTP_201);
}

static void
send_submit(struct kreq *r)
{
	int64_t		 cookie, id;
	struct kjsonreq	 req;
	struct kpair	*kpp, *kps, *kpn, *kpe, *kpa, *kpc;

	if ((kpp = r->fieldmap[VALID_SIGHTING_PLACEID]) == NULL ||
	    (kps = r->fieldmap[VALID_SIGHTING_SPECIESID]) == NULL ||
	    (kpn = r->fieldmap[VALID_SIGHTING_NAME]) == NULL ||
	    (kpe = r->fieldmap[VALID_SIGHTING_EMAIL]) == NULL ||
	    (kpa = r->fieldmap[VALID_SIGHTING_AGE]) == NULL ||
	    (kpc = r->fieldmap[VALID_SIGHTING_COUNT]) == NULL) {
		http_open(r, KHTTP_403);
		return;
	}

	cookie = arc4random();

	id = db_sighting_insert(r->arg,
		kpp->parsed.i, /* placeid */
		kps->parsed.i, /* speciesid */
		kpn->parsed.s, /* name */
		kpe->parsed.s, /* email */
		cookie, /* cookie */
		time(NULL), /* ctime */
		(enum age)kpa->parsed.i, /* age */
		kpc->parsed.i); /* count */

	if (id == -1) {
		kutil_warnx(r, kpe->parsed.s, "db_sighting_insert");
		http_open(r, KHTTP_403);
		return;
	}

	kutil_info(r, kpe->parsed.s,
		"slug sighting: species-%" PRId64 
		", place-%" PRId64 ", count %" PRId64 ": %" PRId64,
		kps->parsed.i, kpp->parsed.i, kpc->parsed.i, id);

	http_open(r, KHTTP_200);
	kjson_open(&req, r);
	kjson_obj_open(&req);
	kjson_putintp(&req, "cookie", cookie);
	kjson_putintp(&req, "id", id);
	kjson_obj_close(&req);
	kjson_close(&req);
}

int
main(void)
{
	struct kreq	 r;
	enum kcgi_err	 er;

	er = khttp_parse(&r, valid_keys,
		VALID__MAX, pages, PAGE__MAX, PAGE_SUBMIT);

	if (er != KCGI_OK)
		kutil_errx(&r, NULL, 
			"khttp_parse: %s", kcgi_strerror(er));

	if (r.page == PAGE__MAX) {
		http_open(&r, KHTTP_404);
		khttp_free(&r);
		return EXIT_SUCCESS;
	}

	if ((r.arg = db_open(DATADIR "/slugcount.db")) == NULL) {
		kutil_errx(&r, NULL, "db_open: %s", 
			DATADIR "/slugcount.db");
		khttp_free(&r);
		return EXIT_SUCCESS;
	}

	if (pledge("stdio", NULL) == -1) {
		kutil_warn(NULL, NULL, "pledge");
		db_close(r.arg);
		khttp_free(&r);
		return EXIT_FAILURE;
	}

	switch (r.page) {
	case PAGE_DELETE:
		send_delete(&r);
		break;
	case PAGE_NSUBMIT:
		send_nsubmit(&r);
		break;
	case PAGE_SUBMIT:
		send_submit(&r);
		break;
	default:
		abort();
	}

	db_close(r.arg);
	khttp_free(&r);
	return EXIT_SUCCESS;
}
