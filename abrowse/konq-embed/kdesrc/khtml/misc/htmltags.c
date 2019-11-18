/* ANSI-C code produced by gperf version 2.7.1 (19981006 egcs) */
/* Command-line: gperf -a -L ANSI-C -D -E -C -l -o -t -k * -NfindTag -Hhash_tag -Wwordlist_tag htmltags.gperf  */
/* This file is automatically generated from htmltags.in by maketags, do not edit */
/* Copyright 1999 Lars Knoll */
#include "htmltags.h"
struct tags {
    const char *name;
    int id;
};
/* maximum key range = 295, duplicates = 1 */

#ifdef __GNUC__
__inline
#endif
static unsigned int
hash_tag (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296,  40,
       25,  30,  35,  10,  20, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296,   0,  20,  90,
       20,   0,  45,   0, 120,  25,   5,  55,  10,  55,
        5,   0,  10,  50,   5, 115,   0,  60,  30, 296,
       10,  40, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296, 296, 296, 296, 296,
      296, 296, 296, 296, 296, 296
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 10:
        hval += asso_values[(unsigned char)str[9]];
      case 9:
        hval += asso_values[(unsigned char)str[8]];
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      case 4:
        hval += asso_values[(unsigned char)str[3]];
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

#ifdef __GNUC__
__inline
#endif
const struct tags *
findTag (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 97,
      MIN_WORD_LENGTH = 1,
      MAX_WORD_LENGTH = 10,
      MIN_HASH_VALUE = 1,
      MAX_HASH_VALUE = 295
    };

  static const unsigned char lengthtable[] =
    {
       1,  2,  2,  4,  1,  2,  3,  1,  2,  2,  8,  1,  2,  2,
       3,  5,  6,  2,  3,  5,  6,  2,  3,  5,  6,  3,  4,  5,
       1,  3,  4,  5,  2,  4,  5,  1,  7,  3,  2,  3,  5,  3,
       3,  4,  5,  6,  8,  3,  4,  5,  3,  5,  6,  7,  4,  5,
       4,  1,  4,  6,  2,  4,  2,  6,  2,  4,  6,  7,  4,  2,
       4,  5,  2,  3,  2,  2,  2,  5,  8,  4,  7,  3,  4,  8,
       5,  3,  7,  6,  7,  6,  8,  8,  8,  6,  8,  7, 10
    };
  static const struct tags wordlist_tag[] =
    {
      {"a", ID_A},
      {"tt", ID_TT},
      {"tr", ID_TR},
      {"area", ID_AREA},
      {"p", ID_P},
      {"ol", ID_OL},
      {"pre", ID_PRE},
      {"b", ID_B},
      {"dt", ID_DT},
      {"td", ID_TD},
      {"textarea", ID_TEXTAREA},
      {"i", ID_I},
      {"br", ID_BR},
      {"dl", ID_DL},
      {"del", ID_DEL},
      {"table", ID_TABLE},
      {"applet", ID_APPLET},
      {"li", ID_LI},
      {"var", ID_VAR},
      {"title", ID_TITLE},
      {"legend", ID_LEGEND},
      {"dd", ID_DD},
      {"bdo", ID_BDO},
      {"label", ID_LABEL},
      {"option", ID_OPTION},
      {"big", ID_BIG},
      {"abbr", ID_ABBR},
      {"tfoot", ID_TFOOT},
      {"q", ID_Q},
      {"dir", ID_DIR},
      {"font", ID_FONT},
      {"plain", ID_PLAIN},
      {"em", ID_EM},
      {"meta", ID_META},
      {"layer", ID_LAYER},
      {"u", ID_U},
      {"nolayer", ID_NOLAYER},
      {"map", ID_MAP},
      {"ul", ID_UL},
      {"dfn", ID_DFN},
      {"param", ID_PARAM},
      {"div", ID_DIV},
      {"img", ID_IMG},
      {"body", ID_BODY},
      {"tbody", ID_TBODY},
      {"button", ID_BUTTON},
      {"optgroup", ID_OPTGROUP},
      {"kbd", ID_KBD},
      {"link", ID_LINK},
      {"embed", ID_EMBED},
      {"col", ID_COL},
      {"input", ID_INPUT},
      {"center", ID_CENTER},
      {"noembed", ID_NOEMBED},
      {"form", ID_FORM},
      {"frame", ID_FRAME},
      {"code", ID_CODE},
      {"s", ID_S},
      {"cite", ID_CITE},
      {"object", ID_OBJECT},
      {"th", ID_TH},
      {"menu", ID_MENU},
      {"hr", ID_HR},
      {"strong", ID_STRONG},
      {"h5", ID_H5},
      {"span", ID_SPAN},
      {"iframe", ID_IFRAME},
      {"caption", ID_CAPTION},
      {"base", ID_BASE},
      {"h6", ID_H6},
      {"head", ID_HEAD},
      {"thead", ID_THEAD},
      {"h2", ID_H2},
      {"ins", ID_INS},
      {"h3", ID_H3},
      {"h4", ID_H4},
      {"h1", ID_H1},
      {"style", ID_STYLE},
      {"colgroup", ID_COLGROUP},
      {"samp", ID_SAMP},
      {"listing", ID_LISTING},
      {"sup", ID_SUP},
      {"html", ID_HTML},
      {"basefont", ID_BASEFONT},
      {"small", ID_SMALL},
      {"sub", ID_SUB},
      {"acronym", ID_ACRONYM},
      {"strike", ID_STRIKE},
      {"isindex", ID_ISINDEX},
      {"select", ID_SELECT},
      {"fieldset", ID_FIELDSET},
      {"frameset", ID_FRAMESET},
      {"noframes", ID_NOFRAMES},
      {"script", ID_SCRIPT},
      {"noscript", ID_NOSCRIPT},
      {"address", ID_ADDRESS},
      {"blockquote", ID_BLOCKQUOTE}
    };

  static const signed char lookup[] =
    {
        -1,    0,    1,   -1,   -1,   -1,   -1,    2,
        -1,    3,   -1,    4,    5,   -1,   -1,   -1,
        -1,   -1,    6,   -1,   -1,    7, -122,   10,
       -89,   -2,   11,   12,   -1,   -1,   -1,   -1,
        13,   14,   -1,   15,   16,   17,   18,   -1,
        19,   20,   21,   22,   -1,   23,   24,   -1,
        25,   26,   27,   28,   -1,   29,   30,   31,
        -1,   32,   -1,   33,   34,   35,   -1,   -1,
        -1,   -1,   -1,   36,   37,   -1,   -1,   -1,
        38,   39,   -1,   40,   -1,   -1,   41,   -1,
        -1,   -1,   -1,   42,   43,   44,   -1,   -1,
        -1,   -1,   -1,   45,   -1,   46,   -1,   -1,
        -1,   -1,   47,   48,   49,   -1,   -1,   50,
        -1,   51,   52,   53,   -1,   54,   55,   -1,
        -1,   -1,   56,   -1,   57,   -1,   -1,   58,
        -1,   59,   60,   -1,   61,   -1,   -1,   62,
        -1,   -1,   -1,   63,   64,   -1,   65,   -1,
        66,   67,   -1,   68,   -1,   -1,   69,   -1,
        70,   71,   -1,   72,   73,   -1,   -1,   -1,
        74,   -1,   -1,   -1,   -1,   75,   -1,   -1,
        -1,   -1,   76,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   77,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   78,
        79,   -1,   -1,   80,   81,   82,   -1,   -1,
        -1,   83,   -1,   84,   -1,   -1,   85,   -1,
        -1,   -1,   86,   -1,   -1,   -1,   87,   88,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   89,   -1,   90,
        -1,   -1,   -1,   -1,   91,   -1,   -1,   -1,
        -1,   92,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   93,   -1,   -1,   -1,   -1,
        -1,   -1,   94,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   95,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   96
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash_tag (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int index = lookup[key];

          if (index >= 0)
            {
              if (len == lengthtable[index])
                {
                  register const char *s = wordlist_tag[index].name;

                  if (*str == *s && !strcmp (str + 1, s + 1))
                    return &wordlist_tag[index];
                }
            }
          else if (index < -TOTAL_KEYWORDS)
            {
              register int offset = - 1 - TOTAL_KEYWORDS - index;
              register const unsigned char *lengthptr = &lengthtable[TOTAL_KEYWORDS + lookup[offset]];
              register const struct tags *wordptr = &wordlist_tag[TOTAL_KEYWORDS + lookup[offset]];
              register const struct tags *wordendptr = wordptr + -lookup[offset + 1];

              while (wordptr < wordendptr)
                {
                  if (len == *lengthptr)
                    {
                      register const char *s = wordptr->name;

                      if (*str == *s && !strcmp (str + 1, s + 1))
                        return wordptr;
                    }
                  lengthptr++;
                  wordptr++;
                }
            }
        }
    }
  return 0;
}


static const char * tagList[] = {
"",
    "A",
    "ABBR",
    "ACRONYM",
    "ADDRESS",
    "APPLET",
    "AREA",
    "B",
    "BASE",
    "BASEFONT",
    "BDO",
    "BIG",
    "BLOCKQUOTE",
    "BODY",
    "BR",
    "BUTTON",
    "CAPTION",
    "CENTER",
    "CITE",
    "CODE",
    "COL",
    "COLGROUP",
    "DD",
    "DEL",
    "DFN",
    "DIR",
    "DIV",
    "DL",
    "DT",
    "EM",
    "EMBED",
    "FIELDSET",
    "FONT",
    "FORM",
    "FRAME",
    "FRAMESET",
    "H1",
    "H2",
    "H3",
    "H4",
    "H5",
    "H6",
    "HEAD",
    "HR",
    "HTML",
    "I",
    "IFRAME",
    "IMG",
    "INPUT",
    "INS",
    "ISINDEX",
    "KBD",
    "LABEL",
    "LAYER",
    "LEGEND",
    "LI",
    "LINK",
    "LISTING",
    "MAP",
    "MENU",
    "META",
    "NOEMBED",
    "NOFRAMES",
    "NOLAYER",
    "NOSCRIPT",
    "OBJECT",
    "OL",
    "OPTGROUP",
    "OPTION",
    "P",
    "PARAM",
    "PLAIN",
    "PRE",
    "Q",
    "S",
    "SAMP",
    "SCRIPT",
    "SELECT",
    "SMALL",
    "SPAN",
    "STRIKE",
    "STRONG",
    "STYLE",
    "SUB",
    "SUP",
    "TABLE",
    "TBODY",
    "TD",
    "TEXTAREA",
    "TFOOT",
    "TH",
    "THEAD",
    "TITLE",
    "TR",
    "TT",
    "U",
    "UL",
    "VAR",
"TEXT",
"COMMENT",
    "/A",
    "/ABBR",
    "/ACRONYM",
    "/ADDRESS",
    "/APPLET",
    "/AREA",
    "/B",
    "/BASE",
    "/BASEFONT",
    "/BDO",
    "/BIG",
    "/BLOCKQUOTE",
    "/BODY",
    "/BR",
    "/BUTTON",
    "/CAPTION",
    "/CENTER",
    "/CITE",
    "/CODE",
    "/COL",
    "/COLGROUP",
    "/DD",
    "/DEL",
    "/DFN",
    "/DIR",
    "/DIV",
    "/DL",
    "/DT",
    "/EM",
    "/EMBED",
    "/FIELDSET",
    "/FONT",
    "/FORM",
    "/FRAME",
    "/FRAMESET",
    "/H1",
    "/H2",
    "/H3",
    "/H4",
    "/H5",
    "/H6",
    "/HEAD",
    "/HR",
    "/HTML",
    "/I",
    "/IFRAME",
    "/IMG",
    "/INPUT",
    "/INS",
    "/ISINDEX",
    "/KBD",
    "/LABEL",
    "/LAYER",
    "/LEGEND",
    "/LI",
    "/LINK",
    "/LISTING",
    "/MAP",
    "/MENU",
    "/META",
    "/NOEMBED",
    "/NOFRAMES",
    "/NOLAYER",
    "/NOSCRIPT",
    "/OBJECT",
    "/OL",
    "/OPTGROUP",
    "/OPTION",
    "/P",
    "/PARAM",
    "/PLAIN",
    "/PRE",
    "/Q",
    "/S",
    "/SAMP",
    "/SCRIPT",
    "/SELECT",
    "/SMALL",
    "/SPAN",
    "/STRIKE",
    "/STRONG",
    "/STYLE",
    "/SUB",
    "/SUP",
    "/TABLE",
    "/TBODY",
    "/TD",
    "/TEXTAREA",
    "/TFOOT",
    "/TH",
    "/THEAD",
    "/TITLE",
    "/TR",
    "/TT",
    "/U",
    "/UL",
    "/VAR",
    0
};
DOMString getTagName(unsigned short id)
{
    if(id > ID_CLOSE_TAG*2) id = ID_CLOSE_TAG+1;
    return DOMString(tagList[id]);
};