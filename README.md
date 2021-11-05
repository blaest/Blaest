# Blæst

### What?

Blæst is a Just in Time (and soon to be Ahead of Time) compiled programming language, based on the syntax of the B Programming Language from Bell Labs.  Since the B Programming Language was not meant to run on modern computers, as it addresses words and not bytes, Blæst was created to feature a Virtual Machine component similar to Java's JVM.  As well as this, Blæst also features a blazing fast Just-In-Time compiler, rivaling even the likes of Lua.  Plus to top it off, Blæst runs on pretty much every system ever, if it runs C and was made after 1989, chances are it can run Blæst.

### Some things to note

Blæst is currently in very early development.  Some features you'd expect are not yet implemented, that mostly has been down to my schedule.  Blæst is still being actively developed, with a few features almost done and nearly ready for release.  Just don't expect everything to work just now.  Also too, Blæst right now is not reflective of Blæst in a year, so keep that in mind.

As well as this, Blæst is planned to be two parts, the virtual machine, as well as libblaest.  The virtual machine will function much like Lua, where you can run files independently.  libblaest will function like liblua, where you can embed Blæst into C programs or likewise.  libblaest is currently not finished.

### Supported

Currently Blæst supports

* Most math (apart from order of operations and bitwise)

* Functions; both user defined, libraries, and syscalls

* Defining and setting local variables

* Defining and setting global variables 

* Labels/gotos

* If/While statements with == or != to test conditions

* Strings and some string escapes

* Macros (just include for now)

* Test conditions <, >, <=, >=

* Else If/Else statements

### Planned

* Foreign Function Interface

* libblaest

(These lists will probably not be updated, but so what, they're pretty cool to look at)

### Building

Anyway, if any of this intrigued you, check out [the build instructions](doc/BUILDING.md).
