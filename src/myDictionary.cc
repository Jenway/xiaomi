#include "myDictionary.h"
#include <unordered_map>

namespace dict {
using std::optional;
using std::string;

void myDictionary::addEntry(const string& en, const string& zh)
{
    enToZh[en] = zh;
    zhToEn[zh] = en;
}

optional<string> myDictionary::translateToChinese(const string& en) const
{
    return translate(enToZh, en);
}

optional<string> myDictionary::translateToEnglish(const string& zh) const
{
    return translate(zhToEn, zh);
}

} // namespace dict
