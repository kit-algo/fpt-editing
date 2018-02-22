#ifndef EDITOR_HPP
#define EDITOR_HPP

#include "../config.hpp"

#include "../util.hpp"
#include "../Options.hpp"


namespace Editor
{
	template<typename... Consumer>
	struct Consumer_valid
	{
		static constexpr bool value = Options::has_tagged_consumer<Options::Tag::Selector, Consumer...>::value && Options::has_tagged_consumer<Options::Tag::Lower_Bound, Consumer...>::value;
	};
}

#endif
