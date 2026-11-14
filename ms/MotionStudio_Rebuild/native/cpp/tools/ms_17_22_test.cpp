#include "ms_scene.hpp"
#include "ms_dcc_bridge.hpp"
#include "ms_color_pipeline.hpp"
#include "ms_openfx.hpp"
#include "ms_render_queue.hpp"
#include <iostream>
#include <filesystem>
static int fail=0;
#define CHECK(c,m) do{if(!(c)){std::cerr<<"FAIL: "<<m<<"\n";++fail;}else std::cout<<"  ok: "<<m<<"\n";}while(0)
int main(){using namespace ms;namespace fs=std::filesystem;std::cout<<"=== Motion Studio 17-22 ===\n";
 Scene s=make_default_studio_scene(640,360);auto *mesh=s.find(4);CHECK(mesh!=nullptr,"3D layer/mesh scene");if(mesh){mesh->material.name="HeroMaterial";mesh->material.metallic=.7f;mesh->material.roughness=.25f;}CHECK(s.to_job_json().find("material")!=std::string::npos,"3D material export");
 auto *cam=s.find(1);CHECK(cam&&cam->focal_mm==35.f,"camera model");auto *light=s.find(2);CHECK(light&&light->intensity>0,"light model");
 DccBridge b;auto r=b.submit(s,DccTarget::Blender,"/tmp/ms_17_22_blender",true);CHECK(fs::exists(r.primary_script),"Blender render job generation");CHECK(fs::exists("/tmp/ms_17_22_blender/scene_job.json"),"Blender scene snapshot");
 ColorPipeline cp;ImageBuffer img(2,2);img.clear(.5f,.25f,.75f,1);CHECK(cp.transform(img,"sRGB","Linear").ok(),"color transform sRGB->Linear");CHECK(cp.transform(img,"Linear","sRGB").ok(),"color transform Linear->display");CHECK(cp.apply_grade(img,.25f,1.1f,1.05f).ok(),"color grade");
 OpenFxRegistry fx;CHECK(fx.register_plugin(make_identity_ofx_plugin()).ok(),"OpenFX registration");ImageBuffer out;CHECK(fx.apply("org.motionstudio.Identity",img,out,{},0).ok()&&out.valid(),"OpenFX execution");
 RenderQueue q;QueueJob j;j.name="background-test";j.backend="realtime_gpu";j.desc.width=64;j.desc.height=64;q.enqueue(j);CHECK(q.start_async(),"background render start");q.wait();CHECK(!q.running(),"background render finished");CHECK(!q.jobs().empty()&&q.jobs()[0].state==JobState::Done,"render queue job done");
 std::cout<<(fail?"PASS 17-22 FAILED\n":"PASS 17-22 PASSED\n");return fail?1:0;}
