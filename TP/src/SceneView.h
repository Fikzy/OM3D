#ifndef SCENEVIEW_H
#define SCENEVIEW_H

#include <Scene.h>
#include <Camera.h>

namespace OM3D {

class SceneView {
    public:
        SceneView(Scene* scene = nullptr);

        Camera& camera();
        const Camera& camera() const;

        Scene* scene() const;
        void set_scene(Scene* scene);

        RenderInfo render() const;
        RenderInfo render_sun_shadowmap() const;

    private:
        Scene* _scene = nullptr;
        Camera _camera;

};

}

#endif // SCENEVIEW_H
