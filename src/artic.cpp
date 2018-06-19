/*--------------------------------------------------------------------------------
 * artic.cpp
 * Author             Chien-Pin Chen,
 * Description        General robot arm Kinematic application
 *--------------------------------------------------------------------------------*/
#include "artic.h"

#include <algorithm>


/*-DEFINES---------------------------------------------------------------------*/
#define R2D 57.295779513082320876798154814105   //!< constant to present converting from radius to degree
#define D2R  0.01745329251994329576923690768489 //!< constant to present  converting from degree to radius


namespace rb
{

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

/* ----------- Robot Arm Kinematics Library --------------*/
Artic::Artic()
{
    /* Initialize */
    m_ini_theta = Array6d::Constant(0.0);
    m_pre_theta = Array6d::Constant(0.0);

    m_T_act = Matrix4d::Constant(0.0);
    m_tool_T = Matrix4d::Constant(0.0);

    pre_fit_solution = 9;

    // Default as KUKA KR5 ( Using Modified DH-Table)
    //    <<     j1,    j2,    j3,     j4,    j5,   j6
        a <<    0.0,  75.0, 270.0,   90.0,   0.0,   0.0;
    alpha <<  180.0,  90.0,   0.0,   90.0, -90.0, -90.0;
        d << -335.0,   0.0,   0.0, -295.0,   0.0,  80.0;
    theta <<    0.0, -90.0,   0.0,    0.0,   0.0,   0.0;

     uplimit <<  170.,   45.,  169.0-90.0,  190.,  120.,  350.; /* degree */
    lowlimit << -170., -190., -119.0-90.0, -190., -120., -350.; /* degree */

    /* Initialize private variable */
    m_tool_T = Matrix4d::Identity();

    /* Initialize work_base HT matrix */
    work_base_T = Matrix4d::Identity();

    /* Initialize theta angle of each joints */
    for (int i=0; i < theta.size(); ++i)
    {
        m_ini_theta[i] = theta[i];
    }

    //Matrix4d T1, T2, T3, T4, T5, T6;
    Matrix4d T1 = homoTrans(a[0], alpha[0], d[0], m_ini_theta[0]); // A01
    Matrix4d T2 = homoTrans(a[1], alpha[1], d[1], m_ini_theta[1]); // A12
    Matrix4d T3 = homoTrans(a[2], alpha[2], d[2], m_ini_theta[2]); // A23
    Matrix4d T4 = homoTrans(a[3], alpha[3], d[3], m_ini_theta[3]); // A34
    Matrix4d T5 = homoTrans(a[4], alpha[4], d[4], m_ini_theta[4]); // A45
    Matrix4d T6 = homoTrans(a[5], alpha[5], d[5], m_ini_theta[5]); // A56

    //Matrix4d T12, T13, T14, T15, T16;
    Matrix4d T12 = T1 * T2;             // A02 = A01 * A12
    Matrix4d T13 = T12 * T3;
    Matrix4d T14 = T13 * T4;
    Matrix4d T15 = T14 * T5;
    Matrix4d T16 = T15 * T6;
    m_T_act = T16 * m_tool_T;           // m_T_act is Tool center point HT Matrix
    work_base = m_T_act * work_base_T;  // get TCP in work base coordination

    /* calculate xyzabc */
    m_pos_act.x = work_base(0,3);
    m_pos_act.y = work_base(1,3);
    m_pos_act.z = work_base(2,3);
    tr2rpy(work_base, m_pos_act.a, m_pos_act.b, m_pos_act.c);
    m_pos_act.a *= R2D;                 // change radius to degree
    m_pos_act.b *= R2D;
    m_pos_act.c *= R2D;

    /* copy HTmatrix */
    m_pos_act.T[0] = T1;
    m_pos_act.T[1] = T12;
    m_pos_act.T[2] = T13;
    m_pos_act.T[3] = T14;
    m_pos_act.T[4] = T15;
    m_pos_act.T[5] = m_T_act;
    m_pos_act.T[6] = work_base;
}

Artic::Artic(
        const Array6d& a0,
        const Array6d& alpha0,
        const Array6d& d0,
        const Array6d& ini_theta,
        const Array6d& uplimit0,
        const Array6d& lowlimit0)
{
    /* Initialize */
    m_ini_theta = Array6d::Constant(0.0);
    m_pre_theta = Array6d::Constant(0.0);

    m_T_act = Matrix4d::Constant(0.0);
    m_tool_T = Matrix4d::Constant(0.0);

    pre_fit_solution = 9;

    /*initialize Modified DH-Table*/
    this->a = a0;
    this->alpha = alpha0;
    this->d = d0;
    this->theta = ini_theta;
    this->uplimit = uplimit0;
    this->lowlimit = lowlimit0;

    /* Initialize private variable */
    // TCP Home Trans Matrix
    m_tool_T = Matrix4d::Identity();

    /* Initialize work_base HT matrix */
    work_base_T = Matrix4d::Identity();

    /* Initialize theta angle of each joints */
    for (int i=0; i < theta.size(); ++i)
    {
        m_ini_theta[i] = theta[i];
    }

    //Matrix4d T1, T2, T3, T4, T5, T6;
    Matrix4d T1 = homoTrans(a[0], alpha[0], d[0], m_ini_theta[0]); // A01
    Matrix4d T2 = homoTrans(a[1], alpha[1], d[1], m_ini_theta[1]); // A12
    Matrix4d T3 = homoTrans(a[2], alpha[2], d[2], m_ini_theta[2]); // A23
    Matrix4d T4 = homoTrans(a[3], alpha[3], d[3], m_ini_theta[3]); // A34
    Matrix4d T5 = homoTrans(a[4], alpha[4], d[4], m_ini_theta[4]); // A45
    Matrix4d T6 = homoTrans(a[5], alpha[5], d[5], m_ini_theta[5]); // A56

    //Matrix4d T12, T13, T14, T15, T16;
    Matrix4d T12 = T1 * T2;             // A02 = A01 * A12
    Matrix4d T13 = T12 * T3;
    Matrix4d T14 = T13 * T4;
    Matrix4d T15 = T14 * T5;
    Matrix4d T16 = T15 * T6;
    m_T_act = T16 * m_tool_T;           // m_T_act is Tool center point HT Matrix
    work_base = m_T_act * work_base_T;  // get TCP in work base coordination

    /* calculate xyzabc */
    m_pos_act.x = work_base(0,3);
    m_pos_act.y = work_base(1,3);
    m_pos_act.z = work_base(2,3);
    tr2rpy(work_base, m_pos_act.a, m_pos_act.b, m_pos_act.c);
    m_pos_act.a *= R2D;                 // change radius to degree
    m_pos_act.b *= R2D;
    m_pos_act.c *= R2D;

    /* copy HTmatrix */
    m_pos_act.T[0] = T1;
    m_pos_act.T[1] = T12;
    m_pos_act.T[2] = T13;
    m_pos_act.T[3] = T14;
    m_pos_act.T[4] = T15;
    m_pos_act.T[5] = m_T_act;
    m_pos_act.T[6] = work_base;
}

/* Destructor */
Artic::~Artic(){}

/* Forward Kinematics */
ArmPose Artic::forwardKin(const Array6d& q)
{
    // storage input angles as previous angles for finding best solution in IK
    m_pre_theta = q;

    // calculate homogeneous matrix for each joint
    Eigen::Array<Matrix4d, 6, 1> T;
    for(int i=0; i < q.size(); ++i)
        T[i] = homoTrans(a[i], alpha[i], d[i], q[i]);

    //Matrix4d T12, T13, T14, T15, T16;
    Matrix4d T12 = T[0] * T[1];             // A02 = A01 * A12
    Matrix4d T13 = T12 * T[2];
    Matrix4d T14 = T13 * T[3];
    Matrix4d T15 = T14 * T[4];
    Matrix4d T16 = T15 * T[5];
    m_T_act = T16 * m_tool_T;           // m_T_act is Tool center point HT Matrix
    work_base = m_T_act * work_base_T;  // get TCP in work base coordination

    /* calculate xyzabc */
    m_pos_act.x = work_base(0, 3);
    m_pos_act.y = work_base(1, 3);
    m_pos_act.z = work_base(2, 3);
    tr2rpy(work_base, m_pos_act.a, m_pos_act.b, m_pos_act.c);
    m_pos_act.a = R2D * m_pos_act.a;
    m_pos_act.b = R2D * m_pos_act.b;
    m_pos_act.c = R2D * m_pos_act.c;

    /* copy m_T_act */
    m_pos_act.T[0] = T[0];
    m_pos_act.T[1] = T12;
    m_pos_act.T[2] = T13;
    m_pos_act.T[3] = T14;
    m_pos_act.T[4] = T15;
    m_pos_act.T[5] = m_T_act;
    m_pos_act.T[6] = work_base;

    pre_fit_solution = 9;       // reset previous solution
    return m_pos_act;
}

/* Inverse Kinematics */
IK_RESULT Artic::inverseKin(const double& x, const double& y, const double& z,
        const double& roll, const double& pitch, const double& yaw,
        Array6d& joints, ArmAxisValue& all_sols)
{
    // reset all possible (8) solutions
    all_sols.axis_value = MatrixXd::Constant(8, 6, 0.0);
    size_t num_sols = sizeof(all_sols.limit_check) / sizeof(all_sols.limit_check[0]);
    for(size_t i=0; i < num_sols; ++i)
    {
        all_sols.solution_check[i] = true;
        all_sols.singular_check[i] = true;
        all_sols.limit_check[i] = true;
    }

    // calculate Transformation matrix of Tool Center Point (TCP)  with
    // respect to world (work base) coordination system.
    Matrix4d workbase_tool_Tr = Matrix4d::Identity();
    double r_deg = roll;
    double p_deg = pitch;
    double y_deg = yaw;
    rpy2tr(r_deg, p_deg, y_deg, workbase_tool_Tr);
    workbase_tool_Tr(0, 3) = x;
    workbase_tool_Tr(1, 3) = y;
    workbase_tool_Tr(2, 3) = z;

    // translate work base coordination system to robot coordination.
    Matrix4d work_base_Tr_inv = this->work_base_T.inverse();
    Matrix4d robot_tool_Tr = workbase_tool_Tr * work_base_Tr_inv;
#ifndef NDEBUG
    std::cout << "\nworkbase_tool_Tr:\n" << workbase_tool_Tr;
    std::cout << "\nwork_base_T:\n" << this->work_base_T;
    std::cout << "\nwork_base_Tr_inv:\n" << work_base_Tr_inv;
    std::cout << "\nrobot_tool_Tr:\n" << robot_tool_Tr;
#endif

    // get HT matrix of the robot arm flange
    Matrix4d tool_Tr_inv = this->m_tool_T.inverse();
    Matrix4d robot_flange_Tr = robot_tool_Tr * tool_Tr_inv;

#ifndef NDEBUG
    printf("\nrobot_flange_Tr:\n");
    printf("[%.4f %.4f %.4f %.4f]\n",robot_flange_Tr(0, 0),robot_flange_Tr(0, 1),robot_flange_Tr(0, 2),robot_flange_Tr(0, 3));
    printf("[%.4f %.4f %.4f %.4f]\n",robot_flange_Tr(1, 0),robot_flange_Tr(1, 1),robot_flange_Tr(1, 2),robot_flange_Tr(1, 3));
    printf("[%.4f %.4f %.4f %.4f]\n",robot_flange_Tr(2, 0),robot_flange_Tr(2, 1),robot_flange_Tr(2, 2),robot_flange_Tr(2, 3));
    printf("[%.4f %.4f %.4f %.4f]\n",robot_flange_Tr(3, 0),robot_flange_Tr(3, 1),robot_flange_Tr(3, 2),robot_flange_Tr(3, 3));
#endif

    /* Start to solve IK, get wrist center point Wc (or P0 = A06 * P6
     * from eqn. 2.77 of J.J. Craig's book),  and solve 1st joint. */
    Eigen::Vector4d p0;
    Eigen::Vector4d p6 = {0., 0., -this->d[5], 1.};
    p0 = robot_flange_Tr * p6;
    // solve joint 1st of solution 1-4
    //all_sols.axis_value.block(0, 0, 4, 1).setConstant(R2D*atan2(-p0[1], p0[0]));
    double theta1 = atan2(-p0[1], p0[0]);
    for(int i=0; i < 4; ++i)
        all_sols.axis_value(i, 0) = theta1;

    /* Solve joint 2nd, 3rd of solutions 1-2, 3-4 */
    std::vector<bool> config = {0, 0, 0};
    solvePitchPitchIK(theta1, p0, config, all_sols);

    if(all_sols.solution_check[0] == true)
    {
        /*solve joint 5,4,6  solution 1-2*/
        solveRowPitchRowIK(theta1, config, robot_flange_Tr, all_sols);

        /*solve theta 5,4,6  solution 3-4*/
        config = {0, 1, 0};
        solveRowPitchRowIK(theta1, config, robot_flange_Tr, all_sols);
    }

    /*solve joint 1st backward solution 5-8*/
    config = {1, 0, 0};
    double theta1_b = theta1 + M_PI;
    if(theta1_b >= 2*M_PI)
        theta1_b = theta1_b - 2*M_PI;

    for(int i=4; i<8; ++i)
        all_sols.axis_value(i, 0) = theta1_b;

    /*solve joint 2nd 3rd of  solution 5-6, 7-8*/
    solvePitchPitchIK(theta1_b, p0, config, all_sols);

    if(all_sols.solution_check[4] == true)
    {
        /*solve joint 5,4,6  solution 5-6*/
        solveRowPitchRowIK(theta1_b, config, robot_flange_Tr, all_sols);

        /*solve theta 5,4,6  solution 7-8*/
        config = {1, 1, 0};
        solveRowPitchRowIK(theta1_b, config, robot_flange_Tr, all_sols);
    }

    // Convert to degree unit before find the most fit solution.
    all_sols.axis_value *= R2D;
    IK_RESULT check = solutionCheck(all_sols);

    if( check != IK_RESULT::IK_COMPLETE)
    {
        // IK fail. return IK result, but not update joint values
        std::cout << "No Solution!!\n";
        return check;
    }

    // update the pose of robot arm
    this->m_pos_act.x = x;
    this->m_pos_act.y = y;
    this->m_pos_act.z = z;
    this->m_pos_act.a = roll;
    this->m_pos_act.b = pitch;
    this->m_pos_act.c = yaw;
    this->m_pos_act.T[5] = robot_tool_Tr;
    this->m_pos_act.T[6] = workbase_tool_Tr;

    // update m_pre_theta to the most fit solution.
    this->m_pre_theta = all_sols.axis_value.row(all_sols.fit);
    joints = all_sols.axis_value.row(all_sols.fit);

#ifndef NDEBUG
    std::cout << "\np0:\n" << p0 << '\n';
    std::cout << "\n axis_value:\n" << all_sols.axis_value.transpose() << '\n';
    std::cout << "\n The most fit solution: " << all_sols.fit << '\n';
#endif

    return check;
}

void Artic::solvePitchPitchIK(const double& th1, const Eigen::Vector4d& p0,
                             const std::vector<bool>& config,
                             ArmAxisValue& all_sols)
{
    double a1 = this->a[1], a2 = this->a[2], a3 = this->a[3];
    double d0 = this->d[0], d3 = this->d[3];
    double k1 = 2 * a2 * d3;
    double k2 = 2 * a2 * a3;
    double k3 = pow(p0[0], 2) + pow(p0[1], 2) + pow((p0[2] + d0), 2) - 2*a1*p0[0] * cos(th1) +
                2*a1*p0[1]*sin(th1) + pow(a1, 2) - pow(a2, 2) - pow(a3, 2) - pow(d3, 2);
    double ks = pow(k1, 2) + pow(k2, 2) - pow(k3, 2);

    // check if has real solution
    if (ks < 0)
    {
        printf("k1^2+k2^2-k3^2 < 0, solution %d-%d no real solution\n", config[0]*4+1, config[0]*4+4);
        for (int i = 0 + config[0]*4 ; i < 4 + config[0]*4; ++i)
            all_sols.solution_check[i] = false;
    }else // has real solutions.
    {
        double th3_12 = 2 * atan2(k1-sqrt(ks), k3+k2);      // upper arm
        double th3_34 = 2 * atan2(k1+sqrt(ks), k3+k2);      // lower arm

        // TODO: add pre check if theta3 between 180 and -180 deg
        all_sols.axis_value(config[0]*4 + 0, 2) = th3_12;
        all_sols.axis_value(config[0]*4 + 1, 2) = th3_12;
        all_sols.axis_value(config[0]*4 + 2, 2) = th3_34;
        all_sols.axis_value(config[0]*4 + 3, 2) = th3_34;

        double u1 = a2 + a3*cos(th3_12) + d3*sin(th3_12);
        double v1 = -a3*sin(th3_12) + d3*cos(th3_12);
        double r1 = p0[0]*cos(th1) - p0[1]*sin(th1) - a1;
        double u2 = a3*sin(th3_12) - d3*cos(th3_12);
        double v2 = u1;
        double r2 = -d0 - p0[2];

        double sin2 = (r1/u1 - r2/u2)/(v1/u1 - v2/u2);
        double cos2 = (r1/v1 - r2/v2)/(u1/v1 - u2/v2);

        double th2_12 = atan2(sin2,cos2);
        //pre_check(2, the2_12, axis_limit, m_pre_theta);
        all_sols.axis_value(config[0]*4 + 0, 1) = th2_12;
        all_sols.axis_value(config[0]*4 + 1, 1) = th2_12;

        u1 = a2 + a3*cos(th3_34) + d3*sin(th3_34);
        v1 = -a3*sin(th3_34) + d3*cos(th3_34);
        r1 = p0[0]*cos(th1) - p0[1]*sin(th1) - a1;
        u2 = a3*sin(th3_34) - d3*cos(th3_34);
        v2 = u1;
        r2 = -d0 - p0[2];

        sin2 = (r1/u1 - r2/u2)/(v1/u1 - v2/u2);
        cos2 = (r1/v1 - r2/v2)/(u1/v1 - u2/v2);

        double th2_34 = atan2(sin2,cos2);
        //pre_check(2, the2_34, axis_limit, m_pre_theta);
        all_sols.axis_value(config[0]*4 + 2, 1) = th2_34;
        all_sols.axis_value(config[0]*4 + 3, 1) = th2_34;
    }

    return;
}

void Artic::solveRowPitchRowIK(const double& th1, const std::vector<bool>& config,
                              const Matrix4d& flange_tr,
                              ArmAxisValue& all_sols)
{
    double config_start = config[0]*4 + config[1]*2;
    double th2up_rad = all_sols.axis_value( config_start, 1);
    double th3up_rad = all_sols.axis_value( config_start, 2);
    Matrix4d tr01 = homoTrans(a[0], alpha[0], d[0], th1 * R2D);
    Matrix4d tr12 = homoTrans(a[1], alpha[1], d[1], th2up_rad * R2D);
    Matrix4d tr23 = homoTrans(a[2], alpha[2], d[2], th3up_rad * R2D);
    Matrix4d tr03 = tr01 * tr12 * tr23;
    //Matrix4d tr03_inv =tr03.inverse();
    Matrix4d tr36 = tr03.inverse() * flange_tr;
    double eps=2.22044604925031e-5;	// to check if close to zero
    double th5_1, th5_2, th4_1, th4_2, th6_1, th6_2;

    // check if in singular point first
    if(fabs(tr36(0,2)) < eps && fabs(tr36(2, 2)) < eps)
    {
        printf("Solution 1-2 Reach sigular position\n");
        for(int i = config_start; i<config_start+2; ++i)
            all_sols.singular_check[i] = false;

        th5_1 = th5_2 = 0;
        double sum_theta = atan2(tr36(0, 1), tr36(0, 0));
        // TODO: may use pre_theta
        th4_1 = th4_2 = sum_theta;
        th6_1 = th6_2 = 0;
    }else
    {
        //solution 1 of theta 5,4,6
        double c4, s4, c6, s6;

        th5_1 = acos(tr36(1, 2));
        c4 = -tr36(0, 2) / sin(th5_1);
        s4 = -tr36(2, 2) / sin(th5_1);
        th4_1 = atan2(s4, c4);
        //pre_check(4, th4_1, axis_limit, m_pre_theta);

        c6 = tr36(1, 0) / sin(th5_1);
        s6 = -tr36(1, 1) / sin(th5_1);
        th6_1 = atan2(s6,c6);
        //pre_check(6, th6_1, axis_limit, m_pre_theta);

        //solution 2 of theta 5,4,6
        th5_2 = -th5_1;
        c4 = -tr36(0, 2) / sin(th5_2);
        s4 = -tr36(2, 2) / sin(th5_2);
        th4_2 = atan2(s4, c4);
        //pre_check(4, th4_2, axis_limit, m_pre_theta);

        c6 = tr36(1, 0) / sin(th5_2);
        s6 = -tr36(1, 1) / sin(th5_2);
        th6_2 = atan2(s6,c6);
        //pre_check(6, th6_2, axis_limit, m_pre_theta);
    }

    all_sols.axis_value(config_start, 3) = th4_1;
    all_sols.axis_value(config_start+1, 3) = th4_2;

    all_sols.axis_value(config_start, 4) = th5_1;
    all_sols.axis_value(config_start+1, 4) = th5_2;

    all_sols.axis_value(config_start, 5) = th6_1;
    all_sols.axis_value(config_start+1, 5) = th6_2;

    return;
}

IK_RESULT Artic::solutionCheck(ArmAxisValue& sols)
{
    // First initial result as complete (find a fit solution
    IK_RESULT check = IK_RESULT::IK_COMPLETE;
    if( std::all_of(sols.solution_check.begin(), sols.solution_check.end(),
                [](bool i){return i==false;} ))
    {
        check = IK_RESULT::IK_NO_SOLUTION;
        return check;
    }

    /* check joint limit first*/
    int n_limit = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (sols.solution_check[i] == true)
        {
            for(int j = 0; j < 6; ++j)
            {
                if (sols.axis_value(i, j)  > uplimit[j] ||
                    sols.axis_value(i, j) < lowlimit[j])
                {
                    sols.limit_check[i] = false;
                    printf ("Solution %d, Joint %d overlimit.\n", i+1, j+1 );
                }
            }
            if (sols.limit_check[i] == false)
                n_limit = n_limit + 1;
        }
    }

    bool check_front4 = std::all_of(sols.solution_check.begin(),
            sols.solution_check.begin()+4, [](bool i){return i==false;});
    bool check_back4 = std::all_of(sols.solution_check.begin()+4,
            sols.solution_check.end(), [](bool i){return i==false;});
    // TODO: add functions to check if reach joint limits.

    // using cosine similarity to check which solutions is most closest to previous step join value
    sols.fit = 0;
    std::array<double, 8> cosine_sim = {{-2.}};   // initialize smaller valus;
    for(int i=0; i < sols.axis_value.rows(); ++i)
    {
        if(sols.solution_check[i] == true && sols.limit_check[i] == true)
        {
            Eigen::VectorXd sol_theta = sols.axis_value.row(i);
            Eigen::VectorXd pre_theta = m_pre_theta;
            cosine_sim[i] = sol_theta.dot(pre_theta) /
                           (sol_theta.norm() * pre_theta.norm());

        }
    }
    auto iter_cosine = std::max_element(cosine_sim.begin(),
                                     cosine_sim.end());
    if(iter_cosine != cosine_sim.end())
        sols.fit = iter_cosine - cosine_sim.begin();
    else
    {
        printf("Note Comsine Similarity found no_solution!\n");
        check = IK_RESULT::IK_NO_SOLUTION;
    }

    // check if it's a singular solution.
    if(sols.solution_check[sols.fit] == false)
    {
        check = IK_RESULT::IK_SINGULAR;
        sols.fit = 0;
        pre_fit_solution = 9;    // set for no previous solutions
        return check;
    }

    return check;
}

ArmPose Artic::getArmPos(void)
{
    return this->m_pos_act;
}


bool Artic::setToolOffset(const ArmPose& tool_offset)
{
    double roll = D2R * tool_offset.a;
    double pitch = D2R * tool_offset.b;
    double yaw = D2R * tool_offset.c;

    // compute rpy matrix and assign it to offset matrix of the tool.
    rpy2tr(roll, pitch, yaw, this->m_tool_T);
    this->m_tool_T(0,3) = tool_offset.x;
    this->m_tool_T(1,3) = tool_offset.y;
    this->m_tool_T(2,3) = tool_offset.z;

    return true;
}

void Artic::setBase(Matrix4d& base)
{
    this->work_base_T = base;
    return;
}

Matrix4d Artic::getBase(void)
{
    return this->work_base_T;
}

Array6d Artic::getA(void)
{
    return this->a;
}

Array6d Artic::getAlpha(void)
{
    return this->alpha;
}

Array6d Artic::getD(void)
{
    return this->d;
}

Array6d Artic::getTheta(void)
{
    return this->theta;
}

void Artic::setUpLimit(Array6d& up_lim)
{
   this->uplimit = up_lim;
   return;
}

Array6d Artic::getUpLimit(void)
{
    return this->uplimit;
}

void Artic::setLowLimit(Array6d& low_lim)
{
   this->lowlimit = low_lim;
   return;
}

Array6d Artic::getLowLimit(void)
{
    return this->lowlimit;
}


/********************************************************************************/
/** \brief Building HT matrix
* A function to build homogeneous transformation matrix for each link.
* \return Matrix4d
*/
Matrix4d Artic::homoTrans(double& A, double& alpha, double& D, const double& theta)
{
    double ct = cos(D2R * theta);
    double st = sin(D2R * theta);
    double ca = cos(D2R * alpha);
    double sa = sin(D2R * alpha);

    Matrix4d T;
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
void Artic::tr2rpy(const Matrix4d& m, double& roll_z, double& pitch_y, double& yaw_x)
{
    double eps=2.22044604925031e-5;
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
void Artic::rpy2tr(double& roll_z, double& pitch_y, double& yaw_x, Matrix4d& tool_mat)
{
    Matrix4d mat_z = rotateZ(roll_z);
    Matrix4d mat_y = rotateY(pitch_y);
    Matrix4d mat_x = rotateX(yaw_x);
    tool_mat = mat_z * mat_y * mat_x;
    return;
}


Matrix4d Artic::rotateX(const double& deg)
{
    Matrix3d m33;
    m33 = AngleAxisd(deg * D2R, Vector3d::UnitX());
    Matrix4d matrix = Matrix4d::Identity();
    matrix.topLeftCorner(3, 3) << m33;

    return matrix;
}


Matrix4d Artic::rotateY(const double& deg)
{
    Matrix3d m33;
    m33 = AngleAxisd(deg * D2R, Vector3d::UnitY());
    Matrix4d matrix = Matrix4d::Identity();
    matrix.topLeftCorner(3,3) << m33;
    return matrix;
}

Matrix4d Artic::rotateZ(const double& deg)
{
    Matrix3d m33;
    m33 = AngleAxisd(deg * D2R, Vector3d::UnitZ());
    Matrix4d matrix = Matrix4d::Identity();
    matrix.topLeftCorner(3,3) << m33;
    return matrix;
}

}   // namespace rb
