#include "application/app.h"
#include "core/model.h"
#include "shaders/shader.h"
#include <vector>

const int width  = 800;
const int height = 800;
std::vector<Model*> scene_models;


int main(int argc, char** argv) {
    // loading models
    // scene_models.push_back(new Model("../assets/obj/african_head/african_head.obj"));
    // scene_models.push_back(new Model("../assets/obj/african_head/african_head_eye_inner.obj"));
    // scene_models.push_back(new Model("../assets/obj/african_head/african_head_eye_outer.obj"));
    
    scene_models.push_back(new Model("../assets/obj/diablo3_pose/diablo3_pose.obj"));
    
    // scene_models.push_back(new Model("../assets/obj/boggie/body.obj"));
    // scene_models.push_back(new Model("../assets/obj/boggie/eyes.obj"));
    // scene_models.push_back(new Model("../assets/obj/boggie/head.obj"));

    scene_models.push_back(new Model("../assets/obj/floor.obj"));
    // Create app instance
    Application app(width, height, scene_models);
    app.run();

    return 0;
}
