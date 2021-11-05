/* Blaest main source.  The following contains the source for the JIT compiler,
 * virtual machine, and front end.  Eventually they will be split out.  But not
 * for right now. */

/******************************************************************************/ 

/* CONFIGURATION BLOCK */

/* Size of the program memory (in words)*/
#define BLANG_MEMORY_SIZE 32000

/* How many mallocs you can have without freeing memory */
#define BLANG_MEMORY_LEASES 256

/* Size of the stack  (in words)*/
#define BLANG_STACK_SIZE 4096

/* Size of the line buffer */
#define BLANG_LINEBUFFER_SIZE 1024

/* How many words should each block of memory constitute? */
#define BLANG_MMAP_LIMIT 10


/* Use the B Lang style of escapes (* instead of \, ex. '*n' instead of '\n') */
/* #define BLANG_OLD_STYLE_ESCAPE */

/* Don't touch this here, change it in the Makefile, if you aren't using one
 * though, change it here. */
#ifndef BLANG_INCLUDE_PATH
    #ifdef __unix
        #define BLANG_INCLUDE_PATH "/usr/local/lib/blaest/include"
    #else
        #define BLANG_INCLUDE_PATH "./include"
    #endif
#endif

/* Make sure this is the size of the bits on your computer, for example, 64-bit
 * uses 64-bit long, 32-bit uses 32-bit int, and so on. */
#define BLANG_WORD_TYPE unsigned long
#define BLANG_WORD_TYPE_SIGNED signed long

/******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __unix
    #include <unistd.h>
    #include <fcntl.h>
#else
    #define BLANG_USE_FAUXNIX
#endif

#define BLANG_INDEX_TYPE int

#ifdef _DEBUG
#define BLANG_VERSION "Blaest Trunk v0.2_dev"
#define DBG_RUN(n) n
#else
#define BLANG_VERSION "Blaest Trunk v0.2"
#define DBG_RUN(n)
#endif

#ifdef _BLANG_USE_BUFFER
    #define BLANG_BUFFER_TYPE char*
#else
    #define BLANG_BUFFER_TYPE FILE*
    #define BLANG_BUFFER_IS_FILE 1
#endif

typedef struct{
    BLANG_WORD_TYPE pos;
    BLANG_WORD_TYPE size;
} memlease_t;

typedef struct{
    BLANG_WORD_TYPE* stack;
    BLANG_WORD_TYPE* args;
    unsigned char* mmap;
    memlease_t* memoryLeases;
    BLANG_WORD_TYPE memlptr;
    BLANG_WORD_TYPE* memory;
    
    struct global_t* globals;
    BLANG_WORD_TYPE globptr;

    BLANG_WORD_TYPE a;
    BLANG_WORD_TYPE bp;
    BLANG_WORD_TYPE sp;
    BLANG_WORD_TYPE pc;
} B_State;

typedef struct global_t{
    const char* name;
    int type;
    BLANG_WORD_TYPE addr;
    BLANG_WORD_TYPE (*function)(B_State* f);
} global_t;

typedef struct{
    const char* name;
    BLANG_WORD_TYPE addr;
} label_t;

typedef struct{
    int from;
    BLANG_WORD_TYPE statementNumber;
    BLANG_WORD_TYPE statementSubNumber;
    BLANG_WORD_TYPE startpos;
    BLANG_WORD_TYPE endpos;
} ifdat_t;

typedef struct{
    B_State *s;
    char* lineBuffer;
    
    BLANG_WORD_TYPE *finalBuffer;
    int fbptr;
    int position;
    char** strLiteralBuffer;
    int strLiteralPtr;
    
    BLANG_WORD_TYPE fnNumber;
    
    int block;
    char macro;
    char lastNL;
} B_JITState;

extern void B_Push(B_State*, BLANG_WORD_TYPE);
extern BLANG_WORD_TYPE B_Pop(B_State*);
extern void B_itoa(BLANG_WORD_TYPE, char*);

/* Check if string 1 (s1) starts with string 2 (s2) or vice versa */
/* TODO: replace this with strncmp unless we have no libc */
char 
strstart(const char* s1, const char* s2)
{
    int x;
    for(x = 0; s1[x] != 0 && s2[x] != 0; x++){
        /* Note: because line_buffer is BLANG_WORD_TYPE, we need to cast it to
         * char before we compare */
        if((char)s1[x] != s2[x]){
            return 0;
        }
    }
    if(s2[x] == 0){
        return 1;
    }
    else{
        return 0;
    }
}

/* [Might be pending removal] Finds if the string given has and opening and
 * closing parenthese.  Probably not the most efficent thing to make its own
 * call, but this was done mostly because I thought it would be more important
 * but turns out it isn't. */
char 
strhasp(const char* s1)
{
    int y = 0;
    int x;
    for(x = 0; s1[x] != 0; x++){
        if(s1[x] == '('){
            y++;
        }
        else if(s1[x] == ')'){
            y++;
        }
    }
    return y;
}

char
strhas(const char* s1, char ch){
    int x = 0;
    for(; s1[x] != 0; x++){
        if((char)s1[x] == ch){
            return x;
        }
    }
    return 0;
}

static char* 
B_GenerateStatementJumpName(char type, BLANG_WORD_TYPE one, BLANG_WORD_TYPE two)
{
    char *zlabName, *oneNumBuffer, *twoNumBuffer;
    int zptr, cptr;
        
    zptr = 0;
    
    zlabName = (char*)malloc(64 * sizeof(char));
    oneNumBuffer = (char*)malloc(25 * sizeof(char));
    twoNumBuffer = (char*)malloc(25 * sizeof(char));

    
    B_itoa(one, oneNumBuffer);
    B_itoa(two, twoNumBuffer);
    zlabName[zptr++] = type;
    
    for(cptr = 0; oneNumBuffer[cptr] != 0; cptr++){
        zlabName[zptr++] = oneNumBuffer[cptr];
    }
    
    zlabName[zptr++] = 'n';
    
    for(cptr = 0; twoNumBuffer[cptr] != 0; cptr++){
        zlabName[zptr++] = twoNumBuffer[cptr];
    }
    zlabName[zptr] = 0;
    
    free(oneNumBuffer);
    free(twoNumBuffer);
    
    return zlabName;
}

BLANG_WORD_TYPE
B_atoi(const char* s1){
    BLANG_WORD_TYPE x, ret;
    ret = 0;

    for(x = 0; s1[x] != 0; x++){
        if(s1[x] >= 48 && s1[x] <= 57){
            BLANG_WORD_TYPE value = s1[x] - 48;

            if(ret == 0){
                ret = value;
            }
            else{
                ret *= 10;
                ret += value;
            }
        }
    }
    return ret;
}

void
B_stripString(char* s1){
    int len = 0;
    
    /* L strip */
    while(*s1 <= 32){
        s1++;
    }
    
    while(s1[len] != 0){
        len++;
    }
    
    while(s1[len] <= 32){
        s1[len] = 0;
        len--;
    }
    
}

static void 
B_innrItoa(BLANG_WORD_TYPE num, char* s1, int* index)
{
    int x;
    
    x = num / 10;
    if(x){
        B_innrItoa(x, s1, index);
    }
    s1[(*index)++] = num % 10 + 48;
}

void 
B_itoa(BLANG_WORD_TYPE num, char* s1)
{
    int index = 0;
    B_innrItoa(num, s1, &index);
    s1[index] = 0;
}


char* 
B_toString (const BLANG_WORD_TYPE* s1)
{
    char* ret = (char*)malloc(64 * sizeof(char));
    int x = 0;

    for(; s1[x] != 0; x++){
        ret[x] = s1[x];
    }
    ret[x] = 0;
    return ret;
}

void
B_copyString (const BLANG_WORD_TYPE* s1, char* s2, BLANG_WORD_TYPE size)
{
    BLANG_WORD_TYPE x = 0;

    for(; x < size; x++){
        s2[x] = (char)s1[x];
    }
    s2[x] = 0;
}

void B_stringToWords(const char *s1, BLANG_WORD_TYPE *s2, int size)
{
    int x = 0;
    
    for(; x != size; x++){
        s2[x] = (char)s1[x];
    }
}

/* 
 * Main runtime system 
 */

BLANG_WORD_TYPE 
B_Run(B_State* s, int pc)
{
    /* A temporary register nobody else needs to know about 
     * (Might be a good idea to keep this in the state) */
    BLANG_WORD_TYPE z = 0;
    
    /* TODO: Make this function single step instructions, so we can run
     * multiple blaest applications from a single thread, will really help with
     * legacy systems and embeded systems which maybe only have a single core 
     * or inefficient multithreading */

    for(s->pc = pc; s->pc < 10000; s->pc++){
        
        DBG_RUN(
            printf("[INT] [LOAD AT %d ] - %x(%c)\n", s->pc, s->memory[s->pc], s->memory[s->pc]);
        );

        switch(s->memory[s->pc]){
            /* Set accumulator */
            case 'A':{
                BLANG_WORD_TYPE value;

                value = s->memory[++s->pc];
                DBG_RUN(printf("Got value %d\n", value));
                s->a = value;
            }
            break;
            case 'a':{
                BLANG_WORD_TYPE_SIGNED value;

                /* The value we get here is the position relative to BP of our
                 * variable on the stack, so read it and add BP to it */
                value = (BLANG_WORD_TYPE_SIGNED)s->memory[++s->pc];
                value += s->bp;

                DBG_RUN(
                    printf("Got value from var %d at %d\n", s->stack[value], value)
                );

                /* Now set A to that value */
                s->a = s->stack[value];
            }
            break;

            /* Set accumulator from memory position */
            /* NOTE: Maybe make y work by popping two values from stack */
            case 'y':{
                BLANG_WORD_TYPE stackpos, mempos;

                stackpos = s->memory[++s->pc];
                stackpos += s->bp;
                DBG_RUN(printf("Got stack pos %d\n", stackpos));

                
                mempos = s->stack[(int)stackpos];
                
                mempos += B_Pop(s);

                s->a = s->memory[mempos];

                DBG_RUN(
                    printf("Got value from mempos %d\n", s->memory[(int)mempos])
                );

            }
            break;
            
            case 'Y':{
                BLANG_WORD_TYPE mempos;

                mempos = s->memory[++s->pc];
                mempos += B_Pop(s);

                s->a = s->memory[mempos];

                DBG_RUN(
                    printf("Got value from mempos %d\n", s->memory[(int)mempos])
                );

            }
            break;
            
            case 'G':{
                s->a = BLANG_MEMORY_SIZE + s->memory[++s->pc];
                s->a += s->bp;
                DBG_RUN(
                    printf("[INT] GOT MEMORY VALUE %d\n", s->a);
                );
            }
            break;
            
            /* Negate Accumulator */
            case 'N':{
                s->a = -(s->a);
                DBG_RUN(
                    printf("Negated %d to %d\n", -(s->a), s->a);
                );
            }
            break;

            /* Store from A */
            case 'S':{
                BLANG_WORD_TYPE stackpos;
                stackpos = s->memory[++s->pc];
                stackpos += s->bp;

                /* If the stackpos is larger than our stack pointer, we are 
                 * going higher into the stack */
                if(stackpos >= s->sp){
                    s->sp = stackpos + 1;
                    DBG_RUN(
                        printf("[INT] SP updated to %d\n", s->sp)
                    );
                }

                /* Im not entirely sure why, but not casting this WILL cause a 
                 * sigsegv (can C not index with unsigned long?)*/
                s->stack[(int)stackpos] = s->a;
                
                DBG_RUN(
                    printf("[INT] Stack %d (%d) [%d] set to %d\n", stackpos - s->bp, stackpos, stackpos + BLANG_MEMORY_SIZE, s->a)
                );

                }
            break;

            /* Store at memory position */
            
            /* NOTE: In the future, we should probably treat arrays teh same as
             * globals when those are implemented, just that the globals are a 
             * specific memory address and arrays are that memory address from
             * the variable plus the amount in the brackets */
            case 's':{
                BLANG_WORD_TYPE stackpos, mempos, addative;
                stackpos = s->memory[++s->pc];
                stackpos += s->bp;
                
                addative = B_Pop(s);

                /* If the stackpos is larger than our stack pointer, we are 
                 * going higher into the stack */
                if(stackpos >= s->sp){
                    s->sp = stackpos + 1;
                    DBG_RUN(
                        printf("[INT] SP updated to %d\n", s->sp)
                    );
                }

                /* Im not entirely sure why, but not casting this WILL cause a 
                 * sigsegv (can C not index with unsigned long?)*/
                mempos = s->stack[(int)stackpos];
                mempos += addative;

                s->memory[mempos] = s->a;
                
                DBG_RUN(
                    printf("Set memory position [%d (+ %d)] to %d\n", mempos - addative, mempos, s->a);
                );

                s->a = 0;
            }
            break;
            
            case 'w':{
                BLANG_WORD_TYPE mempos, addative;
                
                addative = B_Pop(s);
                mempos = s->memory[++s->pc];
                
                mempos += addative;

                s->memory[mempos] = s->a;
                
                DBG_RUN(
                    printf("Set memory position [%d] to %d\n", mempos, s->a);
                );

                s->a = 0;
            }
            break;
            
            /* TODO: MAke opcode that pops two values from stack, adds them
             * together, and sets that mempos to a, for use in 2d arrays */
            
            /* Compare equal */
            case 'e':{
                BLANG_WORD_TYPE left,right;
                
                /* Pop our values from the stack */
                right = B_Pop(s);
                left = B_Pop(s);
                
                if(left == right){
                    s->a = 1;
                }
                else{
                    s->a = 0;
                }
                
                DBG_RUN(
                    printf("Set A to %d\n", s->a);
                );
            }
            break;
            
            /* Compare not equal */
            case 'n':{
                BLANG_WORD_TYPE left,right;
                
                /* Pop our values from the stack */
                right = B_Pop(s);
                left = B_Pop(s);
                
                DBG_RUN(
                    printf("Compare %d and %d\n", left, right);
                );
                
                if(left != right){
                    s->a = 1;
                }
                else{
                    s->a = 0;
                }
                
                DBG_RUN(
                    printf("Set A to %d\n", s->a);
                );
            }
            break;
            
            case '<':{
                BLANG_WORD_TYPE left,right;
                
                /* Pop our values from the stack */
                right = B_Pop(s);
                left = B_Pop(s);
                
                DBG_RUN(
                    printf("Compare %d < %d\n", left, right);
                );
                
                if(left < right){
                    s->a = 1;
                }
                else{
                    s->a = 0;
                }
                
                DBG_RUN(
                    printf("Set A to %d\n", s->a);
                );
            }
            break;
            
            case '>':{
                BLANG_WORD_TYPE left,right;
                
                /* Pop our values from the stack */
                right = B_Pop(s);
                left = B_Pop(s);
                
                DBG_RUN(
                    printf("Compare %d > %d\n", left, right);
                );
                
                if(left > right){
                    s->a = 1;
                }
                else{
                    s->a = 0;
                }
                
                DBG_RUN(
                    printf("Set A to %d\n", s->a);
                );
            }
            break;
            
            /* Jump if the accumulator is zero */
            case 'z':{
                BLANG_WORD_TYPE mempos;
                mempos = s->memory[++s->pc];
                
                if(s->a == 0){
                    s->pc = mempos - 1;
                }
                
                DBG_RUN(
                    printf("Set memory position [%d]\n", s->pc);
                );
            }
            break;
             
            /* Memory allocate */
            case 'm':{
                s->sp += s->memory[++s->pc] - 1;
                DBG_RUN(
                    printf("SP set to [%d]\n", s->sp);
                );
            }  
            break;

            /* Call */
            case 'c':{
                int gotopos;
                /* Eventually maybe get the call address from A (this would
                 * allow for local variables to be called as if they were
                 * functions) */

                /* Reset the accumulator since that will hold our return value */
                s->a = 0;

                DBG_RUN(
                    printf("Current PC = %d\n", s->pc + sizeof(BLANG_WORD_TYPE));
                    printf("[INT] Bp (%d) updated to Sp (%d)\n", s->bp, s->sp);
                    printf("SP: %d, ", s->sp);
                );
                
                /* Push our BP to the stack */
                B_Push(s, s->bp);

                DBG_RUN(
                    printf("SP: %d, ", s->sp)
                );

                /* Do the same with our return address */
                B_Push(s, s->pc + 1);

                DBG_RUN(
                    printf("SP: %d\n", s->sp)
                );

                /* Our stack pointer should point to the absolute top of the 
                 * stack, so set the base pointer to that position */
                s->bp = s->sp;

                DBG_RUN(
                    printf("Stacking arguements\n");
                );

                gotopos = s->memory[++s->pc];
                
                DBG_RUN(
                    printf("[INT] Going to %d\n", gotopos);
                    printf("BP IS %d\n", s->bp);
                );
                
                /* Now actually jump to the position */
                s->pc = gotopos;
                s->pc--;
            }
            break;

            /* Push */
            case 'O':{
                B_Push(s, s->a);

                DBG_RUN(
                    printf("Pushed %d\n", s->a);
                );

                s->a = 0;
                break;
            }
            
            /* Pop */
            case 'o':{
                s->a = B_Pop(s);
                
                DBG_RUN(
                    printf("Popped %d\n", s->a);
                );
            }
            break;
            
            /* Pop and destroy (used for functions)*/
            case 'x':{
                B_Pop(s);
                DBG_RUN(
                    printf("Popped and destroyed value\n");
                );
            }
            break;

            /* Call C Function */
            case 'C':{
                BLANG_WORD_TYPE calladdr;
                int x, bptr;

                DBG_RUN(
                    printf("Current PC = %d\n", s->pc + sizeof(BLANG_WORD_TYPE));
                    printf("[INT] Bp (%d) updated to Sp (%d)\n", s->bp, s->sp);
                );

                B_Push(s, s->bp);
                s->bp = s->sp;
                calladdr = s->memory[++s->pc];

                DBG_RUN(
                    printf("CALLADR: %d\n", calladdr)
                );

                x = 0;
                for(; x < (int)s->globptr; x++){
                    DBG_RUN(
                        printf("CHECKGLOB: %d =? %d\n", s->globals[x].addr, calladdr)
                    );
                    if(s->globals[x].type == 1 && s->globals[x].addr == calladdr){

                        /* Set A to the outcome of the function */
                        s->a = s->globals[x].function(s);
                        
                        
                        
                        goto callCFuncDone;
                    }
                }

                DBG_RUN(
                    printf("Not Found\n")
                );

                callCFuncDone:
                bptr = B_Pop(s);
                s->bp = bptr;
                
            }
            break;

            case 'j':{
                /* We need to separate this out so GCC doesnt give us errors */
                s->pc++;
                s->pc = s->memory[s->pc] - 1;

                DBG_RUN(
                    printf("GO TO %d\n", s->pc);
                );
            }
            break;
            
            /* Will be removed, kept here for now just incase I actually need
             * this to be a real function, I will remove it when I need this 
             * opcode space, do NOT use this function */
            case 'H':{
                DBG_RUN(
                    printf("HALT\n");
                );
                return 0;
            }
            break;

            /* [Might be pending removal] Reset acculumulator */
            case 'R':{
                s->a = 0;
            }
            break;

            /* Return */
            case 'r':{
                BLANG_WORD_TYPE ret, bptr;

                if(s->bp == 0){
                    DBG_RUN(
                        printf("HALT CATCH FIRE\n");
                    );

                    return s->a;
                }
                s->sp = s->bp;
                DBG_RUN(
                    printf("SP: %d, ", s->sp)
                );
                ret = B_Pop(s);

                DBG_RUN(
                    printf("SP: %d, ", s->sp)
                );

                bptr = B_Pop(s);
                
                DBG_RUN(
                    printf("SP: %d\n", s->sp);
                    printf("Going back to %d\n", ret);
                    printf("BP set to %d\n", bptr);
                );
                s->pc = ret;
                s->bp = bptr;
            }
            break;
            
            /* Math stuff here */
            
            /* Add to temp register from Acculumulator */
            case '+':{
                z += s->a;
            }
            break;

            /* Same as above, but opposite... you get what I mean */
            case '-':{
                z -= s->a;
            }
            break;
            
            case '*':{
                z *= s->a;
            }
            break;
            
            case '/':{
                z /= s->a;
            }
            break;
            
            case '%':{
                z %= s->a;
            }
            break;
            
            case '&':{
                z &= s->a;
            }
            break;
            
            case '^':{
                z ^= s->a;
            }
            break;
            
            case '|':{
                z |= s->a;
            }
            break;

            /* Set A to value of temp register, then reset the temp register */
            case '=':{
                s->a = z;
                z = 0;
            }
            break;
           

            default:
                DBG_RUN(
                    printf("[INT] LOAD BAD OPCODE %d - %x(%c)\n", s->pc, s->memory[s->pc], s->memory[s->pc]);
                );
            break;
        }

    }
    return -1;
}

/*
 * Main linker routine
 */

/* Go through the source and find globals, resolve them to the proper addresses
 * of their values (or if that address is their value) */
void
B_ResolveGlobals(B_State* s, BLANG_WORD_TYPE* src, BLANG_WORD_TYPE size)
{
    char* nameBuffer = (char*)malloc(64 * sizeof(char));
    int nbptr = 0;
    int resptr = 0;
    int addr = 0;
    global_t glob;
    int x;
    int memsize = 0;
    


    /* Tell our memory map about the size of our memory that is reserved for
     * the actual program */
    if(size % BLANG_MMAP_LIMIT){
        memsize++;
    }

    memsize += size / BLANG_MMAP_LIMIT;
    DBG_RUN(
        printf("MEMSIZE: %d\n", memsize);
    );

    for(x = 0; x < memsize; x++){
        s->mmap[x] = 1;
    }

    /* There is absolutely nothing wrong with this code somehow, although
     * for some reason it really looks like there should be */
    for(s->pc = 0; s->pc < size; s->pc++){

        DBG_RUN(
            int ptr = 0;
        );

        if(src[s->pc] == 'g' && ( 
        (src[s->pc - 1] == 'c' ) || 
        (src[s->pc - 1] == 'j' ) || 
        (src[s->pc - 1] == 'A' ) || 
        (src[s->pc - 1] == 'Y' ) || 
        (src[s->pc - 1] == 'z' ) || 
        (src[s->pc - 1] == 'w' )) ){
            for(s->pc++; src[s->pc] != 0; s->pc++){
                if(src[s->pc] < 33 || src[s->pc] > 127){
                    DBG_RUN(
                        printf("Fake global found\n");
                    );

                    goto resolveGlobalsError;
                }
                else{
                    nameBuffer[nbptr++] = src[s->pc];
                }
            }
            nameBuffer[nbptr] = 0;

            DBG_RUN(
                printf("Found global: %s\n", nameBuffer);
                for(; ptr < nbptr; ptr++){
                    printf("%x (%c)\n", nameBuffer[ptr], nameBuffer[ptr]);
                }
            );

            for(x = 0; x < (int)s->globptr; x++){
                if(strcmp(nameBuffer, s->globals[x].name) == 0){
                    DBG_RUN(
                        printf("Resolved to %d\n", s->globals[x].addr);
                    );
                    glob = s->globals[x];
                    addr = s->globals[x].addr;
                    goto resolveGlobalsResolved;
                }
                
            }

            printf("Blaest has run into a linking error and must stop\n-------------------------------------------------\n");
            printf("Couldn't resolve global '%s'\n[", nameBuffer);
            for(x = 0; nameBuffer[x] != 0; x++){
                printf(" 0x%x ", nameBuffer[x]);
            }
            printf("]\n\n");
            printf("This may be an error with variable parsing or global parsing, if so please report this to Blaest, make sure to include your source code which refuses to work.  ");
            printf("If it looks like you mispelled the variable, or maybe tried to reference a local variable where it can't be found, or really any other type of user error, please do not send a bug report.  Oh, and congratulations!\n");
            exit(1);

            goto resolveGlobalsError;

            resolveGlobalsResolved:
            if(glob.type == 0 || glob.type == 2 || glob.type == 3){
                s->memory[resptr++] = addr;
            }
            else if(glob.type == 1){
                s->memory[resptr - 1] = 'C';
                s->memory[resptr++] = addr;
                
            }

            resolveGlobalsError:

            memset(nameBuffer, 0, 64 * sizeof(char));
            nbptr = 0;
        }
        else{
        
            s->memory[resptr++] = src[s->pc];
        }
    }
    free(nameBuffer);
    free(src);
}


void
B_ResolveStringLiterals(B_JITState* bjs)
{
    int x, y, z;
    
    for(x = 0; x < bjs->strLiteralPtr; x++){
        global_t string;
        char* name = malloc(50 * sizeof(char));

        string.addr = bjs->position;
        bjs->finalBuffer[bjs->fbptr++] = ++(bjs->position);
        
        for(y = 0; bjs->strLiteralBuffer[x][y] != 0; y++){
            bjs->finalBuffer[bjs->fbptr++] = bjs->strLiteralBuffer[x][y];
            bjs->position++;
        }
        bjs->finalBuffer[bjs->fbptr++] = (BLANG_WORD_TYPE)0;
        bjs->position++;

        B_itoa(x, name);
        
        DBG_RUN(
            printf("NAME IS: \n");
            for(y = 0; y < (int)strlen(name); y++){
                printf("\t(%x) - %c\n", name[y], name[y]);
            }
        );

        z = strlen(name);

        name[z++] = 'S';
        name[z++] = 'l';
        name[z++] = 0;

        DBG_RUN(
            printf("NAME IS: \n");
            for(y = 0; y < (int)strlen(name); y++){
                printf("\t(%x) - %c\n", name[y], name[y]);
            }
        );

        string.name = name;
        string.type = 3;

        bjs->s->globals[bjs->s->globptr++] = string;

    }
}

/*
 * Main JIT Compiler routine
 */

/* This will make everything easier, trust me */
#define jit_line_recur(x) B_PrivJITLine(s, x, finalBuffer, symBuffer, globals, globptr, block, position, fbptr, sym, fnNumber, ifTree, globalStatementNumber, ifPtr, lineEnding, isNegative)

#define BLANG_FROM_BLANK            0
#define BLANG_FROM_IF_NO_BLOCK      1
#define BLANG_FROM_IF_BLOCK         2
#define BLANG_FROM_ELSE_NO_BLOCK    3
#define BLANG_FROM_ELSE_BLOCK       4
#define BLANG_FROM_WHILE_NO_BLOCK   5
#define BLANG_FROM_WHILE_BLOCK      6
#define BLANG_SEARCH                7

static const char B_Operators[] = {'+', '-', '*', '/', '%', '&', '|', '^'};

/* TODO: Eventaully turn this long list of arguements into a single struct that
 * gets passed around. (This will require a very lengthy rewrite)*/
static void
B_PrivJITLine(B_State* s, char* lineBuffer, BLANG_WORD_TYPE* finalBuffer, char** symBuffer, global_t* globals, BLANG_WORD_TYPE* globptr, int* block, int* position, int* fbptr, int* sym, BLANG_WORD_TYPE* fnNumber, ifdat_t* ifTree, int* globalStatementNumber, int* ifPtr, int lineEnding, int isNegative)
{
    /* Eventually these will need to be added to the struct once that gets 
     * implemented */
    static int testGlobalNum = 0;
    static int isRecurredArray = 0;
    
    DBG_RUN(
        printf("\n");
        printf("[JIT] Compiling %s\n", lineBuffer);
        printf("[JIT] Position is %d\n", *position);
    );

    /* Check for line first characters */
    if(lineBuffer[0] == 0){
        /* Quick and dirty exit if we have a null string */
        return;
    }
    /* Eventually check for things like * or & here too */
    
    if(strstart(lineBuffer, "auto")){
        char* arrbuffer;
        int p, y, a, b, stackpos, arrnum;
        char* nameBuffer, *extbuffer;

        nameBuffer = (char*)malloc(64 * sizeof(char)); 

        p = 5;
        y = 0;
        for(; lineBuffer[p - 1] != 0; p++){

            if(lineBuffer[p] > 32 || lineBuffer[p] == 0){
                if(lineBuffer[p] == ',' || lineBuffer[p] == 0){
                    nameBuffer[y] = 0;
                    
                    DBG_RUN(
                        printf("NAMEBUFFER: %s\n", nameBuffer);
                        printf("Last Char %c\n", nameBuffer[y - 1]);
                    );
                    
                    if(nameBuffer[y - 1] == ']'){
                        int presym;
                        arrbuffer = (char*)malloc(64 * sizeof(char)); 
                        extbuffer = (char*)malloc(64 * sizeof(char)); 

                        for(b = 0; nameBuffer[b] != '[' && b < y; b++){
                            extbuffer[b] = nameBuffer[b];
                        };
                        
                        extbuffer[b] = 0; 
                        
                        for(b++, a = 0; nameBuffer[b] != ']' && b < y; b++){
                            arrbuffer[a++] = nameBuffer[b];
                        }
                        arrbuffer[a] = 0;
                        DBG_RUN(printf("Arrbuffer: %s\n", arrbuffer));
                        arrnum = B_atoi(arrbuffer);

                        stackpos = *sym;
                        symBuffer[(*sym)++] = extbuffer;
                        
                        finalBuffer[(*fbptr)++] = 'G';
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = (*sym) + 1;
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = 'S';
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = stackpos;
                        (*position)++;
                        
                        presym = *sym;
                        for(; *sym != presym + arrnum - 1; (*sym)++){
                            symBuffer[*sym] = NULL;
                        }
                        
                        finalBuffer[(*fbptr)++] = 'm';
                        (*position)++;

                        finalBuffer[(*fbptr)++] = arrnum;
                        (*position)++;

                        free(arrbuffer);
                    }
                    else{
                        DBG_RUN(printf("NameBuffer: %s\n", nameBuffer));
                        symBuffer[(*sym)++] = nameBuffer;
                    }
                    
                    y = 0;
                    nameBuffer = (char*)malloc(64 * sizeof(char)); 

                }
                else{
                    nameBuffer[y++] = lineBuffer[p];
                }
            }
        }
        free(nameBuffer);
    }
    else if(strstart(lineBuffer, "goto")){
        int lineptr = 5;
        int f;
        char* number = malloc(50 * sizeof(char));
        
        /* Get our function number as a string */
        B_itoa(*fnNumber, number);

        finalBuffer[(*fbptr)++] = 'j';
        (*position)++;

        finalBuffer[(*fbptr)++] = 'g';
        
        DBG_RUN(
            printf("Found GOTO: ");
        );
        
        for(f = 0; f < (int)strlen(number); f++){
            finalBuffer[(*fbptr)++] = number[f];
            
            DBG_RUN(
                putchar(number[f]);
            );
        }

        for(;lineBuffer[lineptr] != 0; lineptr++){
            finalBuffer[(*fbptr)++] = lineBuffer[lineptr];
            
            DBG_RUN(
                putchar(lineBuffer[lineptr]);
            );
        }
        
        DBG_RUN( printf("\n"); );
        finalBuffer[(*fbptr)++] = 0;
        (*position)++;
    }
    else if(strstart(lineBuffer, "return")){
        if(lineBuffer[7] == 0){
            /* We have a quick return, no return type */
            finalBuffer[(*fbptr)++] = 'R';
            (*position)++;
            
            finalBuffer[(*fbptr)++] = 'r';
            (*position)++;
        }
        else{
            /* We have an actual return arguement */
            
            jit_line_recur(lineBuffer + 7);
            
            finalBuffer[(*fbptr)++] = 'r';
            (*position)++;
        }
    }
    else if(strstart(lineBuffer, "if") || strstart(lineBuffer, "while") || strstart(lineBuffer, "else if")){
        /* We have an if statement */
        int ifptr, cptr, startpos, inBrackets;
        char* cmpBuffer, *statementNumBuffer, *statementSubNumBuffer;
        
        if(ifTree[*ifPtr].from == BLANG_SEARCH && lineBuffer[0] != 'e'){
            global_t zlabel;
            char* zlabName;
            /* We have a previous if statement looking for an else if */
            DBG_RUN(
                printf("Found no new statement for previous statement, ending, called from if parse\n");
            );
            
            /* There is no actual new statement */
            DBG_RUN(
                printf("Found no new statement for previous statement, ending\n");
            );
            
            zlabName = B_GenerateStatementJumpName('d', ifTree[*ifPtr].statementNumber, 0);
            zlabel.name = zlabName;
            zlabel.type = 2;
            zlabel.addr = ifTree[*ifPtr].endpos;

            globals[(*globptr)++] = zlabel;
            
            (*ifPtr)--;
            
        }
        
        cmpBuffer = (char*)malloc(64 * sizeof(char));
        
        /* Save our starting position in case we have a while loop */
        startpos = *position;
        
        DBG_RUN(
            printf("If Statement Here\n");
        );
        
        for(ifptr = 0; lineBuffer[ifptr] != '('; ifptr++);
        
        /* Go through and get the interior of the IF statement, since it is 
         * looking for the closing parenthesis, we need to keep track of the
         * level deep it goes so we can make it end on the right place */
        for(ifptr++, inBrackets = 1, cptr = 0; inBrackets; ifptr++){
            
            if(lineBuffer[ifptr] > 32){
                if(lineBuffer[ifptr] == '('){
                    inBrackets++;
                }
                else if(lineBuffer[ifptr] == ')'){
                    inBrackets--;
                    if(!inBrackets){
                        break;
                    }
                }
                
                cmpBuffer[cptr++] = lineBuffer[ifptr];
            }
            
        }
        cmpBuffer[cptr] = 0;
        
        DBG_RUN(
            printf("Got internal sequence '%s'.  Compiling that...\n", cmpBuffer);
        );
        
        jit_line_recur(cmpBuffer);
        free(cmpBuffer);
        
        /* Get our statement number in array form */
        statementNumBuffer = (char*)malloc(25 * sizeof(char));
        statementSubNumBuffer = (char*)malloc(25 * sizeof(char));
        
        
        
        if(lineBuffer[0] == 'e'){
            ifTree[*ifPtr].statementSubNumber++;
        }
        else{
            (*ifPtr)++;
            
            ifTree[*ifPtr].statementNumber = testGlobalNum++;
            ifTree[*ifPtr].statementSubNumber = 0;
        }
    
        B_itoa(ifTree[*ifPtr].statementNumber, statementNumBuffer);
        B_itoa(ifTree[*ifPtr].statementSubNumber, statementSubNumBuffer);
        
        /* If zero, jump to the end of the statement */
        finalBuffer[(*fbptr)++] = 'z';
        (*position)++;

        finalBuffer[(*fbptr)++] = 'g';
        finalBuffer[(*fbptr)++] = 's';
        
        for(cptr = 0; statementNumBuffer[cptr] != 0; cptr++){
            finalBuffer[(*fbptr)++] = statementNumBuffer[cptr];
        }
        
        finalBuffer[(*fbptr)++] = 'n';
        
        for(cptr = 0; statementSubNumBuffer[cptr] != 0; cptr++){
            finalBuffer[(*fbptr)++] = statementSubNumBuffer[cptr];
        }
        
        finalBuffer[(*fbptr)++] = 0;
        (*position)++;
        
        free(statementNumBuffer);
        free(statementSubNumBuffer);
        
        /* TODO: Set from here */ 
        if(lineEnding){
            /* Since we use this same function for both If and While definitions
             * we need to be able to tell them apart, specifically here since we
             * already match the string start we can just check the first char
             * and see if that is an i or a w, and we can infer the statement 
             * type based on that */
             
            if(lineBuffer[0] == 'i' || lineBuffer[0] == 'e'){
                ifTree[*ifPtr].from = BLANG_FROM_IF_BLOCK;
            }
            else{
                ifTree[*ifPtr].from = BLANG_FROM_WHILE_BLOCK;
                ifTree[*ifPtr].startpos = startpos;
            }
        }
        else{
            /* We have a one line If statement */
            global_t zlabel;
            char* zlabName = B_GenerateStatementJumpName('s', ifTree[*ifPtr].statementNumber, ifTree[*ifPtr].statementSubNumber);
            
            for(ifptr++; lineBuffer[ifptr] <= 32; ifptr++)
            ;
            
            DBG_RUN(
                printf("Compiling statement '%s'\n", lineBuffer + ifptr);
            );
            
            jit_line_recur(lineBuffer + ifptr);
            
            if(lineBuffer[0] == 'i' || lineBuffer[0] == 'e'){
                ifTree[*ifPtr].from = BLANG_FROM_IF_NO_BLOCK;
            }
            else{
                ifTree[*ifPtr].from = BLANG_FROM_WHILE_NO_BLOCK;
                
                /* Make the jump infinite */
                finalBuffer[(*fbptr)++] = 'j';
                (*position)++;
                finalBuffer[(*fbptr)++] = startpos;
                (*position)++;
            }
            
            /* Do our position for the global down here so we can inject while
             * code for the while loop */
            zlabel.name = zlabName;
            zlabel.type = 2;
            zlabel.addr = *position;

            s->globals[s->globptr++] = zlabel;
            
            /* Now position should be set for our zero condition */
        }
    }
    else if(strstart(lineBuffer, "else")){   
        
        ifTree[*ifPtr].from = BLANG_FROM_ELSE_BLOCK;
        
    }
    else if(strhas(lineBuffer, '<') || strhas(lineBuffer, '>')){
        int lbptr, comparison;
        char* leftSide;
        
        comparison = 0;
        leftSide = (char*)malloc(64 * sizeof(char));
        
        DBG_RUN(
            printf("Finding left side...\n");
        );
        
        /* Get the left side of the compare */
        for(lbptr = 0; lineBuffer[lbptr] != '<' && lineBuffer[lbptr] != '>'; lbptr++){
            leftSide[lbptr] = lineBuffer[lbptr];
        }
        
        leftSide[lbptr] = 0;
        
        
        DBG_RUN(
            printf("Got left side: %s\n", leftSide);
        );
        
        /* Find out which type of compare we are doing, and set the variable 
         * accordingly */
        if(lineBuffer[lbptr] == '<'){
            comparison = 1;
        }
        else if(lineBuffer[lbptr] == '>'){
            comparison = 2;
        }
        
        if(lineBuffer[lbptr + 1] == '='){
            /* We increase the lbptr by two since we will use that to find the 
             * right side, and <= or >= take up two characters */
            lbptr += 2;
            
            /* Update to reflect we are also checking equals */
            if(comparison == 1){
                comparison = 3;
            }
            else if(comparison == 2){
                comparison = 4;
            }
        }
        else{
            /* If we are just < or >, those both only take up one character, so
             * we only need one additional character */
            lbptr += 1;
        }
        
        DBG_RUN(
            printf("Got other side: %s\n", lineBuffer + lbptr);
        );
        
        /* Now we compile everything */
        B_stripString(leftSide);
        jit_line_recur(leftSide);
        
        
        finalBuffer[(*fbptr)++] = 'O';
        (*position)++;
        
        jit_line_recur(lineBuffer + lbptr);
        
        finalBuffer[(*fbptr)++] = 'O';
        (*position)++;
        
        /* Setup the specific comparisons */
        if(comparison == 1){
            finalBuffer[(*fbptr)++] = '<';
            (*position)++;
        }
        else if(comparison == 2){
            finalBuffer[(*fbptr)++] = '>';
            (*position)++;
        }
        else{
            if(comparison == 3){
            finalBuffer[(*fbptr)++] = '<';
            (*position)++;
            }
            else if(comparison == 4){
                finalBuffer[(*fbptr)++] = '>';
                (*position)++;
            }
            
            /* Because we also need to check equals, we actually add the results
             * of the check for either < or > to the result for =.  As long as 
             * the result is 1 we will run an if statement */
            finalBuffer[(*fbptr)++] = '+';
            (*position)++;
            
            /* We also need to run everything through the compiler again since 
             * we are testing for another condition */
            jit_line_recur(leftSide);
        
        
            finalBuffer[(*fbptr)++] = 'O';
            (*position)++;
            
            jit_line_recur(lineBuffer + lbptr);
            
            finalBuffer[(*fbptr)++] = 'O';
            (*position)++;
            
            finalBuffer[(*fbptr)++] = 'e';
            (*position)++;
            
            finalBuffer[(*fbptr)++] = '+';
            (*position)++;
            
            finalBuffer[(*fbptr)++] = '=';
            (*position)++;
        }
        free(leftSide);
    }
    /* Make this faster */
    else if(strhas(lineBuffer, '=')){
        char* arrayValue, *value;
        char* varName;
        int vnptr, lbptr, stackpos, isarray, aptr;

        isarray = 0;
        varName = (char*)malloc(64 * sizeof(char));
        for(lbptr = 0, vnptr = 0; lineBuffer[lbptr] != '='; lbptr++){
            if(lineBuffer[lbptr] == '['){
                isarray = 1; /* THIS NEEDS TO BE RECURED BACK INTO JIT_LINE!! */
                aptr = 0;

                arrayValue = (char*)malloc(64 * sizeof(char));
                for(lbptr++; lineBuffer[lbptr] != ']'; lbptr++){
                    if(lineBuffer[lbptr > 32]){
                        arrayValue[aptr++] = lineBuffer[lbptr];
                        
                    }
                }
                arrayValue[aptr] = 0;
                B_stripString(arrayValue);
                jit_line_recur(arrayValue);
                
                
                finalBuffer[(*fbptr)++] = 'O';
                (*position)++;
                
                free(arrayValue);
            }
            else if(lineBuffer[lbptr] > 32){
                varName[vnptr++] = lineBuffer[lbptr];
            }
            
        }
        varName[vnptr] = 0;
        
        if(lineBuffer[lbptr + 1] == '=' || lineBuffer[lbptr - 1] == '!'){
            char comparison;
            
            DBG_RUN(
                printf("LINEBUFFER: %s\n", lineBuffer + lbptr); 
            );
            
            if (lineBuffer[lbptr - 1] == '!'){
                DBG_RUN(
                    printf("LINE ENDING NOT EQUAL!!!\n");
                    printf("VARNAME: %s - VnPTR = %d", varName, vnptr);
                );
                
                varName[vnptr - 1] = 0;
                comparison = 'n';
                lbptr += 1;
            }
            else{
                DBG_RUN(
                    printf("LINE ENDING EQUAL!!!\n");
                );
                
                comparison = 'e';
                lbptr += 2;
            }
            
            /* We have a boolean compare statement */
            DBG_RUN(
                printf("Boolean compare here\n");
                printf("VARNAME: %s\n", varName);
            );
            /* At this point, varname should be the first thing we are
             * comparing, and after two equals signs we should have the other
             * thing to compare.  All we need to do is feed those into jit_line
             * and push them to the arguement stack */
             
             value = (char*)malloc(64 * sizeof(char));
             
             
             
             for(vnptr = 0; lineBuffer[lbptr] != 0; lbptr++){
                if(lineBuffer[lbptr] > 32){
                    value[vnptr++] = lineBuffer[lbptr];
                }
            }
            value[vnptr] = 0;
            
            DBG_RUN(
                printf("Value: %s\n", value);
            );
            
            /* Send the left side in first and push to the arg stack */
            
            /* Because the left side will have descarded the array data, we need
             * to set it like this */
            if(isarray){
                isRecurredArray = 1;
            }
            jit_line_recur(varName);
            free(varName);
            
            finalBuffer[(*fbptr)++] = 'O';
            (*position)++;
            
            /* Same with the right side */
            jit_line_recur(value);
            free(value);
            
            finalBuffer[(*fbptr)++] = 'O';
            (*position)++;
            
            /* Finally compare if they are equal or not */
            finalBuffer[(*fbptr)++] = comparison;
            (*position)++;
        }
        else{
            int isMathEqu, j;
            
            isMathEqu = 0;
            
            DBG_RUN(
                printf("Not a boolean compare statement! \n");
            );
            
            /* Is the character before the equals an operator, if it is, we have
             * ourself an assignment operator */
            for(j = 0; j < 8; j++){
                if(lineBuffer[lbptr - 1] == B_Operators[j]){
                    isMathEqu = lineBuffer[lbptr - 1];
                    varName[vnptr - 1] = 0;
                    break;
                }
            }
            
            /* This is literally ancient, please don't mind it, it still kinda 
             * works*/
            stackpos = -1;

            DBG_RUN(
                for(vnptr = 0; vnptr < *sym; vnptr++){
                    printf("SYM: %s\n", symBuffer[vnptr]);
                }
            );
            /* Reusing vnptr, for here it is a symbol pointer */
            for(vnptr = 0; vnptr <= *sym; vnptr++){
                if(symBuffer[vnptr] == NULL){
                    
                }
                else if(strcmp(varName, symBuffer[vnptr]) == 0){
                    stackpos = vnptr;
                    goto setvarPostLoop;
                }
            }
            /* We have a global value statement... hopefully */
            stackpos = -1;
            
            setvarPostLoop:
            
            
            if(isMathEqu){
                
                finalBuffer[(*fbptr)++] = 'a';
                (*position)++;
                
                finalBuffer[(*fbptr)++] = stackpos;
                (*position)++;
                
                finalBuffer[(*fbptr)++] = '+';
                (*position)++;
                
            }

            value = (char*)malloc(64 * sizeof(char));

            lbptr++;
            vnptr = 0;
            
            /* Reusing lbptr, reusing vnptr as well for value */
            for(; lineBuffer[lbptr] != 0; lbptr++){
                if(lineBuffer[lbptr] > 32){
                    value[vnptr++] = lineBuffer[lbptr];
                }
            }
            value[vnptr] = 0;

            lbptr = 0;
            
            DBG_RUN(
                printf("VALUE IS %s\n", value);
            );
            
            /* This needs to be here so we dont screw with the value */
            if(stackpos == -1){
                if(!isarray){
                    finalBuffer[(*fbptr)++] = 'R';
                    (*position)++;
                    
                    finalBuffer[(*fbptr)++] = 'O';
                    (*position)++;
                }
            }

            jit_line_recur(value);

            free(value);
            
            if(stackpos == -1){
                /* If we dont have an array, we need to set the addative to 0 */
                
                finalBuffer[(*fbptr)++] = 'w';
                (*position)++;
                
                finalBuffer[(*fbptr)++] = 'g';
                (*position)++;
                
                for(vnptr = 0; varName[vnptr] != 0; vnptr++){
                    finalBuffer[(*fbptr)++] = varName[vnptr];
                }
                
                free(varName);
                
                finalBuffer[(*fbptr)++] = 0;
                (*position)++;
                
                return;
            }
            else if(isarray){
                finalBuffer[(*fbptr)++] = 's';
                (*position)++;
            }
            else if(isMathEqu){
                finalBuffer[(*fbptr)++] = isMathEqu;
                (*position)++;
                
                finalBuffer[(*fbptr)++] = '=';
                (*position)++;
                
                finalBuffer[(*fbptr)++] = 'S';
                (*position)++;
            }
            else{
                finalBuffer[(*fbptr)++] = 'S';
                (*position)++;
            }

            free(varName);
            finalBuffer[(*fbptr)++] = stackpos;
            (*position)++;
            
            return;
        }
    }
    else if(strhasp(lineBuffer) && *block > 0){
        char* argbuf;
        char *namebuf;
        int p, j, i, argcount, inBrackets;

        argbuf = (char*)malloc(64 * sizeof(char)); 
        namebuf = (char*)malloc(64 * sizeof(char)); 
        p = 0;
        j = 0;
        argcount = 0;

        for(; lineBuffer[p] != '('; p++){
            if(lineBuffer[p] > 32){
                namebuf[j++] = (char)lineBuffer[p];
            }
        }
        namebuf[j] = 0;
        DBG_RUN(printf("NAMEBUF: %s\n", namebuf));

        j = 0;

        p++;

        /* TODO: Refine this whole section */
        for(inBrackets = 1; inBrackets; p++){
            
            if(lineBuffer[p] > 32 && (inBrackets >= 1 || (lineBuffer[p] != ',' || lineBuffer[p] != ')'))){
                /* Have we finished collecting this arguement? */
            
                /* We check inBrackets here to make sure we are parsing the correct
                 * arguements from the scope we want */
                if(inBrackets == 1 && (lineBuffer[p] == ',' || lineBuffer[p] == ')')){
                    argbuf[j] = 0;
                    if(j > 0){
                        DBG_RUN(
                            printf("Collected arguement %s\n", argbuf);
                        );
                        
                        argcount++;
                        jit_line_recur(argbuf);
                        finalBuffer[(*fbptr)++] = 'O';
                        (*position)++;
                    }
                    memset(argbuf, 0, 64 * sizeof(char));
                    j = 0;
                }
                else{
                    argbuf[j++] = lineBuffer[p];
                }
            }
            
            if(lineBuffer[p] == '('){
                inBrackets++;
            }
            else if(lineBuffer[p] == ')'){
                inBrackets--;
            }

        }
        free(argbuf);

        finalBuffer[(*fbptr)++] = 'c';
        (*position)++;

        finalBuffer[(*fbptr)++] = 'g';

        i = 0;
        for(; lineBuffer[i] != '('; i++){
            if(lineBuffer[i] > 32){
                finalBuffer[(*fbptr)++] = lineBuffer[i];
            }
        }
        finalBuffer[(*fbptr)++] = 0;

        free(namebuf);

        (*position)++; 
        
        for(; argcount > 0; argcount--){
            finalBuffer[(*fbptr)++] = 'x';

            (*position)++; 
        }
        return;
    }
    else{
        int g, h, j, isnumber, isalgo, isvar, isPointer, isBorrow;
        BLANG_WORD_TYPE valint;

        g = 0;
        isnumber = 1;
        isalgo = 0;
        isPointer = 0;
        isBorrow = 0;
        
        if(lineBuffer[0] == '-'){
            isNegative = 1;
            lineBuffer++;
        }
        
        for(; lineBuffer[g] != 0; g++){
            if(lineBuffer[g] < 48 || lineBuffer[g] > 57){
                DBG_RUN(
                    printf("\tLineBufferValue %d\n", lineBuffer[g]);
                );
                
                /* Go through our known operators and see if we have a math 
                 * operator, if we do we can somewhat rest assured we have some
                 * sort of math equation */
                for(j = 0; j < 8; j++){
                    if(lineBuffer[g] == B_Operators[j] && g >= 1){
                        isalgo = 1;
                        break;
                    }
                }
                
                isnumber = 0;
            }
        }

        DBG_RUN(
            printf("DONE HERE\n");
        );

        if(isnumber){
            
            DBG_RUN(
                printf("!!!Number found!!!\n");
            );

            /* We have a number, this means we just load that into the 
             * acculumulator, pretty simple */
            finalBuffer[(*fbptr)++] = 'A';
            (*position)++;
            
            valint = B_atoi(lineBuffer);
            DBG_RUN(printf("!!!!IMPORTANT!!!!\nVALINT: %d\n", valint));
            
          
            if(isNegative){
                finalBuffer[(*fbptr)++] = -valint;
                isNegative = 0;
            }
            else{
                finalBuffer[(*fbptr)++] = valint;
            }
            
            
            (*position)++;

            return;
        }
        else if(isalgo){
            /* We have a maths */
            char* algbuf;
            int h = 0;
            char lastSign = 0;
            int lastSignPos = 0;

            DBG_RUN(
                printf("ALGO FOUND\n");
            );

            /* Assign a buffer for our algorithm to live */
            algbuf = (char*)malloc(64 * sizeof(char));
            
            for(lastSignPos = 0, g = 0; lineBuffer[g] != 0; g++){
                for(j = 0; j < 8; j++){
                    if(lineBuffer[g] == B_Operators[j] && (g - lastSignPos != 0)){
                    
                        /* We have something starting out as negative */
                        if(g == 0){
                            
                        }
                        /* Since the first value should be the starting value, 
                         * we ALWAYS want that to be '+', otherwise I'd have to 
                         * write more lines of code (which I don't want to do) 
                         */
                        if(lastSign == 0){
                            lastSign = '+';
                        }

                        DBG_RUN(
                            printf("%c Found\n", lineBuffer[g]);
                        );
                        
                        algbuf[h] = 0;
                        
                        jit_line_recur(algbuf);
                        
                        DBG_RUN(
                            printf("[LOOP] WRITING SIGN %c while algbuf %s\n", lastSign, algbuf);
                        );
                        
                        finalBuffer[(*fbptr)++] = lastSign;
                        (*position)++;
                        lastSign = lineBuffer[g];
                        
                        lastSignPos = g + 1;
                        memset(algbuf, 0, 64 * sizeof(char));
                        h = 0;
                        break;
                        continue;
                    }
                    else if(j == 7){
                        if(lineBuffer[g] > 32){
                            algbuf[h++] = lineBuffer[g];
                        }
                    }
                }
                
                
            }
            jit_line_recur(algbuf);
            free(algbuf);

            
            finalBuffer[(*fbptr)++] = lastSign;
            (*position)++;
            
            
            finalBuffer[(*fbptr)++] = '=';
            (*position)++;

            return;
        }
        
        /* [Unintended consequence] The JIT will assume something is a variable
         * if it can't find anything else it should be, luckily it will be 
         * caught by the fake variable trap. */

        isvar = 0;
        valint = 0;
        
        if(lineBuffer[0] == '*'){
            isPointer = 1;
            lineBuffer++;
        }
        else if(lineBuffer[0] == '&'){
            isBorrow = 1;
            lineBuffer++;
        }
        
        for(g = 0; g <= *sym - 1; g++){
            isvar = 0;
            
            if(symBuffer[g] == NULL){
            }
            else{
                /* Go though and compare the strings */
                for(h = 0; lineBuffer[h] != 0 && symBuffer[g][h] != 0; h++){             
                    /* If they equal, keep isvar as 1 */
                    if((char)lineBuffer[h] == symBuffer[g][h]){
                        isvar = 1;
                    }
                    /* Else its not, go back to beginning */
                    else{

                        DBG_RUN(
                            printf("NOT FOUND: %s\n", symBuffer[g]);
                        );

                        isvar = 0;
                        break;
                    }
                }
            }
            DBG_RUN(
                printf("Here: \n");
                printf("LINEBUFFER: %s, Deliminator: %d\n", lineBuffer, h);
            );
            /* Actually sort the data */
            if(isvar == 1){
                
                /* Since we wait until either of the strings is null terminated
                 * if we have an array it will end before we know if it is an
                 * array, so we check if the current character at line buffer
                 * is not null terminated and instead has an open bracket */
                if((lineBuffer[h] != 0  && lineBuffer[h] == '[') || isRecurredArray){
                    /* Array found */
                    
                    char* arrayValue = (char*)malloc(64 * sizeof(char));
                    int arrayptr = 0;
                    
                    DBG_RUN(
                        printf("!!! ARRAY FOUND !!!\n");
                    );
                    
                    if(isRecurredArray){
                        DBG_RUN(
                            printf("!!! RECURRED ARRAY FOUND !!!\n");
                        );
                        goto recurredArrayResolve;
                    }
                    
                    /* Process the subscript for the array */
                    for(h++; lineBuffer[h] != ']' && lineBuffer[h] != 0; h++){
                        if(lineBuffer[h] > 32){
                            arrayValue[arrayptr++] = lineBuffer[h];
                        }
                    }
                    arrayValue[arrayptr] = 0;
                    
                    recurredArrayResolve:
                    
                    /* Do we have a borrow, this will make things weird for
                     * arrays, so we basically turn the array into a math 
                     * statement, arrayPosition + index = our value */
                    if(isBorrow){
                        
                        /* If we have come from an if statement, we dealt with
                         * the index already, so pop that and use it */
                        if(isRecurredArray){
                            finalBuffer[(*fbptr)++] = 'o';
                            (*position)++;
                            isRecurredArray = 0;
                            free(arrayValue);
                        }
                        else{
                            jit_line_recur(arrayValue);
                            free(arrayValue);
                        }
                        
                        finalBuffer[(*fbptr)++] = '+';
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = 'a';
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = g;
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = '+';
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = '=';
                        (*position)++;
                        
                        DBG_RUN(
                            printf("BORROW ARRAY %d\n", g);
                        );
                    }
                    else{
                        /* Otherwise just get the value */
                        if(isRecurredArray){
                            finalBuffer[(*fbptr)++] = 'o';
                            (*position)++;
                            isRecurredArray = 0;
                            free(arrayValue);
                        }
                        else{
                            jit_line_recur(arrayValue);
                            free(arrayValue);
                        }

                        finalBuffer[(*fbptr)++] = 'O';
                        (*position)++;

                        finalBuffer[(*fbptr)++] = 'y';
                        (*position)++;

                        finalBuffer[(*fbptr)++] = g;
                        (*position)++;
                        
                        /* If we are trying to get from a memory position, we
                         * just push the value and try to access memory position
                         * zero plus the value. */
                        if(isPointer){
                            finalBuffer[(*fbptr)++] = 'O';
                            (*position)++;
                            
                            finalBuffer[(*fbptr)++] = 'y';
                            (*position)++;

                            finalBuffer[(*fbptr)++] = 0;
                            (*position)++;
                        }
                    }
                    if(isNegative){
                        finalBuffer[(*fbptr)++] = 'N';
                        (*position)++;
                        isNegative = 0;
                    }
                    return;
                }
                /* If it isnt null terminated and doesnt have an open bracket,
                 * its a false match */
                else if(lineBuffer[h] != 0){

                    /* False positive */
                    DBG_RUN(printf("FAKE\n"););
                    isvar = 0;
                }
                /* MAKE SURE 100% it is correct */
                else if(lineBuffer[h] == symBuffer[g][h]){

                    /* Varible found */
                    
                        /*printf("!!!VARIABLE FOUND!!!\n");*/
                    symBuffer[g][h] = 0;
                    lineBuffer[h] = 0;
                    
                    
                    /* If we have a pointer, just substitute a for a y */
                    if(isPointer){
                        finalBuffer[(*fbptr)++] = 0;
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = 'O';
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = 'y';
                        (*position)++;
                    
                        finalBuffer[(*fbptr)++] = g;
                        (*position)++;
                        
                    }
                    else if(isBorrow){
                        finalBuffer[(*fbptr)++] = 'G';
                        (*position)++;
                        
                        finalBuffer[(*fbptr)++] = g;
                        (*position)++;
                    }
                    else{
                        finalBuffer[(*fbptr)++] = 'a';
                        (*position)++;
                    
                        finalBuffer[(*fbptr)++] = g;
                        (*position)++;
                    }
                    
                    if(isNegative){
                        finalBuffer[(*fbptr)++] = 'N';
                        (*position)++;
                        isNegative = 0;
                    }
                    return;
                }
            }
        
        }
        DBG_RUN(
            printf("Maybe global?\n");
        );
        finalBuffer[(*fbptr)++] = 'A';
        (*position)++;
        finalBuffer[(*fbptr)++] = 0;
        (*position)++;
        
        finalBuffer[(*fbptr)++] = 'O';
        (*position)++;

        finalBuffer[(*fbptr)++] = 'Y';
        (*position)++;
        finalBuffer[(*fbptr)++] = 'g';
        (*position)++;

        for(h = 0; lineBuffer[h] != 0; h++){
            if(lineBuffer[h] > 32){
                finalBuffer[(*fbptr)++] = lineBuffer[h];
            }
        }
        finalBuffer[(*fbptr)++] = 0;

    }
    
}

/* TODO: This function needs to MAJORLY be overhauled, since it basically has 
 * stayed the same since B_JIT and jit_line were the same function, but now that
 * JIT-ing needs to be done recursively B_JIT() itself needs to be reworked a bit
 * more. */
void 
B_JITStageOne(B_JITState* bjs, BLANG_BUFFER_TYPE src)
{
    char** symBuffer = (char**)malloc(1024 * sizeof(char*));

    char c; 
    int x = 0;
    int sym = 0;
    
    #ifdef _BLANG_USE_BUFFER
    int bptr = 0;
    #endif
    
    int stringLiteralStart = 0;
    int inStringLiteral = 0;
    

    int fnStartPos = 0;
    
    label_t* labelBuffer = (label_t*)malloc(1024 * sizeof(label_t));
    int lab = 0;

    /* For use with IF/ELSE/WHILE/FOR */
    ifdat_t* ifTree = (ifdat_t*)malloc(16 * sizeof(ifdat_t));
    int ifPtr = -1;
    int globalStatementNumber = 0;
    int lineEnding = 0;
    
   
    #ifdef _BLANG_USE_BUFFER
    while((c = src[bptr++]) != 0){
    #else
    while((c = getc(src)) != EOF){
    #endif
        if(!bjs->macro && !inStringLiteral){
            switch(c){

            /* We dont want to parse macros right now */
            case '#':
                bjs->macro = 1;
            break;

            case '}':
                
                if(ifPtr != -1){
                    /* Handle our if statements here */
                    
                    /* Get rid of any outgoing searches before we check what statement we have */
                    if(ifTree[ifPtr].from == BLANG_SEARCH ){
                        /* Resolve ALL searches */
                        while(ifTree[ifPtr].from == BLANG_SEARCH){
                            global_t zlabel;
                            char* zlabName;
                            /* There is no actual new statement */
                            
                            DBG_RUN(
                                zlabName = B_GenerateStatementJumpName('s', ifTree[ifPtr].statementNumber, ifTree[ifPtr].statementSubNumber);
                                printf("Found no new statement for previous statement, %s, ending\n", zlabName );
                                free(zlabName);
                            );
                            
                            zlabName = B_GenerateStatementJumpName('d', ifTree[ifPtr].statementNumber, 0);
                            zlabel.name = zlabName;
                            zlabel.type = 2;
                            zlabel.addr = ifTree[ifPtr].endpos;
                            

                            bjs->s->globals[bjs->s->globptr++] = zlabel;
                            ifPtr--;
                            
                            DBG_RUN(
                                printf("Ending search\n");
                                printf("IFPTR: %d\n", ifPtr);
                            );
                        }
                    }
                    
                    /* Now check what kind of statement we are coming from */
                    if(ifTree[ifPtr].from == BLANG_FROM_IF_BLOCK){
                        global_t zlabel;
                        char* zlabName;
                        char* doneName;
                        int dnptr = 0;
                        
                        doneName = B_GenerateStatementJumpName('d', ifTree[ifPtr].statementNumber, 0);
                        bjs->finalBuffer[bjs->fbptr++] = 'j';
                        bjs->position++;
                        
                        bjs->finalBuffer[bjs->fbptr++] = 'g';
                        for(dnptr = 0; doneName[dnptr] != 0; dnptr++){
                            bjs->finalBuffer[bjs->fbptr++] = doneName[dnptr];
                        }
                        bjs->finalBuffer[bjs->fbptr++] = 0;
                        bjs->position++;
                        
                        free(doneName);
                        
                        DBG_RUN(
                            printf("We are post if block\n");
                            printf("IFPTR: %d\n", ifPtr);
                        );
                        
                        
                        
                        zlabName = B_GenerateStatementJumpName('s', ifTree[ifPtr].statementNumber, ifTree[ifPtr].statementSubNumber);
                        
                        DBG_RUN(
                            printf("ZLabName is %s\n", zlabName);
                        );
                        
                        zlabel.name = zlabName;
                        zlabel.type = 2;
                        zlabel.addr = bjs->position;

                        bjs->s->globals[bjs->s->globptr++] = zlabel;
                        
                        ifTree[ifPtr].from = BLANG_SEARCH;
                        ifTree[ifPtr].endpos = bjs->position;
                        
                        DBG_RUN(
                            printf("Set position for %s as %d\n", zlabName, bjs->position);
                            printf("[IFLOG] %ds%dn set to BLANG_SEARCH\n", ifTree[ifPtr].statementNumber, ifTree[ifPtr].statementSubNumber);
                            printf("[IFLOG] %ds%dn Endpos set to %d\n", ifTree[ifPtr].statementNumber, ifTree[ifPtr].statementSubNumber, ifTree[ifPtr].endpos);
                        );
                        
                       
                        
                    }
                    else if(ifTree[ifPtr].from == BLANG_FROM_ELSE_BLOCK){
                        /* For else we just set search */
                        
                        ifTree[ifPtr].from = BLANG_SEARCH;
                        ifTree[ifPtr].endpos = bjs->position;
                    }
                    else if(ifTree[ifPtr].from == BLANG_FROM_WHILE_BLOCK){
                        global_t zlabel;
                        char* zlabName;
                        
                        DBG_RUN(
                            printf("We are post if block\n");
                            printf("IFPTR: %d\n", ifPtr);
                        );
                        
                        /* Make the jump infinite */
                        bjs->finalBuffer[bjs->fbptr++] = 'j';
                        bjs->position++;
                        bjs->finalBuffer[bjs->fbptr++] = ifTree[ifPtr].startpos;
                        bjs->position++;
                            
                        zlabName = B_GenerateStatementJumpName('s', ifTree[ifPtr].statementNumber, ifTree[ifPtr].statementSubNumber);
                        zlabel.name = zlabName;
                        zlabel.type = 2;
                        zlabel.addr = bjs->position;

                        bjs->s->globals[bjs->s->globptr++] = zlabel;
                        
                        DBG_RUN(
                            printf("Set position for %s as %d\n", zlabName, bjs->position);
                        );
                        
                        ifPtr--;
                    }
                    
                }
                
                                if(bjs->block >= 1){

                    if(lab > 0){
                        for(; fnStartPos < bjs->fbptr; fnStartPos++){
                            if(bjs->finalBuffer[fnStartPos] == 'g' && bjs->finalBuffer[fnStartPos - 1] == 'j'){
                                char* labname = malloc(64 * sizeof(char));
                                int lnptr = 0;

                                for(fnStartPos++; bjs->finalBuffer[fnStartPos] != 0; fnStartPos++){
                                    labname[lnptr++] = bjs->finalBuffer[fnStartPos];
                                }
                                labname[lnptr] = 0;
                                
                                for(lnptr = 0; lnptr < lab; lnptr++){
                                    if(strcmp(labname, labelBuffer[lnptr].name) == 0){

                                    }
                                }

                                DBG_RUN(printf("Found goto statement with label '%s'\n", labname););
                            }
                        }
                    }
                }
                
                if(bjs->block == 1){    
                    /* Free all of our symbols */
                    for(sym--; sym >= 0; sym--){
                        free(symBuffer[sym]);
                    }
                    sym = 0;

                    /* This might not be needed, will test */
                    memset(symBuffer, 0, 1024 * sizeof(char*));
                    
                    DBG_RUN(
                        printf("Removing symbols buffer\n");
                    );
                    bjs->finalBuffer[bjs->fbptr++] = 'R';
                    bjs->position++;

                    bjs->finalBuffer[bjs->fbptr++] = 'r';
                    bjs->position++;
                }

                bjs->block--;

            break;

            /* There has to be a better way to do this, I'll probably revise it 
             * eventually */
            case '"':{
                inStringLiteral = 1;
                
                stringLiteralStart = x;
            }
            break;

            case ':':{
                global_t label;
                char* labelFinalName;
                
                bjs->lineBuffer[x] = 0;
                

                labelFinalName = malloc(1024 * sizeof(char));
                B_itoa(bjs->fnNumber, labelFinalName);
                
                strcat(labelFinalName, bjs->lineBuffer);
                /* We picked up a label */
                
                DBG_RUN(
                    printf("REGISTERED LABEL: %s\n", labelFinalName);
                );
                
                label.name = labelFinalName;
                label.type = 2;
                label.addr = bjs->position;

                bjs->s->globals[bjs->s->globptr++] = label;

                /*printf("Picked up label '%s' at position %d\n", lineBuffer, position);*/
                
                bjs->lineBuffer = (char*)malloc(2048 * sizeof(char));
                x = 0;
                bjs->lastNL = 1;
            }
            break;
            
            case '{':
                if(bjs->block == 0){

                    /* We have a function definition */
                    if(strhasp(bjs->lineBuffer)){
                        BLANG_WORD_TYPE *positions;
                        int i, j, k, argsdeep;
                        char* nameBuffer, *argbuf;
                        global_t glob;

                        bjs->fnNumber++;
                        
                        bjs->lineBuffer[x] = 0;

                        DBG_RUN(
                            printf("Funcdef\n");
                            printf("FNDEF: %s\n", bjs->lineBuffer);
                            printf("Position %d\n", bjs->position);
                        );
                        
                        fnStartPos = bjs->fbptr;
                        
                        /* We are going to push a global that we have a fndef here.
                         * 'position' should be our memory location */
                        
                        /* Get name, will make this faster eventually */
                        nameBuffer = (char*)malloc(64 * sizeof(char)); 
                        argbuf = (char*)malloc(64 * sizeof(char));
                        
                        positions = malloc(16 * sizeof(BLANG_WORD_TYPE)); 
                        
                        i = 0;
                        j = 0;
                        for(; bjs->lineBuffer[i] != '('; i++){
                            if(bjs->lineBuffer[i] > 32){
                                nameBuffer[j++] = bjs->lineBuffer[i];
                            }
                        }
                        nameBuffer[j] = 0; 
                        
                        j = 0;
                        argsdeep = 0;
                        
                        /* This needs to be here so all of our injected code gets 
                         * saved */
                        glob.addr = bjs->position;
                        
                        for(i++; bjs->lineBuffer[i-1] != ')'; i++){
                            if(bjs->lineBuffer[i] > 32 && bjs->lineBuffer[i] != ',' && bjs->lineBuffer[i] != ')'){
                                argbuf[j++] = bjs->lineBuffer[i];
                            }
                            /* Have we finished collecting this arguement? */
                            else if(bjs->lineBuffer[i] == ',' || bjs->lineBuffer[i] == ')'){
                                argbuf[j] = 0;
                                if(strlen(argbuf) > 0){
                                    DBG_RUN(printf("Collected starting arguement %s\n", argbuf););
                                    symBuffer[sym++] = argbuf;
                                    
                                    /* Set the arg value from the value pushed */
                                    bjs->finalBuffer[bjs->fbptr++] = 'a';
                                    bjs->position++;
                                    
                                    bjs->finalBuffer[bjs->fbptr++] = -3 - argsdeep;
                                    bjs->position++;
                                    
                                    bjs->finalBuffer[bjs->fbptr++] = 'S';
                                    bjs->position++;
                                    
                                    bjs->finalBuffer[bjs->fbptr++] = 0;
                                    bjs->position++;
                                    
                                    for(k = 0; k < argsdeep; k++){
                                        
                                        bjs->finalBuffer[positions[k]] += 1;
                                    }
                                    
                                    positions[argsdeep++] = bjs->fbptr - 1;
                                    
                                    
                                    
                                    argbuf = (char*)malloc(64 * sizeof(char));
                                }
                                else{
                                    memset(argbuf, 0, strlen(argbuf));
                                }

                                j = 0;
                            }
                        }

                        glob.name = nameBuffer;
                        glob.type = 0;
                        
                        glob.function = NULL;

                        bjs->s->globals[bjs->s->globptr++] = glob;

                        free(positions);
                        free(argbuf);

                        bjs->block++;
                        goto end;
                    }
                }
            bjs->block++;
            lineEnding = 1;
            /* fall through */

            case ';':
                bjs->lineBuffer[x] = 0;
                

                if(bjs->macro){
                    bjs->macro = 0;
                }
                else if(bjs->block == 0){
                    char* nameBuffer;
                    int lb, nameptr;
                    global_t glob;
                    
                    nameBuffer = malloc(64 * sizeof(char));
                    
                    DBG_RUN(
                        printf("GLOBAL DEFN [%s]\n", bjs->lineBuffer);
                    );
                    
                    for(lb = 0, nameptr = 0; lb < x && bjs->lineBuffer[lb] != ' ' && bjs->lineBuffer[lb] != '['; lb++){
                        nameBuffer[nameptr++] = bjs->lineBuffer[lb];
                    }
                    nameBuffer[nameptr] = 0;
                    glob.name = nameBuffer;
                    
                    if(bjs->lineBuffer[lb] == '['){
                        /* We have an array */
                        /* TODO: Properly implement global arrays */
                    }
                    else{
                        BLANG_WORD_TYPE value;
                        /* We have a standard definition */
                        DBG_RUN(
                            printf("NAME IS %s\n", nameBuffer);
                        );
                        lb++;
                        value = B_atoi(bjs->lineBuffer + lb);
                        
                        DBG_RUN(
                            printf("VALUE IS %d\n", value);
                        );
                        
                        glob.type = 0;
                        glob.addr = bjs->position;
                        glob.function = NULL;

                        bjs->s->globals[bjs->s->globptr++] = glob;
                        
                        bjs->finalBuffer[bjs->fbptr++] = value;
                        bjs->position++;
                    }

                }
                else{
                    B_PrivJITLine(bjs->s, bjs->lineBuffer, bjs->finalBuffer, symBuffer, bjs->s->globals, &bjs->s->globptr, &bjs->block, &bjs->position, &bjs->fbptr, &sym, &bjs->fnNumber, ifTree, &globalStatementNumber, &ifPtr, lineEnding, 0);
                }
                

    end:
                memset(bjs->lineBuffer, 0, 1024 * sizeof(char));
                
                bjs->lastNL = 1;
                lineEnding = 0;
                
                x = 0;

            break;
            default:
                
                if( c > 32){
                    bjs->lastNL = 0; 
                    bjs->lineBuffer[x++] = c;
                }
                else if(!bjs->lastNL){
                    bjs->lineBuffer[x++] = ' ';
                    bjs->lastNL = 1;
                }
                
            break;
            }
        }
        else if(bjs->macro && c == '\n'){

            bjs->lineBuffer[x] = 0;

            /* Time to parse what macro we have... Fun... */
            DBG_RUN(
                printf("GOT MACRO: %s\n", bjs->lineBuffer);
            );
            
            if(bjs->lineBuffer[0] == '!'){
                /* This is a hashbang, do nothing with it */
            }
            else if(strstart(bjs->lineBuffer, "include")){
                #ifdef BLANG_BUFFER_IS_FILE
                /* We have an include statement */
                FILE *include;
                char *fileNamebuf;
                int iptr, fnb;
                
                fileNamebuf = malloc(512 * sizeof(char));
                fnb = 0;
                
                for(iptr = 0; bjs->lineBuffer[iptr] != '<' && bjs->lineBuffer[iptr] != '"'; iptr++);
                if(bjs->lineBuffer[iptr] == '<'){
                    fileNamebuf[0] = 0;
                    strcat(fileNamebuf, BLANG_INCLUDE_PATH);
                    fnb = strlen(fileNamebuf);
                    fileNamebuf[fnb++] = '/';
                    
                    for(iptr++; bjs->lineBuffer[iptr] != '>'; iptr++){
                        fileNamebuf[fnb++] = bjs->lineBuffer[iptr];
                    }
                    
                }
                else if(bjs->lineBuffer[iptr] == '"'){
                    for(iptr++; bjs->lineBuffer[iptr] != '"'; iptr++){
                        fileNamebuf[fnb++] = bjs->lineBuffer[iptr];
                    }
                }
                fileNamebuf[fnb] = 0;
                
                DBG_RUN(
                    printf("Opening %s\n", fileNamebuf);
                );
                
                include = fopen(fileNamebuf, "r");
                if(include == NULL){
                    printf("Failed to open file %s\n", fileNamebuf);
                }
                
                free(fileNamebuf);
                
                /* We don't to treat our included files as if they are macros, 
                 * so we need to reset this here */
                bjs->macro = 0;
                B_JITStageOne(bjs, include);
                
                fclose(include);
                #else
                    _BLANG_LOAD_INCLUDE
                #endif
            }
            
            bjs->macro = 0;
            memset(bjs->lineBuffer, 0, 1024 * sizeof(char));
            x = 0;
            bjs->lastNL = 1;
        }
        else if(inStringLiteral){
            
            /* We found the second quote mark */
            if(c == '"' && bjs->lineBuffer[x - 1] != '\\' && x - stringLiteralStart > 0){
                char* str, *sNum;
                int strptr;
                int s = 0;

                str = (char*)malloc(64 * sizeof(char));
                sNum = malloc(50 * sizeof(char));

                bjs->lineBuffer[x] = 0;
                /* our string literal start is now the start of the string then to the current position is our whole string */
                DBG_RUN(
                    printf("String literal found '%s'\n", bjs->lineBuffer);
                );

                /* Copy it over and resolve escape sequences */
                for(strptr = 0, x = stringLiteralStart; bjs->lineBuffer[x] != 0; x++){
                    /* I can't tell what to make as escape characters, so I've
                     * implemented both and can change them with macros. */
                    
                    #ifndef _BLANG_ESCAPE_CHAR
                        #ifdef BLANG_OLD_STYLE_ESCAPE
                            #define _BLANG_ESCAPE_CHAR '*'
                        #else
                            #define _BLANG_ESCAPE_CHAR '\\'
                        #endif
                    #endif
                    
                    if(bjs->lineBuffer[x] == _BLANG_ESCAPE_CHAR){
                        switch(bjs->lineBuffer[++x]){
                            case _BLANG_ESCAPE_CHAR:
                                str[strptr++] = _BLANG_ESCAPE_CHAR;
                            break;
                            case 'n':
                                str[strptr++] = '\n';
                            break;
                            case '"':
                                str[strptr++] = '"';
                            break;
                            default:
                                printf("ERROR: Unknown Escape sequence %c%c", _BLANG_ESCAPE_CHAR, bjs->lineBuffer[x]);
                            break;
                        }
                    }
                    else{
                        str[strptr++] = bjs->lineBuffer[x];
                    }
                }
                inStringLiteral = 0;

                x = stringLiteralStart;
                B_itoa(bjs->strLiteralPtr, sNum);

                for(; s < (int)strlen(sNum); s++){
                    bjs->lineBuffer[x++] = sNum[s];
                }
                bjs->lineBuffer[x++] = 'S';
                bjs->lineBuffer[x++] = 'l';
                bjs->lineBuffer[x] = 0;
                
                free(sNum);
                
                str[strptr] = 0;

                bjs->strLiteralBuffer[bjs->strLiteralPtr++] = str;
            }
            else{
                bjs->lineBuffer[x++] = c;
            }
        }
        else{
            bjs->lineBuffer[x++] = c;
        }
    }
    
    DBG_RUN(
        printf("Resolving lost if statements\n");
    );
    
    for(; ifPtr > -1; ifPtr--){
        printf("IFPTR Value: %d\n", ifTree[ifPtr].statementNumber);
        if(ifTree[ifPtr].from == BLANG_SEARCH || ifTree[ifPtr].from == BLANG_FROM_ELSE_BLOCK){
            global_t zlabel;
            char* zlabName;
            /* There is no actual new statement */
            
            DBG_RUN(
                zlabName = B_GenerateStatementJumpName('s', ifTree[ifPtr].statementNumber, ifTree[ifPtr].statementSubNumber);
                printf("Found no new statement for previous statement, %s, ending\n", zlabName );
                free(zlabName);
            );
            
            zlabName = B_GenerateStatementJumpName('d', ifTree[ifPtr].statementNumber, 0);
            zlabel.name = zlabName;
            zlabel.type = 2;
            zlabel.addr = ifTree[ifPtr].endpos;

            bjs->s->globals[bjs->s->globptr++] = zlabel;
            ifPtr--;
            
            DBG_RUN(
                printf("Ending search\n");
                printf("IFPTR: %d\n", ifPtr);
            );
            
            
        }
    }

    free(symBuffer);
    
    free(ifTree);
    
    free(labelBuffer);

    
}

void
B_JIT(B_State* b, BLANG_BUFFER_TYPE src)
{
    /* Setup our inner compiler variables */
    B_JITState* state = (B_JITState*)malloc(sizeof(B_JITState));
    state->s = b;
    
    state->lineBuffer = (char*)malloc(BLANG_LINEBUFFER_SIZE * sizeof(char));
    state->finalBuffer = (BLANG_WORD_TYPE*)malloc(BLANG_MEMORY_SIZE * sizeof(BLANG_WORD_TYPE));
    state->strLiteralBuffer = (char**)malloc(64 * sizeof(char*));
    state->fbptr = 0;
    state->strLiteralPtr = 0;
    
    state->lastNL = 1;
    state->macro = 0;
    state->block = 0;

    /* We need to keep a separate log of position since some code may grow or 
     * shirnk, such as global definitons, we need to keep those out of the 
     * memory locations at compile time. */
    state->position = 0;
    
    state->fnNumber = 0;
    
    /* Enter stage 1 of the compile */
    B_JITStageOne(state, src);
    
    #ifndef _BLANG_CUSTOM_LINK_FUNC
    
    /* Resolve and link */
    B_ResolveStringLiterals(state);  
    B_ResolveGlobals(b, state->finalBuffer, state->fbptr);
    
    #else
    
    /* Something else will handle everything from here */
    _BLANG_CUSTOM_LINK_FUNC(state);
    
    #endif
    
            
    DBG_RUN(
        printf("\n\nPost Compile Memory Dump:\n");
        printf("POS  | HEX  | DEC  | CHAR \n");
        for(state->fbptr = 0; state->fbptr < state->position; state->fbptr++){
            printf("%4d | %4x | %4d | %4c \n", state->fbptr, b->memory[state->fbptr], b->memory[state->fbptr], b->memory[state->fbptr]);
        }
    );

    for(; state->strLiteralPtr > 0; state->strLiteralPtr--){
        free(state->strLiteralBuffer[state->strLiteralPtr - 1]);
    }
    
    /* Eventually move this to ResolveStringLiterals */
    free(state->strLiteralBuffer);
    free(state->lineBuffer);
    free(state);
}

/*
 * Most of the general libblaest stuff
 */

B_State* 
B_CreateState()
{
    B_State* state = (B_State*)malloc(sizeof(B_State));
    state->a = 0;
    state->bp = 0;
    state->sp = 0;
    state->pc = 0;

    state->memory = (BLANG_WORD_TYPE*)malloc( (BLANG_MEMORY_SIZE + BLANG_STACK_SIZE) * sizeof(BLANG_WORD_TYPE));
    state->mmap = (unsigned char*)malloc((BLANG_MEMORY_SIZE / BLANG_MMAP_LIMIT) * sizeof(unsigned char));
    memset(state->mmap, 0, (BLANG_MEMORY_SIZE / BLANG_MMAP_LIMIT) * sizeof(unsigned char));
    
    state->memoryLeases = (memlease_t*)malloc(BLANG_MEMORY_LEASES  * sizeof(memlease_t));
    memset(state->memoryLeases, 0, BLANG_MEMORY_LEASES * sizeof(memlease_t));
    
    state->memlptr = 0;

    state->globals = (global_t*)malloc( 1024 * sizeof(global_t));
    state->globptr = 0;

    /* [Might Be Pending Removal] Stack pretty much only exists as a macro now
     * to assist old code that I don't feel like changing.  Originally stack 
     * was separate to the rest of memory, but now that they are combined it is
     * no longer needed to be separate. */
    state->stack = state->memory + BLANG_MEMORY_SIZE;

    return state;
}

void 
B_FreeState(B_State* s)
{
    free(s->memory);
    free(s->memoryLeases);
    
    for(; s->globptr > 0; s->globptr--){
        if(s->globals[s->globptr - 1].type != 1){
            free((void*)s->globals[s->globptr - 1].name);
        }
        
    }
    
    
    
    free(s->globals);
    free(s->mmap);

    free(s);
}

void 
B_Push(B_State* s, BLANG_WORD_TYPE v)
{
    s->stack[s->sp++] = v;
}
BLANG_WORD_TYPE 
B_Pop(B_State* s)
{
    BLANG_WORD_TYPE ret = 0;
    
    ret = s->stack[--s->sp];
    return ret;
}

BLANG_WORD_TYPE
B_GetArg(B_State* s, BLANG_WORD_TYPE argnum)
{
    BLANG_WORD_TYPE ret;
    
    ret = s->stack[s->bp - argnum - 1];    
    return ret;
}

/* A simple Adler32 type hash function, not perfect I know, but it'll work 
 * for what is needed here.  Written in such a way where it should conform to
 * the size of BLANG_WORD_TYPE */
BLANG_WORD_TYPE 
B_HashFunctionName(const char* name, int size)
{
    
    BLANG_WORD_TYPE s1 = 1;
    BLANG_WORD_TYPE s2 = 0;
    int n = 0;

    for(; n < size; n++){
        s1 = (s1 + name[n]) % ((BLANG_WORD_TYPE)-1);
        s2 = (s2 + s1) % ((BLANG_WORD_TYPE)-1);
    }
    return (s2 << sizeof(BLANG_WORD_TYPE)) | s1;
}

/* Just a small rant, this is a HORRIBLE implementation of exposing functions
 * but I honestly can't come up with a better way.  The function needs to do 
 * all of the following.  Act the same independent of the order the functions
 * are added, allow for many functions to coexist, and make sure the call to
 * said function is no more the size of BLANG_WORD_TYPE.  The only real option
 * I can see for this is to hash the function name and call that, this solution
 * WILL cause problems, more so for platforms which use 8 or 16 bits (I might
 * need to add an override or something for those). */

void 
B_ExposeFunction (B_State *s, const char* name, BLANG_WORD_TYPE (*fn)(B_State* f), BLANG_WORD_TYPE fnNumber)
{
    /* TODO: Check if fnNumber is already in use */
    global_t glob;

    glob.name = name;
    glob.type = 1;
    glob.addr = fnNumber;
    glob.function = fn;

    s->globals[s->globptr++] = glob;
}

/* Basic malloc() implementation,  this will eventually be part of the standard
 * library component of libblaest, however for brevity it is included in the 
 * main JIT right now */
BLANG_WORD_TYPE
B_Malloc(B_State* s)
{
    BLANG_WORD_TYPE actsize = B_GetArg(s, 1);
    BLANG_WORD_TYPE size = 0;
    int x, y, pos, ml;
    memlease_t lease;

    if(actsize % BLANG_MMAP_LIMIT){
        size++;    
    }
    size += actsize / BLANG_MMAP_LIMIT;

    
    x = 0;
    y = 0;

    for(; (BLANG_WORD_TYPE)y != size && x < BLANG_MEMORY_SIZE / BLANG_MMAP_LIMIT; x++){
        DBG_RUN( printf("[MMAP] [%x] - %d\n", x, s->mmap[x]); );
        if(s->mmap[x] == 0){
            y++;
        }
        else if(s->mmap[x] == 1){
            y = 0;
        }
    }

    DBG_RUN (printf("Found malloc position at %d\n", x - y));
    pos = x - y;
    
    
    lease.pos = pos;
    lease.size = size;
    
    for(ml = 0; ml < BLANG_MEMORY_LEASES; ml++){
        if(s->memoryLeases[ml].pos == 0 && s->memoryLeases[ml].size == 0){
            DBG_RUN( printf("[MLEASE] Memory leased at %d (pos: %d, size: %d)\n", ml, pos, size) );
            s->memoryLeases[ml] = lease;
            break;
        }
    }
    
    for(y = x - y; y < x ; y++){
        s->mmap[y] = 1;
        DBG_RUN(printf("[MMAP] SET [%x] - %d\n", y, s->mmap[y]));
        
    }
    return pos * BLANG_MMAP_LIMIT;
}

BLANG_WORD_TYPE
B_Free(B_State* s)
{
    BLANG_WORD_TYPE pos = B_GetArg(s, 1) / BLANG_MMAP_LIMIT;
    BLANG_WORD_TYPE ml, x;
    

    for(ml = 0; ml < BLANG_MEMORY_LEASES; ml++){
        if(s->memoryLeases[ml].pos <= pos && s->memoryLeases[ml].pos + s->memoryLeases[ml].size >= pos){
            DBG_RUN( printf("[MLEASE] Memory freed at %d (pos: %d, size: %d)\n", ml, s->memoryLeases[ml].pos, s->memoryLeases[ml].size) );
            
            for(x = s->memoryLeases[ml].pos; x < s->memoryLeases[ml].pos + s->memoryLeases[ml].size; x++){
                s->mmap[x] = 0;
            }
            memset(&s->memoryLeases[ml], 0, sizeof(memlease_t));
        }
    }
    
    return 0;
}

BLANG_WORD_TYPE B_putnumb(B_State* s){
    BLANG_WORD_TYPE arg = B_GetArg(s, 1);
    printf("Got arg %d\n", arg);
    printf("%d\n", arg);
    return 0;
}

BLANG_WORD_TYPE B_puts(B_State* s){
    BLANG_WORD_TYPE arg = B_GetArg(s, 1);
    char* str = B_toString(s->memory + arg);
    printf("%s\n", str);
    free(str);
    return 0;
}

BLANG_WORD_TYPE 
B_functionLookup(B_State* s, const char* name)
{
    int x;
    
    for(x = 0; s->globals[x].name != NULL; x++){
        if(strcmp(name, s->globals[x].name) == 0){
            DBG_RUN(
                printf("Function looked up to %d\n", s->globals[x].addr);
            );
            return s->globals[x].addr;
        }
    }
    return 0;
}


#ifdef BLANG_USE_FAUXNIX

/*
 * Faux-nix functions to allow for Unix-like functions on platforms that only 
 * support ANSI C
 */

#ifdef _WIN32
/* Since windows implements sockets weird, we need to make these separate */

    typedef int FAUX_SocketType;          /* The type to use for our socket */
    typedef FILE FAUX_FileType;           /* The type to use for our files */

#else
    /* This implementation will work on most every system, but without socket
     * support (since of all things,  those are the least standard) */
     
    typedef int FAUX_SocketType;          /* The type to use for our socket */
    typedef FILE FAUX_FileType;           /* The type to use for our files */

    #define _BLANG_NO_SOCKET 1
          
#endif

typedef struct{
    int type;
    FAUX_FileType* file;
    FAUX_SocketType socket;
} FAUX_FileDescriptor;

FAUX_FileDescriptor* fileDescriptors;

#define FAUX_Nothing    0
#define FAUX_File       1 
#define FAUX_Socket     2

#define FAUX_MaxDescriptors 1024

void FAUX_Init(){
    
    fileDescriptors = malloc(FAUX_MaxDescriptors * sizeof(FAUX_FileDescriptor));
    memset(fileDescriptors, 0, FAUX_MaxDescriptors * sizeof(FAUX_FileDescriptor));
    
    fileDescriptors[0].type = 1;
    fileDescriptors[0].file = stdin;
    fileDescriptors[0].socket = 0;
    
    fileDescriptors[1].type = 1;
    fileDescriptors[1].file = stdout;
    fileDescriptors[1].socket = 0;
    
    fileDescriptors[2].type = 1;
    fileDescriptors[2].file = stderr;
    fileDescriptors[2].socket = 0;
}

/* Just a note, these functions are not designed to be fast at all, they are 
 * just designed to run on as many systems as possible, if the system properly
 * implements unix like functions USE THEM, do NOT use faux-nix unless your 
 * system does not implement any UNIX or POSIX compatability */
 
/* Just an aisde, we are forced to do this with windows, not because POSIX 
 * compat. doesn't exist, but because it isn't good (or standard across many 
 * versions), as well Winsock doesn't use the POSIX compatability so we would
 * need to implement this anyway.  Maybe in the future we will piggy back off
 * Windows' implementation, but for now its ANSI C */
int FAUX_Read(int fd, char* buf, int size){
    FAUX_FileDescriptor filedesc = fileDescriptors[fd];
    
    if(filedesc.type == 0){
        return -1;
    }
    else if(filedesc.type == FAUX_File){
        /* We need to treat stdin differently */
        if(fd == 0){
            int read = 0;
            
            for(; read < size; read++){
                buf[read] = fgetc(stdin);
                if(buf[read] == '\n'){
                    break;
                }
            }
             
            read++;
            return read;
        }
        else{
            return fread(buf, 1, size, filedesc.file);
        }
    }
    return 0;
}

int FAUX_Write(int fd, char* buf, int size){
    FAUX_FileDescriptor filedesc = fileDescriptors[fd];
    
    if(filedesc.type == 0){
        return -1;
    }
    else if(filedesc.type == 1){
        return fwrite(buf, 1, size, filedesc.file);
    }
    return 0;
}

int FAUX_Open(char* name, char* flags){
    int index;
    
    for(index = 0; index < (int)(FAUX_MaxDescriptors * sizeof(FAUX_FileDescriptor)); index++){
        if(fileDescriptors[index].type == 0){
            fileDescriptors[index].type = FAUX_File;
            fileDescriptors[index].file = fopen(name, flags);
            return index;
        }
    }
    return 0;
}

int FAUX_Close(int fd){
        
    fileDescriptors[fd].type = FAUX_Nothing;
    fclose(fileDescriptors[fd].file);
    return 0;
}

#endif

/*
 * Blaest Systemcalls
 */


/* Notes on using real system calls.  Real system calls should take less cycles,
 * however they are not really needed since unistd.h should call them straight 
 * away, the difference should only be a few MS at the most.  Please use them
 * if you can, but otherwise don't bother.  Currently only implemented in Linux.
 */
#define _BLANG_USE_SYSCALLS 1

BLANG_WORD_TYPE
B_sysREAD(B_State *s)
{
    BLANG_WORD_TYPE ret, fd, size, *buff, mempos;
    char *tmpbuf;
    size = B_GetArg(s, 1);
    
    mempos = B_GetArg(s, 2);
    buff = s->memory + mempos;
    
    fd     = B_GetArg(s, 3);
    tmpbuf = malloc(size * sizeof(char));
    
    #ifdef BLANG_USE_FAUXNIX
    
        ret = (BLANG_WORD_TYPE)FAUX_Read(fd, tmpbuf, size);
    
    #elif defined(__linux) && defined(__LP64__) && defined(_BLANG_USE_SYSCALLS)
        __asm(
            "syscall"
            : "=a" (ret)
            : "a"(0), "D"(fd), "S"(tmpbuf), "d"(size)
            : "cc", "rcx", "r11", "memory");
    #else
        ret = (BLANG_WORD_TYPE)read(fd, tmpbuf, size);
    #endif
    
    B_stringToWords(tmpbuf, buff, size);
    
    free(tmpbuf);
    return ret;
}

BLANG_WORD_TYPE
B_sysWRITE(B_State *s){
    BLANG_WORD_TYPE ret, fd, size, *buff, mempos;
    char *tmpbuf;
    size = B_GetArg(s, 1);
    
    mempos = B_GetArg(s, 2);
    buff = s->memory + mempos;
    
    fd     = B_GetArg(s, 3);
    
    tmpbuf = (char*)malloc(size + 1);
    
    B_copyString(buff, tmpbuf, size);
    
    
    #ifdef BLANG_USE_FAUXNIX
    
        ret = (BLANG_WORD_TYPE)FAUX_Write(fd, tmpbuf, size);
        
    #elif defined(__linux) && defined(__LP64__) && defined(_BLANG_USE_SYSCALLS)
        __asm(
            "syscall"
            : "=a" (ret)
            : "a"(1), "D"(fd), "S"(tmpbuf), "d"(size)
            : "cc", "rcx", "r11", "memory");
    #else
        ret = (BLANG_WORD_TYPE)write(fd, tmpbuf, size);
    #endif
    
    free(tmpbuf);
    return ret;
}

BLANG_WORD_TYPE
B_sysOPEN(B_State *s){
    BLANG_WORD_TYPE ret, *flagsBuff, *buff, mempos, flags;
    char *fname;
    
    #ifdef BLANG_USE_FAUXNIX
    char *flagsBuffer;
    #endif
    
    flagsBuff = B_GetArg(s, 1) + s->memory;
    
    mempos = B_GetArg(s, 2);
    buff = s->memory + mempos;

    fname = B_toString(buff);
    
    #ifdef BLANG_USE_FAUXNIX
        flags = 0;
        flags = flags + flags;
        
        flagsBuffer = B_toString(flagsBuff);
        
        ret = (BLANG_WORD_TYPE)FAUX_Open(fname, flagsBuffer);
        
        free(flagsBuffer);
    #else
    
        /* Parse out the mode we have to open the file with.  Since we use the 
         * C standard modes for portability, we make sure to translate them into the
         * proper UNIX-y modes */
        if(flagsBuff[1] == '+'){
            switch(flagsBuff[0]){
                case 'r':
                    flags = O_RDWR;
                break;
                
                case 'w':
                    flags = O_RDWR | O_CREAT | O_TRUNC;
                break;
                
                case 'a':
                    flags = O_RDWR | O_CREAT | O_APPEND;
                break;
                
                default:
                    flags = 0;
                break;
            }
        }
        else{
            switch(flagsBuff[0]){
                case 'r':
                    flags = O_RDONLY;
                break;
                
                case 'w':
                    flags = O_WRONLY | O_CREAT | O_TRUNC;
                break;
                
                case 'a':
                    flags = O_WRONLY | O_CREAT | O_APPEND;
                break;
                
                default:
                    flags = 0;
                break;
            }
        }

        ret = open(fname, flags);
    
    #endif
    free(fname);
    return ret;
}

BLANG_WORD_TYPE
B_sysCLOSE(B_State *s){
    BLANG_WORD_TYPE ret, fd;
    fd = B_GetArg(s, 1);
    
    #ifdef BLANG_USE_FAUXNIX
        ret = FAUX_Close(fd);
    #else
        ret = close(fd);
    #endif
    return ret;
}

#ifdef _BLANG_USE_BUFFER

BLANG_WORD_TYPE B_CompileAndRun
(char* src)
{
    BLANG_WORD_TYPE x;
    B_State* b;
    
    b = B_CreateState();
        
    /* Expose our system calls to Blaest, all other functions will come from
     * our standard library. */
    B_ExposeFunction(b, "read",  B_sysREAD, 0);
    B_ExposeFunction(b, "write",  B_sysWRITE, 1);
    B_ExposeFunction(b, "open",  B_sysOPEN, 2);
    B_ExposeFunction(b, "close",  B_sysCLOSE, 3);
    
    B_ExposeFunction(b, "malloc",  B_Malloc, 9);
    B_ExposeFunction(b, "free",  B_Free, 10);

    B_JIT(b, src);
    
    /* Actually run the program */
    x = B_Run(b, B_functionLookup(b, "main"));

    B_FreeState(b);
    return x;
}

#endif

#ifndef _BLANG_BUILD_AS_LIBRARY

int main(int argc, char* argv[]){
    FILE* src;
    BLANG_WORD_TYPE x;
    B_State* b;
        
    #ifdef BLANG_USE_FAUXNIX
        FAUX_Init();
    #endif

    if(argc != 2){
        /* We are in interactive mode here, but thats not implemented */
        printf("No\n");
        return 1;
    }
    else{
        
        src = fopen(argv[1], "r");
        if(src == NULL){
            printf("Failed to open file %s\n", argv[1]);
            return 1;
        }

        b = B_CreateState();
        
        /* Expose our system calls to Blaest, all other functions will come from
         * our standard library. */
        B_ExposeFunction(b, "read",  B_sysREAD, 0);
        B_ExposeFunction(b, "write",  B_sysWRITE, 1);
        B_ExposeFunction(b, "open",  B_sysOPEN, 2);
        B_ExposeFunction(b, "close",  B_sysCLOSE, 3);
        
        B_ExposeFunction(b, "malloc",  B_Malloc, 9);
        B_ExposeFunction(b, "free",  B_Free, 10);
        
        B_JIT(b, src);
        fclose(src);
        
        DBG_RUN(
            printf("Post Compile Global Dump:\n");
            printf("%3s | %25s | %8s | %8s\n", "PTR", "NAME", "TYPE", "VALUE");
            for(x = 0; x < b->globptr; x++){
                if(b->globals[x].type == 0){
                    printf("%3d | %25s | %8s | %8d\n", x, b->globals[x].name, "GLOBAL", b->globals[x].addr);
                }
                else if(b->globals[x].type == 1){
                    printf("%3d | %25s | %8s | %8d\n", x, b->globals[x].name, "SYSCALL", b->globals[x].addr);
                }
                else if(b->globals[x].type == 2){
                    printf("%3d | %25s | %8s | %8d\n", x, b->globals[x].name, "LABEL", b->globals[x].addr);
                }
                else{
                    printf("%3d | %25s | %8d | %8d\n", x, b->globals[x].name, b->globals[x].type, b->globals[x].addr);
                }
                
            }
        );


        /* Actually run the program */
        x = B_Run(b, B_functionLookup(b, "main"));

        B_FreeState(b);
        return x;
    }
}
#endif
