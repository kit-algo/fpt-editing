#ifndef UTIL_HPP
#define UTIL_HPP

#include <utility>
#include <array>

namespace std
{
	template<typename A, typename B>
	struct hash<pair<A, B>>
	{
		size_t operator()(pair<A, B> const &p) const
		{
			hash<A> ha;
			hash<B> hb;
			return ha(p.first) ^ hb(p.second);
		}
	};
}

namespace Util
{
	template <typename... Args, std::size_t... Is>
	std::tuple<Args &...> MakeTupleRef(std::tuple<Args...> &tuple, std::index_sequence<Is...>)
	{
		return std::tie(std::get<Is>(tuple)...);
	}

	template <typename... Args>
	std::tuple<Args &...> MakeTupleRef(std::tuple<Args...> &tuple)
	{
		return MakeTupleRef(tuple, std::make_index_sequence<sizeof...(Args)>());
	}

	template<typename T, typename Iter, std::size_t... Indices>
	std::array<T, sizeof...(Indices)> to_array(Iter iter, std::index_sequence<Indices...>) {
		return {{((void)Indices, *iter++)...}};
	}

	template<typename T, std::size_t N, typename Iter>
	std::array<T, N> to_array(Iter iter) {
		return to_array<T>(std::move(iter), std::make_index_sequence<N>{});
	}
}

#endif
