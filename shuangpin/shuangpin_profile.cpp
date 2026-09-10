#include "shuangpin_profile.h"

const ShuangpinProfile &GetXiaoheShuangpinProfile()
{
    static const ShuangpinProfile profile{
        "xiaohe",
        {
            {"sh", "u"},
            {"ch", "i"},
            {"zh", "v"},
        },
        {
            {"a", "aa"},
            {"ai", "ai"},
            {"an", "an"},
            {"ao", "ao"},
            {"ang", "ah"},
            {"e", "ee"},
            {"ei", "ei"},
            {"en", "en"},
            {"eng", "eg"},
            {"er", "er"},
            {"o", "oo"},
            {"ou", "ou"},
        },
        {
            {"iu", "q"},   {"ei", "w"},   {"e", "e"},    {"uan", "r"}, {"ue", "t"},  {"ve", "t"}, {"un", "y"},
            {"u", "u"},    {"i", "i"},    {"uo", "o"},   {"o", "o"},   {"ie", "p"},  {"a", "a"},  {"ong", "s"},
            {"iong", "s"}, {"ai", "d"},   {"en", "f"},   {"eng", "g"}, {"ang", "h"}, {"an", "j"}, {"uai", "k"},
            {"ing", "k"},  {"uang", "l"}, {"iang", "l"}, {"ou", "z"},  {"ua", "x"},  {"ia", "x"}, {"ao", "c"},
            {"ui", "v"},   {"v", "v"},    {"in", "b"},   {"iao", "n"}, {"ian", "m"},
        },
    };
    return profile;
}

const ShuangpinProfile &GetZiranmaShuangpinProfile()
{
    static const ShuangpinProfile profile{
        "ziranma",
        {
            {"sh", "u"},
            {"ch", "i"},
            {"zh", "v"},
        },
        {
            {"a", "aa"},
            {"ai", "ai"},
            {"an", "an"},
            {"ao", "ao"},
            {"ang", "ah"},
            {"e", "ee"},
            {"ei", "ei"},
            {"en", "en"},
            {"eng", "eg"},
            {"er", "er"},
            {"o", "oo"},
            {"ou", "ou"},
        },
        {
            {"iu", "q"},  {"ia", "w"},   {"ua", "w"},  {"e", "e"},    {"uan", "r"},  {"ue", "t"}, {"ve", "t"},
            {"ing", "y"}, {"uai", "y"},  {"u", "u"},   {"i", "i"},    {"o", "o"},    {"uo", "o"}, {"un", "p"},
            {"a", "a"},   {"iong", "s"}, {"ong", "s"}, {"iang", "d"}, {"uang", "d"}, {"en", "f"}, {"eng", "g"},
            {"ang", "h"}, {"an", "j"},   {"ao", "k"},  {"ai", "l"},   {"ei", "z"},   {"ie", "x"}, {"iao", "c"},
            {"ui", "v"},  {"v", "v"},    {"ou", "b"},  {"in", "n"},   {"ian", "m"},
        },
    };
    return profile;
}

const ShuangpinProfile &GetShoudaoShuangpinProfile()
{
    static const ShuangpinProfile profile{
        "shoudao",
        {
            {"sh", "e"},
            {"ch", "i"},
            {"zh", "v"},
        },
        {
            {"a", "aa"},
            {"ai", "ai"},
            {"an", "an"},
            {"ao", "ao"},
            {"ang", "ay"},
            // sh sits on "e", so "ee"/"ei"/"ef" would collide with she/shi/sheng.
            {"e", "ue"},
            {"ei", "ui"},
            {"en", "en"},
            {"eng", "uf"},
            {"er", "er"},
            {"o", "oo"},
            {"ou", "ou"},
        },
        {
            {"iu", "q"},  {"ua", "w"},  {"e", "e"},   {"ie", "r"},  {"uan", "t"},  {"ang", "y"},  {"u", "u"},
            {"i", "i"},   {"o", "o"},   {"uo", "o"},  {"iao", "p"}, {"a", "a"},    {"ou", "s"},   {"ao", "d"},
            {"eng", "f"}, {"uai", "g"}, {"ing", "g"}, {"ong", "h"}, {"iong", "h"}, {"an", "j"},   {"en", "k"},
            {"ia", "k"},  {"ai", "l"},  {"ue", "l"},  {"un", "z"},  {"iang", "x"}, {"uang", "x"}, {"in", "c"},
            {"v", "v"},   {"ui", "v"},  {"ve", "b"},  {"ian", "n"}, {"ei", "m"},
        },
    };
    return profile;
}

const ShuangpinProfile &GetMicrosoftShuangpinProfile()
{
    static const ShuangpinProfile profile{
        "microsoft",
        {
            {"sh", "u"},
            {"ch", "i"},
            {"zh", "v"},
        },
        {
            {"a", "oa"},
            {"ai", "ol"},
            {"an", "oj"},
            {"ang", "oh"},
            {"ao", "ok"},
            {"e", "oe"},
            {"ei", "oz"},
            {"en", "of"},
            {"eng", "og"},
            {"er", "or"},
            {"o", "oo"},
            {"ou", "ob"},
        },
        {
            {"iu", "q"},  {"ia", "w"},   {"ua", "w"},  {"e", "e"},    {"uan", "r"},  {"ue", "t"}, {"ve", "v"},
            {"uai", "y"}, {"v", "y"},    {"u", "u"},   {"i", "i"},    {"o", "o"},    {"uo", "o"}, {"un", "p"},
            {"a", "a"},   {"iong", "s"}, {"ong", "s"}, {"iang", "d"}, {"uang", "d"}, {"en", "f"}, {"eng", "g"},
            {"ang", "h"}, {"an", "j"},   {"ao", "k"},  {"ai", "l"},   {"ing", ";"},  {"ei", "z"}, {"ie", "x"},
            {"iao", "c"}, {"ui", "v"},   {"ou", "b"},  {"in", "n"},   {"ian", "m"},
        },
    };
    return profile;
}

const ShuangpinProfile &GetJiajiaShuangpinProfile()
{
    // Use the vowel-initial spelling rule; see docs/shuangpin-jiajia.md for sources.
    static const ShuangpinProfile profile{
        "jiajia",
        {
            {"sh", "i"},
            {"ch", "u"},
            {"zh", "v"},
        },
        {
            {"a", "aa"},
            {"ai", "as"},
            {"an", "af"},
            {"ang", "ag"},
            {"ao", "ad"},
            {"e", "ee"},
            {"ei", "ew"},
            {"en", "er"},
            {"eng", "et"},
            {"er", "eq"},
            {"o", "oo"},
            {"ou", "op"},
        },
        {
            {"ing", "q"},  {"er", "q"}, {"ei", "w"}, {"e", "e"},   {"en", "r"},   {"eng", "t"},  {"ong", "y"},
            {"iong", "y"}, {"u", "u"},  {"i", "i"},  {"o", "o"},   {"uo", "o"},   {"ou", "p"},   {"a", "a"},
            {"ai", "s"},   {"ao", "d"}, {"an", "f"}, {"ang", "g"}, {"iang", "h"}, {"uang", "h"}, {"ian", "j"},
            {"iao", "k"},  {"in", "l"}, {"un", "z"}, {"uai", "x"}, {"ue", "x"},   {"ve", "x"},   {"uan", "c"},
            {"ui", "v"},   {"v", "v"},  {"ia", "b"}, {"ua", "b"},  {"iu", "n"},   {"ie", "m"},
        },
    };
    return profile;
}

const ShuangpinProfile &GetShuangpinProfile(std::string_view name)
{
    if (name == "ziranma")
    {
        return GetZiranmaShuangpinProfile();
    }
    if (name == "shoudao")
    {
        return GetShoudaoShuangpinProfile();
    }
    if (name == "microsoft")
    {
        return GetMicrosoftShuangpinProfile();
    }
    if (name == "jiajia")
    {
        return GetJiajiaShuangpinProfile();
    }
    return GetXiaoheShuangpinProfile();
}
