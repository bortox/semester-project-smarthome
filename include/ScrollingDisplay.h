#ifndef SCROLLING_DISPLAY_H
#define SCROLLING_DISPLAY_H

#include <Arduino.h>

class ScrollingDisplay {
private:
    static const int rows = 4;
    static const int cols = 20;
    char matrix[rows][cols];
    int cursorRow;
    int cursorCol;

public:
    ScrollingDisplay();
    void addChar(char c);
    void scrollUp();
    void scrollDown();
    void scrollLeft();
    void stopScroll();
    void update();
};

#endif
