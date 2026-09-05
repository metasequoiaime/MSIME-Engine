"""
把全拼词组转换成小鹤双拼，

src: 
../cn/BaseDictAllV1Part2.txt
"""
import os.path

# 声母映射
flyInitialDict = {"sh": "u", "ch": "i", "zh": "v"}

# 韵母映射
flyFinalDict = {
    "iu": "q",
    "ei": "w",
    "e": "e",
    "uan": "r",
    "ue": "t",
    "ve": "t",
    "un": "y",
    "u": "u",
    "i": "i",
    "uo": "o",
    "o": "o",
    "ie": "p",
    "a": "a",
    "ong": "s",
    "iong": "s",
    "ai": "d",
    "en": "f",
    "eng": "g",
    "ang": "h",
    "an": "j",
    "uai": "k",
    "ing": "k",
    "uang": "l",
    "iang": "l",
    "ou": "z",
    "ua": "x",
    "ia": "x",
    "ao": "c",
    "ui": "v",
    "v": "v",
    "in": "b",
    "iao": "n",
    "ian": "m",
}


def cvt_pinyin_to_ul(pinyin: str) -> str:
    """
    把单个拼音转换为小鹤双拼
    """
    if len(pinyin) == 1:
        # 如果是 n、a 这种单声母或者韵母，且，是单字母的拼音
        pinyin = pinyin + pinyin
    elif len(pinyin) == 2:
        # 双字母保持全拼方式，如：li，xi，wu，ng，an
        pinyin = pinyin
    elif len(pinyin) > 2:
        # 如果是 ang 这种单韵母拼音，且为三字母，规则是首字母加韵母所在键
        if pinyin in flyFinalDict.keys():
            pinyin = pinyin[0] + flyFinalDict[pinyin]
        # 如果声母是两个字母
        elif pinyin[:2] in flyInitialDict.keys():
            pinyin = flyInitialDict[pinyin[:2]] + flyFinalDict[pinyin[2:]]
        # 声母是单字母
        else:
            pinyin = pinyin[0] + flyFinalDict[pinyin[1:]]
    return pinyin


def cvt_words_to_ul(pinyin: str) -> str:
    """
    把词组转换成小鹤双拼
    """
    pinyinArr = pinyin.split("'")
    for i in range(len(pinyinArr)):
        pinyinArr[i] = cvt_pinyin_to_ul(pinyinArr[i])
    res = "".join(pinyinArr)
    return res


words_path = os.path.join(os.path.dirname(
    __file__), "../cn/BaseDictAllV1Part2.txt")
res_path = os.path.join(os.path.dirname(__file__),
                        "./cn/BaseDictAllV1Part2_Ul_V1.txt")

count = 0
with open(words_path, "rb") as file:
    with open(res_path, "wb") as res_file:
        all_lines = file.readlines()
        for line in all_lines:
            cur_line = line.decode().strip()
            cur_line_list = cur_line.split("\t")
            try:
                cur_line_to_write = cur_line_list[0] + "\t" + cvt_words_to_ul(
                    cur_line_list[1]) + "\t" + cur_line_list[2] + "\n"
                count += 1
            except:
                print(cur_line_list)
                break
            res_file.write(cur_line_to_write.encode())
print(count)
