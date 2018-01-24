#ifndef UTIL_HPP
#define UTIL_HPP

#include <utility>

namespace std
{
		template<typename A, typename B>
		struct hash<pair<A, B>>
		{
				size_t operator ()(pair<A, B> const& p) const
				{
						hash<A> ha;
						hash<B> hb;
						return ha(p.first) ^ hb(p.second);
				}
		};
}

#endif // UTIL_HPP
