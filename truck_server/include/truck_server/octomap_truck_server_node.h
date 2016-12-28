#include <ros/ros.h>
#include <truck_server/TruckOctomapServer.h>
#include <unistd.h>
#include <geometry_msgs/Vector3.h>
#include <std_msgs/Empty.h>
#include <iostream>

using namespace octomap_server;

struct aStarDataType
{
  point3d pos;
  double g_val;
  double h_val;
  double f_val;
};

class TruckServerNode
{
public:
  ros::NodeHandle nh_;
  ros::Subscriber sub_point_occupied_query_;
  ros::Subscriber sub_truck_octomap_flag_;
  ros::Subscriber sub_point_depth_query_;
  ros::Publisher pub_point_octocube_;

  std::vector<aStarDataType> points_open_set, points_close_set;

  TruckOctomapServer truck_;
  void pointOccupiedQueryCallback(const geometry_msgs::Vector3ConstPtr& msg);
  void pointDepthQueryCallback(const geometry_msgs::Vector3ConstPtr& msg);
  void truckOctomapCallback(const std_msgs::Empty msg);
  void onInit();
  // return true if the grid is free
  bool getGridCenter(point3d query_point, point3d& center_point, int depth);
  void aStarSearch(point3d start_point, point3d end_point);
};

void TruckServerNode::onInit()
{
  sub_point_occupied_query_ = nh_.subscribe<geometry_msgs::Vector3>("/query_point_occupied", 10, &TruckServerNode::pointOccupiedQueryCallback, this);
  sub_truck_octomap_flag_ = nh_.subscribe<std_msgs::Empty>("/truck_octomap_flag", 1, &TruckServerNode::truckOctomapCallback, this);
  sub_point_depth_query_ = nh_.subscribe<geometry_msgs::Vector3>("/query_point_depth", 1, &TruckServerNode::pointDepthQueryCallback, this);

  pub_point_octocube_ = nh_.advertise<visualization_msgs::Marker>("octo_cube_marker", 1);

  std::cout << "onInit finished.\n";
  ROS_INFO("onInit");
}

void TruckServerNode::pointOccupiedQueryCallback(const geometry_msgs::Vector3ConstPtr& msg){
  ROS_INFO("Query");
  std::cout << "query comes.\n";
  point3d query(msg->x, msg->y, msg->z);
  OcTreeNode* result = truck_.m_octree->search (query);
  if(result == NULL)
    {
      std::cout << "Unknown point" << query << std::endl;
    }
  else
    {
      std::cout << "occupancy probability at " << query << ":\t " << result->getOccupancy() << std::endl;
      std::cout << "Logodds at " << query << ":\t " << result->getLogOdds() << std::endl << std::endl;
    }
}

void TruckServerNode::truckOctomapCallback(const std_msgs::Empty msg)
{
  // x,y,z,r,p,y
  truck_.WriteVehicleOctree(0, Pose6D(0.0f, 0.0f, 0.0f, 0.0, 0.0, 0.0));
  truck_.WriteVehicleOctree(1, Pose6D(0.0f, 3.5f, 0.0f, 0.0, 0.0, 0.0));
  truck_.WriteVehicleOctree(2, Pose6D(0.0f, -3.5f, 0.0f, 0.0, 0.0, 0.0));
  truck_.laneMarkerVisualization();

 truck_.publishTruckAll(ros::Time().now());
 usleep(1000000);
}

void TruckServerNode::pointDepthQueryCallback(const geometry_msgs::Vector3ConstPtr& msg){
  ROS_INFO("QueryDepth");
  point3d query(msg->x, msg->y, msg->z);
  std::cout << msg->x <<' ' << msg->y << ' ' << msg->z <<'\n';
  OcTreeNode* result = truck_.m_octree->search (query, 0);
  // Depth from 1 to 16
  int cur_depth = result->depth;
  double cube_size = truck_.m_octree->resolution * pow(2, truck_.m_octree->tree_depth-cur_depth);
  std::cout << "Point is " << cur_depth << "\n";
  std::cout << "Prob is  " << result->getOccupancy() << "\n";
  point3d center;
  getGridCenter(query, center, cur_depth);

  visualization_msgs::Marker query_point_marker, octo_cube_marker;
  octo_cube_marker.ns = query_point_marker.ns = "octocubes";
  octo_cube_marker.header.frame_id = query_point_marker.header.frame_id = std::string("/world");
  octo_cube_marker.header.stamp = query_point_marker.header.stamp = ros::Time().now();
  octo_cube_marker.action = query_point_marker.action = visualization_msgs::Marker::ADD;
  octo_cube_marker.id = 0;
  query_point_marker.id = 1;
  octo_cube_marker.type = visualization_msgs::Marker::CUBE;
  query_point_marker.type = visualization_msgs::Marker::CUBE;

  query_point_marker.pose.position.x = msg->x;
  query_point_marker.pose.position.y = msg->y;
  query_point_marker.pose.position.z = msg->z;
  query_point_marker.pose.orientation.x = 0.0;
  query_point_marker.pose.orientation.y = 0.0;
  query_point_marker.pose.orientation.z = 0.0;
  query_point_marker.pose.orientation.w = 1.0;
  query_point_marker.scale.x = 0.1;
  query_point_marker.scale.y = 0.1;
  query_point_marker.scale.z = 0.1;
  query_point_marker.color.a = 1.0;
  query_point_marker.color.r = 0.8f;
  query_point_marker.color.g = 0.0f;
  query_point_marker.color.b = 0.0f;

  octo_cube_marker.pose.position.x = center.x();
  octo_cube_marker.pose.position.y = center.y();
  octo_cube_marker.pose.position.z = center.z();
  octo_cube_marker.pose.orientation.x = 0.0;
  octo_cube_marker.pose.orientation.y = 0.0;
  octo_cube_marker.pose.orientation.z = 0.0;
  octo_cube_marker.pose.orientation.w = 1.0;
  octo_cube_marker.scale.x = cube_size;
  octo_cube_marker.scale.y = cube_size;
  octo_cube_marker.scale.z = cube_size;
  octo_cube_marker.color.a = 0.5;
  octo_cube_marker.color.r = 0.0f;
  octo_cube_marker.color.g = 0.0f;
  octo_cube_marker.color.b = 1.0f;

  pub_point_octocube_.publish(query_point_marker);
  //sleep(1.5);
  pub_point_octocube_.publish(octo_cube_marker);

}

bool TruckServerNode::getGridCenter(point3d query_point, point3d& center_point, int depth)
{
  bool isGridFree = true;
  // when not have prior knowledge of depth, assign depth as -1
  if (depth == -1)
    {
        OcTreeNode* result = truck_.m_octree->search (query_point, 0);
        depth = result->depth;
        // 0.4 is free, 0.97 is occupied
        if (result->getOccupancy() > 0.8)
          isGridFree = false;
    }
  key_type key_x = truck_.m_octree->coordToKey(query_point.x(), depth);
  key_type key_y = truck_.m_octree->coordToKey(query_point.y(), depth);
  key_type key_z = truck_.m_octree->coordToKey(query_point.z(), depth);
  double center_x = truck_.m_octree->keyToCoord(key_x, depth);
  double center_y = truck_.m_octree->keyToCoord(key_y, depth);
  double center_z = truck_.m_octree->keyToCoord(key_z, depth);
  center_point.x() = center_x;
  center_point.y() = center_y;
  center_point.z() = center_z;
  return isGridFree;
}

void TruckServerNode::aStarSearch(point3d start_point, point3d end_point)
{
  OcTreeNode* start_node = truck_.m_octree->search (start_point, 0);
  int start_node_depth = start_node->depth;
  double start_grid_size = truck_.m_octree->resolution * pow(2, truck_.m_octree->tree_depth-start_node_depth);
  double neighbor_grid_gap = start_grid_size + truck_.m_octree->resolution/2.0;
  for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y)
      for (int z = -1; z <= 1; ++z)
        {
          point3d neighbor_center_point;
          if (getGridCenter((start_point + point3d(neighbor_grid_gap*x,
                                                   neighbor_grid_gap*y,
                                                   neighbor_grid_gap*z)),
                            neighbor_center_point, -1))
            {
              aStarDataType new_node_data;
              new_node_data.pos = neighbor_center_point;
              new_node_data.g_val = start_point.distance(neighbor_center_point);
              new_node_data.h_val = neighbor_center_point.distance(end_point);
              new_node_data.f_val = new_node_data.g_val + new_node_data.h_val;
              points_open_set.push_back(new_node_data);
            }
        }
}
