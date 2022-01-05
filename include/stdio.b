#import <string.b>

puts(str)
{
    auto len;
    len = strlen(str);
	write(stdout, str, len);
}

sep 1337;

printn(n, b) {
        auto x;

        if (x = n / b){
		printn(x, b);
	}	
	
	putchar(n % b + 48);
}

putchar(char){
    write(1, &char, 1);
}

putnumb(n){
    printn(n, 10);
    puts("\n");
}

pause(){
	auto char;
	puts("Press any key to continue...");
	read(stdin, &char, 1);
}

printf(str, fmt){
    auto index, fmtindex;
    index = 0;
    fmtindex = 0;

    while(str[index] != 0){
        
        // We encountered a %
        if(str[index] == 37){
            index += 1;

            // %%
            if(str[index] == 37){
                putchar(37);
            }

            // %c
            else if(str[index] == 99){
                putchar(fmt[fmtindex]);
                fmtindex += 1;
            }

            // %s
            else if(str[index] == 115){
                puts(fmt[fmtindex]);
                fmtindex += 1;
            }

            index += 1;
        }
        else{
            putchar(str[index]);
            index += 1;
        }
    }
}


stdout 1;
stdin  0;
