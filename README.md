# Maloader [![Build Status](https://travis-ci.org/shinh/maloader.svg)](https://travis-ci.org/shinh/maloader)
This is a userland Mach-O loader for linux.

## Installation

```bash
$ make release
```
## Usage

```bash
$ ./ld-mac mac_binary [options...]
```
You need OpenCFLite (http://sourceforge.net/projects/opencflite/)
installed if you want to run some programs such as dsymutil.
opencflite-476.17.2 is recommended.

## How to use compiler toolchains of Xcode

Recent Xcode toolchains are built by clang/libc++. This means some C++
programs do not work with ld-mac linked against libstdc++. To build
ld-mac with clang/libc++, run

```bash
$ make clean
$ make USE_LIBCXX=1
```
You need compiler toolchain binaries on your Linux. unpack_xcode.sh in
this repository helps you to set up them if you have a dmg package of
Xcode. This script was checked with Xcode 4.3.3, 4.4.1, 4.5.2, 4.6.2,
and 5.0.1. It would work even on other Xcode releases, but you may
need to modify the script by yourself. How things are stored in dmg
packages heavily depend on the version of Xcode. You can use this like

```bash
$ ./unpack_xcode.sh ~/Downloads/xcode_5.0.1_command_line_tools*.dmg
```

This will create xcode_5.0.1_command_line_tools*/root. We will call
this directory as $ROOT.

```bash
$ ./ld-mac $ROOT/usr/bin/clang --sysroot=$ROOT -c mach/hello.c
$ file hello.o
hello.o: Mach-O 64-bit x86_64 object
```
To link binaries on Linux, you also need necessary dylibs in
root/usr/lib. For example, simple hello world program requires
/usr/lib/libSystem.dylib and /usr/lib/system. Do something like

```bash
(mac)$ tar -cvzf sys.tgz /usr/lib/libSystem* /usr/lib/system
(mac)$ scp sys.tgz $USER@linux:/tmp
(linux)$ cd $ROOT && tar -xvzf /tmp/sys.tgz
```
Also note that it seems ld does not like the version number of
Xcode. So you need to move xcode_5.0.1_command_line_tools*/root to
somewhere else. For example:

```bash
$ ln -sf xcode_5.0.1_command_line_tools_10.9_20131022/root
$ ./ld-mac $ROOT/usr/bin/clang --sysroot=$ROOT -g mach/hello.c
$ ./ld-mac ./a.out
Hello, 64bit world!
```
## How to use compiler toolchains of Xcode 3.2.6

Get xcode_3.2.6_and_ios_sdk_4.3__final.dmg (or another xcode package).

```bash
$ git clone git@github.com:shinh/maloader.git
$ ./maloader/unpack_xcode.sh xcode_3.2.6_and_ios_sdk_4.3__final.dmg
$ sudo cp -a xcode_3.2.6_and_ios_sdk_4.3__final/root /usr/i686-apple-darwin10
$ cd maloader
$ make release
$ ./ld-mac /usr/i686-apple-darwin10/usr/bin/gcc mach/hello.c
$ ./ld-mac a.out
```

## How to run Mach-O binaries using binfmt_misc
```bash
$ ./binfmt_misc.sh
$ /usr/i686-apple-darwin10/usr/bin/gcc mach/hello.c
$ ./a.out
```
To remove the entries, run the following command:

```bash
$ ./binfmt_misc.sh stop
```

## How to try 32bit support

```bash
$ make clean
$ make all BITS=32
```
If you see permission errors like:

```bash
ld-mac: ./mach/hello.c.bin mmap(file) failed: Operation not permitted
```
you should run the following command to allow users to mmap files to
addresses less than 0x10000.

```bash
$ sudo sh -c 'echo 4096 > /proc/sys/vm/mmap_min_addr'
```
Or, running ld-mac as a super user would also work.

## How to run both 64bit Mach-O and 32bit Mach-O binaries
```bash
$ make both
$ ./binfmt_misc.sh start `pwd`/ld-mac.sh
$ /usr/i686-apple-darwin10/usr/bin/gcc -arch i386 mach/hello.c -o hello32
$ /usr/i686-apple-darwin10/usr/bin/gcc -arch x86_64 mach/hello.c -o hello64
$ /usr/i686-apple-darwin10/usr/bin/gcc -arch i386 -arch x86_64 mach/hello.c -o hello
$ ./hello32
Hello, 32bit world!
$ ./hello64
Hello, 64bit world!
$ ./hello
Hello, 64bit world!
$ LD_MAC_BITS=32 ./hello
Hello, 32bit world!
```

## Which programs should work

OK

- gcc-4.2 (link with -g requires OpenCFLite)
- otool
- nm
- dyldinfo
- dwarfdump
- strip
- size
- dsymutil (need OpenCFLite)
- cpp-4.2
- clang

not OK

- llvm-gcc
- gnumake and bsdmake
- lex and flex
- ar
- m4
- gdb
- libtool
- nasm and ndisasm (i386)
- mpicc, mpicxx, and mpic++

## Notice

- Running all Mac binaries isn't my goal. Only command line tools such
  as compiler tool chain can be executed by this loader.
- A slide about this: http://shinh.skr.jp/slide/ldmac/000.html

## TODO

- read dwarf for better backtracing
- make llvm-gcc work
- improve 32bit support
- handle dwarf and C++ exception

## License

Simplified BSD License or GPLv3.

Note that all files in "include" directory and some files in "libmac"
were copied from Apple's Libc-594.9.1.
http://www.opensource.apple.com/release/mac-os-x-1064/

See http://www.gnu.org/licenses/gpl-3.0.txt for GPLv3.
