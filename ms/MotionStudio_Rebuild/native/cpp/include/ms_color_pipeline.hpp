#pragma once
#include "ms_image.hpp"
#include "ms_error.hpp"
#include <string>
#include <vector>
namespace ms { struct ColorSpaceDesc{std::string name;bool is_display=false;float gamma=1.f;}; class ColorPipeline{std::vector<ColorSpaceDesc>spaces_;std::string working_="Linear",display_="sRGB";public:ColorPipeline();const std::vector<ColorSpaceDesc>&spaces()const{return spaces_;}void set_working(const std::string&n){working_=n;}void set_display(const std::string&n){display_=n;}const std::string&working()const{return working_;}const std::string&display()const{return display_;}Error transform(ImageBuffer&,const std::string&,const std::string&)const;Error apply_grade(ImageBuffer&,float,float,float)const;Error apply_display_transform(ImageBuffer&i)const{return transform(i,working_,display_);}};}
