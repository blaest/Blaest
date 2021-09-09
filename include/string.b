strlen(string)
{
    auto x;
    x = 0;
    while(string[x] != 0){
        x = x + 1;
    }

    return x;
}

strcmp(str1, str2){
    auto index;
    index = 0;
    
    while(str1[index] == str2[index]){
        if(str1[index] == 0){
            return 1;
        }
        index += 1;
    }
    return 0;
}

strncmp(str1, str2, size){
    auto index;
    index = 0;
    
    while(str1[index] == str2[index]){
        index += 1;
        if(index == size){
            return 1;
        }
        
        if(str1[index] == 0){
            return 0;
        }
        
        if(str2[index] == 0){
            return 0;
        }
        
    }
    return 0;
}

substr(string, start, end){
    auto sub, indx;
    indx = 0;
    
    sub = malloc(end - start);
    
    while(start != end){
        sub[indx] = string[start];
        
        start += 1;
        indx += 1;
    }
    
    sub[indx] = 0;
    return sub;
}

memcpy(src, dest, size){
    auto indx;
    
    indx = 0;
    
    while( indx != size ){
        dest[indx] = src[indx];
        indx += 1;
    }
}

strclone(src){
    auto len, dest;
    
    len = strlen(src) + 1;
    dest = malloc(len);
    
    memcpy(src, dest, len);
    
    dest[len] = 0;
    
    return dest;
}

split(string, token){
    auto outcome, stri, outi, len, tlen, last;
    
    len = strlen(string);
    tlen = strlen(token);
    stri = 0;
    outi = 0;
    
    last = 0;
    
    outcome = malloc(64);
    
    while(stri != len){
    	if( strncmp( string + stri, token, tlen) ){
			if( stri - last != 0){
            	outcome[outi] = substr(string, last, stri);
			}
            outi += 1;
            last = stri + tlen;
            stri += tlen - 1;

        }
        stri += 1;
    }
	
	if( stri - last != 0){
    	outcome[outi] = substr(string, last, stri);
	}


    return outcome;
}

strip(string){
	auto index;

	index = 0;
	while(string[0] == 32){
		string += 1;
	}

	while(string[index] != 0){
		index += 1;
	}

	index -= 1;

	while(string[index] == 32){
		string[index] = 0;
		index -= 1;
	}

	return string;
}

lstrip(string){
    while(string[0] == 32){
		string += 1;
	}
    return string;
}
