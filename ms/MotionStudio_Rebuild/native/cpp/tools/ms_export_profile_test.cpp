#include "ms_export.hpp"
#include <iostream>
#include <algorithm>
static int fails=0;
#define CHECK(x,m) do{if(!(x)){std::cerr<<"FAIL "<<m<<"\n";++fails;}else std::cout<<"ok "<<m<<"\n";}while(0)
int main(){
  using namespace ms;
  const auto p=default_export_presets();
  auto has=[&](const char* id){return std::find_if(p.begin(),p.end(),[&](const ExportPreset& x){return x.id==id;})!=p.end();};
  CHECK(has("social_portrait_1080p"),"vertical social preset exists");
  CHECK(has("social_portrait_4k"),"vertical 4K preset exists");
  CHECK(has("square_1080p"),"square preset exists");
  CHECK(has("yt_4k"),"UHD preset exists");
  const auto* webm=find_preset("webm_vp9_1080p");
  CHECK(webm && webm->container==ExportContainer::Webm && webm->video_codec==ExportCodec::Vp9,"WebM preset uses matching container and codec");
  const auto* prores=find_preset("prores_4k_master");
  CHECK(prores && prores->container==ExportContainer::Mov,"ProRes master uses MOV container");
  std::cout<<(fails?"EXPORT PROFILE TEST FAILED\n":"EXPORT PROFILE TEST PASSED\n");
  return fails?1:0;
}
