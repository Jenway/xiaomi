#include<iostream>
#include<cstdlib>


/*
 * A function manually cause a coredump
 * 
 * Refer to https://stackoverflow.com/questions/979141/how-to-programmatically-cause-a-core-dump-in-c-c
 *
 */

void coreDump() {
    abort();
}

int main(void) {
    std::cout << "Before calling coreDump()\n";
    coreDump();
    std::cout << "After calling coreDump()\n";
}
