/** @file snippetgenerator.h
 * @brief parse free text and generate snippets
 */
/* Copyright (C) 2007,2009,2011 Olly Betts
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

#ifndef XAPIAN_INCLUDED_SNIPPETGENERATOR_H
#define XAPIAN_INCLUDED_SNIPPETGENERATOR_H

#include <xapian/types.h>
#include <xapian/unicode.h>
#include <xapian/intrusive_ptr.h>
#include <xapian/visibility.h>

#include <string>

namespace Xapian {

class Stem;

/** Parses a piece of text and generate snippets.
 *
 * This module takes a piece of text and parses it to produce words which are
 * then used to match against a set of matched terms previously returned
 * from a query.  The output is a snippet, i.e. string showing the matches
 * highlighted and some surrounding context.
 */
class XAPIAN_VISIBILITY_DEFAULT SnippetGenerator {
  public:
    /// @private @internal Class representing the SnippetGenerator internals.
    class Internal;
    /// @private @internal Reference counted internals.
    Xapian::Internal::intrusive_ptr<Internal> internal;

    /// Copy constructor.
    SnippetGenerator(const SnippetGenerator & o);

    /// Assignment.
    SnippetGenerator & operator=(const SnippetGenerator & o);

    /// Default constructor.
    SnippetGenerator();

    /// Destructor.
    ~SnippetGenerator();

    /// Set the Xapian::Stem object to be used for generating stemmed terms.
    void set_stemmer(const Xapian::Stem & stemmer);

    /** Add a match term to be highlighted.
     *
     * The term will be stemmed if a stemmer has been configured.
     * The term will be matched case insensitively but the case
     * will be preserved in the snippets output.
     *
     * @param term  A term to be highlighted in the snippets.
     */
    void add_match(const std::string & term);

    /** Accept some text.
     *
     * @param itor	Utf8Iterator pointing to the text to index.
     */
    void accept_text(const Xapian::Utf8Iterator & itor);

    /** Accept some text in a std::string.
     *
     * @param text	The text to index.
     */
    void accept_text(const std::string & text) {
	return accept_text(Utf8Iterator(text));
    }

    /** Increase the term position used by index_text.
     *
     *  This can be used between indexing text from different fields or other
     *  places to prevent phrase searches from spanning between them (e.g.
     *  between the title and body text, or between two chapters in a book).
     *
     *  @param delta	Amount to increase the term position by (default: 100).
     */
    void increase_termpos(Xapian::termcount delta = 100);

    /// Get the current term position.
    Xapian::termcount get_termpos() const;

    /** Set the current term position.
     *
     *  @param termpos	The new term position to set.
     */
    void set_termpos(Xapian::termcount termpos);

    /** Get the pre-match string.
     */
    std::string get_pre_match() const;

    /** Set the pre-match string.
     *
     * Matched terms are shown in the snippets string surrounded
     * by the pre-match and post-match strings, whose default values
     * are "<b>" and "</b>".
     *
     * @param text  The new pre-match string to set.
     */
    void set_pre_match(std::string & text);

    /** Get the post-match string.
     */
    std::string get_post_match() const;

    /** Set the post-match string.
     *
     * Matched terms are shown in each snippet string surrounded
     * by the pre-match and post-match strings, whose default values
     * are "<b>" and "</b>".
     *
     * @param text  The new post-match string to set.
     */
    void set_post_match(std::string & text);

    /** Get the inter-snippet string.
     */
    std::string get_inter_snippet() const;

    /** Set the inter-snippet string.
     *
     * Snippets are shown separated by the inter-snippet string, whose
     * default value is "...".
     *
     * @param text  The new inter-snippet string to set.
     */
    void set_inter_snippet(std::string & text);

    /** Get the context length, in words
     */
    unsigned get_context_length() const;

    /** Set the context length, in words
     *
     * Before and after each highlighted match, each snippet
     * shows some context.  This parameter controls how many
     * words of context are shown.
     */
    void set_context_length(unsigned length);

    /** Get the resulting snippets string.
     */
    std::string get_snippets();

    /** Reset the snippets state for another set of text.
     *
     * Resets the snippet results, matches, termpos, and any saved
     * context, so that another document or another field of the same
     * document can be accepted.  Preserves parameters like stemmer etc.
     */
    void reset();

    /// Return a string describing this object.
    std::string get_description() const;
};

}

#endif // XAPIAN_INCLUDED_SNIPPETGENERATOR_H
