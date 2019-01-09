#include "jtracer.hpp"

void __cyg_profile_func_enter (void *func, void *call_site)
{
	Dl_info info;
	std::shared_ptr<opentracing::Span> span;

	FILE *f = fopen("/home/ubuntu/home.txt", "a+");
	fprintf(f, "h\n");
	fclose(f);

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


		/*printf("Function Entry : %p %p [%s] [%s] \n", func, call_site,
		 *                               info.dli_fname ? info.dli_fname : "?",
		 *                                                             info.dli_sname ? info.dli_sname : "?");*/

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
