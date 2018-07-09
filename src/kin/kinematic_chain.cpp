/*!
 * @file        kinematic_chain.cpp
 * @brief       A source file for the class of general kinematic chain
 * @author      Chien-Pin Chen
 */
#include "kinematic_chain.h"

#include <cmath>

namespace rb //! Robot Arm Library namespace
{
namespace kin //! Kinematics module namespace
{

using rb::math::RAD2DEG;
using rb::math::DEG2RAD;
using rb::math::PI;

using rb::math::Matrix3;
using rb::math::Matrix4;
using rb::math::Vector3;
using rb::math::AngleAxis;

std::ostream& operator<< (std::ostream& ost, const ArmPose& pose)
{
#if __cplusplus >= 201402L
  auto ostrVec = [](std::ostream& ost , auto vec_inp, int width=12){   // use lambda to handle
    ost << '\n';
    for(auto& inp : vec_inp)
      ost << std::right << std::setw(width) << inp;
  };
#endif

  ostrVec( ost, std::vector<char>({'x', 'y', 'z'}) );
  ostrVec( ost, std::vector<double>({pose.x, pose.y, pose.z}) );
  ostrVec( ost, std::vector<char>({'a', 'b', 'c'}) );
  ostrVec( ost, std::vector<double>({pose.a, pose.b, pose.c}) );
  return ost;
}

// Kinematic Chain Class
KinematicChain::KinematicChain(){}

KinematicChain::KinematicChain( std::vector<rb::kin::Link*> links,
    rb::math::Matrix4  base, rb::math::Matrix4  tool, rb::math::Vector3  gravity,
    std::string manufactor, std::string model
    )
{
}

KinematicChain::~KinematicChain(){}

ArmPose KinematicChain::forwardKin(const std::vector<double>& q)
{
  return  ArmPose();
}

IK_RESULT KinematicChain::inverseKin(void)
{
  return IK_RESULT::IK_COMPLETE;
}

bool KinematicChain::setTool(const ArmPose& tool_pose)
{
  double roll = DEG2RAD * tool_pose.a;
  double pitch = DEG2RAD * tool_pose.b;
  double yaw = DEG2RAD * tool_pose.c;

  // compute rpy matrix and assign it to offset matrix of the tool.
  rpy2tr(roll, pitch, yaw, this->tool_tf_);
  this->tool_tf_(0,3) = tool_pose.x;
  this->tool_tf_(1,3) = tool_pose.y;
  this->tool_tf_(2,3) = tool_pose.z;

  return true;
}

bool KinematicChain::getTool(ArmPose& tool_pose) const
{
  double roll, pitch, yaw;
  tr2rpy(this->tool_tf_, roll, pitch, yaw);
  tool_pose.a = roll * RAD2DEG;
  tool_pose.b = pitch * RAD2DEG;
  tool_pose.c = yaw * RAD2DEG;

  tool_pose.x = this->tool_tf_(0, 3);
  tool_pose.y = this->tool_tf_(1, 3);
  tool_pose.z = this->tool_tf_(2, 3);

  return true;
}

void KinematicChain::setBase(const rb::math::Matrix4& base)
{
  this->base_tf_ = base;
  return;
}

rb::math::Matrix4 KinematicChain::getBase(void) const
{
  return this->base_tf_;
}

void KinematicChain::setDOF(void)
{
  this->dof_ = this->links_.size();
  return;
}

int KinematicChain::getDOF(void) const
{
  return this->dof_;
}

/********************************************************************************/
/** \brief Building HT matrix
 * A function to build homogeneous transformation matrix for each link.
 * \return Matrix4
 */
rb::math::Matrix4 KinematicChain::homoTrans(double& A, double& alpha, double& D, const double& theta)
{
  double ct = cos(DEG2RAD * theta);
  double st = sin(DEG2RAD * theta);
  double ca = cos(DEG2RAD * alpha);
  double sa = sin(DEG2RAD * alpha);

  Matrix4 T;
  T << ct,   -st,   0,     A,
    st*ca, ct*ca, -sa, -sa*D,
    st*sa, ct*sa,  ca,  ca*D,
        0,     0,   0,     1;
  return T;
}


/********************************************************************************/
/** \brief Rotation Matrix to Roll Pitch Yaw
 * A function to get roll, pitch, yaw from rotation matrix
 * \return N/A
 */
void KinematicChain::tr2rpy(const Matrix4& m, double& roll_z, double& pitch_y, double& yaw_x) const
{
  double eps = rb::math::EPSILON;     // to check if close to zero
  if(fabs(m(0,0)) < eps && fabs(m(1,0)) < eps)
  {
    roll_z  = 0;
    pitch_y = atan2(-m(2,0), m(0,0));
    yaw_x   = atan2(-m(1,2), m(1,1));
  }
  else
  {
    roll_z  = atan2(m(1,0), m(0,0));
    double sr = sin(roll_z);
    double cr = cos(roll_z);
    pitch_y = atan2(-m(2,0), cr * m(0,0) + sr * m(1,0));
    yaw_x   = atan2(sr * m(0,2) - cr * m(1,2), cr * m(1,1) - sr * m(0,1));
  }
  return;
}


/********************************************************************************/
/** \brief Roll Pitch Yaw to Rotation Matrix
 * A function to get roll, pitch, yaw from rotation matrix
 * \return N/A
 */
void KinematicChain::rpy2tr(double& roll_z, double& pitch_y, double& yaw_x, Matrix4& tool_mat)
{
  Matrix4 mat_z = rotateZ(roll_z);
  Matrix4 mat_y = rotateY(pitch_y);
  Matrix4 mat_x = rotateX(yaw_x);
  tool_mat = mat_z * mat_y * mat_x;
  return;
}


Matrix4 KinematicChain::rotateX(const double& deg)
{
  Matrix3 m33;
  m33 = AngleAxis(deg * DEG2RAD, Vector3::UnitX());
  Matrix4 matrix = Matrix4::Identity();
  matrix.topLeftCorner(3, 3) << m33;

  return matrix;
}


Matrix4 KinematicChain::rotateY(const double& deg)
{
  Matrix3 m33;
  m33 = AngleAxis(deg * DEG2RAD, Vector3::UnitY());
  Matrix4 matrix = Matrix4::Identity();
  matrix.topLeftCorner(3,3) << m33;
  return matrix;
}

Matrix4 KinematicChain::rotateZ(const double& deg)
{
  Matrix3 m33;
  m33 = AngleAxis(deg * DEG2RAD, Vector3::UnitZ());
  Matrix4 matrix = Matrix4::Identity();
  matrix.topLeftCorner(3,3) << m33;
  return matrix;
}

}       // namespace kin
}       // namespace rb
