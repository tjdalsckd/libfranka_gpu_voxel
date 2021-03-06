// this is for emacs file handling -*- mode: c++; indent-tabs-mode: nil -*-

// -- BEGIN LICENSE BLOCK ----------------------------------------------
// This file is part of the GPU Voxels Software Library.
//
// This program is free software licensed under the CDDL
// (COMMON DEVELOPMENT AND DISTRIBUTION LICENSE Version 1.0).
// You can find a copy of this license in LICENSE.txt in the top
// directory of the source code.
//
// © Copyright 2018 FZI Forschungszentrum Informatik, Karlsruhe, Germany
//
// -- END LICENSE BLOCK ------------------------------------------------

//----------------------------------------------------------------------
/*!\file
 *
 * \author  Andreas Hermann
 * \date    2018-01-07
 *
 */
//----------------------------------------------------------------------/*
#include <iostream>
using namespace std;
#include <signal.h>

#include <gpu_voxels/logging/logging_gpu_voxels.h>
#include <trajectory_msgs/JointTrajectory.h>
#include <trajectory_msgs/JointTrajectoryPoint.h>
#define IC_PERFORMANCE_MONITOR
#include <icl_core_performance_monitor/PerformanceMonitor.h>
#include <unistd.h> 
#include <ompl/geometric/planners/sbl/SBL.h>
#include <ompl/geometric/planners/kpiece/LBKPIECE1.h>
#include <ompl/geometric/planners/fmt/FMT.h>

#include <kdl_parser/kdl_parser.hpp>
#include <kdl/chain.hpp>
#include <kdl/chainfksolver.hpp>
#include <kdl/frames.hpp>
#include <sensor_msgs/JointState.h>
#include <kdl/chainfksolverpos_recursive.hpp>

#include <kdl/chainiksolverpos_nr_jl.hpp>
#include <kdl/chainiksolverpos_nr.hpp>

#include <kdl/chainiksolvervel_pinv.hpp>
#include <kdl/frames_io.hpp>

#include <ompl/geometric/PathSimplifier.h>

#include "gvl_ompl_planner_helper.h"
#include <stdlib.h>
#include <ros/ros.h>
#include <thread>
#include <memory>

#include <Python.h>
#include <stdlib.h>
#include <vector>
namespace ob = ompl::base;
namespace og = ompl::geometric;

#define PI 3.141592
#define D2R 3.141592/180.0
#define R2D 180.0/3.141592
using namespace KDL;
using namespace std;
// initial quaternion 0.49996,0.86605,0.00010683,0



std::shared_ptr<GvlOmplPlannerHelper> my_class_ptr;
enum States { READY, FIND, GRASP, MOVE1,MOVE2,UNGRASP };


void goHome(){
    double start_values[7];
    my_class_ptr->getJointStates(start_values);

    std::vector<std::array<double,7>> q_list;
    q_list.clear();
    std::array<double,7> start = { {start_values[0],start_values[1],start_values[2],start_values[3],start_values[4],start_values[5],start_values[6]}};
    std::array<double,7> end =     {{0.000 ,-0.785 ,0.000 ,-2.356, 0.000 ,1.571 ,1.585/2}};
    std::array<double,7> mid =     {{start_values[0]*0.5+end.at(0)*0.5,start_values[1]*0.5+end.at(1)*0.5,start_values[2]*0.5+end.at(2)*0.5,start_values[3]*0.5+end.at(3)*0.5,start_values[4]*0.5+end.at(4)*0.5,start_values[5]*0.5+end.at(5)*0.5,start_values[6]*0.5+end.at(6)*0.5}};

    q_list.push_back(start);
    q_list.push_back(mid);
    q_list.push_back(end);
    q_list.push_back(end);

    my_class_ptr->rosPublishJointTrajectory(q_list);
    std::cout<<"Waiting for JointState"<<std::endl;
     std::cout << "Press Enter Key if ready!" << std::endl;
    std::cin.ignore();
    q_list.clear();
     std::cout<<"Recived JointState"<<std::endl;

        double  temp_values_[7] = {0.000 ,-0.785 ,0.000 ,-2.356, 0.000 ,1.571 ,1.585/2};
        double *temp_values= (double*)temp_values_;
        my_class_ptr->rosPublishJointStates(temp_values);
    	my_class_ptr->visualizeRobot(temp_values_);
}

std::vector<std::array<double,7>> doTaskPlanning(double* goal_values,double start_values[7]){


    PERF_MON_INITIALIZE(100, 1000);
    PERF_MON_ENABLE("planning");

    // construct the state space we are planning in
    auto space(std::make_shared<ob::RealVectorStateSpace>(7));
    //We then set the bounds for the R3 component of this state space:
    ob::RealVectorBounds bounds(7);
    bounds.setLow(-3.14159265);
    bounds.setHigh(3.14159265);
 
    space->setBounds(bounds);
    //Create an instance of ompl::base::SpaceInformation for the state space
    auto si(std::make_shared<ob::SpaceInformation>(space));
    std::shared_ptr<GvlOmplPlannerHelper> my_class_ptr(std::make_shared<GvlOmplPlannerHelper>(si));

    //Set the state validity checker
    // std::cout << "-------------------------------------------------" << std::endl;
    og::PathSimplifier simp(si);

    si->setStateValidityChecker(my_class_ptr->getptr());
    si->setMotionValidator(my_class_ptr->getptr());
    si->setup();
    KDL::Tree my_tree;
    KDL::Chain my_chain;


   if (!kdl_parser::treeFromFile("panda_coarse/panda_7link.urdf", my_tree)){
      ROS_ERROR("Failed to construct kdl tree");
   }

    LOGGING_INFO(Gpu_voxels, "\n\nKDL Number of Joints : "<<my_tree.getNrOfJoints() <<"\n"<< endl);
    LOGGING_INFO(Gpu_voxels, "\n\nKDL Chain load : "<<my_tree.getChain("panda_link0","panda_link8",my_chain) <<"\n"<< endl);
  
  //  std::cout<<my_chain.getNrOfJoints()<<std::endl;


    KDL::JntArray q1(my_chain.getNrOfJoints());
    KDL::JntArray q_init(my_chain.getNrOfJoints());
  
    //std::cout<<my_chain.getNrOfJoints()<<std::endl;

//    double start_values[7];
//    my_class_ptr->getJointStates(start_values);
    q_init(0) = start_values[0];
    q_init(1) = start_values[1];
    q_init(2) = start_values[2];
    q_init(3) = start_values[3];
    q_init(4) = start_values[4];
    q_init(5) = start_values[5];
    q_init(6) = start_values[6];
    KDL::JntArray q_min(my_chain.getNrOfJoints()),q_max(my_chain.getNrOfJoints());
    q_min(0) = -2.8973;
    q_min(1) = -1.7628;
    q_min(2) = -2.8973;
    q_min(3) = -3.0718;
    q_min(4) = -2.8973;
    q_min(5) = -0.0175;
    q_min(6) = -2.8973;

    q_max(0) = 2.8973;
    q_max(1) = 1.7628;
    q_max(2) = 2.8973;
    q_max(3) = -0.0698;
    q_max(4) = 2.8973;
    q_max(5) = 3.7525;
    q_max(6) = 2.8973;

    KDL::Frame cart_pos;
    KDL::ChainFkSolverPos_recursive fk_solver = KDL::ChainFkSolverPos_recursive(my_chain);
    fk_solver.JntToCart(q_init, cart_pos);
   // cout<<cart_pos<<endl;
    KDL::ChainIkSolverVel_pinv iksolver1v(my_chain);
    KDL::ChainIkSolverPos_NR_JL iksolver1(my_chain,q_min,q_max,fk_solver,iksolver1v,2000,0.01);
  	KDL::Vector pos = KDL::Vector(0.05,-0.1865,1.20);
  	
    //KDL::Frame goal_pose( KDL::Rotation::RPY(goal_values[0],goal_values[1],goal_values[2]),KDL::Vector(goal_values[3],goal_values[4],goal_values[5]));
    KDL::Frame goal_pose( KDL::Rotation::Quaternion(goal_values[0],goal_values[1],goal_values[2],goal_values[3]),KDL::Vector(goal_values[4],goal_values[5],goal_values[6]));

    bool ret = iksolver1.CartToJnt(q_init,goal_pose,q1);
   // std::cout<<"ik ret : "<<ret<<std::endl;
   // std::cout<<"ik q : "<<q1(0)<<","<<q1(1)<<","<<q1(2)<<","<<q1(3)<<","<<q1(4)<<","<<q1(5)<<","<<q1(6)<<std::endl;
    
    ob::ScopedState<> start(space);


    start[0] = double(q_init(0));
    start[1] = double(q_init(1));
    start[2] = double(q_init(2));
    start[3] = double(q_init(3));
    start[4] = double(q_init(4));
    start[5] = double(q_init(5));
    start[6] = double(q_init(6));

   ob::ScopedState<> goal(space);
    goal[0] = double(q1(0));
    goal[1] =  double(q1(1));
    goal[2] =  double(q1(2));
    goal[3] = double(q1(3));
    goal[4] =  double(q1(4));
    goal[5] = double(q1(5));
    goal[6] = double(q1(6));
    

	my_class_ptr->insertStartAndGoal(start, goal);
    my_class_ptr->doVis();

    auto pdef(std::make_shared<ob::ProblemDefinition>(si));
    pdef->setStartAndGoalStates(start, goal);
    auto planner(std::make_shared<og::LBKPIECE1>(si));
    planner->setProblemDefinition(pdef);
    planner->setup();

    int succs = 0;

    std::system("clear");
    ob::PathPtr path ;

     while(succs<3)
    {
        try{
                my_class_ptr->moveObstacle();
                planner->clear();
                ob::PlannerStatus  solved = planner->ob::Planner::solve(200.0);
                PERF_MON_SILENT_MEASURE_AND_RESET_INFO_P("planner", "Planning time", "planning");


                if (solved)
                {
                    ++succs;
                    path = pdef->getSolutionPath();
                    //std::cout << "Found solution:" << std::endl;
                    path->print(std::cout);
                    simp.simplifyMax(*(path->as<og::PathGeometric>()));

                }else{
                    std::cout << "No solution could be found" << std::endl;
                }

                PERF_MON_SUMMARY_PREFIX_INFO("planning");
                std::cout << "END OMPL" << std::endl;
                my_class_ptr->doVis();
               }
        catch(int expn){
        }


    }
    PERF_MON_ADD_STATIC_DATA_P("Number of Planning Successes", succs, "planning");

    PERF_MON_SUMMARY_PREFIX_INFO("planning");

    // keep the visualization running:
    og::PathGeometric* solution= path->as<og::PathGeometric>();
    solution->interpolate();

    int step_count = solution->getStateCount();

    std::vector<std::array<double,7>> q_list;
    q_list.clear();
    for(int i=0;i<step_count;i++){
        const double *values = solution->getState(i)->as<ob::RealVectorStateSpace::StateType>()->values;
        double *temp_values = (double*)values;
        std::array<double,7> temp_joints_value={{temp_values[0],temp_values[1],temp_values[2],temp_values[3],temp_values[4],temp_values[5],temp_values[6]}};
        q_list.push_back(temp_joints_value);
        //my_class_ptr->rosPublishJointStates(temp_values);
    	//my_class_ptr->visualizeRobot(values);
	
     }
    return q_list;
    my_class_ptr->rosPublishJointTrajectory(q_list);
    std::system("clear");

    q_list.clear();


}

Eigen::Matrix<float, 4, 4>  loadText(string filename){
     string testline;
    string word[4][4];
    Eigen::Matrix<float, 4, 4> TBaseToCamera = Eigen::Matrix<float, 4, 4>::Identity();
    ifstream Test (filename);

    if (!Test)
    {
        cout << "There was an error opening the file.\n";
        return TBaseToCamera;
    }
    int x=0,y=0;
    while( Test>>testline ){
        word[y][x]=testline;
        x++;
        if (testline=="")
        y++;
    }
        for (int y=0;y<4;y++)
        {
            for (int x=0;x<4;x++)
                 TBaseToCamera(y,x)= std::stod(word[y][x]);
        }
    return TBaseToCamera;
}


std::array<double,7>  loadPosition(string filename){
     string testline;
    string word[7];
    std::array<double,7>  result = {{0,0,0,0,0,0,0}};
    ifstream Test (filename);

    if (!Test)
    {
        cout << "There was an error opening the file.\n";
        return result;
    }
    int y=0;

 
    while( Test>>testline ){
        word[y]=testline;
        y++;
    }
        for (int y=0;y<7;y++)
        {
                 result.at(y)= std::stod(word[y]);
        }
    return result;
}

int main(int argc, char **argv)
{
    float roll = atof(argv[1]);
    float pitch = atof(argv[2]);
    float yaw = atof(argv[3]);
    float X = atof(argv[4]);
    float Y = atof(argv[5]);
    float Z = atof(argv[6]);
Eigen::Matrix<float, 4, 4>  TBaseToCamera = loadText("TBaseToCamera.txt");
cout<<"==========TBaseToCmaera.txt==========\n";
std::cout<<TBaseToCamera<<std::endl;
std::cout<<"========Input Roll Pitch Yaw X Y Z========="<<std::endl;
std::cout<<roll<<"\t"<<pitch<<"\t"<<yaw<<"\t"<<X<<"\t"<<Y<<"\t"<<Z<<std::endl;
roll = roll*PI/180;
pitch = pitch*PI/180;
yaw = yaw*PI/180;
Eigen::Matrix<float, 4, 4> inputT  = Eigen::Matrix<float, 4, 4>::Identity();
Eigen::Matrix<float, 3, 3> Rx  = Eigen::Matrix<float, 3, 3>::Identity();
Eigen::Matrix<float, 3, 3> Ry  = Eigen::Matrix<float, 3, 3>::Identity();
Eigen::Matrix<float, 3, 3> Rz  = Eigen::Matrix<float, 3, 3>::Identity();
Rx(1,1) = cos(roll);
Rx(1,2) = -sin(roll);
Rx(2,1) = sin(roll);
Rx(2,2) = cos(roll);
Ry(0,0) = cos(pitch);
Ry(0,2) = sin(pitch);
Ry(2,0) = -sin(pitch);
Ry(2,2) = cos(pitch);
Rz(0,0) = cos(yaw);
Rz(0,1) = -sin(yaw);
Rz(1,0) = sin(yaw);
Rz(1,1) = cos(yaw);
Eigen::Matrix<float, 3, 3>  R = Rx*Ry*Rz;
inputT(0,0)=R(0,0);
inputT(0,1)=R(0,1);
inputT(0,2)=R(0,2);
inputT(1,0)=R(1,0);
inputT(1,1)=R(1,1);
inputT(1,2)=R(1,2);
inputT(2,0)=R(2,0);
inputT(2,1)=R(2,1);
inputT(2,2)=R(2,2);
inputT(0,3) = X;
inputT(1,3) = Y;
inputT(2,3) = Z;
std::cout<<"========Input T========="<<std::endl;
std::cout<<inputT<<std::endl;
std::cout<<"========InputT*TBaseToCmaera========="<<std::endl;
TBaseToCamera = inputT*TBaseToCamera;
std::cout<<TBaseToCamera<<std::endl;


cout<<"==========TargetPosition.txt==========\n";
std::array<double,7> targetPosition = loadPosition("TargetPosition.txt");
std::cout<<targetPosition.at(0)<<","<<targetPosition.at(1)<<","<<targetPosition.at(2)<<","<<targetPosition.at(3)<<","<<targetPosition.at(4)<<","<<targetPosition.at(5)<<","<<targetPosition.at(6)<<std::endl;
std::cout<<"===================================="<<std::endl;
std::cout << "Press Enter Key if ready!" << std::endl;


std::cin.ignore();
 

signal(SIGINT, ctrlchandler);
  signal(SIGTERM, killhandler);

    icl_core::logging::initialize(argc, argv);

    PERF_MON_INITIALIZE(100, 1000);
    PERF_MON_ENABLE("planning");

    // construct the state space we are planning in
    auto space(std::make_shared<ob::RealVectorStateSpace>(7));
    //We then set the bounds for the R3 component of this state space:
    ob::RealVectorBounds bounds(7);
    bounds.setLow(-3.14159265);
    bounds.setHigh(3.14159265);
 	bounds.setLow(0,-2.8973);
    bounds.setHigh(0,2.9671);

    bounds.setLow(1,-1.7628);
    bounds.setHigh(1,1.7628);

    bounds.setLow(2,-2.8973);
    bounds.setHigh(2,2.8973);

    bounds.setLow(3,-3.0718);
    bounds.setHigh(3,-0.0698);

    bounds.setLow(4,-2.8973);
    bounds.setHigh(4,2.8973);

    bounds.setLow(5,-0.0175);
    bounds.setHigh(5,3.7525);

    bounds.setLow(6,-2.8973);
    bounds.setHigh(6,2.8973);

 
    space->setBounds(bounds);
    //Create an instance of ompl::base::SpaceInformation for the state space
    auto si(std::make_shared<ob::SpaceInformation>(space));
    //Set the state validity checker
    std::shared_ptr<GvlOmplPlannerHelper> my_class_ptr(std::make_shared<GvlOmplPlannerHelper>(si));
    my_class_ptr->doVis();

    

    //my_class_ptr->setParams(roll,pitch,yaw,X,Y,Z);
    my_class_ptr->setTransformation(TBaseToCamera);


    thread t1{&GvlOmplPlannerHelper::rosIter ,my_class_ptr};    
    thread t2{&GvlOmplPlannerHelper::doVis2 ,my_class_ptr};    

    //thread t2(jointStateCallback);

    sleep(10);
   
   States state = READY;
   int toggle = 1;
double task_goal_values00[7] ={targetPosition.at(0),targetPosition.at(1),targetPosition.at(2),targetPosition.at(3),targetPosition.at(4),targetPosition.at(5),targetPosition.at(6)};
double task_goal_values11[7] ={targetPosition.at(0),targetPosition.at(1),targetPosition.at(2),targetPosition.at(3),targetPosition.at(4),-targetPosition.at(5),targetPosition.at(6)};
      
        while(1){
        std::system("clear");
        int continue_value=0;
        std::cout << "Press Enter Key if ready!" << std::endl;
        std::cin.ignore();

        std::cout<<"Start Motion 1"<<std::endl;    
      my_class_ptr->isMove(1);
	double start_values[7]={0.000 ,-0.785 ,0.000 ,-2.356, 0.000 ,1.571 ,1.585/2};
       std::vector<std::array<double,7>> q_list1=doTaskPlanning(task_goal_values00,start_values);
       
       std::array<double,7> endq =  q_list1.at(q_list1.size()-1);
        for(int j =0;j<7;j++)
             start_values[j]=endq[j];
       std::system("clear");
        std::cout<<"Start Motion 2"<<std::endl;
       
       std::vector<std::array<double,7>> q_list2=doTaskPlanning(task_goal_values11,start_values);  
       
        endq =  q_list2.at(q_list2.size()-1);
        for(int j =0;j<7;j++)
             start_values[j]=endq[j];
        std::system("clear");
        std::cout<<"Start Motion 3"<<std::endl;
       std::vector<std::array<double,7>> q_list3=doTaskPlanning(task_goal_values00,start_values);
       std::system("clear");


        std::system("clear");
        std::cout << "Calculation Complete. Press Enter Key if ready!" << std::endl;
        std::cin.ignore();
            my_class_ptr->rosPublishJointTrajectory(q_list1);
            std::system("clear");
            std::cout<<"Waiting for JointState"<<std::endl;
             std::cout << "Press Enter Key if ready!" << std::endl;
            std::cin.ignore();
            std::cout<<"Recived JointState"<<std::endl;
            q_list1.clear();
            my_class_ptr->rosPublishJointTrajectory(q_list2);
            std::system("clear");
            std::cout<<"Waiting for JointState"<<std::endl;

             std::cout << "Press Enter Key if ready!" << std::endl;
            std::cin.ignore();

            std::cout<<"Recived JointState"<<std::endl;
            q_list2.clear();                
            my_class_ptr->rosPublishJointTrajectory(q_list3);
            std::system("clear");
            std::cout<<"Waiting for JointState"<<std::endl;
   
             std::cout << "Press Enter Key if ready!" << std::endl;
            std::cin.ignore();
            std::cout<<"Recived JointState"<<std::endl;
            q_list3.clear();
        std::cout<<"Start Motion 4"<<std::endl;
      // double task_goal_values13[7] ={0.92395 ,-0.38252, 0 ,0 ,0.3, 0.0, 0.6};
      // doTaskPlanning(task_goal_values13);  

        goHome();
        q_list1.clear();
        q_list2.clear();
        q_list3.clear();
         my_class_ptr->isMove(0);
        }


//----------------------------------------------------//
    t1.join();
    t2.join();

    //t2.join();
    return 1;
}
