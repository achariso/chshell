# chshell v0.4
Linux Shell Simulator written in pure C99.\
This project is the semester project for the course "Υ1501 - Operating Systems" at E.C.E. Dept., Aristotle University of Thessaloniki, Greece.

## Compilation & Execution ( v.0.4 )
### Simply run ```make all``` to compile
From v.0.4 the procect uses ```MAKE``` to compile code and resolve dependencies. The executable is placed in the ```bin``` directory after successful compilation.

### Run
To run the shell, simply navigate to ```bin``` directory and run the executable:
```
 cd ./bin
 ./chshell
```

## Compilation & Execution ( v.0.3 )
### 1) Install makeinfo if not installed
```termcap``` is a dependancy of this shell, and in order for it to be compiled ```textinfo``` must be present in the system.
This can be installed with the following command:
```
sudo apt-get install texinfo
```
### 2) Compile code
Compilation chain is based on ```cmake```. If ```cmake``` is not present in your system, you may install it via the following command:
```
sudo apt-get install cmake3
``` 
Else, to compile the code, run the following command(s):
```
cd ./cmake-build-debug && \
cmake -DCMAKE_BUILD_TYPE=Debug -B./cmake-build-debug -G "CodeBlocks - Unix Makefiles" .. && \
cd .. && cmake --build ./cmake-build-debug --target chshell -- -j 4 && \
mv ./cmake-build-debug/chshell .
```
### 3) Run 
To run the shell, simply navigate to cmake's output directory and run the executable.
If you compiled using the above command, then to run, use the following:
```
 ./chshell
```

## Features
The shell is currently under development and thus features are constantly being added or expanded.
The main features of the chshell, include:
 - feature #1
 - feature #2
 - feature #3
