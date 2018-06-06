#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <typeinfo>
#include <type_traits>

#include "config.hpp"


namespace Options
{
	/** Restrictions applied to editing specific edges */
	namespace Restrictions
	{
		/** No restrictions */
		struct None {static constexpr char const *name = "None";};
		/** Editing already edited edges (thus undoing the edit) is forbidden */
		struct Undo {static constexpr char const *name = "Undo";};
		/** Editing edited or marked edges is forbidden. In the provided Editors this prevents redundant exploration */
		struct Redundant {static constexpr char const *name = "Redundant";};
	}

	/** How to handle editing the edge converting a Px to a Cx or vice versa */
	namespace Conversions
	{
		/** No special treatment */
		struct Normal {static constexpr char const *name = "Normal";};
		/** Handle the edge after all other edges of the forbidden subgraph */
		struct Last {static constexpr char const *name = "Last";};
		/** Ignore the edge, do not consider it for editing */
		struct Skip {static constexpr char const *name = "Skip";};
	}

	/** Editing mode */
	namespace Modes
	{
		/** Deletions and insertions are allowed */
		struct Edit {static constexpr char const *name = "Edit";};
		/** Only deletions are allowed */
		struct Delete {static constexpr char const *name = "Delete";};
		/** Only insertions are allowed */
		struct Insert {static constexpr char const *name = "Insert";};
	}

	/** Empty classes/tags for Consumers to inherit from. Allows Editors to figure out if a Consumer is suitable for a given task */
	namespace Tag
	{
		/** The consumer can provide a forbidden subgraphs */
		struct Selector {};
		/** The consumer calculates a lower bound on the number of edits required */
		struct Lower_Bound {};
		/** The consumer returns a lower bound and this lower bound without freshly edited edges should be set again before calling prepare() */
		struct Lower_Bound_Update {};
		/** The consumers result function should be called (for side effects) */
		struct Result {};
	}

	/* Helper Functions */

	/** Is there a Consumer supporting the given Tag? */
	template<typename Tag, typename... Consumer>
	struct has_tagged_consumer
	{
		static constexpr bool value = (std::is_base_of<Tag, Consumer>::value || ...);
	};

	/** Finds the first Consumer supporting the given Tag, static_assert()s if no Consumer supports the Tag */
	template<typename Tag, typename... Consumer>
	struct get_tagged_consumer
	{
		static_assert(has_tagged_consumer<Tag, Consumer...>::value, "No matching consumer for tag");

	private:
		template<typename Tag2, typename Consumer2, typename... Consumer_Tail>
		struct impl
		{
			static constexpr size_t value()
			{
				return v<Tag2, Consumer2>();
			}
			template<typename T, typename C>
			static constexpr
			typename std::enable_if<std::is_base_of<T, C>::value, size_t>::type
			v()
			{
				return 0;
			}

			template<typename T, typename C>
			static constexpr
			typename std::enable_if < !std::is_base_of<T, C>::value, size_t >::type
			v()
			{
				return 1 + impl<Tag2, Consumer_Tail...>::value();
			}
		};

	public:
		static constexpr size_t value = impl<Tag, Consumer...>::value();
	};
}

#endif
