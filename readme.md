# SimpleHashmap
Hashtables are often at the core of many software development solutions as they often provide nice runtime averages compared to Binary Search Trees when searching for specific data and don't even require that data can be sorted or maintained in that sorted order. I mean, O(1) sounds better than O(LogN) so surely its better. There's just that problem of "average" runtime. The worst case runtime of any hashtable is still O(N) which is worse than O(LogN) and its not really a comparison. You can be better off using an Array with O(N) search time in some cases. So why are they still used so much?

Turns out its possible to have more control over that Average, the memory overhead of hashtables can be lower than a binary search tree, and even when you need more comparisons in a hashtable, you have better cache locality making it so performance can still be better. So how efficient can you get a hashtable?

The motivation for this is simple. I need a better approach to grabbing values based on keys. Sorting isn't necessarily an issue as for me, I'm just sorting 64 bit numbers but in practice, I need to remove things from this list somewhat often and reference that list often too. I also would like to have limited overhead. All of this to make my current Smart Pointer System more efficient. I don't particularly care if it beats out the standard library approach in everyway but if it is faster than it currently is, that is a win. If it comes with less overhead memory overhead than Binary Search Trees, that is also a win. If a single pointer requires 32 bytes of extra information, that is a lot. Adding in another 24 bytes doesn't make it any better.

TLDR; this is will end up very similar to [unordered_dense](https://github.com/martinus/unordered_dense) which you should use instead but its valuable to understand the process to get there too. Also this works with C++14 and does not require C++17

If you want, you can skip to conclusion to see the benchmarks

# Hash function
Every hash map's efficiency is bound to the hash function itself. If the hash function is slow, so too will the entire hash map. In a perfect world, every potential "Key" would hash to something unique which can then go to a unique bucket in the hashmap. In practice, this is not feasible as the output would have to be a variable size.
That being said just having guarantees, or at the very least high confidence, that the total number of collisions will be low is more than adequate in practice. Hence why SHA1, SHA2, and several other hash functions exist.

You could throw SHA at the problem and quantize the result down to a single index number to go into the total number of buckets in a hash table but its far from ideal. It has many of the properties that a good hash function should have. Being collision resistant, unpredictable, and having great distribution. It, however, suffers from raw speed. SHA isn't particularly fast.

On the opposite side of the spectrum, the Identity hash exists which just spits back out the value that was input. Its as fast as you could possibly get but its highly predictable, not collision resistant, and has terrible distribution.

```C++
struct IdentityHash
{
    uint64_t operator()(uint64_t v)
    {
        return v;
    }
};
```

Note that being predictable means that you can force collisions which can reduce performance of any hash map. Alongside this, even if the hashmap has terrible distribution, values may end up too close to each other which can be an issue for some hash map implementations. As the hashmap increases the total number of buckets, bad distribution can be negated but that requires far more memory allocation than what may be needed resulting in reduced performance.

So in the ideal situation we have fast performance, great distribution, low predictability, and is pretty collision resistance. In practice for a hashmap, it is not required to have the collision resistance of crytographic hashes like SHA so using a hash function like [rapidhash](https://github.com/Nicoshev/rapidhash)

Know that while you can use a variety of other hash functions, the hash function just shouldn't be the identity hash function so using the standard hash function provided in C++ is not ideal.

# Potential storage approaches
Of the approaches that can be used to create a hash map, there are 2 main approaches. Either Separate Chaining (this is almost certainly what std::unordered_map does), and Open Adress Probing. Both offer advantages and disadvantages to them but the approach chosen for SimpleHashmap (and unordered_dense) is Open Address Probing.

Separate Chaining offers the advantage that it can be grown and shrunk at will easily. Deleting data is fast and there is no wasted memory. It just has the disadvantage of poor cache locality in general.

For Open Address probing, everything is stored next to each other in buckets that may only store one thing (different for multimaps) so cache locality is a bit better. Here, when a collision happens, because there is only one value per bucket, the hashmap must insert it into a new bucket. Depending on the chosen approach, that new bucket location may or may not be directly next to the previous one. There are other approaches than linear probing that offer different properties.

Here, Linear probing is picked though the option to use other approaches is left open.

After experimenting with other approaches such as using pointers to refer to values, std::optional, and a custom std::optional that also stores the hash value as well, ultimately the solution settled on was to store 3 arrays of data. A partial hash value that also specifies whether the bucket is filled or not, a pair consisting of the actual hash value and some redirection information, and an array of all of the values actually added.

Pointers were slow due to having to create and delete each one separately and cache locality being basically non existant.

std::optional was slow due to its size properties just like the custom std::optional. Grabbing a hash value means skipping over a bunch of bytes that are the stored values resulting in more reads from RAM. Even with simple keys (uint64_t), if the value is 100 bytes, grabbing buckets is always gonna cause issues. They'd need to be stored separately and if they are stored separately, you'd still end up with things being a bit slower than the chosen approach. This is due to the cost of comparing the keys.

The method chosen lines up pretty well with what [google](https://www.youtube.com/watch?v=ncHmEUmJZf4) found was efficient for quickly eliminating invalid bucket locations. It results in better cache properties since when using bytes, more buckets get loaded in and can be compared against before needing to load more.

Note that the single byte that is used contains a part of the original hash so it must contain a lot of useful data which varies between hash function too. If the hash function has great distribution, this byte will have a lot of useful information but if it has bad distribution, it may not change enough resulting in more checks against the key.

The other 2 arrays comes as a natural finding from realizing that rehashing is slow if you have to recompute the hash for each value. Also, in order to have the benefits of cache locality, buckets shouldn't be stored with the values as they mess with the size of the buckets making it so less buckets are in a single cache line.

This results in storing values separately but when thinking about the problem more, you don't need to store empty spots as the first array handles that. Just need a different strategy for relating buckets to values. This can be done using some redirection value which can be stored with the original hash value hence why its a pair. 

This lines up pretty well with what [unordered_dense](https://github.com/martinus/unordered_dense) does as well but they store slightly different information.

There is another approach that is not used due to its extra complexity. Robinhood hashing results in better distribution of data upon collision so that on average the total number of probes is lower. This results in slower more complex insert and erase operators but faster search.

Note that the information about how well different open address strategies work is obtained from [here](https://thenumb.at/Hashtables/) alongside some prior testing showing how each strategy affects search, delete, insert times along with average and maximum probe lengths over a large set of keys.

# Deletion approaches
With the use of Open Address Probing, deleting data is a bit different. You must mark the bucket free but its actually a bit more complex. A typical approach is to use tombstones which mark the spot as deleted but its not the same as free. It is treated as a free spot on insertion but when searching, the spot is treated as filled. This is required as values inserted previously may have been shifted away from their desired spot due to the deleted value and its possible and likely to miss a value.

Tombstones have an issue though. They affect the speed of searching and inserting negatively as they have to process a bunch of empty buckets just to skip over themm anyways making probe lengths longer than necessary. The fix for this is to rehash more often. Rehashing after accumalating a certain number of elements or tombstones. Rehashing does not need to extend the total number of buckets either but it does need to remove the tombstones and move values around to be closer to their desired spots.

The other approach avoids this altogether. Backwards shift deletion puts all of the cost into the deletion function and upon deleting a value, the bucket is marked free (NOT A TOMBSTONE) and the buckets in front of it are shifted backwards to hopefully its desired bucket. At the very least, it will be closer to its desired bucket.

The approach used in SimpleHashmap is backwards shifting but it is slower than unordered_dense's approach and robin hood's approach due to having to recompute distances from the desired location. The hash is stored so this isn't much worse. Just not instant.

To make deletion even faster, since data is separated from the buckets and are referred to by a redirection index, the order in the value array need not be maintained. Just that the redirection indicies are correct. This means that upon deletion, the value being deleted can be swapped with the last value in the value array and then the size of that value array can be reduced by one (lazy deletion) and if necessary calling the destructor. Using std::vector provides the function pop_back() which does the last part so the swap just needs to be implemented while updating redirection indicies and only 1 of them needs to be updated.

# Iterators
Iterators here have different properties than std::unordered_map. Here iterators remain valid upon a rehash and remain valid upon tightly fitting the hashmap to reduce unnecessary waste. This comes at a cost. 

All Iterators are completely invalid upon deletion of any element. While this is not strictly true, it should be assummed to be true. Actually only 2 iterators are invalidated which is the iterator that is being deleted (expected) and the iterator pointing to the last element in the hashmap's value array. This is not strictly the last element added.

It may be possible to keep track of which specific iterators are invalid without invalidating more than necessary but in practice it is a bit complicated.

Iterators may be used exactly like they are used for std::unordered_map and std::unordered_multimap though they are expected to be slower than unordered_dense's Iterators and other Iterators due to the extra data stored and processing needed for multimaps (this is disabled for non multimaps so performance should be comparable).

# Usage
Like std::unordered_map, this should have very similar usage with different names. This, like unordered_dense, allows for a direct replacement assuming iteration validation is not important or it has the same properties for the particular use case.

Creation and inserting into the hashmap can be done like this:
```C++
int main()
{
	SimpleHashMap<size_t, size_t> map = {{0, 32}, {3232, 101}, {101678, 9785}};
	auto it = map.insert({20202, 1111}); //returns an iterator to the newly inserted element
	it = map.insert({0, 47}); //returns an iterator to an existing element. Does not replace it.
	it = map.insertOrReplace({0, 47}); //returns an iterator to an existing element but it does replace the value.
	return 0;
}
```

Erasing from the hashmap can be done like this:
```C++
int main()
{
	SimpleHashMap<size_t, size_t> map = {{0, 32}, {3232, 101}, {101678, 9785}};
	auto it = map.erase(0); //Always returns map.end()

	it = map.begin();
	while(it != map.end())
	{
		//get rid of even keys
		if(it->first % 2 == 0)
			it = map.erase(it); //Will return an iterator to the next element
		else
			++it;
	}
	return 0;
}
```
Note that here, the iterator you get after map.erase() is not the same that you would have gotten from ++it. Internal order of the hashmap is not guaranteed to be stable which should not matter as the order is arbitrary in other hashmaps as well.

Shrinking can be done to claim back memory where needed. Its best used when the hashmap has hit its final size / maximum size. Also a fast clear function is provided if the hashmap is to be reused and will end up with a similar size resulting in little to no rehashing and optimal performance in all cases but memory usage will remain static till a rehash occurs.
```C++
int main()
{
	SimpleHashMap<size_t, size_t> map;
	for(size_t i=0; i<100000; i++)
	{
		map.insert({i, i});
	}
	map.tightlyFit(); //attempts to reduce wasted space from the internal vectors as the data has hit its maximum size


	map.fastClear(); //buckets remain the same size but the data vector is completely cleared.
	for(size_t i=0; i<100000; i++)
	{
		map.insert({i, i*47}); //never causes a rehash
	}

	return 0;
}
```

There exists a function to force rehash. This is useful in certain cases allowing the hash map to reduce the total number of buckets or if using a different deletion strategy, allows the removal of tombstones. It may be beneficial to call forceRehash() after a very large number of deletions. If the load on the hashmap is lower than 40% load, it may be better to claim back some of that memory space. It will lead to additional rehash calls if used at bad times like if following a large number of deletions, a large number of insertions happen. In that case, keeping the excess buckets will be more beneficial as they will be filled instead of having to recreate them.
```C++
int main()
{
	SimpleHashMap<size_t, size_t> map;
	//insert a somewhat large number of elements
	for(size_t i=0; i<100000; i++)
	{
		map.insert({i, i});
	}
	
	//delete half of the elements
	it = map.begin();
	while(it != map.end())
	{
		if(it->first % 2 == 0)
			it = map.erase(it);
		else
			++it;
	}

	map.forceRehash(); //reduces the total number of buckets hopefully placing elements closer to their desired location if they aren't already close.

	//DON'T DO THIS. This will result in more rehashes than necessary. Removing the forceRehash() part would allow this to never need to rehash
	for(size_t i=100000; i<150000; i++)
	{
		map.insert({i, i});
	}
	return 0;
}
```

All other use cases should be identical to std::unordered_map, std::unordered_set, std::unordered_multimap, std::unordered_multiset. Also note that transparent functionality is supported so comparisons against types that aren't directly the key's type is supported.

# Small Optimizations
In order to make the hashmap perform as well as possible, a few tricky things are added. For multimaps, a separate key for each list is added that can be referred to instead of having to go into the list which results in better performance but it uses more memory than desired.

No SIMD is used here as in testing, it offered worse performance and at best, the same performance.

The hashmap offers the option of setting the maximum size in the template through the parameter called BIG. This will set the maximum size of the hash stored and the set the data types accordingly. This results in faster performance and less memory usage but a limit to the total number of elements you can insert into the hashmap.

A lot of C++ meta programming functions are included that results in difficult to read code and duplicate code. This results in better performance as it may remove unnecessary code but it requires a little more work on those that wish to modify the hash map itself for different experiments or new functionality.

An example of those C++ meta programming function for better performance is for trivial keys. For non trivial keys, it may make more sense to compare against the actual hash values if the partial hash (that single byte) matches before checking if the keys match. This results in less checks against keys which may be more costly but if the keys are trivial, checking the actual hashes is just extra work that is not needed. This is handled by the hashmap if the key is numerical. If its numerical, it is considered trivial. Otherwise its non trivial currently.

Note that some functions are marked constexpr and sometimes the preprocessor functions LIKELY and UNLIKELY appear. These provide small optimizations where possible and may show better performance in later C++ standards.

# Conclusions
Unordered_dense is basically what we created here though its a little different. unordered_dense is certainly more polished and you should expect it to run a bit faster. An actually complete version of the hashmap this discusses is here in this repository and is also to be featured in my own library I use for C++ development [SMPL](https://github.com/Davidah121/SMPL). The objective here wasn't to beat unordered_dense or anything but to see how far you can go and understand WHY its like this along with adding multimap functionality.

With this hashmap, you have the choice of different iterator strategies depending on implementation. If you choose to ignore multimaps, iterators are invalidated whenever the internal vector storing elements has to resize and on erasure of any element, the last element in the internal vector also becomes invalid.

If you choose to have multimaps but use linked list, the same rules apply. If you implement multimaps in place and have tombstones, iterators are invalid upon rehash and only the element being deleted is invalidated when you erase something. If you use backwards shifting, rehashes invalidates all iterators and erasure invalidates many and potentially all valid iterators too.

While none of these strategies are perfect, they aren't too far away from what std::unordered_map and std::unordered_multimap offer. Tombstones do offer what I would consider the best iterator properties matching what std::unordered_map and std::unordered_multimap offer and since you can control rehashing by setting the total number of buckets, you have more control over when your iterators become invalid.

Another note, without backwards shifting, its possible to trigger rehashing simply due to all the excess tombstones. You'd need to clean those up or take note at the rehashing stage that you don't need to double the number of buckets. This means its possible to rehash multiple times doing sommething like this (thanks google):
```C++
void badFunction(HashMap& map)
{
    for(int i=0; i<10000; i++)
    {
        map.insert(i);
        ///do something probably
        map.remove(i);
    }
}
```

Its worth noting even if you may not do this. Backwards shifting doesn't have this issue as a bucket doesn't have to note if it had been used and you won't be rehashing over used buckets but instead occupied buckets.

The trade off of deletion strategy comes in the form of making insert,find,rehash slower while keeping more stable iterators on delete or making deletion slower with iterators that aren't stable at all upon deletion.

Enough talking. Benchmarks

Disclaimer: These numbers will vary upon hardware, OS, and what you are doing on your system and shouldn't be taken as if they are hard truth values but instead of how they are relative to each other for the specific task

### These benchmarks used std::string as the key and a 32 byte structure that is trivially destructable as its value. 100 iterations

#### NEWER BENCHMARKS
| Hashmap Name       | Clear Time | Random Clear Time | In order Insert | Random Insert   | Search    | Remove     |
|--------------------|------------|-------------------|-----------------|-----------------|-----------|------------|
| std::unordered_map | 0.054098161 | 0.000803468 | 0.119160767 | 0.060375320 | 0.000000081 | 0.000000155 |
| smpl::SimpleHashTable | 0.002123916 | 0.000022448 | 0.063534452 | 0.049051729 | 0.000000049 | 0.000000076 |
| ankerl::unordered_dense | 0.002586656 | 0.000607927 | 0.053142968 | 0.049052183 | 0.000000048 | 0.000000062 |
| ankerl::unordered_dense_segmented | 0.003074640 | 0.000644016 | 0.052255512 | 0.057540908 | 0.000000058 | 0.000000079 |
| robinhood::node_map | 0.011814384 | 0.001297396 | 0.102819642 | 0.062919741 | 0.000000057 | 0.000000131 |
| robinhood::unordered_flat_map | 0.005707878 | 0.000929507 | 0.108942996 | 0.049280971 | 0.000000046 | 0.000000096 |

Quick note: This used to list the times for a tombstone version without backwards shift deletion. I did not retest it but it performed about 3% worse on all insertion task, 2.1% better on search, and about 8% better on remove.

std::unordered_map does NOT use std::hash as it uses the identity hash for numbers which is just a bad hash. It instead uses rapid hash just like SimpleHashTable. The other maps use their default hash function. Sure you could have all of them use the identity hash too or all use the same hash function too. I chose this method as it is likely how a developer will chose to use the map out of the box with the exception of the std::unordered_map. wyhash is also very similar in performance to rapidhash considering rapidhash is designed to be the upgraded version of wyhash. Because of this, all of the hash functions should be similar enough in performance that they can be removed from the equation

Notice that all of these hashmaps beat out the standard one though that doesn't mean that they have the same guarantees. These numbers are bound to have a bit of noise and its not as if the benchmarks are the best benchmarks in the world. There are bound to be cases were this implemented hashmap falls short due to some C++ thing I didn't do which is why using unordered_dense is definitely the way to go.

What do these benchmarks even cover? It does record time to construct the hashmap but all of these don't do anything noteworthy in their constructors so its basically instant. Omitted because it would basically be 0 in each of them. 
- Clear time records how long it takes to clear out 1 million elements that were added to it. 
- In order Insert inserts 1 Million strings that are the numbers 0 - million in order. This was originally done with 64 bit integers however if the identity hash is used, std::unordered_map looks far better.
    - All hashmaps look better by a lot when you do this and they still beat out the standard. Using strings does come with the cost of comparing strings so if a hashmap compares keys a lot and can't exit quickly with partial hashes or just the hash alone, this will be slower for that hashmap.
- Random Insert inserts 1 Million strings that are the numbers 0 - 32768.
    - This will cause a lot of collisions so early exiting is important. Also useful when developing as you can see if you insert the correct number of elements. If you insert more than 32768, you messed up.
    - These numbers will be lower but not too far apart from each other
- Search. This adds 1 million elements in the same was as In order Insert does and searches for a random value. This is done multiple times to get a better average. This is expected to be pretty low.
    - Note that the code that does this is not ideal. You can make a better benchmark for sure but the relative difference between the search times is what matters not the exact values.
- Remove inserts 1 Million strings in the same was as In order Insert and then removes 10000 elements from that also in order.
    - This approach always deletes something so there is no early exit if the key was already deleted.

For fun, these are the results for multimaps. Here since there is no multimap for unordered_dense or robinhood, those will be skipped but note that the performance if you have it store std::list<std::pair<K, V>>, its very comparable:

### These benchmarks used size_t as the key and a 32 byte structure that is trivially destructable as its value. 100 iterations
| Hashmap Name       | In Order Insert Clear Time | Random Insert Clear Time | In order Insert | Random Insert   | Search    | Remove     | Iteration    |
|--------------------|----------------------------|--------------------------|-----------------|-----------------|-----------|------------|--------------|
| std::unordered_multimap | 0.061485461           | 0.085785920              | 0.104064487     | 1.086142850     | 0.000000033 | 0.000000163| 0.021734782|
| smpl::SimpleHashMultiMap | 0.025469959          | 0.084360004              | 0.077554960     | 0.059063060     | 0.000000030 | 0.000000149| 0.010471630|
| smpl::SimpleHashMultiMap (Inplace insertion) | 0 | 0                       | 0.071669810     | 0.276171670     | N/A         | N/A        | N/A        |


These benchmarks are identical to the above except that deletion doesn't quite work the same way so its not quite fair to compare the inplace version with the other 2. Deletion for unordered_map removes ALL duplicate keys. SimpleHashMultiMap does the same but the inplace insertion method does not do this. Also note that even though the inplace insertion solution has faster search time, the iteration time is going to be slower based on how data was inserted into the hashmap itself (along with iterators having different properties).

Here, both multimap still out do the std::unordered_multimap however, the search and remove times are quite comparable. They fluctuate so running with more iterations is necessary to remove testing variance. I changed to 100 iterations to test just search and remove though more is likely needed.

Iteration time is the time required to visit all elements in the multimap. Not the time on average to visit a single element. That can be solved by dividing the total time by 1 million but the important thing here is again the time relative to the std::unordered_map.

Note that making a psuedo multimap with unordered_dense is slightly faster but is lacking a custom iterator implementation. Also these are using size_t as the key so clear times are instant / near instant. If you use a non trivially destructable key like the previous testing, deletion is no longer instant 


As you can tell, all of these hashmaps (which can all be sets too) are faster and often by a lot. If you can deal with some of the quirks associated with them, its worth trying them out. Otherwise, with all the things covered, you can surely change the internal data structures in the hashmap provided and get back some of those comforts you wish for. 

If you want better benchmarks, check these 2 out:
- [Benchmark 2019](https://martin.ankerl.com/2019/04/01/hashmap-benchmarks-01-overview/)
- [Benchmark 2022](https://martin.ankerl.com/2022/08/27/hashmap-bench-01/#result-analysis)
- [Optimizing Open Addressing Strategies](https://thenumb.at/Hashtables/)

As for memory usage, well I didn't test the memory usage for all maps but I can talk about the memory usage of the current implemented map created. 9 or 17 bytes are used per bucket depending on if it is defined as BIG or not. The map only ever rehashes at past 80% fill so lets say we calculate this for 1 million elements. We only care about overhead associated with the hashmap and therefore the Key and or Value types stored won't affect anything. We are comparing against std::vector where you'd store those things and use the naive O(N) linear search approach. If comparing to that, you'd have 1.2 million buckets in total since you have 1 million objects and you are assuming a mostly filled hashmap (you can force this too by allocating the exact number of buckets needed).
With that in mind, its 
- 10.8 MBytes if its not BIG
- 20.4 MBytes if it is BIG

The data stored in the benchmark is 32 bytes in size and the key is 8 bytes so a total of 40 bytes. Our hashmap does not need to allocate space for KeyValuePairs that aren't associated with a bucket yet so that array is always the exact size needed.
- 40 MBytes

So in total its upto 60.4 MBytes (potentially more if you don't manually allocate the total needed buckets to have it more tightly fit). Note that std::vector overhead isn't included here and that is because it is possible to tightly fit that as well.

This is quite ideal compared to previous attempts as with a Balanced Binary Search Tree (assuming no padding) would need 25 bytes per tree node (need one boolean for red-black node) and 40 bytes for the KeyValuePair (assuming we still store both key and value). So 65 MBytes in total. Note that if a multimap implementation is used, add 16 bytes per element added and add the size of the key for each unique key.
If you assume all unique keys
- 24 MBytes
For a total of 74.8 MBytes if its not BIG and 84.4 if it is. Note that using a Balance Binary Search Tree with duplicates would also need to factor in the extra book keeping by std::list (and really any linked list) so add 16-24 MBytes there depending on how you chose to store it and if you store a version of the key separately for faster searching.

As for the std::unordered_map, its a bit confusing but the leading idea is that it isn't very good. I used [this](https://stackoverflow.com/questions/25375202/how-to-measure-the-memory-usage-of-stdunordered-map) to approximate the total so just know its not 100% accurate.
- ~111.5 MBytes

This value may be larger or smaller depending on implementation and things outside of the programmers control but the general problem is that its almost 2x bigger than our implementation and that Balanced Binary Search Tree. That is a huge overhead compared to just storing the data directly (which is 40 MBytes if tightly fit). We are using 1 million elements so its not as bad. If you use a lot less elements its perfectly fine and its terrible if you are using far more. Since this requires tightly fitting to get these kinds of results, the hash table offers the option to do that.

This should cover everything I set out to do. As for me, I'm satisfied. I even got to use the complicated C++ Meta programming stuff and I got to create an amazing hashmap that I'll personally be using in the future.

# Extra
This is a different readme from the original that is still included in the project. Its less code heavy while still providing the necessary information to understand the hashmap. Its more consise in my opinion making it a faster read.

# References
- [Google's efficient hashmap C++Con talk](https://www.youtube.com/watch?v=ncHmEUmJZf4)
- [Robinhood hashmap](https://github.com/martinus/robin-hood-hashing)
- [Wyhash](https://github.com/wangyi-fudan/wyhash)
- [rapidhash](https://github.com/Nicoshev/rapidhash)
- [unordered_dense](https://github.com/martinus/unordered_dense)
- [C++ Meta Programming Reference](https://en.cppreference.com/w/cpp/meta.html)
- [std::unordered_map size approximation](https://stackoverflow.com/questions/25375202/how-to-measure-the-memory-usage-of-stdunordered-map)
- [ankerl's Benchmark 2019](https://martin.ankerl.com/2019/04/01/hashmap-benchmarks-01-overview/)
- [ankerl's Benchmark 2022](https://martin.ankerl.com/2022/08/27/hashmap-bench-01/#result-analysis)
- [Optimizing Open Addressing Strategies](https://thenumb.at/Hashtables/)
- [std::unordered_multimap::erase](https://en.cppreference.com/w/cpp/container/unordered_multimap/erase.html)
