#include <iostream>
#include "../include/ms_release.hpp"
int main(){ auto r=ms::run_release_check();
 std::cout<<"23-30 release check: "<<(r.ok?"PASS":"FAIL")<<"\n";
 std::cout<<"project_io="<<r.project_io<<" crash_recovery="<<r.crash_recovery<<" performance="<<r.performance<<" licenses="<<r.licenses<<"\n";
 return r.ok?0:1; }
