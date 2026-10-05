/**
 * @file Regex.hpp
 * @brief Regular expression engine for the Xi framework.

 */

#ifndef KSEE_PARSE_REGEX_HPP
#define KSEE_PARSE_REGEX_HPP

#include "../Array.hpp"
#include "../Map.hpp"
#include "../String.hpp"
#include "../Types.hpp"

/**
 * @namespace Data
 * @brief Contains data serialization and processing utilities.
 */
namespace Ksee {



/**
 * @struct RegexMatch
 * @brief Represents a single match from a regular expression.
 */
struct RegexMatch : public Array<String> {
  String full;                     ///< The full matched string.
  long long start = -1;            ///< Start index of the match.
  long long end = -1;              ///< End index of the match.
  Map<String, String> namedGroups; ///< Map of named capture groups.

  RegexMatch() = default;
};

/**
 * @class Regex
 * @brief Regular expression parser and matching engine.
 */
class Regex {
public:
  /**
   * @struct DFAState
   * @brief Internal state for the DFA engine.
   */
  struct DFAState {
    Array<int> pcs;
    void sort();
    bool operator==(const DFAState &o) const;

    // FNV hash support for Map
    static usz fnvHash(const DFAState &s) {
#if __SIZEOF_SIZE_T__ == 4 || defined(__i386__) || defined(_M_IX86)
      usz h = 2166136261U;
      for (usz i = 0; i < s.pcs.size(); ++i)
        h = (h ^ (usz)s.pcs[i]) * 16777619U;
#else
      usz h = 14695981039346656037ULL;
      for (usz i = 0; i < s.pcs.size(); ++i)
        h = (h ^ (usz)s.pcs[i]) * 1099511628211ULL;
#endif
      return h;
    }
  };

  /**
   * @brief Constructs and compiles a regular expression.
   * @param p The pattern string.
   */
  Regex(const String &p);

  /**
   * @brief Finds all matches in the input string.
   * @param input The string to search.
   * @param maxMatches Maximum number of matches to find (0 for all).
   * @param limitUs Time limit in microseconds (0 for no limit).
   * @return Array of matches.
   */
  Array<RegexMatch> matchAll(const String &input, int maxMatches = 0,
                             u64 limitUs = 0) const;

  /**
   * @brief Splits a string by the regular expression.
   * @param s The string to split.
   * @return Array of split parts.
   */
  Array<String> split(const String &s) const;

  /**
   * @brief Replaces all occurrences of the pattern with a replacement string.
   * @param s The subject string.
   * @param rep The replacement string.
   * @return The resulting string.
   */
  String replace(const String &s, const String &rep) const;

  bool parsed = false;           ///< Whether the regex was successfully parsed.
  String code;                   ///< The compiled regex bytecode.
  bool globalIgnoreCase = false; ///< Global case-insensitivity flag.
  bool dotAll = false;           ///< Whether dot matches newlines.
  bool anchored = false; ///< Whether the match must be anchored at the start.

  static constexpr int MAX_CAPS = 32; ///< Maximum number of capture groups.
  static constexpr int MAX_DFA_STATES = 1000; ///< Maximum number of DFA states.
  static constexpr int RECURSION_LIMIT =
      512; ///< Recursion limit for compilation.

private:
  enum class Op {
    Match,
    Char,
    CharIC,
    Any,
    Class,
    Jmp,
    Split,
    Save,
    AssertStart,
    AssertEnd,
    AssertWordBound,
    Lookahead,
    NegLookahead,
    Lookbehind,
    NegLookbehind
  };

  struct Inst {
    Op op;
    int x = 0;
    int y = 0;
    bool invert = false;
    String chars;
    Array<Inst> sub;
  };

  Array<Inst> inst;
  struct CapName {
    String name;
    int idx;
  };
  Array<CapName> capNames;
  int numCaps = 1;

  String prefixLiteral;
  int skipTable[256];

  mutable Array<DFAState> states;
  mutable Map<DFAState, int> stateMap;
  mutable int transitions[MAX_DFA_STATES][256];

  void resetDFA() const;
  void addEpsilon(int pc, Array<int> &set, Array<int> &visited) const;
  void addConsuming(int pc, Array<int> &set, Array<int> &visited) const;
  bool isWord(char c) const;
  bool checkClass(const Inst &ins, u8 c) const;
  int getNextDFAState(int stateId, u8 c) const;
  void emit(Array<Inst> &p, Op op, int x = 0, int y = 0);
  Array<Inst> compileSub(const String &p, int &pos);
  void addClassRange(Inst &ci, char start, char end);
  void addClassEscape(Inst &ci, char nc);
  void compileCore(const String &p, int &pos, Array<Inst> &prog, int depth = 0,
                   bool localIC = false);
  void buildSkipTable();
  void compile(const String &p);
  bool runVM(const String &in, int start, const Array<Inst> &prog,
             bool rev = false) const;
};




template <> struct FNVHasher<Regex::DFAState> {
  static usz fnvHash(const Regex::DFAState &s) {
    return Regex::DFAState::fnvHash(s);
  }
};
} // namespace Xi

#endif // XI_DATA_REGEX_HPP
