#!/bin/bash
 
while getopts "g" opt; do
  case $opt in
    g)
      gcc -g -o clox src/main.c src/chunk.c src/memory.c src/debug.c src/value.c src/vm.c src/compiler.c src/scanner.c src/object.c src/table.c
      echo "CLox debug build successful"
      exit
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      ;;
  esac
done

gcc -o clox src/main.c src/chunk.c src/memory.c src/debug.c src/value.c src/vm.c src/compiler.c src/scanner.c src/object.c src/table.c
echo "CLox build successful"