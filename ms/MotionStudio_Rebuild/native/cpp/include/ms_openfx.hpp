#pragma once
#include "ms_error.hpp"
#include "ms_image.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>
namespace ms {
struct OfxParam { std::string name; double value=0; };
struct OfxPluginInfo { std::string identifier,name,vendor,version,binary_path; };
class OpenFxPlugin { public: virtual ~OpenFxPlugin()=default; virtual const OfxPluginInfo& info() const=0; virtual bool supports(const ImageBuffer&) const=0; virtual Error render(const ImageBuffer&,ImageBuffer&,const std::vector<OfxParam>&,double)=0; };
class OpenFxRegistry { std::map<std::string,std::shared_ptr<OpenFxPlugin>> plugins_; public: Error register_plugin(std::shared_ptr<OpenFxPlugin>); bool remove(const std::string&); std::shared_ptr<OpenFxPlugin> find(const std::string&) const; std::vector<OfxPluginInfo> list() const; Error apply(const std::string&,const ImageBuffer&,ImageBuffer&,const std::vector<OfxParam>&,double) const; };
std::shared_ptr<OpenFxPlugin> make_identity_ofx_plugin();
}
