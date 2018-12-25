#ifndef JOINT_TRAJECTORY_DOWNLOADER_H
#define JOINT_TRAJECTORY_DOWNLOADER_H

#include <map>
#include <vector>
#include <string>

#include <ros/ros.h>
#include <industrial_msgs/CmdJointTrajectory.h>
#include "industrial_msgs/StopMotion.h"
#include "sensor_msgs/JointState.h"
#include "simple_message/messages/joint_traj_pt_message.h"
#include "trajectory_msgs/JointTrajectory.h"

#include "control_msgs/FollowJointTrajectoryFeedback.h"

#include "../SocketHandler/IndyDCPSocket.h"

using industrial::joint_traj_pt_message::JointTrajPtMessage;

class JointTrajectoryDownloader
{
	enum
	{
		JOINT_DOF = 6
	};

public:
	JointTrajectoryDownloader();
	~JointTrajectoryDownloader();

	void trajectoryStop();

	bool init(std::string default_ip = SERVER_IP, int default_port = SERVER_PORT);
	void run();

	void jointStateCB(const sensor_msgs::JointStateConstPtr &msg);
	bool stopMotionCB(industrial_msgs::StopMotion::Request &req,
                                    industrial_msgs::StopMotion::Response &res);
	void jointTrajectoryCB(const trajectory_msgs::JointTrajectoryConstPtr &msg);
	bool jointTrajectoryCB(industrial_msgs::CmdJointTrajectory::Request &req,
                         industrial_msgs::CmdJointTrajectory::Response &res);

private:
	ros::NodeHandle node_;
	ros::Subscriber sub_cur_pos_;  // handle for joint-state topic subscription
	ros::Subscriber sub_joint_trajectory_; // handle for joint-trajectory topic subscription
	ros::ServiceServer srv_joint_trajectory_;  // handle for joint-trajectory service
	ros::ServiceServer srv_stop_motion_;   // handle for stop_motion service
	ros::Publisher pub_joint_control_state_;
	sensor_msgs::JointState cur_joint_pos_;  // cache of last received joint state

	IndyDCPSocket _indySocket;

	double _jTar[JOINT_DOF];
};

#endif /* JOINT_TRAJECTORY_DOWNLOADER_H */