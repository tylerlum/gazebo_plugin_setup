#include <functional>
#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>
#include <ignition/math/Vector3.hh>

namespace gazebo
{
  class GZ_RENDERING_VISIBLE SimpleVisualPlugin : public VisualPlugin
  {
    public: void Load(rendering::VisualPtr _visual, sdf::ElementPtr _sdf)
    {
      gzmsg << "IN Simple Visual" << std::endl;

      // Listen to the update event. This event is broadcast every
      // simulation iteration.
      this->updateConnection = event::Events::ConnectRender(
          std::bind(&SimpleVisualPlugin::OnUpdate, this));
    }

    // Called by the world update start event
    public: void OnUpdate()
    {
    }

    // Pointer to the update event connection
    private: event::ConnectionPtr updateConnection;
  };

  // Register this plugin with the simulator
  GZ_REGISTER_VISUAL_PLUGIN(SimpleVisualPlugin)
}
