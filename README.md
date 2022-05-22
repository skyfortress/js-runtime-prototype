## Overview

Prototype of JavaScript runtime heavliy enspired by [Node.js][], based on V8 and LibUV. Project was made only for educational purposes. Could be used as en example of V8 ingtegration into any C++ project.

## Implemented features

 * Console.log()
 * Promises using internal V8 queue
 * fs.existsSync()
 * setTimeout()
 * setInterval()
 * require()
 * part of basic HTTP server implementation

## Build instruction
```bash
$ g++ -I /usr/local/Cellar/v8/8.7.220.25/libexec -I /usr/local/Cellar/v8/8.7.220.25/libexec/include -luv -lv8 -lv8_libbase -lv8_libplatform -lv8 -licuuc -licui18n -stdlib=libc++ -L /usr/local/Cellar/v8/8.7.220.25/libexec/ ${file} -o ${fileDirname}/${fileBasenameNoExtension} -pthread -DV8_COMPRESS_POINTERS -std=c++14'
```
There is vscode build task for convenience