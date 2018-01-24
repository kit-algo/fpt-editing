#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <functional>
#include <map>
#include <typeinfo>
#include <vector>

#include "config.hpp"


namespace Options
{
	namespace Restrictions
	{
		struct None {static constexpr char const *name = "None";};
		struct Undo {static constexpr char const *name = "Undo";};
		struct Redundant {static constexpr char const *name = "Redundant";};
	}

	namespace Conversions
	{
		struct Normal {static constexpr char const *name = "Normal";};
		struct Last {static constexpr char const *name = "Last";};
		struct Skip {static constexpr char const *name = "Skip";};
	}

	namespace Modes
	{
		struct Edit {static constexpr char const *name = "Edit";};
		struct Delete {static constexpr char const *name = "Delete";};
		struct Insert {static constexpr char const *name = "Insert";};
	}

	namespace Tag
	{
		struct Selector {};
		struct Lower_Bound {};
	}

	template<typename Tag, typename... Consumer>
	struct get_tagged_consumer;

	template<typename Tag, typename Consumer, typename... Consumer_Tail>
	struct get_tagged_consumer<Tag, Consumer, Consumer_Tail...>
	{
		static constexpr size_t value() {return v<Tag, Consumer>();}

		template<typename T, typename C>
		static constexpr
		typename std::enable_if<std::is_base_of<T, C>::value, size_t>::type
		v()
		{
			return 0;
		}

		template<typename T, typename C>
		static constexpr
		typename std::enable_if<!std::is_base_of<T, C>::value, size_t>::type
		v()
		{
			return 1 + get_tagged_consumer<Tag, Consumer_Tail...>::value();
		}
	};

	template<typename Tag>
	struct get_tagged_consumer<Tag>
	{
		static_assert(!std::is_base_of<Tag, Tag>::value, "No matching consumer for tag");
		//static constexpr size_t value() {return std::numeric_limits<size_t>::max();}
	};

	template<typename Tag, typename... Consumer>
	struct has_tagged_consumer;

	template<typename Tag, typename Consumer, typename... Consumer_Tail>
	struct has_tagged_consumer<Tag, Consumer, Consumer_Tail...>
	{
		static constexpr bool value = has_tagged_consumer<Tag, Consumer_Tail...>::value;
	};

	template<typename Tag, typename... Consumer_Tail>
	struct has_tagged_consumer<Tag, Tag, Consumer_Tail...>
	{
		static constexpr bool value = true;
	};

	template<typename Tag>
	struct has_tagged_consumer<Tag>
	{
		static constexpr bool value = false;
	};

}

#endif
