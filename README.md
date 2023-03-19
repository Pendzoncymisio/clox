# Overcommented CLox compiler

CLox is a Lox compiler written in C with guidance from [Crafting Interpreters](https://craftinginterpreters.com/) by Bob Nystrom.

I've added rather extensive amount of comments, in order to be able to easily remind myself (and hopefully others) what part of the code was doing. Nevertheless comments does not explain whole picture of solution nor some big concepts. For that you will need to read a book :P

## Build

Simply run provided build script, with additional gcc -g flag for debug build.

You can run Lox script by providing it's location, or run REPL session when tun without any arguments.

```bash
$ ./build.sh [-g]
$ ./clox [script]
```