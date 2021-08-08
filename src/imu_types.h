#ifndef GYROPREDICTIMUTYPES_H
#define GYROPREDICTIMUTYPES_H

#include <mutex>
#include <vector>
#include <utility>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
//#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Dense>

class CameraParams{
public:
    CameraParams(){};
    CameraParams(std::string type_, float fx_, float fy_, float cx_, float cy_,
            float k1_, float k2_, float p1_, float p2_, float k3_,
            int width_, int height_, int fps_) {
        type = type_;

        mK = cv::Mat::eye(3,3,CV_32F);
        mK.at<float>(0,0) = fx_;
        mK.at<float>(1,1) = fy_;
        mK.at<float>(0,2) = cx_;
        mK.at<float>(1,2) = cy_;

        mDistCoef = cv::Mat::zeros(4,1,CV_32F);
        mDistCoef.at<float>(0) = k1_;
        mDistCoef.at<float>(1) = k2_;
        mDistCoef.at<float>(2) = p1_;
        mDistCoef.at<float>(3) = p2_;
        mDistCoef.at<float>(4) = k3_;

        mDistCoefTmp = mDistCoef.clone();

        width = width_; height = height_;
        fps = fps_;
        dt = 1.0 / fps;

        cv::Size imageSize = cv::Size(width, height);

//        // undistort and select ROI to whole image
//        cv::initUndistortRectifyMap(mK, mDistCoef, cv::Mat(),
//                                    cv::Mat(), imageSize,
//                                    CV_32F, M1, M2);

        // undistort and keep the size. has black region
        double alpha = 0;   // Free scaling parameter between 0 (when all the pixels in the undistorted image are
                            // valid) and 1 (when all the source image pixels are retained in the undistorted image).
        cv::initUndistortRectifyMap(mK, mDistCoef, cv::Mat(),
                                    cv::getOptimalNewCameraMatrix(mK, mDistCoef, imageSize, alpha, imageSize, 0),
                                    imageSize, CV_32F, M1, M2);

        //ComputeNormalizeTable();
    }

    void ResetDistCoef(){
        mDistCoef = cv::Mat::zeros(5,1,CV_32F);
    }

    void operator=(const CameraParams &s){
        type = s.type;
        mK = s.mK.clone();
        mDistCoef = s.mDistCoef.clone();
        mDistCoefTmp = s.mDistCoefTmp.clone();
        width = s.width; height = s.height;
        fps = s.fps;
        dt = s.dt;
        M1 = s.M1.clone();
        M2 = s.M2.clone();
    }

public:
    std::string type;
    cv::Mat mK;
    cv::Mat mDistCoef;
    cv::Mat mDistCoefTmp;
    int width;
    int height;
    int fps;
    double dt;
    cv::Mat M1, M2;
};

namespace IMU {

//IMU measurement (gyro, accelerometer and timestamp)
class Point
{
public:
    Point(const float &acc_x, const float &acc_y, const float &acc_z,
             const float &ang_vel_x, const float &ang_vel_y, const float &ang_vel_z,
             const double &timestamp): a(acc_x,acc_y,acc_z), w(ang_vel_x,ang_vel_y,ang_vel_z), t(timestamp){}
    Point(const cv::Point3f Acc, const cv::Point3f Gyro, const double &timestamp):
        a(Acc.x,Acc.y,Acc.z), w(Gyro.x,Gyro.y,Gyro.z), t(timestamp){}
public:
    cv::Point3f a;
    cv::Point3f w;
    double t;
};

//IMU biases (gyro and accelerometer)
class Bias
{
public:
    Bias():bax(0),bay(0),baz(0),bwx(0),bwy(0),bwz(0){}
    Bias(const float &b_acc_x, const float &b_acc_y, const float &b_acc_z,
            const float &b_ang_vel_x, const float &b_ang_vel_y, const float &b_ang_vel_z):
            bax(b_acc_x), bay(b_acc_y), baz(b_acc_z), bwx(b_ang_vel_x), bwy(b_ang_vel_y), bwz(b_ang_vel_z){}
    void CopyFrom(Bias &b);
    friend std::ostream& operator<< (std::ostream &out, const Bias &b);

public:
    float bax, bay, baz;
    float bwx, bwy, bwz;
};

//IMU calibration (Tbc, Tcb, noise)
class Calib
{
public:
    Calib(const cv::Mat &Tbc_, const float &ng, const float &na, const float &ngw, const float &naw, const int &fps_)
    {
        Set(Tbc_,ng,na,ngw,naw,fps_);
    }
    Calib(const Calib &calib);
    Calib(){}

    void Set(const cv::Mat &Tbc_, const float &ng, const float &na, const float &ngw, const float &naw, const int &fps_);

public:
    cv::Mat Tcb;
    cv::Mat Tbc;
    cv::Mat Cov, CovWalk;
    int fps;
    double dt;
};

//Integration of 1 gyro measurement
class IntegratedRotation
{
public:
    IntegratedRotation(){}
    IntegratedRotation(const cv::Point3f &angVel, const Bias &imuBias, const float &time);

public:
    float deltaT; //integration time
    cv::Mat deltaR; //integrated rotation
    cv::Mat rightJ; // right jacobian
};

//Preintegration of Imu Measurements
class Preintegrated
{
public:
    Preintegrated(const Bias &b_, const Calib &calib);
    Preintegrated(Preintegrated* pImuPre);
    Preintegrated() {}
    ~Preintegrated() {}
    void CopyFrom(Preintegrated* pImuPre);
    void Initialize(const Bias &b_);
    void IntegrateNewMeasurement(const cv::Point3f &acceleration, const cv::Point3f &angVel, const float &dt);
    void Reintegrate();
    void MergePrevious(Preintegrated* pPrev);
    void SetNewBias(const Bias &bu_);
    IMU::Bias GetDeltaBias(const Bias &b_);
    cv::Mat GetDeltaRotation(const Bias &b_);
    cv::Mat GetDeltaVelocity(const Bias &b_);
    cv::Mat GetDeltaPosition(const Bias &b_);
    cv::Mat GetUpdatedDeltaRotation();
    cv::Mat GetUpdatedDeltaVelocity();
    cv::Mat GetUpdatedDeltaPosition();
    cv::Mat GetOriginalDeltaRotation();
    cv::Mat GetOriginalDeltaVelocity();
    cv::Mat GetOriginalDeltaPosition();
    Eigen::Matrix<double,15,15> GetInformationMatrix();
    cv::Mat GetDeltaBias();
    Bias GetOriginalBias();
    Bias GetUpdatedBias();

public:
    float dT;
    cv::Mat C;  // 15x15 covariance, order: Phi, V, P, Biasg, Biasa
    cv::Mat Info;   // 15x15, information matrix, infor = inv(covariance)
    cv::Mat Nga, NgaWalk;

    // Values for the original bias (when integration was computed)
    Bias b;
    cv::Mat dR, dV, dP;
    cv::Mat JRg, JVg, JVa, JPg, JPa;    // jacobians w.r.t bias correction
    cv::Mat avgA;
    cv::Mat avgW;


private:
    // Updated bias
    Bias bu;
    // Dif between original and updated bias
    // This is used to compute the updated values of the preintegration
    cv::Mat db;

    struct integrable
    {
        integrable(const cv::Point3f &a_, const cv::Point3f &w_ , const float &t_):a(a_),w(w_),t(t_){}
        cv::Point3f a;
        cv::Point3f w;
        float t;
    };

    std::vector<integrable> mvMeasurements;

    std::mutex mMutex;
};


// Lie Algebra Functions
cv::Mat ExpSO3(const float &x, const float &y, const float &z);
Eigen::Matrix<double,3,3> ExpSO3(const double &x, const double &y, const double &z);
cv::Mat ExpSO3(const cv::Mat &v);
cv::Mat LogSO3(const cv::Mat &R);
cv::Mat RightJacobianSO3(const float &x, const float &y, const float &z);
cv::Mat RightJacobianSO3(const cv::Mat &v);
cv::Mat InverseRightJacobianSO3(const float &x, const float &y, const float &z);
cv::Mat InverseRightJacobianSO3(const cv::Mat &v);
cv::Mat Skew(const cv::Mat &v);
cv::Mat NormalizeRotation(const cv::Mat &R);


} // namespace IMU


#endif // GYROPREDICTIMUTYPES_H
