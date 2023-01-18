#include "SceneView.h"

namespace OM3D {

SceneView::SceneView(const Scene* scene) : _scene(scene) {
}

Camera& SceneView::camera() {
    return _camera;
}

const Camera& SceneView::camera() const {
    return _camera;
}

const Scene* SceneView::scene() const {
    return _scene;
}

void SceneView::set_scene(const Scene* scene) {
    if (scene) {
        _scene = scene;
    }
}

RenderInfo SceneView::render() const {
    if(_scene) {
        return _scene->render(_camera);
    }
    return {};
}

RenderInfo SceneView::render_sun_shadowmap() const {
    if(_scene) {
        return _scene->render_sun_shadowmap(_camera);
    }
    return {};
}

}
