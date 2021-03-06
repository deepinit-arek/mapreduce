// MapReduce library
// Copyright (C) 2009-2013 Craig Henderson
// cdm.henderson@gmail.com

#ifndef MAPREDUCE_MERGESORT_HPP
#define MAPREDUCE_MERGESORT_HPP

//#define DEBUG_TRACE_OUTPUT

#ifdef DEBUG_TRACE_OUTPUT
#include <iostream>
#endif

#include <deque>
#include <list>
#include <map>
#include <sstream>
#include <fstream>
#include <boost/filesystem.hpp>

#ifdef __GNUC__
#include <cstring> // ubuntu linux
#include <fstream> // ubuntu linux
#endif

namespace mapreduce {

namespace detail {

template<typename T>
bool const less_2nd(T const &first, T const &second)
{
    return first.second < second.second;
}

template<typename T>
bool const greater_2nd(T const &first, T const &second)
{
    return first.second > second.second;
}

template<typename It>
bool const do_file_merge(It first, It last, char const *outfilename)
{
#ifdef _DEBUG
    int const max_files=10;
#endif

    int count = 0;
    std::ofstream outfile(outfilename, std::ios_base::out | std::ios_base::binary);
    while (first!=last)
    {
        //!!!subsequent times around the loop need to merge with outfilename from previous iteration
        // in the meantime, we assert if we go round the loop more than once as it will produce incorrect results
        assert(++count == 1);

        typedef std::list<std::pair<std::shared_ptr<std::ifstream>, std::string> > file_lines_t;
        file_lines_t file_lines;
        for (; first!=last; ++first)
        {
            auto file = std::make_shared<std::ifstream>(first->c_str(), std::ios_base::in | std::ios_base::binary);
            if (!file->is_open())
                break;
#ifdef _DEBUG
            if (file_lines.size() == max_files)
                break;
#endif

            std::string line;
            std::getline(*file, line, '\r');
            file_lines.push_back(std::make_pair(file, line));
        }

        while (file_lines.size() > 0)
        {
            typename file_lines_t::iterator it;
            if (file_lines.size() == 1)
                it = file_lines.begin();
            else
                it = std::min_element(file_lines.begin(), file_lines.end());
            outfile << it->second << "\r";

            std::getline(*it->first, it->second, '\r');
            if (it->first->eof())
                file_lines.erase(it);
        }
    }

    return true;
}

inline bool const delete_file(std::string const &pathname)
{
    if (pathname.empty())
        return true;

    bool success = false;
    try
    {
#ifdef DEBUG_TRACE_OUTPUT
        std::cout << "\n   deleting " << pathname;
#endif
        success = boost::filesystem::remove(pathname);
    }
    catch (std::exception &e)
    {
#ifdef DEBUG_TRACE_OUTPUT
        std::cerr << "\n" << e.what() << "\n";
#endif
        e;
    }
    return success;
}

template<typename Filenames>
class temporary_file_manager : detail::noncopyable
{
  public:
    temporary_file_manager(Filenames &filenames)
      : filenames_(filenames)
    {
    }

    ~temporary_file_manager()
    {
        try
        {
            // bind to the pass-through delete_file function because boost::filesystem::remove
            // takes a boost::filesystem::path parameter and not a std::string parameter - the
            // compiler will understandably not bind using an implicit conversion
            std::for_each(filenames_.begin(), filenames_.end(), std::bind(delete_file, std::placeholders::_1));
        }
        catch (std::exception &)
        {
        }
    }

  private:
    Filenames &filenames_;
};

}   // namespace detail

template<typename T>
struct shared_ptr_indirect_less
{
    bool operator()(std::shared_ptr<T> const &left, std::shared_ptr<T> const &right) const
    {
        return *left < *right;
    }
};

template<typename Record>
bool const merge_sort(char     const *in,
                      char     const *out,
                      unsigned const  max_lines = 10000000)
{
#ifdef DEBUG_TRACE_OUTPUT
    std::cout << "\n   merge_sort " << in << " to " << out;
#endif
    std::deque<std::string>         temporary_files;
    detail::temporary_file_manager<
        std::deque<std::string> >   tfm(temporary_files);
    
    std::ifstream infile(in, std::ios_base::in | std::ios_base::binary);
    if (!infile.is_open())
    {
        std::ostringstream err;
        err << "Unable to open file " << in;
        BOOST_THROW_EXCEPTION(std::runtime_error(err.str()));
    }

    while (!infile.eof())
    {
        typedef std::map<std::shared_ptr<Record>, unsigned, shared_ptr_indirect_less<Record> > lines_t;
        lines_t lines;

        for (unsigned loop=0; !infile.eof()  &&  loop<max_lines; ++loop)
        {
            if (infile.fail()  ||  infile.bad())
                BOOST_THROW_EXCEPTION(std::runtime_error("An error occurred reading the input file."));

            std::string line;
            std::getline(infile, line, '\r');
            if (line.length() > 0) // ignore blank lines
            {
                auto record = std::make_shared<Record>();
                std::istringstream l(line);
                l >> *record;
                ++lines.insert(std::make_pair(record,0U)).first->second;
            }
        }

        std::string const temp_filename(platform::get_temporary_filename());
        temporary_files.push_back(temp_filename);
        std::ofstream file(temp_filename.c_str(), std::ios_base::out | std::ios_base::binary);
        for (typename lines_t::const_iterator it=lines.begin(); it!=lines.end(); ++it)
        {
            if (file.fail()  ||  file.bad())
                BOOST_THROW_EXCEPTION(std::runtime_error("An error occurred writing temporary a file."));

            for (unsigned loop=0; loop<it->second; ++loop)
                file << *it->first << "\r";
        }
    }
    infile.close();

    if (temporary_files.size() == 1)
    {
        detail::delete_file(out);
        boost::filesystem::rename(*temporary_files.begin(), out);
        temporary_files.clear();
    }
    else
        detail::do_file_merge(temporary_files.begin(), temporary_files.end(), out);

	return true;
}

}   // namespace mapreduce

#endif  // MAPREDUCE_MERGESORT_HPP
