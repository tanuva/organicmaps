#pragma once

#include "indexer/search_delimiters.hpp"

#include "base/levenshtein_dfa.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace search
{
size_t GetMaxErrorsForTokenLength(size_t length);
size_t GetMaxErrorsForToken(strings::UniString const & token);

strings::LevenshteinDFA BuildLevenshteinDFA(strings::UniString const & s);

// This function should be used for all search strings normalization.
// It does some magic text transformation which greatly helps us to improve our search.
strings::UniString NormalizeAndSimplifyString(std::string_view s);

// Replace abbreviations which can be split during tokenization with full form.
// Eg. "пр-т" -> "проспект".
void PreprocessBeforeTokenization(strings::UniString & query);

template <class Delims, typename Fn>
void SplitUniString(strings::UniString const & uniS, Fn && f, Delims const & delims)
{
  /// @todo Make string_view analog for strings::UniString.

  using namespace strings;
  TokenizeIterator<Delims, UniString::const_iterator> iter(uniS.begin(), uniS.end(), delims);
  for (; iter; ++iter)
    f(iter.GetUniString());
}

template <typename Tokens, typename Delims>
void NormalizeAndTokenizeString(std::string_view s, Tokens & tokens, Delims const & delims)
{
  SplitUniString(NormalizeAndSimplifyString(s), ::base::MakeBackInsertFunctor(tokens), delims);
}

template <typename Tokens>
void NormalizeAndTokenizeString(std::string_view s, Tokens & tokens)
{
  SplitUniString(NormalizeAndSimplifyString(s), ::base::MakeBackInsertFunctor(tokens),
                 search::Delimiters());
}

template <typename Tokens>
void NormalizeAndTokenizeAsUtf8(std::string_view s, Tokens & tokens)
{
  tokens.clear();
  auto const fn = [&](strings::UniString const & s) { tokens.emplace_back(strings::ToUtf8(s)); };
  SplitUniString(NormalizeAndSimplifyString(s), fn, search::Delimiters());
}

inline std::vector<std::string> NormalizeAndTokenizeAsUtf8(std::string_view s)
{
  std::vector<std::string> result;
  NormalizeAndTokenizeAsUtf8(s, result);
  return result;
}

template <typename Fn>
void ForEachNormalizedToken(std::string const & s, Fn && fn)
{
  SplitUniString(NormalizeAndSimplifyString(s), std::forward<Fn>(fn), search::Delimiters());
}

strings::UniString FeatureTypeToString(uint32_t type);

template <class Tokens, class Delims>
bool TokenizeStringAndCheckIfLastTokenIsPrefix(strings::UniString const & s,
                                               Tokens & tokens,
                                               Delims const & delims)
{
  SplitUniString(s, ::base::MakeBackInsertFunctor(tokens), delims);
  return !s.empty() && !delims(s.back());
}

template <class Tokens, class Delims>
bool TokenizeStringAndCheckIfLastTokenIsPrefix(std::string_view sv, Tokens & tokens,
                                               Delims const & delims)
{
  return TokenizeStringAndCheckIfLastTokenIsPrefix(NormalizeAndSimplifyString(sv), tokens, delims);
}

// Chops off the last query token (the "prefix" one) from |str|.
std::string DropLastToken(std::string const & str);

strings::UniString GetStreetNameAsKey(std::string_view name, bool ignoreStreetSynonyms);

// *NOTE* The argument string must be normalized and simplified.
bool IsStreetSynonym(strings::UniString const & s);
bool IsStreetSynonymPrefix(strings::UniString const & s);
bool IsStreetSynonymWithMisprints(strings::UniString const & s);
bool IsStreetSynonymPrefixWithMisprints(strings::UniString const & s);

/// Normalizes both str and substr, and then returns true if substr is found in str.
/// Used in native platform code for search in localized strings (cuisines, categories, strings etc.).
bool ContainsNormalized(std::string const & str, std::string const & substr);

// This class can be used as a filter for street tokens.  As there can
// be street synonyms in the street name, single street synonym is
// skipped, but multiple synonyms are left as is. For example, when
// applied to ["улица", "ленина"] the filter must emit only
// ["ленина"], but when applied to ["улица", "набережная"] the filter
// must emit both tokens as is, i.e. ["улица", "набережная"].
class StreetTokensFilter
{
public:
  using Callback = std::function<void(strings::UniString const & token, size_t tag)>;

  template <typename C>
  StreetTokensFilter(C && callback, bool withMisprints)
    : m_callback(std::forward<C>(callback)), m_withMisprints(withMisprints)
  {
  }

  // Puts token to the filter. Filter checks following cases:
  // * when |token| is the first street synonym met so far, it's delayed
  // * when |token| is the second street synonym met so far,
  //   callback is called for the |token| and for the previously delayed token
  // * otherwise, callback is called for the |token|
  void Put(strings::UniString const & token, bool isPrefix, size_t tag);

private:
  using Cell = std::pair<strings::UniString, size_t>;

  inline void EmitToken(strings::UniString const & token, size_t tag) { m_callback(token, tag); }

  strings::UniString m_delayedToken;
  size_t m_delayedTag = 0;
  size_t m_numSynonyms = 0;

  Callback m_callback;
  bool m_withMisprints = false;
};
}  // namespace search
