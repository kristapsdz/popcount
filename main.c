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

#include "extern.h"

enum	page {
	PAGE_SUBMIT,
	PAGE__MAX
};

static const char *const pages[PAGE__MAX] = {
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
		"%s", kmimetypes[KMIME_APP_OCTET_STREAM]);
	khttp_body(r);
}

static void
send_submit(struct kreq *r)
{
	enum age	 age;

	if (r->fieldmap[VALID_SIGHTING_PLACEID] == NULL ||
	    r->fieldmap[VALID_SIGHTING_SPECIESID] == NULL ||
	    r->fieldmap[VALID_SIGHTING_NAME] == NULL ||
	    r->fieldmap[VALID_SIGHTING_EMAIL] == NULL ||
	    r->fieldmap[VALID_SIGHTING_COOKIE] == NULL ||
	    r->fieldmap[VALID_SIGHTING_AGE] == NULL ||
	    r->fieldmap[VALID_SIGHTING_COUNT] == NULL) {
		http_open(r, KHTTP_403);
		return;
	}

	age = r->fieldmap[VALID_SIGHTING_AGE]->parsed.i;

	db_sighting_insert(r->arg,
		r->fieldmap[VALID_SIGHTING_PLACEID]->parsed.i,
		r->fieldmap[VALID_SIGHTING_SPECIESID]->parsed.i,
		r->fieldmap[VALID_SIGHTING_NAME]->parsed.s,
		r->fieldmap[VALID_SIGHTING_EMAIL]->parsed.s,
		r->fieldmap[VALID_SIGHTING_COOKIE]->parsed.i,
		time(NULL),
		age,
		r->fieldmap[VALID_SIGHTING_COUNT]->parsed.i);

	kutil_info(r,
		r->fieldmap[VALID_SIGHTING_EMAIL]->parsed.s,
		"slug sighting: species-%" PRId64 
		", place-%" PRId64 ", count %" PRId64,
		r->fieldmap[VALID_SIGHTING_SPECIESID]->parsed.i,
		r->fieldmap[VALID_SIGHTING_PLACEID]->parsed.i,
		r->fieldmap[VALID_SIGHTING_COUNT]->parsed.i);

	http_open(r, KHTTP_201);
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

	if (PAGE__MAX == r.page) {
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

	send_submit(&r);

	db_close(r.arg);
	khttp_free(&r);
	return EXIT_SUCCESS;
}
