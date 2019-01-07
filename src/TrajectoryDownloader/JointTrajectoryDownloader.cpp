#include "JointTrajectoryDownloader.h"
#include "simple_message/joint_traj_pt.h"
#include "industrial_utils/param_utils.h"

using namespace industrial_utils::param;
typedef industrial::joint_traj_pt::JointTrajPt rbt_JointTrajPt;
typedef trajectory_msgs::JointTrajectoryPoint  ros_JointTrajPt;

#define ROS_ERROR_RETURN(rtn,...) do {ROS_ERROR(__VA_ARGS__); return(rtn);} while(0)

JointTrajectoryDownloader::JointTrajectoryDownloader()
{
}

JointTrajectoryDownloader::~JointTrajectoryDownloader()
{

	trajectoryStop();
	this->sub_joint_trajectory_.shutdown();
}
bool JointTrajectoryDownloader::init(std::string default_ip, int default_port)
{
	std::string robotName, ip;
	int port;

	// override IP/port with ROS params, if available
	ros::param::param<std::string>("robot_name", robotName, "");
	ros::param::param<std::string>("robot_ip_address", ip, default_ip);
  	ros::param::param<int>("~port", port, default_port);

	// check for valid parameter values
	if (robotName.empty())
	{
		ROS_ERROR("No valid robot's name found.  Please set ROS 'robot_name' param");
		return false;
	}
	if (ip.empty())
	{
		ROS_ERROR("No valid robot IP address found.  Please set ROS 'robot_ip_address' param");
		return false;
	}
	if (port <= 0)
	{
		ROS_ERROR("No valid robot IP port found.  Please set ROS '~port' param");
		return false;
	}

	/* Make socket connection */
	/********************************/
	_indySocket.init(robotName, ip, port);
	// ROS_INFO("JointTrajectoryDownloader : Init MotionSocket(%s, %s, %d)", robotName.c_str(), ip.c_str(), port);

	this->srv_stop_motion_ = this->node_.advertiseService("stop_motion", &JointTrajectoryDownloader::stopMotionCB, this);
	this->srv_joint_trajectory_ = this->node_.advertiseService("joint_path_command", &JointTrajectoryDownloader::jointTrajectoryCB, this);
	this->sub_joint_trajectory_ = this->node_.subscribe("joint_path_command", 1, &JointTrajectoryDownloader::jointTrajectoryCB, this);
	this->sub_cur_pos_ = this->node_.subscribe("joint_states", 1, &JointTrajectoryDownloader::jointStateCB, this);

	this->pub_joint_control_state_ = this->node_.advertise<control_msgs::FollowJointTrajectoryFeedback>("feedback_states", 1);

	return true;
}

void JointTrajectoryDownloader::trajectoryStop()
{
	ROS_INFO("Joint trajectory handler: entering stopping state");
	ROS_DEBUG("Sending stop command");
}

bool JointTrajectoryDownloader::stopMotionCB(industrial_msgs::StopMotion::Request &req,
		                                    industrial_msgs::StopMotion::Response &res)
{
	trajectoryStop();

	// no success/fail result from trajectoryStop.  Assume success.
	res.code.val = industrial_msgs::ServiceReturnCode::SUCCESS;

	return true;  // always return true.  To distinguish between call-failed and service-unavailable.
}


bool JointTrajectoryDownloader::jointTrajectoryCB(industrial_msgs::CmdJointTrajectory::Request &req,
                                                 industrial_msgs::CmdJointTrajectory::Response &res)
{

	trajectory_msgs::JointTrajectoryPtr traj_ptr(new trajectory_msgs::JointTrajectory);
	*traj_ptr = req.trajectory;  // copy message data

	this->jointTrajectoryCB(traj_ptr);

	// no success/fail result from jointTrajectoryCB.  Assume success.
	res.code.val = industrial_msgs::ServiceReturnCode::SUCCESS;

	return true;  // always return true.  To distinguish between call-failed and service-unavailable.
}

void JointTrajectoryDownloader::jointTrajectoryCB(const trajectory_msgs::JointTrajectoryConstPtr &msg)
{
	unsigned int numPoints = msg->points.size();

	// check for STOP command
	if (numPoints <= 0)
	{
		ROS_INFO("Empty trajectory received, canceling current trajectory");
		trajectoryStop();
		return;
	}

	// send command messages to robot
	if (_indySocket.isWorking())
	{
		Data data;
		data.int2dArr[0] = 11; // Extended Joint Waypoint Set Command
		data.int2dArr[1] = numPoints*(JOINT_DOF*sizeof(double)); // Extended Waypoint Set Command

		if (!_indySocket.sendCommand(800, data, 8))
			return;

		unsigned char * exDataBuff = new unsigned char[numPoints*(JOINT_DOF*sizeof(double))];
		unsigned int exIdx = 0;
		for (int i = 0; i < numPoints; i++)
		{			
			for (int j = 0; j < JOINT_DOF; j++)
				_jTar[j] = msg->points[i].positions[j];
			
			memcpy(exDataBuff + exIdx, _jTar, JOINT_DOF*sizeof(double));
			exIdx += JOINT_DOF*sizeof(double);
		}
		
		bool res = _indySocket.sendExData(exDataBuff, numPoints*(JOINT_DOF*sizeof(double)));
		delete [] exDataBuff;
	}
}

// copy robot JointState into local cache
void JointTrajectoryDownloader::jointStateCB(const sensor_msgs::JointStateConstPtr &msg)
{
  this->cur_joint_pos_ = *msg;
  // printf("Received Joint States: ");
  // for (int i = 0; i < JOINT_DOF; i++)
  // 	printf("[%s]: %f, ", this->cur_joint_pos_.name[i].c_str(), this->cur_joint_pos_.position[i]*180.0/3.1415);
  // printf("\n");
}

void JointTrajectoryDownloader::run()
{
	ros::Rate loop_rate(1);
	control_msgs::FollowJointTrajectoryFeedback control_state;
	while (ros::ok())
	{
		control_state.header.stamp = ros::Time::now();
		control_state.joint_names = cur_joint_pos_.name;
		control_state.actual.positions = cur_joint_pos_.position;
		this->pub_joint_control_state_.publish(control_state);
		ros::spinOnce();
		loop_rate.sleep();
	} 
}