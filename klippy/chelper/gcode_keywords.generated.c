/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf -e'	' out/gcode_parser.keywords.gperf  */
/* Computed positions: -k'1' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "out/gcode_parser.keywords.gperf"

#include <stdlib.h>
#include <string.h>
#line 7 "out/gcode_parser.keywords.gperf"
struct GCodeKeywordDetail { const char* name; int id; };

#define TOTAL_KEYWORDS 27
#define MIN_WORD_LENGTH 1
#define MAX_WORD_LENGTH 8
#define MIN_HASH_VALUE 1
#define MAX_HASH_VALUE 32
/* maximum key range = 32, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register size_t len)
{
  static unsigned char asso_values[] =
    {
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      19, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 14, 33, 33, 33,  9, 33, 33,
      31, 26, 15, 28, 23, 18, 13, 30, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      10, 25,  5, 33, 33, 10, 33, 33, 33,  5,
       0, 33, 33,  0, 33, 33, 33, 33,  0, 20,
      33, 33, 33, 33,  0, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 20, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
      33, 33, 33, 33, 33,  0
    };
  return len + asso_values[(unsigned char)str[0]];
}

struct GCodeKeywordDetail *
gcode_keyword_lookup (register const char *str, register size_t len)
{
  static struct GCodeKeywordDetail wordlist[] =
    {
      {""},
#line 19 "out/gcode_parser.keywords.gperf"
      {"\377",288},
#line 23 "out/gcode_parser.keywords.gperf"
      {"IF",278},
#line 16 "out/gcode_parser.keywords.gperf"
      {"NAN",284},
#line 20 "out/gcode_parser.keywords.gperf"
      {"TRUE",286},
#line 31 "out/gcode_parser.keywords.gperf"
      {"FALSE",287},
#line 14 "out/gcode_parser.keywords.gperf"
      {">",274},
#line 10 "out/gcode_parser.keywords.gperf"
      {">=",276},
#line 25 "out/gcode_parser.keywords.gperf"
      {"INFINITY",285},
#line 28 "out/gcode_parser.keywords.gperf"
      {"ELSE",279},
#line 12 "out/gcode_parser.keywords.gperf"
      {"%",269},
#line 34 "out/gcode_parser.keywords.gperf"
      {"<",273},
#line 15 "out/gcode_parser.keywords.gperf"
      {"<=",275},
#line 32 "out/gcode_parser.keywords.gperf"
      {"AND",264},
#line 22 "out/gcode_parser.keywords.gperf"
      {".",280},
#line 11 "out/gcode_parser.keywords.gperf"
      {"!",277},
#line 33 "out/gcode_parser.keywords.gperf"
      {"*",271},
#line 27 "out/gcode_parser.keywords.gperf"
      {"**",270},
      {""},
#line 30 "out/gcode_parser.keywords.gperf"
      {"-",268},
#line 26 "out/gcode_parser.keywords.gperf"
      {"\012",262},
#line 24 "out/gcode_parser.keywords.gperf"
      {"~",266},
#line 13 "out/gcode_parser.keywords.gperf"
      {"OR",263},
      {""},
#line 35 "out/gcode_parser.keywords.gperf"
      {",",281},
      {""},
#line 21 "out/gcode_parser.keywords.gperf"
      {"=",265},
#line 9 "out/gcode_parser.keywords.gperf"
      {")",283},
      {""},
#line 17 "out/gcode_parser.keywords.gperf"
      {"+",267},
      {""},
#line 18 "out/gcode_parser.keywords.gperf"
      {"/",272},
#line 29 "out/gcode_parser.keywords.gperf"
      {"(",282}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
