/** @file snippetgenerator_internal.cc
 * @brief SnippetGenerator class internals
 */
/* Copyright (C) 2007,2010,2011 Olly Betts
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <config.h>

#include "snippetgenerator_internal.h"

#include <xapian/queryparser.h>
#include <xapian/unicode.h>

#include "stringutils.h"

#include <limits>
#include <string>
#include "cjk-tokenizer.h"

using namespace std;

namespace Xapian {

// 'ngram_len' is the length in *characters* (not octets) of an N-gram,
// or 0 if 'term' is a complete term.  We use this to detect when we
// are being passed N-grams.  We also rely on the behaviour of the
// CJKTokenizer class which returns N-grams in increasing length order,
// so that when ngram_len==1 we know the N-gram base position just
// incremented.

void
SnippetGenerator::Internal::accept_term(const string & term,
					termcount pos, int ngram_len)
{
    string stem;
    if (normalizer.get()) {
        stem = normalizer->normalize(term);
    } else {
        stem = term;
    }
    stem = Unicode::tolower(stem);
    if (stemmer.internal.get())
        stem = stemmer(stem);

    // we don't keep context across termpos discontinuities
    if (pos > (lastpos+2)) {
	context.clear();
	leading_nonword = "";
	pending_1gram = "";
	ignore_1grams = 0;
    }
    if (ngram_len <= 1)
	xpos += (pos - lastpos);
    lastpos = pos;
    nwhitespace = 0;

    if (matches.find(stem) != matches.end()) {
	// found a match
	if (xpos > horizon + context.size() + 1 && result != "") {
	    // there was a gap from the end of the context after
	    // the previous snippet, so start a new snippet
	    push_result();
	    result += inter_snippet;
	} else {
	    result += leading_nonword;
	}
	leading_nonword = "";

	if (ngram_len == 1 && pending_1gram != "") {
	    push_context(pending_1gram);
	    pending_1gram = "";
	}

	// flush the before-context
	while (!context.empty()) {
	    result += context.front();
	    context.pop_front();
	}

	// emit the match, highlighted
	result += pre_match;
	result += term;
	result += post_match;
	match_cover.insert(term);

	// some following 1-grams may be included in the
	// match text, so don't add them to context.
	ignore_1grams = (ngram_len > 1 ? ngram_len-1 : 0);

	// set the horizon to mark the end of the after-context
	horizon = xpos + context_length + ignore_1grams;
    } else if (xpos <= horizon) {
	// the after-context for a match
	if (ngram_len == 0) {
	    result += term;
	} else if (ngram_len == 1) {
	    if (ignore_1grams)
		ignore_1grams--;
	    else
		result += term;
	}
	// don't keep N>1 N-grams in context, they're redundant
    } else {
	// not explicitly in a context, but remember the
	// term in the context queue for later
	if (ngram_len == 0) {
	    push_context(term);
	} else if (ngram_len == 1) {
	    if (pending_1gram != "") {
		push_context(pending_1gram);
		pending_1gram = "";
	    }
	    if (ignore_1grams)
		ignore_1grams--;
	    else
		pending_1gram = term;
	}
	// don't keep N>1 N-grams in context, they're redundant
    }
}

// push_result pushes the current snippet result in the snippet
// queue. It ensures the following invariants:
//
// - The head of the queue always contains the result with the
//   highest matchcount seen in the text.
//
// - Any following members of the queue have the same matchcount
//   as head and are prepended with inter_snippet. They occur in
//   the same order as in the original text.
//
// The matchcount is defined as the number of search terms which
// occur at least once in the snippet.
void
SnippetGenerator::Internal::push_result()
{
    unsigned matchcount = match_cover.size();

    if (result.empty() || !matchcount)
        return;

    if (matchcount > best_matchcount) {
        snippets.clear();
        best_matchcount = matchcount;
        snippets.push_back(result);
    } else if (matchcount == best_matchcount) {
        snippets.push_back(result);
    }

    result = "";
    match_cover.clear();
}

void
SnippetGenerator::Internal::push_context(const string & term)
{
    context.push_back(term);
    while (context.size() > context_length) {
	context.pop_front();
	leading_nonword = "";
    }
    // this order handles the context_length=0 case gracefully
}

void
SnippetGenerator::Internal::accept_nonword_char(unsigned ch, termcount pos)
{
    if (context.empty() && leading_nonword != "") {
	Unicode::append_utf8(leading_nonword, ch);
	return;
    }
    xpos += (pos - lastpos);

    if (Unicode::is_whitespace(ch)) {
	if (++nwhitespace > 1)
	    return;
	ch = ' ';
    } else {
	nwhitespace = 0;
    }

    if (pending_1gram != "") {
	push_context(pending_1gram);
	pending_1gram = "";
    }
    ignore_1grams = 0;

    if (!pos) {
	// non-word characters before the first word
	Unicode::append_utf8(leading_nonword, ch);
    }
    else if (xpos <= horizon) {
	// the last word of the after-context of a snippet
	if (ch == ' ' && xpos == horizon) {
	    // after-context ends on first whitespace
	    // after the last word in the horizon,
	    // ...unless another one abuts it, but we
	    // don't know that yet, so keep it around
	    // just in case
	    Unicode::append_utf8(leading_nonword, ch);
	    return;
	}

	// the after-context for a match
	Unicode::append_utf8(result, ch);
    } else {
	if (context.size())
	    Unicode::append_utf8(context.back(), ch);
    }
}

// Put a limit on the size of terms to help prevent the index being bloated
// by useless junk terms.
static const unsigned int MAX_PROB_TERM_LENGTH = 64;
// FIXME: threshold is currently in bytes of UTF-8 representation, not unicode
// characters - what actually makes most sense here?

inline bool
U_isupper(unsigned ch) {
    return (ch < 128 && C_isupper((unsigned char)ch));
}

inline unsigned check_wordchar(unsigned ch) {
    if (Unicode::is_wordchar(ch)) return ch;
    return 0;
}

/** Value representing "ignore this" when returned by check_infix() or
 *  check_infix_digit().
 */
const unsigned UNICODE_IGNORE = numeric_limits<unsigned>::max();

inline unsigned check_infix(unsigned ch) {
    if (ch == '\'' || ch == '&' || ch == 0xb7 || ch == 0x5f4 || ch == 0x2027) {
	// Unicode includes all these except '&' in its word boundary rules,
	// as well as 0x2019 (which we handle below) and ':' (for Swedish
	// apparently, but we ignore this for now as it's problematic in
	// real world cases).
	return ch;
    }
    // 0x2019 is Unicode apostrophe and single closing quote.
    // 0x201b is Unicode single opening quote with the tail rising.
    if (ch == 0x2019 || ch == 0x201b) return '\'';
    if (ch >= 0x200b && (ch <= 0x200d || ch == 0x2060 || ch == 0xfeff))
	return UNICODE_IGNORE;
    return 0;
}

inline unsigned check_infix_digit(unsigned ch) {
    // This list of characters comes from Unicode's word identifying algorithm.
    switch (ch) {
	case ',':
	case '.':
	case ';':
	case 0x037e: // GREEK QUESTION MARK
	case 0x0589: // ARMENIAN FULL STOP
	case 0x060D: // ARABIC DATE SEPARATOR
	case 0x07F8: // NKO COMMA
	case 0x2044: // FRACTION SLASH
	case 0xFE10: // PRESENTATION FORM FOR VERTICAL COMMA
	case 0xFE13: // PRESENTATION FORM FOR VERTICAL COLON
	case 0xFE14: // PRESENTATION FORM FOR VERTICAL SEMICOLON
	    return ch;
    }
    if (ch >= 0x200b && (ch <= 0x200d || ch == 0x2060 || ch == 0xfeff))
	return UNICODE_IGNORE;
    return 0;
}

inline bool
is_digit(unsigned ch) {
    return (Unicode::get_category(ch) == Unicode::DECIMAL_DIGIT_NUMBER);
}

inline unsigned check_suffix(unsigned ch) {
    if (ch == '+' || ch == '#') return ch;
    // FIXME: what about '-'?
    return 0;
}

void
SnippetGenerator::Internal::accept_text(Utf8Iterator itor)
{
    bool cjk_ngram = CJK::is_cjk_enabled();

    while (true) {
	// Advance to the start of the next term.
	unsigned ch;
	while (true) {
	    if (itor == Utf8Iterator()) return;
	    ch = check_wordchar(*itor);
	    if (ch) break;
	    accept_nonword_char(*itor, termpos);
	    ++itor;
	}

	string term;
	// Look for initials separated by '.' (e.g. P.T.O., U.N.C.L.E).
	// Don't worry if there's a trailing '.' or not.
	if (U_isupper(*itor)) {
	    const Utf8Iterator end;
	    Utf8Iterator p = itor;
	    do {
		Unicode::append_utf8(term, Unicode::tolower(*p++));
	    } while (p != end && *p == '.' && ++p != end && U_isupper(*p));
	    // One letter does not make an acronym!  If we handled a single
	    // uppercase letter here, we wouldn't catch M&S below.
	    if (term.size() > 1) {
		// Check there's not a (lower case) letter or digit
		// immediately after it.
		if (p == end || !Unicode::is_wordchar(*p)) {
		    itor = p;
		    goto endofterm;
		}
	    }
	    term.resize(0);
	}

	while (true) {
	    if (cjk_ngram &&
		CJK::codepoint_is_cjk(*itor) &&
		Unicode::is_wordchar(*itor)) {
		const string & cjk = CJK::get_cjk(itor);
		for (CJKTokenIterator tk(cjk); tk != CJKTokenIterator(); ++tk) {
		    const string & cjk_token = *tk;
		    if (cjk_token.size() > MAX_PROB_TERM_LENGTH) continue;

		    // Add unstemmed form positional information.
		    accept_term(cjk_token, ++termpos, tk.get_length());
		}
		while (true) {
		    if (itor == Utf8Iterator()) return;
		    ch = check_wordchar(*itor);
		    if (ch) break;
		    accept_nonword_char(*itor, termpos);
		    ++itor;
		}
	    }
	    unsigned prevch;
	    do {
		Unicode::append_utf8(term, ch);
		prevch = ch;
		if (++itor == Utf8Iterator() ||
		    (cjk_ngram && CJK::codepoint_is_cjk(*itor)))
		    goto endofterm;
		ch = check_wordchar(*itor);
	    } while (ch);

	    Utf8Iterator next(itor);
	    ++next;
	    if (next == Utf8Iterator()) break;
	    unsigned nextch = check_wordchar(*next);
	    if (!nextch) break;
	    unsigned infix_ch = *itor;
	    if (is_digit(prevch) && is_digit(*next)) {
		infix_ch = check_infix_digit(infix_ch);
	    } else {
		// Handle things like '&' in AT&T, apostrophes, etc.
		infix_ch = check_infix(infix_ch);
	    }
	    if (!infix_ch) break;
	    if (infix_ch != UNICODE_IGNORE)
		Unicode::append_utf8(term, infix_ch);
	    ch = nextch;
	    itor = next;
	}

	{
	    size_t len = term.size();
	    unsigned count = 0;
	    while ((ch = check_suffix(*itor))) {
		if (++count > 3) {
		    term.resize(len);
		    break;
		}
		Unicode::append_utf8(term, ch);
		if (++itor == Utf8Iterator()) goto endofterm;
	    }
	    // Don't index fish+chips as fish+ chips.
	    if (Unicode::is_wordchar(*itor))
		term.resize(len);
	}

endofterm:
	if (term.size() > MAX_PROB_TERM_LENGTH) continue;
	accept_term(term, ++termpos, 0);
    }
}

void
SnippetGenerator::Internal::add_match(const std::string & str)
{
    unsigned ch;
    Utf8Iterator itor(str);
    while (true) {

	// skip non-word characters
	while (true) {
	    if (itor == Utf8Iterator()) return;
	    ch = check_wordchar(*itor);
	    ++itor;
	    if (ch) break;
	}

	string term;
	Unicode::append_utf8(term, ch);

#if 0
	// treat CJK characters as 1-character terms
	// this is probably wrong.
	if (CJK::codepoint_is_cjk(ch)) {
	    matches.insert(term);
	    continue;
	}
#endif

	// collect word characters into a term
	while (true) {
	    if (itor == Utf8Iterator()) break;
	    ch = check_wordchar(*itor);
	    if (!ch) break;
	    Unicode::append_utf8(term, ch);
	    ++itor;
	}

	string stem = Unicode::tolower(term);
	if (stemmer.internal.get())
	    stem = stemmer(stem);
	matches.insert(stem);
    }
}

void
SnippetGenerator::Internal::reset()
{
    result = "";
    horizon = 0;
    lastpos = 0;
    xpos = 0;
    nwhitespace = 0;
    context.clear();
    matches.clear();
    termpos = 0;
    leading_nonword = "";
    pending_1gram = "";
    ignore_1grams = 0;
    best_matchcount = 0;
    snippets.clear();
    match_cover.clear();
    if (normalizer.get()) normalizer->reset();
}

}
