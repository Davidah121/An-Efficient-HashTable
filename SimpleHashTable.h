#pragma once
#include "ImportantInclude.h"
#include <bitset>
#include <climits>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <__functional/is_transparent.h>

namespace smpl
{
    template<typename Key, typename Value, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    class SimpleHashTable;

    template<typename Key, typename Value, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashMap = SimpleHashTable<Key, Value, HashFunc, KeyEqual, BIG>;

    template<typename Key, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashSet = SimpleHashTable<Key, void, HashFunc, KeyEqual, BIG>;

    //is_transparent stuff
    template<typename Hash, typename KeyEqual>
    constexpr bool both_transparent_v = std::__is_transparent_v<Hash> && std::__is_transparent_v<KeyEqual>;


    template<typename Key, typename Value, typename HashFunc, typename KeyEqual, bool BIG>
    class SimpleHashTable
    {
        using RedirectType = std::conditional_t<BIG, uint64_t, uint32_t>;
        using HashRedirectPair = std::pair<RedirectType, RedirectType>;
        using KeyValueType = std::conditional_t<std::is_same_v<void, Value>, Key, std::pair<Key, Value>>;

    public:

        /**
         * @brief Construct a new Hash Table.
         *      Note that whether it is a map or set depends on what template parameters are set.
         *          To create a set, Set the Value template parameter to void
         *      The table is completely empty with no memory allocated yet so everything is in an invalid state.
         */
        SimpleHashTable(){}

        /**
         * @brief Construct a new Hash Table
         *      Initializes the hash data table to an initial size of buckets.
         *      Useful to avoid rehashing (or multiple rehashing) when you have a known minimum/maximum size of elements to go into the table.
         *
         *      Note that whether it is a map or set depends on what template parameters are set.
         *          To create a set, Set the Value template parameter to void
         * 
         * @param initSize 
         *      This value should be more than the total number of items to go into the table to avoid rehashing
         *      Rehashing happens at 80% full
         *          To avoid Rehashing buckets should be set to totalElements*1.2 at least.
         *          Note that with more buckets, collisions are less likely resulting in faster performance at the cost of memory
         */
        SimpleHashTable(size_t initSize)
        {
            fastHashInfo = std::vector<uint8_t>(initSize);
            redirectInfo = std::vector<HashRedirectPair>(initSize);
        }

        /**
         * @brief Destroy the Hash Table.
         * 
         */
        ~SimpleHashTable()
        {
        }
        
        /**
         * @brief Copy Construct a new Hash Table object
         * 
         * @param other 
         */
        SimpleHashTable(SimpleHashTable<Key, Value>& other)
        {
            arr = other.arr;
            fastHashInfo = other.fastHashInfo;
            redirectInfo = other.redirectInfo;
            MaxLoadBalance = other.MaxLoadBalance;
        }
        /**
         * @brief Copy Assign a new Hash Table object
         * 
         * @param other 
         */
        void operator=(SimpleHashTable<Key, Value>& other)
        {
            arr = other.arr;
            fastHashInfo = other.fastHashInfo;
            redirectInfo = other.redirectInfo;
            MaxLoadBalance = other.MaxLoadBalance;
        }

        /**
         * @brief Move Construct a new Hash Table object
         *      Note that "other" will be invalidated by this
         * 
         * @param other 
         */
        SimpleHashTable(SimpleHashTable<Key, Value>&& other) noexcept
        {
            arr = std::move(other.arr);
            fastHashInfo = std::move(other.fastHashInfo);
            redirectInfo = std::move(other.redirectInfo);
            MaxLoadBalance = other.MaxLoadBalance;
        }
        
        /**
         * @brief Move Assign a new Hash Table object
         *      Note that "other" will be invalidated by this
         * 
         * @param other 
         */
        void operator=(SimpleHashTable<Key, Value>&& other) noexcept
        {
            arr = std::move(other.arr);
            fastHashInfo = std::move(other.fastHashInfo);
            redirectInfo = std::move(other.redirectInfo);
            MaxLoadBalance = other.MaxLoadBalance;
        }

        /**
         * @brief Clears the hash table.
         * 
         */
        void clear()
        {
            fastHashInfo.clear();
            redirectInfo.clear();
            arr.clear();
        }

        //enable if map meaning that Value is not void.

        /**
         * @brief Attempts to either find the provided key or emplace an object with that key
         *      and return a reference to it.
         * 
         *      Is not enabled for Sets and does not work if the object isn't constructible from an empty constructor.
         *
         * @param k 
         * @return Value& 
         */
        template<typename Q = Value, std::enable_if_t<!std::is_same_v<void, Q>, bool> = true>
        Value& operator[](const Key& k)
        {
            return try_emplace(k)->second;
        }

        //enable if map meaning that Value is not void. Also need the KeyEquals and HashFunc to be transparent
        //  (accepts types that aren't just the Key and that they hash the same way along with the ability to compare them correctly)

        /**
         * @brief Attempts to either find the provided key or emplace an object with that key
         *      and return a reference to it.
         * 
         *      Is not enabled for Sets and does not work if the object isn't constructible from an empty constructor.
         *      Note that this version requires that the HashFunction and KeyEquals function are also transparent
         *          Meaning that you are allowing different types other than the actual Key type to be used for hashing and comparison.
         * 
         * @tparam P 
         * @param k 
         * @return Value& 
         */
        template<typename P, typename Q = Value, typename H = HashFunc, typename KE = KeyEqual,
        std::enable_if_t<!std::is_same_v<void, Q> && both_transparent_v<H, KE>, bool> = true>
        Value& operator[](P&& k)
        {
            return try_emplace(std::forward<P>(k))->second;
        }


        /**
         * @brief Attempts to insert into the hash table.
         *      KeyValueType is either the Key or std::pair<Key, Value> depending on if its a Set or Map.
         *      Returns an iterator to either the newely constructed element or an existing element with the specified key.
         * 
         * @param v 
         * @return auto 
         */
        auto insert(const KeyValueType& v)
        {
            return emplace(v);
        }

        /**
         * @brief Attempts to insert into the hash table.
         *      KeyValueType is either the Key or std::pair<Key, Value> depending on if its a Set or Map.
         *      Returns an iterator to either the newely constructed element or an existing element with the specified key.
         * 
         * @param v 
         * @return auto 
         */
        auto insert(KeyValueType&& v)
        {
            return emplace(std::move(v));
        }

        //If its possible to construct from P, this is allowed.

        /**
         * @brief Attempts to insert into the hash table.
         *      Enabled if its possible to construct KeyValueType (the Key or std::pair<Key, Value>) from the templated type P
         * 
         * @param v 
         * @return auto 
         */
        template<class P, std::enable_if_t<std::is_constructible_v<KeyValueType, P&&>, bool> = true>
        auto insert(P&& v)
        {
            return emplace(std::forward<P>(v));
        }

        /**
         * @brief Emplaces into the hash table.
         *      This will construct a new element with the given arguments in place.
         *          Note that this will always construct a new element first and will remove it if an element is found that
         *          uses the specified key.
         *      May rehash. If you exceed 80% fill rate, it will rehash.
         *          Note that a rehash does not invalidate any iterator references however, if the internal data structure
         *          (std::vector) resizes, it will invalidate all iterators.
         * 
         * @tparam Args 
         * @param v 
         * @return auto 
         */
        template<typename... Args>
        auto emplace(Args&&... v)
        {
            if(fastHashInfo.size() == 0)
            {
                fastHashInfo = std::vector<uint8_t>(1024);
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
            uint64_t intendedLocation = actualHash % fastHashInfo.size();
            uint64_t distanceFromDesired = 0;
            while(!getLocationEmpty(intendedLocation))
            {
                if(getPartialHash(intendedLocation) == partialHash) //fast path but 2 checks which may be unnecessary
                {
                    if(comparePartialHashEx(intendedLocation, extraHash))
                    {
                        if(LIKELY( keyEqualFunc(getKey(arr[getRedirectInfo(intendedLocation)]), key) ))
                        {
                            arr.pop_back();
                            return arr.begin() + getRedirectInfo(intendedLocation);
                        }
                    }
                }

                intendedLocation = (intendedLocation+1) % fastHashInfo.size();
            }
            
            fastHashInfo[intendedLocation] = partialHash;
            redirectInfo[intendedLocation] = {actualHash, arr.size()-1};

            float currentLoadBalance = (float)arr.size() / (float)fastHashInfo.size();
            if(currentLoadBalance > MaxLoadBalance)
            {
                //re-balance
                rebalance();
            }

            return arr.begin() + (arr.size()-1);
        }
        
        /**
         * @brief Attempts to find an element by P comparing it to it an element's Key.
         *      If it exists, returns an iterator to it. Otherwise returns an iterator to the end of the hash table.
         *      Enabled if HashFunc and KeyEqual are both transparent and its possible to compare P to Key.
         *          Meaning that you are allowing different types other than the actual Key type to be used for hashing and comparison.
         *
         * @tparam P 
         * @param p 
         * @return auto 
         */
        template<typename P, typename H = HashFunc, typename KE = KeyEqual,
        std::enable_if_t<both_transparent_v<H, KE>, bool> = true>
        auto find(const P& p)
        {
            return search(p);
        }

        /**
         * @brief Attempts to find an element by its Key.
         *      If it exists, returns an iterator to it. Otherwise returns an iterator to the end of the hash table.
         *      
         *      Note that these iterators may be invalidated by erase() or insert()
         * 
         * @param k 
         * @return auto 
         */
        auto find(const Key& k)
        {
            return search(k);
        }
        

        /**
         * @brief Attempts to find an element by P comparing it to an element's Key and remove it. 
         *      Enabled if HashFunc and KeyEqual are both transparent and its possible to compare P to Key.
         *          Meaning that you are allowing different types other than the actual Key type to be used for hashing and comparison.
         *
         *      Note that if any deletion happens, more than one iterator will be invalidated.
         *          The iterator pointing to this element becomes invalid and the iterator pointing to the last element added
         *          This results in faster deletions at the cost of erase potentially causing issues.
         * 
         * @tparam P 
         * @param k 
         */
        template<typename P, typename H = HashFunc, typename KE = KeyEqual,
        std::enable_if_t<both_transparent_v<H, KE>, bool> = true>
        void erase(const P& k)
        {
            remove(k);
        }

        /**
         * @brief Attempts to find an element by its Key and remove it.
         *      Note that if any deletion happens, more than one iterator will be invalidated.
         *          The iterator pointing to this element becomes invalid and the iterator pointing to the last element added
         *          This results in faster deletions at the cost of erase potentially causing issues.
         * 
         * @param k 
         * @return auto 
         */
        void erase(const Key& k)
        {
            remove(k);
        }

        /**
         * @brief Get the Total number of buckets allocated
         *      For reference, A bucket takes up 9 bytes if its not a big hash table.
         *          BIG is not set in the template definition.
         *          Otherwise it is 17 bytes
         *          One byte for fast checking, 4-8 bytes for full hash. 4-8 bytes for redirection pointer
         * 
         * @return uint64_t 
         */
        uint64_t getTotalBuckets()
        {
            return fastHashInfo.size();
        }

        /**
         * @brief Gets the total number of elements added.
         *      Not the same as the total number buckets but insteads its all of the things you've added.
         *      Erasing directly affects this.
         * 
         * @return uint64_t 
         */
        uint64_t size()
        {
            return arr.size();
        }

        /**
         * @brief Forces a rebalance of the hashmap.
         *      This will resize the total number of buckets (typically increasing them) resulting in a hash map with
         *      better performance for searching but requires more memory.
         *          If you have removed a lot from your hashmap, forcing a rehash may result in less total buckets if
         *          it would not affect performance (so like a reduction of 2x or more is required for this to happen).
         * 
         */
        void forceRehash()
        {
            rebalance();
        }

        /**
         * @brief Returns an iterator to the begining of the elements.
         *      Note that this is not related to the buckets but insteads its everything you added.
         *          its just std::vector::begin()
         * 
         *      Allows 
         * @return auto 
         */
        auto begin()
        {
            return arr.begin();
        }
        
        /**
         * @brief Returns an iterator to the end of the elements.
         *      Note that this is not related to the buckets but insteads its everything you added.
         *          its just std::vector::end()
         * 
         * @return auto 
         */
        auto end()
        {
            return arr.end();
        }

    private:

        template<typename... Args>
        void attemptToAdd(Args&&... args)
        {
            arr.emplace_back(args...);
        }

        template<typename K, typename... Args>
        auto try_emplace(K&& key, Args&&... args)
        {
            if(fastHashInfo.size() == 0)
            {
                fastHashInfo = std::vector<uint8_t>(1024);
                redirectInfo = std::vector<HashRedirectPair>(1024);
            }
            
            //extra check needed if and only if its possible to overflow
            //does nothing if BIG is enabled. Otherwise throws an exception
            checkIfOverflowPossible();
            uint64_t actualHash = hasher(key);

            uint8_t partialHash = extractPartialHash(actualHash); //must replace top bit so its considered valid
            RedirectType extraHash = extractPartialHashEx(actualHash);
            uint64_t intendedLocation = actualHash % fastHashInfo.size();
            uint64_t distanceFromDesired = 0;
            while(!getLocationEmpty(intendedLocation))
            {
                if(getPartialHash(intendedLocation) == partialHash) //fast path but 2 checks which may be unnecessary
                {
                    if(comparePartialHashEx(intendedLocation, extraHash))
                    {
                        if(LIKELY( keyEqualFunc(getKey(arr[getRedirectInfo(intendedLocation)]), key) ))
                        {
                            arr.pop_back();
                            return arr.begin() + getRedirectInfo(intendedLocation);
                        }
                    }
                }

                intendedLocation = (intendedLocation+1) % fastHashInfo.size();
            }
            
            //you are required to have a default constructor for this to work. Consistent with other hashtables
            attemptToAdd(key, args...);

            fastHashInfo[intendedLocation] = partialHash;
            redirectInfo[intendedLocation] = {actualHash, arr.size()-1};

            float currentLoadBalance = (float)arr.size() / (float)fastHashInfo.size();
            if(currentLoadBalance > MaxLoadBalance)
            {
                //re-balance
                rebalance();
            }

            return arr.begin() + (arr.size()-1);
        }

        template<typename P>
        auto search(const P& k)
        {
            if(UNLIKELY(arr.size() == 0))
                return arr.end();
            
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
                            return arr.begin() + getRedirectInfo(location);
                        }
                    }
                }
                location = (location+1) % fastHashInfo.size();
            }
            return arr.end();
        }

        template<typename P>
        void remove(const P& k)
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

        void rebalance()
        {
            std::vector<uint8_t> newHashInfo = std::vector<uint8_t>(fastHashInfo.size()*2);
            std::vector<HashRedirectPair> newRedirectInfo = std::vector<HashRedirectPair>(fastHashInfo.size()*2);

            for(size_t i=0; i<fastHashInfo.size(); i++)
            {
                if(!getLocationEmpty(i))
                    specialInsert(i, newHashInfo, newRedirectInfo);
            }

            fastHashInfo = std::move(newHashInfo);
            redirectInfo = std::move(newRedirectInfo);
        }

        void specialInsert(uint64_t nodeLocation, std::vector<uint8_t>& newHashInfo, std::vector<HashRedirectPair>& newRedirectInfo)
        {
            //does not create new memory nor recompute hash
            RedirectType storedHash = getPartialHashEx(nodeLocation);
            uint64_t hashLocation = storedHash % newHashInfo.size();
            while(!getLocationEmpty(hashLocation, newHashInfo))
            {
                hashLocation = (hashLocation + 1) % newHashInfo.size();
            }
            
            //cut down hash info to 7 bits.
            newHashInfo[hashLocation] = fastHashInfo[nodeLocation];
            newRedirectInfo[hashLocation] = redirectInfo[nodeLocation];
        }

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
            return fastHashInfo[loc] == 0;
        }
        constexpr bool getLocationEmpty(size_t loc, std::vector<uint8_t>& externalArr)
        {
            return externalArr[loc] == 0;
        }

        constexpr uint8_t getPartialHash(size_t loc)
        {
            return fastHashInfo[loc];
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

        constexpr RedirectType getDistanceFromDesiredSpot(size_t loc)
        {
            RedirectType hash = getPartialHashEx(loc);
            uint64_t desiredLocation = hash % fastHashInfo.size();
            return (loc >= desiredLocation) ? (loc - desiredLocation) : (loc+fastHashInfo.size() - desiredLocation);
        }

        constexpr uint8_t extractPartialHash(uint64_t hash)
        {
            uint64_t temp = rapid_mix(hash, std::uint64_t{0x9ddfea08eb382d69});
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
        getKey(const KeyValueType& v)
        {
            return v;
        }

        //Value
        template<class K = KeyValueType>
        typename std::enable_if<std::is_same_v<Key, K>, const Value&>::type
        getValue(const KeyValueType& v)
        {
            return v;
        }

        //std::pair<Key, Value>
        template<class K = KeyValueType>
        typename std::enable_if<!std::is_same_v<Key, K>, const Key&>::type
        getKey(const KeyValueType& v)
        {
            return v.first;
        }

        //std::pair<Key, Value>
        template<class K = KeyValueType>
        typename std::enable_if<!std::is_same_v<Key, K>, const Value&>::type
        getValue(const KeyValueType& v)
        {
            return v.second;
        }
        

        static const uint8_t VALID_BIT = 0x80;
        const float MaxLoadBalance = 0.8;

        std::vector<uint8_t> fastHashInfo; //0x00 == empty. 0x7F == deleted (only first bit empty)
        std::vector<HashRedirectPair> redirectInfo; //redirect info + stored hash
        std::vector<KeyValueType> arr;
        HashFunc hasher;
        KeyEqual keyEqualFunc;

    };

}