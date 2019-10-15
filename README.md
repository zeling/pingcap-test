# Find Top 100 urls with limited memory

## Build and test instructions

### Prerequisites: 
Linux machine is assumed. Please make sure you have cmake >= 3.13 installed. To run the unit tests, you will have to 
install [Catch2](https://github.com/catchorg/Catch2) and ctest (should be bundled with cmake on most distributions).

### Build:
```
mkdir build
cd build
cmake ..
make
```

### Run unit tests:
Either run `ctest` or `./test_main` in the `build/` folder.

### Run the program:
`./top100 [-l hard_limit] [-w water_mark] [-s shards] [-t top_k] inputfile`
Both `hard_limit` and `water_mark` are in bytes, the former one is enforced by the OS, the program might abort if the memory requirement cannot be met.
The later one is more flexible, it is only to tell the program to cooperatively flush memory to the disk when the `water_mark` is triggered. It is required
that $water_mark < hard_limit$. `water_mark` has default of `0.9G` while `hard_limit` has default of `1G`. `shard` is the number of shards, defaults to `std::thread::hardware_concurrency()`; `top_k` is the top k URLs the user is interested in (defaults to 100). 

## Design

### Overview
1. Read the file into the file, keep the count of the URL so far, whenever the memory usage is about to exceed the memory limit, flush the memory tables onto the disk. The disk storage is like sorted string table. It is helpful because we can later merge different sst's back with fairly little memory overhead.

2. Once we have done reading all the files, we merge the sst files on disk in the previous stage to produce the top-k URLs for each shard. To speed up the second step, the URLs are sharded based on their hashes.

3. In the final step all top-k results in each shard is merged back to get the final result.

For example: 
The step 1 will generate the following files, with each a sorted string table.
```
shard0: _0/sst-0 _0/sst-1
shard1: _1/sst-0
shard2:
shard3: _2/sst-0 sst-1 sst-3
```
Then in step2, we use the `merge_iter<sst_read_iter>` over each shard's stage files to get top-k URLs per-shard.
Finally we merge the results per shard to become the final result.

This is correct because hashing guarantees that the same URL will not occur in two shards, each shard is independent to each other.


### Memory usage
There two mechanisms to protect make sure the program respects the memory limit.
1. The cooperative mechanism:
  
  It is easy to notice that the second phase is not the major consumption of memory. So the memroy pressure mainly comes from the first step.
  In `master`, we estimate the memory usage whenever a new `url` is seen and become owned by the memtables. In that case the memtables must 
  allocate memory to hold the new url. So we have to work out the estimation for each entry in the memory. Basically, for each entry, the 
  memory consumption is roughly $url_len + entry_overhead$. I used `memusage_allocator` to find out the `entry_overhead`. Note: I didn't use
  `memusage_allocator` to actually be the allocator, because we don't need to track the memory usage, and since some of the code uses C library
  functions, it is not accurate anyway. The member that controls the threshold is in `master::_mem_high_watermark`.

2. The hard limit:
  
  The memory limit is enforced by `setrlimit`, and specifically in `memusage_guard.h`. Unfortunately, this doesn't work on 
  [WSL](https://github.com/microsoft/WSL/issues/4509). And in the `memusage_guard`, we register a new `new_handler` when the
  system is having hardship in allocating new memory, we flush the memtables currently reside in memory out to disk to help
  the program progress. The member that controls the threshold is in `memusage_guard::limit`.



### Iterators
There are three iterators: `read_line_iter`, `sst_read_iter` and `merge_iter`.

The `read_line_iter` splits an text file in lines. The `sst_read_iter` is an iterator over all the key-values inside a 
SST table. The `merge_iter` does a k-way merge on k iterators.


## Fault tolerence
No. There is no fault tolerence but it should not be too difficult to add, as long as you write logs to the disk before you modify
the memtables and persist the file position of the input file onto the disk.

## Notes on testing
The unit tests in `test_master.cpp` have some workload that mimics the whole execution of the program. So we can have
some confidence that this works. Besides you can run the program on real inputs. I use a zipf-generator to generate
URLs, the file is in `third_party/genzipf.c`. You can use `build/genzipf` to generate numbers. And you should be able to use
`top100` to find that the top-100 numbers are actual 1 - 100 with decreasing frequences that respects the zipf(N, alpha) distribution. I don't have a powerful laptop, and I have very limited disk space, thus I didn't run the program on real
100G input (of course with lower memory limit). I have pre-generated a `test-urls-zipf` file using the zipf distribution.

## Caveats and future improvement
1. The URL is likely to share prefixes (e.g. http://www.), it might be benificial to make the keys in a SST share prefixes. 
   I am not sure about performance implications for this optimization, but sounds promising.
2. I don't have enough time to experiment with the following idea, and in my opinion it is unlikely to generate a better result.
   The idea is that whenever we can't find the key in the memtable, we do binary search in SST files to find the key. Since we 
   assum the URL respects the zipf distribution, we can use bloom filters to skip SST files that definitely don't have the key. 
   But it will make the program extremely complicated and will increse the # of reads to the file system significantly. I don't think 
   this is a good idea but maybe worth trying if I have enough time.
3. Currently, The resource is limited as `RLIMIT_AS`, may be consider `RLIMIT_RSS`? (Done as 0ae2e3e0286c93e2a143db28b856209eed802245)
4. There will be lots of "sst_files" in the same directory, this will bring problems when there are a lot of files.
   May be split them into different directories like git? (Done as 901dbe5488260d4607a09c65397e80c2299cd9b1).