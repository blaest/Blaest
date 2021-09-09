# Building Blæst

Blæst support many different build systems.  Basically, if it can compile ANSI C 89, Blæst supports it.  However, here are some of the recommended ways to build Blæst for your platform.

# Various - GCC

If your platform supports GCC and has a working copy of GNU Make (this may be called gmake on your system, or just make), you should be able to run the Makefile at the root of the project.  Note, this does not work on Plan 9 even with the ANSI POSIX Environment.

To build simply type:

```
make
```

To install, type:

```
make install
```

If you are on Windows under either MinGW32 or Cygwin, I do not recommend doing the installing step, as it assumes a Unix environment.  Instead just copy blaest.exe to a place for safe keeping.

**NOTE: Clang should also work with this makefile, just change CC to reflect that.**

# Windows - Visual Studio

Simply open the "Visual Studio Command Line" (should be an option in the start menu), then navigate to the location of the root of Blæst .

From there type:
```
cl src/blaest.c
```

**NOTE: THIS IS UNTESTED, I'M GOING OFF OF GENERAL KNOWLEDGE.  BLÆST HAS BEEN TESTED IN VISUAL STUDIO 6 AND VISUAL STUDIO 2010, YOUR MILEAGE MAY VARY**

# Plan 9 (9Front) - PCC

Simply open the terminal and type:

```
ape/psh
```

to enter the POSIX shell.  From there, navigate to the root of Blæst, then to compile run:

```
c89 src/blaest.c
```

