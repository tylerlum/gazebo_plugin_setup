/*
 * Copyright (C) 2019  Rhys Mainwaring
 * * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <memory>
#include <thread>
#include <vector>
#include <string>

#include <gazebo/gazebo.hh>
#include <gazebo/common/Event.hh>
#include <gazebo/common/Time.hh>
#include <gazebo/common/Plugin.hh>
#include <gazebo/rendering/rendering.hh>
#include <gazebo/rendering/RenderTypes.hh>
#include <gazebo/rendering/Scene.hh>
#include <gazebo/rendering/Visual.hh>
#include <gazebo/transport/transport.hh>
#include <gazebo/transport/Node.hh>

#include <ignition/math/Vector2.hh>
#include <ignition/math/Vector3.hh>

#include "gazebo/rendering/RTShaderSystem.hh"
#include "gazebo/rendering/ogre_gazebo.h"

#include "ocean_visual.hh"

using namespace gazebo;

namespace asv
{
  GZ_REGISTER_VISUAL_PLUGIN(WavefieldVisualPlugin)

// WavefieldVisualPluginPrivate

  /// \internal
  /// \brief Private data for the WavefieldVisualPlugin
  class WavefieldVisualPluginPrivate
  {
    /// \brief The visual containing this plugin.
    public: rendering::VisualPtr visual;

    /// \brief The wavefield visual plugin SDF.
    public: sdf::ElementPtr sdf;

    /// \brief Event based connections.
    public: event::ConnectionPtr connection;

    // OGRE
    public: Ogre::RenderTarget *renderTarget;
    public: Ogre::RenderTarget *refractionRt;
    public: Ogre::Camera *camera;
    public: Ogre::Camera *trackedCamera;
    public: Ogre::SceneNode *planeNode;
    public: gazebo::rendering::ScenePtr scene;
    public: Ogre::Plane plane;
    public: Ogre::Entity* planeEntity;
    public: Ogre::TexturePtr rttReflectionTexture;
    public: Ogre::TexturePtr rttRefractionTexture;
  };

///////////////////////////////////////////////////////////////////////////////
// WavefieldVisualPlugin

  WavefieldVisualPlugin::~WavefieldVisualPlugin()
  {
    // Reset connections and transport.
    this->data->connection.reset();
  }

  WavefieldVisualPlugin::WavefieldVisualPlugin() :
    VisualPlugin(),
    RenderTargetListener(),
    data(new WavefieldVisualPluginPrivate())
  {
  }

  void WavefieldVisualPlugin::Load(
    rendering::VisualPtr _visual,
    sdf::ElementPtr _sdf)
  {
    gzerr << "IN OCEAN VISUAL" << std::endl;

    // Capture visual and plugin SDF
    GZ_ASSERT(_visual != nullptr, "Visual must not be null");
    GZ_ASSERT(_sdf != nullptr, "SDF Element must not be null");

    // Capture the visual and sdf.
    this->data->visual = _visual;
    this->data->sdf = _sdf;

    // OGRE setup
    this->data->scene = _visual->GetScene();

    // Only load plugin once
    // load the visual plugin in gzclient for now.
    if (!this->data->scene->EnableVisualizations())
      return;

    // Setup camera
    Ogre::Camera *userCamera = this->data->scene->GetUserCamera(0)->OgreCamera();
    if (userCamera)
    {
      this->data->trackedCamera = userCamera;
      this->data->camera =
        this->data->scene->OgreSceneManager()->createCamera(
        this->data->visual->Name() + "_fresnelCam");

      Ogre::SceneNode *node =
          this->data->scene->OgreSceneManager()->getRootSceneNode()->
          createChildSceneNode();
      node->attachObject(this->data->camera);
    }
    else
    {
      gzerr << "User camera not found" << std::endl;
      return;
    }


    Ogre::SceneNode *ogreNode = _visual->GetSceneNode();
    this->data->planeEntity =
        dynamic_cast<Ogre::Entity *>(ogreNode->getAttachedObject(0));
    if (!this->data->planeEntity)
    {
      gzerr << "No plane entity found" << std::endl;
      return;
    }

    // Create Plane for reflection texture
    // this->data->plane = new Ogre::MovablePlane("Plane");
    // this->data->plane->d = 0;
    // this->data->plane->normal = Ogre::Vector3::UNIT_Z;
    this->data->plane = Ogre::Plane(Ogre::Vector3::UNIT_Z, 0);
    // Ogre::MeshManager::getSingleton().createPlane(
    //   "PlaneMesh2",
    //   Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
    //   this->data->plane,
    //   512, 512, 1, 1,
    //   true,
    //   1, 1, 1,
    //   Ogre::Vector3::UNIT_Y);
    // this->data->planeEntity = this->data->scene->OgreSceneManager()->
    //     createEntity("PlaneMesh", "PlaneMesh2");
    // (this->data->planeNode = this->data->scene->OgreSceneManager()->
    //  getRootSceneNode()->createChildSceneNode());
    // this->data->planeNode->attachObject(this->data->planeEntity);
    // this->data->planeNode->attachObject(this->data->plane);

    // QUESTION: Create render texture, if I give it the same name as
    // the texture in scripts/waves.material, it would not work for some reason
    this->data->rttReflectionTexture =
      Ogre::TextureManager::getSingleton().createManual(
        this->data->visual->Name() + "_reflection",
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D,
        512, 512,
        0,
        Ogre::PF_R8G8B8,
        Ogre::TU_RENDERTARGET);

    this->data->rttRefractionTexture =
      Ogre::TextureManager::getSingleton().createManual(
        this->data->visual->Name() + "_refraction",
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D,
        512, 512,
        0,
        Ogre::PF_R8G8B8,
        Ogre::TU_RENDERTARGET);

    this->data->renderTarget =
        this->data->rttReflectionTexture->getBuffer()->getRenderTarget();
    this->data->renderTarget->setAutoUpdated(false);

    // Setup render texture
    Ogre::Viewport *vp = this->data->renderTarget->addViewport(this->data->camera);
    vp->setClearEveryFrame(true);
    vp->setOverlaysEnabled(false);
    vp->setVisibilityMask(GZ_VISIBILITY_ALL &
        ~(GZ_VISIBILITY_GUI | GZ_VISIBILITY_SELECTABLE));
    rendering::RTShaderSystem::AttachViewport(vp, this->data->scene);
    this->data->renderTarget->addListener(this);

    this->data->refractionRt =
        this->data->rttRefractionTexture->getBuffer()->getRenderTarget();
    this->data->refractionRt->setAutoUpdated(false);
    vp = this->data->refractionRt->addViewport(this->data->camera);
    vp->setClearEveryFrame(true);
    vp->setOverlaysEnabled(false);
    vp->setAutoUpdated(false);
    vp->setVisibilityMask(GZ_VISIBILITY_ALL &
        ~(GZ_VISIBILITY_GUI | GZ_VISIBILITY_SELECTABLE));
    rendering::RTShaderSystem::AttachViewport(vp, this->data->scene);
    this->data->refractionRt->addListener(this);

    Ogre::MaterialPtr origMat =
        Ogre::MaterialManager::getSingleton().getByName("fresnel");
    Ogre::MaterialPtr mat = origMat->clone(this->data->visual->Name() + "_mat");
    Ogre::TextureUnitState *reflectTex =
        mat->getTechnique(0)->getPass(0)->getTextureUnitState(1);
    reflectTex->setTexture(this->data->rttReflectionTexture);
    Ogre::TextureUnitState *refractTex =
        mat->getTechnique(0)->getPass(0)->getTextureUnitState(2);
    refractTex->setTexture(this->data->rttRefractionTexture);

    // Camera reflection and clip plane setup
    this->data->camera->enableReflection(this->data->plane);
    // this->data->camera->enableCustomNearClipPlane(this->data->plane);
    this->data->planeEntity->setMaterialName(mat->getName());

    // Bind the update method to ConnectPreRender events
    this->data->connection = event::Events::ConnectRender(
        std::bind(&WavefieldVisualPlugin::OnUpdate, this));
  }

  void WavefieldVisualPlugin::OnUpdate()
  {
    if (!this->data->camera || !this->data->trackedCamera ||
        !this->data->renderTarget)
      return;

    this->data->camera->setCustomProjectionMatrix(true,
      this->data->trackedCamera->getProjectionMatrix());
    this->data->camera->setPosition(
        this->data->trackedCamera->getDerivedPosition());
    this->data->camera->setOrientation(
        this->data->trackedCamera->getDerivedOrientation());

    this->data->renderTarget->update();
    // this->data->renderTarget->writeContentsToFile("reflection.png");
  }

  void WavefieldVisualPlugin::preRenderTargetUpdate(
      const Ogre::RenderTargetEvent& rte)
  {
    if (!this->data->camera)
      return;

    if (this->data->planeEntity)
    {
      this->data->planeEntity->setVisible(false);
    }

    // reflection
    if (rte.source == this->data->renderTarget)
    {
      this->data->camera->enableReflection(this->data->plane);
      // this->data->camera->enableCustomNearClipPlane(this->data->plane);
    }
    // refraction
    else
    {
      // TODO
    }
  }

  void WavefieldVisualPlugin::postRenderTargetUpdate(
      const Ogre::RenderTargetEvent& rte)
  {
    if (!this->data->camera)
      return;

    if (this->data->planeEntity)
    {
      this->data->planeEntity->setVisible(true);
    }

    // reflection
    if (rte.source == this->data->renderTarget)
    {
      this->data->camera->disableReflection();
      // this->data->camera->disableCustomNearClipPlane();
    }
    // refraction
    else
    {
      // TODO
    }
  }
}
