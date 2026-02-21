# Building an Efficient HashTable
Hashtables are often at the core of many software development solutions as they often provide nice runtime averages compared to Binary Search Trees when searching for specific data and don't even require that data can be sorted or maintained in that sorted order. I mean, O(1) sounds better than O(LogN) so surely its better. There's just that problem of "average" runtime. The worst case runtime of any hashtable is still O(N) which is worse than O(LogN) and its not really a comparison. You can be better off using an Array with O(N) search time in some cases. So why are they still used so much?

Turns out its possible to have more control over that Average, the memory overhead of hashtables can be lower than a binary search tree, and even when you need more comparisons in a hashtable, you have better cache locality making it so performance can still be better. So how efficient can you get a hashtable?

The motivation for this is simple. I need a better approach to grabbing values based on keys. Sorting isn't necessarily an issue as for me, I'm just sorting 64 bit numbers but in practice, I need to remove things from this list somewhat often and reference that list often too. I also would like to have limited overhead. All of this to make my current Smart Pointer System more efficient. I don't particularly care if it beats out the standard library approach but if it is faster than it currently is, that is a win. If it comes with less overhead memory overhead than Binary Search Trees, that is also a win. If a single pointer requires 32 bytes of extra information, that is a lot. Adding in another 24 bytes doesn't make it any better.

TLDR; this is building up to something very similar to [unordered_dense](https://github.com/martinus/unordered_dense) which you should use instead but its valuable to understand the process to get there too.
If you want, you can skip to conclusion to see the benchmarks

# Layout
For starters, lets establish what we are comparing against. This is all going to be done C++ so we should compare to the standard library unordered_map which is the typical first instinct when going for a hashmap. Note that a hashset and hashmap aren't that different from each other (which will be shown later) so this is just over hashmaps for the most part and it'll just be converted into a hashset around the end.

std::unordered_map is a little complicated as its implementation is up to the distributer of your standard library as long as they abide by the standards. Due to how the standards are defined however, implementations are pretty restricted. Our hashmap should have similar functionality so lets lay out all the main functions we need.

```C++
template<typename K, typename V>
class HashMap
{
public:
    HashMap();
    ~HashMap();

    auto insert(const std::pair<K, V>& p);
    auto find(const K& key);
    void remove(const K& key);
    void clear();

private:
};
```

This will be expanded upon as we go along but this is basically what we need. Its missing somethings of course but the idea is there.

# Hash function
Every good hashmap relies on its hash function. It must be fast, result in as few collisions as possible, and also have good distribution. So a bad hash function would be the identity function:

```C++
struct IdentityHash
{
    uint64_t operator()(uint64_t v)
    {
        return v;
    }
};
```

Its certainly fast (the fastest you can have) but its distribution is not good at all. Ideally, the value '1' and '2' should not be right next to each other. More over, you shouldn't be able to force collisions. Typically, you don't have infinite buckets so you use the modulus operation to reduce your hash down to a valid bucket index. 

Say you only have 1024 buckets. If you use the identity hash, you can map every key value pair to the first bucket just by using multiples of 1024. Not ideal. At that point, it doesn't matter which collision strategy you use as you are always gonna hit worse case. With that in mind, using the default hash function in the standard library may not work out for you as it does use the identity for integer values.

You shouldn't have to think about what values you use as a key at all. It should just work. Now creating a hash function that is fast but also results in great distribution of data is very complicated and to be honest, above my understanding so lets just use an existing hash function that provides those things.

- [Wyhash](https://github.com/wangyi-fudan/wyhash) (used by unordered_dense)
- [rapidhash](https://github.com/Nicoshev/rapidhash)

These 2 will work and you could use any other hash that you like. The hashmap we create will just take in any existing hash function you provide and that includes the standard hash function.
Note that Cryptographically safe hashes are going to give you amazing distribution and lower collisions but they aren't going to be fast. Probably too slow as well. Remember that the goal is to be as close to std::vector's or any array's access speed. It should also just be better than a Balanced Binary Search Tree in terms of search speed.

# Basic Collision Resolution
So what do you do when a collision happens? We don't live in a perfect theoretical world where you have infinite space and can put everything into its own unique bucket. The less buckets you allocate, the more likely you are to have collisions and it just doesn't make sense to allocate 100x more buckets than items just so you can attempt to minimize collisions (and they can still happen).

### Separate chaining
This is the approach used by unordered_map most likely. Its the easiest to understand. Each bucket is actually a linked list so if you run into a collision, just tack it on the end of the linked list. Provided you don't have a lot of collisions, each linked list should be pretty small. You do have an issue though. Its not stored in continuous memory so when you have to check over these linked list to ensure that your key is actually unique, you can assume that by default each check is gonna be a cache miss. A linked list is easy to delete from though and easy to insert into and all references will remain stable.

### Open Adress probing
This approach treats each bucket as if it can only hold one thing. When a collision happens, you insert into the next avaliable bucket you find. This results in new data not always being able to be inserted into its desired bucket and additional book keeping to know if a bucket is filled. Deleting is not as simple as unlike a linked list, this is all continuous memory.

# Approach 1
Well since we know what performance Separate chaining offers since we have std::unordered_map, what about Open Adressing? We just need to solve a few problems with it.
Separate Chaining is a valid choice still but it runs a few issues regarding total buckets and rehashing. When do you rehash and how many buckets do you create. Many of the higher performance hashmaps use open addressing as well so there must be something to it.

An obvious solution is to just use open addressing with pointers. This allows us to know if a bucket is filled at all and deleting is easy. Just delete the pointer and set it to nullptr. For storage, use a std::vector as its the easiest choice.

For all of these code segments, assume there will be things not correct so don't copy this. Ignore typos and errors as these are just to get the point across.

```C++

template<typename K, typename V, typename HashFunction>
class HashMap
{
public:
    HashMap()
    {

    }
    ~HashMap()
    {
        clear();
    }
    
    auto insert(const std::pair<K, V>& p)
    {
        if(data.empty())
            data = std::vector<std::pair<K, V>*>(1024);

        uint64_t keyHash = hf(p.first);
        uint64_t desiredLocation = keyHash % data.size();
        while(data[desiredLocation] != nullptr)
        {
            if(data[desiredLocation] == p)
                return data[desiredLocation];
            desiredLocation = (desiredLocation + 1) % 1024;
        }

        //create a new pointer which will hold the data.
        data[desiredLocation] = new std::pair<K, V>(p);
        elements++;
        return data[desiredLocation];
    }
    void clear()
    {
        //must delete pointers
        for(std::pair<K, V>* p : data)
        {
            if(p != nullptr)
                delete p;
        }
        data.clear();
    }

private:
    uint64_t elements=0;
    std::vector<std::pair<K, V>*> data;
    HashFunction hf;
};
```

Nice so we have this now.... what happens when the table fills up. What happens when you delete things too? Lets say you have 2 keys that hash to the same thing K1 and K2. You insert K2 first and then K1. Then you decide that K2 is no longer needed and so it gets removed. Then you go and insert K1 again which shouldn't do anything but return the existing K1 since its not a unique key. Well since they hashed to the same location and K2 was added first. The desired location of K1 is empty now so you can insert K1 in that spot. When you insert K1 again, its going to go to that empty spot instead of realizing K1 already exists.

This means we need to keep up with what spots had data deleted from it. These are the other part of tombstones and they are the extra book keeping required for open addressing.

```C++
template<typename K, typename V, typename HashFunction>
class HashMap
{
public:
    auto insert(const std::pair<K, V>& p)
    {
        if(data.empty())
        {
            data = std::vector<std::pair<K, V>*>(1024);
            usedSpot = std::vector<bool>(1024);
        }

        uint64_t keyHash = hf(p.first);
        uint64_t desiredLocation = keyHash % data.size();
        uint64_t insertLocation = -1;
        while(usedSpot[desiredLocation] == true)
        {
            if(data[desiredLocation] == nullptr && insertLocation != -1)
            {
                insertLocation = desiredLocation;
            }
            else
            {
                if(data[desiredLocation]->first == p.first)
                    return data[desiredLocation];
            }
            desiredLocation = (desiredLocation + 1) % 1024;
        }
        if(insertLocation == -1)
            insertLocation = desiredLocation
        //create a new pointer which will hold the data.
        data[insertLocation] = new std::pair<K, V>(p);
        usedSpot[insertLocation] = true;
        elements++;
        return data[insertLocation];
    }

    auto find(const K& key)
    {
        if(data.empty())
            return nullptr;

        uint64_t keyHash = hf(p.first);
        uint64_t desiredLocation = keyHash % data.size();
        while(usedSpot[desiredLocation] == true)
        {
            if(data[desiredLocation] != nullptr)
            {
                if(data[desiredLocation]->first == p.first)
                    return data[desiredLocation];
            }
            desiredLocation = (desiredLocation + 1) % 1024;
        }
        return nullptr;
    }
    void remove(const K& key)
    {
        if(data.empty())
            return;

        uint64_t keyHash = hf(p.first);
        uint64_t desiredLocation = keyHash % data.size();
        while(usedSpot[desiredLocation] == true)
        {
            if(data[desiredLocation] != nullptr)
            {
                if(data[desiredLocation]->first == p.first)
                {
                    delete data[desiredLocation];
                    data[desiredLocation] = nullptr;
                }
            }
            desiredLocation = (desiredLocation + 1) % 1024;
        }
    }
private:
    uint64_t elements=0;
    std::vector<bool> usedSpot;
    std::vector<std::pair<K, V>*> data;
    HashFunction hf;
};
```

Now we have a mostly functional hashmap using tombstones. Notice we still don't have a rehash yet so this still only works up to 1024 elements. Notice that when deleting, we don't mark the slot as not used meaning that even though that slot can be inserted into, it has been tainted as important things may exist past it. This is important to note as rehashing will have to take that into account. Lets define another counter for all usedspots. That way we don't have to constantly iterate over that array everytime we need that value.

We should also rehash. Rehashing is pretty easy and there isn't much you can do about its speed but lets add that too.

```C++
template<typename K, typename V, typename HashFunction>
class HashMap
{
public:
    auto insert(const std::pair<K, V>& p)
    {
        //all of the inserting stuff. Its the same

        //create a new pointer which will hold the data.
        data[desiredLocation] = new std::pair<K, V>(p);
        if(usedSpot[insertLocation] != true)
            actualUsedSpots++;
        usedSpot[insertLocation] = true;
        elements++;

        if((float)actualUsedSpots / (float)usedSpots.size() > 0.8)
        {
            rehash();
        }

        return data[insertLocation];
    }

private:

    void rehash()
    {
        std::vector<bool> newUsedSpot = std::vector<bool>(usedSpot.size()*2);
        std::vector<std::pair<K, V>*> newData = std::vector<std::pair<K, V>*>(usedSpot.size()*2);
        actualUsedSpots=0;
        elements=0;

        for(std::pair<K, V>* p : data)
        {
            specialInsert(newUsedSpot, newData, p);
        }

        usedSpot = std::move(newUsedSpot);
        data = std::move(newData);
    }

    void specialInsert(std::vector<bool>& newUsedSpot, std::vector<std::pair<K, V>*>& newData, std::pair<K, V>* p)
    {
        uint64_t keyHash = hf(p.first);
        uint64_t desiredLocation = keyHash % data.size();
        uint64_t insertLocation = -1;
        while(usedSpot[desiredLocation] == true)
        {
            if(data[desiredLocation] == nullptr && insertLocation != -1)
            {
                insertLocation = desiredLocation;
                break;
            }
            desiredLocation = (desiredLocation + 1) % 1024;
        }
        if(insertLocation == -1)
            insertLocation = desiredLocation

        //create a new pointer which will hold the data.
        newData[insertLocation] = p
        newUsedSpot[insertLocation] = true;
        elements++;
    }

    uint64_t actualUsedSpots=0;
    uint64_t elements=0;
    std::vector<bool> usedSpot;
    std::vector<std::pair<K, V>*> data;
    HashFunction hf;
};
```

Notice how in rehash, we didn't create any new data and we also don't check if its already in the list. Its unnecessary since we didn't allow duplicate keys in the public facing insert function. Using std::move is important as well as it avoids unnecessary copying.

So with all this, you should have a functional hashmap. So how good is it? 

The quick and certainly not exhaustive test I developed should give an idea of how good a hashmap is at the basics. Not any one pattern. Its possible that your particular usage pattern can exhibit properties that make one hashmap better than another so its best to benchmark with your specific use case if you can. These benchmarks just care about determining how fast the individual functions are.
So how does our last approach do? Terrible. It took so long with the default settings of 1 million elements that I lowered it and it still took a while. Its clearly slower but why? Spoiler alert. I knew it'd be slower by a lot and unusable. Creating and deleting Memory is slow. Pointers aren't the best for this due to that. Specifically, each element is a pointer and you have to delete each and everyone of those which is bound to be slow so clear times are as slow as std::unordered_map. Random insert is also terrible (ignoring the fact that its incorrect even after fixing up the code so it'd work properly.). Constantly comparing against keys that have already been inserted just to have to ditch the entire result just isn't good. We also skipped a few extra optimizations with C++ but those will come later too. But now we know a bit more so lets tackle those problems

# Approach 2
First things first. Lets drop pointers if we can. We'd still like to have one of the benefits of pointers (being able to tell if it exists) but not the redirection and memory upkeep of them. C++ gives us std::optional which will do basically everything we need. This results in a simple change. Swap all pointers with std::optional. This results in much much faster clear times though this doesn't fix the other main issue which is searching speed. This is where it may make more sense to store the hash value along side the optional data. Due to the nature of how std::optional works, it likely is padded and stores a boolean or something alongside your data to determine if it exists. If we could take advantage of that value and say store the hash in that padded data and keep say the first bit as the valid bit, we'd have 63 bits worth of hash info which is more than enough for our application.

```C++
template<typename T>
struct TestNode
{
    static const uint64_t VALID_BIT = 0x8000000000000000;
    static const uint64_t HASH_BITS = 0x7FFFFFFFFFFFFFFF;
    TestNode()
    {
        // isValid = false;
    }
    TestNode(const T& data)
    {
        isValid = VALID_BIT;
        this->data = data;
    }
    TestNode(T&& data)
    {
        isValid = VALID_BIT;
        this->data = std::move(data);
    }
    TestNode(const TestNode<T>& other)
    {
        copy(other);
    }
    void operator=(const T& data)
    {
        isValid = VALID_BIT;
        this->data = data;
    }
    void operator=(const TestNode<T>& other)
    {
        copy(other);
    }
    TestNode(TestNode<T>&& other) noexcept
    {
        move(other);
    }
    void operator=(T&& data) noexcept
    {
        isValid = VALID_BIT;
        this->data = std::move(data);
    }
    void operator=(TestNode<T>&& other)
    {
        move(other);
    }
    ~TestNode()
    {
        properDestroy();
    }

    bool getValid()
    {
        return isValid & VALID_BIT;
    }

    T& getData()
    {
        return data;
    }

    void setHash(uint64_t h)
    {
        isValid = isValid | (h & HASH_BITS);
    }
    uint64_t getHash()
    {
        return isValid & HASH_BITS;
    }

    uint64_t isValid; //lowest bit is the valid bit. uint64_t so it is possible to fit more data into it through subclasses
    union
    {
        T data;
        bool empty;
    };

private:
    void copy(const TestNode<T>& other)
    {
        properDestroy();
        isValid = other.isValid;
        if(isValid & VALID_BIT)
            data = other.data;
    }
    void move(TestNode<T>&& other) noexcept
    {
        properDestroy();
        isValid = other.isValid;
        data = std::move(other.data);
        other.isValid = false;
    }

    template<class K = T>
    typename std::enable_if<!std::is_trivially_destructible<K>::value, void>::type
    properDestroy()
    {
        if(isValid & VALID_BIT)
            data.~T();
        isValid = 0;
    }
    
    template<class K = T>
    typename std::enable_if<std::is_trivially_destructible<K>::value, void>::type
    properDestroy()
    {
        isValid = 0;
    }
    
};
```

Trying to add a custom optional type instead of just hacking the one in the standard library is kinda simple. You just need to use unions and these not so easy to understand meta programming templates.
Those are just so you can skip some parts of destroying an object. Does this help? well it can certainly help when rehashing. You no longer need to recompute a hash which can be time consuming and since you are already using optionals, you might not lose any extra space since by default it should be padded. The question is, does this actually help? Well sorta. If you go the full extra mile, you can have the benefits of instant delete and never needing to recalculate hashing but it still has some issues. Note that I also added in a check to see if the hashes are the same in searching to avoid the cost of comparing keys which could be high. Also.... this breaks some standards.... well we weren't interested in maintaining those standards but one of those is actually useful. References are valid when returned until deleted. Here, that isn't really true. It was true when dealing with pointers but not here. Note that a rehash also invalidates your references.

# Approach 3
At this point, I was a bit disappointed. Sure clear times were better but insert time and random insert times were terrible. Seems like at this point, you should just use the default given hashmap. Then Robinhood hashing stepped in. To save ourselves the trouble of actually implementing it, I'm going to link to an existing one and talk about it as its not what we will ultimately go with due to there being better performance out there.

At its core, robinhood hashing is just open addressing but it has 2 important things that make it better at least theoretically. In robinhood hashing, you keep track of how far any value is from their desired position. This is used when inserting. When you insert, in order to keep things fair, values that are really close to their desired location should be moved so that other values can be a bit closer. This moving around results in better average search times. Its called Robinhood hashing due to this idea of "take from the rich and give to the poor". This also comes with the benefits of not needing to maintain tombstones. Its possible to exit early with robinhood hashing before hitting an empty spot just based on how far values are from their desired spot. When deleting, you just need to slide values back (shifting them backwards into previous buckets) until you hit an empty bucket, or you move something into its desired spot. In fact with Robinhood hashing, its possible to get O(LogN) in the worst case for searching though you need to maintain more data so this is not likely worth it.

For testing, I attempted my own once and it worked out okay baring glitches but for reference you can use [this](https://github.com/martinus/robin-hood-hashing). When tested in the same benchmark, it performs about 1.8x faster to 2x faster depending on datatype, and if its a flatmap or not. Besides its complexity, in order to get that kind of performance, you'd need more C++ specific functions which will come a bit later. For now just take note of the fact that traditional tombstones aren't needed here. Just need to know if a slot is empty or not but don't need to know anything about whether it was previously used.


# Approach 4
During my search for fast hashmaps, I came across a [nice C++ talk](https://www.youtube.com/watch?v=ncHmEUmJZf4) (thanks google). This talk drops a ton of useful information and pictures which you'll notice I have none of. Worth a watch just for that. Here in this talk, they approach building a hashmap a bit different in a way that does break standards (though this isn't necessarily a problem provided you know this). The main take aways from this is that you can avoid checking the key by checking the hashes first and you can also avoid checking the entire hash if you just check the first byte. Assuming your hash is good, the first byte should be quite unique so you could attempt to use that as another filter. While I won't do it, there was the idea of using SSE to check multiple potential things at once. This in practice is not as good as it sounds since at the end of all this, you still need to extract the indicies that do have potential matching hashes. I tried and it was slower though it could be possible to be faster so I won't write it off for good.

The main thing of note is that we can store partial hash values and like before, we can store the entire hash so we never recompute them. We still have tombstones though so to be space efficient, lets store that information into our partial hash. That will leave us 7 bits of hash to work with but its fine.

Next thing is to try to use more cache locality. Its the main reason of using partial hashes anyways so why not try to get more out of it. Normally we'd store hash data with our own data but what if our data is large? If so, we have to step over a lot of bytes and potentially an entire cacheline every time we want to compare hashes. With this in mind, Separate our data from the hashmap specific stuff. So something like this:

```C++
template<typename K, typename V, typename HashFunction>
class HashMap
{
public:
    auto insert(const std::pair<K, V>& p)
    {
        //Mostly the same initial part

        while(partialHash[desiredLocation] & VALID_BIT != 0)
        {
            if(!data[desiredLocation].getValid() && insertLocation != -1)
            {
                insertLocation = desiredLocation;
            }
            else
            {
                if(partialHash[desiredLocation] = thisKeyPartialHash)
                {
                    if(data[desiredLocation].getHash() == keyHash)
                    {
                        if(data[desiredLocation].data.first == p.first)
                            return data[desiredLocation];
                    }
                }
            }
            desiredLocation = (desiredLocation + 1) % data.size();
        }
        //Mostly the same final part
    }

private:

    uint64_t actualUsedSpots=0;
    uint64_t elements=0;
    std::vector<uint8_t> partialHash;
    std::vector<uint64_t> fullHash;
    std::vector<TestNode<std::pair<K, V>>> data;
    HashFunction hf;
};
```

Skipping a lot of the work as to not get bogged down by implementation details. Just note that TestNode is the same as above and you can use std::optional as you did before since we no longer store hash information in it. That way you don't need to worry about all the extra C++ stuff you'd need to do so it always performs well.
This is going to work out better than you'd think even though we added extra stuff to keep track of. Turns out, cache misses are problematic and that those partial hashes don't often equal each other. Well they do but not so much that it matters. It just results in more checks against the actual hash. This is why having a good hash is important. If the first byte of the hash isn't very good, you'll have to check against the actual hash more often. At this point, checking against the key is more like a sanity check since its highly unlikely (assuming a good hash) that both 64 bit hashes will be the same. Note that you don't need to use the first byte of the hash from a key as the partial hash. It could be a completely separate hash algorithm or it could be a combination of all the bytes in a hash. Just as long as its consistent and its not too slow.

If you notice something here, we are using a lot of extra space too and it should be clearer now that cache misses are a problem so extra space that isn't being used that you have to step over is also problematic.
This helps a lot but creating memory and freeing memory is a bit slow. Also it seemed that optionals weren't exactly fast either (uses more memory)

A quick note. No hashmap could ever be faster than std::vector when creating the same amount of elements. You can easily test this. Just create a std::vector of 1 million elements. Now do the same but for all of the things in our hashmap and add those together since you need them all for the hashmap to function. Time that and what you'll see is that it is slower. Not really insightful. Its pretty obvious. Even if you just extract out the data part of the hashmap, creating 1 million std::optional<std::pair<K, V>> is slower than creating 1 million std::pair<K, V>. Using TestNode doesn't make it any faster either. All this to say, if you reduce the memory overhead, you get a faster hashmap by default with no extra work.

# Approach 5 (Almost at the end)
That leaves us with just a bit more that we can do. You can replace that whole data part with just a basic vector of pairs. This reduces our overhead per data value down to nothing compared to 1-8 bytes depending on padding. With pointers, you have an extra 8 bytes too (the actual pointer to the location + the size of the memory at that location). Now this comes with a few problems. How do we delete things? Isn't deleting from an array O(N)? It is normally if you respect the order in which items were added. Its O(1) if you don't care about the order of the items which we kinda don't. Its a hashmap where you don't directly access the underlying data structures typically and when you do, you don't expect them to be in any particular order. This lets us delete by swaping the last element with our element we are deleting and just pop_back() our vector. This also means that we can keep our vector sized exactly to how many elements we actually have. The only thing we have to do is make sure that the buckets can still reference the correct spot in our data vector.

```C++
private:

    static const uint8_t VALID_BIT = 0x80;
    static const uint8_t INVALID = 0x7F;
    uint64_t actualUsedSpots=0;
    std::vector<uint8_t> partialHash;
    std::vector<std::pair<uint64_t, uint64_t>> fullHashRedirection;
    std::vector<std::pair<K, V>> data;
    HashFunction hf;
```

This is basically the solution. Instead of just storing the hash, we also store what spot in the data vector you use as the actual data for this bucket. This is great because now the total size of our hashmap is lower as well. its just some extra hashmap stuff on top of what we already using if we chose to just suck it up and use an Array with O(N) search. Now that extra stuff is not particular small either. Its 17 bytes per bucket

To save some headache, I will go ahead and cover something else note worthy. When using any datastructure, its not very often that you don't have some cap in mind when you use it. For example, when you load an image, you certainly don't expect the image to take over 2GB of ram to store. That'd be an absurd resolution which may not even be allowed and even if it is, can you even load it properly and display it? 2GB is a lot for a single image. With this in mind, is it likely that you'll use 4 billion buckets? If not, you can actually shrink the overhead required by the hashmap which in turn results in better performance because remember. Allocation of memory is not free. It may be fast but its not free. Allocating less is faster than allocating more than you need.

Using some fancy C++, you can change the type of hash and redirect index. You could keep the hash as a uint64_t but its not really necessary as you are limited to 32 bits worth of space anyways and the upper 32 bits of that hash aren't being used for location anyways. As long as the partial hash does its job well the total number of false checks should still be pretty low.
Also here, we will start to prepare for turning this into a Set. When its a set, you just won't have a value so V will be void. 

```C++

template<typename Key, typename Value, typename HashFunction, bool BIG = false>
class HashTable;

template<typename Key, typename Value, typename HashFunction, bool BIG = false>
using HashMap = HashTable<Key, Value, HashFunction, BIG>;

template<typename Key, bool BIG = false>
using HashSet = HashTable<Key, void, HashFunction, BIG>;

template<typename Key, typename Value, typename HashFunction, bool BIG>
class HashTable
{

public:

    using RedirectType = std::conditional_t<BIG, uint64_t, uint32_t>;
    using HashRedirectPair = std::pair<RedirectType, RedirectType>;
    using KeyValueType = std::conditional_t<std::is_same_v<void, Value>, Key, std::pair<Key, Value>>;
private:

    template<typename K = Key>
    typename std::enable_if_t<std::is_arithmetic_v<K>, bool>
    comparePartialHashEx(size_t loc, RedirectType h)
    {
        return true;
    }

    template<typename K = Key>
    typename std::enable_if_t<!std::is_arithmetic_v<K>, bool>
    comparePartialHashEx(size_t loc, RedirectType h)
    {
        return getPartialHashEx(loc) == h;
    }

    constexpr bool getLocationEmpty(size_t loc)
    {
        return partialHashInfo[loc] == 0;
    }
    constexpr bool getLocationEmpty(size_t loc, std::vector<uint8_t>& externalArr)
    {
        return externalArr[loc] == 0;
    }

    constexpr bool getLocationFree(size_t loc)
    {
        return partialHashInfo[loc] < VALID_BIT;
    }

    constexpr uint8_t getPartialHash(size_t loc)
    {
        return partialHashInfo[loc];
    }

    constexpr RedirectType getPartialHashEx(size_t loc)
    {
        return redirectInfo[loc].first;
    }
    constexpr RedirectType extractPartialHashEx(uint64_t hash)
    {
        return hash;
    }
    constexpr RedirectType getRedirectInfo(size_t loc)
    {
        return redirectInfo[loc].second;
    }

    constexpr uint8_t extractPartialHash(uint64_t hash)
    {
        return temp | VALID_BIT;
    }

    
    template<class K = RedirectType>
    typename std::enable_if<std::is_same_v<uint64_t, K>, void>::type
    checkIfOverflowPossible()
    {

    }
    
    template<class K = RedirectType>
    typename std::enable_if<std::is_same_v<uint32_t, K>, void>::type
    checkIfOverflowPossible()
    {
        if(arr.size() == UINT_MAX-1)
            throw std::runtime_error("TOO LARGE");
    }

    //Value
    template<class K = KeyValueType>
    typename std::enable_if<std::is_same_v<Key, K>, const Key&>::type
    getKey(KeyValueType& v)
    {
        return v;
    }

    //Value
    template<class K = KeyValueType>
    typename std::enable_if<std::is_same_v<Key, K>, const Value&>::type
    getValue(KeyValueType& v)
    {
        return v;
    }

    //std::pair<Key, Value>
    template<class K = KeyValueType>
    typename std::enable_if<!std::is_same_v<Key, K>, const Key&>::type
    getKey(KeyValueType& v)
    {
        return v.first;
    }

    //std::pair<Key, Value>
    template<class K = KeyValueType>
    typename std::enable_if<!std::is_same_v<Key, K>, const Value&>::type
    getValue(KeyValueType& v)
    {
        return v.second;
    }
    

    static const uint8_t VALID_BIT = 0x80;
    static const uint8_t INVALID = 0x7F;
    float maxLoadBalance = 0.8;

    std::vector<uint8_t> partialHashInfo; //0x00 == empty. 0x7F == deleted (only first bit empty)
    std::vector<HashRedirectPair> redirectInfo; //redirect info + stored hash
    std::vector<KeyValueType> arr;
    HashFunction hf;
    size_t usedSlots = 0;
};
```

You'll notice that there are a lot of helper functions. These are required since we are now dealing with a type that can change based on template. Extracting the partial hash here doesn't do anything but take the first byte but we do insert the valid bit there because we are using that to store tombstone information too. Also note that if the key is a number, checking the full hash is skipped as it is just slower. The cost to compare 2 numbers is certainly lower than the cost to compare 4 numbers. A few things weren't covered like what about emplace as that would make insert faster right? We can now do this with no problem using emplace_back() but we need to change how we go about inserting data as a whole. Note that you can use emplace_at() too I just didn't. Its totally not because I forgot it existed. Take a look at this:

```C++

    auto insert(const KeyValueType& v)
    {
        return emplace(v);
    }
    auto insert(KeyValueType&& v)
    {
        return emplace(std::move(v));
    }

    template<typename... Args>
    auto emplace(Args&&... v)
    {
        if(fastHashInfo.size() == 0)
        {
            partialHashInfo = std::vector<uint8_t>(1024);
            redirectInfo = std::vector<HashRedirectPair>(1024);
        }
        
        //extra check needed if and only if its possible to overflow
        //does nothing if BIG is enabled
        checkIfOverflowPossible();

        arr.emplace_back(v...); //always add to the list. Will be removed if necessary

        const Key& key = getKey(arr.back());
        uint64_t actualHash = hasher(key);

        uint8_t partialHash = extractPartialHash(actualHash); //must replace top bit so its considered valid
        RedirectType extraHash = extractPartialHashEx(actualHash);
        uint64_t intendedLocation = actualHash % partialHashInfo.size();

        uint64_t insertLocation = SIZE_MAX; //its possible that you may find a free slot before you determine if it is safe to insert
        uint64_t distanceFromDesired = 0;
        while(!getLocationEmpty(intendedLocation))
        {
            if(getLocationFree(intendedLocation))
            {
                if(insertLocation == SIZE_MAX)
                    insertLocation = intendedLocation;
            }
            else
            {
                if(getPartialHash(intendedLocation) == partialHash) //fast path but 2 checks which may be unnecessary
                {
                    if(comparePartialHashEx(intendedLocation, extraHash))
                    {
                        if(LIKELY( getKey(arr[getRedirectInfo(intendedLocation)]) == key ))
                        {
                            arr.pop_back();
                            return arr.begin() + getRedirectInfo(intendedLocation);
                        }
                    }
                }
            }
            
            intendedLocation = (intendedLocation+1) % partialHashInfo.size();
            if(insertLocation != SIZE_MAX)
                distanceFromDesired++;
        }
        
        if(insertLocation == SIZE_MAX)
            insertLocation = intendedLocation;
        
        
        partialHashInfo[insertLocation] = partialHash;
        redirectInfo[insertLocation] = {actualHash, arr.size()-1};
        usedSlots++;

        maxDistance = __max(maxDistance, distanceFromDesired);

        float currentLoadBalance = (float)usedSlots / (float)partialHashInfo.size();
        if(currentLoadBalance > maxLoadBalance)
        {
            //re-balance
            rebalance();
        }

        return arr.begin() + (arr.size()-1);
    }
```

Using this approach we can return an iterator on insert so we can still modify that value after doing so but we also construct in place. Notice that we construct in place regardless of if we need to do so or not. This results in less performance when we find a duplicate key as we need to pop_back which will destruct the object though this may not actually matter as before we were copying the object anyways. For the most part, this isn't all that different from before attempts. We still need to check if that spot is truly empty, if its a free slot or if it was used before, and if that key already exists. We just now get the advantage of skipping over costly checks if the first byte (or however you calculate partial hash) isn't the same. Also note that LIKELY appears in here. Its not really important and can be omitted but for reference:

```C++
#ifndef LIKELY
#define LIKELY(x)      __builtin_expect(!!(x), 1)
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)
#endif
```

Its just a compiler hint so that it knows which branch is expected. At that point, its unlikely that the key isn't the correct key. Note that earlier attempts could probably also be speedup if emplace is used so its probably still worth doing in practice though the issues associated with using pointers still exists. Slow deletions since you must delete each element separately instead of all at once.

From this point, its just about doing those complicated C++ meta programming template things to get extra performance out of different data types and stuff right? Except deleting is something that should be covered. It can be done in O(1) but its a bit messy at first. Should probably be added here too:

```C++
    void erase(const Key& k)
    {
        remove(k);
    }

    void remove(const Key& k)
    {
        if(UNLIKELY(arr.size() == 0))
            return;
        
        uint64_t actualHash = hasher(k);
        uint8_t partialHash = extractPartialHash(actualHash);
        RedirectType extraHash = extractPartialHashEx(actualHash);
        uint64_t location = actualHash % fastHashInfo.size();

        while(!getLocationEmpty(location))
        {
            if(getPartialHash(location) == partialHash) //fast path but 2 checks which may be unnecessary
            {
                if(comparePartialHashEx(location, extraHash))
                {
                    if(LIKELY( getKey(arr[getRedirectInfo(location)]) == k ))
                    {
                        break;
                    }
                }
            }
            location = (location+1) % fastHashInfo.size();
        }

        if(getLocationEmpty(location))
            return; //not found

        
        //if found, find the location of the last item in arr and swap that with our current spot
        if(arr.size() == 1)
        {
            //fast path that almost never happens. No need to swap with anything
            //this helps prevent extra rehashes (if you ever have 2 items, this won't help)
            if(usedSlots > 1)
                fastHashInfo[location] = 0x7F;
            else
            {
                fastHashInfo[location] = 0x00;
                usedSlots = 0;
            }
            arr.pop_back();
            return;
        }

        uint64_t lastSpotHash = hasher(getKey(arr.back()));
        uint8_t lastSpotPartialHash = extractPartialHash(lastSpotHash);
        RedirectType lastSpotExtraHash = extractPartialHashEx(lastSpotHash);
        uint64_t lastSpotLocation = lastSpotHash % fastHashInfo.size();
        
        //it exists so we can skip the extra work of checking free slots.
        //note that partialHash will ALWAYS have the valid bit set so you don't need to check if empty or free 
        //  as if the partialHash at that location is invalid, the valid bit won't be set which means it won't be equal
        while(true)
        {
            if(getPartialHash(lastSpotLocation) == lastSpotPartialHash) //fast path but 2 checks which may be unnecessary
            {
                if(comparePartialHashEx(lastSpotLocation, lastSpotExtraHash))
                {
                    if(getRedirectInfo(lastSpotLocation) == (arr.size()-1))
                        break;
                }
            }
            lastSpotLocation = (lastSpotLocation+1) % fastHashInfo.size();
        }

        //set current location to be deleted
        fastHashInfo[location] = 0x7F;

        //swap locations (don't need to fully swap)
        redirectInfo[lastSpotLocation].second = redirectInfo[location].second;

        //swap data and pop back which completes the deletion
        std::swap(arr[redirectInfo[lastSpotLocation].second], arr[redirectInfo[location].second]);
        arr.pop_back();
    }
```

This searches for thing you want to delete. If you find it, search for the bucket that holds the last element in the data array. We can do this one a bit faster as you don't need to check if it exists or not. You don't even need to compare the keys. Just compare the stored redirect location. After that, mark the current spot as deleted (not empty for the same reasons as above) and then swap redirect locations, swap data, and then pop_back() which completes the deletion. There is a little bit in there about when arr.size()==1. You can avoid setting that spot as deleted and set it to empty in certain cases but its basically pointless.

So with this we should be basically done. Just polish everything up and make sure it actually works and its faster than the standard hashmap right? Well there is one more thing. What if you simplified the tombstone process. Robinhood doesn't need to know if the tombstone was freed or not. Just needs to know if a slot is empty. It does this by changing how deleting items work. You know. The backwards shifting part. Here, we can do this too because we store the hash for each element. We can calculate where it wants to be and check how far it is from that location.

```C++

public:
    void remove(const Key& k)
    {
        if(UNLIKELY(arr.size() == 0))
            return;
        
        uint64_t actualHash = hasher(k);
        uint8_t partialHash = extractPartialHash(actualHash);
        RedirectType extraHash = extractPartialHashEx(actualHash);
        uint64_t location = actualHash % fastHashInfo.size();

        while(!getLocationEmpty(location))
        {
            if(getPartialHash(location) == partialHash) //fast path but 2 checks which may be unnecessary
            {
                if(comparePartialHashEx(location, extraHash))
                {
                    if(LIKELY( keyEqualFunc(getKey(arr[getRedirectInfo(location)]), k) ))
                    {
                        break;
                    }
                }
            }
            location = (location+1) % fastHashInfo.size();
        }

        if(getLocationEmpty(location))
            return; //not found

        
        //if found, find the location of the last item in arr and swap that with our current spot

        uint64_t lastSpotHash = hasher(getKey(arr.back()));
        uint8_t lastSpotPartialHash = extractPartialHash(lastSpotHash);
        RedirectType lastSpotExtraHash = extractPartialHashEx(lastSpotHash);
        uint64_t lastSpotLocation = lastSpotHash % fastHashInfo.size();
        
        //it exists so we can skip the extra work of checking free slots.
        //note that partialHash will ALWAYS have the valid bit set so you don't need to check if empty or free 
        //  as if the partialHash at that location is invalid, the valid bit won't be set which means it won't be equal
        while(true)
        {
            if(getPartialHash(lastSpotLocation) == lastSpotPartialHash) //fast path but 2 checks which may be unnecessary
            {
                if(comparePartialHashEx(lastSpotLocation, lastSpotExtraHash))
                {
                    if(getRedirectInfo(lastSpotLocation) == (arr.size()-1))
                        break;
                }
            }
            lastSpotLocation = (lastSpotLocation+1) % fastHashInfo.size();
        }

        //set current location to be deleted
        fastHashInfo[location] = 0;

        //swap locations (don't need to fully swap)
        redirectInfo[lastSpotLocation].second = redirectInfo[location].second;

        //swap data and pop back which completes the deletion
        std::swap(arr[redirectInfo[lastSpotLocation].second], arr[redirectInfo[location].second]);

        //extra step. shift data back till we hit an empty spot or we hit a node that is in its desired spot
        uint64_t previousLocation = location;
        location = (location+1) % fastHashInfo.size();

        while(!getLocationEmpty(location))
        {
            if(getDistanceFromDesiredSpot(location) > 0)
            {
                fastHashInfo[previousLocation] = fastHashInfo[location];
                redirectInfo[previousLocation] = redirectInfo[location];
            }
            else
                break;

            previousLocation = location;
            location = (location+1) % fastHashInfo.size();
        }
        arr.pop_back();
    }
private:
    constexpr RedirectType getDistanceFromDesiredSpot(size_t loc)
    {
        RedirectType hash = getPartialHashEx(loc);
        uint64_t desiredLocation = hash % fastHashInfo.size();
        return (loc >= desiredLocation) ? (loc - desiredLocation) : (loc+fastHashInfo.size() - desiredLocation);
    }
```

Its basically identical except we shift things back before we do the pop back. All other functions are also basically the same but the whole getLocationFree() is gone resulting in simpler code at the cost of slower deletion though that slower deletion should help keep everything closer to their desired locations a little bit.

So this is it right? Nothing else? Well yeah basically. There are a few things left like Heterogenous lookup which require a HashFunction and whatever function you use to compare keys is transparent but that is basically everything.

# Meta Programming and Polish
At this point, you can mostly ignore this part as this is C++ specific and if you were looking for a fast hashmap in general, you have everything you need already. Here though, we are looking at C++ meta programmming functionality and trying to implement some nice comforts. Somethings were kinda skipped over earlier but lets look a bit further into those template functions:
```C++
template<typename Key, typename Value, typename HashFunction, bool BIG>
class HashTable
{

public:

    using RedirectType = std::conditional_t<BIG, uint64_t, uint32_t>;
    using HashRedirectPair = std::pair<RedirectType, RedirectType>;
    using KeyValueType = std::conditional_t<std::is_same_v<void, Value>, Key, std::pair<Key, Value>>;

///REMAINING STUFF
};
```

C++ provides templated functions that let you select between things, check type, check properties of those types, etc. Using that, we were able to change our hashmap into a set without duplicating a lot of code.
You could use standard object oriented programing to do this by just having the class extend and overload some functions but it won't fix all of the issues that meta programming can.

Just to get the idea, std::conditional_t lets you select between two types based on the first parameter which is the condition. We check if Value is void and if so, we change from using a pair or just the key. We also use that same function to select between whether to use a 32 or 64 bit type for indexing and the hash we store. Remember that we don't require a 64 bit hash if we limit our space to 32 bit. It will just cut out the upper 32 bits. Its only useful for comparisons between the hashes and provided the hash is good, bottom 32 bits should be pretty unique resulting in fewer collisions. We already require that the first byte (partial hash) is pretty well distributed to avoid a lot of collisions.

You can go a bit further and just remove entire functions based on type too.
```C++
template<typename Key, typename Value, typename HashFunction, bool BIG>
class HashTable
{
private:
    template<class K = RedirectType>
    typename std::enable_if<std::is_same_v<uint64_t, K>, void>::type
    checkIfOverflowPossible()
    {

    }
    
    template<class K = RedirectType>
    typename std::enable_if<std::is_same_v<uint32_t, K>, void>::type
    checkIfOverflowPossible()
    {
        if(arr.size() == UINT_MAX-1)
            throw std::runtime_error("TOO LARGE");
    }
};
```
Here we just completely remove the overflow possibility from all Hashtables that are considered big (using 64 bit redirection indicies). The std::enable_if function can be used on other conditions. Like before when std::optional was re-implemented, you can remove parts or the entire destructor if the object is trivially destructable. There are quite a few functions and not all of them are used in this hashmap so its worth looking into [them](https://en.cppreference.com/w/cpp/meta.html). 

Also a quick note, there are 2 things that aren't discussed here but they are valuable. First things first. being able to do something like this:
```C++
Hashmap<size_t, std::string> map = Hashmap<size_t, std::string>();
map[12] = "something I guess";
```
is highly convenient. This requires a few things though. You must be able to construct an element with an empty constructor (so you can insert an element with just a key) or you need some other approach that allows you to delay the insert of the object till after you set it to something. This is left to the implementer but std::unordered_map does not allow you to insert an element this way if the default constructor does not exist.

Secondly, Heterogenous lookup. Also leaving this up to the implementer but the idea is to allow other types than the Key to be used for searching and Deletion. Its great for Sets too when the Key isn't expected to change in a way that would mess with the hash. A typical example used is you have a database and an Entry in that database containing a variety of things. The first colummn is the actual Key though. Lets say its just an ID number. When inserted into our hashset/map, the key is the only part used in the hashing process since we gave it a somewhat custom hash function. Well if you want to search for that entry, you ideally would like to just give it an ID but with the current approach, you have to give it the entire entry. This results in a lot of extra waste since you have to create a mostly empty entry and you may not even be able to do that easily (say there are no empty constructors). Strings and String_views are often the other example used (and its more common to come across). Well the solution here is to allow Heterogenous Lookup.

This requires that both the hash function and the comparison function are marked as transparent. Looks something like this:
```C++

template<typename Hash, typename KeyEqual>
constexpr bool both_transparent_v = std::__is_transparent_v<Hash> && std::__is_transparent_v<KeyEqual>;

struct IdentityHash
{
    typename is_transparent = void;
    uint64_t operator()(const CustomType& v)
    {
        return v.key;
    }
    uint64_t operator()(const uint64_t& v)
    {
        return v; //must be the same as the above if you want them to hash correctly
    }
};
struct EqualsFunction
{
    typename is_transparent = void;
    bool operator()(const CustomType& v, const uint64_t& v2)
    {
        return v.key == v2;
    }
    bool operator()(const uint64_t& v2, const CustomType& v)
    {
        return v.key == v2;
    }

    //you could also just do this
    //works for all types provided there exists an overload for comparisons 
    // template<typename V1, typename V2>
    // bool operator()(const V1& v, const V2& v2)
    // {
    //     return v == v2;
    // }
};

template<typename Key, typename Value, typename HashFunction, typename KeyEqual, bool BIG>
class HashTable
{
public:
    template<typename P, typename H = HashFunction, typename KE = KeyEqual,
    std::enable_if_t<both_transparent_v<H, KE>, bool> = true>
    auto find(const P& p); //do normal find functions but allow any type that can be compared.
};

```
Notice that we need both to be transparent so we define a way to do this above. std::__is_transparent_v may not actually exist for you though. It may be called something different but it is possible to implement with existing meta programming functions. unordered_dense does this so if you need a more cross platform way of doing it, use that. Note that you don't really have to check if its transparent or not and can just assume you can do so if its your own code. It is required for standard containers for backwards compatibility but its not required for your own code especially considering we already broke standards with our delete function.


# Multimap
Assuming you want something like this, its pretty easy to add. You can just make a wrapper around this hashmap but use linked lists for the value. That way, you could easily delete from the linked list, references would stay valid, etc. The main issue being that you must make a wrapper and you introduce deletion and clear overhead which isn't quite desireable.

A simpler solution may be to just allow duplicates to exist in the hashmap which makes insert/emplace easier. Doing this automatically solves all of the problems except 1. Really it does. Rehashes always keep the same order, deletion just deletes the oldest item and you still have all of the exact same properties as before.... but about that 1 problem. How do you delete a specific duplicate key? This may not always be something desired but it is something that the standard library provides and something you'd expect out of other hashtables too. Solving this is not particularly that easy but nor is it that difficult.

There were 2 approaches to maintaining the hashtable based on what strategy was used for deletion. If you chose to keep tombstones, you'll have better iterator properties. Otherwise, you'll have worse iterator properties. 

Basically, we need a new Custom Iterator for our HashTable. There are 2 things that we want to do. We want the same functionality we currently have so being able to iterate over all added things in the hashmap regardless of key. We also want to be able to grab an iterator through find, insert/emplace and modify the raw values (note that Key should not be modified). Normally, the current iterators becomme invalid once the internal vector resizes so insert and remove may invalidate all iterators or just 2.

With new iterators, we can change those properties somewhat. The new iterators are invalidated on rehash only and invalidated on delete.... potentially. If backwards shifting is used, many iterators will be invalidated on erase and since hashes are intended to be pretty random, its unknown which iterators become invalid so assume all of them. Its possible that only one iterator is invalidated and its possible that all of them all invalidated.

If tombstones are used, erasure will only invalidate one iterator. Insert still has the same properties of invalidating hashes upon rehash. This better mimics std::unordered_map which invalidates iterators on rehash and only one iterator on erase. With that in mind, this is a custom iterator and the necessary changes for the hashtable to allow multi map/set while using backwards shift deletion

```C++
template<typename Key, typename Value, bool MULTI = false, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    class SimpleHashTable;

    template<typename Key, typename Value, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashMap = SimpleHashTable<Key, Value, false, HashFunc, KeyEqual, BIG>;

    template<typename Key, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashSet = SimpleHashTable<Key, void, false, HashFunc, KeyEqual, BIG>;

    template<typename Key, typename Value, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashMultiMap = SimpleHashTable<Key, Value, true, HashFunc, KeyEqual, BIG>;

    template<typename Key, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashMultiSet = SimpleHashTable<Key, void, true, HashFunc, KeyEqual, BIG>;

    //is_transparent stuff
    template<typename Hash, typename KeyEqual>
    constexpr bool both_transparent_v = std::__is_transparent_v<Hash> && std::__is_transparent_v<KeyEqual>;

    template<typename Key, typename Value, bool MULTI, typename HashFunc, typename KeyEqual, bool BIG>
    class SimpleHashBucketIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::conditional_t<std::is_same_v<void, Value>, Key, std::pair<Key, Value>>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        
        SimpleHashBucketIterator(){}
        SimpleHashBucketIterator(SimpleHashTable<Key, Value, MULTI, HashFunc, KeyEqual, BIG>* ptr, size_t bucketLocation)
        {
            tablePtr = ptr;
            this->bucketLocation = bucketLocation;
            if(bucketLocation != -1)
            {
                k = ptr->getKey(ptr->arr[ptr->redirectInfo[bucketLocation].second]);
            }
        }
        ~SimpleHashBucketIterator(){}

        auto operator++() -> SimpleHashBucketIterator&
        {
            tablePtr->getNext(*this);
            return *this;
        }
        reference operator*() const
        {
            return tablePtr->arr[tablePtr->redirectInfo[bucketLocation].second];
        }
        pointer operator->() const
        {
            return &tablePtr->arr[tablePtr->redirectInfo[bucketLocation].second];
        }

        bool operator==(const SimpleHashBucketIterator& other) const
        {
            return bucketLocation == other.bucketLocation;
        }
        
        bool operator!=(const SimpleHashBucketIterator& other) const
        {
            return bucketLocation != other.bucketLocation;
        }
    private:
        friend SimpleHashTable<Key, Value, MULTI, HashFunc, KeyEqual, BIG>;
        SimpleHashTable<Key, Value, MULTI, HashFunc, KeyEqual, BIG>* tablePtr = nullptr;
        uint64_t bucketLocation = -1;
        Key k;
    };



    template<typename Key, typename Value, bool MULTI, typename HashFunc, typename KeyEqual, bool BIG>
    class SimpleHashTable
    {
        using RedirectType = std::conditional_t<BIG, uint64_t, uint32_t>;
        using HashRedirectPair = std::pair<RedirectType, RedirectType>;
        using KeyValueType = std::conditional_t<std::is_same_v<void, Value>, Key, std::pair<Key, Value>>;
        using Iterator = SimpleHashBucketIterator<Key, Value, MULTI, HashFunc, KeyEqual, BIG>;
    public:
        template<typename P, typename H = HashFunc, typename KE = KeyEqual,
        std::enable_if_t<both_transparent_v<H, KE>, bool> = true>
        auto erase(const P& k)
        {
            return remove(find(k));
        }
        auto erase(const Key& k)
        {
            return remove(find(k));
        }
        auto erase(const Iterator& it)
        {
            return remove(it);
        }
        Iterator bucketEnd()
        {
            return Iterator(this, -1);
        }
    private:
        auto remove(const Iterator& it)
        {
            if(UNLIKELY(arr.size() == 0))
                return bucketEnd();
            if(it == bucketEnd())
                return bucketEnd();

            uint64_t location = it.bucketLocation;
            uint8_t currentPartialHash = getPartialHash(location);
            RedirectType currentHash = getPartialHashEx(location);
            
            //if found, find the location of the last item in arr and swap that with our current spot

            uint64_t lastSpotHash = hasher(getKey(arr.back()));
            uint8_t lastSpotPartialHash = extractPartialHash(lastSpotHash);
            RedirectType lastSpotExtraHash = extractPartialHashEx(lastSpotHash);
            uint64_t lastSpotLocation = lastSpotHash % fastHashInfo.size();
            
            //it exists so we can skip the extra work of checking free slots.
            //note that partialHash will ALWAYS have the valid bit set so you don't need to check if empty or free 
            //  as if the partialHash at that location is invalid, the valid bit won't be set which means it won't be equal
            while(true)
            {
                if(getPartialHash(lastSpotLocation) == lastSpotPartialHash) //fast path but 2 checks which may be unnecessary
                {
                    if(comparePartialHashEx(lastSpotLocation, lastSpotExtraHash))
                    {
                        if(getRedirectInfo(lastSpotLocation) == (arr.size()-1))
                            break;
                    }
                }
                lastSpotLocation = (lastSpotLocation+1) % fastHashInfo.size();
            }

            //set current location to be deleted
            fastHashInfo[location] = 0;
            
            //swap data and pop back which completes the deletion
            std::swap(arr[redirectInfo[lastSpotLocation].second], arr[redirectInfo[location].second]);

            //swap locations too
            redirectInfo[lastSpotLocation].second = redirectInfo[location].second;

            //extra step. shift data back till we hit an empty spot or we hit a node that is in its desired spot
            uint64_t previousLocation = location;
            location = (location+1) % fastHashInfo.size();

            uint64_t locationOfNextValidKey = SIZE_MAX;

            while(!getLocationEmpty(location))
            {
                if(locationOfNextValidKey == SIZE_MAX)
                {
                    if(getPartialHash(location) == currentPartialHash)
                        if(comparePartialHashEx(location, currentPartialHash))
                            if(getKey( arr[getRedirectInfo(location)] ) == it.k)
                                locationOfNextValidKey = location;
                }
                if(getDistanceFromDesiredSpot(location) > 0)
                {
                    if(locationOfNextValidKey == location)
                        locationOfNextValidKey = previousLocation;

                    fastHashInfo[previousLocation] = fastHashInfo[location];
                    redirectInfo[previousLocation] = redirectInfo[location];
                }
                else
                    break;

                previousLocation = location;
                location = (location+1) % fastHashInfo.size();
            }
            arr.pop_back();

            if(locationOfNextValidKey != SIZE_MAX)
                return Iterator(this, locationOfNextValidKey);
            return bucketEnd();
        }

        template<bool K = MULTI>
        typename std::enable_if<!K, bool>::type
        checkForDuplicate(uint64_t intendedLocation, uint8_t partialHash, RedirectType extraHash, const Key& key)
        {
            if(getPartialHash(intendedLocation) == partialHash) //fast path but 2 checks which may be unnecessary
            {
                if(comparePartialHashEx(intendedLocation, extraHash))
                {
                    if(LIKELY( keyEqualFunc(getKey(arr[getRedirectInfo(intendedLocation)]), key) ))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        //If its a multimap, insert / emplace is allowed to insert multiple copies of the same key.
        template<bool K = MULTI>
        typename std::enable_if<K, bool>::type
        checkForDuplicate(uint64_t intendedLocation, uint8_t partialHash, RedirectType extraHash, const Key& key)
        {
            return false;
        }

        
        template<bool K = MULTI>
        typename std::enable_if<K, void>::type
        getNext(Iterator& bucket)
        {
            //key stored in iterator
            if(UNLIKELY(arr.size() == 0))
                bucket.bucketLocation = -1;
            
            uint64_t actualHash = hasher(bucket.k);
            uint8_t partialHash = extractPartialHash(actualHash);
            RedirectType extraHash = extractPartialHashEx(actualHash);
            uint64_t location = bucket.bucketLocation+1;

            while(!getLocationEmpty(location))
            {
                if(getPartialHash(location) == partialHash) //fast path but 2 checks which may be unnecessary
                {
                    if(comparePartialHashEx(location, extraHash))
                    {
                        if(LIKELY( keyEqualFunc(getKey(arr[getRedirectInfo(location)]), bucket.k) ))
                        {
                            bucket.bucketLocation = location;
                            return;
                        }
                    }
                }
                location = (location+1) % fastHashInfo.size();
            }
            bucket.bucketLocation = -1;
        }

        template<bool K = MULTI>
        typename std::enable_if<!K, void>::type
        getNext(Iterator& bucket)
        {
            bucket.bucketLocation = -1;
        }
        
        friend Iterator;
    };
```
To keep things brief, the other changes are skipped but Insert, Emplace, etc. use checkForDuplicate now so that it can be removed if the table is a multi map/set. getNext() is the main important part where it will look for the next valid key modifying the iterator itself (though returning a new iterator is perfectly fine too. Probably better to do this). If it fails to find one, bucket location is set to -1 which will get cast to SIZE_MAX but it represents an invalid value. I chose to use a value that would always be invalid regardless of how the underlying data structure changes.

Again, its important to note that if you choose to use full tombstones, iterators continue to be valid even after erase and with a few modifications, its possible to have more stable iterators that could sorta survive rehash. In order to survive rehashing, the iterator must store the redirection info. That means that rehashes do not completely break the iterator. You may not use that iterator for anything other than dereferencing data so no deleting or stepping forward as the bucket location is invalid. This remains true regardless of deletion strategy.

Note that the standard invalidates iterators all on rehash and 1 iterator on erase. With tombstones, we have the same guarantees and with backwards shift deletion, we have slightly worse ones. Just as long as the programmer is aware of this, they can design code that is robust against iterator invalidation. Not knowing if an iterator is invalid is problematic but this is also on par with what std::unordered_map provides.

Deletion in a multimap is simple. It follows a similar way with how [std::unordered_multimap](https://en.cppreference.com/w/cpp/container/unordered_multimap/erase.html) deletes things
```C++
int main()
{
    SimpleHashMultiMap<int, std::string> map = 
    {
        {1, "one"}, {1, "two"}, {1, "three"},
        {4, "four"}, {5, "five"}, {6, "six"}
    };

    // Iterate over all keys that are one
    for (auto it = map.find(1); it != map.bucketEnd();)
    {
        if (it->second != "two")
            it = map.erase(it);
        else
            ++it;
    }

    //table should only have { {1, "two"}, {4, "four"}, {5, "five"}, {6, "six"} }
    return 0;
}
```


# The ACTUAL Implemented Multimap
With all of the above work and after testing, I found that the map performs identical to the normal version when you only insert unique values. When you insert a lot of duplicates, the speed is quite slow. Now its still faster than std::unordered_multimap. In fact its like 3x faster with instant deletion but what I noticed is that if you used the normal non multimap version with std::list and just take care to insert into the list when you find the key, it was significantly faster with worse but very tolerable delete speeds.

There is an optimization you can take that requires extra information to be stored. For each bucket, store how many values wanted to be in that specific spot (must also adjust this on delete). That way when inserting, you can skip that many slots avoiding a lot of extra checks. This helped but it ultimately couldn't compete with just using std::list.

With this in mind, I sought out to make an implementation of multimap and to reduce duplicate code, just add it as apart of the normal table implementation. Not much code will be shown as the final implementation has a lot of meta programming functions but in order to achieve high speed, some significant changes were needed.

```C++
template<typename Key, typename Value, bool MULTI, typename HashFunc, typename KeyEqual, bool BIG>
class SimpleHashTable
{
public:
    using RedirectType = std::conditional_t<BIG, uint64_t, uint32_t>;
    using HashRedirectPair = std::pair<RedirectType, RedirectType>;
    using KeyValueType = std::conditional_t<std::is_same_v<void, Value>, Key, std::pair<Key, Value>>;
    using KVStorageType = std::conditional_t<MULTI, std::list<KeyValueType>, KeyValueType>;
    using Iterator = SimpleHashTableIterator<Key, Value, MULTI, HashFunc, KeyEqual, BIG>;

    //STUFF

private:

    //STUFF

    friend SimpleHashTableIterator<Key, Value, MULTI, HashFunc, KeyEqual, BIG>;

    static const uint8_t VALID_BIT = 0x80;
    const float MaxLoadBalance = 0.80;

    std::vector<uint8_t> fastHashInfo; //0x00 == empty. 0x7F == deleted (only first bit empty)
    std::vector<HashRedirectPair> redirectInfo; //redirect info + stored hash
    std::vector<KVStorageType> arr;
    std::vector<Key> extraKeyStorage;

    //Typically in sync with arr but for a multimap, must also keep track of all the elements in each list. Ideally, size() = O(1)
    size_t totalElements = 0;
    uint64_t rehashCounter = 0;

    HashFunc hasher;
    KeyEqual keyEqualFunc;
};
```

Take notice that we have a custom iterator again but this one is not the same as the previous one. Lets ignore it for now and come back to it. The main thing of note is the type KVStorageType, extraKeyStorage, totalElements, and rehashCounter.

KVStorageType lets us store a std::list of the elements or just the raw elements directly. Then extraKeyStorage exists. This is for speed. When using a multimap, having to go into the list and dereference a pointer to get a key is slower than just getting the key directly so the Multimap will need to store an additional key for each unique key. This could be removed if performance is less of a concern. The performance difference is not bad either. Its not really worse than inserting only unique elements in testing though remember that benchmark isn't amazing.

totalElements is required now as the total elements actually inserted is no longer just the number of elements in the data array. It could be higher if multiple of the same key are stored.

rehashCounter is unique and only used for that custom iterator. Lets look at that custom iterator.

```C++
template<typename Key, typename Value, bool MULTI, typename HashFunc, typename KeyEqual, bool BIG>
struct SimpleHashTableIterator
{
private:
    using RedirectType = std::conditional_t<BIG, uint64_t, uint32_t>;
    using HashRedirectPair = std::pair<RedirectType, RedirectType>;
    using KeyValueType = std::conditional_t<std::is_same_v<void, Value>, Key, std::pair<Key, Value>>;

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::conditional_t<std::is_same_v<void, Value>, Key, std::pair<Key, Value>>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    SimpleHashTableIterator(){}

    SimpleHashTableIterator(SimpleHashTable<Key, Value, true, HashFunc, KeyEqual, BIG>* ptr, size_t index, bool all)
    {
        this->all = all;
        this->ptr = ptr;
        this->index = index;
        if(ptr != nullptr)
        {
            this->rehashCounter = ptr->rehashCounter;
            
            if(index < ptr->size())
                listIterator = ptr->arr[index].begin();
        }
    }
    SimpleHashTableIterator(SimpleHashTable<Key, Value, false, HashFunc, KeyEqual, BIG>* ptr, size_t index, bool all)
    {
        this->all = all;
        this->ptr = ptr;
        this->index = index;
        if(ptr != nullptr)
        {
            this->rehashCounter = ptr->rehashCounter;
        }
    }
    
    SimpleHashTableIterator(SimpleHashTable<Key, Value, MULTI, HashFunc, KeyEqual, BIG>* ptr, size_t index, bool all, typename std::list<KeyValueType>::iterator desiredListIterator)
    {
        this->all = all;
        this->ptr = ptr;
        this->index = index;
        this->listIterator = desiredListIterator;
        if(ptr != nullptr)
        {
            this->rehashCounter = ptr->rehashCounter;
        }
    }

    ~SimpleHashTableIterator(){}

    //More Implementation
    
private:
    //More Implementation
    friend SimpleHashTable<Key, Value, MULTI, HashFunc, KeyEqual, BIG>;

    bool all = false;
    SimpleHashTable<Key, Value, MULTI, HashFunc, KeyEqual, BIG>* ptr = nullptr;
    uint64_t index;
    
    typename std::list<KeyValueType>::iterator listIterator;

    //Allowing deletion of a specific element fast
    uint64_t rehashCounter;
    uint64_t bucketIndex = -1;
};
```
Being a bit brief, notice how the iterator stores a pointer to the original object and the information needed to reference an item. This results in slower iterators but they remain more stable.

listIterator is stored here but completely unused when you aren't working with multimaps. The addition of the all variable also allows the iterator (when configured correctly) to iterate over all elements over all list. Though in many cases, it will just iterate over the selected bucket.

Also rehashCounter and bucketIndex are stored here too. You'd think if we use bucketIndex, surely this would be invalidated on rehash and you'd be kinda correct. This only makes the bucketIndex invalid though and we can check that if you compare the stored rehashCounter against the current rehashCounter.

That bucketIndex is only used in deletion so it could be fast. If a rehash for some reason occurs and the iterator is still valid, you should still be able to delete based on the iterator so if their rehashCounters are off sync, the bucketIndex will be recomputed keeping most things the same. Also note that when incrementing any iterator, the bucketIndex will not be valid as they don't get updated so in those cases, the bucket index will need to be solved for again anyways.

Note that this is only a forward iterator but nothing stops this from being a bidirectional iterator.

So that is everything that changes right? Well I discovered that I don't actually benefit as much from some of the implementation approaches I took so I'll just highlight the changes to function signatures and what changed about the function if they aren't changed too much

```C++
public:
    template<typename ...Args>
    auto try_insert(const Key& key, Args&&... args)
    {
        return try_emplace(key, std::forward<Args>(args)...);
    }

    template<typename ...Args>
    auto try_insert(Key&& key, Args&&... args)
    {
        return try_emplace(std::move(key), std::forward<Args>(args)...);
    }

    auto emplace(KeyValueType&& v){}
        
private:
    template<bool M = MULTI, typename... Args>
    typename std::enable_if<M, void>::type
    attemptToAdd(Args&&... args)
    {
        arr.emplace_back(std::list<KeyValueType>());
        arr.back().emplace_back(std::forward<Args>(args)...);
        extraKeyStorage.push_back(getKey(arr.back()));
    }

    template<bool M = MULTI, typename... Args>
    typename std::enable_if<!M, void>::type
    attemptToAdd(Args&&... args)
    {
        arr.emplace_back(std::forward<Args>(args)...);
    }
    
    template<bool M = MULTI, typename... Args>
    typename std::enable_if<M, Iterator>::type
    appendMultimap(uint64_t intendedLocation, Args&&... v)
    {
        uint64_t actualLocation = getRedirectInfo(intendedLocation);
        arr[actualLocation].emplace_back(std::forward<Args>(v)...);
        totalElements++;
        
        Iterator returnIt = Iterator(this, actualLocation, false, std::prev(arr[actualLocation].end()));
        returnIt.bucketIndex = intendedLocation;
        return returnIt;
    }

    template<bool M = MULTI, typename... Args>
    typename std::enable_if<!M, Iterator>::type
    appendMultimap(uint64_t intendedLocation, Args&&... v)
    {
        uint64_t actualLocation = getRedirectInfo(intendedLocation);
        Iterator returnIt = Iterator(this, actualLocation, false);
        returnIt.bucketIndex = intendedLocation;
        return returnIt;
    }
```
The main functions to pay attention to though many function are changed / adjusted even if only slightly. Emplace no longer takes in any potential values and therefore no longer needs to insert into the data array first before getting a key. This puts it on par with try_emplace in terms of performance and makes multimaps faster by avoiding unnecessary memory allocations. try_insert was added as a means to allow the same type of functionality though I can't really think of a scenario where I needed emplace to take in generic data types (I'm sure they exist).

extraKeyStorage is only needed in multimaps and you have to actually append to those list in multimaps so functions are added for those than can be disabled / adjusted where necessary.

The result? A multimap that is, at least according to benchmarks, 6x faster than std::unordered_multimap where you factor in creation time, clear time, and random insert time with duplicates. unordered_dense also does this but its not wrapped up in a neat package for multimaps. You'd need to add this manually.

# Conclusions
Unordered_dense is basically what we created here though its a little different. unordered_dense is certainly more polished and you should expect it to run a bit faster. An actually complete version of the hashmap this discusses is here in this repository and is also to be featured in my own library I use for C++ development [SMPL](https://github.com/Davidah121/SMPL). The objective here wasn't to beat unordered_dense or anything but to see how far you can go and understand WHY its like this.

With this hashmap, you have the choice of different iterator strategies depending on implementation. If you choose to ignore multimaps, iterators are invalidated whenever the internal vector storing elements has to resize and on erasure of any element, the last element in the internal vector also becomes invalid.

If you choose to have multimaps but use linked list, the same rules apply. If you implement multimaps in place and have tombstones, iterators are invalid upon rehash and only the element being deleted is invalidated when you erase something. If you use backwards shifting, rehashes invalidates all iterators and erasure invalidates many and potentially all valid iterators too.

While none of these strategies are perfect, they aren't too far away from what std::unordered_map and std::unordered_multimap offer. Tombstones do offer what I would consider the best iterator properties matching what std::unordered_map and std::unordered_multimap offer and since you can control rehashing by setting the total number of buckets, you have more control over when your iterators become invalid.

Another note, without backwards shifting, its possible to trigger rehashing simply due to all the excess tombstones. You'd need to clean those up or take note at the rehashing stage that you don't need to double the number of buckets. This means its possible to rehash multiple times doing sommething like this (thanks again google):
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

The trade off of deletion strategy comes in the form of making insert,find,rehash slower while keeping more stable iterators on delete or making deletion slower with iterators that aren't stable at all upon deletion. Deletion with backwards shifting can hit O(N) easier if you are using a multimap and have many of the same key present but with tombstones, it is easier to hit O(1)

Enough talking. Benchmarks

Disclaimer: These numbers will vary upon hardware, OS, and what you are doing on your system and shouldn't be taken as if they are hard truth values but instead of how they are relative to each other for the specific task

### These benchmarks used std::string as the key and a 32 byte structure that is trivially destructable as its value
| Hashmap Name       | Clear Time | In order Insert | Random Insert   | Search    | Remove     |
|--------------------|------------|-----------------|-----------------|-----------|------------|
| std::unordered_map | 0.062620650 | 0.167738690      | 0.074343670      | 0.000000300 | 0.001932380 |
| smpl::SimpleHashTable (no backwards shift) |  0.001955050  | 0.074587850 | 0.064974890  | 0.000000193 | 0.000934600 |
| smpl::SimpleHashTable | 0.002003420 | 0.072411350      | 0.062643930      | 0.000000197 | 0.001016310 |
| ankerl::unordered_dense | 0.002513210 | 0.067642640      | 0.082310160      | 0.000000159 | 0.000807790 |
| ankerl::unordered_dense_segmented | 0.003142450 | 0.066112510      | 0.058892820      | 0.000000203 | 0.001060870 |
| robinhood::node_map | 0.012487270 | 0.110946770      | 0.071795110      | 0.000000207 | 0.001516520 |
| robinhood::unordered_flat_map | 0.005814300 | 0.113954350      | 0.054560710      | 0.000000129 | 0.001029440 |



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

### These benchmarks used size_t as the key and a 32 byte structure that is trivially destructable as its value
| Hashmap Name       | In Order Insert Clear Time | Random Insert Clear Time | In order Insert | Random Insert   | Search    | Remove     |
|--------------------|----------------------------|--------------------------|-----------------|-----------------|-----------|------------|
| std::unordered_multimap | 0.058221820           | 0.081144950              | 0.117551020     | 0.860157620     | 0.000000142 | 0.000000164|
| smpl::SimpleHashMultiMap | 0.023494120          | 0.075687500              | 0.077554960     | 0.059063060     | 0.000000129 | 0.000000150|
| smpl::SimpleHashMultiMap (Inplace insertion) | 0 | 0                       | 0.071669810     | 0.276171670     | N/A         | N/A        |


These benchmarks are identical to the above except that deletion doesn't quite work the same way so its not quite fair to compare the inplace version with the other 2. Deletion for unordered_map removes ALL duplicate keys. SimpleHashMultiMap does the same but the inplace insertion method does not do this. Also note that even though the inplace insertion solution has faster search time, the iteration time is going to be slower based on how data was inserted into the hashmap itself (along with iterators having different properties).

Here, both multimap still out do the std::unordered_multimap however, the search and remove times are quite comparable. They fluctuate so running with more iterations is necessary to remove testing variance. I changed to 100 iterations to test just search and remove though more is likely needed.

Note that making a psuedo multimap with unordered_dense is slightly faster but is lacking a custom iterator implementation. Also these are using size_t as the key so clear times are instant / near instant. If you use a non trivially destructable key like the previous testing, deletion is no longer instant 


As you can tell, all of these hashmaps (which can all be sets too) are faster and often by a lot. If you can deal with some of the quirks associated with them, its worth trying them out. Otherwise, with all the things covered, you can surely change the internal data structures in the hashmap provided and get back some of those comforts you wish for. 

If you want better benchmarks, check these 2 out:
- [Benchmark 2019](https://martin.ankerl.com/2019/04/01/hashmap-benchmarks-01-overview/)
- [Benchmark 2022](https://martin.ankerl.com/2022/08/27/hashmap-bench-01/#result-analysis)

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

This value may be larger or smaller depending on implementation and things outside of the programmers control but the general problem is that its almost 2x bigger than our implementation and that Balanced Binary Search Tree. That is a huge overhead compared to just storing the data directly (which is 40 MBytes if tightly fit). We are using 1 million elements so its not as bad. If you use a lot less elements its perfectly fine and its terrible if you are using far more.

This should cover everything I set out to do. As for me, I'm satisfied. I even got to use the complicated C++ Meta programming stuff and I got to create an amazing hashmap that I'll personally be using in the future.

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
- [std::unordered_multimap::erase](https://en.cppreference.com/w/cpp/container/unordered_multimap/erase.html)