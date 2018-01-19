#ifndef LOWER_BOUND_BASIC_HPP
#define LOWER_BOUND_BASIC_HPP

#include <vector>

#include "../config.hpp"

#include "../Editor/Editor.hpp"

namespace Lower_Bound
{
	template<typename Graph, typename Graph_Edits, typename Mode, typename Restriction, typename Conversion>
	class Basic
	{
	public:
		static constexpr char const *name = "Basic";

	private:
		size_t found = 0;
		Graph_Edits used;

	public:
		Basic(Graph const &graph) : used(graph.size())
		{
			;
		}

		void prepare()
		{
			used.clear();
			found = 0;
		}

		bool next(Graph const &graph, Graph_Edits const &edited, std::vector<VertexID>::const_iterator b, std::vector<VertexID>::const_iterator e)
		{
			if(std::is_same<Mode, Editor::Options::Modes::Edit>::value)
			{
				// test
				for(auto vit = b + 1; vit != e; vit++)
				{
					for(auto uit = b; uit != vit; uit++)
					{
						if(std::is_same<Conversion, Editor::Options::Conversions::Skip>::value && uit == b && vit == e - 1) {continue;}
						if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
						{
							if(edited.has_edge(*uit, *vit)) {continue;}
						}
						if(used.has_edge(*uit, *vit)) {return true;}
					}
				}
				// add
				for(auto vit = b + 1; vit != e; vit++)
				{
					for(auto uit = b; uit != vit; uit++)
					{
						if(std::is_same<Conversion, Editor::Options::Conversions::Skip>::value && uit == b && vit == e - 1) {continue;}
						if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
						{
							if(edited.has_edge(*uit, *vit)) {continue;}
						}
						used.set_edge(*uit, *vit);
					}
				}
			}
			else if(std::is_same<Mode, Editor::Options::Modes::Delete>::value)
			{
				// test
				for(auto uit = b, vit = b + 1; vit != e; uit++, vit++)
				{
					if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					if(used.has_edge(*uit, *vit)) {return true;}
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
						if(used.has_edge(*uit, *vit)) {return true;}
						// add
						used.set_edge(*uit, *vit);
					} while(false);
				}
				// add
				for(auto uit = b, vit = b + 1; vit != e; uit++, vit++)
				{
					if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
					{
						if(edited.has_edge(*uit, *vit)) {continue;}
					}
					used.set_edge(*uit, *vit);
				}
			}
			else if(std::is_same<Mode, Editor::Options::Modes::Insert>::value)
			{
				// test
				for(auto vit = b + 2; vit != e; vit++)
				{
					for(auto uit = b; uit != vit - 1; uit++)
					{
						if(uit == b && vit == e - 1 && (std::is_same<Conversion, Editor::Options::Conversions::Skip>::value || graph.has_edge(*b, *(e - 1)))) {continue;}
						if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
						{
							if(edited.has_edge(*uit, *vit)) {continue;}
						}
						if(used.has_edge(*uit, *vit)) {return true;}
					}
				}
				// add
				for(auto vit = b + 2; vit != e; vit++)
				{
					for(auto uit = b; uit != vit - 1; uit++)
					{
						if(uit == b && vit == e - 1 && (std::is_same<Conversion, Editor::Options::Conversions::Skip>::value || graph.has_edge(*b, *(e - 1)))) {continue;}
						if(!std::is_same<Restriction, Editor::Options::Restrictions::None>::value)
						{
							if(edited.has_edge(*uit, *vit)) {continue;}
						}
						used.set_edge(*uit, *vit);
					}
				}
			}

			found++;
			return true;
		}

		size_t result() const
		{
			return found;
		}
	};
}

#endif
