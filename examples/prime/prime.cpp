// MapReduce library example program
// Copyright (C) 2009,2010,2011,2012 Craig Henderson.
// cdm.henderson@gmail.com

// The prime number code is based on work from Christian Henning [chhenning@gmail.com]

#include <boost/config.hpp>
#if defined(BOOST_MSVC)
#   pragma warning(disable: 4127)

// turn off checked iterators to avoid performance hit
#   if !defined(__SGI_STL_PORT)  &&  !defined(_DEBUG)
#       define _SECURE_SCL 0
#       define _HAS_ITERATOR_DEBUGGING 0
#   endif
#endif

#include "mapreduce.hpp"
#include <iostream>

namespace prime_calculator {

bool const is_prime(long const number)
{
    if (number > 2)
    {
        if (number % 2 == 0)
            return false;

        long const n = std::abs(number);
        long const sqrt_number = static_cast<long>(std::sqrt(static_cast<double>(n)));

        for (long i = 3; i < sqrt_number; i+=2)
        {
            if (n % i == 0)
                return false;
        }
    }
    else if (number == 0 || number == 1)
        return false;
    
    return true;
}

template<typename MapTask>
class number_source : mapreduce::detail::noncopyable
{
  public:
    number_source(long first, long last, long step)
      : sequence_(0), first_(first), last_(last), step_(step)
    {
    }

    bool const setup_key(typename MapTask::key_type &key)
    {
        key = sequence_++;
        return (key * step_ <= last_);
    }

    bool const get_data(typename MapTask::key_type const &key, typename MapTask::value_type &value)
    {
        typename MapTask::value_type val;

        val.first  = first_ + (key * step_);
        val.second = std::min(val.first + step_ - 1, last_);

        std::swap(val, value);
        return true;
    }

  private:
    long       sequence_;
    long const step_;
    long const last_;
    long const first_;
};

struct map_task : public mapreduce::map_task<long, std::pair<long, long> >
{
    template<typename Runtime>
    void operator()(Runtime &runtime, key_type const &/*key*/, value_type const &value) const
    {
        for (key_type loop=value.first; loop<=value.second; ++loop)
            runtime.emit_intermediate(is_prime(loop), loop);
    }
};

struct reduce_task : public mapreduce::reduce_task<bool, long>
{
    template<typename Runtime, typename It>
    void operator()(Runtime &runtime, key_type const &key, It it, It ite) const
    {
        if (key)
            std::for_each(it, ite, std::bind(&Runtime::emit, &runtime, true, std::placeholders::_1));
    }
};

typedef
mapreduce::job<prime_calculator::map_task,
               prime_calculator::reduce_task,
               mapreduce::null_combiner,
               prime_calculator::number_source<prime_calculator::map_task>
> job;

} // namespace prime_calculator

int main(int argc, char *argv[])
{
    mapreduce::specification spec;

    int prime_limit = 10000;
    if (argc > 1)
        prime_limit = std::max(1, atoi(argv[1]));

    if (argc > 2)
        spec.map_tasks = std::max(1, atoi(argv[2]));

    if (argc > 3)
        spec.reduce_tasks = atoi(argv[3]);
    else
        spec.reduce_tasks = std::max(1U, std::thread::hardware_concurrency());

    prime_calculator::job::datasource_type datasource(0, prime_limit, prime_limit/spec.reduce_tasks);

    std::cout <<"\nCalculating Prime Numbers in the range 0 .. " << prime_limit << " ..." <<std::endl;
    prime_calculator::job job(datasource, spec);
    mapreduce::results result;
#ifdef _DEBUG
    job.run<mapreduce::schedule_policy::sequential<prime_calculator::job> >(result);
#else
    job.run<mapreduce::schedule_policy::cpu_parallel<prime_calculator::job> >(result);
#endif
    std::cout <<"\nMapReduce finished in " << result.job_runtime.count() << " with " << std::distance(job.begin_results(), job.end_results()) << " results" << std::endl;

    for (prime_calculator::job::const_result_iterator it = job.begin_results();
         it!=job.end_results();
         ++it)
    {
        std::cout <<it->second <<" ";
    }

	return 0;
}
