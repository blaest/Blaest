#!/usr/bin/env blaest
#import <stdio.b>
#import <string.b>

/* strcmp.b
 * One of the first real 'impressive' examples of Blaest.  And by that, I mean
 * its one of the first that wasn't just setting values */

main(){
    auto x, y, z;
    x = "Hello World";
    y = "Hello World";
    z = split(x, " ");

    puts("String 1:            ");
    puts(x);
    puts("\n");

    puts("String 2:            ");
    puts(y);
    puts("\n");    

    puts("Test of substring:   ");
    puts(substr(x, 2, strlen(x) - 2 ) );
    puts("\n");

    puts("Test of split:       ");
    puts(z[0]);
    puts("\n");
 
    puts("The two strings are: ");

    if(strcmp(x, y)){
        puts("Equal\n");
        return 0;
    }
    
    puts("Not Equal\n");
    return 1;
}
