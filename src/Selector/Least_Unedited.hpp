#ifndef SELECTOR_LEAST_UNEDITED_HPP
#define SELECTOR_LEAST_UNEDITED_HPP

#include <vector>

#include "../config.hpp"

#include "../Editor/Editor.hpp"

namespace Selector
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Least_Unedited : Editor::Tag::Selector
	{
	public:
		static constexpr char const *name = "Least";

	private:
		std::vector<VertexID> problem;
		size_t free_pairs;

	public:
		Least_Unedited(Graph const &) {;}

		void prepare()
		{
			problem.clear();
			free_pairs = 0;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			size_t free = 0;

			// count unedited vertex pairs
			if(std::is_same<Mode, Editor::Options::Modes::Edit>::value)
			{
				for(auto vit = b + 1; vit != e; vit++)
				{
					for(auto uit = b; uit != vit; uit++)
					{
						if(std::is_same<Conversion, Editor::Options::Conversions::Skip>::value && uit == b && vit == e - 1) {continue;}
						if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
						{
							if(edited.has_edge(*uit, *vit)) {continue;}
						}
						free++;
					}
				}
			}
			else if(std::is_same<Mode, Editor::Options::Modes::Delete>::value)
			{
				for(auto uit = b, vit = b + 1; vit != e; uit++, vit++)
				{
					if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					free++;
				}
				if(!std::is_same<Conversion, Editor::Options::Conversions::Skip>::value && graph.has_edge(*b, *(e - 1)))
				{
					auto uit = b;
					auto vit = e - 1;
					do
					{
						if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
						{
							if(edited.has_edge(*uit, *vit)) {continue;}
						}
						free++;
					} while(false);
				}
			}
			else if(std::is_same<Mode, Editor::Options::Modes::Insert>::value)
			{
				for(auto vit = b + 2; vit != e; vit++)
				{
					for(auto uit = b; uit != vit - 1; uit++)
					{
						if(uit == b && vit == e - 1 && (std::is_same<Conversion, Editor::Options::Conversions::Skip>::value || graph.has_edge(*b, *(e - 1)))) {continue;}
						if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
						{
							if(edited.has_edge(*uit, *vit)) {continue;}
						}
						free++;
					}
				}
			}

			if(free < free_pairs || free_pairs == 0)
			{
				free_pairs = free;
				problem = std::vector<VertexID>(b, e);
				if(free == 0) {return false;}
			}
			return true;
		}

		std::vector<VertexID> const &result(size_t, Graph const &, Graph const &) const
		{
			return problem;
		}
	};
}

#endif
