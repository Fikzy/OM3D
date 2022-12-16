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

void SceneView::render() const {
    if(_scene) {
        _scene->render(_camera);
    }
}

}
