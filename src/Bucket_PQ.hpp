#ifndef BUCKET_PQ_HPP
#define BUCKET_PQ_HPP

#include <vector>
#include <limits>
#include <cassert>
#include <numeric>
#include <stdexcept>
#include <random>

class BucketPQ
{
	static constexpr size_t invalid_pos = std::numeric_limits<size_t>::max();
public:
	BucketPQ(size_t size, size_t seed) : gen(seed), pos(size, invalid_pos), max_val(0), buckets_built(false) {};

	void insert(size_t el, size_t val)
	{
		assert(el < pos.size());
		if (buckets_built) throw std::runtime_error("Elements cannot be inserted after building");
		if (pos[el] != invalid_pos) throw std::runtime_error("Error, element already inserted");
		pos[el] = elements.size();
		elements.emplace_back(el, val);
		if (val > max_val) max_val = val;
	}

	void build()
	{
		if (buckets_built) throw std::runtime_error("Buckets already built");
		if (empty()) throw std::runtime_error("Cannot build an empty PQ");

		bucket_end.resize(max_val + 1);

		for (const std::pair<size_t, size_t>& el : elements)
		{
			++bucket_end[el.second];
		}

		std::partial_sum(bucket_end.begin(), bucket_end.end(), bucket_end.begin());
		bucket_begin = bucket_end;

		std::vector<std::pair<size_t, size_t>> tmp;
		std::swap(tmp, elements);

		elements.resize(tmp.size());

		for (const std::pair<size_t, size_t>& el : tmp)
		{
			const size_t p = --bucket_begin[el.second];
			elements[p] = el;
			pos[el.first] = p;
		}

		min_val = elements[0].second;

		buckets_built = true;
	}

	std::pair<size_t, size_t> pop()
	{
		if (!buckets_built) throw std::runtime_error("Buckets must be built first");
		assert(bucket_begin[min_val] < bucket_end[min_val]);

		std::uniform_int_distribution<size_t> dist(bucket_begin[min_val], bucket_end[min_val] - 1);
		const size_t i = dist(gen);
		auto result = elements[i];
		assert(result.second == min_val);
		elements[i] = elements[bucket_begin[min_val]];
		pos[elements[i].first] = i;
		pos[result.first] = invalid_pos;

		++bucket_begin[min_val];

		advance_min_val();

		return result;
	}

	void erase(size_t el)
	{
		if (!buckets_built) throw std::runtime_error("Buckets must be built first");
		const size_t i = pos[el];
		if (i == invalid_pos) throw std::runtime_error("Element does not exist");
		const size_t val = elements[i].second;
		assert(bucket_begin[val] <= i && i < bucket_end[val]);

		elements[i] = elements[bucket_begin[val]];
		pos[elements[i].first] = i;
		pos[el] = invalid_pos;
		++bucket_begin[val];

		advance_min_val();
	}

	void decrease_key_by_one(size_t el)
	{
		if (!buckets_built) throw std::runtime_error("Buckets must be built first");
		const size_t i = pos[el];
		if (i == invalid_pos) throw std::runtime_error("Element does not exist");
		const size_t val = elements[i].second;
		if (val == 0) throw std::runtime_error("Cannot decrease below zero");
		const size_t new_val = val - 1;
		assert(bucket_begin[val] <= i && i < bucket_end[val]);

		// Remove element from bucket
		const size_t j = bucket_begin[val];
		elements[i] = elements[j];
		// Update position of previous bucket begin
		pos[elements[i].first] = i;
		++bucket_begin[val];

		// Insert element at the end of bucket before.
		// Due to shrinking of the next bucket there is space!
		const size_t k = bucket_end[new_val];
		elements[k] = {el, new_val};
		pos[el] = k;

		++bucket_end[new_val];

		if (val == min_val) --min_val;
	}

	bool contains(size_t el)
	{
		return pos[el] != invalid_pos;
	}

	bool empty() const
	{
		if (!buckets_built) return elements.empty();
		return bucket_begin[min_val] == bucket_end[min_val];
	}

	void clear()
	{
		std::fill(pos.begin(), pos.end(), invalid_pos);
		elements.clear();
		bucket_begin.clear();
		bucket_end.clear();
		max_val = 0;
		min_val = 0;
		buckets_built = false;
	}


private:
	std::mt19937_64 gen; // Random numbers
	std::vector<size_t> pos; // Map an element to its position
	std::vector<std::pair<size_t, size_t>> elements; // The elements and their values, sorted in buckets
	std::vector<size_t> bucket_begin, bucket_end; // The bucket boundaries, indexed by value
	size_t max_val;
	size_t min_val;
	bool buckets_built;

	void advance_min_val()
	{
		while (bucket_begin[min_val] == bucket_end[min_val] && min_val < max_val) {
			++min_val;
		}
	}
};

#endif
