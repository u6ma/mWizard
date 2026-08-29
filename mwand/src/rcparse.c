/*
 * Copyright (C) 2018-2026 alx@fastestcode.org
 *
 * Modified 2026 for mWizard's mWand. See NOTICE for details.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Toolbox configuration file parser
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include "rcparse.h"

static char* get_line(void);
static char* skip_blanks(char *p);
static void parse_line(char *line, struct tb_entry *e);
static int is_bare_keyword(const char *line);
static void set_parse_error(int line, const char *text);
static struct tb_entry* add_entry(const struct tb_entry *ent);
static int parse_buffer(void);

#define MAX_PARSE_ERROR	256
static char parse_error[MAX_PARSE_ERROR];

static char *buffer = NULL;
static char *buf_ptr = NULL;
struct tb_entry *entries = NULL;

/* Get next line from global buffer */
static char* get_line(void)
{
	char *p, *cur = buf_ptr;
	
	if(*buf_ptr == '\0') return NULL;
	
	p = strchr(buf_ptr,'\n');
	
	if(p){
		buf_ptr = p + 1;
		*p = '\0';
	} else {
		p = buf_ptr;
		
		while(*p != '\0') p++;
		
		buf_ptr = p;
	}

	while(p != cur) {
		p--;
		
		if(*p == ' ' || *p == '\t')
			*p = '\0';
		else
			break;
	}
	return skip_blanks(cur);
}

static char* skip_blanks(char *p)
{
	while(*p == '\t' || *p == ' ') p++;
	return p;
}

/*
 * True for one all-caps word on its own -- the shape every built-in keyword
 * has. Digits and underscores allowed so that a later FOO_2 still matches.
 */
static int is_bare_keyword(const char *line)
{
	const char *p = line;

	if(!*p) return 0;

	for(; *p; p++){
		if(*p >= 'A' && *p <= 'Z') continue;
		if(*p >= '0' && *p <= '9') continue;
		if(*p == '_') continue;
		return 0;
	}

	/* has to start with a letter, and be more than one character */
	return (line[0] >= 'A' && line[0] <= 'Z' && line[1] != '\0');
}

/* Parses a line into the given entry structure */
static void parse_line(char *line, struct tb_entry *e)
{
	char *p = line;
	
	memset(e, 0, sizeof(struct tb_entry));
	
	if(!strcmp(line,"SEPARATOR")){
		e->type=TBE_SEPARATOR;
		return;	
	}
	
	/*
	 * A built-in rather than a command: mWinfo is the window manager's own
	 * window, and mWand asks for it over the same path the Commands menu
	 * uses. Written bare like SEPARATOR, since there is nothing to give it
	 * -- no command to run, and the label is mWand's to supply.
	 */
	if(!strcmp(line,"MWINFO")){
		e->type=TBE_MWINFO;
		return;
	}

	/* Likewise MONITORS, which posts mWmonitor, the monitor arranger. */
	if(!strcmp(line,"MONITORS")){
		e->type=TBE_MONITORS;
		return;
	}

	/*
	 * A bare keyword this build does not know: skipped, not fatal.
	 *
	 * The built-ins are written as one all-caps word with no command, so
	 * anything shaped like that and not recognised above is a keyword from
	 * a newer mWand, not a menu title -- nobody labels a menu "MONITORS".
	 *
	 * Without this it parses as a cascade, a cascade with no brace after
	 * it is a fatal error, and a fatal error in the rc file means mWand
	 * does not start at all. So installing a config one release ahead of
	 * the binary -- which is a normal thing to end up with, since mWand is
	 * built by a separate make target from the files it reads -- left the
	 * user with no panel and a parser message about menu scope. A keyword
	 * from the future should cost its own menu entry and nothing else.
	 */
	if(is_bare_keyword(line)){
		/*
		 * The title is kept: a brace on the next line says this was a
		 * menu after all, and parse_buffer() turns it back into one.
		 */
		e->title = line;
		e->type = TBE_IGNORE;
		return;
	}
	
	e->title = line;
	
	while(*p != '\0'){
		if(*p == '\\' && (p[1] == '\\' || p[1] == '&' || p[1] == ':')){
			memmove(p, p + 1, strlen(p + 1) + 1);
		} else if(*p == '&') {
			e->mnemonic = p[1];
			memmove(p, p + 1,strlen(p + 1) + 1);
		} else if(*p == ':') {
			e->command = skip_blanks(p + 1);
			*p = '\0';
			break;
		}
		p++;
	}
	if(e->command)
		e->type = TBE_COMMAND;
	else
		e->type = TBE_CASCADE;
}

/* True if a top level line opens the named block. */
static int is_block_header(const char *line, const char *kw)
{
	const char *p = line;
	int i;

	for(i = 0; kw[i]; i++) {
		if(tolower((unsigned char)p[i]) != kw[i]) return 0;
	}
	p += i;

	/* only whitespace or the opening brace may follow */
	while(*p == ' ' || *p == '\t') p++;

	return (*p == '\0' || *p == '{');
}

/*
 * True if a top level line opens a block that is not menu syntax.
 *
 * Both are read by config.c before this parser ever runs -- Settings into
 * the resource database, Variables into the environment -- and neither
 * holds menu entries, so they have to be stepped over here.
 */
static int is_settings_header(const char *line)
{
	return (is_block_header(line, "settings") ||
		is_block_header(line, "variables"));
}

/* Parses the global buffer */
static int parse_buffer(void)
{
	char *line;
	struct tb_entry tmp;
	struct tb_entry *prev = NULL;
	int nlevel = 0;
	int iline = 0;
	int skip_level = 0;
	int skipping = 0;
	
	while((line = get_line())){
		iline++;
		
		if(*line == '\0' || *line == '#' || *line == '!'){
			continue;
		}

		/*
		 * Step over the Settings block. Depth is counted so that a brace
		 * inside it cannot end the skip early.
		 */
		if(skipping) {
			char *q;

			for(q = line; *q; q++) {
				if(*q == '{') skip_level++;
				else if(*q == '}') {
					skip_level--;
					if(skip_level <= 0) { skipping = 0; break; }
				}
			}
			continue;
		}

		if(nlevel == 0 && is_settings_header(line)) {
			char *q;

			skipping = 1;
			skip_level = 0;
			for(q = line; *q; q++) {
				if(*q == '{') skip_level++;
				else if(*q == '}') skip_level--;
			}
			if(skip_level <= 0 && strchr(line, '{')) skipping = 0;
			continue;
		}

		if(*line == '{'){
			/*
			 * A brace settles what the line before it was.
			 *
			 * parse_line() cannot tell an unknown keyword from a
			 * menu title -- both are one bare word -- so it guesses
			 * "keyword" and this undoes the guess when the next
			 * line proves otherwise. Nothing but a menu title has a
			 * brace after it.
			 *
			 * Without this, a menu whose title happens to be one
			 * all-caps word ("SYSTEM", "TOOLS", "GAMES") was read
			 * as a keyword from a newer mWand and skipped, its
			 * brace then had no cascade to open, and that is a
			 * fatal parse error. A fatal parse error means mWand
			 * exits before it maps anything: the panel simply does
			 * not appear, and the message names a delimiter rather
			 * than the menu it came from.
			 */
			if(prev && prev->type == TBE_IGNORE && prev->title){
				prev->type = TBE_CASCADE;
			}

			if(!prev || prev->type != TBE_CASCADE){
				set_parse_error(iline,
					"Delimiter \'{\' must follow a cascade entry");
				return -1;
			}
			nlevel++;
			continue;
		}else if(*line == '}'){
			/*
			 * A menu may end with any entry except a cascade.
			 *
			 * This used to insist the last entry before a brace be
			 * a command, which quietly made "SEPARATOR", "MWINFO"
			 * or "MONITORS" as the final line of a menu a fatal
			 * parse error -- and a fatal parse error here means
			 * mWand does not start at all, with a message about
			 * delimiters that says nothing about the real cause.
			 * There is no reason a menu cannot end in a built-in.
			 *
			 * What is genuinely wrong is a brace straight after a
			 * cascade, which is an empty submenu, and that is
			 * still refused.
			 */
			if(!nlevel){
				set_parse_error(iline,"Delimiter \'}\' out of scope");
				return -1;
			}
			if(prev && prev->type == TBE_CASCADE &&
				prev->level == nlevel - 1){
				set_parse_error(iline,
					"Cascade entry has an empty menu");
				return -1;
			}
			nlevel--;
			continue;
		}else if(prev && prev->type == TBE_CASCADE && prev->level == nlevel){
			set_parse_error(iline,"Cascade entry must have a menu scope");
			return -1;
		}

		parse_line(line, &tmp);
		tmp.level = nlevel;

		if(tmp.type == TBE_COMMAND) {
			if(tmp.level < 1){
				set_parse_error(iline,
					"Command entries must reside within a menu scope");
				return -1;
			}
			if(!strlen(tmp.command)) {
				set_parse_error(iline,
					"Command string expected after ':' ");
				return -1;
			}
		} else if(tmp.type == TBE_CASCADE) {
			size_t len = strlen(tmp.title);
			char *p = tmp.title + (len - 1);
			/* allow { on the same line as menu title */
			if(*p == '{') {
				nlevel++;
				*p = '\0';
				p--;
				
				while(p != tmp.title && (*p == ' ' || *p == '\t')) {
					*p = '\0';
					p--;
				}
			}
		}
		
		if((prev = add_entry(&tmp)) == NULL) return ENOMEM;
	}
	return 0;
}

static void set_parse_error(int line, const char *text)
{
	snprintf(parse_error,MAX_PARSE_ERROR,"Line %d: %s",line,text);
}

/* Duplicates the given entry and adds it to the global list */
static struct tb_entry* add_entry(const struct tb_entry *ent)
{
	struct tb_entry *new;

	new = malloc(sizeof(struct tb_entry));
	if(!new) return NULL;
	memcpy(new, ent, sizeof(struct tb_entry));
	
	if(!entries){
		entries = new;
		new->next = NULL;
	} else {
		struct tb_entry *last = entries;
		while(last->next) last = last->next;
		
		last->next = new;
	}
	return new;
}

/*
 * Parses a toolbox menu file.
 * Returns zero on success or errno otherwise.
 * If EINVAL is returned, the file contains syntax errors, and
 * tb_parser_error_string() may be used to obtain detailed information.
 */
int tb_parse_config(const char *filename, struct tb_entry **ent_root)
{
	FILE *file;
	struct stat st;
	int err;
	char *old_buf = buffer;
	struct tb_entry *old_ent = entries;
	
	entries = NULL;
	parse_error[0]='\0';
	
	if(stat(filename, &st) < 0) return errno;
	if(st.st_size == 0) return EIO;
	
	if(!(buffer = malloc(st.st_size + 1))) {
		buffer = old_buf;
		return errno;
	}

	buf_ptr = buffer;
	
	file = fopen(filename, "r");
	if(!file){
		err = errno;
		free(buffer);
		buffer = old_buf;
		return err;
	}

	if(fread(buffer, 1, st.st_size, file) < st.st_size){
		err = errno;
		free(buffer);
		fclose(file);
		buffer = old_buf;
		return err;
	}
	fclose(file);
		
	buffer[st.st_size] = '\0';

	if((err = parse_buffer())){
		free(buffer);
		buffer = old_buf;
		return err;
	}

	if(old_ent){
		struct tb_entry *tmp, *cur = old_ent;
		while(cur){
			tmp = cur;
			cur = cur->next;
			free(tmp);
		}
	}

	*ent_root = entries;

	return 0;
}

char* tb_parser_error_string(void)
{
	return (parse_error[0] == '\0') ? NULL : parse_error;
}
