#ifdef _MINIX
#include <minix/fault.h>
#else
#include <edfi.h>
#endif

#ifdef _MINIX
int do_fault_injector_request(message *m){
    message replymsg;
    do_fault_injector_request_impl(m);
    return send(m->m_source, &replymsg);
}
#endif

