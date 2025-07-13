#include "myDictionary.h"
#include <iostream>
#include <string>

int main()
{
    using std::cin;
    using std::cout;
    dict::myDictionary myDict;

    myDict.addEntry("hello", "你好");
    myDict.addEntry("apple", "苹果");
    myDict.addEntry("world", "世界");
    myDict.addEntry("computer", "计算机");

    std::string input;
    cout << "请输入英文或中文单词（输入 q 退出）: ";
    while (cin >> input && input != "q") {
        // 先尝试英文转中文
        if (auto zh = myDict.translateToChinese(input)) {
            cout << "中文翻译是: " << *zh << '\n';
        }
        // 否则尝试中文转英文
        else if (auto en = myDict.translateToEnglish(input)) {
            cout << "English translation: " << *en << '\n';
        } else {
            cout << "未找到对应词汇，请检查拼写。" << '\n';
        }

        cout << "\n请输入英文或中文单词（输入 q 退出）: ";
    }

    return 0;
}
