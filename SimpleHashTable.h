#pragma once
#include "ImportantInclude.h"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <initializer_list>
#include <list>


#ifndef LIKELY
#define LIKELY(x)      __builtin_expect(!!(x), 1)
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)
#endif

//is_transparent stuff
template<typename Hash, typename KeyEqual>
constexpr bool both_transparent_v = std::__is_transparent_v<Hash> && std::__is_transparent_v<KeyEqual>;

namespace smpl
{
    template<typename Key, typename Value, bool MULTI, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    class SimpleHashTable;

    template<typename Key, typename Value, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashMap = SimpleHashTable<Key, Value, false, HashFunc, KeyEqual, BIG>;

    template<typename Key, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashSet = SimpleHashTable<Key, void, false, HashFunc, KeyEqual, BIG>;

    template<typename Key, typename Value, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashMultiMap = SimpleHashTable<Key, Value, true, HashFunc, KeyEqual, BIG>;

    template<typename Key, typename HashFunc = TestHashFunction<Key>, typename KeyEqual = std::equal_to<Key>, bool BIG = false>
    using SimpleHashMultiSet = SimpleHashTable<Key, void, true, HashFunc, KeyEqual, BIG>;
	
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

		template<bool M = MULTI>
		typename std::enable_if_t<M, SimpleHashTableIterator&>
		operator++()
		{
			bool listIteratorAtEnd = false;
			if(listIterator != ptr->arr[index].end())
			{
				++listIterator;
				if(listIterator == ptr->arr[index].end())
					listIteratorAtEnd = true;
			}
			else
				listIteratorAtEnd = true;

			if(listIteratorAtEnd)
			{
				if(all)
				{
					index++;
					if(index < ptr->arr.size())
						listIterator = ptr->arr[index].begin();
					else
					 	*this = ptr->end();
				}
				else
					*this = ptr->end();
			}
            bucketIndex = -1;
			return *this;
		}
		
		template<bool M = MULTI>
		typename std::enable_if_t<!M, SimpleHashTableIterator&>
		operator++()
		{
			index++;
            bucketIndex = -1;
			return *this;
		}

		
		template<bool M = MULTI>
		typename std::enable_if_t<M, reference>
		operator*() const
		{
			return listIterator.operator*();
		}
		
		template<bool M = MULTI>
		typename std::enable_if_t<M, pointer>
		operator->() const
		{
			return listIterator.operator->();
		}

		template<bool M = MULTI>
		typename std::enable_if_t<!M, reference>
		operator*() const
		{
			return ptr->arr[index];
		}
		
		template<bool M = MULTI>
		typename std::enable_if_t<!M, pointer>
		operator->() const
		{
			return &ptr->arr[index];
		}

		
		template<bool M = MULTI>
		typename std::enable_if_t<M, bool>
		operator==(const SimpleHashTableIterator& other) const
        {
            return index == other.index && listIterator == other.listIterator;
        }
        
		template<bool M = MULTI>
		typename std::enable_if_t<M, bool>
        operator!=(const SimpleHashTableIterator& other) const
        {
            return index != other.index || listIterator != other.listIterator;
        }

		template<bool M = MULTI>
		typename std::enable_if_t<!M, bool>
		operator==(const SimpleHashTableIterator& other) const
        {
            return index == other.index;
        }
        
		template<bool M = MULTI>
		typename std::enable_if_t<!M, bool>
        operator!=(const SimpleHashTableIterator& other) const
        {
            return index != other.index;
        }

	private:
		friend SimpleHashTable<Key, Value, MULTI, HashFunc, KeyEqual, BIG>;

		bool all = false;
		SimpleHashTable<Key, Value, MULTI, HashFunc, KeyEqual, BIG>* ptr = nullptr;
		uint64_t index;
		
		typename std::list<KeyValueType>::iterator listIterator;

		//Allowing deletion of a specific element fast
		uint64_t rehashCounter;
		uint64_t bucketIndex = -1;
	};

    template<typename Key, typename Value, bool MULTI, typename HashFunc, typename KeyEqual, bool BIG>
    class SimpleHashTable
    {
    public:
        using RedirectType = std::conditional_t<BIG, uint64_t, uint32_t>;
        using HashRedirectPair = std::pair<RedirectType, RedirectType>;
        using KeyValueType = std::conditional_t<std::is_same_v<void, Value>, Key, std::pair<Key, Value>>;
		using KVStorageType = std::conditional_t<MULTI, std::list<KeyValueType>, KeyValueType>;
		using Iterator = SimpleHashTableIterator<Key, Value, MULTI, HashFunc, KeyEqual, BIG>;


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
         *          Note that you may not have less than 1024 buckets though you may request it.
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
            if(initSize < 1024)
                initSize = 1024;

            fastHashInfo = std::vector<uint8_t>(initSize);
            redirectInfo = std::vector<HashRedirectPair>(initSize);
        }

        SimpleHashTable(const std::initializer_list<KeyValueType>& defaultValues)
        {
            for(const KeyValueType& p : defaultValues)
            {
                insert(p);
            }
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
        SimpleHashTable(SimpleHashTable<Key, Value, MULTI>& other)
        {
            arr = other.arr;
            extraKeyStorage = other.extraKeyStorage;
            fastHashInfo = other.fastHashInfo;
            redirectInfo = other.redirectInfo;
			totalElements = other.totalElements;
			rehashCounter = other.rehashCounter;
        }
        /**
         * @brief Copy Assign a new Hash Table object
         * 
         * @param other 
         */
        void operator=(SimpleHashTable<Key, Value, MULTI>& other)
        {
            arr = other.arr;
            extraKeyStorage = other.extraKeyStorage;
            fastHashInfo = other.fastHashInfo;
            redirectInfo = other.redirectInfo;
			totalElements = other.totalElements;
			rehashCounter = other.rehashCounter;
        }

        /**
         * @brief Move Construct a new Hash Table object
         *      Note that "other" will be invalidated by this
         * 
         * @param other 
         */
        SimpleHashTable(SimpleHashTable<Key, Value, MULTI>&& other) noexcept
        {
            arr = std::move(other.arr);
            extraKeyStorage = std::move(other.extraKeyStorage);
            fastHashInfo = std::move(other.fastHashInfo);
            redirectInfo = std::move(other.redirectInfo);
			totalElements = std::move(other.totalElements);
			rehashCounter = std::move(other.rehashCounter);
        }
        
        /**
         * @brief Move Assign a new Hash Table object
         *      Note that "other" will be invalidated by this
         * 
         * @param other 
         */
        void operator=(SimpleHashTable<Key, Value, MULTI>&& other) noexcept
        {
            arr = std::move(other.arr);
            extraKeyStorage = std::move(other.extraKeyStorage);
            fastHashInfo = std::move(other.fastHashInfo);
            redirectInfo = std::move(other.redirectInfo);
			totalElements = std::move(other.totalElements);
			rehashCounter = std::move(other.rehashCounter);
        }

        /**
         * @brief Completely Clears the hash table.
         * 
         */
        void clear()
        {
            fastHashInfo.clear();
            redirectInfo.clear();
            arr.clear();
			extraKeyStorage.clear();
			totalElements = 0;
			rehashCounter++;
        }

		/**
         * @brief Clears the hash table but does not deallocate the total number of buckets.
		 *		Useful if the hashmap is to be used again and a similar number of elements are to be place in it.
		 *		If the total number of elements is significantly less, completely clearing the entire table is often better for memory usage.
         * 
         */
        void fastClear()
        {
			std::memset((void*)fastHashInfo.data(), 0, fastHashInfo.size());
			std::memset((void*)redirectInfo.data(), 0, redirectInfo.size()*sizeof(HashRedirectPair));
            arr.clear();
			extraKeyStorage.clear();
			totalElements = 0;
			rehashCounter++;
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

        // //If its possible to construct from P, this is allowed.

        // /**
        //  * @brief Attempts to insert into the hash table.
        //  *      Enabled if its possible to construct KeyValueType (the Key or std::pair<Key, Value>) from the templated type P
        //  * 
        //  * @param v 
        //  * @return auto 
        //  */
        // template<class P, std::enable_if_t<std::is_constructible_v<KeyValueType, P&&>, bool> = true>
        // auto insert(P&& v)
        // {
        //     return emplace(std::forward<P>(v));
        // }

        /**
         * @brief Emplaces into the hash table.
         *      This will construct a new element with the given arguments in place.
         *          Note that this will always construct a new element first and will remove it if an element is found that
         *          uses the specified key.
         *      May rehash. If you exceed 80% fill rate, it will rehash.
         *          Note that a rehash does not invalidate any iterator references however, if the internal data structure
         *          (std::vector) resizes, it will invalidate all iterators.
         * 
         * @param v 
         * @return auto 
         */
        auto emplace(KeyValueType&& v)
        {
            if(fastHashInfo.size() == 0)
            {
                fastHashInfo = std::vector<uint8_t>(1024);
                redirectInfo = std::vector<HashRedirectPair>(1024);
            }
            
            //extra check needed if and only if its possible to overflow
            //does nothing if BIG is enabled
            checkIfOverflowPossible();
			
            const Key& key = getKey(v);
            uint64_t actualHash = hasher(key);

            uint8_t partialHash = extractPartialHash(actualHash); //must replace top bit so its considered valid
            RedirectType extraHash = extractPartialHashEx(actualHash);
            uint64_t intendedLocation = actualHash % fastHashInfo.size();
            while(!getLocationEmpty(intendedLocation))
            {
                if(checkForDuplicate(intendedLocation, partialHash, extraHash, key))
                {
					return appendMultimap(intendedLocation, std::forward<KeyValueType>(v)); //will handle the pop_back()
                }

                intendedLocation = (intendedLocation+1) % fastHashInfo.size();
            }
            
			attemptToAdd(std::forward<KeyValueType>(v));
            fastHashInfo[intendedLocation] = partialHash;
            redirectInfo[intendedLocation] = {actualHash, arr.size()-1};

			Iterator returnIt = Iterator(this, arr.size()-1, false);
			returnIt.bucketIndex = intendedLocation;
			
            float currentLoadBalance = (float)arr.size() / (float)fastHashInfo.size();
            if(currentLoadBalance > MaxLoadBalance)
            {
				//re-balance
                rebalance();
            }
			
			totalElements++;
            return returnIt; //Bucket index may be invalid if a rehash occured right before this returns.
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
         * @brief Attempts to find an element by P comparing it to an element's Key and remove it.  This will remove ALL elements with that key if this is a multimap.
         *      Enabled if HashFunc and KeyEqual are both transparent and its possible to compare P to Key.
         *          Meaning that you are allowing different types other than the actual Key type to be used for hashing and comparison.
         *
         *      Note that if any deletion happens, more than one iterator may be invalidated.
		 *			If this is a multimap and more than one element in the list exists, only iterators pointing to the deleted elemment are invalidated.
		 *			Otherwise
         *          The iterator pointing to the list becomes invalid and the iterator pointing to the last element in the interal array is invalid
         *          	This is used to make fast deletes.
         * 
		 *		If you are unsure if the iterator is valid, re-obtain it.
		 *		DO NOT ASSUME THE ITERATOR IS VALID ACROSS THREADS
         * 
         * @tparam P 
         * @param k 
         * @return auto
         *      Returns an iterator to the next valid key if it exists. If this is not a multimap, returns an invalid iterator.
         */
        template<typename P, typename H = HashFunc, typename KE = KeyEqual,
        std::enable_if_t<both_transparent_v<H, KE>, bool> = true>
        auto erase(const P& k)
        {
            return remove(find(k), true);
        }

        /**
         * @brief Attempts to find an element by its Key and remove it. This will remove ALL elements with that key if this is a multimap.
         *      Note that if any deletion happens, more than one iterator may be invalidated.
		 *			If this is a multimap and more than one element in the list exists, only iterators pointing to the deleted elemment are invalidated.
		 *			Otherwise
         *          The iterator pointing to the list becomes invalid and the iterator pointing to the last element in the interal array is invalid
         *          	This is used to make fast deletes.
         * 
		 *		If you are unsure if the iterator is valid, re-obtain it.
		 *		DO NOT ASSUME THE ITERATOR IS VALID ACROSS THREADS
         * @param k 
         * @return auto
         *      Returns an iterator to the next valid key if it exists. If this is not a multimap, returns an invalid iterator.
         */
        auto erase(const Key& k)
        {
            return remove(find(k), true);
        }

		/**
		 * @brief Attempts to delete the specified iterator that MUST come from this object and MUST be valid.
		 *		An iterator is invalid under 2 cases:
		 *			The iterator was deleted (obvious one to catch)
		 *			The iterator is the last element in the internal array (not obvious to catch)
		 *				This is used to make fast deletes.
		 *
		 *		If you are unsure if the iterator is valid, re-obtain it.
		 *		DO NOT ASSUME THE ITERATOR IS VALID ACROSS THREADS
		 * 
		 * @param it 
		 * @return auto 
		 */
		auto erase(const Iterator& it)
		{
			return remove(it, false);
		}

		/**
		 * @brief Attempts to delete the specified iterator that MUST come from this object and MUST be valid. Will remove ALL elements with that key if this is a multimap
		 *		An iterator is invalid under 2 cases:
		 *			The iterator was deleted (obvious one to catch)
		 *			The iterator is the last element in the internal array (not obvious to catch)
		 *				This is used to make fast deletes.
		 *
		 *		If you are unsure if the iterator is valid, re-obtain it.
		 *		DO NOT ASSUME THE ITERATOR IS VALID ACROSS THREADS
		 * 
		 * @param it 
		 * @return auto 
		 */
		auto eraseAll(const Iterator& it)
		{
			return remove(it, true);
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
            return totalElements;
        }

        /**
         * @brief Forces a rebalance of the hashmap.
         *      This will resize the total number of buckets (typically increasing them) resulting in a hash map with
         *      better performance for searching but requires more memory.
         *          If you have removed a lot from your hashmap, forcing a rehash may result in less total buckets if
         *          it would not affect performance. If the total of elements is less than 40% of the total number of buckets, you should expect lower total buckets.
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
            return Iterator(this, 0, true);
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
            return Iterator(this, arr.size(), true);
        }

		/**
		 * @brief Attempts to tightly fit the internal data structures removing the allocated but unused capacity of the internal data structures.
		 *		The functionality of this depends entirely on compiler implementation. std::vector::shrink_to_fit() is not guaranteed to actually do anything.
		 *		
		 *		This is best used when the maximum number of items that will be inserted into the list is known. An example being where you inserted a large number of items and don't plan to ever
		 *		insert again. Deletion and re-insertion is fine as the internal data structures will still be tightly fit.
		 *		Using this when you plan to continue inserting will result worse performance causing additional copy operations
		 * 
		 */
		void tightlyFit()
		{
			fastHashInfo.shrink_to_fit();
			redirectInfo.shrink_to_fit();
			arr.shrink_to_fit();
			extraKeyStorage.shrink_to_fit();
		}
		
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
			while(!getLocationEmpty(intendedLocation))
            {
				if(checkForDuplicate(intendedLocation, partialHash, extraHash, key))
				{
					return appendMultimap(intendedLocation, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<Args>(args)...));
				}

                intendedLocation = (intendedLocation+1) % fastHashInfo.size();
            }
            
            //you are required to have a default constructor for this to work. Consistent with other hashtables
            attemptToAdd(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<Args>(args)...));
			
            fastHashInfo[intendedLocation] = partialHash;
            redirectInfo[intendedLocation] = {actualHash, arr.size()-1};

			
			Iterator returnIt = Iterator(this, arr.size()-1, false);
			returnIt.bucketIndex = intendedLocation;
			
            float currentLoadBalance = (float)arr.size() / (float)fastHashInfo.size();
            if(currentLoadBalance > MaxLoadBalance)
            {
				//re-balance
                rebalance();
            }
			
			totalElements++;
            return returnIt;
        }

        template<typename P>
        auto search(const P& k)
        {
            if(UNLIKELY(arr.size() == 0))
                return end();
            
            uint64_t actualHash = hasher(k);
            uint8_t partialHash = extractPartialHash(actualHash);
            RedirectType extraHash = extractPartialHashEx(actualHash);
            uint64_t location = actualHash % fastHashInfo.size();

            while(!getLocationEmpty(location))
            {
				if(checkForDuplicate(location, partialHash, extraHash, k))
				{
					Iterator returnIt = Iterator(this, getRedirectInfo(location), false);
					returnIt.bucketIndex = location;
					return returnIt;
				}
                location = (location+1) % fastHashInfo.size();
            }
            return end();
        }

        auto remove(const Iterator& it, bool deleteAll)
        {
            if(UNLIKELY(arr.size() == 0))
                return end();

			if(UNLIKELY(it == end()))
				return end();

			//check if iterator's bucket index is valid. It must not be -1 (SIZE_MAX) and it must be before a rehash.
			//If failed, recompute the bucket index.
			//NOTE: an invalid iterator will have an invalid bucket index and can not recompute the bucket index.

			//will attempt to delete from the spot first. Can get away with it if its more than 1 thing in the list
			size_t elementCounter = elementsAtLocation(it.index);
			if(!deleteAll)
			{
				if(elementCounter > 1)
				{
					//fast path
					totalElements--;
					return removeFromList(it);
				}
			}

			Iterator newIT = it;
			//slower path
			if(it.rehashCounter != rehashCounter || it.bucketIndex == -1)
			{
				//invalid bucket index. Recompute (search for it again)
				newIT = find(it->first);
				newIT.all = it.all;
			}

			//assume has a valid bucket index now. If its invalid, its end()
			if(UNLIKELY(newIT == end()))
				return end(); //something very very odd happened.

			uint64_t bucketLocation = newIT.bucketIndex;
            uint8_t currentPartialHash = getPartialHash(bucketLocation);
            RedirectType currentHash = getPartialHashEx(bucketLocation);
            
            //if found, find the location of the last item in arr and swap that with our current spot

            uint64_t lastSpotHash = hasher(getKey(arr.back()));
            uint8_t lastSpotPartialHash = extractPartialHash(lastSpotHash);
            RedirectType lastSpotExtraHash = extractPartialHashEx(lastSpotHash);
            uint64_t lastSpotLocation = lastSpotHash % fastHashInfo.size();
            
            //it exists so we can skip the extra work of checking free slots.
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
            fastHashInfo[bucketLocation] = 0;
            
            //swap data and pop back which completes the deletion
			swapDataStorageAndDelete(it.index);
			swapExtraKeyStorageAndDelete(it.index);

            //swap locations too
            redirectInfo[lastSpotLocation].second = redirectInfo[bucketLocation].second;

            //extra step. shift data back till we hit an empty spot or we hit a node that is in its desired spot
            uint64_t previousLocation = bucketLocation;
            bucketLocation = (bucketLocation+1) % fastHashInfo.size();

            while(!getLocationEmpty(bucketLocation))
            {
                if(getDistanceFromDesiredSpot(bucketLocation) > 0)
                {
                    fastHashInfo[previousLocation] = fastHashInfo[bucketLocation];
                    redirectInfo[previousLocation] = redirectInfo[bucketLocation];
                }
                else
                    break;

                previousLocation = bucketLocation;
                bucketLocation = (bucketLocation+1) % fastHashInfo.size();
            }

			totalElements -= elementCounter;

			//IMPORTANT. There are cases where you'd want the next valid spot even if its not apart of the same list.
			//example: iterating from begin() to end() over all elements (not caring about the key) and deleting certain elements based on some criteria that is not strictly the key.
			//		Think ID value as key but you wish to delete invalid names which have nothing to do with ID.

			if(newIT.all)
				return Iterator(this, newIT.index, true); //No known bucket index. Will have to solve for it later.

            return end();
        }

		//returns if the list is empty. Will be used to actually delete
		template<bool K = MULTI>
		typename std::enable_if_t<K, Iterator>
		removeFromList(const Iterator& it)
		{
			auto it2 = arr[it.index].erase(it.listIterator);
			if(it2 == arr[it.index].end())
				return end();
			
			Iterator returnIt = Iterator(this, it.index, it.all, it2); //must maintain the same "all" nature of the previous iterator
			returnIt.bucketIndex = it.bucketIndex;
			return returnIt;
		}

		template<bool K = MULTI>
		typename std::enable_if_t<!K, Iterator>
		removeFromList(const Iterator& it)
		{
			return end();
		}

		template<bool K = MULTI>
		typename std::enable_if_t<K, size_t>
		elementsAtLocation(size_t arrIndex)
		{
			return arr[arrIndex].size();
		}

		template<bool K = MULTI>
		typename std::enable_if_t<!K, size_t>
		elementsAtLocation(size_t arrIndex)
		{
			return 1;
		}

        void rebalance()
        {
            //Allowed to scale down the total buckets too now.
            size_t newSize = fastHashInfo.size();
            double load = (double)arr.size() / (double)fastHashInfo.size();
            if(load < (MaxLoadBalance/2))
                newSize = fastHashInfo.size()/2;
            else if(load >= MaxLoadBalance)
                newSize = fastHashInfo.size()*2;
            
            newSize = __max(newSize, 1024); //not allowed to have less than 1024 buckets

            std::vector<uint8_t> newHashInfo = std::vector<uint8_t>(newSize);
            std::vector<HashRedirectPair> newRedirectInfo = std::vector<HashRedirectPair>(newSize);
			rehashCounter++;

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

		const Key& getKey(const std::list<KeyValueType>& v)
		{
			return getKey(v.back());
		}
        
		const Value& getValue(const std::list<KeyValueType>& v)
		{
			return getValue(v.back());
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
		
		void swapDataStorageAndDelete(uint64_t index)
		{
			std::swap(arr.back(), arr[index]);
			arr.pop_back();
		}
		
		template<bool M = MULTI>
		typename std::enable_if<M, void>::type
		swapExtraKeyStorageAndDelete(uint64_t index)
		{
			std::swap(extraKeyStorage.back(), extraKeyStorage[index]);
			extraKeyStorage.pop_back();
		}

		template<bool M = MULTI>
		typename std::enable_if<!M, void>::type
		swapExtraKeyStorageAndDelete(uint64_t index)
		{
		}

		template<bool M = MULTI, typename P>
		typename std::enable_if<M, bool>::type
        checkForDuplicate(uint64_t intendedLocation, uint8_t partialHash, RedirectType extraHash, const P& key)
        {
            if(getPartialHash(intendedLocation) == partialHash) //fast path but 2 checks which may be unnecessary
            {
                if(comparePartialHashEx(intendedLocation, extraHash))
                {
                    if(LIKELY( keyEqualFunc(extraKeyStorage[getRedirectInfo(intendedLocation)], key) ))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

		
		template<bool M = MULTI, typename P>
		typename std::enable_if<!M, bool>::type
        checkForDuplicate(uint64_t intendedLocation, uint8_t partialHash, RedirectType extraHash, const P& key)
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
}