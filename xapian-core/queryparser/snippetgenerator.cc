/** @file snippetgenerator.cc
 * @brief SnippetGenerator class implementation
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

#include <config.h>

#include <xapian/snippetgenerator.h>
#include <xapian/types.h>

#include "snippetgenerator_internal.h"

#include "str.h"

using namespace std;
using namespace Xapian;

SnippetGenerator::SnippetGenerator(const SnippetGenerator & o) : internal(o.internal) { }

SnippetGenerator &
SnippetGenerator::operator=(const SnippetGenerator & o) {
    internal = o.internal;
    return *this;
}

SnippetGenerator::SnippetGenerator() : internal(new SnippetGenerator::Internal) { }

SnippetGenerator::~SnippetGenerator() { }

void
SnippetGenerator::set_stemmer(const Xapian::Stem & stemmer)
{
    internal->stemmer = stemmer;
}

void
SnippetGenerator::set_normalizer(Xapian::SnippetGenerator::TermNormalizer * n)
{
    internal->normalizer = n;
}

void
SnippetGenerator::add_match(const std::string & term)
{
    internal->add_match(term);
}

void
SnippetGenerator::accept_text(const Xapian::Utf8Iterator & itor)
{
    internal->accept_text(itor);
}

void
SnippetGenerator::increase_termpos(Xapian::termcount delta)
{
    internal->termpos += delta;
}

Xapian::termcount
SnippetGenerator::get_termpos() const
{
    return internal->termpos;
}

void
SnippetGenerator::set_termpos(Xapian::termcount termpos)
{
    internal->termpos = termpos;
}

std::string
SnippetGenerator::get_pre_match() const
{
    return internal->pre_match;
}

void
SnippetGenerator::set_pre_match(std::string & text)
{
    internal->pre_match = text;
}

std::string
SnippetGenerator::get_post_match() const
{
    return internal->post_match;
}

void
SnippetGenerator::set_post_match(std::string & text)
{
    internal->post_match = text;
}

std::string
SnippetGenerator::get_inter_snippet() const
{
    return internal->inter_snippet;
}

void
SnippetGenerator::set_inter_snippet(std::string & text)
{
    internal->inter_snippet = text;
}

unsigned
SnippetGenerator::get_context_length() const
{
    return internal->context_length;
}

void
SnippetGenerator::set_context_length(unsigned length)
{
    internal->context_length = length;
}

std::string
SnippetGenerator::get_snippets()
{
    internal->push_result();
    std::string s = "";
    std::deque<string>::iterator it;
    for (it = internal->snippets.begin(); it != internal->snippets.end(); ++it)
        s += *it;
    return s;
}

void
SnippetGenerator::reset()
{
    internal->reset();
}

string
SnippetGenerator::get_description() const
{
    string s("Xapian::SnippetGenerator(");
    if (internal.get()) {
	s += "stem=";
	s += internal->stemmer.get_description();
    }
    s += ")";
    return s;
}
