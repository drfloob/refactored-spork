#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/format.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include <iostream>
#include <string>
#include <unordered_set>
#include <functional>
#include <set>

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
    // TODO: check for memory leak. new without delete
    
    ss >> time;
    // std::cout << time << std::endl;
  }
  
  payment(const std::string& actor_, const std::string& target_, const boost::posix_time::ptime time_)
    : target(target_), actor(actor_), time(time_)
  {}

  const payment reverse() const
  {
    return payment(target, actor, time);
  }  
  
  struct Compare {
    size_t operator () (const payment& c1, const payment& c2) const {
      return c1.time < c2.time;
    }
  };

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

  std::size_t degree() const
  {
    return connections.size();
  }
  
  friend std::ostream& operator << (std::ostream &out, const userConnections& uc)
  {
    out << uc.actor << " (" << uc.connections.size() << " connections)" << std::endl;
    return out;
  }

};

//------------------------------------------------------------------------------
// HEADER STUFF
//------------------------------------------------------------------------------

// tags
struct actor{};
struct median{};

// define a multiply indexed set with indices by id and name
typedef boost::multi_index_container<
  userConnections,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<
      boost::multi_index::tag<actor>,
      BOOST_MULTI_INDEX_MEMBER(userConnections, std::string, actor)
      >,
    boost::multi_index::ranked_non_unique<
      boost::multi_index::tag<median>,
      boost::multi_index::const_mem_fun<userConnections, std::size_t, &userConnections::degree>
      >
    >
  > connection_set;

typedef boost::multi_index::index<connection_set,actor>::type connection_set_by_actor;
typedef boost::multi_index::index<connection_set,median>::type connection_set_by_rank;

typedef std::set<payment, payment::Compare> payment_set;

//------------------------------------------------------------------------------
// END HEADER STUFF
//------------------------------------------------------------------------------


template<typename Tag, typename MultiIndexContainer>
void print_out_by(const MultiIndexContainer& es)
{
  // get a view to index #1 (name)
  const typename boost::multi_index::index<MultiIndexContainer, Tag>::type& i = boost::multi_index::get<Tag>(es);
  typedef typename MultiIndexContainer::value_type value_type;
  
  // use name_index as a regular std::set
  std::copy(i.begin(), i.end(), std::ostream_iterator<value_type>(std::cout));
}

void _addOrUpdateConnections_process(const payment& p, connection_set& cs, connection_set_by_actor& index)
{
  connection_set_by_actor::iterator found = index.find(p.actor);
      
  if (found == index.end()) {
    // dude not found
    // std::cout << "   NOT FOUND: " << p.actor << std::endl;
    cs.insert(userConnections(p));
  } else {
    // dude found
    // std::cout << "found actor: " << found->actor << std::endl;
    userConnections uc = *found;
    cs.erase(found);
    connection c(p);
    // TODO: check if connection exists
    uc.connections.insert(c);
    cs.insert(uc);
  }
}

void addOrUpdateConnections(const payment& p, connection_set& cs, payment_set& ps)
{
  // check if new time is older than 60 seconds
  payment_set::reverse_iterator rit = ps.rbegin();
  if (rit != ps.rend()) {
    std::cout << boost::format("new time(%1%); latest stored time(%2%)") % p.time % rit->time << std::endl;
    std::cout << " (debug) time diff: " << (p.time - rit->time) << std::endl;
    std::cout << " (debug) is new time more than 60 seconds ahead?: " << ((p.time - rit->time) > boost::posix_time::seconds(60)) << std::endl;
    std::cout << " (debug) is new time more than 60 seconds behind?: " << ((rit->time - p.time) > boost::posix_time::seconds(60)) << std::endl;
  } else {
    ps.insert(p);
  }
  
  connection_set_by_actor& index = cs.get<actor>();
  _addOrUpdateConnections_process(p, cs, index);
  _addOrUpdateConnections_process(p.reverse(), cs, index);
}




void printRank(const connection_set& cs) {
  const connection_set_by_rank& index = cs.get<median>();

  std::size_t size = index.size();
  double medianDegree;

  // std::cout << " (debug) size: " << size << std::endl;

  int idx = std::ceil((size / 2.0) - 1);
  // std::cout << " (debug) index: " << idx << std::endl;
  connection_set_by_rank::const_iterator it = index.nth(idx);

  if (size % 2 == 0) {
    std::size_t d1 = it->degree(), d2 = (++it)->degree();    
    // std::cout << " (debug) median of 2: " << d1 << " and " << d2 << std::endl;
    medianDegree = (d1 + d2)/2.0;
  } else {
    // std::cout << " (debug) single median: " << it->degree() << std::endl;
    medianDegree = it->degree();
  }
  // std::cout << "Median: " << medianDegree << std::endl;
  std::cout << medianDegree << std::endl;
}



int main() {
  std::cout.precision(2);

  connection_set cs;
  payment_set ps;

  payment p("Jordan-Gruber", "Jamie-Korn", "2016-04-07T03:33:18Z");
  addOrUpdateConnections(p, cs, ps);
  printRank(cs);
  
  p= payment("Maryann-Berry", "Jamie-Korn", "2016-04-07T03:33:19Z");
  addOrUpdateConnections(p, cs, ps);
  printRank(cs);

  p= payment("Ying-Mo", "Maryann-Berry", "2016-04-07T03:33:20Z");
  addOrUpdateConnections(p, cs, ps);
  printRank(cs);
  
  p= payment("Ying-Mo", "FartButt", "2016-04-07T03:44:19Z");
  addOrUpdateConnections(p, cs, ps);
  printRank(cs);

  
  std::cout << std::endl;
  print_out_by<actor>(cs);

  std::cout << std::endl;
  print_out_by<median>(cs);

  
  return 0;
}
