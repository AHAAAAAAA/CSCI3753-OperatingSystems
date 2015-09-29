#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#define DEVICE "/dev/dd"

int main () {
    char cmd;
    char buff[2048];
    int file = open(DEVICE, O_RDWR);
    bool terminal = false;
    while(!terminal)
    {
        printf("\nr) Read\nw) Write\ne) Exit\n");
        scanf("%c", &cmd);
        switch (cmd)
        {
            case 'w':
            printf("Enter string: ");
            scanf(" %[^\n]", buff);
            printf("Wrote %d bytes to device file\n", write(file, buff, 2048));
            while(getchar()!='\n');
            break;

            case 'r':
            read(file, buff, 2048);
            printf("Data: %s\n", buff); //Can't get it to read entire file with just printf :/
        
            while(getchar()!='\n');
            break;

            case 'e':
            printf("Exiting\n");
            terminal = true;
            break;

            default:
            while(getchar()!='\n');
        }
    }
    close(file);
    return 0;
}