#include "include/ms_effect_presets.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
namespace ms {
void EffectPresetLibrary::add(EffectPreset p){ if(p.id.empty())return; remove(p.id); presets_.push_back(std::move(p)); }
bool EffectPresetLibrary::remove(const std::string& id){auto n=presets_.size(); presets_.erase(std::remove_if(presets_.begin(),presets_.end(),[&](const auto&p){return p.id==id;}),presets_.end());return n!=presets_.size();}
const EffectPreset* EffectPresetLibrary::find(const std::string& id) const{for(auto& p:presets_)if(p.id==id)return &p;return nullptr;}
std::vector<EffectPreset> EffectPresetLibrary::search(const std::string&q,const std::string&cat)const{std::string x=q;std::transform(x.begin(),x.end(),x.begin(),::tolower);std::vector<EffectPreset> r;for(auto&p:presets_){std::string hay=p.name+" "+p.description+" "+p.category;std::transform(hay.begin(),hay.end(),hay.begin(),::tolower);if((cat.empty()||p.category==cat)&&(x.empty()||hay.find(x)!=std::string::npos))r.push_back(p);}return r;}
bool EffectPresetLibrary::save(const std::string&path)const{std::ofstream f(path);if(!f)return false;f<<"{\n  \"presets\": [\n";for(size_t i=0;i<presets_.size();++i){auto&p=presets_[i];f<<"    {\"id\":\""<<p.id<<"\",\"name\":\""<<p.name<<"\",\"category\":\""<<p.category<<"\",\"description\":\""<<p.description<<"\",\"author\":\""<<p.author<<"\",\"license\":\""<<p.license<<"\"}"<<(i+1<presets_.size()?",":"")<<"\n";}f<<"  ]\n}\n";return true;}
bool EffectPresetLibrary::load(const std::string&path){std::ifstream f(path);if(!f)return false;std::stringstream s;s<<f.rdbuf();return s.str().find("presets")!=std::string::npos;}
EffectPresetLibrary EffectPresetLibrary::builtins(){EffectPresetLibrary l;l.add({"clean_grade","Clean Grade","Color","Balanced contrast and brightness","Motion Studio","GPL-3.0-or-later",{{"brightness",{{"amount",0.05}},true},{"contrast",{{"amount",1.05}},true}}, {"grade","clean","color"}});l.add({"soft_glow","Soft Glow","Stylize","Subtle highlight glow","Motion Studio","GPL-3.0-or-later",{{"blur",{{"radius",8}},true},{"brightness_contrast",{{"brightness",0.03},{"contrast",1.0}},true}}, {"glow","soft","stylize"}});l.add({"chromatic_push","Chromatic Push","Distort","Controlled RGB separation","Motion Studio","GPL-3.0-or-later",{{"chromatic",{{"amount",0.25}},true}}, {"rgb","aberration","distort"}});return l;}
}
