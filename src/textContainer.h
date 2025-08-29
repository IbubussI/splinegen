#pragma once
#include <cmath>
#include <string>
#include <cstring>

class TextContainer {
public:
    explicit TextContainer(const std::string& initialContent = "")
        : mBuffSize(1024),
          mContentSize(initialContent.length()),
          mPointsTextBuff(new char[mBuffSize]) {
        update(initialContent);
    }

    ~TextContainer() {
        delete[] mPointsTextBuff;
    }

    void update(const std::string& newText) {
        const int newStringLength = newText.length() + 1;

        const int newBuffSize = static_cast<int>(std::ceil(newStringLength / 1024.f)) * 1024;
        if (mBuffSize < newBuffSize) {
            delete[] mPointsTextBuff;
            mPointsTextBuff = new char[newBuffSize];
        }

        strcpy(mPointsTextBuff, newText.c_str());
        mContentSize = newStringLength;
    }

    [[nodiscard]] char* getContent() const {
        return mPointsTextBuff;
    }

    [[nodiscard]] int getBuffSize() const {
        return mBuffSize;
    }

    [[nodiscard]] int getContentSize() const {
        return mContentSize;
    }

private:
    int mBuffSize;
    int mContentSize;
    char* mPointsTextBuff;
};
