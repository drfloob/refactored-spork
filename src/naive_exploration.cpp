#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif


#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>


#include <iostream>
#include <string>
#include <json/json.h>


struct employee
{
  int         id;
  std::string name;

  employee(int id, const std::string& name)
    :id(id), name(name)
  {}

  bool operator < (const employee& e) const
  {
    return id < e.id;
  }

  friend std::ostream & operator << (std::ostream &outStream, const employee &e ) {
    return outStream << e.name << " : " << e.id << std::endl;
  }
};

struct id{};
struct name{};


// define a multiply indexed set with indices by id and name
typedef boost::multi_index_container<
  employee,
  boost::multi_index::indexed_by<
    // sort by employee::operator<
    boost::multi_index::ordered_unique<
      boost::multi_index::tag<id>, BOOST_MULTI_INDEX_MEMBER(employee, int, id)
      >,
    
    // sort by less<string> on name
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<name>, BOOST_MULTI_INDEX_MEMBER(employee, std::string, name)
      >
    >
  > employee_set;


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
  employee_set es;
  es.insert(employee(2, "george"));
  es.insert(employee(1, "paul"));
  es.insert(employee(3, "ringo"));
  es.insert(employee(0, "john"));
  es.insert(employee(-1, "yoko"));

  print_out_by<name>(es);

  std::cout << "\n";
  
  print_out_by<id>(es);
  
  return 0;
}
