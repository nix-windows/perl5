/*    dquote.c
 *
 * This file contains functions that are related to
 * parsing double-quotish expressions.
 *
*/

#include "EXTERN.h"
#define PERL_IN_DQUOTE_C
#include "perl.h"
#include "dquote_inline.h"

/* XXX Add documentation after final interface and behavior is decided */
/* May want to show context for error, so would pass S_grok_bslash_c(pTHX_ const char* current, const char* start, const bool output_warning)
    U8 source = *current;
*/

char
Perl_grok_bslash_c(pTHX_ const char source, const bool output_warning)
{

    U8 result;

    if (! isPRINT_A(source)) {
        Perl_croak(aTHX_ "%s",
                        "Character following \"\\c\" must be printable ASCII");
    }
    else if (source == '{') {
        const char control = toCTRL('{');
        if (isPRINT_A(control)) {
            /* diag_listed_as: Use "%s" instead of "%s" */
            Perl_croak(aTHX_ "Use \"%c\" instead of \"\\c{\"", control);
        }
        else {
            Perl_croak(aTHX_ "Sequence \"\\c{\" invalid");
        }
    }

    result = toCTRL(source);
    if (output_warning && isPRINT_A(result)) {
        U8 clearer[3];
        U8 i = 0;
        if (! isWORDCHAR(result)) {
            clearer[i++] = '\\';
        }
        clearer[i++] = result;
        clearer[i++] = '\0';

        Perl_ck_warner(aTHX_ packWARN(WARN_SYNTAX),
                        "\"\\c%c\" is more clearly written simply as \"%s\"",
                        source,
                        clearer);
    }

    return result;
}

bool
Perl_grok_bslash_o(pTHX_ char **s, const char * const send, UV *uv,
                      const char** message,
                      const bool output_warning, const bool strict,
                      const bool UTF)
{

/*  Documentation to be supplied when interface nailed down finally
 *  This returns FALSE if there is an error which the caller need not recover
 *  from; otherwise TRUE.  In either case the caller should look at *len [???].
 *  It guarantees that the returned codepoint, *uv, when expressed as
 *  utf8 bytes, would fit within the skipped "\o{...}" bytes.
 *  On input:
 *	s   is the address of a pointer to a string.  **s is 'o', and the
 *	    previous character was a backslash.  At exit, *s will be advanced
 *	    to the byte just after those absorbed by this function.  Hence the
 *	    caller can continue parsing from there.  In the case of an error,
 *	    this routine has generally positioned *s to point just to the right
 *	    of the first bad spot, so that a message that has a "<--" to mark
 *	    the spot will be correctly positioned.
 *	send - 1  gives a limit in *s that this function is not permitted to
 *	    look beyond.  That is, the function may look at bytes only in the
 *	    range *s..send-1
 *	uv  points to a UV that will hold the output value, valid only if the
 *	    return from the function is TRUE
 *      message is a pointer that will be set to an internal buffer giving an
 *	    error message upon failure (the return is FALSE).  Untouched if
 *	    function succeeds
 *	output_warning says whether to output any warning messages, or suppress
 *	    them
 *	strict is true if this should fail instead of warn if there are
 *	    non-octal digits within the braces
 *	UTF is true iff the string *s is encoded in UTF-8.
 */
    char* e;
    STRLEN numbers_len;
    I32 flags = PERL_SCAN_ALLOW_UNDERSCORES
		| PERL_SCAN_DISALLOW_PREFIX
                | PERL_SCAN_SILENT_NON_PORTABLE
	        | PERL_SCAN_SILENT_ILLDIGIT;

    PERL_ARGS_ASSERT_GROK_BSLASH_O;

    assert(*(*s - 1) == '\\');
    assert(* *s       == 'o');
    (*s)++;

    if (send <= *s || **s != '{') {
	*message = "Missing braces on \\o{}";
	return FALSE;
    }

    e = (char *) memchr(*s, '}', send - *s);
    if (!e) {
        (*s)++;  /* Move past the '{' */
        while (isOCTAL(**s)) { /* Position beyond the legal digits */
            (*s)++;
        }
        *message = "Missing right brace on \\o{";
	return FALSE;
    }

    (*s)++;    /* Point to expected first digit (could be first byte of utf8
                  sequence if not a digit) */
    numbers_len = e - *s;
    if (numbers_len == 0) {
        (*s)++;    /* Move past the } */
	*message = "Empty \\o{}";
	return FALSE;
    }

    *uv = grok_oct(*s, &numbers_len, &flags, NULL);
    /* Note that if has non-octal, will ignore everything starting with that up
     * to the '}' */

    if (numbers_len != (STRLEN) (e - *s)) {
        if (strict) {
            *s += numbers_len;
            *s += (UTF) ? UTF8_SAFE_SKIP(*s, send) : 1;
            *message = "Non-octal character";
            return FALSE;
        }
        else if (output_warning) {
            Perl_ck_warner(aTHX_ packWARN(WARN_DIGIT),
            /* diag_listed_as: Non-octal character '%c'.  Resolved as "%s" */
                        "Non-octal character '%c'.  Resolved as \"\\o{%.*s}\"",
                        *(*s + numbers_len),
                        (int) numbers_len,
                        *s);
        }
    }

    /* Return past the '}' */
    *s = e + 1;

    return TRUE;
}

bool
Perl_grok_bslash_x(pTHX_ char **s, const char * const send, UV *uv,
                      const char** message,
                      const bool output_warning, const bool strict,
                      const bool UTF)
{

/*  Documentation to be supplied when interface nailed down finally
 *  This returns FALSE if there is an error which the caller need not recover
 *  from; otherwise TRUE.
 *  It guarantees that the returned codepoint, *uv, when expressed as
 *  utf8 bytes, would fit within the skipped "\x{...}" bytes.
 *
 *  On input:
 *	s   is the address of a pointer to a string.  **s is 'x', and the
 *	    previous character was a backslash.  At exit, *s will be advanced
 *	    to the byte just after those absorbed by this function.  Hence the
 *	    caller can continue parsing from there.  In the case of an error,
 *	    this routine has generally positioned *s to point just to the right
 *	    of the first bad spot, so that a message that has a "<--" to mark
 *	    the spot will be correctly positioned.
 *	send - 1  gives a limit in *s that this function is not permitted to
 *	    look beyond.  That is, the function may look at bytes only in the
 *	    range *s..send-1
 *	uv  points to a UV that will hold the output value, valid only if the
 *	    return from the function is TRUE
 *      message is a pointer that will be set to an internal buffer giving an
 *	    error message upon failure (the return is FALSE).  Untouched if
 *	    function succeeds
 *	output_warning says whether to output any warning messages, or suppress
 *	    them
 *	strict is true if anything out of the ordinary should cause this to
 *	    fail instead of warn or be silent.  For example, it requires
 *	    exactly 2 digits following the \x (when there are no braces).
 *	    3 digits could be a mistake, so is forbidden in this mode.
 *	UTF is true iff the string *s is encoded in UTF-8.
 */
    char* e;
    STRLEN numbers_len;
    I32 flags = PERL_SCAN_DISALLOW_PREFIX
              | PERL_SCAN_SILENT_NON_PORTABLE;


    PERL_ARGS_ASSERT_GROK_BSLASH_X;

    assert(*(*s - 1) == '\\');
    assert(* *s      == 'x');

    (*s)++;

    if (send <= *s) {
        if (strict) {
            *message = "Empty \\x";
            return FALSE;
        }

        /* Sadly, to preserve backcompat, an empty \x at the end of string is
         * interpreted as a NUL */
        *uv = 0;
        return TRUE;
    }

    if (strict || ! output_warning) {
        flags |= PERL_SCAN_SILENT_ILLDIGIT;
    }

    if (**s != '{') {
        STRLEN len = (strict) ? 3 : 2;

	*uv = grok_hex(*s, &len, &flags, NULL);
	*s += len;
        if (strict && len != 2) {
            if (len < 2) {
                *s += (UTF) ? UTF8_SAFE_SKIP(*s, send) : 1;
                *message = "Non-hex character";
            }
            else {
                *message = "Use \\x{...} for more than two hex characters";
            }
            return FALSE;
        }
	return TRUE;
    }

    e = (char *) memchr(*s, '}', send - *s);
    if (!e) {
        (*s)++;  /* Move past the '{' */
        while (isXDIGIT(**s)) { /* Position beyond the legal digits */
            (*s)++;
        }
        /* XXX The corresponding message above for \o is just '\\o{'; other
         * messages for other constructs include the '}', so are inconsistent.
         */
	*message = "Missing right brace on \\x{}";
	return FALSE;
    }

    (*s)++;    /* Point to expected first digit (could be first byte of utf8
                  sequence if not a digit) */
    numbers_len = e - *s;
    if (numbers_len == 0) {
        if (strict) {
            (*s)++;    /* Move past the } */
            *message = "Empty \\x{}";
            return FALSE;
        }
        *s = e + 1;
        *uv = 0;
        return TRUE;
    }

    flags |= PERL_SCAN_ALLOW_UNDERSCORES;

    *uv = grok_hex(*s, &numbers_len, &flags, NULL);
    /* Note that if has non-hex, will ignore everything starting with that up
     * to the '}' */

    if (strict && numbers_len != (STRLEN) (e - *s)) {
        *s += numbers_len;
        *s += (UTF) ? UTF8_SAFE_SKIP(*s, send) : 1;
        *message = "Non-hex character";
        return FALSE;
    }

    /* Return past the '}' */
    *s = e + 1;

    return TRUE;
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
