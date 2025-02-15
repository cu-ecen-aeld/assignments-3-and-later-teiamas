#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    //open log facility
    openlog("writer",LOG_PID | LOG_CONS ,LOG_USER);
    if (argc != 3) {
        fprintf(stderr, "missing parameter(s)\n");
        fprintf(stderr, "usage: %s writefile writestr\n", argv[0]);
        syslog(LOG_ERR,"Invalid number of arguments: %d",argc );
        return 1;
    }
    //transfer command line params on more readable vars
    const char *writefile = argv[1];
    const char *writestr = argv[2];
    //open the file
    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        //perror("Could not create or write on the file");
        syslog(LOG_ERR,"Could not create or write on the file");
        return 1;
    }
    //writing the data
    syslog(LOG_DEBUG,"Writing %s to %s",writestr,writefile);
    if (fprintf(file, "%s\n", writestr) < 0) {
        //perror("Could not write to the file");
        syslog(LOG_ERR,"Could not write to the file");
        fclose(file);
        return 1;
    }
    // Close the connection to the system logger
    closelog();

    //close the file and exit   
    fclose(file);
    printf("File %s created successfully with content: %s\n", writefile, writestr);
    return 0;
}
