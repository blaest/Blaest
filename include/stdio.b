#include <string.b>

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

stdout 1;
stdin  0;
