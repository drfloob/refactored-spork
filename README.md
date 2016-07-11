# Insight Coding Challenge - Venmo Payments

This challenge asks you to report the median connection degree in a social network graph of Venmo payment users, filtered through a 60 second sliding window. Read the full problem description [here](https://github.com/drfloob/refactored-spork/blob/master/README.challenge.md).


# Linux Build Dependencies

This project was built on ArchLinux (3.18.1-1, x86_64) with the following dependencies, using the currently available ArchLinux packages as of 2016.07.07:

* CMake v3.5.2
* GNU Make v4.2.1
* GNU G++ v6.1.1 20160602
* Boost v1.60.0 (MultiIndex, DateTime)
* JsonCpp v1.7.2
* C++11 (required by JsonCpp v1.y.z)

These are the only versions that have been tested, but I expect any relatively recent versions should work fine.


# Mac Build Dependencies

If you have a modern mac with XCODE and homebrew, all you should need is:

```
brew install cmake
brew install boost
```


# Running Tests

```
rm -r build/
./run.sh # to pre-build the binary for faster test execution
cd insight_testsuite
./run_tests.sh
tail -1 results.txt
```

Note: These tests run fast. Don't assume (like I did) that the script exited without doing anything, double check the output and results!



# Performance

On my Acer Chromebook, the "MedianDegreeEngine" implementation processes the 1792 lines from the large data-gen dataset in about 0.1 seconds. The "Naive" implementation takes about 1.3 seconds to do the same thing. The challenge instructions suggest we aim for sub-minute processing time on a minute of data; this works about 3 orders of magnitude faster than that on the densest dataset provided.



# Tweaks

I modified the `insight_testsuite/run_tests.sh` script to copy the `build/` folder as well as the rest. This means the binary doesn't need to be rebuilt for every test. Building the binary takes about 25 seconds on my machine, which is nontrivial compared to the processing time (0.1 seconds for a "large" dataset).



# Design Choices

I chose C++ because it felt like the right tool for the job: performant, flexible, and expressive with *serious* algorithm libraries.

I wanted to solve the problem without using a graph-specific library, to prove some amount of algorithmic design skill, so I fleshed out the overall algorithm, and then needed to choose indexed/sorted data structures with strong time guarantees. I found Indexable Skiplists, which promise an average time complexity of O(log n) for median-lookup, find by key, insert, and delete operations, but I didn't find a great implementation in C++, and decided against writing my own due to personal/real-life time constraints. Boost::MultiIndex is able to provide much the same behavior and performance at the cost of more storage space, and that's a fine tradeoff even for my memory-constrained chromebook on the largest test sets.

I also considered a dual-heap implementation, where finding the median would be a constant-time operation. Since I probably would have used Boost::MultiIndex for that implementation anyway, I wanted to see if a function-based rank index would be practically performant enough before I added the additional complexity of a dual-heap implementation. The rank index is absolutely fast enough for my purposes.

To validate my results on generated test sets, I also implemented a much simpler naive solution that runs in significantly more time, to compare output. It maintains only the 60 second sliding window of payment records, and rebuilds the social network graph for every new payment recieved. It's about 35% less code, and easier to understand, giving some greater measure of certainty to fast implementation's results.

The fast implementation maintains both the 60 second sliding window of payments, and the current social network graph state. Whenever a valid payment is recieved, it is considered for inclusion in the 60 second sliding window, possibly triggering a purge event of old payments, and the network graph is updated to reflect the new and expired connections, before the new median connectivity degree is found and reported.


# Notes

I did not get a solid grasp on Boost's time input facet. It has some odd behaviors that may or may not be bugs; they've been reported as bugs, but I'm not fully convinced they are behaving incorrectly since I haven't read all their documentation or implementation yet. For example, if you modify a timestamp on the input, setting the day to "08888888", no errors will get thrown, and your date will be parsed some days into the future. But setting the day to "088" or "0888" will cause `bad_day` errors to be thrown. YMMV.