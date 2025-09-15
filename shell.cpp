/****************
LE2: Introduction to Unnamed Pipes
****************/
#include <iostream>
#include <unistd.h> // pipe, fork, dup2, execvp, close
#include <sys/wait.h> // waitpid
using namespace std;

int main () {
    // lists all the files in the root directory in the long format
    char* cmd1[] = {(char*) "ls", (char*) "-al", (char*) "/", nullptr};
    // translates all input from lowercase to uppercase
    char* cmd2[] = {(char*) "tr", (char*) "a-z", (char*) "A-Z", nullptr};

    // TODO: add functionality
    // Create pipe
    // fds[0] = read end, fds[1] = write end
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    // Create child to run first command
    // In child, redirect output to write end of pipe
    // Close the read end of the pipe on the child side.
    // In child, execute the command
    // Create the first child to run the 'ls' command
    pid_t pid1 = fork();

    if(pid1 == -1){
        perror("fork");
        return 1;
    }

    // In the first child (pid1 == 0), redirect stdout to the pipe's write end
    if (pid1 == 0) {
        // Close the read end of the pipe, as the child won't read from it.
        close(fds[0]);
        
        dup2(fds[1], STDOUT_FILENO);

        close(fds[1]);

        // Execute the first command.
        execvp(cmd1[0], cmd1);
        perror("execvp"); // execvp only returns if an error occurred
        return 1;
    }





    waitpid(pid1, nullptr, 0);

    // Now, in the parent, create the second child to run the 'tr' command
    pid_t pid2 = fork();
    
    if (pid2 == -1) {
        perror("fork");
        return 1;
    }

    // Create another child to run second command
    // In child, redirect input to the read end of the pipe
    // Close the write end of the pipe on the child side.
    // Execute the second command.

    // In the second child (pid2 == 0), redirect stdin to the pipe's read end
    if (pid2 == 0) {
        close(fds[1]);

        dup2(fds[0], STDIN_FILENO);

        close(fds[0]);

        execvp(cmd2[0], cmd2);
        perror("execvp");
        return 1;
    }

    close(fds[0]);
    close(fds[1]);

    // Reset the input and output file descriptors of the parent.
    waitpid(pid1, nullptr, 0);
    waitpid(pid2, nullptr, 0);

    return 0;
}
