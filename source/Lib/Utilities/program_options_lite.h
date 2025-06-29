/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2022, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <iostream>
#include <sstream>
#include <string>
#include <list>
#include <map>
#include <cstdint>

#define JVET_O0549_ENCODER_ONLY_FILTER_POL 1 // JVET-O0549: Encoder-only GOP-based temporal filter. Program Options Lite related changes.

#include <vector>

#ifndef __PROGRAM_OPTIONS_LITE__
#define __PROGRAM_OPTIONS_LITE__

//! \ingroup TAppCommon
//! \{
using namespace std;

template <class T>
struct SMultiValueInput
{
  static_assert(!std::is_same<T, uint8_t>::value, "SMultiValueInput<uint8_t> is not supported");
  static_assert(!std::is_same<T, int8_t>::value, "SMultiValueInput<int8_t> is not supported");
  const T              minValIncl;
  const T              maxValIncl;
  const std::size_t    minNumValuesIncl;
  const std::size_t    maxNumValuesIncl; // Use 0 for unlimited
  std::vector<T> values;
  SMultiValueInput() : minValIncl(0), maxValIncl(0), minNumValuesIncl(0), maxNumValuesIncl(0), values() { }
  SMultiValueInput(std::vector<T> &defaults) : minValIncl(0), maxValIncl(0), minNumValuesIncl(0), maxNumValuesIncl(0), values(defaults) { }
  SMultiValueInput(const T &minValue, const T &maxValue, std::size_t minNumberValues = 0, std::size_t maxNumberValues = 0)
    : minValIncl(minValue), maxValIncl(maxValue), minNumValuesIncl(minNumberValues), maxNumValuesIncl(maxNumberValues), values() { }
  SMultiValueInput(const T &minValue, const T &maxValue, std::size_t minNumberValues, std::size_t maxNumberValues, const T* defValues, const uint32_t numDefValues)
    : minValIncl(minValue), maxValIncl(maxValue), minNumValuesIncl(minNumberValues), maxNumValuesIncl(maxNumberValues), values(defValues, defValues + numDefValues) { }
  SMultiValueInput<T> &operator=(const std::vector<T> &userValues) { values = userValues; return *this; }
  SMultiValueInput<T> &operator=(const SMultiValueInput<T> &userValues) { values = userValues.values; return *this; }

  T readValue(const char *&pStr, bool &bSuccess);

  istream& readValues(std::istream &in);
};

namespace df
{
  namespace program_options_lite
  {
    struct Options;

    struct ParseFailure : public std::exception
    {
      ParseFailure(std::string arg0, std::string val0) throw()
      : arg(arg0), val(val0)
      {}

      ~ParseFailure() throw() {};

      std::string arg;
      std::string val;

      const char* what() const throw() { return "Option Parse Failure"; }
    };

    struct ErrorReporter
    {
      ErrorReporter() : is_errored(0) {}
      virtual ~ErrorReporter() {}
      virtual std::ostream& error(const std::string& where);
      virtual std::ostream& warn(const std::string& where);
      bool is_errored;
    };

    extern ErrorReporter default_error_reporter;

    struct SilentReporter : ErrorReporter
    {
      SilentReporter() { }
      virtual ~SilentReporter() { }
      virtual std::ostream& error( const std::string& where ) { return dest; }
      virtual std::ostream& warn( const std::string& where ) { return dest; }
      std::stringstream dest;
    };

    void doHelp(std::ostream& out, Options& opts, unsigned columns = 80);
    std::list<const char*> scanArgv(Options& opts, unsigned argc, const char* argv[], ErrorReporter& error_reporter = default_error_reporter);
    void setDefaults(Options& opts);
    void parseConfigFile(Options& opts, const std::string& filename, ErrorReporter& error_reporter = default_error_reporter);

    /** OptionBase: Virtual base class for storing information relating to a
     * specific option This base class describes common elements.  Type specific
     * information should be stored in a derived class. */
    struct OptionBase
    {
      OptionBase(const std::string& name, const std::string& desc)
      : opt_string(name), opt_desc(desc)
      {};

      virtual ~OptionBase() {}

      /* parse argument arg, to obtain a value for the option */
      virtual void parse(const std::string& arg, ErrorReporter&) = 0;
      /* set the argument to the default value */
      virtual void setDefault() = 0;

      std::string opt_string;
      std::string opt_desc;
    };

    /** Type specific option storage */
    template<typename T>
    struct Option : public OptionBase
    {
      Option(const std::string& name, T& storage, T default_val, const std::string& desc)
      : OptionBase(name, desc), opt_storage(storage), opt_default_val(default_val)
      {}

      void parse(const std::string& arg, ErrorReporter&);

      void setDefault()
      {
        opt_storage = opt_default_val;
      }

      T& opt_storage;
      T opt_default_val;
    };

    /* Generic parsing */
    template<typename T>
    inline void
    Option<T>::parse(const std::string& arg, ErrorReporter&)
    {
      std::istringstream arg_ss (arg,std::istringstream::in);
      arg_ss.exceptions(std::ios::failbit);
      try
      {
        arg_ss >> opt_storage;
      }
      catch (...)
      {
        throw ParseFailure(opt_string, arg);
      }
    }

    /* string parsing is specialized -- copy the whole string, not just the
     * first word */
    template<>
    inline void
    Option<std::string>::parse(const std::string& arg, ErrorReporter&)
    {
      opt_storage = arg;
    }

    /** Option class for argument handling using a user provided function */
    struct OptionFunc : public OptionBase
    {
      typedef void (Func)(Options&, const std::string&, ErrorReporter&);

      OptionFunc(const std::string& name, Options& parent_, Func *func_, const std::string& desc)
      : OptionBase(name, desc), parent(parent_), func(func_)
      {}

      void parse(const std::string& arg, ErrorReporter& error_reporter)
      {
        func(parent, arg, error_reporter);
      }

      void setDefault()
      {
        return;
      }

    private:
      Options& parent;
      Func* func;
    };

    class OptionSpecific;
    struct Options
    {
      ~Options();

      OptionSpecific addOptions();

      struct Names
      {
        Names() : opt(0) {};
        ~Names()
        {
          if (opt)
          {
            delete opt;
          }
        }
        std::list<std::string> opt_long;
        std::list<std::string> opt_short;
        std::list<std::string> opt_prefix;
        OptionBase* opt;
      };

      void addOption(OptionBase *opt);

      typedef std::list<Names*> NamesPtrList;
      NamesPtrList opt_list;

      typedef std::map<std::string, NamesPtrList> NamesMap;
      NamesMap opt_long_map;
      NamesMap opt_short_map;
      NamesMap opt_prefix_map;
    };

    /* Class with templated overloaded operator(), for use by Options::addOptions() */
    class OptionSpecific
    {
    public:
      OptionSpecific(Options& parent_) : parent(parent_) {}

      /**
       * Add option described by name to the parent Options list,
       *   with storage for the option's value
       *   with default_val as the default value
       *   with desc as an optional help description
       */
      template<typename T>
      OptionSpecific&
      operator()(const std::string& name, T& storage, T default_val, const std::string& desc = "")
      {
        parent.addOption(new Option<T>(name, storage, default_val, desc));
        return *this;
      }
      template<typename T>
      OptionSpecific&
        operator()(const std::string& name, T* storage, T default_val, unsigned uiMaxNum, const std::string& desc = "")
      {
        std::string cNameBuffer;
        std::string cDescriptionBuffer;

        for (unsigned int uiK = 0; uiK < uiMaxNum; uiK++)
        {
          // it needs to be reset when extra digit is added, e.g. number 10 and above
          cNameBuffer.resize(name.size() + 10);
          cDescriptionBuffer.resize(desc.size() + 10);

          // isn't there are sprintf function for string??
          sprintf((char*)cNameBuffer.c_str(), name.c_str(), uiK, uiK);
          sprintf((char*)cDescriptionBuffer.c_str(), desc.c_str(), uiK, uiK);

          size_t pos = cNameBuffer.find_first_of('\0');
          if (pos != std::string::npos)
          {
            cNameBuffer.resize(pos);
          }

          parent.addOption(new Option<T>(cNameBuffer, (storage[uiK]), default_val, cDescriptionBuffer));
        }

        return *this;
      }

      template<typename T>
      OptionSpecific&
        operator()(const std::string& name, T** storage, T default_val, unsigned uiMaxNum, const std::string& desc = "")
      {
        std::string cNameBuffer;
        std::string cDescriptionBuffer;

        for (unsigned int uiK = 0; uiK < uiMaxNum; uiK++)
        {
          // it needs to be reset when extra digit is added, e.g. number 10 and above
          cNameBuffer.resize(name.size() + 10);
          cDescriptionBuffer.resize(desc.size() + 10);

          // isn't there are sprintf function for string??
          sprintf((char*)cNameBuffer.c_str(), name.c_str(), uiK, uiK);
          sprintf((char*)cDescriptionBuffer.c_str(), desc.c_str(), uiK, uiK);

          size_t pos = cNameBuffer.find_first_of('\0');
          if (pos != std::string::npos)
            cNameBuffer.resize(pos);

          parent.addOption(new Option<T>(cNameBuffer, *(storage[uiK]), default_val, cDescriptionBuffer));
        }

        return *this;
      }
      /**
       * Add option described by name to the parent Options list,
       *   with desc as an optional help description
       * instead of storing the value somewhere, a function of type
       * OptionFunc::Func is called.  It is upto this function to correctly
       * handle evaluating the option's value.
       */
      OptionSpecific&
      operator()(const std::string& name, OptionFunc::Func *func, const std::string& desc = "")
      {
        parent.addOption(new OptionFunc(name, parent, func, desc));
        return *this;
      }
    private:
      Options& parent;
    };

  } /* namespace: program_options_lite */
} /* namespace: df */

//! \}

#endif
