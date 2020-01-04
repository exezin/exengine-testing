# exengine, a C 3D game engine.

[Discuss](https://webchat.freenode.net/##oodnet) | [Website](https://exez.in/exengine) | [TODO](https://github.com/exezin/exengine/projects/1)

### What exactly is exengine?
exengine is a 3d engine that takes a slightly different approach than other libraries and engines do, in that it's a code-base you include directly into your own.  Rather than using it as a static/shared library.

This approach allows easy and direct access to the engine back-end should you want to make modifications to suit your specific needs, so think of it as more of a template.

Assuming you don't want to set up your own build system (I can't blame you), you can clone the repo and use the existing build system and file structure as a starting template.  It compiles on Linux, BSD, and Windows. It *might* compile and run on MacOS, but this isn't [directly supported any more.](https://news.ycombinator.com/item?id=17231593)

**The contents of main.c/game.c are only supplied as examples to showcase how one might use the engine.**

### What are the features?
* *Simple* and small
* A straight-forward C99 codebase
* A deferred[*](https://github.com/exezin/exengine/commit/c28d1a8e2f3d99ef4a87253ae3166f2f67e33fbc) and forward renderer
* Various light casters
* Smooth shadow mapping
* Normal & specular mapping
* Half-kernel SSAO (deferred only)
* IQM model loading
* 3D model animation
* Scene manager
* Instancing
* Polygon soup collision detection
* Smooth collision response
* Various cameras
* [More to come..](https://github.com/exezin/exengine/projects/1)

### (External) Depends
* A C99 compiler, preferably gcc. Clang and others should also work
* OpenGL 3.3+
* SDL2

### Credits
* [SDL2](https://www.libsdl.org/index.php)
* [MojoAL](https://hg.icculus.org/icculus/mojoAL/)
* [PhysFS](https://icculus.org/physfs/)
* [STB](https://github.com/nothings/stb)
* [Linmath](https://github.com/datenwolf/linmath.h)
* [IQM](http://sauerbraten.org/iqm/)
* [GLAD](https://github.com/Dav1dde/glad)

### Getting Started
#### Documentation
The current method of documentation is code comments, every engine header has extensive documentation as to its purpose and how one might use it.  Better documentation will come in the near future.

#### Using & Compiling

Simply clone the repository and install the required libraries and compilers listed below for the OS you are using.

**This only applies to those wanting to use the current build system and game code as a starting template, and not those wanting to include exengine into an existing project or build system.**


----
### Linux

````
sudo apt-get update
sudo apt-get install build-essential libsdl2-dev
cd src && make
````

The resulting binary will be in src/build/

----
### Windows

Download and install [Cygwin](https://www.cygwin.com/) with the following packages.  *All required libraries for Windows are included in the codebase.*

Alternatively switch to category view and set ['Devel' to install.](https://i.stack.imgur.com/2uzkB.jpg)  *This method takes up 1-2GB of space*

````
mingw64-x86_64-gcc
make
zip
````

**You can also install the [Linux subsystem for Windows 10](https://docs.microsoft.com/en-us/windows/wsl/install-win10)**, after which you just need to run the following.

````
sudo apt update
sudo apt install mingw-w64 make zip
cd /mnt/c/Users/MyUsername/Desktop/exengine/src
````

After this compiling is the same as Linux, except for the addition of setting the OS variable.

````
cd C:\exengine\src (or wherever you've put this)
OS=Windows_NT make
````

the resulting .exe will be in src/build/

----
### OpenBSD
Make sure you have the correct dependicies installed, the equivalent of linux using pkg_add.
For OpenBSD specific you will need the 'gmake' and 'gcc' packages.
The gcc package will install a more modern compiler as egcc in your path.

after this the compilation is almost the same as in linux, this time specificly using gmake (stands for GNU make).

````
cd src && gmake
````

----
### Stand-alone (*recommended*)

Alternatively you can drop the exengine/ directory right into your own project and use it with your own build system.  This method obviously takes more work and time.

----
### Gallery
![scrot](http://i.imgur.com/4NGlapU.png)
![scrot](https://i.imgur.com/vTKB3T8.png)
![scrot](http://i.imgur.com/H1pMBXI.png)
![scrot](https://i.imgur.com/n6FcPau.png)
![scrot](https://i.imgur.com/t7CcuiK.png)
![scrot](https://i.imgur.com/qesG2md.png)