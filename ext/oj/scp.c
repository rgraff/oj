/* scp.c
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
#include <math.h>
#include <sys/types.h>
#include <unistd.h>

#include "oj.h"
#include "parse.h"

inline static int
respond_to(VALUE obj, ID method) {
#ifdef JRUBY_RUBY
    /* There is a bug in JRuby where rb_respond_to() returns true (1) even if
     * a method is private. */
    {
	VALUE	args[1];

	*args = ID2SYM(method);
	return (Qtrue == rb_funcall2(obj, rb_intern("respond_to?"), 1, args));
    }
#else
    return rb_respond_to(obj, method);
#endif
}


static VALUE
start_hash(ParseInfo pi) {
    rb_funcall((VALUE)pi->cbc, oj_hash_start_id, 0);

    return Qnil;
}

static void
end_hash(ParseInfo pi) {
    rb_funcall((VALUE)pi->cbc, oj_hash_end_id, 0);
}

static VALUE
start_array(ParseInfo pi) {
    rb_funcall((VALUE)pi->cbc, oj_array_start_id, 0);

    return Qnil;
}

static void
end_array(ParseInfo pi) {
    rb_funcall((VALUE)pi->cbc, oj_array_end_id, 0);
}

static void
add_value(ParseInfo pi, VALUE val) {
     rb_funcall((VALUE)pi->cbc, oj_add_value_id, 1, val);
}

VALUE
oj_sc_parse(int argc, VALUE *argv, VALUE self) {
    struct _ParseInfo	pi;
    char		*buf = 0;
    VALUE		input;
    VALUE		handler;

    if (argc < 2) {
	rb_raise(rb_eArgError, "Wrong number of arguments to saj_parse.");
    }
    handler = *argv;;
    input = argv[1];
    pi.options = oj_default_options;
    if (3 == argc) {
	oj_parse_options(argv[2], &pi.options);
    }
    pi.cbc = (void*)handler;

    pi.start_hash = respond_to(handler, oj_hash_start_id) ? start_hash : 0;
    pi.end_hash = respond_to(handler, oj_hash_end_id) ? end_hash : 0;
    pi.start_array = respond_to(handler, oj_array_start_id) ? start_array : 0;
    pi.end_array = respond_to(handler, oj_array_end_id) ? end_array : 0;
    pi.add_value = respond_to(handler, oj_add_value_id) ? add_value : 0;
    pi.add_cstr = 0;

    if (rb_type(input) == T_STRING) {
	pi.json = StringValuePtr(input);
    } else {
	VALUE	clas = rb_obj_class(input);
	VALUE	s;

	if (oj_stringio_class == clas) {
	    s = rb_funcall2(input, oj_string_id, 0, 0);
	    pi.json = StringValuePtr(input);
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
#endif
#endif
	} else if (rb_respond_to(input, oj_read_id)) {
	    s = rb_funcall2(input, oj_read_id, 0, 0);
	    pi.json = StringValuePtr(s);
	} else {
	    rb_raise(rb_eArgError, "saj_parse() expected a String or IO Object.");
	}
    }
    oj_parse2(&pi);
    if (0 != buf) {
	xfree(buf);
    }
    stack_cleanup(&pi.stack);
    if (err_has(&pi.err)) {
	oj_err_raise(&pi.err);
    }
    return Qnil;
}
