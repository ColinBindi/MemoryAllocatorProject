# Project 3: Memory Allocator

See: https://www.cs.usfca.edu/~mmalensek/cs326/assignments/project-3.html 

Author: Colin Bindi

Project Information:

In this project, we develop a custom memory allocator. For this allocator, we use mmap and we allocate entire regions of memory at a time. The size of each region is be a multiple of the system page size. We also use the free space management algorithms we discussed in class to split up and reuse empty regions. These algorithms are first fit, best fit, and worst fit. We add some extra features to our memory allocator such as a name for each allocation and scripting.

To learn more about mmap use:

man mmap

To compile and use the allocator:

```bash
make
LD_PRELOAD=$(pwd)/allocator.so ls /
```

(in this example, the command `ls /` is run with the custom memory allocator instead of the default).

## Testing

To execute the test cases, use `make test`. To pull in updated test cases, run `make testupdate`. You can also run a specific test case instead of all of them:

```
# Run all test cases:
make test

# Run a specific test case:
make test run=4

# Run a few specific test cases (4, 8, and 12 in this case):
make test run='4 8 12'

[csbindi@csbindi-vm P3-ColinBindi]$ make test
Building test programs
make[1]: Entering directory '/home/csbindi/P3-ColinBindi/tests/progs'
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '/home/csbindi/P3-ColinBindi/tests/progs'
Running Tests: (v17)
 * 01 Basic Allocation     [1 pts]  [  OK  ]
 * 02 Block Splitting      [1 pts]  [  OK  ]
 * 03 Basic First Fit      [1 pts]  [  OK  ]
 * 04 Basic Best Fit       [1 pts]  [  OK  ]
 * 05 Basic Worst Fit      [1 pts]  [  OK  ]
 * 06 First Fit            [1 pts]  [  OK  ]
 * 07 Best Fit             [1 pts]  [  OK  ]
 * 08 Worst Fit            [1 pts]  [  OK  ]
 * 10 scribbling           [1 pts]  [ FAIL ]
 * 12 Static Analysis      [1 pts]  [  OK  ]
 * 13 Documentation        [1 pts]  [  OK  ]
 * 99 purple lace point    [1 pts]  [  OK  ]
Execution complete. [11/12 pts] (91.7%)

```
