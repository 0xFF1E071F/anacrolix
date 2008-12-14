//#include <iterator>
#include <cassert>
#include <deque>
#include <map>
#include <set>
#include <vector>

size_t const FAIL_STATE = -1;

template <typename SymbolT>
class AhoCorasick
{
public:
	typedef size_t state_t;

	template <typename KeywordIterT>
	AhoCorasick(
			KeywordIterT const & kw_begin,
			KeywordIterT const & kw_end)
	:	goto_(kw_begin, kw_end, output_),
		fail_(goto_, output_),
		where_(0),
		state_(0)
	{
	}

	template <typename InputIterT, typename CallbackT>
	void search(
			InputIterT input_it,
			InputIterT input_end,
			CallbackT & callback)
	{
		for ( ; input_it != input_end; ++input_it, where_++)
		{
			SymbolT const &input(*input_it);
			{
				state_t next;
				while ((next = goto_(state_, input)) == FAIL_STATE)
					state_ = fail_(state_);
				state_ = next;
			}
			{
				std::set<size_t> const &out_node = output_[state_];
				typename std::set<size_t>::const_iterator output_it;
				for (output_it = out_node.begin(); output_it != out_node.end(); ++output_it)
				{
					callback(*output_it, where_);
				}
			}
		}
	}

	void reset() { state_ = 0; where_ = 0; }

private:
#if 0
	class OutputFunction
	{
	public:
		OutputFunction() {}
		std::set<size_t> & operator[](state_t index)
		{
			return outputs_[index];
		}

	private:
		std::map<state_t, std::set<size_t> > outputs_;
	};
#else
	typedef std::map<state_t, std::set<size_t> > OutputFunction;
#endif
	class GotoFunction
	{
	public:
		typedef std::map<SymbolT, state_t> edges_t;
		typedef std::vector<edges_t> nodes_t;

		template <typename KeywordIterT>
		GotoFunction(
				KeywordIterT kw_iter,
				KeywordIterT const & kw_end,
				OutputFunction & output_f)
		{
			state_t newstate = 0;
			size_t kw_index = 0;
			for ( ; kw_iter != kw_end; ++kw_iter)
			{
				enter(*kw_iter, newstate);
				output_f[newstate].insert(kw_index++);
			}
		}

		state_t operator()(state_t state, SymbolT const & symbol) const
		{
			assert(state < graph_.size());
			edges_t const &node(graph_[state]);
			typename edges_t::const_iterator const &edge_it(node.find(symbol));
			if (edge_it != node.end())
			{
				return edge_it->second;
			}
			else
			{
				return (state == 0) ? 0 : FAIL_STATE;
			}
		}

		nodes_t const & get_nodes() const { return graph_; }

	private:
		template <typename KeywordT>
		void enter(
				KeywordT const & keyword,
				state_t & newstate)
		{
			state_t state = 0;
			size_t index = 0;
			std::map<SymbolT, state_t> *node;

			// follow existing symbol edges
			for ( ; index < keyword.size(); index++)
			{
				// this node won't be initialized
				if (state == graph_.size())
					graph_.resize(state + 1);
				node = &graph_[state];
				typename std::map<SymbolT, state_t>::iterator edge =
						node->find(keyword[index]);
				if (edge == node->end()) break;
				state = edge->second;
			}
			/* could resize here as we know how many more symbols remain,
			 * however we can't invalidate node yet */
			graph_.resize(graph_.size() + keyword.size() - index);
			node = &graph_[state];
			// generate new symbol edges
			for ( ; index < keyword.size(); index++)
			{
				(*node)[keyword[index]] = ++newstate;
				state = newstate;
				//if (state == graph_.size())
				//	graph_.resize(state + 1);
				node = &graph_[state];
			}
		}

		std::vector<std::map<SymbolT, state_t> > graph_;
	};

	class FailureFunction
	{
	public:
		FailureFunction(
				GotoFunction const & _goto,
				OutputFunction & output)
		:	table_(_goto.get_nodes().size(), FAIL_STATE)
		{
			std::deque<state_t> queue;

			queue_edges(_goto.get_nodes()[0], queue);

			while (!queue.empty())
			{
				state_t r = queue.front();
				queue.pop_front();
				typename GotoFunction::edges_t const &node(_goto.get_nodes()[r]);
				typename GotoFunction::edges_t::const_iterator edge_it;
				for (edge_it = node.begin(); edge_it != node.end(); ++edge_it)
				{
					std::pair<SymbolT, state_t> const &edge(*edge_it);
					SymbolT const &a(edge.first);
					state_t const &s(edge.second);

					queue.push_back(s);
					state_t state = table_[r];
					while (_goto(state, a) == FAIL_STATE)
						state = table_[state];
					table_[s] = _goto(state, a);
					output[s].insert(
							output[table_[s]].begin(),
							output[table_[s]].end());
				}
			}
		}

		state_t operator()(state_t state) const
		{
			return table_[state];
		}

	private:
		std::vector<state_t> table_;

		/* queue nonfail edges */
		inline void queue_edges(
				typename GotoFunction::edges_t const & node,
				std::deque<state_t> & queue)
		{
			typename GotoFunction::edges_t::const_iterator edge_it;
			for (edge_it = node.begin(); edge_it != node.end(); ++edge_it)
			{
				std::pair<SymbolT, state_t> const &edge(*edge_it);
				queue.push_back(edge.second);
				table_[edge.second] = 0;
			}
		}
	};

	OutputFunction output_;
	GotoFunction const goto_;
	FailureFunction const fail_;
	size_t where_;
	state_t state_;
};