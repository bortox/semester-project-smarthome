#include "CommandProcessor.h"

void CommandProcessor::process(char c) {
    // Converti in minuscolo per gestire comandi
    char cmd = tolower(c);
    
    if (cmd == 'w') {
        display.scrollUp();
    } else if (cmd == 's') {
        display.scrollDown();
    } else if (cmd == 'e') {
        display.scrollLeft();  // Enter
    } else if (cmd == 'q') {
        display.stopScroll();  // Indietro
    } else {
        display.addChar(c);  // Carattere normale
    }
}