/* strict.c
 * Copyright (c) 2012, Peter Ohler
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *  - Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 *  - Neither the name of Peter Ohler nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "oj.h"
#include "err.h"
#include "parse.h"

static void
add_value(ParseInfo pi, VALUE val) {
    Val	parent = stack_peek(&pi->stack);

    if (0 == parent) {
	pi->stack.head->val = val;
    } else if (TYPE_ARRAY == parent->type) {
	rb_ary_push(parent->val, val);
    } else if (TYPE_HASH == parent->type) {
	switch (parent->next) {
	case NEXT_HASH_COLON:
	    // nothing to do yet
	    break;
	case NEXT_HASH_COMMA:
	    {
		Val	keyVal = stack_prev(&pi->stack);

		if (0 != keyVal) {
		    rb_hash_aset(parent->val, keyVal->val, val);
		} else {
		    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected hash key");
		}
	    }
	    break;
	default:
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected comma or colon in hash parse");
	    break;
	}
    }
}

static VALUE
start_hash(ParseInfo pi) {
    VALUE	hash = rb_hash_new();

    add_value(pi, hash);

    return hash;
}

static VALUE
start_array(ParseInfo pi) {
    VALUE	array = rb_ary_new();

    add_value(pi, array);

    return array;
}

VALUE
oj_strict_parse(int argc, VALUE *argv, VALUE self) {
    struct _ParseInfo	pi;
    char		*buf = 0;
    VALUE		input;
    VALUE		result = Qnil;

    if (argc < 1) {
	rb_raise(rb_eArgError, "Wrong number of arguments to strict_parse.");
    }
    input = argv[0];
    pi.options = oj_default_options;
    if (2 == argc) {
	oj_parse_options(argv[1], &pi.options);
	
    }
    pi.cbc = (void*)0;

    pi.start_hash = start_hash;
    pi.end_hash = 0;
    pi.start_array = start_array;
    pi.end_array = 0;
    pi.add_value = add_value;
    pi.add_cstr = 0;

    if (rb_type(input) == T_STRING) {
	pi.json = StringValuePtr(input);
    } else {
	VALUE	clas = rb_obj_class(input);
	VALUE	s;

	if (oj_stringio_class == clas) {
	    s = rb_funcall2(input, oj_string_id, 0, 0);
	    pi.json = StringValuePtr(s);
#ifndef JRUBY_RUBY
#if !IS_WINDOWS
	    // JRuby gets confused with what is the real fileno.
	} else if (rb_respond_to(input, oj_fileno_id) && Qnil != (s = rb_funcall(input, oj_fileno_id, 0))) {
	    int		fd = FIX2INT(s);
	    ssize_t	cnt;
	    size_t	len = lseek(fd, 0, SEEK_END);

	    lseek(fd, 0, SEEK_SET);
	    if (pi.options.max_stack < len) {
		buf = ALLOC_N(char, len + 1);
		pi.json = buf;
	    } else {
		pi.json = ALLOCA_N(char, len + 1);
	    }
	    if (0 >= (cnt = read(fd, (char*)pi.json, len)) || cnt != (ssize_t)len) {
		if (0 != buf) {
		    xfree(buf);
		}
		rb_raise(rb_eIOError, "failed to read from IO Object.");
	    }
	    ((char*)pi.json)[len] = '\0';
	    /* skip UTF-8 BOM if present */
	    if (0xEF == (uint8_t)*pi.json && 0xBB == (uint8_t)pi.json[1] && 0xBF == (uint8_t)pi.json[2]) {
		pi.json += 3;
	    }
#endif
#endif
	} else if (rb_respond_to(input, oj_read_id)) {
	    s = rb_funcall2(input, oj_read_id, 0, 0);
	    pi.json = StringValuePtr(s);
	} else {
	    rb_raise(rb_eArgError, "strict_parse() expected a String or IO Object.");
	}
    }
    oj_parse2(&pi);
    if (0 != buf) {
	xfree(buf);
    }
    result = stack_head_val(&pi.stack);
    stack_cleanup(&pi.stack);
    if (err_has(&pi.err)) {
	oj_err_raise(&pi.err);
    }
    return result;
}
