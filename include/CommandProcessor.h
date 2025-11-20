#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include "ScrollingDisplay.h"

class CommandProcessor {
private:
    ScrollingDisplay& display;

public:
    CommandProcessor(ScrollingDisplay& disp);
    void process(char c);
    void reset();
};

#endif
