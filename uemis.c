/*
 * uemis.c
 *
 * UEMIS SDA file importer
 * AUTHOR:  Dirk Hohndel - Copyright 2011
 *
 * Licensed under the MIT license.
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#define __USE_XOPEN
#include <time.h>
#include <regex.h>

#include <gtk/gtk.h>

#include "dive.h"
#include "display.h"
#include "uemis.h"

/*
 * following code is based on code found in at base64.sourceforge.net/b64.c
 * AUTHOR:         Bob Trower 08/04/01
 * COPYRIGHT:      Copyright (c) Trantor Standard Systems Inc., 2001
 * NOTE:           This source code may be used as you wish, subject to
 *                 the MIT license.
 */
/*
 * Translation Table to decode (created by Bob Trower)
 */
static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

/*
 * decodeblock -- decode 4 '6-bit' characters into 3 8-bit binary bytes
 */
static void decodeblock( unsigned char in[4], unsigned char out[3] ) {
	out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
	out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
	out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}

/*
 * decode a base64 encoded stream discarding padding, line breaks and noise
 */
static void decode( uint8_t *inbuf, uint8_t *outbuf, int inbuf_len ) {
	uint8_t in[4], out[3], v;
	int i,len,indx_in=0,indx_out=0;

	while (indx_in < inbuf_len) {
		for (len = 0, i = 0; i < 4 && (indx_in < inbuf_len); i++ ) {
			v = 0;
			while ((indx_in < inbuf_len) && v == 0) {
				v = inbuf[indx_in++];
				v = ((v < 43 || v > 122) ? 0 : cd64[ v - 43 ]);
				if (v)
					v = ((v == '$') ? 0 : v - 61);
			}
			if (indx_in < inbuf_len) {
				len++;
				if (v)
					in[i] = (v - 1);
			}
			else
				in[i] = 0;
		}
		if( len ) {
			decodeblock( in, out );
			for( i = 0; i < len - 1; i++ )
				outbuf[indx_out++] = out[i];
		}
	}
}
/* end code from Bob Trower */

/* small helper functions */
/* simpleregex allocates (and reallocates) the found buffer
 * don't forget to free it when you are done
 */
static int simpleregex(char *buffer, char *regex, char **found) {
	int status;
	regex_t re;
	regmatch_t match[5];

	if (regcomp(&re, regex, 0) !=0) {
		fprintf(stderr,"internal error, regex failed!\n");
		exit(1);
	}
	status = regexec(&re,buffer,5,match,0);
	if(status == 0) {
		*found = realloc(*found,match[1].rm_eo-match[1].rm_so + 1);
		strncpy(*found,buffer+match[1].rm_so,match[1].rm_eo-match[1].rm_so);
		(*found)[match[1].rm_eo-match[1].rm_so] = '\0';
	}
	return(status == 0);
}

/* read in line of arbitrary length (important for SDA files that can
 * have lines that are tens of kB long
 * don't forget to free it when you are done
 */
#define MYGETL_INCR 1024
static char * mygetline(FILE * f) {
	size_t size = 0;
	size_t len  = 0;
	char * buf  = NULL;

	do {
		size += MYGETL_INCR;
		if ((buf = realloc(buf,size)) == NULL)
			break;
		fgets(buf+len,MYGETL_INCR,f);
		len = strlen(buf);
	} while (!feof(f) && buf[len-1]!='\n');
	return buf;
}

/* text matching, used to build very poor man's XML parser */
int matchit(FILE *infd, char *regex, char *typeregex, char **found) {
	char *buffer;

	while (!feof(infd)) {
		buffer = mygetline(infd);
		if (buffer && simpleregex(buffer,regex,found)) {
			buffer = mygetline(infd);
			if (buffer && simpleregex(buffer,typeregex,found)) {
				return 1;
			}
		}
	}
	return 0;
}


/*
 * convert the base64 data blog
 */
int uemis_convert_base64(char *base64, uint8_t **data) {
	int len,datalen;

	len = strlen(base64);
	datalen = (len/4 + 1)*3;
	if (datalen < 0x123+0x25) {
		/* less than header + 1 sample??? */
		fprintf(stderr,"suspiciously short data block\n");
	}
	*data = malloc(datalen);
	if (! *data) {
		datalen = 0;
		fprintf(stderr,"Out of memory\n");
		goto bail;
	}
	decode(base64, *data, len);

	if (memcmp(*data,"Dive\01\00\00",7))
		fprintf(stderr,"Missing Dive100 header\n");

bail:
	return datalen;
}

/*
 * parse uemis base64 data blob into struct dive
 */
static void parse_divelog_binary(char *base64, struct dive **divep) {
	int datalen;
	int i;
	uint8_t *data;
	struct sample *sample;

	datalen = uemis_convert_base64(base64, &data);

	/* first byte of divelog data is at offset 0x123 */
	i = 0x123;
	while ((i < datalen) && (*(uint16_t *)(data+i))) {
		/* it seems that a dive_time of 0 indicates the end of the valid readings */
		sample = prepare_sample(divep);
		sample->time.seconds = *(uint16_t *)(data+i);
		sample->depth.mm = (*(uint16_t *)(data+i+2) - 100) / 0.101428571 + 0.5;
		sample->temperature.mkelvin = (*(uint16_t *)(data+i+4) * 100) + 273150;
		sample->cylinderpressure.mbar= *(uint16_t *)(data+i+23) * 10;
		sample->cylinderindex = *(uint8_t *)(data+i+22);
		finish_sample(*divep, sample);
		i += 0x25;
	}
	return;
}

void
parse_uemis_file(char *divelogfilename,GError **error) {
	char *found=NULL;
	struct tm tm;
	struct dive *dive;

	FILE *divelogfile = fopen(divelogfilename,"r");

	dive = alloc_dive();

	if (! matchit(divelogfile,"val key=\"date\"","<ts>\\([^<]*\\)</ts>",&found)) {
		/* some error handling */
		goto bail;
	}
	strptime(found, "%Y-%m-%dT%H:%M:%S", &tm);
	dive->when = utc_mktime(&tm);
	if (! matchit(divelogfile,"<val key=\"duration\">",
			"<float>\\([0-9.]*\\)</float>",	&found)) {
		/* some error handling */
		goto bail;
	}
	dive->duration.seconds = 60.0 * atof(found);

	if (! matchit(divelogfile,"<val key=\"depth\">",
			"<int>\\([0-9.]*\\)</int>", &found)) {
		/* some error handling */
		goto bail;
	}
	dive->maxdepth.mm = atof(found) / 0.10143 + 0.5;

	if (! matchit(divelogfile,"<val key=\"file_content\">",
			">\\([a-zA-Z0-9+/]*\\)<", &found)) {
		/* some error handling */
		goto bail;
	}
	parse_divelog_binary(found,&dive);
	record_dive(dive);
bail:
	if (found)
		free(found);
}

/*
 * parse the two files extracted from the SDA
 */
void
uemis_import() {
	GtkWidget *dialog;
	GtkFileFilter *filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*.SDA");
	gtk_file_filter_set_name(filter, "uemis Zurich SDA files");
	dialog = gtk_file_chooser_dialog_new("Open File",
					GTK_WINDOW(main_window),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog),filter);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		GSList *filenames;
		char *filename;
		filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

		GError *error = NULL;
		while(filenames != NULL) {
			filename = (char *)filenames->data;
			parse_uemis_file(filename, &error);
			if (error != NULL)
			{
				report_error(error);
				g_error_free(error);
				error = NULL;
			}

			g_free(filename);
			filenames = g_slist_next(filenames);
		}
		g_slist_free(filenames);
		report_dives();
	}
	gtk_widget_destroy(dialog);
}
