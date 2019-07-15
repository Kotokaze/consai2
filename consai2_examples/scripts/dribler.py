#!/usr/bin/env python2
# coding: UTF-8

import rospy
from consai2_msgs.msg import ControlTarget
from geometry_msgs.msg import Pose2D
from consai2_msgs.msg import VisionDetections, VisionGeometry, BallInfo, RobotInfo


ball_pose = Pose2D()

def path_example(target_id):
    
    
    # 制御目標値を生成
    control_target = ControlTarget()

    # ロボットID
    control_target.robot_id = target_id
    # Trueで走行開始
    control_target.control_enable = True

    # 経由位置1
    # print ball_pose
    pose = Pose2D()
    control_target.path.append(ball_pose)

    # ゴール地点での速度
    # goal_velocity = Pose2D(0.0, 0.0, 0.0)
    # control_target.goal_velocity = goal_velocity

    # キック威力 0.0 ~ 1.0
    control_target.kick_power = 1.0
    control_target.chip_enable = False
    control_target.dribble_power = 0.0

    return control_target

def BallPose(data):
    global ball_pose
    ball_pose = data.pose

def main():
    rospy.init_node('control_example')
    
    MAX_ID = rospy.get_param('consai2_description/max_id', 15)
    COLOR = "blue" # 'blue' or 'yellow'
    TARGET_ID = 3 # 0 ~ MAX_ID

    # 末尾に16進数の文字列をつける
    topic_id = hex(TARGET_ID)[2:]
    topic_name = 'consai2_game/control_target_' + COLOR +'_' + topic_id


    # print sub
    pub = rospy.Publisher(topic_name, ControlTarget, queue_size=1)

    print 'control_exmaple start'
    rospy.sleep(3.0)

    # 制御目標値を生成
    while 1:
        sub = rospy.Subscriber('vision_wrapper/ball_info', BallInfo, BallPose)
        control_target = path_example(TARGET_ID)
        pub.publish(control_target)
    print 'control_exmaple finish'

if __name__ == '__main__':
    main()