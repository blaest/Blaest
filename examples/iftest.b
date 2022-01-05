#!/usr/bin/env blaest
#import <stdio.b>

/* iftest.b
 * A test file for the if statement, turned example.  This was used initally
 * to make sure if statements work correctly, and debug them when they aren't */

main(){
    auto x,y,pass_str,fail_str;
    x = 1;
    y = 5;

    pass_str = "Passed!\n";
    fail_str = "Failed!\n";
    
    if(x == 1){
        if(y == 2){
            puts(fail_str);
        }
        else if(y == 5){
            puts(pass_str);
        }
        else{
            puts(fail_str);
        }
    }
    else if(x == 5){
        if(y == 2){
            puts(fail_str);
        }
    }
    else{
        puts(fail_str);
    }
}
