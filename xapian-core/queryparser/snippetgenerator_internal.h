/** @file snippetgenerator_internal.h
 * @brief SnippetGenerator class internals
 */
/* Copyright (C) 2007 Olly Betts
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

#ifndef XAPIAN_INCLUDED_SNIPPETGENERATOR_INTERNAL_H
#define XAPIAN_INCLUDED_SNIPPETGENERATOR_INTERNAL_H

#include <xapian/snippetgenerator.h>
#include <xapian/queryparser.h>
#include <xapian/stem.h>
#include <tr1/unordered_set>
#include <queue>

namespace Xapian {

class SnippetGenerator::Internal : public Xapian::Internal::intrusive_base {
    friend class SnippetGenerator;
    Stem stemmer;
    std::string pre_match;
    std::string post_match;
    std::string inter_snippet;
    termcount context_length;
    std::tr1::unordered_set<std::string> matches;
    unsigned nwhitespace;
    std::string leading_nonword;
    std::string pending_1gram;
    unsigned ignore_1grams;

    termcount horizon;
    termcount lastpos;
    termcount xpos;	// doesn't count N>1 N-grams
    std::deque<std::string> context;
    std::string result;

    termcount termpos;

    void push_context(const std::string & term);
    void accept_term(const std::string & term, termcount pos, int ngram_len);
    void accept_nonword_char(unsigned ch, termcount pos);

  public:
    Internal() : pre_match("<b>"), post_match("</b>"),
		 inter_snippet("..."), context_length(5),
		 nwhitespace(0), horizon(0), lastpos(0),
		 termpos(0) { }
    void add_match(const std::string & term);
    void accept_text(Utf8Iterator itor);
    void reset();
};

}

#endif // XAPIAN_INCLUDED_SNIPPETGENERATOR_INTERNAL_H
