#include "systemcalls.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <fcntl.h>
/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int syscall_result = system(cmd); 
    int success = false;
    system(cmd);
    if (syscall_result == -1) {
        // system() call failed
        success = false; 
    } else if (WIFEXITED(syscall_result)) {
        // The process exited normally check for result code
        success = (WEXITSTATUS(syscall_result) == EXIT_SUCCESS);
    }  else {
        // The process did not exit normally
        success = false;
    }
    return success;    
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    int success = false; // Initialize success to false
    int status;          // Variable to hold the status of the child process
    fflush(stdout);      // Flush the output buffer to ensure all output is written to the console
    pid_t pid = fork();  // Create a new process by forking

    if (pid == -1) {
        // fork failed
        success = false; 
    } else if (pid == 0) {
        // child process

        execv(command[0], command); // Execute the command in the child process
        // execv only returns if an error occurred
        perror("execv");
        success = false; 
        exit(EXIT_FAILURE); // Ensure child process exits if execv fails
} else {
        // parent process, waits for the child process to finish
        if (waitpid(pid, &status, 0) == -1) {
            // child proc finished with failure code
            success = false; 
        } else if (WIFEXITED(status)) {
            // Child process terminated normally
            success = (WEXITSTATUS(status) == EXIT_SUCCESS); // Set success to the exit status of the child process
        }
    }
    //reinit arg list macroes
    va_end(args);

    return success;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    // command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    // Create a new process using 
    fflush(stdout); // Flush the output buffer to ensure all output is written to the console
    int success = false; // Initialize success to false
    int status;          // Variable to hold the status of the child process
    fflush(stdout);      // Flush the output buffer to ensure all output is written to the console
    pid_t pid = fork();  // Create a new process by forking
    // Check if the command is an absolute path
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd >= 0) { 
        if (pid == -1) {
            // fork failed
            success = false; 
        } else if (pid == 0) {
            // child process
            
            // Redirect stdout to the file descriptor
            if (dup2(fd, 1) < 0) { 
                perror("dup2");
                exit(EXIT_FAILURE); // Ensure child process exits if execv fails 
            }
            close(fd); // Close the file descriptor no more needed

            execv(command[0], command); // Execute the command in the child process
            // execv only returns if an error occurred
            perror("execv");
            success = false; 
            exit(EXIT_FAILURE); // Ensure child process exits if execv fails
        } else {
            // parent process, waits for the child process to finish
            close(fd); // Close the file descriptor in the parent process
            // parent process, waits for the child process to finish
            if (waitpid(pid, &status, 0) == -1) {
                // child proc finished with failure code
                success = false; 
            } else if (WIFEXITED(status)) {
                // Child process terminated normally
                success = (WEXITSTATUS(status) == EXIT_SUCCESS); // Set success to the exit status of the child process
            }
        }
    }else {
        printf("Error opening file\n");
        fflush(stdout);
        success = false;
    }

    //reinit arg list macroes
    va_end(args);

    return success;
}
