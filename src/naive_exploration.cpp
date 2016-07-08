#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include <iostream>
#include <string>
#include <unordered_set>
#include <functional>

#include <json/json.h>

/*------------------------------------------------------------------------------
Payments are streamed in, parsed into timestamped connections. 

The set of connections by user (userConnections) is located for each user.

The new connection is added if not already present, otherwise the timestamp is 
updated.
------------------------------------------------------------------------------*/

struct payment
{
  std::string actor;
  std::string target;
  boost::posix_time::ptime time;

  payment(const std::string& actor_, const std::string& target_, const std::string& time_)
    : target(target_), actor(actor_)
  {
    // 2014-03-27T04:28:20Z
    std::stringstream ss(time_);
    boost::posix_time::time_input_facet *dif = new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ");
    ss.imbue(std::locale(ss.getloc(), dif));

    ss >> time;
    std::cout << time << std::endl;
  }

};

struct connection
{
  std::string target;
  boost::posix_time::ptime time;

  connection(const payment& p) :target(p.target), time(p.time) {}


  // Within a specific user's set of connections, the other party's
  // name is sufficient to determine connection equality.
  bool operator == (const connection& c2) const {
    return target == c2.target;
  }
  
  struct Hash {
    size_t operator () (const connection& c) const {
      return std::hash<const char *>()(c.target.c_str());
      // return std::hash<const std::string>()(c.target);
    }
  };


  friend std::ostream& operator << (std::ostream &out, const connection& c)
  {
    out << "[" << c.target << "]";
    return out;
  }
    
};


struct userConnections
{
  std::string actor;
  std::unordered_set<connection, connection::Hash> connections;

  userConnections(const payment& p)
    : actor(p.actor)
  {
    connections.insert(connection(p));
  }

  friend std::ostream& operator << (std::ostream &out, const userConnections& uc)
  {
    out << uc.actor << " (" << uc.connections.size() << " connections)" << std::endl;
    return out;
  }

};

struct actor{};
struct target{};
struct time{};


// define a multiply indexed set with indices by id and name
typedef boost::multi_index_container<
  userConnections,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<
      boost::multi_index::tag<actor>, BOOST_MULTI_INDEX_MEMBER(userConnections, std::string, actor)
      >
    >
  > connection_set;


template<typename Tag, typename MultiIndexContainer>
void print_out_by(const MultiIndexContainer& es)
{
  // get a view to index #1 (name)
  const typename boost::multi_index::index<MultiIndexContainer, Tag>::type& i = boost::multi_index::get<Tag>(es);
  typedef typename MultiIndexContainer::value_type value_type;
  
  // use name_index as a regular std::set
  std::copy(i.begin(), i.end(), std::ostream_iterator<value_type>(std::cout));
}


int main() {
  connection_set cs;

  payment p("Jordan-Gruber", "Jamie-Korn", "2014-03-27T04:28:20Z");
  cs.insert(userConnections(p));

  p= payment("Maryann-Berry", "Jamie-Korn", "2016-04-07T03:33:19Z");
  cs.insert(userConnections(p));

  p= payment("Ying-Mo", "Maryann-Berry", "2016-04-07T03:33:19Z");
  cs.insert(userConnections(p));
  
  p= payment("Ying-Mo", "FartButt", "2016-04-07T03:44:19Z");
  cs.insert(userConnections(p));
  

  print_out_by<actor>(cs);

  
  return 0;
}
