#include  <iostream>

#include <world_observer/estimator.hpp>


// PoseKalmanFilterクラス
// 二次元座標での位置と速度のカルマンフィルタ

PoseKalmanFilter::PoseKalmanFilter()
{
}

void PoseKalmanFilter::Init(double loop_time)
{
    this->dt = loop_time;

    this->InitSystemModel(&(this->sys_pdf), &(this->sys_model));
    this->InitMeasurementModel(&(this->meas_pdf), &(this->meas_model));
    this->InitPrior(&(this->prior));

    /****************************
     * Linear prior DENSITY     *
     ***************************/
    // Continuous Gaussian prior (for Kalman filters)
    this->filter = new ExtendedKalmanFilter(this->prior);
}

geometry2d::Odometry PoseKalmanFilter::estimate()
{
    std::vector<geometry2d::Pose> null_poses;
    return  this->estimate(null_poses);
}

geometry2d::Odometry PoseKalmanFilter::estimate(std::vector<geometry2d::Pose> observations)
{
    geometry2d::Accel  null_acc;
    return  this->estimate(null_acc, observations);
}

geometry2d::Odometry PoseKalmanFilter::estimate(geometry2d::Accel accel, std::vector<geometry2d::Pose> observations)
{
    // System update by only system model with input
    predict(accel.ToColumnVector());

    for (auto observation : observations)
    {
        MatrixWrapper::ColumnVector observation_cv = observation.ToColumnVector();

        // collect angle continuity
        observation_cv(3) = EulerAngle::normalize(observation_cv(3), this->last_estimation.val(3));

        if (isOutlier(observation_cv)) {
            continue;
        }

        update(observation_cv);
    }

    return convetEstimationToOdometry();
}


void PoseKalmanFilter::Reset()
{
    ColumnVector prior_Mu(6);
    prior_Mu = 0.0;

    SymmetricMatrix prior_Cov(6);
    prior_Cov = 0.0;
    prior_Cov(1,1) = 100.0;
    prior_Cov(2,2) = 100.0;
    prior_Cov(3,3) = 100.0;
    prior_Cov(4,4) = 100.0;
    prior_Cov(5,5) = 100.0;
    prior_Cov(6,6) = 100.0;

    this->prior->ExpectedValueSet(prior_Mu);
    this->prior->CovarianceSet(prior_Cov);

    this->filter->Reset(this->prior);
}


// Private methods
//

void PoseKalmanFilter::collectAngleOverflow(ColumnVector& state, SymmetricMatrix& cov)
{
    if (state(3) < -M_PI || state(3) > M_PI) {
        state(3) = EulerAngle::normalize(state(3));

        this->prior->ExpectedValueSet(state);
        this->prior->CovarianceSet(cov);

        filter->Reset(prior);
    }
}

PoseKalmanFilter::Estimation PoseKalmanFilter::getResult()
{
    Estimation  est;
    Pdf<ColumnVector> * posterior = filter->PostGet();

    est.val = posterior->ExpectedValueGet();
    est.cov = posterior->CovarianceGet();

    return  est;
}


void PoseKalmanFilter::predict(ColumnVector input)
{
    Estimation  est;

    this->filter->Update(sys_model, input);
    est = getResult();
    collectAngleOverflow(est.val, est.cov);

    this->last_estimation = est;
}


void PoseKalmanFilter::update(ColumnVector measurement)
{
    Estimation  est;

    this->filter->Update(meas_model, measurement);
    est = getResult();

    collectAngleOverflow(est.val, est.cov);

    this->last_estimation = est;
}


geometry2d::Odometry PoseKalmanFilter::convetEstimationToOdometry()
{
    geometry2d::Odometry odom;

    odom.pose.x = this->last_estimation.val(1);
    odom.pose.y = this->last_estimation.val(2);
    odom.pose.theta = this->last_estimation.val(3);

    odom.velocity.x = this->last_estimation.val(4);
    odom.velocity.y = this->last_estimation.val(5);
    odom.velocity.theta = this->last_estimation.val(6);

    return  odom;
}


bool PoseKalmanFilter::isOutlier(ColumnVector measurement){
    // Reference: https://myenigma.hatenablog.com/entry/20140825/1408975706

    double mahala_dist = mahalanobisDistance(measurement);
    double thresh = 5.99; //自由度2、棄却率5%のしきい値
     
    if(mahala_dist > thresh){
        return true;
    }
    return false;
}


double PoseKalmanFilter::mahalanobisDistance(ColumnVector measurement){
    double measurementX= measurement(1);
    double measurementY= measurement(2);
    double estimationX = this->last_estimation.val(1);
    double estimationY = this->last_estimation.val(2);
    double covarianceX = this->last_estimation.cov(1,1);
    double covarianceY = this->last_estimation.cov(2,2);

    double diffX = measurementX - estimationX;
    double diffY = measurementY - estimationY;

    // avoid 0 division
    if(covarianceX == 0 || covarianceY == 0){
        return 0;
    }

    return sqrt(pow(diffX, 2)/covarianceX + pow(diffY, 2)/covarianceY);
}


PoseKalmanFilter::~PoseKalmanFilter()
{
    delete  this->sys_pdf;
    delete  this->sys_model;
    delete  this->meas_pdf;
    delete  this->meas_model;
    delete  this->prior;
    delete  this->filter;
}

// EnemyEstimatorクラス
// ボールの位置・速度推定を担当
void EnemyEstimator::InitSystemModel(LinearAnalyticConditionalGaussian** sys_pdf, LinearAnalyticSystemModelGaussianUncertainty** sys_model)
{
    // Create the matrices A and B for the linear system model
    Matrix A(6,6);
    A(1,1) = 1.0;  A(1,2) = 0.0;  A(1,3) = 0.0;  A(1,4) = dt;   A(1,5) = 0.0;  A(1,6) = 0.0;
    A(2,1) = 0.0;  A(2,2) = 1.0;  A(2,3) = 0.0;  A(2,4) = 0.0;  A(2,5) = dt;   A(2,6) = 0.0;
    A(3,1) = 0.0;  A(3,2) = 0.0;  A(3,3) = 1.0;  A(3,4) = 0.0;  A(3,5) = 0.0;  A(3,6) = dt;
    A(4,1) = 0.0;  A(4,2) = 0.0;  A(4,3) = 0.0;  A(4,4) = 1.0;  A(4,5) = 0.0;  A(4,6) = 0.0;
    A(5,1) = 0.0;  A(5,2) = 0.0;  A(5,3) = 0.0;  A(5,4) = 0.0;  A(5,5) = 1.0;  A(5,6) = 0.0;
    A(6,1) = 0.0;  A(6,2) = 0.0;  A(6,3) = 0.0;  A(6,4) = 0.0;  A(6,5) = 0.0;  A(6,6) = 1.0;

    Matrix B(6,3);
    B(1,1) = 0.0;  B(1,2) = 0.0;  B(1,3) = 0.0;
    B(2,1) = 0.0;  B(2,2) = 0.0;  B(2,3) = 0.0;
    B(3,1) = 0.0;  B(3,2) = 0.0;  B(3,3) = 0.0;
    B(4,1) = dt;   B(4,2) = 0.0;  B(4,3) = 0.0;
    B(5,1) = 0.0;  B(5,2) = dt;   B(5,3) = 0.0;
    B(6,1) = 0.0;  B(6,2) = 0.0;  B(6,3) = dt;

    vector<Matrix> AB(2);
    AB[0] = A;
    AB[1] = B;

    // create gaussian
    ColumnVector sysNoise_Mu(6);
    sysNoise_Mu = 0.0;

    // 位置、速度変化はノイズとして表現
    const double MAX_LINEAR_ACC_MPS = 5.0;
    const double MAX_ANGULAR_ACC_RADPS = 2*M_PI;

    const double MAX_LINEAR_MOVEMENT_IN_DT  = MAX_LINEAR_ACC_MPS    / 2 * pow(dt, 2);
    const double MAX_ANGULAR_MOVEMENT_IN_DT = MAX_ANGULAR_ACC_RADPS / 2 * pow(dt, 2);
    const double MAX_LINEAR_ACCEL_IN_DT     = MAX_LINEAR_ACC_MPS    * dt;
    const double MAX_ANGULAR_ACCEL_IN_DT    = MAX_ANGULAR_ACC_RADPS * dt;

    SymmetricMatrix sysNoise_Cov(6);
    sysNoise_Cov = 0.0;
    sysNoise_Cov(1,1) = pow(MAX_LINEAR_MOVEMENT_IN_DT, 2);
    sysNoise_Cov(2,2) = pow(MAX_LINEAR_MOVEMENT_IN_DT, 2);
    sysNoise_Cov(3,3) = pow(MAX_ANGULAR_MOVEMENT_IN_DT, 2);
    sysNoise_Cov(4,4) = pow(MAX_LINEAR_ACCEL_IN_DT, 2);
    sysNoise_Cov(5,5) = pow(MAX_LINEAR_ACCEL_IN_DT, 2);
    sysNoise_Cov(6,6) = pow(MAX_ANGULAR_ACCEL_IN_DT, 2);

    Gaussian system_Uncertainty(sysNoise_Mu, sysNoise_Cov);

    // create the model
    *sys_pdf = new LinearAnalyticConditionalGaussian(AB, system_Uncertainty);
    *sys_model = new  LinearAnalyticSystemModelGaussianUncertainty(*sys_pdf);
}

void EnemyEstimator::InitMeasurementModel(LinearAnalyticConditionalGaussian** meas_pdf, LinearAnalyticMeasurementModelGaussianUncertainty** meas_model)
{
    // create matrix H for linear measurement model
    Matrix H(3,6);
    H = 0.0;
    H(1,1) = 1.0;
    H(2,2) = 1.0;
    H(3,3) = 1.0;

    // Construct the measurement noise
    ColumnVector measNoise_Mu(3);
    measNoise_Mu = 0.0;

    SymmetricMatrix measNoise_Cov(3);
    measNoise_Cov(1,1) = pow(0.02, 2);   // 観測ノイズを標準偏差 0.02[m] と仮定
    measNoise_Cov(2,2) = pow(0.02, 2);
    measNoise_Cov(3,3) = pow(0.02, 2);
    Gaussian measurement_Uncertainty(measNoise_Mu, measNoise_Cov);

    // create the model
    *meas_pdf = new LinearAnalyticConditionalGaussian(H, measurement_Uncertainty);
    *meas_model = new LinearAnalyticMeasurementModelGaussianUncertainty (*meas_pdf);
}

void EnemyEstimator::InitPrior(Gaussian** prior)
{
    ColumnVector prior_Mu(6);
    prior_Mu = 0.0;

    SymmetricMatrix prior_Cov(6);
    prior_Cov = 0.0;
    prior_Cov(1,1) = 100.0;
    prior_Cov(2,2) = 100.0;
    prior_Cov(3,3) = 100.0;
    prior_Cov(4,4) = 100.0;
    prior_Cov(5,5) = 100.0;
    prior_Cov(6,6) = 100.0;

    *prior = new Gaussian (prior_Mu,prior_Cov);
}

// BallEstimatorクラス
// ボールの位置・速度推定を担当
void BallEstimator::InitSystemModel(LinearAnalyticConditionalGaussian** sys_pdf, LinearAnalyticSystemModelGaussianUncertainty** sys_model)
{
    // Create the matrices A and B for the linear system model
    Matrix A(6,6);
    A(1,1) = 1.0;  A(1,2) = 0.0;  A(1,3) = 0.0;  A(1,4) = dt;   A(1,5) = 0.0;  A(1,6) = 0.0;
    A(2,1) = 0.0;  A(2,2) = 1.0;  A(2,3) = 0.0;  A(2,4) = 0.0;  A(2,5) = dt;   A(2,6) = 0.0;
    A(3,1) = 0.0;  A(3,2) = 0.0;  A(3,3) = 1.0;  A(3,4) = 0.0;  A(3,5) = 0.0;  A(3,6) = dt;
    A(4,1) = 0.0;  A(4,2) = 0.0;  A(4,3) = 0.0;  A(4,4) = 1.0;  A(4,5) = 0.0;  A(4,6) = 0.0;
    A(5,1) = 0.0;  A(5,2) = 0.0;  A(5,3) = 0.0;  A(5,4) = 0.0;  A(5,5) = 1.0;  A(5,6) = 0.0;
    A(6,1) = 0.0;  A(6,2) = 0.0;  A(6,3) = 0.0;  A(6,4) = 0.0;  A(6,5) = 0.0;  A(6,6) = 1.0;

    Matrix B(6,3);
    B(1,1) = 0.0;  B(1,2) = 0.0;  B(1,3) = 0.0;
    B(2,1) = 0.0;  B(2,2) = 0.0;  B(2,3) = 0.0;
    B(3,1) = 0.0;  B(3,2) = 0.0;  B(3,3) = 0.0;
    B(4,1) = dt;   B(4,2) = 0.0;  B(4,3) = 0.0;
    B(5,1) = 0.0;  B(5,2) = dt;   B(5,3) = 0.0;
    B(6,1) = 0.0;  B(6,2) = 0.0;  B(6,3) = dt;

    vector<Matrix> AB(2);
    AB[0] = A;
    AB[1] = B;

    // create gaussian
    ColumnVector sysNoise_Mu(6);
    sysNoise_Mu = 0.0;

    // 位置、速度変化はノイズとして表現
    const double MAX_LINEAR_ACC_MPS = 500.0;    // 6.5[m/s] / 16[ms] = 500

    const double MAX_LINEAR_MOVEMENT_IN_DT  = MAX_LINEAR_ACC_MPS    / 2 * pow(dt, 2);
    const double MAX_LINEAR_ACCEL_IN_DT     = MAX_LINEAR_ACC_MPS    * dt;

    SymmetricMatrix sysNoise_Cov(6);
    sysNoise_Cov = 0.0;
    sysNoise_Cov(1,1) = pow(MAX_LINEAR_MOVEMENT_IN_DT, 2);
    sysNoise_Cov(2,2) = pow(MAX_LINEAR_MOVEMENT_IN_DT, 2);
    sysNoise_Cov(3,3) = 1e9;
    sysNoise_Cov(4,4) = pow(MAX_LINEAR_ACCEL_IN_DT, 2);
    sysNoise_Cov(5,5) = pow(MAX_LINEAR_ACCEL_IN_DT, 2);
    sysNoise_Cov(6,6) = 1e9;

    Gaussian system_Uncertainty(sysNoise_Mu, sysNoise_Cov);

    // create the model
    *sys_pdf = new LinearAnalyticConditionalGaussian(AB, system_Uncertainty);
    *sys_model = new  LinearAnalyticSystemModelGaussianUncertainty(*sys_pdf);
}

void BallEstimator::InitMeasurementModel(LinearAnalyticConditionalGaussian** meas_pdf, LinearAnalyticMeasurementModelGaussianUncertainty** meas_model)
{
    // create matrix H for linear measurement model
    Matrix H(3,6);
    H = 0.0;
    H(1,1) = 1.0;
    H(2,2) = 1.0;
    H(3,3) = 1.0;

    // Construct the measurement noise
    ColumnVector measNoise_Mu(3);
    measNoise_Mu = 0.0;

    SymmetricMatrix measNoise_Cov(3);
    measNoise_Cov(1,1) = pow(0.05, 2);   // 観測ノイズを標準偏差 0.02[m] と仮定
    measNoise_Cov(2,2) = pow(0.05, 2);
    measNoise_Cov(3,3) = 1e9;
    Gaussian measurement_Uncertainty(measNoise_Mu, measNoise_Cov);

    // create the model
    *meas_pdf = new LinearAnalyticConditionalGaussian(H, measurement_Uncertainty);
    *meas_model = new LinearAnalyticMeasurementModelGaussianUncertainty (*meas_pdf);
}

void BallEstimator::InitPrior(Gaussian** prior)
{
    ColumnVector prior_Mu(6);
    prior_Mu = 0.0;

    SymmetricMatrix prior_Cov(6);
    prior_Cov = 0.0;
    prior_Cov(1,1) = 100.0;
    prior_Cov(2,2) = 100.0;
    prior_Cov(3,3) = 100.0;
    prior_Cov(4,4) = 100.0;
    prior_Cov(5,5) = 100.0;
    prior_Cov(6,6) = 100.0;

    *prior = new Gaussian (prior_Mu,prior_Cov);
}
/**********************************************************
 * This is implementation of EulerAngle class
 ***********************************************************/
double  EulerAngle::normalize(double angle)
{
    while(angle>=M_PI)  angle-=2.0*M_PI;
    while(angle<=-M_PI) angle+=2.0*M_PI;
    return angle;
}


double  EulerAngle::normalize(double angle, double center)
{
    return  center + normalize(angle - center);
}