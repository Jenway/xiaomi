#include <stdio.h>
#include <unistd.h>



int main() {
    pid_t pid = fork();
    printf("pid = %d at address %p\n", pid, (void*)&pid);
}

