/*
 * Copyright 2014 Marios Kogias <marioskogias@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials
 *      provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef JTRACER_H
#define JTRACER_H

//#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <memory>
#include <thread>

#include <dlfcn.h>
#include <stack> 


#include <string>
extern "C" {
#include <zipkin_c.h>
}

#include <yaml-cpp/yaml.h>
#include <jaegertracing/Tracer.h>
#include <jaegertracing/reporters/Config.h>
#include <jaegertracing/samplers/Config.h>
#include <jaegertracing/Span.h>
#include <opentracing/span.h>


#include "jaeger_carrier.hpp"

using namespace std;

#define BT_BUF_SIZE 1000


extern "C" {
void __cyg_profile_func_enter (void *, void *) __attribute__((no_instrument_function));
void __cyg_profile_func_exit  (void *, void *) __attribute__((no_instrument_function));
}

namespace ZTracer {
    using std::string;
    class Trace;

    const char* const CLIENT_SEND = "cs";
    const char* const CLIENT_RECV = "cr";
    const char* const SERVER_SEND = "ss";
    const char* const SERVER_RECV = "sr";
    const char* const WIRE_SEND = "ws";
    const char* const WIRE_RECV = "wr";
    const char* const CLIENT_SEND_FRAGMENT = "csf";
    const char* const CLIENT_RECV_FRAGMENT = "crf";
    const char* const SERVER_SEND_FRAGMENT = "ssf";
    const char* const SERVER_RECV_FRAGMENT = "srf";

//    static FILE *logf;
    
    using StrMap = jaegertracing::SpanContext::StrMap;

    static inline int jtrace_init(const char *name="ceph-services") { 

        static pthread_mutex_t blkin_init_mutex = PTHREAD_MUTEX_INITIALIZER;
        static int initialized2 = 0;

	blkin_init();
        /*
         * Initialize srand with sth appropriete
         * time is not good for archipelago: several deamons -> same timstamp
         */
        pthread_mutex_lock(&blkin_init_mutex);
        if (!initialized2) {
	   auto configYAML = YAML::LoadFile("/home/ubuntu/config.yml");
	   static const auto config = jaegertracing::Config::parse(configYAML);
       	   static const auto tracer = jaegertracing::Tracer::make(
              name, config, jaegertracing::logging::nullLogger());

	   opentracing::Tracer::InitGlobal(
               std::static_pointer_cast<opentracing::Tracer>(tracer));

           initialized2 = 1;
        }
        pthread_mutex_unlock(&blkin_init_mutex);
        return 0;
    }

    class Endpoint : private blkin_endpoint {
    private:
	string _ip; // storage for blkin_endpoint.ip, see copy_ip()
	string _name; // storage for blkin_endpoint.name, see copy_name()

	friend class Trace;
    public:
	Endpoint(const char *name)
	    {
		blkin_init_endpoint(this, "0.0.0.0", 0, name);
	    }
	Endpoint(const char *ip, int port, const char *name)
	    {
		blkin_init_endpoint(this, ip, port, name);
	    }

	// copy constructor and operator need to account for ip/name storage
	Endpoint(const Endpoint &rhs) : _ip(rhs._ip), _name(rhs._name)
	    {
		blkin_init_endpoint(this, _ip.size() ? _ip.c_str() : rhs.ip,
				    rhs.port,
				    _name.size() ? _name.c_str(): rhs.name);
	    }
	const Endpoint& operator=(const Endpoint &rhs)
	    {
		_ip.assign(rhs._ip);
		_name.assign(rhs._name);
		blkin_init_endpoint(this, _ip.size() ? _ip.c_str() : rhs.ip,
				    rhs.port,
				    _name.size() ? _name.c_str() : rhs.name);
		return *this;
	    }

	// Endpoint assumes that ip and name will be string literals, and
	// avoids making a copy and freeing on destruction.  if you need to
	// initialize Endpoint with a temporary string, copy_ip/copy_name()
	// will store it in a std::string and assign from that
	void copy_ip(const string &newip)
	    {
		_ip.assign(newip);
		ip = _ip.c_str();
	    }
	void copy_name(const string &newname)
	    {
		_name.assign(newname);
		name = _name.c_str();
	    }

	void copy_address_from(const Endpoint *endpoint)
	    {
		_ip.assign(endpoint->ip);
		ip = _ip.c_str();
		port = endpoint->port;
	    }
	void share_address_from(const Endpoint *endpoint)
	    {
		ip = endpoint->ip;
		port = endpoint->port;
	    }
	void set_port(int p) { port = p; }
    };


    class Trace : private blkin_trace {
    private:
	string _name; // storage for blkin_trace.name, see copy_name()
	std::shared_ptr<jaegertracing::Tracer> tracer;
    public:
	std::shared_ptr<opentracing::Span> _span;
//        std::shared_ptr<jaegertracing::Span>  __span;
    public:
	// default constructor zero-initializes blkin_trace valid()
	// will return false on a default-constructed Trace until
	// init()
	Trace()
	    {
		// zero-initialize so valid() returns false
                tracer = std::static_pointer_cast<jaegertracing::Tracer>(opentracing::Tracer::Global());
		name = NULL;
		info.trace_id = 0;
		info.span_id = 0;
		info.parent_span_id = 0;
		endpoint = NULL;
	    }

	// construct a Trace with an optional parent
	Trace(const char *name, const Endpoint *ep, const Trace *parent = NULL)
	    {
		tracer = std::static_pointer_cast<jaegertracing::Tracer>(opentracing::Tracer::Global());
		opentracing::StartSpanOptions options;
		options.tags.push_back({ "tag-key", 1.23 });

		if (parent && parent->valid()) {
		    blkin_init_child(this, parent, ep ? : parent->endpoint,
				     name);
		    auto span = tracer->StartSpan(
		        name, { opentracing::ChildOf(&((parent->_span)->context())) });
		    _span = std::move(span);

		} else { // start new span
		    blkin_init_new_trace(this, name, ep);
		    _span = tracer->StartSpan(name);
		}
	    }


       //void
       //get_bt(void)
       //{
	//   FILE *fd = fopen("/home/ubuntu/testtrace.txt", "a+");

          // int j, nptrs;
          // void *buffer[BT_BUF_SIZE];
          // char **strings;

         //  nptrs = backtrace(buffer,  BT_BUF_SIZE);
         //  printf("backtrace() returned %d addresses\n", nptrs);

         //  /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
         //     would produce similar output to the following: */

         //  strings = backtrace_symbols(buffer, nptrs);
         //  if (strings == NULL) {
         //      perror("backtrace_symbols");
         //      exit(EXIT_FAILURE);
         //  }

         //  for (j = 0; j < nptrs; j++)
          //     fprintf(fd, "%s\n", strings[j]);

         //  free(strings);
	  // fprintf(fd, "\n\n\n\n");
	 //  fclose(fd);
      // }

	// construct a Trace from blkin_trace_info
	Trace(const char *name, const Endpoint *ep,
	      const blkin_trace_info *i, bool child=false)
	    {
		tracer = std::static_pointer_cast<jaegertracing::Tracer>(opentracing::Tracer::Global());
		if (child) {
		    blkin_init_child_info(this, i, ep, name);

                    // I should build an span context using i
                    opentracing::StartSpanOptions options;
		    const jaegertracing::SpanContext parentCtx;// = i->parent_span_ctx;
		    options.references.emplace_back(opentracing::SpanReferenceType::ChildOfRef,
                           &parentCtx);
		    options.references.emplace_back(opentracing::SpanReferenceType::FollowsFromRef, 
			   &parentCtx); 
		    _span = std::unique_ptr<opentracing::Span>(
		        tracer->StartSpanWithOptions(name, options).release());  
 
		} else {
		    /*   */
		    blkin_init_new_trace(this, name, ep);
		    set_info(i);
		    _span = tracer->StartSpan(name);

		}
	    }

	// copy constructor and operator need to account for name storage
	Trace(const Trace &rhs) : _name(rhs._name)
	    {
		tracer = std::static_pointer_cast<jaegertracing::Tracer>(opentracing::Tracer::Global());
		name = _name.size() ? _name.c_str() : rhs.name;
		info = rhs.info;
		endpoint = rhs.endpoint;
	
		_span = rhs._span;

	    }
	const Trace& operator=(const Trace &rhs)
	    {
		_name.assign(rhs._name);
		name = _name.size() ? _name.c_str() : rhs.name;
		info = rhs.info;
		endpoint = rhs.endpoint;
		_span = rhs._span;
		return *this;
	    }

	// return true if the Trace has been initialized
	bool valid() const { return info.trace_id != 0; } //_span->context()->isValid(); }
	operator bool() const { return valid(); }

	// (re)initialize a Trace with an optional parent
	int init(const char *name, const Endpoint *ep,
		 const Trace *parent = NULL)
	    {
		if (parent && parent->valid()) {
		     _span = opentracing::Tracer::Global()->StartSpan(
                        name, { opentracing::ChildOf(&((parent->_span)->context())) });

		    return blkin_init_child(this, parent,
					    ep ? : parent->endpoint, name);
		}
                _span = opentracing::Tracer::Global()->StartSpan(name);
		return blkin_init_new_trace(this, name, ep);
	    }

	// (re)initialize a Trace from blkin_trace_info
	int init(const char *name, const Endpoint *ep,
		 const blkin_trace_info *i, bool child=false)
	    {
		//get_bt();
		if (child)
		{
                    opentracing::StartSpanOptions options;
                    const jaegertracing::SpanContext parentCtx;// = i->parent_span_ctx;
                    options.references.emplace_back(opentracing::SpanReferenceType::ChildOfRef,
                           &parentCtx);
                    options.references.emplace_back(opentracing::SpanReferenceType::FollowsFromRef, 
                           &parentCtx);
                    _span = std::unique_ptr<opentracing::Span>(
                        tracer->StartSpanWithOptions(name, options).release());

		    return blkin_init_child_info(this, i, ep, _name.c_str());
		}

		 _span = opentracing::Tracer::Global()->StartSpan(name);
		return blkin_set_trace_properties(this, i, _name.c_str(), ep);
	    }

        int init(const char *name, const Endpoint *ep,
                 const blkin_trace_info *i, int io, bool child=false)
            {
                if (child)
                {
                    return blkin_init_child_info(this, i, ep, _name.c_str());
                }

                return blkin_set_trace_properties(this, i, _name.c_str(), ep);
            }


	// Trace assumes that name will be a string literal, and avoids
	// making a copy and freeing on destruction.  if you need to
	// initialize Trace with a temporary string, copy_name() will store
	// it in a std::string and assign from that
	void copy_name(const string &newname)
	    {
		_name.assign(newname);
		name = _name.c_str();
	    }

	const blkin_trace_info* get_info() const { return &info; }
	void set_info(const blkin_trace_info *i) { info = *i; }

	// record key-value annotations
	void keyval(const char *key, const char *val) const
	    {
		if (valid()) {
		    BLKIN_KEYVAL_STRING(this, endpoint, key, val);
//		    _span->Log({{ string(key), string(val) }});
		}
	    }
	void keyval(const char *key, int64_t val) const
	    {
		if (valid()) {
		    BLKIN_KEYVAL_INTEGER(this, endpoint, key, val);
//		    _span->Log({{ string(key), val }});
		}
	    }
	void keyval(const char *key, const char *val, const Endpoint *ep) const
	    {
		if (valid()) {
		    BLKIN_KEYVAL_STRING(this, ep, key, val);
//		    _span->Log({{ string(key), val }});
		}
	    }
	void keyval(const char *key, int64_t val, const Endpoint *ep) const
	    {
		if (valid()) {
		    BLKIN_KEYVAL_INTEGER(this, ep, key, val);
//		    _span->Log({{ string(key), val }});
		}
	    }

	// record timestamp annotations
	void event(const char *event) const
	    {
		if (valid()) {
		    BLKIN_TIMESTAMP(this, endpoint, event);
//		    _span->Log({{ string(event), std::chrono::system_clock::now() }});
		}
	    }
	void event(const char *event, const Endpoint *ep) const
	    {
		if (valid()) {
		    BLKIN_TIMESTAMP(this, ep, event);
//		    _span->Log({{ string(event), std::chrono::system_clock::now() }});
		}
	    }
    
        string inject(const char *name="jon-duo") 
	    {
	        std::stringstream ss;

		if (!_span) {
		     _span = opentracing::Tracer::Global()->StartSpan(name);
		}
		auto err = tracer->Inject(_span->context(), ss);
		assert(err);
		return ss.str();
	    }
        void extract(const char *name, string t_meta) 
	    {
		std::stringstream ss(t_meta);
		auto span_context_maybe = tracer->Extract(ss);
	        assert(span_context_maybe);
		_span = tracer->StartSpan(name, //"propagationSpan",
                                  {ChildOf(span_context_maybe->get())});
	    }
    };
    
    static thread_local Trace active_trace;

    static thread_local std::stack<std::shared_ptr<opentracing::Span>> trace_stack;
}
/*
#ifdef __GNUC__
void __cyg_profile_func_enter (void *func, void *call_site)
{
	Dl_info info;
	std::shared_ptr<opentracing::Span> span;

	if (dladdr(func, &info)) { // get function call information 
		if (ZTracer::trace_stack.empty()) {
			span = opentracing::Tracer::Global()->StartSpan(
					info.dli_sname ? info.dli_sname : "?");
		} else {
			auto parent = ZTracer::trace_stack.top();
			span = opentracing::Tracer::Global()->StartSpan(
					info.dli_sname ? info.dli_sname : "?",
					{opentracing::ChildOf(&(parent->context()))});
		}
		ZTracer::trace_stack.push(span);


		//printf("Function Entry : %p %p [%s] [%s] \n", func, call_site,
		//		info.dli_fname ? info.dli_fname : "?",
		//		info.dli_sname ? info.dli_sname : "?");

	}
}

void __cyg_profile_func_exit (void *func, void *call_site)
{
	Dl_info info;
	std::shared_ptr<opentracing::Span> span;
	if (dladdr(func, &info)) {
		if (!ZTracer::trace_stack.empty()) {
			span = ZTracer::trace_stack.top();
			span->Finish();
			ZTracer::trace_stack.pop();
		}
	}
}
#endif */
#endif /* end of include guard: ZTRACER_H */
