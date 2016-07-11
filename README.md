# Dependencies

This project was built on ArchLinux (3.18.1-1, x86_64) with the following dependencies, using the currently available ArchLinux packages as of 2016.07.07:

CMake v3.5.2
GNU Make v4.2.1
GNU G++ v6.1.1 20160602
Boost v1.60.0 (MultiIndex, DateTime)
JsonCpp v1.7.2
C++11 (required by JsonCpp v1.y.z)

These are the only versions that have been tested, but I expect any relatively recent versions should work fine.


# Performance

On my Acer Chromebook, the "MedianDegreeEngine" implementation processes the 1792 lines from the data-gen dataset in about 0.1 seconds. The "Naive" implementation takes about 1.3 seconds to do the same.


# Tweaks

I modified the `insight_testsuite/run_tests.sh` script to copy the `build/` folder as well as the rest. This means the binary doesn't need to be rebuilt for every test. Building the binary takes about 25 seconds on my machine, which is nontrivial compared to the processing time (0.1 seconds for a "large" dataset).